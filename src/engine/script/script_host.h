#pragma once

#include "script_entity_handle.h"
#include "script_module.h"

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <sol/sol.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace engine::script {

/// Lua 脚本宿主——管理 sol::state 生命周期并提供脚本加载/执行接口。
///
/// 采用两阶段初始化（构造 + init()），init 失败时宿主进入"未就绪"状态，
/// 后续所有操作安全跳过，不会导致崩溃（soft-failure 模式）。
///
/// 生命周期约束：registry 的引用由场景持有，
/// ScriptHost 作为 runtime services 的成员始终先于 registry 析构。
class ScriptHost final {
public:
    explicit ScriptHost(entt::registry& registry);

    /// 打开 Lua 标准库并安装模块扩展（由调用方注入）。
    [[nodiscard]] bool init(entt::dispatcher& dispatcher,
                            const std::vector<ScriptModuleInstaller>& installers = {});
    /// 清理脚本上下文并失效化当前会话 token，可重复调用（幂等）。
    void shutdown();

    /// 编译并执行磁盘脚本文件，成功时记录路径供 reload() 使用。
    [[nodiscard]] bool loadFile(std::string_view file_path);

    /// 编译并执行内存中的 Lua 字符串（适用于测试和动态脚本）。
    [[nodiscard]] bool exec(std::string_view script);

    /// 重新加载上一次 loadFile 成功的脚本，用于热重载。
    [[nodiscard]] bool reload();

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] std::uint64_t sceneToken() const noexcept;
    [[nodiscard]] ScriptEntityHandle makeHandle(entt::entity entity) const noexcept;
    [[nodiscard]] bool validateHandle(const ScriptEntityHandle& handle,
                                      entt::entity& out_entity,
                                      std::string_view source) const;

private:
    /// 统一处理 sol::protected_function 的执行结果，失败时记录日志而非抛异常。
    [[nodiscard]] bool runResult(sol::protected_function_result&& result, std::string_view source);

    /// 前置检查：若未初始化则记录警告并返回 false，保护所有公共方法。
    [[nodiscard]] bool ensureReady(std::string_view op_name) const;
    void hardenLuaGlobals();
    void configureInstructionLimit();

    entt::registry& registry_;
    sol::state lua_;                  ///< RAII 封装的 Lua 虚拟机，析构时自动释放
    std::string last_loaded_file_{};  ///< 最近一次 loadFile 的路径，供 reload 使用
    std::uint64_t scene_token_{0};    ///< 当前脚本会话 token（跨场景代际校验）
    bool ready_{false};               ///< init() 成功后置 true
};

} // namespace engine::script
