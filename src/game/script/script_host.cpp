#include "script_host.h"

#include "script_bindings.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <string>

namespace game::script {

ScriptHost::ScriptHost(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
}

bool ScriptHost::init() {
    try {
        // 选择性加载标准库：不加载 io / os 以限制脚本的文件系统和系统调用权限
        lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string, sol::lib::package);

        // 注册 tf.* 命名空间下的全部 C++ → Lua 绑定
        bindScriptAPI(lua_, registry_, dispatcher_);
        ready_ = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("ScriptHost: 初始化失败: {}", e.what());
        ready_ = false;
        return false;
    }
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

} // namespace game::script
