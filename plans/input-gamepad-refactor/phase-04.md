### Phase 4: UI 导航 - Menu 动作 + 导航控制器

**目标**：为菜单、对话、暂停界面建立手柄可用的导航能力，让 UI 不再依赖鼠标悬停和点击。

**前置**：Phase 3（InputContext 已可用，Menu context 已定义）。

---

#### 设计要点

**菜单语义动作**：

| 动作名 | 键盘绑定 | 手柄绑定 |
|--------|---------|---------|
| `menu_up` | `W`, `Up` | `GamepadDpadUp`, `LeftStickUp` |
| `menu_down` | `S`, `Down` | `GamepadDpadDown`, `LeftStickDown` |
| `menu_left` | `A`, `Left` | `GamepadDpadLeft`, `LeftStickLeft` |
| `menu_right` | `D`, `Right` | `GamepadDpadRight`, `LeftStickRight` |
| `menu_confirm` | `Return`, `Space` | `GamepadSouth` |
| `menu_cancel` | `Escape` | `GamepadEast` |

注意：`menu_up/down/left/right` 和 `move_*` 绑定了相同的物理键，但通过 InputContext 隔离——Gameplay context 中 `menu_*` 不激活，Menu context 中 `move_*` 不激活。

**导航模型**：

输入层只产出语义动作，不直接操控 UI 框架。导航控制器消费 `menu_*` 动作，驱动焦点移动和确认/取消。

```
[InputManager] --menu_up/down/confirm/cancel--> [UINavigationController] --> [自研 UI / RmlUI]
```

**RmlUI 桥接**：

RmlUI 本身有内置的键盘导航支持（Tab / 方向键可移动焦点到 focusable 元素）。桥接层的职责是把 `menu_*` 语义动作翻译为 `Rml::Context::ProcessKeyDown` 调用，而不是从零实现导航逻辑。

---

#### 需要新增的文件

- `src/engine/ui/ui_navigation_controller.h`
- `src/engine/ui/ui_navigation_controller.cpp`
- `tests/engine/ui/ui_navigation_controller_test.cpp`

#### 需要修改的文件

- `config/input.json` — 添加 `menu_*` 动作
- `src/engine/input/input_manager.cpp` — `defaultMappings` 同步
- `src/engine/ui/rmlui/rml_ui_layer.cpp` — 接入导航桥接
- `src/game/scene/pause_menu_scene.cpp` — 首个接入点

---

#### Step 4.1: 添加菜单语义动作

- 在 `config/input.json` 和 `defaultMappings()` 中添加上述 `menu_*` 动作。
- 在 Phase 3 定义的 Menu context 白名单中确认这些动作已包含。

#### Step 4.2: UINavigationController

实现一个轻量的导航控制器：

```cpp
class UINavigationController {
public:
    void update(InputManager& input);  // 每帧调用，检查 menu_* 动作状态

    // 导航事件回调（由 UI 实现层监听）
    entt::sigh<void()> on_navigate_up;
    entt::sigh<void()> on_navigate_down;
    entt::sigh<void()> on_navigate_left;
    entt::sigh<void()> on_navigate_right;
    entt::sigh<void()> on_confirm;
    entt::sigh<void()> on_cancel;
};
```

- 只在 `menu_*` 动作 PRESSED 时触发信号（不处理 HELD 重复——如需连续导航，后续可加定时重复逻辑）。

#### Step 4.3: RmlUI 导航桥接

在 `rml_ui_layer.cpp` 中监听 `UINavigationController` 的信号：

- `on_navigate_up/down/left/right` → 调用 `rml_context->ProcessKeyDown(对应方向键, 0)`
- `on_confirm` → 调用 `rml_context->ProcessKeyDown(Rml::Input::KI_RETURN, 0)`
- `on_cancel` → 调用 `rml_context->ProcessKeyDown(Rml::Input::KI_ESCAPE, 0)`

> 先调研 RmlUI 的 `ProcessKeyDown` 是否能驱动焦点移动。如果不够用，再考虑手动操控 `Element::Focus()` / `Element::Click()`。

#### Step 4.4: 暂停菜单接入

- `PauseMenuScene` 作为首个接入点：确保手柄可以在"继续"、"保存"、"读取"、"返回标题"之间导航并确认。
- 验证键鼠操作不退化（鼠标点击仍然可用）。

#### Step 4.5: 自研 UI 导航（如需）

如果项目中有不通过 RmlUI 渲染的菜单/面板（如物品栏、快捷栏），为这些 UI 也接入 `UINavigationController` 信号。

---

#### 待办清单

- [ ] 添加 `menu_*` 语义动作配置
- [ ] 实现 `UINavigationController`
- [ ] RmlUI 导航桥接（`ProcessKeyDown` 方案）
- [ ] 在暂停菜单中验证手柄导航
- [ ] 确保键鼠操作不退化
- [ ] 编写导航控制器测试

#### 完成标准

- 暂停菜单可以仅用手柄完成导航和操作。
- 菜单导航不依赖鼠标悬停和点击。
- 自研 UI 与 RmlUI 都能通过统一语义动作响应手柄。
