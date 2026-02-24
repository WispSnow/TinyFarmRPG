# 逻辑分辨率与视口（Letterbox）：Window / Logical / World 坐标系速查

> 用途：解释 TinyFarm 的“逻辑分辨率渲染 + 信箱(letterbox)适配窗口”架构，并统一三个常见坐标系的边界与换算方式。  
> 本文档也会影响输入（鼠标坐标）、相机与视口剔除等模块的理解。

---

## 1) 三种坐标系：你正在用哪个？

### 1.1 Window Coordinates（窗口坐标）
**含义**：SDL 的“窗口坐标尺寸”。  
**获取**：`SDL_GetWindowSize()`  
**典型用途**：
- SDL 鼠标事件坐标：`event.motion.x/y`、`event.button.x/y`、`SDL_GetMouseState()`（都写明“relative to window”）
- 输入侧的 letterbox 映射：把鼠标从 window 坐标映射到 logical 坐标

> 高 DPI 下：window 坐标尺寸可能 **小于** drawable 的像素尺寸。

### 1.2 Window Pixels（窗口像素 / drawable 像素）
**含义**：OpenGL 实际渲染输出的像素尺寸（drawable）。  
**获取**：`SDL_GetWindowSizeInPixels()`  
**典型用途**：
- OpenGL viewport：`glViewport` 的单位是像素
- 渲染侧的 letterbox 计算：把 logical 画面“等比缩放”到窗口像素里，并居中留黑边

### 1.3 Logical Coordinates（逻辑分辨率坐标）
**含义**：项目约定的“固定渲染坐标空间”，也是 UI 的坐标空间。  
**来源**：`config/window.json` 的 `width/height` 与 `logical_scale` 共同决定：
- `logical_size = (width * logical_scale, height * logical_scale)`

**典型用途**：
- 作为离屏渲染目标（Scene/Lighting/Emissive）与 UI 的“设计分辨率”
- 作为 Camera 的屏幕坐标空间（相机把 world 映射到 logical）
- InputManager 把鼠标映射到 logical，保证 UI/交互基于一致的坐标空间

### 1.4 World Coordinates（世界坐标）
**含义**：ECS 中实体的位置坐标（`TransformComponent.position_`）。  
**典型用途**：
- 玩法系统（移动/碰撞/空间索引）
- 渲染系统：相机把 world → logical，再提交绘制
- 视口剔除：用相机在 world 中的 view rect 判断“完全在视野外就不提交 draw”

---

## 2) Letterbox（信箱）是怎么计算的？

给定：
- `W` = window size（注意：可以是 window coordinates，也可以是 window pixels；取决于你在哪个模块使用）
- `L` = logical size

letterbox 的核心是 **等比缩放**：
- `scale = min(W.x / L.x, W.y / L.y)`
- `viewport.size = L * scale`
- `viewport.pos = (W - viewport.size) * 0.5`

项目里对应函数：
- `engine::utils::computeLetterboxMetrics()`（`src/engine/utils/math.h`）

它会返回：
- `viewport`：居中后的可用区域（用来画 logical 画面）
- `scale`：窗口单位 / logical 单位 的缩放因子

---

## 3) 渲染侧：为什么要“逻辑分辨率 → 合成到窗口”？

核心直觉：
> 先把世界渲染到固定大小的离屏缓冲（logical size），再把结果等比缩放到任意窗口尺寸（viewport），这样画面不会随窗口比例变化而变形。

数据流线索：
- `GameApp::initGLRenderer()` 计算并传入 `logical_size`（来自 `logical_scale`）
- `GLRenderer::present()`：
  - Scene/Lighting/Emissive 先在 **logical 分辨率** 的 FBO 上执行
  - `ViewportManager` 用 **window pixels** 计算 viewport（letterbox），并应用 `glViewport`
  - `CompositePass` 把 logical 的结果合成到 viewport（默认帧缓冲）
  - `UIPass` 也绘制到 viewport（UI 坐标按 logical 设计）

对应文件：
- `src/engine/render/opengl/gl_renderer.cpp`
- `src/engine/render/opengl/viewport_manager.cpp`

---

## 4) 输入侧：鼠标为什么要映射到 logical？

如果直接用鼠标的 window 坐标（Window Coordinates）做 UI/交互，会遇到两类问题：
1) window 尺寸变化导致 UI 交互“手感”变化
2) letterbox 黑边区域里鼠标坐标没有意义（应该 clamp 到画面内）

因此项目做了统一换算：
- 输入事件坐标（window coordinates）→ 通过 `computeLetterboxMetrics(window_size, logical_size)` 映射到 logical
- 并把结果 clamp 到 `[0, logical_size]`

对应文件：
- `src/engine/input/input_manager.cpp`：`InputManager::recalculateLogicalMousePosition()`

---

## 5) 相机：world 怎么变成 logical？

Camera 在项目里的约定是：
- 屏幕坐标空间就是 `logical_size`
- 相机中心在 `logical_size * 0.5` 处
- zoom 影响“看到的 world 范围”（`view_size_world = logical_size / zoom`）

线索文件：
- `src/engine/render/camera.h/.cpp`
- `src/engine/render/opengl/gl_renderer.cpp`：`computeViewProjection()` / `computeCameraViewRect()`

---

## 6) 调试验证：推荐同时打开三个面板

1) `Core: Game State`：Window Size / Logical Size / Camera View Size（world）  
2) `Input`：Mouse Position（window） / Logical Position（logical）  
3) `OpenGL Renderer`：Viewport Clipping、View Rect（world）、Pass Stats（以及 viewport/scale 信息）

当你拖拽窗口大小时，你应该能观察到：
- Window Size 与 Window Pixels（高 DPI 下）不一定相等
- Logical Size 保持不变（除非你修改配置或做动态逻辑分辨率切换）
- Viewport（letterbox）会变化，但画面不应被拉伸变形

