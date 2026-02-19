#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <sol/sol.hpp>

namespace game::script {
class ScriptHost;

/// 向 Lua 虚拟机注册 "tf" 命名空间下的全部 C++ 绑定。
///
/// 注册后 Lua 可通过 tf.time / tf.player / tf.command / tf.dialogue 访问游戏逻辑。
/// 所有绑定 lambda 通过引用捕获 ScriptHost / registry / dispatcher，
/// 由 ScriptHost 的 RAII 析构顺序保证引用安全（详见 script_host.h 注释）。
void bindScriptAPI(sol::state& lua, ScriptHost& host, entt::registry& registry, entt::dispatcher& dispatcher);

} // namespace game::script
