# 重构优化

这是一个农场 RPG 游戏项目，参见 @docs/overview.md

## 初期目标（教学 demo 闭环，已全部完成）

- [x] 添加脚本支持：Lua + Sol2（`engine::script::ScriptHost` + game 层 `tinyfarm_script_module` 暴露 `tf.time / tf.player / tf.command / tf.dialogue`）
- [x] 可切换角色外观服装（衣服、头发、武器等部件分层）（`AppearanceCatalog` + `AppearanceComponent` + `LayeredSpriteComponent` + `AppearanceSystem`）
- [x] 特效粒子系统，集成 Effekseer（`engine::vfx::VfxService` + `EffekseerBackend`，world/overlay 双通道）
- [x] 拓展 RPG 玩法：商店、任务线、回合制战斗（RPGMaker 风格）、技能系统、队伍/招募、装备、角色成长、用户偏好（存档 schema 已迁至 v6）

## 下一阶段：让 Lua 承载更多游戏逻辑

接下来准备把更多剧本式玩法（对话脚本、任务推进、招募对白、商店预设、地图事件等）从 C++ 迁出到 Lua，由 `scripts/bootstrap.lua` 演化为真正的脚本组合根；同时扩展 `tf.*` 绑定（quest/shop/party/battle/event 等命名空间和回调挂载）。详见 `docs/overview.md` 末节与 `docs/tutorial/lua-binding-guide.md`。

## Note

- 此项目是一个开发中的程序，并未上线。尽量采用最优方案，不必考虑向后兼容。
- 进行代码编写、review 时，遵照此规范：@for_agent/code-guide.md
- 进行 UI（rmlui）文件编写时，先读备忘：@for_agent/rmlui-guide.md
- 编写/修改弹出场景 UI 时，遵循风格指引：@for_agent/ui-style-guide.md
- 项目文档位于 `docs` 文件夹中，撰写文档时先读备忘：@for_agent/docs-guide.md
- 构建时使用 ninja 工具加快速度