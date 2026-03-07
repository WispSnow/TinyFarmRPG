# L07 补充：`change` 事件与冒泡

> 本文是 [[L07-forms.md]] 的补充，解答"change 事件是什么、如何绑定"及"什么是事件冒泡"两个问题。
> 事件系统基础（EventListener 接口、三阶段传播）参见 [[L05-events.md]]。

---

## 1. `change` 事件是什么？

`change` 是 RmlUi 内置的表单控件事件（`EventId::Change`），当控件**值发生变化**时自动派发，并**向上冒泡**。

从源码确认（`EventSpecification.cpp`）：

```cpp
// { id,           name,      cancellable, bubbles, ... }
{EventId::Change, "change",   false,       true,    ...}
//                                         ^^^^  冒泡开启
```

### 各控件触发时机与事件参数

| 控件 | 触发时机 | `ev.GetParameter<T>(key)` |
|------|---------|--------------------------|
| `input[type=text/password]`、`<textarea>` | 每次字符输入 / 删除 | `"value"`（string）、`"linebreak"`（bool，回车时为 true） |
| `input[type=range]` | 拖动 / 按键改变值 | `"value"`（float） |
| `<select>` | 选中新选项 | `"value"`（string） |
| `input[type=checkbox]` | 勾选 / 取消 | `"value"`（`"on"` 或 `""`）、`"checked"`（bool） |
| `input[type=radio]` | 切换选中 | `"value"`（被选中项的 value 或 `""`）、`"checked"`（bool） |

---

## 2. 三种绑定方式

### 方式一：`data-value` 自动绑定（最简洁）

使用 `data-value` 时，RmlUi 内部**自动**监听 `change` 事件，无需任何额外代码。
控件值改变 → 写入 C++ 变量 → 标脏 → 所有引用该变量的 UI 刷新。

```xml
<input type="range" data-value="bgm_volume" min="0" max="100"/>
<div>当前音量：{{bgm_volume}}%</div>
```

> 只需同步值、无额外逻辑时，首选此方式。

---

### 方式二：`data-event-change`（数据模型回调）

在数据模型作用域内使用，值变化时执行额外 C++ 逻辑：

```xml
<input type="range" data-value="bgm_volume" min="0" max="100"
       data-event-change="on_volume_changed"/>
```

```cpp
constructor.BindEventCallback("on_volume_changed",
    [](Rml::DataModelHandle model, Rml::Event& ev, const Rml::VariantList&) {
        float vol = ev.GetParameter<float>("value", 0.f);
        spdlog::info("volume -> {}", vol);
        // 若需要触发数据模型刷新：
        model.DirtyVariable("bgm_volume");
    });
```

> `data-event-XXX` 的 `XXX` 对应事件名，`change` → `data-event-change`。

---

### 方式三：`AddEventListener`（C++ 监听器）

不依赖数据模型，最灵活：

```cpp
class MyChangeListener : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& ev) override {
        auto id    = ev.GetTargetElement()->GetId();
        auto value = ev.GetParameter<Rml::String>("value", "");
        spdlog::info("[{}] changed -> {}", id, value);
    }
};

// 挂载（可挂在任意祖先元素，利用冒泡统一处理）
doc->GetElementById("settings")->AddEventListener("change", &listener_);

// 卸载文档前必须移除（见 rmlui-guide.md）
doc->GetElementById("settings")->RemoveEventListener("change", &listener_);
```

---

### 三种方式对比

| 方式 | 适用场景 | 需要数据模型 |
|------|---------|------------|
| `data-value` | 只需双向同步值，无额外逻辑 | ✅ 是 |
| `data-event-change` | 值变化时需要执行 C++ 逻辑 | ✅ 是 |
| `AddEventListener` | 无数据模型，或需精细控制 | ❌ 否 |

---

## 3. 事件冒泡

### 什么是冒泡？

事件触发后，会从**目标元素**一路向上传播到根元素，就像气泡从水底浮到水面：

```
        <form>            ← ③ 最后传到这里
           │
        <div>             ← ② 再传到父元素
           │
    <input type="range">  ← ① change 事件在这里发生
```

用户拖动 `<input>` → 触发 `change` → 自动冒泡经过 `<div>` → 到达 `<form>`。

### 利用冒泡：一个监听器管所有子控件

**不用冒泡**（每个控件单独绑）：

```cpp
doc->GetElementById("vol")  ->AddEventListener("change", &listener_);
doc->GetElementById("speed")->AddEventListener("change", &listener_);
doc->GetElementById("lang") ->AddEventListener("change", &listener_);
// 每新增控件就要加一行，且要逐一 Remove
```

**利用冒泡**（挂在父元素一次搞定）：

```cpp
doc->GetElementById("settings")->AddEventListener("change", &listener_);
```

```cpp
void ProcessEvent(Rml::Event& ev) override {
    // GetTargetElement() → 真正触发事件的子控件
    // GetCurrentElement() → 挂监听器的父元素（settings form）
    Rml::String id    = ev.GetTargetElement()->GetId();
    Rml::String value = ev.GetParameter<Rml::String>("value", "");

    if (id == "vol")   { /* 处理音量 */ }
    if (id == "speed") { /* 处理速度 */ }
    if (id == "lang")  { /* 处理语言 */ }
}
```

### 阻止冒泡

某个中间元素处理完后，不想让事件继续向上传播：

```cpp
ev.StopPropagation();
```

---

## 要点总结

| 概念 | 说明 |
|------|------|
| `change` 事件 | 表单控件值变化时触发，携带 `value` 参数 |
| 冒泡 | 事件从触发元素向祖先元素逐级传播 |
| `ev.GetTargetElement()` | 真正触发事件的元素 |
| `ev.GetCurrentElement()` | 当前执行监听器的元素 |
| `ev.StopPropagation()` | 阻止继续向上冒泡 |
| `data-value` | 内部自动监听 `change`，无需手动绑定 |
| `data-event-change` | 数据模型内的 `change` 回调 |
| `AddEventListener("change", ...)` | 纯 C++ 监听，可挂在父元素利用冒泡 |
