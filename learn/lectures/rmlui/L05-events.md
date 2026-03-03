# L05: 事件系统

> 配套代码：`learn/rmlui_events/` | 构建目标：`learn_rmlui_events`

---

## 1. RmlUi 事件模型

RmlUi 的事件模型与 W3C DOM 事件流一致，分三个阶段：

```
捕获阶段（Capture）    目标阶段（Target）    冒泡阶段（Bubble）
       body                                      body
        ↓                                          ↑
       div                                        div
        ↓                                          ↑
      button  ─────────→  button  ─────────→   button
```

1. **捕获**：从根元素向下传播到目标
2. **目标**：到达触发事件的元素
3. **冒泡**：从目标向上传播到根元素

默认情况下，事件监听器在**冒泡阶段**触发。

---

## 2. EventListener 接口

实现 `Rml::EventListener` 来处理事件：

```cpp
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Event.h>

class MyListener : public Rml::EventListener {
public:
    // 必须实现：处理事件
    void ProcessEvent(Rml::Event& event) override {
        // 处理逻辑
    }

    // 可选：监听器被附加到元素时调用
    void OnAttach(Rml::Element* element) override {}

    // 可选：监听器从元素分离时调用
    void OnDetach(Rml::Element* element) override {}
};
```

---

## 3. 注册监听器

通过 `Element::AddEventListener()` 注册：

```cpp
// 用事件名称字符串
element->AddEventListener("click", &my_listener);

// 用 EventId 枚举（性能更好）
element->AddEventListener(Rml::EventId::Click, &my_listener);

// 在捕获阶段监听（第三个参数）
element->AddEventListener("click", &my_listener, true);
```

移除监听器：

```cpp
element->RemoveEventListener("click", &my_listener);
```

> 监听器必须在 `AddEventListener` 和 `OnDetach` 回调之间保持存活。

---

## 4. Event 对象

`Rml::Event` 提供事件的完整信息：

### 事件类型

```cpp
void ProcessEvent(Rml::Event& event) override {
    // 用字符串比较
    if (event == "click") { ... }

    // 用 EventId 比较（推荐）
    if (event.GetId() == Rml::EventId::Click) { ... }

    // 获取类型名称
    const Rml::String& type = event.GetType();  // "click"
}
```

### 目标元素

```cpp
// 触发事件的原始元素
Rml::Element* target = event.GetTargetElement();

// 当前正在处理事件的元素（冒泡过程中可能不同）
Rml::Element* current = event.GetCurrentElement();
```

### 事件参数

```cpp
// 鼠标事件参数
int mouse_x = event.GetParameter("mouse_x", 0);
int mouse_y = event.GetParameter("mouse_y", 0);
int button   = event.GetParameter("button", 0);  // 0=左 1=右 2=中

// 键盘事件参数
auto key = event.GetParameter<Rml::Input::KeyIdentifier>(
    "key_identifier", Rml::Input::KeyIdentifier::KI_UNKNOWN);

// 元素属性读取
Rml::String id = target->GetId();
Rml::String attr = target->GetAttribute<Rml::String>("data-value", "");
```

### 传播控制

```cpp
// 阻止事件继续冒泡，但同一元素上的其他监听器仍会执行
event.StopPropagation();

// 立即停止，同一元素上的后续监听器也不再执行
event.StopImmediatePropagation();
```

---

## 5. 常用事件类型

| 事件 | 触发时机 | 常用参数 |
|------|----------|----------|
| `click` | 鼠标点击（按下并释放） | `mouse_x`, `mouse_y`, `button` |
| `dblclick` | 双击 | 同上 |
| `mousedown` / `mouseup` | 鼠标按下 / 释放 | 同上 |
| `mouseover` | 鼠标移入元素 | `mouse_x`, `mouse_y` |
| `mouseout` | 鼠标移出元素 | 同上 |
| `mousemove` | 鼠标在元素上移动 | 同上 |
| `keydown` / `keyup` | 键盘按键 | `key_identifier` |
| `focus` / `blur` | 获得 / 失去焦点 | — |
| `change` | 表单控件值改变 | `value` |
| `submit` | 表单提交 | — |
| `animationend` | CSS 动画结束 | — |
| `transitionend` | CSS 过渡结束 | — |

---

## 6. DOM 访问

从 `Rml::ElementDocument` 查询元素：

```cpp
// 通过 ID 查找（最常用）
Rml::Element* btn = document->GetElementById("attack-btn");

// 通过标签名查找
Rml::ElementList buttons;
document->GetElementsByTagName(buttons, "button");

// 通过 class 名查找
Rml::ElementList items;
document->GetElementsByClassName(items, "menu-item");

// CSS 选择器查找
Rml::Element* first = document->QuerySelector(".active");
Rml::ElementList all;
document->QuerySelectorAll(all, ".menu-item.active");
```

### 动态修改元素

```cpp
// 修改文本内容
element->SetInnerRML("New Text");

// 修改属性
element->SetAttribute("data-state", "active");

// 修改 class
element->SetClass("highlight", true);   // 添加
element->SetClass("highlight", false);  // 移除

// 修改内联样式
element->SetProperty("background-color", "#ff0000");
element->SetProperty("opacity", "0.5");
```

---

## 7. 引擎中的 RmlEventBridge

TinyFarmRPG 引擎提供了 `RmlEventBridge`，通过 `data-command` 属性简化事件分发：

```cpp
#include "engine/ui/rmlui/rml_event_bridge.h"

// 注册命名回调
bridge_.on("attack", [](Rml::Event&) {
    spdlog::info("Attack!");
});
bridge_.on("flee", [](Rml::Event&) {
    spdlog::info("Flee!");
});

// 注册到文档（冒泡阶段在 body 上统一监听）
bridge_.registerTo(document->GetElementById("menu"), "click");
```

RML 侧用 `data-command` 指定回调名：

```xml
<div id="menu">
    <button data-command="attack">Attack</button>
    <button data-command="flee">Flee</button>
</div>
```

`RmlEventBridge` 会沿 DOM 树向上查找 `data-command` 属性，找到后调用对应回调。

---

## 8. 配套代码

### 场景代码

`learn/rmlui_events/events_scene.cpp` — 演示三种事件处理模式：

1. **直接 EventListener** — 自定义 `Rml::EventListener` 子类，注册到单个元素
2. **RmlEventBridge** — 引擎提供的命令分发桥，通过 `data-command` 匹配
3. **动态 DOM 操作** — 点击后修改元素内容和样式

### RML 文档

文档包含 3 个 demo 区块：

1. **Click Counter** — 点击按钮，C++ 端累加计数并更新显示
2. **Command Menu** — 5 个 JRPG 命令按钮，通过 RmlEventBridge 分发
3. **Hover Info** — 鼠标悬停显示不同提示信息

---

## 9. 构建与运行

```bash
ninja -C build/debug learn_rmlui_events
./build/debug/learn/learn_rmlui_events
```

---

## 10. 练习

### 练习 1：颜色切换器

创建 3 个颜色按钮（红/绿/蓝），点击后改变一个预览区域的 `background-color`。
使用 `element->SetProperty()` 动态修改样式。

### 练习 2：计数器

实现 +/- 两个按钮和一个数字显示：
- 点击 + 数字加 1，点击 - 数字减 1
- 不能低于 0
- 通过 `SetInnerRML()` 更新显示

### 练习 3：信息面板

3 个菜单项（Items / Skills / Status），鼠标悬停时在右侧面板显示不同的描述文本。
使用 `mouseover` 事件 + `SetInnerRML()` 实现。

---

## 11. 要点回顾

| 概念 | 要点 |
|------|------|
| 事件流 | 捕获 → 目标 → 冒泡（默认监听冒泡阶段） |
| EventListener | 实现 `ProcessEvent(Rml::Event&)`，可选 `OnAttach` / `OnDetach` |
| 注册监听 | `element->AddEventListener("click", &listener)` |
| 事件比较 | `event == "click"` 或 `event.GetId() == Rml::EventId::Click` |
| 目标元素 | `event.GetTargetElement()` / `event.GetCurrentElement()` |
| 参数读取 | `event.GetParameter<T>("key", default)` |
| 传播控制 | `StopPropagation()` / `StopImmediatePropagation()` |
| DOM 查询 | `GetElementById()` / `GetElementsByClassName()` / `QuerySelector()` |
| DOM 修改 | `SetInnerRML()` / `SetProperty()` / `SetClass()` / `SetAttribute()` |
| RmlEventBridge | 引擎桥接器，用 `data-command` 属性做命令分发 |

---

**上一课 <-** [L04: 文字排版与视觉样式](L04-styling.md)
**下一课 ->** [L06: 数据绑定](L06-data-binding.md)
