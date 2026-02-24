#include "script_host.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <string>

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
    lua_ = sol::state{};
    last_loaded_file_.clear();
    ready_ = false;
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
    return runResult(fn(), "ScriptHost::exec");
}

bool ScriptHost::reload() {
    if (last_loaded_file_.empty()) {
        spdlog::warn("ScriptHost: reload 失败，尚未加载任何脚本文件");
        return false;
    }
    return loadFile(last_loaded_file_);
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

void ScriptHost::hardenLuaGlobals() {
    // 阻止脚本动态加载任意 chunk / 文件。
    lua_["dofile"] = sol::lua_nil;
    lua_["loadfile"] = sol::lua_nil;
    lua_["load"] = sol::lua_nil;
    // 避免绕过只读代理表（rawset 不触发 __newindex）。
    lua_["rawset"] = sol::lua_nil;
    lua_["rawget"] = sol::lua_nil;

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
