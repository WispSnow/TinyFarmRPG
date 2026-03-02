# L01: 文档结构与第一个界面

> 配套代码：`learn/rmlui_basics/` | 构建目标：`learn_rmlui_basics`

---

## 1. RmlUi 是什么

RmlUi 是一个 **C++ UI 库**，使用类似 HTML/CSS 的标记语言来描述界面。它的核心思想是：

- **RML**（RmlUi Markup Language）≈ HTML —— 描述界面**结构**
- **RCSS**（RmlUi CSS）≈ CSS —— 描述界面**样式**
- **C++ API** —— 处理**交互逻辑**（事件、数据绑定等）

和 Web 开发的区别：RmlUi 运行在游戏引擎内部，没有浏览器，没有 JavaScript。所有动态逻辑都通过 C++ 完成。

---

## 2. RML 文档结构

一个最小的 RML 文档：

```xml
<rml>
<head>
    <link type="text/rcss" href="my_style.rcss"/>
</head>
<body>
    <p>Hello, RmlUi!</p>
</body>
</rml>
```

### 三层结构

| 标签 | 作用 |
|------|------|
| `<rml>` | 根容器，包裹整个文档 |
| `<head>` | 元数据区域：引入样式表、设置标题。**不产生可见元素** |
| `<body>` | 可见内容区域，所有 UI 元素放在这里 |

### `<head>` 中可以放什么

```xml
<head>
    <!-- 引入外部样式表（最常用） -->
    <link type="text/rcss" href="style.rcss"/>

    <!-- 也可以写 text/css，效果完全相同 -->
    <link type="text/css" href="style.rcss"/>

    <!-- 内联样式 -->
    <style>
        p { color: #ff0000; }
    </style>

    <!-- 文档标题（程序可通过 API 读取） -->
    <title>My UI Page</title>
</head>
```

`<link>` 标签的两个必需属性：
- `type` —— 必须是 `"text/rcss"` 或 `"text/css"`
- `href` —— 样式表文件路径（相对于 .rml 文件所在目录）

---

## 3. 基础元素

RmlUi 内置了一组类似 HTML 的元素。在本课中只关注最基础的部分：

### 容器与文本元素

| 元素 | 默认行为 | 说明 |
|------|----------|------|
| `<div>` | block | 通用容器，用于布局分组 |
| `<p>` | block | 段落，带默认上下 margin |
| `<h1>` ~ `<h4>` | block | 标题，字号从大到小 |
| `<span>` | inline | 行内文本容器，不换行 |
| `<br/>` | — | 强制换行 |
| `<hr/>` | block | 水平分隔线 |

> RmlUi 中**任何未注册的标签名**都会被创建为通用元素（和 `<div>` 行为一致）。
> 所以 `<section>`, `<header>`, `<footer>` 等标签也可以使用，只是没有默认样式。

### 图片元素

```xml
<!-- 基本用法 -->
<img src="assets/textures/icon.png" />

<!-- 指定尺寸 -->
<img src="assets/textures/icon.png" width="32" height="32" />

<!-- 从精灵表引用（后续课程详解） -->
<img sprite="icon-sword" />
```

`<img>` 支持的属性：

| 属性 | 说明 |
|------|------|
| `src` | 图片文件路径 |
| `width` / `height` | 显示尺寸（可选，默认为图片原始尺寸） |
| `sprite` | 从 `@spritesheet` 引用精灵名称（L09 详解） |
| `rect` | 源矩形裁切，格式 `"x y w h"` |

### 按钮元素

```xml
<button>Click Me</button>
```

`<button>` 本质上也是一个通用容器元素，但在 RCSS 中你可以针对它单独编写样式和伪类（`:hover`、`:active`）。

### 通用属性

所有元素都支持：

| 属性 | 说明 |
|------|------|
| `id` | 唯一标识符，RCSS 中用 `#id` 选择 |
| `class` | 类名（可空格分隔多个），RCSS 中用 `.class` 选择 |
| `style` | 内联样式（同 HTML），但推荐用 RCSS 文件 |

---

## 4. RCSS 基础

RCSS 的语法几乎与 CSS 相同，但有几个关键差异需要注意：

> **重要**：RmlUi 中所有元素的默认 `display` 是 `inline`（不是 `block`）。
> 因此每个 RCSS 文件开头通常需要加一条重置规则。
> `border` 简写不支持 `solid` 关键字，语法为 `border: <width> <color>;`。

```css
/* 选择器 { 属性: 值; } */

/* 重置块级元素（RmlUi 默认是 inline） */
body, div, h1, h2, h3, h4, p, hr {
    display: block;
}

body {
    margin: 0;
    font-size: 16dp;
    font-family: "VonwaonBitmap 16px";
    color: #e0e0e0;
}

#panel {
    padding: 16dp;
    background-color: #1a1b26e0;
    border: 1dp #565f89;              /* 注意：没有 solid */
    border-radius: 8dp;
}

.card {
    padding: 10dp;
    background-color: #24283b;
}
```

### 选择器

| 选择器 | 示例 | 匹配 |
|--------|------|------|
| 元素选择器 | `div` | 所有 `<div>` |
| ID 选择器 | `#panel` | `id="panel"` 的元素 |
| 类选择器 | `.card` | `class="card"` 的元素 |
| 后代选择器 | `#panel p` | `#panel` 内部所有 `<p>` |
| 子选择器 | `#panel > p` | `#panel` 的直接子 `<p>` |

### 伪类（简单预览）

```css
button:hover  { background-color: #44537b; }  /* 鼠标悬停 */
button:active { background-color: #2f3650; }  /* 鼠标按下 */
```

伪类将在后续课程（L05 事件、L12 键盘导航）中深入讲解。

### `dp` 单位

RmlUi 中最常用的尺寸单位是 `dp`（density-independent pixel，密度无关像素）。
它会根据显示缩放比自动调整，**在高 DPI 屏幕上保持视觉大小一致**。

| 单位 | 说明 |
|------|------|
| `dp` | 密度无关像素（推荐默认使用） |
| `px` | 物理像素 |
| `%` | 相对于父元素 |
| `em` | 相对于当前字号 |

---

## 5. 引擎如何加载 RML 文档

在 TinyFarmRPG 引擎中，RmlUi 已经集成为引擎的一部分。加载一个 RML 文档只需要一行代码：

```cpp
auto& gl_renderer = context_.getGLRenderer();
gl_renderer.loadRmlUiDocument("ui/rmlui/learn_hello.rml");
```

### 引擎内部的完整流程

```
loadRmlUiDocument(path)
    │
    ▼
RmlUILayer::loadDocument(path)
    │
    ├── 关闭当前文档（如有）
    │       current_document_->Close()
    │
    ├── 加载新文档
    │       context_->LoadDocument(path)
    │       │
    │       ├── XML 解析器读取 .rml 文件
    │       ├── 处理 <head>：加载 .rcss 样式表
    │       └── 构建 <body> 元素树
    │
    └── 显示文档
            current_document_->Show()
```

你**不需要**手动初始化 RmlUi、创建 Context 或管理渲染循环——引擎已经处理了这些。

### 引擎已预加载的字体

引擎启动时自动加载了一个像素字体：

```
assets/fonts/VonwaonBitmap-16px.ttf
```

在 RCSS 中通过 `font-family: "VonwaonBitmap 16px"` 引用。
如果需要额外字体，后续课程会讲解如何通过 `Rml::LoadFontFace()` 注册。

---

## 6. 配套代码解析

### 项目结构

```
learn/rmlui_basics/
├── main.cpp                    # 入口：创建 GameApp + 注册场景
├── rmlui_basics_scene.h        # 场景声明
└── rmlui_basics_scene.cpp      # 场景实现：加载 RML 文档

ui/rmlui/
├── learn_hello.rml             # RML 文档
└── learn_hello.rcss            # RCSS 样式表
```

### main.cpp — 程序入口

```cpp
#include "engine/core/context.h"
#include "engine/core/game_app.h"
#include "engine/utils/events.h"
#include "rmlui_basics_scene.h"

#include <SDL3/SDL_main.h>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace {

void setupInitialScene(engine::core::Context& context) {
    // 创建场景实例并推入场景栈
    auto scene = std::make_unique<learn::rmlui::RmlUiBasicsScene>(
        "RmlUiBasics", context);
    context.getDispatcher().trigger<engine::utils::PushSceneEvent>(
        engine::utils::PushSceneEvent{std::move(scene)});
}

} // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    spdlog::set_level(spdlog::level::info);

    engine::core::GameApp app;
    app.registerSceneSetup(setupInitialScene);  // 注册场景初始化回调
    app.run();                                  // 启动主循环
    return 0;
}
```

这是所有 learn target 共用的启动模式：
1. 创建 `GameApp`
2. 注册一个回调，在引擎初始化完成后创建并推入场景
3. 调用 `app.run()` 进入主循环

### rmlui_basics_scene.h — 场景声明

```cpp
#pragma once

#include "engine/scene/scene.h"

namespace learn::rmlui {

class RmlUiBasicsScene final : public engine::scene::Scene {
public:
    using Scene::Scene;   // 继承基类构造函数

    [[nodiscard]] bool init() override;
    void clean() override;
};

} // namespace learn::rmlui
```

### rmlui_basics_scene.cpp — 场景实现

```cpp
#include "rmlui_basics_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <spdlog/spdlog.h>

namespace learn::rmlui {

bool RmlUiBasicsScene::init() {
    if (!Scene::init()) {
        return false;
    }

    auto& gl_renderer = context_.getGLRenderer();
    gl_renderer.setDebugUIEnabled(true);   // 启用 ImGui 调试面板

    // 加载 RML 文档 —— 核心调用
    if (!gl_renderer.loadRmlUiDocument("ui/rmlui/learn_hello.rml")) {
        spdlog::error("Failed to load learn_hello.rml");
        return false;
    }

    spdlog::info("RmlUi basics scene initialized.");
    return true;
}

void RmlUiBasicsScene::clean() {
    Scene::clean();
}

} // namespace learn::rmlui
```

关键点：
- `Scene::init()` 必须先调用（初始化基类）
- `setDebugUIEnabled(true)` 启用 ImGui 调试面板，方便观察
- `loadRmlUiDocument()` 加载 .rml 文件并显示

### learn_hello.rml — RML 文档

```xml
<!-- RmlUi 入门：基本文档结构 -->
<rml>
<head>
    <!-- 引用外部样式表 -->
    <link type="text/rcss" href="learn_hello.rcss"/>
</head>
<body>
    <!-- 主面板容器 -->
    <div id="panel">
        <h1>Hello, RmlUi!</h1>
        <p>This is a minimal RmlUi document for learning.</p>

        <!-- 基本布局：水平排列 -->
        <div class="row">
            <div class="card">Card A</div>
            <div class="card">Card B</div>
        </div>

        <button id="btn-test">Click Me</button>
    </div>
</body>
</rml>
```

逐行解读：
- `<rml>` 根标签包裹整个文档
- `<head>` 通过 `<link>` 引入 `learn_hello.rcss` 样式表
- `<body>` 包含一个 `#panel` 容器
- `#panel` 内部有标题、段落、两列卡片、一个按钮
- 使用 `id` 和 `class` 属性为 RCSS 选择器提供锚点

### learn_hello.rcss — RCSS 样式表

```css
/* === 重置：RmlUi 默认 display 是 inline，需要手动设置块级元素 === */
body, div, h1, h2, h3, h4, p, hr {
    display: block;
}

/* === 全局 === */
body {
    margin: 0;
    font-size: 16dp;                        /* 基础字号 */
    font-family: "VonwaonBitmap 16px";      /* 引擎预加载的字体 */
    color: #e0e0e0;                         /* 默认文字颜色 */
}

/* === 面板容器 === */
#panel {
    position: absolute;                     /* 绝对定位 */
    left: 24dp;
    top: 24dp;
    width: 280dp;
    padding: 16dp;
    background-color: #1a1b26e0;            /* 半透明深色背景（e0 = alpha） */
    border: 1dp #565f89;
    border-radius: 8dp;
}

/* === 标题 === */
h1 {
    margin: 0 0 12dp 0;                     /* 仅保留底部 margin */
    font-size: 20dp;
    color: #9ece6a;                         /* 绿色高亮 */
}

/* === 段落 === */
p {
    margin: 0 0 12dp 0;
    color: #a9b1d6;                         /* 柔和蓝灰 */
}

/* === 水平行布局 === */
.row {
    display: flex;                          /* Flexbox 容器 */
    flex-direction: row;                    /* 水平排列 */
    gap: 8dp;                              /* 子元素间距 */
    margin-bottom: 12dp;
}

/* === 卡片 === */
.card {
    flex: 1;                               /* 等分剩余空间 */
    padding: 10dp;
    background-color: #24283b;
    border: 1dp #414868;
    border-radius: 4dp;
    text-align: center;
}

/* === 按钮 === */
button {
    width: 100%;
    padding: 8dp 12dp;
    background-color: #3b4261;
    border: 1dp #7aa2f7;
    border-radius: 4dp;
    color: #ffffff;
}

button:hover {
    background-color: #44537b;             /* 悬停变亮 */
}

button:active {
    background-color: #2f3650;             /* 按下变暗 */
}
```

---

## 7. 构建与运行

```bash
# 配置（如果还没开启 BUILD_LEARN）
cmake -B build -G Ninja -DBUILD_LEARN=ON

# 编译
ninja -C build learn_rmlui_basics

# 运行
./build/learn/learn_rmlui_basics
```

运行后你应该看到：
- 左上角一个深色半透明面板
- "Hello, RmlUi!" 绿色标题
- 一段灰色说明文字
- 两个并排的卡片（Card A / Card B）
- 一个蓝色边框按钮，鼠标悬停/点击时颜色变化

---

## 8. 练习

### 练习 1：修改文档内容

打开 `ui/rmlui/learn_hello.rml`，尝试：

1. 把标题改为你的名字
2. 在两张卡片下方再添加第三张卡片 Card C
3. 添加一个 `<hr/>` 分隔线，然后在下方写一段 `<p>` 文字
4. 用 `<span>` 在段落中给某个词加上不同颜色（用 `style="color: #ff9e64;"` 内联样式）

修改后重新运行程序查看效果。

### 练习 2：修改样式

打开 `ui/rmlui/learn_hello.rcss`，尝试：

1. 把 `#panel` 的 `background-color` 改为其他颜色（提示：`#RRGGBBAA` 格式，最后两位是透明度）
2. 把 `border-radius` 改为 `0dp`，观察直角 vs 圆角的差异
3. 给 `.card` 添加 `:hover` 伪类，让卡片在鼠标悬停时变亮
4. 把 `#panel` 的 `position` 从 `absolute` 改为默认值（删除这行），观察变化

### 练习 3：创建新页面

创建一个新文件 `ui/rmlui/learn_profile.rml` 和 `learn_profile.rcss`：

1. 做一个"角色简介卡"：角色名（h1）、职业（h3）、简介描述（p）
2. 用 `<img>` 加载一张角色图片（可使用 `assets/textures/` 中已有的图片）
3. 修改 `rmlui_basics_scene.cpp` 中的路径，加载你的新页面

---

## 9. 要点回顾

| 概念 | 要点 |
|------|------|
| 文档结构 | `<rml>` → `<head>` + `<body>` |
| 样式引入 | `<link type="text/rcss" href="..."/>` |
| 基础元素 | `<div>`, `<p>`, `<h1>`~`<h4>`, `<span>`, `<img>`, `<button>`, `<br/>`, `<hr/>` |
| RCSS 选择器 | 元素 / `#id` / `.class` / 后代 / 子代 |
| 推荐单位 | `dp`（密度无关像素） |
| 引擎加载 | `gl_renderer.loadRmlUiDocument("path/to/file.rml")` |

---

**下一课 →** [L02: 盒模型与定位](L02-box-model.md)
