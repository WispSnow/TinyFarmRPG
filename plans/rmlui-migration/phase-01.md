### Phase 1: 静态 HUD — TimeClockUI

**目标**：用 RmlUi 重写时钟 HUD，验证 data binding + sprite 图集方案。

---

#### 现有 TimeClockUI 视觉参考

```
┌──────────────────────────────────────────┐
│                                          │
│  ┌─────────┐┌─────────────────────────┐  │  整体位置: 屏幕左上角 (10, 10)
│  │         ││  ┌───────────────────┐  │  │
│  │  Clock  ││  │     Day 1        │  │  │  背景面板(米色九宫格)
│  │  Face   ││  ├───────────────────┤  │  │  - 与表盘重叠 24px
│  │  +Hand  ││  │     06:00        │  │  │
│  │         ││  └───────────────────┘  │  │  标签背景(独立九宫格)
│  └─────────┘└─────────────────────────┘  │  - Day 和 Time 各一个
│   64x64px        118x56px                │  - 文本居中对齐
└──────────────────────────────────────────┘
   UI_SCALE = 2x
```

**组件清单**（均需在 RML 中复刻）：

| 组件 | 纹理来源 | 基础尺寸 | 说明 |
|------|---------|---------|------|
| 时钟表盘 | `Clock.png` 全图 | 32x32 | 静态底图 |
| 时钟指针 | `clock hand.png` 8 帧水平排列 | 每帧 32x32 | 按时间段切帧，以 10:30 为起点，每 180 分钟一帧 |
| 背景面板 | `Extras.png` [66,65,59,28] | 59x28 | 九宫格 (1,3,1,1)，放在表盘右侧，重叠 24px |
| 标签背景×2 | `Extras.png` [71,99,33,10] | 33x10 | 九宫格 (1,1,1,1)，Day/Time 各一个 |
| Day 文本 | — | — | 格式 `"Day {N}"`，居中于标签背景 |
| Time 文本 | — | — | 格式 `"HH:MM"` 24 小时制，居中于标签背景 |

---

#### Step 1.1: Spritesheet 补充

**修改** `ui/rmlui/theme/spritesheet.rcss`

在现有 `ui-clock` 中补充指针帧和背景面板 sprite：

```css
@spritesheet ui-clock {
    src: assets/farm-rpg/UI/Clock/Clock.png;
    clock-face: 0px 0px 32px 32px;
}

@spritesheet ui-clock-hand {
    src: assets/farm-rpg/UI/Clock/clock hand.png;
    clock-hand-0: 0px 0px 32px 32px;
    clock-hand-1: 32px 0px 32px 32px;
    clock-hand-2: 64px 0px 32px 32px;
    clock-hand-3: 96px 0px 32px 32px;
    clock-hand-4: 128px 0px 32px 32px;
    clock-hand-5: 160px 0px 32px 32px;
    clock-hand-6: 192px 0px 32px 32px;
    clock-hand-7: 224px 0px 32px 32px;
}

@spritesheet ui-clock-extras {
    src: assets/farm-rpg/UI/Clock/Extras.png;
    clock-panel-bg:    66px 65px 59px 28px;
    clock-label-bg:    71px 99px 33px 10px;
}
```

#### Step 1.2: RML 文档 + RCSS

**新建** `ui/rmlui/hud/time_clock.rml` + `time_clock.rcss`

布局要求：
- 根容器 `position: absolute; left: 10dp; top: 10dp`
- 左侧：表盘 `64x64dp`（2x 缩放），指针叠加在表盘之上
- 右侧：背景面板 `118x56dp`，与表盘重叠 `24dp`（即面板 `left` = 64 - 24 = 40dp）
- 面板内部：垂直排列两个标签行，各 `66x20dp`，间距 `4dp`
- 标签文本居中对齐
- 背景面板和标签使用九宫格 sprite（`decorator: ninepatch`）

指针帧切换：
- 通过 data binding `data-attr-class` 动态设置 CSS class
- 8 个 class（`.hand-0` ~ `.hand-7`）分别引用 `clock-hand-0` ~ `clock-hand-7`

Data binding 表达式：
- `{{ day_text }}` → "Day 1"
- `{{ time_text }}` → "06:00"
- `{{ hand_class }}` → "hand-0" ~ "hand-7"

#### Step 1.3: Data Model + 更新逻辑

**新建** `src/game/ui/time_clock_hud.h/cpp`

```cpp
/// 时钟 HUD 的 RmlUi 数据驱动层
class TimeClockHud {
public:
    TimeClockHud(engine::ui::rmlui::RmlUILayer& layer,
                 Rml::Context* context);
    ~TimeClockHud();

    /// 每帧从 GameTime 刷新 data model
    void update(const game::data::GameTime& game_time);

private:
    engine::ui::rmlui::RmlDataBridge data_bridge_;
    Rml::ElementDocument* document_{nullptr};

    // 绑定数据
    Rml::String day_text_{"Day --"};
    Rml::String time_text_{"??:??"};
    Rml::String hand_class_{"hand-0"};

    // 脏检测缓存
    int last_day_{-1};
    int last_hour_{-1};
    int last_minute_{-1};

    static int pickHandIndex(float hour, float minute);
};
```

指针帧计算逻辑（从现有实现搬运）：
```
起点 = 10:30 (630 分钟)
每帧 = 180 分钟
index = floor((total_minutes - 630 + 1440) mod 1440 / 180)
clamp(index, 0, 7)
```

更新策略：仅当 day/hour/minute 变化时才 `markDirty`。

#### Step 1.4: GameScene 集成

**修改** `src/game/scene/game_scene.cpp`

- 从 `initUI()` 移除 `TimeClockUI` 创建代码
- 创建 `TimeClockHud` 实例，加载 `ui/rmlui/hud/time_clock.rml`
- 在 `update()` 中调用 `TimeClockHud::update(game_time)`
- 在 `clean()` 中销毁 `TimeClockHud`（文档由 Scene 自动卸载）

#### Step 1.5: 清理旧代码

- 删除 `src/game/ui/time_clock_ui.h/cpp`
- 从 `src/CMakeLists.txt`（或对应 game 源文件列表）中移除相关条目
- 检查是否有其他文件引用 `TimeClockUI`，一并清理

---

**涉及文件**：

| 操作 | 文件 |
|------|------|
| 修改 | `ui/rmlui/theme/spritesheet.rcss`（补充指针帧 + Extras sprite） |
| 新建 | `ui/rmlui/hud/time_clock.rml` |
| 新建 | `ui/rmlui/hud/time_clock.rcss` |
| 新建 | `src/game/ui/time_clock_hud.h/cpp`（RmlUi 数据驱动层） |
| 修改 | `src/game/scene/game_scene.cpp`（替换初始化 + update） |
| 删除 | `src/game/ui/time_clock_ui.h/cpp` |
| 修改 | `src/CMakeLists.txt`（源文件列表增删） |

**验证**：
- 时钟 HUD 在屏幕左上角显示，位置与旧版一致
- 背景面板和标签背景正确渲染（九宫格拉伸，非拉伸变形）
- 时钟表盘 + 指针正确叠加显示
- 指针随游戏时间推进自动切帧（8 个方向）
- Day/Time 文本动态更新，文本在标签内居中
- GameTime 不存在时显示 fallback（"Day --" / "??:??"）
- 鼠标移动不被时钟 HUD 拦截（body 设置 `pointer-events: none`）

---
