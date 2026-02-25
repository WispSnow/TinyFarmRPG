# FND-008ex 分层外观扩展（多方向显示 + 调试换装面板）

## 元信息
- 任务ID：`FND-008ex`
- 任务标题：`分层外观多方向修复与调试换装扩展`
- 优先级：`P0`
- 状态：`Done`
- 负责人：`TBD`
- 计划时间：`2026-02-25` ～ `2026-02-27`（2~3d）
- 依赖任务：`FND-008`（已完成主链路）

## 背景与问题定位
- 当前现象：玩家仅“朝下”方向能显示分层；`up/right/left` 方向多数情况下为空白。
- 已确认根因：分层素材是单行图集（如 `Idle=512x32`、`Walk=768x32`），当前分层渲染直接复用主 `SpriteComponent.src_rect` 的 `y`（来自 Pre-made 三行图 `128x96` / `192x96`），导致 `y>0` 时越界采样到透明区。
- 本次目标：
1. 各方向动作都能使用默认分层素材稳定显示。
2. 增强玩家调试面板，支持服装/头发等槽位切换。

## 审阅意见分析与取舍
1. `src_rect` 计算职责放置（采纳）  
采纳“预计算布局 + Render 纯数学取样”方案，避免 `RenderSystem` 依赖 `AppearanceCatalog`（保持 engine/game 分层）。

2. 改动文件遗漏（采纳）  
补充 `layered_sprite_component`、`appearance_system`、`appearance_catalog` 接口扩展到计划改动列表。

3. `frames_per_direction` 来源（采纳）  
采用 catalog 显式配置（最可控），并在测试期/离线校验与纹理宽度一致。

4. 方向块顺序假设（采纳并补充前置验证）  
将“方向块验证”提升为实施前置步骤（T0）；验证通过后才固化默认顺序。  
当前预期顺序以验证结果为准，不在代码中硬编码猜测。
已做离线样本比对（`idle/walk/watering/hoe`，默认 profile 合层对比 Pre-made），当前样本结论为：`block0=down`、`block1=up`、`block2=right`、`block3=left`。

5. `slot_variants` 补全工作量（采纳）  
将 `runtime_switchable_slots` 扩展为 `hair/skin/eyes/clothes/acc`，并在 T1 明确补全对应 variant 列表；同时配套“预加载收敛策略”（默认 profile + 当前可见候选，非首屏变体延迟加载），避免启动负担线性膨胀。

6. `left` 方向策略（采纳并调整）  
不再默认“left 复用 right + flip”。优先使用分层图集的独立 `left` 方向块；仅当某动作缺 `left` 块时才回退镜像策略。

7. 小问题处理（采纳）  
“缺层仅跳过该层”语义保持并补充回归断言。  
面板测试采用“命令链路自动化 + UI 手工验收”组合，避免在当前无 ImGui 面板测试基建下引入脆弱测试。

8. 帧索引语义（采纳）  
分层采样不直接使用 `current_frame_index` 作为图集列索引；改为基于 `AnimationFrame.src_rect` 预计算 `source_frame_index_by_runtime_frame`，以兼容非连续/重排帧序列。

## 实现思路
1. 扩展分层缓存结构（关键）  
在 `LayeredSpriteComponent` 中引入每动画布局数据：
- `texture_id`
- `direction_block_index`
- `frames_per_direction`
- `frame_width/frame_height`
- `source_frame_index_by_runtime_frame`（运行时帧索引 -> 源图帧索引）
- `use_animation_flip`（是否沿用 `Sprite.is_flipped_`）

2. 由 AppearanceSystem 预计算布局  
`AppearanceSystem::rebuildLayerCache` 读取 `appearance_catalog`，按 `animation_id` 计算并写入上述布局缓存（含帧索引映射）。  
Render 阶段不查 catalog，不解析动作目录，不做文件系统访问。

3. RenderSystem 只执行采样数学  
基于 `current_frame_index` 与缓存映射计算：
- `source_frame_index = source_frame_index_by_runtime_frame[current_frame_index]`（越界时 clamp）
- `x = (direction_block_index * frames_per_direction + source_frame_index) * frame_width`
- `y = 0`
并按 `use_animation_flip` 决定是否沿用主 sprite 的 flip。

4. catalog 显式配置动作布局  
采用“保留 `action_dirs` + 新增 `action_layouts`”的双字段方案：  
- `action_dirs` 继续负责动作到目录映射（兼容现有路径解析）  
- `action_layouts` 负责 `frames_per_direction`、`direction_block_order`、`left_fallback` 等布局信息  
校验策略分层：运行时做轻量结构校验；纹理宽度一致性做测试期/离线校验，避免启动期重 I/O。

5. 预加载策略收敛（配套）  
将分层外观预加载从“所有 runtime 可切换变体全量注册”调整为：
- 启动时仅默认 profile + 当前槽位首屏候选
- 面板切换时按需补注册/加载目标变体
- 保持 `weapon=auto` 相关动作资源预加载不变

6. 调试面板扩展多槽位  
在玩家面板统一提供 `skin/eyes/clothes/hair/acc` 切换；`weapon` 保持 `auto`。  
新增 `Reset To Profile Default` / `Refresh Appearance`。

## 需要新增的文件
- `tests/game/appearance_catalog_layout_test.cpp`  
  覆盖 `action_layouts` 解析与宽度一致性校验。
- `tests/game/appearance_layered_direction_mapping_test.cpp`  
  覆盖 `AppearanceSystem` 预计算布局（方向块、帧数、flip 策略）。
- `tests/engine/system/render_system_layered_layout_test.cpp`  
  覆盖 Render 层基于布局计算 `src_rect` 的逻辑与缺层跳过。

## 预计改动文件
- `assets/data/appearance_catalog.json`
- `src/game/data/appearance_catalog.h`
- `src/game/data/appearance_catalog.cpp`
- `src/engine/component/layered_sprite_component.h`
- `src/game/system/appearance_system.h`
- `src/game/system/appearance_system.cpp`
- `src/engine/system/render_system.h`
- `src/engine/system/render_system.cpp`
- `src/game/runtime/game_runtime_assembler.cpp`
- `src/game/debug/player_debug_panel.cpp`
- `tests/game/appearance_catalog_layout_test.cpp`
- `tests/game/appearance_layered_direction_mapping_test.cpp`
- `tests/game/appearance_system_test.cpp`
- `tests/engine/system/render_system_layered_layout_test.cpp`
- `tests/engine/system/render_system_layered_source_test.cpp`
- `tests/CMakeLists.txt`

## 分步骤实现
1. T0 前置验证：方向块与左右关系  
对 `idle/walk/watering/hoe` 做样本验证，确认 `direction_block_order` 与 `left` 是否独立块；结果写入计划注释与配置默认值。

2. T1 扩展 catalog 数据契约  
新增 `action_layouts`（保留现有 `action_dirs`）并补全 `runtime_switchable_slots` + `slot_variants`（`skin/eyes/clothes/hair/acc`）。

3. T2 扩展 catalog 解析与校验  
提供 `frames_per_direction` / `direction_block_index` 查询接口；运行时仅做结构/字段校验。

4. T2.1 校验策略分层  
运行时仅做结构与字段完整性校验；纹理宽度一致性迁移到测试（`appearance_catalog_layout_test`）与离线抽样脚本，避免启动期阻塞。

5. T3 扩展 LayeredSpriteComponent 运行时布局缓存  
引入 `layout_by_animation_id`（替代纯 `texture_id` 映射），并新增 `source_frame_index_by_runtime_frame`；保留缺层兜底语义。

6. T4 改造 AppearanceSystem 缓存重建  
在 slot 变更或刷新时一次性填充每动画布局，包含方向块索引、flip 策略与帧索引映射。

7. T5 改造 RenderSystem 分层采样  
Render 仅使用缓存做 `src_rect` 计算，不依赖 game 层目录/配置；按 `source_frame_index_by_runtime_frame` 取样并修复非 down 方向空白。

8. T6 收敛外观预加载  
`collectAppearanceAssets` 改为“默认 profile + 首屏候选”预注册，面板切换时按需加载其余变体。

9. T7 扩展玩家调试面板  
增加多槽位循环切换与重置按钮，保持 `weapon=auto`。

10. T8 测试与验收  
补齐 catalog/layout/render 测试；执行回归并做实机四方向动作验证。

## 待办清单（可打勾追踪）
- [x] T0 完成 `idle/walk/watering/hoe` 方向块顺序样本验证并记录结论
- [x] T1 `appearance_catalog.json` 新增 `action_layouts`（保留 `action_dirs`，含 `frames_per_direction`）
- [x] T1.1 `runtime_switchable_slots` 扩展为 `hair/skin/eyes/clothes/acc`
- [x] T1.2 补齐 `skin/eyes/clothes/acc` 的 `slot_variants`
- [x] T2 `AppearanceCatalog` 解析布局字段并暴露查询接口
- [x] T2.1 运行时仅做轻量校验（结构/字段完整性），纹理宽度一致性转测试期校验
- [x] T3 `LayeredSpriteComponent` 扩展 `layout_by_animation_id + source_frame_index_by_runtime_frame`
- [x] T4 `AppearanceSystem` 预计算每动画布局（方向块/帧数/flip/帧索引映射）
- [x] T5 RenderSystem 按布局计算分层 `src_rect`（使用 source_frame_index，修复非 down 空白）
- [x] T5.1 缺层时仅跳过该层（保留既有语义并补回归断言）
- [x] T6 `collectAppearanceAssets` 收敛为“默认 profile + 首屏候选”，其余按需加载
- [x] T7 Player Debug 面板支持 `skin/eyes/clothes/hair/acc` 切换
- [x] T7.1 增加 `Reset To Profile Default` 与 `Refresh Appearance`
- [x] T8 新增 `appearance_catalog_layout_test.cpp`
- [x] T9 新增 `appearance_layered_direction_mapping_test.cpp`（覆盖非连续帧映射）
- [x] T10 新增 `render_system_layered_layout_test.cpp`
- [x] T11 运行 `ctest --test-dir build/debug/tests --output-on-failure -R \"Appearance|RenderSystem\" -j4`
- [x] T11.1 运行 `ctest --test-dir build/debug/tests --output-on-failure -j4`（当前通过 `315/315`）
- [x] T12 实机验收：`idle/walk/hoe/pickaxe/axe/sickle/watering/planting` 四方向全部可见

## 验收标准（DoD）
- 分层玩家在 `down/up/right/left` 四方向均可见，不再出现“非 down 为空”。
- `RenderSystem` 不依赖 `AppearanceCatalog`，engine/game 分层保持清晰。
- 分层取样可正确覆盖“非连续帧/重排帧”，不因 `current_frame_index` 直接索引导致错帧。
- 调试面板可切换 `skin/eyes/clothes/hair/acc`，切换当帧生效。
- `weapon` 维持动作驱动自动显隐，不引入手动覆盖冲突。
- 扩展可切换槽位后，启动期预加载数量受控（默认 profile + 首屏候选），无明显启动退化。
- 新增测试与回归通过，读档后外观显示保持正确。

## 疑问与待澄清
- 暂无。
