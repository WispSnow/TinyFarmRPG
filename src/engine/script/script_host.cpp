#include "script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <cctype>
#include <exception>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace engine::script {

namespace {

constexpr int SCRIPT_INSTRUCTION_LIMIT = 200000;
std::uint64_t g_next_scene_token = 1;

std::uint64_t allocateSceneToken() {
    return g_next_scene_token++;
}

void onInstructionLimitReached(lua_State* lua_state, lua_Debug*) {
    luaL_error(lua_state, "ScriptHost: script exceeded instruction limit");
}

bool isValidModuleSegment(std::string_view segment) {
    if (segment.empty()) {
        return false;
    }

    for (const char ch : segment) {
        const auto byte = static_cast<unsigned char>(ch);
        if (!std::isalnum(byte) && ch != '_') {
            return false;
        }
    }
    return true;
}

bool isValidScriptModuleName(std::string_view module_name) {
    if (module_name.empty()) {
        return false;
    }

    std::size_t segment_begin = 0;
    while (segment_begin < module_name.size()) {
        const std::size_t dot = module_name.find('.', segment_begin);
        const std::size_t segment_end = dot == std::string_view::npos ? module_name.size() : dot;
        if (!isValidModuleSegment(module_name.substr(segment_begin, segment_end - segment_begin))) {
            return false;
        }
        if (dot == std::string_view::npos) {
            return true;
        }
        segment_begin = dot + 1;
    }

    return false;
}

std::string scriptModulePath(std::string_view root_dir, std::string_view module_name) {
    std::string path(root_dir);
    if (!path.empty() && path.back() != '/') {
        path.push_back('/');
    }

    for (const char ch : module_name) {
        path.push_back(ch == '.' ? '/' : ch);
    }
    path += ".lua";
    return path;
}

} // namespace

ScriptHost::ScriptHost(entt::registry& registry)
    : registry_(registry), scene_token_(allocateSceneToken()) {
}

bool ScriptHost::init(entt::dispatcher& dispatcher, const std::vector<ScriptModuleInstaller>& installers) {
    if (ready_) {
        return true;
    }

    if (scene_token_ == 0) {
        scene_token_ = allocateSceneToken();
    }

    try {
        // 选择性加载标准库：不加载 io / os / package。
        lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
        hardenLuaGlobals();
        configureInstructionLimit();
        script_modules_ = lua_.create_table();

        for (const auto& installer : installers) {
            if (!installer) {
                continue;
            }
            installer(lua_, *this, registry_, dispatcher);
        }
        ready_ = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ScriptHost: 初始化失败: {}", e.what());
        ready_ = false;
        return false;
    }
}

void ScriptHost::shutdown() {
    ready_ = false;
    clearScriptRuntimeState();
    lua_ = sol::state{};
    last_loaded_file_.clear();
    scene_token_ = 0;
}

bool ScriptHost::loadFile(std::string_view file_path) {
    if (!ensureReady("loadFile")) {
        return false;
    }

    // 第一步：编译——将脚本文件解析为字节码 chunk（不执行）
    const std::string path(file_path);
    sol::load_result chunk = lua_.load_file(path);
    if (!chunk.valid()) {
        const sol::error err = chunk;
        spdlog::error("ScriptHost: 脚本加载失败 [{}]: {}", path, err.what());
        return false;
    }

    // 第二步：执行——Lua 文件的顶层代码本身是一个匿名函数，调用它使其生效
    sol::protected_function fn = chunk;
    configureInstructionLimit();
    if (!runResult(fn(), path)) {
        return false;
    }

    last_loaded_file_ = path;
    return true;
}

bool ScriptHost::exec(std::string_view script) {
    if (!ensureReady("exec")) {
        return false;
    }

    // 第二个参数 "ScriptHost::exec" 是 chunk 名称，出错时会出现在 Lua 错误栈信息中
    sol::load_result chunk = lua_.load(std::string(script), "ScriptHost::exec");
    if (!chunk.valid()) {
        const sol::error err = chunk;
        spdlog::error("ScriptHost: 脚本编译失败: {}", err.what());
        return false;
    }

    sol::protected_function fn = chunk;
    configureInstructionLimit();
    return runResult(fn(), "ScriptHost::exec");
}

bool ScriptHost::reload() {
    if (last_loaded_file_.empty()) {
        spdlog::warn("ScriptHost: reload 失败，尚未加载任何脚本文件");
        return false;
    }

    const std::string path = last_loaded_file_;
    scene_token_ = allocateSceneToken();
    clearScriptRuntimeState();
    return loadFile(path);
}

void ScriptHost::setScriptRoot(std::string_view root_dir) {
    script_root_ = root_dir.empty() ? "scripts" : std::string{root_dir};
    if (ready_) {
        script_modules_ = lua_.create_table();
    }
}

bool ScriptHost::isReady() const noexcept {
    return ready_;
}

std::uint64_t ScriptHost::sceneToken() const noexcept {
    return scene_token_;
}

ScriptEntityHandle ScriptHost::makeHandle(const entt::entity entity) const noexcept {
    if (entity == entt::null || scene_token_ == 0) {
        return {};
    }

    return ScriptEntityHandle{
        entity,
        scene_token_,
    };
}

bool ScriptHost::validateHandle(const ScriptEntityHandle& handle,
                                entt::entity& out_entity,
                                std::string_view source) const {
    out_entity = entt::null;

    if (!ready_) {
        spdlog::warn("ScriptHost: 句柄校验失败 [{}]，宿主未初始化", source);
        return false;
    }

    if (isNullHandle(handle)) {
        spdlog::warn("ScriptHost: 句柄校验失败 [{}]，空句柄", source);
        return false;
    }

    if (handle.scene_token != scene_token_) {
        spdlog::warn("ScriptHost: 句柄校验失败 [{}]，scene_token 不匹配（got={}, expect={}）",
                     source,
                     handle.scene_token,
                     scene_token_);
        return false;
    }

    if (!registry_.valid(handle.entity)) {
        spdlog::warn("ScriptHost: 句柄校验失败 [{}]，entity 无效（raw={}）",
                     source,
                     toRawEntity(handle));
        return false;
    }

    out_entity = handle.entity;
    return true;
}

sol::state& ScriptHost::luaState() noexcept {
    return lua_;
}

sol::object ScriptHost::requireScriptModule(std::string_view module_name) {
    if (!ensureReady("requireScriptModule")) {
        return sol::make_object(lua_, sol::lua_nil);
    }

    if (!isValidScriptModuleName(module_name)) {
        spdlog::warn("ScriptHost: 模块名非法 [{}]", module_name);
        return sol::make_object(lua_, sol::lua_nil);
    }

    const std::string module_key(module_name);
    sol::object cached = script_modules_.get<sol::object>(module_key);
    if (cached.valid() && cached.get_type() != sol::type::lua_nil) {
        return cached;
    }

    const std::string path = scriptModulePath(script_root_, module_name);
    sol::load_result chunk = lua_.load_file(path);
    if (!chunk.valid()) {
        const sol::error err = chunk;
        spdlog::error("ScriptHost: 模块加载失败 [{} -> {}]: {}", module_key, path, err.what());
        return sol::make_object(lua_, sol::lua_nil);
    }

    // Sentinel for circular/self require. Modules that return no value stay cached as true.
    script_modules_.set(module_key, true);

    sol::protected_function fn = chunk;
    configureInstructionLimit();
    sol::protected_function_result result = fn();
    if (!result.valid()) {
        const sol::error err = result;
        spdlog::error("ScriptHost: 模块执行失败 [{} -> {}]: {}", module_key, path, err.what());
        script_modules_.set(module_key, sol::lua_nil);
        return sol::make_object(lua_, sol::lua_nil);
    }

    if (result.return_count() > 0) {
        sol::object module_value = result.get<sol::object>();
        if (module_value.get_type() != sol::type::lua_nil) {
            script_modules_.set(module_key, module_value);
            return module_value;
        }
    }

    return script_modules_.get<sol::object>(module_key);
}

bool ScriptHost::registerEventCallback(std::string_view event_name, sol::protected_function callback) {
    if (!ensureReady("registerEventCallback")) {
        return false;
    }
    if (event_name.empty() || !callback.valid()) {
        spdlog::warn("ScriptHost: event callback 注册失败，事件名为空或回调无效");
        return false;
    }

    event_callbacks_[std::string{event_name}].push_back(std::move(callback));
    return true;
}

bool ScriptHost::emitEvent(std::string_view event_name, const sol::table& payload) {
    if (!ensureReady("emitEvent")) {
        return false;
    }

    const auto found = event_callbacks_.find(std::string{event_name});
    if (found == event_callbacks_.end()) {
        return true;
    }

    auto callbacks = found->second;
    bool all_ok = true;
    for (auto& callback : callbacks) {
        if (!callback.valid()) {
            all_ok = false;
            continue;
        }

        std::vector<std::function<void()>> callback_commands{};
        auto* previous_commands = active_callback_commands_;
        active_callback_commands_ = &callback_commands;

        configureInstructionLimit();
        sol::protected_function_result result = callback(payload);
        active_callback_commands_ = previous_commands;

        const std::string source = "tf.event." + std::string{event_name};
        if (!runResult(std::move(result), source)) {
            all_ok = false;
            continue;
        }

        auto& destination = previous_commands ? *previous_commands : deferred_commands_;
        destination.insert(destination.end(),
                           std::make_move_iterator(callback_commands.begin()),
                           std::make_move_iterator(callback_commands.end()));
    }

    return all_ok;
}

bool ScriptHost::isHandlingScriptCallback() const noexcept {
    return active_callback_commands_ != nullptr;
}

void ScriptHost::enqueueDeferredCommand(std::function<void()> command) {
    if (!command) {
        return;
    }

    if (active_callback_commands_) {
        active_callback_commands_->push_back(std::move(command));
        return;
    }

    deferred_commands_.push_back(std::move(command));
}

void ScriptHost::drainDeferredCommands() {
    if (deferred_commands_.empty()) {
        return;
    }

    auto commands = std::move(deferred_commands_);
    deferred_commands_.clear();
    for (auto& command : commands) {
        if (command) {
            command();
        }
    }
}

/// 使用 sol::protected_function（内部走 lua_pcall）而非 sol::function，
/// 确保脚本运行时错误被捕获为返回值，而非向上传播 C++ 异常导致崩溃。
bool ScriptHost::runResult(sol::protected_function_result&& result, std::string_view source) {
    if (result.valid()) {
        return true;
    }

    const sol::error err = result;
    spdlog::error("ScriptHost: 脚本执行失败 [{}]: {}", source, err.what());
    return false;
}

bool ScriptHost::ensureReady(std::string_view op_name) const {
    if (ready_) {
        return true;
    }
    spdlog::warn("ScriptHost: {} 被忽略，宿主未初始化", op_name);
    return false;
}

void ScriptHost::clearScriptRuntimeState() {
    event_callbacks_.clear();
    deferred_commands_.clear();
    active_callback_commands_ = nullptr;
    script_modules_ = ready_ ? lua_.create_table() : sol::table{};
}

void ScriptHost::hardenLuaGlobals() {
    // 阻止脚本动态加载任意 chunk / 文件。
    lua_["dofile"] = sol::lua_nil;
    lua_["loadfile"] = sol::lua_nil;
    lua_["load"] = sol::lua_nil;
    // 避免绕过只读代理表（rawset 不触发 __newindex）。
    lua_["rawset"] = sol::lua_nil;
    lua_["rawget"] = sol::lua_nil;
    lua_["collectgarbage"] = sol::lua_nil;

    // 可选进一步收敛：避免脚本导出字节码。
    sol::object string_obj = lua_["string"];
    if (string_obj.is<sol::table>()) {
        sol::table string_table = string_obj.as<sol::table>();
        string_table["dump"] = sol::lua_nil;
    }
}

void ScriptHost::configureInstructionLimit() {
    lua_sethook(lua_.lua_state(), &onInstructionLimitReached, LUA_MASKCOUNT, SCRIPT_INSTRUCTION_LIMIT);
}

} // namespace engine::script
