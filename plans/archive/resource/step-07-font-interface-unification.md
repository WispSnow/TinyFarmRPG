# 步骤 7: Font 接口统一（保留现有缓存实现）

- 对应上层计划：`./resource-refactor.md`

## 思路

本步骤不把 Font 迁移到 `entt::resource_cache`，继续保留 `FontManager` 当前的
`unordered_map<(font_id, pixel_size), unique_ptr<Font>>` 实现；重点收敛两件事：

- 完成 Font API 的最终统一形态（严格预加载 + ID-only + size）
- 修复 TextRenderer 布局缓存持有 `Font*` 时的失效时序风险

当前风险点是：`ResourceManager::unloadFont/clearFonts` 先释放字体，再 `enqueue` 事件；而主循环在 render 后才 `dispatcher.update()`，导致本帧内 `TextRenderer` 可能仍读取到旧布局里的悬垂指针。  
本步骤采用“立即分发失效事件”的方案，直接消除这一帧窗口。

按 `for_agent/design-guide.md`，本步骤直接落实最终方案，不保留兼容写法。

## 关键设计

1. Font 缓存模型保持不变

- 保留 `FontManager` 现有复合键缓存结构，不引入 `entt::resource_cache`。
- 外部接口统一保持：
- `loadFont(id, pixel_size, path)`
- `getFont(id, pixel_size)`（只读缓存，不隐式加载）
- `unloadFont(id, pixel_size)` / `clearFonts()`
- 说明：步骤 6 已完成 Font 路径回退与 hash 重载清理；本步骤对该点以“防回归校验”为主，不做结构性 API 改造。

2. 字体失效事件改为立即分发（关键）

- `ResourceManager::unloadFont`：从 `dispatcher_->enqueue<FontUnloadedEvent>` 改为 `dispatcher_->trigger<FontUnloadedEvent>`。
- `ResourceManager::clearFonts`：从 `enqueue<FontsClearedEvent>` 改为 `trigger<FontsClearedEvent>`。
- 保持事件语义“字体已卸载/已清空”不变，但把通知时机收敛到同一调用栈，避免跨帧延迟。
- 调用顺序保持为“先释放字体，再 trigger 通知”；需要在代码注释中明确该顺序的意图：listener 不应解引用历史 `Font*`。
- 新增约束：`FontUnloadedEvent/FontsClearedEvent` 的 listener 禁止在回调中调用 `ResourceManager` 的 `load/unload/clear`（避免同步分发下的重入风险）。

3. TextRenderer 缓存失效契约收敛

- 保留 `TextLayout` 缓存键 `(font_id, font_size, text, layout_options)`。
- 在 `onFontUnloaded` 中按 `(font_id, font_size)` 精确清除对应布局；在 `onFontsCleared` 中全量清空。
- 任何字体卸载发生后，本帧后续 `drawText/getTextSize` 必须重新走 `getFont(id, size)`；若未预加载则返回失败，不允许继续使用旧布局。
- 本步骤默认不修改 `TextRenderer` 逻辑代码，以“行为验证 + 防回归测试”为主；仅在诊断需要时补充注释/日志。

4. 严格预加载与配置契约

- Font 继续执行严格预加载语义：仅注册不预加载时，`getFont(id, size)` 返回 `nullptr`。
- `resource_mapping.json` 字体条目采用多 size 注册（`sizes`）作为标准写法；`size/point_size` 仅作为兼容输入。

5. 调试与文档一致性

- `events.h` 中资源事件注释更新为“Font 失效事件使用 trigger（立即分发）”。
- 调试日志保留 `id + pixel_size`，便于定位具体字号问题。

## 需要新增的文件

| 文件 | 说明 |
|------|------|
| `tests/engine/resource/resource_manager_font_event_dispatch_test.cpp` | 验证 `unloadFont/clearFonts` 事件为立即分发（调用后无需 `dispatcher.update()` 即可观测） |

> 需要同步更新（无新增）：`src/engine/resource/resource_manager.cpp`、`src/engine/utils/events.h`、`tests/engine/resource/resource_manager_strict_preload_api_test.cpp`、`tests/CMakeLists.txt`。
> 可选更新：`src/engine/render/text_renderer.cpp`（仅在补充诊断日志/注释时修改）。

## 实现步骤

### 7.1 收敛 Font API 约束（保持 ID + size）

- 定位为“验证确认”步骤：确认步骤 6 后已无 path/hash 的 Font get 重载残留。
- 仅做注释与测试防回归补强，不做额外 API 结构变更。
- 明确 `getFont(id, size)` 只查缓存，不做隐式加载（与 Texture/Audio 完全对齐）。

### 7.2 将字体失效事件改为 `trigger`

- 修改 `ResourceManager::unloadFont` 与 `ResourceManager::clearFonts` 分发方式。
- 保证事件在字体释放同帧立即通知到 `TextRenderer` 与其他监听者。
- 在 `resource_manager.cpp` 写明“先释放后通知”的顺序意图，以及 listener 禁止重入 `load/unload/clear` 的约束。
- 同步更新 `events.h` 对 Font 资源事件的分发建议注释（从 enqueue 更新为 trigger）。

### 7.3 收敛 TextRenderer 的字体失效行为

- 复核 `onFontUnloaded/onFontsCleared` 的失效逻辑，确保只依赖 key 删除布局缓存，不依赖已释放字体对象。
- 验证 `trigger` 模式下同调用栈内完成缓存清理；默认不改实现，仅在必要时补充注释/日志。

### 7.4 补齐 Font 严格预加载测试

- 在 `resource_manager_strict_preload_api_test.cpp` 增加 Font 用例：
- 仅注册 `(font_id, size)` 时 `getFont` 返回空
- `preloadRegisteredResources()` 后 `getFont` 返回有效指针
- 增加编译期防回归断言：旧的 Font 路径型 `get` 重载不可调用（concept + `static_assert`）。

### 7.5 新增字体事件时序测试

- 新增 `resource_manager_font_event_dispatch_test.cpp`：
- `unloadFont` 调用后立即收到 `FontUnloadedEvent`
- `clearFonts` 调用后立即收到 `FontsClearedEvent`
- 对照验证：不调用 `dispatcher.update()` 也应成立。
- 边界验证：`unloadFont` 后不调用 `dispatcher.update()`，`getFont(id, size)` 立即返回 `nullptr`。

### 7.6 回归验证

- 编译：`cmake --build build -j 8`
- 测试：`ctest --test-dir build --output-on-failure`
- 手动回归：
- 标题场景 -> 主场景切换，观察 `getTextSize`/字体日志无“未注册路径”误报
- 运行期执行字体卸载路径（如场景清理）时无崩溃、无悬垂访问

## 待办

- [x] 7.1 复核并收敛 Font API 到 `id + pixel_size`，确保 `getFont` 严格只读缓存
- [x] 7.2 将 `unloadFont/clearFonts` 事件从 `enqueue` 改为 `trigger`
- [x] 7.2A 在 `resource_manager.cpp` 标注 Font 事件顺序与 listener 重入约束
- [x] 7.2B 更新 `events.h` 的 Font 资源事件注释为 trigger 语义
- [x] 7.3 复核 TextRenderer 字体失效处理，确保同帧缓存清理生效
- [x] 7.4 在严格预加载测试中补齐 Font 语义与“旧路径型 get 不可调用”防回归断言
- [x] 7.5 新增字体事件分发时序测试（含 unload 后立即 `getFont == nullptr`）并接入构建
- [x] 7.6 完成构建、自动化测试与场景切换手动回归

## 需要澄清

暂无。
