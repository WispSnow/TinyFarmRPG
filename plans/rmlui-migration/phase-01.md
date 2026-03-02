### Phase 1: 静态 HUD — TimeClockUI

**目标**：用 RmlUi 重写时钟 HUD，验证 data binding + sprite 图集方案。

#### Step 1.1: RML 文档

**新建** `ui/rmlui/hud/time_clock.rml` + `time_clock.rcss`

- 布局：左侧时钟 sprite + 右侧 Day / Time 文本
- 时钟指针通过 data binding 选择 sprite frame（`data-attr-class` 切换 CSS class 控制背景 sprite）
- 日/时文本通过 data binding 绑定 `{{day}}` / `{{time}}`

#### Step 1.2: Data Model

在 `GameScene` 中创建 time clock data model：

```cpp
struct TimeClockModel {
    int day;
    std::string time_text;     // "HH:MM"
    int clock_hand_frame;      // 0-7
};
```

每帧从 `GameTime` 读取并 `DirtyVariable()` 通知 RmlUi。

#### Step 1.3: 替换

- 从 `GameScene::initUI()` 中移除 `TimeClockUI` 创建代码
- 改为加载 `time_clock.rml` 文档
- 删除 `src/game/ui/time_clock_ui.h/cpp`

**涉及文件**：
| 操作 | 文件 |
|------|------|
| 新建 | `ui/rmlui/hud/time_clock.rml` |
| 新建 | `ui/rmlui/hud/time_clock.rcss` |
| 修改 | `src/game/scene/game_scene.cpp`（替换 TimeClockUI） |
| 删除 | `src/game/ui/time_clock_ui.h/cpp` |

**验证**：时钟 HUD 显示正常、时间动态更新、指针随时间旋转。

---

