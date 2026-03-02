### Phase 2: 全屏 Overlay — UIScreenFade（含 MapTransitionSystem 适配）

**目标**：用 RmlUi 实现全屏淡入淡出效果，同时保证 `MapTransitionSystem` 的异步加载流程不断裂。

#### Step 2.1: RML ScreenFade 实现

**新建** `ui/rmlui/overlay/screen_fade.rml` + `screen_fade.rcss`

- 一个全屏 `<div>`，CSS `background-color: black` + `opacity` 属性
- 通过 C++ 侧定时器每帧更新 `opacity`（RmlUi CSS transition 不支持精确的回调时机，需要 C++ 侧驱动以保证状态机可靠性）

#### Step 2.2: RmlScreenFade 类

**新建** `src/engine/ui/rmlui/rml_screen_fade.h/cpp`

- `RmlScreenFade : public IScreenFade`
- 内部持有 `Rml::ElementDocument*` + `Rml::Element*`（全屏遮罩元素）
- 维护与旧 `UIScreenFade` 相同的 4 阶段状态机（Idle → FadingOut → Holding → FadingIn → Idle）
- `fadeOut(seconds)` / `fadeIn(seconds)` 启动过渡，每帧由外部调用 `update(delta_time)` 推进 alpha 插值
- `phase()` 返回当前状态——`MapTransitionSystem` 的轮询契约不变

#### Step 2.3: 替换

- `GameScene::initUI()` 中创建 `RmlScreenFade` 替代旧 `UIScreenFade`
- `MapTransitionSystem::setFadeOverlay()` 已在 Phase 0 改为接受 `IScreenFade*`，此处传入 `RmlScreenFade*`
- 旧 `UIScreenFade` 不再被引用（最终在 Phase 8 随框架一起删除）

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/overlay/screen_fade.rml/rcss` |
| 新建 | `src/engine/ui/rmlui/rml_screen_fade.h/cpp` |
| 修改 | `src/game/scene/game_scene.h/cpp`（替换 ScreenFade 创建） |

**验证**：
- 地图切换时淡出→黑屏→加载→淡入流程完整
- `MapTransitionSystem` 轮询 `phase()` 时序正确（FadingOut → Holding → FadingIn → Idle）
- 非地图切换场景的全屏 fade 效果正常

---

