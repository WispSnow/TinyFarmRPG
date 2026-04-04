# L08: 自定义元素与文档管理

> 配套代码：`learn/rmlui_custom_elements/` | 构建目标：`learn_rmlui_custom_elements`

---

## 1. 为什么需要自定义元素？

RmlUi 内置了 `<div>`、`<input>`、`<progress>` 等通用元素。
但游戏 UI 中经常需要**语义化的可复用组件**——例如 HP 条、技能图标、迷你地图等。

自定义元素允许我们：
- 定义新标签名（如 `<hp-bar>`），在 RML 中像内置元素一样使用
- 封装渲染逻辑和属性响应，让 RML 侧保持声明式
- 配合 `data-attr-*` 数据绑定实现自动更新

---

## 2. 创建自定义元素类

自定义元素继承自 `Rml::Element`，重写生命周期方法：

```cpp
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

class ElementHpBar : public Rml::Element {
public:
    explicit ElementHpBar(const Rml::String& tag) : Rml::Element(tag) {}

    // 属性变化时调用
    void OnAttributeChange(const Rml::ElementAttributes& changed) override {
        Element::OnAttributeChange(changed);  // 必须调用父类
        // 处理自定义属性...
    }

    // 元素被加入 DOM 时调用
    void OnChildAdd(Rml::Element* child) override {
        Element::OnChildAdd(child);
        if (child == this) {
            // 自身被加入 DOM，初始化内部结构
        }
    }
};
```

### 生命周期方法一览

| 方法 | 触发时机 |
|------|---------|
| `OnChildAdd(child)` | 子元素（或自身）被加入 DOM。`child == this` 表示自身被插入 |
| `OnChildRemove(child)` | 子元素（或自身）被移除。`child == this` 表示自身被移除 |
| `OnAttributeChange(changed)` | 元素的 HTML 属性发生变化 |
| `OnPropertyChange(changed)` | 元素的 CSS 属性发生变化 |
| `OnUpdate()` | 每帧更新（在 `Context::Update()` 中） |
| `OnRender()` | 渲染前调用 |
| `OnLayout()` | 布局计算时调用 |
| `OnResize()` | 元素尺寸变化时调用 |

> **注意**：`OnChildAdd` / `OnChildRemove` 会为**两层以内**的后代触发，不只是直接子元素。并且可能以 `child == this` 调用来表示自身被添加/移除。

---

## 3. 注册自定义元素

使用 `Rml::ElementInstancerGeneric<T>` 模板和 `Rml::Factory` 注册：

```cpp
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Factory.h>

// 1. 创建 instancer（必须 static，生命周期持续到 Rml::Shutdown()）
static Rml::ElementInstancerGeneric<ElementHpBar> hp_bar_instancer;

// 2. 注册标签名 → instancer 映射
Rml::Factory::RegisterElementInstancer("hp-bar", &hp_bar_instancer);
```

注册后，在 RML 中即可直接使用：

```xml
<hp-bar value="80" max="100"/>
```

### 注册时机

- 必须在 **加载使用该标签的 RML 文档之前** 注册
- 全局注册一次即可，后续所有文档共享
- instancer 对象必须保持存活（static 变量最安全）

---

## 4. 实战：`<hp-bar>` 自定义元素

### 设计

| 属性 | 类型 | 说明 |
|------|------|------|
| `value` | float | 当前值 |
| `max` | float | 最大值 |
| `bar-color` | string | 固定填充色（留空则按百分比自动变色） |

### 核心实现

```cpp
class ElementHpBar : public Rml::Element {
public:
    explicit ElementHpBar(const Rml::String& tag) : Rml::Element(tag) {}

    void OnAttributeChange(const Rml::ElementAttributes& changed) override {
        Element::OnAttributeChange(changed);

        if (auto it = changed.find("value"); it != changed.end())
            value_ = it->second.Get<float>(0.0f);
        if (auto it = changed.find("max"); it != changed.end())
            max_ = it->second.Get<float>(100.0f);
        if (auto it = changed.find("bar-color"); it != changed.end())
            fixed_color_ = it->second.Get<Rml::String>("");

        updateFill();
    }

    void OnChildAdd(Rml::Element* child) override {
        Element::OnChildAdd(child);
        if (child == this) {
            // 读取初始属性
            value_ = GetAttribute("value", 0.0f);
            max_   = GetAttribute("max", 100.0f);
            fixed_color_ = GetAttribute("bar-color", Rml::String(""));
            createFill();
        }
    }

private:
    void createFill() {
        if (fill_) return;
        auto el = GetOwnerDocument()->CreateElement("div");
        el->SetClassNames("hp-bar-fill");
        fill_ = AppendChild(std::move(el));
        updateFill();
    }

    void updateFill() {
        if (!fill_) return;
        float pct = (max_ > 0.0f) ? (value_ / max_ * 100.0f) : 0.0f;
        pct = std::clamp(pct, 0.0f, 100.0f);

        fill_->SetProperty("width", std::to_string(static_cast<int>(pct)) + "%");

        if (!fixed_color_.empty()) {
            fill_->SetProperty("background-color", fixed_color_);
        } else {
            // 自动变色：绿 → 黄 → 红
            if (pct > 50.0f)  fill_->SetProperty("background-color", "#2ea043");
            else if (pct > 25.0f) fill_->SetProperty("background-color", "#e0af68");
            else fill_->SetProperty("background-color", "#f7768e");
        }
    }

    float value_ = 0.0f;
    float max_   = 100.0f;
    Rml::String fixed_color_;
    Rml::Element* fill_ = nullptr;
};
```

### 配合数据绑定

`data-attr-*` 将数据变量绑定到元素属性，自动触发 `OnAttributeChange`：

```xml
<!-- hp 变化 → value 属性更新 → OnAttributeChange → 填充条刷新 -->
<hp-bar data-attr-value="hp" data-attr-max="max_hp"/>

<!-- 固定蓝色 MP 条 -->
<hp-bar bar-color="#1f6feb" data-attr-value="mp" data-attr-max="max_mp"/>
```

### RCSS 样式

```css
hp-bar {
    display: block;
    width: 130dp;
    height: 10dp;
    background-color: #1e2540;
    border: 1dp #2d4060;
    border-radius: 2dp;
    overflow: hidden;
}
hp-bar .hp-bar-fill {
    display: block;
    height: 100%;
    border-radius: 2dp;
}
```

---

## 5. DOM 查询

RmlUi 提供了多种 DOM 查询方法：

```cpp
// 通过 ID 查找（最常用）
auto* elem = document->GetElementById("action-log");

// CSS 选择器查询（返回第一个匹配）
auto* btn = document->QuerySelector(".btn-primary");

// CSS 选择器查询（返回所有匹配）
Rml::ElementList results;
document->QuerySelectorAll(results, ".menu-item");

// 通过标签名查找
document->GetElementsByTagName(results, "hp-bar");

// 通过类名查找
document->GetElementsByClassName(results, "active");

// 向上查找祖先
auto* panel = element->Closest(".panel");
```

---

## 6. 动态 DOM 操作

### CreateElement + AppendChild — 创建并插入元素

```cpp
// 创建新元素
auto entry = document->CreateElement("div");
entry->SetClassNames("log-entry");

// 设置内容
entry->SetInnerRML("&gt; Took 15 damage!");

// 添加到容器（所有权转移给 DOM 树）
auto* log = document->GetElementById("action-log");
log->AppendChild(std::move(entry));
```

### RemoveChild — 移除子元素

```cpp
// 移除第一个子元素（返回 unique_ptr，可以丢弃或复用）
auto removed = log->RemoveChild(log->GetFirstChild());

// 保持日志最多 6 条
while (log->GetNumChildren() > 6) {
    log->RemoveChild(log->GetFirstChild());
}
```

### SetInnerRML — 替换全部内容

```cpp
// 用 RML 字符串替换所有子元素
element->SetInnerRML("<span class='highlight'>New content</span>");

// 清空内容
element->SetInnerRML("");
```

### 其它操作

| 方法 | 说明 |
|------|------|
| `InsertBefore(new_elem, ref_elem)` | 在指定子元素前插入 |
| `ReplaceChild(new_elem, old_elem)` | 替换子元素 |
| `CreateTextNode(text)` | 创建纯文本节点 |

### 导航

```cpp
element->GetParentNode();
element->GetFirstChild();
element->GetLastChild();
element->GetChild(index);
element->GetNumChildren();
element->GetNextSibling();
element->GetPreviousSibling();
```

---

## 7. 多文档管理

RmlUi 支持在同一个 `Rml::Context` 中同时加载多个文档，各自独立渲染和交互。

### 加载文档

```cpp
// 引擎提供的便捷接口（带场景所有权管理）
auto* hud  = loadRmlDocument("ui/rmlui/learn/learn_custom_hud.rml");
auto* menu = loadRmlDocument("ui/rmlui/learn/learn_custom_menu.rml");
```

### 显示 / 隐藏

```cpp
// 显示文档（可选模态 + 焦点策略）
menu->Show();                                  // 普通显示
menu->Show(Rml::ModalFlag::Modal);             // 模态（阻止下层交互）
menu->Show(Rml::ModalFlag::None,
           Rml::FocusFlag::Document);           // 显示并聚焦

// 隐藏文档（不销毁，可以再次 Show）
menu->Hide();

// 关闭文档（隐藏并标记销毁，下次 Update 时释放）
menu->Close();
```

### 文档层级

多个文档按加载顺序堆叠。可以调整层级：

```cpp
// 拉到最前面（覆盖其他文档）
menu->PullToFront();

// 推到最后面（被其他文档覆盖）
menu->PushToBack();
```

### 典型用法：HUD + 弹出菜单

```cpp
bool CustomElementsScene::init() {
    // 加载 HUD（底层，始终可见）
    hud_doc_ = loadRmlDocument("ui/rmlui/learn/learn_custom_hud.rml");

    // 加载菜单（顶层，初始隐藏）
    menu_doc_ = loadRmlDocument("ui/rmlui/learn/learn_custom_menu.rml");
    menu_doc_->Hide();
    return true;
}

// 切换菜单显示
void toggleMenu() {
    if (menu_visible_) {
        menu_doc_->Hide();
    } else {
        menu_doc_->Show();
        menu_doc_->PullToFront();
    }
    menu_visible_ = !menu_visible_;
}
```

---

## 8. 多文档共享数据模型

数据模型注册在 `Rml::Context` 上，同一 Context 中的所有文档可以共享同一个模型。
只需在各文档的 `<body>` 上指定相同的 `data-model` 名称：

HUD 文档：
```xml
<body data-model="demo">
    <hp-bar data-attr-value="hp" .../>
</body>
```

Menu 文档：
```xml
<body data-model="demo">
    <div data-event-click="close_menu">Close</div>
</body>
```

两个文档中的 `{{hp}}`、`data-event-click` 等绑定表达式都引用同一个数据模型实例。

---

## 9. 配套代码

### 文件结构

| 文件 | 说明 |
|------|------|
| `element_hp_bar.h` | 自定义 `<hp-bar>` 元素（header-only） |
| `custom_elements_scene.h/cpp` | 场景：注册元素 + 数据模型 + DOM 操作 + 双文档管理 |
| `main.cpp` | 入口 |
| `learn_custom_hud.rml` | HUD 文档：`<hp-bar>` + 操作按钮 + 行动日志 |
| `learn_custom_menu.rml` | 菜单文档：全屏覆盖 + 菜单面板 |
| `learn_custom_elements.rcss` | 两个文档共用的样式 |

### 演示内容

| 功能 | 使用的 API |
|------|-----------|
| HP/MP 条 | 自定义 `<hp-bar>` + `data-attr-value` |
| Damage/Heal 按钮 | `BindEventCallback` + `DirtyVariable` |
| 行动日志 | `GetElementById` + `CreateElement` + `AppendChild` + `RemoveChild` |
| Clear Log | `SetInnerRML("")` |
| Menu 按钮 | `Show()` / `Hide()` / `PullToFront()` |
| Menu 关闭 | 跨文档共享数据模型的 `close_menu` 回调 |

---

## 10. 构建与运行

```bash
ninja -C build/debug learn_rmlui_custom_elements
cd build/debug/learn && ./learn_rmlui_custom_elements
```

运行后可观察：
- `<hp-bar>` 根据 HP 百分比自动变色（绿 → 黄 → 红）
- MP 条使用 `bar-color` 固定蓝色
- 点击 Damage/Heal → HP 条实时更新（`data-attr-value` 绑定）
- 点击 MP -10 → MP 条更新
- 行动日志区域动态添加条目（`CreateElement` + `AppendChild`），超过 6 条自动移除最早的
- 点击 Clear Log → 清空日志（`SetInnerRML("")`）
- 点击 Menu → 半透明覆盖层 + 居中菜单面板（第二文档 `Show` + `PullToFront`）
- 菜单中点击 Close → 菜单隐藏（`Hide()`）

---

## 11. 练习

### 练习 1：EXP 条

创建自定义 `<exp-bar>` 元素，属性为 `exp` 和 `next_level`。
内部显示经验值百分比，满 100% 时填充条闪烁（通过 `@keyframes` 动画）。
在 HUD 中添加到 HP/MP 下方。

### 练习 2：弹窗确认

在菜单中点击 "Save" 时，用 `CreateElement` 动态创建一个确认弹窗。
弹窗包含 "Save to Slot 1?" 文字和 "Yes/No" 按钮。
点击 "Yes" 后用 `RemoveChild` 移除弹窗。

### 练习 3：QuerySelectorAll

在 HUD 文档中添加多个 `<hp-bar>` 表示队伍成员。
用 `QuerySelectorAll("hp-bar")` 获取所有 HP 条，
在 Damage 回调中让所有队员同时受伤（全体伤害效果）。

---

## 12. 要点回顾

| 概念 | 要点 |
|------|------|
| 自定义元素 | 继承 `Rml::Element`，重写 `OnChildAdd` / `OnAttributeChange` |
| 元素注册 | `ElementInstancerGeneric<T>` + `Factory::RegisterElementInstancer("tag", &inst)` |
| instancer 生命周期 | 必须 static / 全局存活到 `Rml::Shutdown()` |
| OnChildAdd | `child == this` 表示自身被加入 DOM，此时可初始化内部结构 |
| OnAttributeChange | 配合 `data-attr-*` 实现数据绑定驱动的属性更新 |
| DOM 查询 | `GetElementById` / `QuerySelector` / `QuerySelectorAll` / `Closest` |
| DOM 操作 | `CreateElement` + `AppendChild` 插入；`RemoveChild` 移除；`SetInnerRML` 替换 |
| 多文档 | 同一 Context 可加载多个文档，各自独立渲染 |
| 文档层级 | `Show()` / `Hide()` 控制可见性；`PullToFront()` / `PushToBack()` 控制层级 |
| 共享数据模型 | 多个文档的 `data-model` 指向同一个模型名即可共享 |

---

**上一课 <-** [L07: 表单控件](L07-forms.md)
**下一课 ->** [L09: 精灵表与九宫格装饰器](L09-spritesheet.md)
