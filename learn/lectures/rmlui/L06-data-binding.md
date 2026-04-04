# L06: 数据绑定

> 配套代码：`learn/rmlui_data_binding/` | 构建目标：`learn_rmlui_data_binding`

---

## 1. 什么是 Data Model？

L05 的事件处理方式是**命令式**的：用户操作 → C++ 回调 → 手动调用 `SetInnerRML()` 刷新界面。
数据绑定是**声明式**的：C++ 持有数据 → 数据变化时通知 RmlUi → 界面**自动**更新。

```
C++ 数据                    RmlUi 界面
---------                   -----------
char_data_.hp = 75  ──→  {{hp}}  自动变为 75
                          data-style-width 自动更新
                          data-if 自动显示/隐藏
```

核心 API 均在 `Rml::DataModelConstructor` 和 `Rml::DataModelHandle` 中。

---

## 2. 创建数据模型

数据模型在 **加载 RML 文档之前** 创建，否则文档中的绑定表达式无法解析。

```cpp
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

// 通过 Rml::Context 创建模型
Rml::DataModelConstructor constructor = rml_context->CreateDataModel("character");
if (!constructor) {
    // 同名模型已存在，或 context 无效
    return false;
}

// ... 注册类型 / 绑定变量 / 注册回调 ...

// 保存句柄，后续用于标记脏数据
Rml::DataModelHandle model_handle = constructor.GetModelHandle();
```

模型名称（`"character"`）需要与 RML 中 `data-model="character"` 对应。

```xml
<body data-model="character">
    ...
</body>
```

模型作用域是该元素及其所有后代元素。一个页面可以有多个不同名的模型。

---

## 3. 标量绑定与文本插值

### Bind — 绑定标量变量

```cpp
std::string name = "Aelindra";
int hp = 85, max_hp = 100;
bool is_poisoned = false;

constructor.Bind("name",        &name);
constructor.Bind("hp",          &hp);
constructor.Bind("max_hp",      &max_hp);
constructor.Bind("is_poisoned", &is_poisoned);
```

`Bind` 支持的类型：`std::string`（`Rml::String`）、`int`、`float`、`double`、`bool`，以及已注册的结构体 / 数组。

### 文本插值 `{{var}}`

```xml
<p>{{name}}  Lv.{{level}}</p>
<p>HP: {{hp}} / {{max_hp}}</p>
```

花括号内可以写任意数据表达式，不只是变量名：

```xml
<p>HP百分比: {{hp * 100 / max_hp}}%</p>
<p>你好，{{name + "！"}}</p>
```

---

## 4. 动态样式 `data-style-*`

`data-style-PROPERTY` 允许用数据表达式控制任意 CSS 属性：

```xml
<!-- 根据 hp/max_hp 比例动态设置进度条宽度 -->
<div class="bar-fill" data-style-width="hp * 100 / max_hp + '%'"></div>
```

```xml
<!-- 根据条件改变颜色 -->
<span data-style-color="hp < 25 ? '#ff4444' : '#9ece6a'">{{hp}}</span>
```

表达式求值为字符串后直接作为该属性的值，语法与 RCSS 属性值相同。

---

## 5. 条件显示 `data-if` / `data-visible`

### data-if — 从 DOM 中移除/插入

```xml
<span class="poison-badge" data-if="is_poisoned">Poisoned</span>
<span class="normal"       data-if="!is_poisoned &amp;&amp; !is_stunned">Normal</span>
```

> **注意**：RML 是 XML，逻辑与 `&&` 需要写成 `&amp;&amp;`，否则 XML 解析失败。

`data-if` 为 `false` 时元素**不存在于 DOM 中**，不参与布局。

### data-visible — 只控制可见性

```xml
<!-- 元素仍占用布局空间，只是 visibility: hidden -->
<div data-visible="show_hint">提示文字</div>
```

---

## 6. 动态 class `data-class-*`

```xml
<!-- 当 skill.mp_cost > mp 时，给元素添加 .mp-low 样式类 -->
<div class="skill-row" data-class-mp-low="skill.mp_cost > mp">
    ...
</div>
```

RCSS 中对应定义：

```css
.skill-row.mp-low {
    background-color: #1a1b26;
    color: #414868;
}
```

`data-class-CLASSNAME="expression"` 会根据表达式的布尔值动态添加 / 移除 `CLASSNAME` 这个 CSS 类。

---

## 7. 列表渲染 `data-for`

### 绑定数组

```cpp
std::vector<std::string> items = {"Potion", "Ether", "Elixir"};
constructor.RegisterArray<std::vector<std::string>>();
constructor.Bind("items", &items);
```

```xml
<div data-for="item : items">
    <span>{{item}}</span>
</div>
```

### 结构体数组

若数组元素是自定义结构体，需先注册结构体类型：

```cpp
struct Skill {
    std::string name;
    std::string type;
    int mp_cost = 0;
};

// 1. 注册结构体成员（必须在注册数组之前）
if (auto h = constructor.RegisterStruct<Skill>()) {
    h.RegisterMember("name",    &Skill::name);
    h.RegisterMember("type",    &Skill::type);
    h.RegisterMember("mp_cost", &Skill::mp_cost);
}
// 2. 注册对应数组类型
constructor.RegisterArray<std::vector<Skill>>();
// 3. 绑定
constructor.Bind("skills", &char_data_.skills);
```

```xml
<div data-for="skill : skills">
    <span>{{skill.name}}</span>
    <span>{{skill.mp_cost}} MP</span>
</div>
```

### it_index — 循环索引

`data-for` 循环内始终可用特殊变量 `it_index`（从 0 开始的整数索引）：

```xml
<div data-for="skill : skills">
    <span>{{it_index + 1}}. {{skill.name}}</span>
</div>
```

---

## 8. 事件回调 `data-event-*`

与 L05 的 `EventListener` 不同，数据绑定的事件回调通过模型注册，直接写在 RML 属性里。

### 注册回调

```cpp
// 无参数回调
constructor.BindEventCallback("toggle_poison",
    [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
        char_data_.is_poisoned = !char_data_.is_poisoned;
        model.DirtyVariable("is_poisoned");
    });

// 带参数回调（参数来自 RML 表达式）
constructor.BindEventCallback("use_skill",
    [this](Rml::DataModelHandle model, Rml::Event& /*ev*/, const Rml::VariantList& args) {
        if (args.empty()) return;
        const auto idx = args[0].Get<size_t>();     // 取第一个参数
        // ...
    });
```

### 在 RML 中绑定

```xml
<!-- 无参：直接写回调名 -->
<div data-event-click="toggle_poison">Toggle Poison</div>

<!-- 传递字面量 -->
<div data-event-click="use_skill(0)">Use first skill</div>

<!-- 传递 data-for 的循环索引 -->
<div data-for="skill : skills"
     data-event-click="use_skill(it_index)">
    {{skill.name}}
</div>

<!-- 内联表达式（简单赋值，无需注册回调）-->
<div data-event-click="is_poisoned = false">Cure</div>
```

`data-event-EVENTNAME` 支持所有 DOM 事件名：`click`、`mouseover`、`mouseout`、`keydown` 等。

---

## 9. 脏标记：DirtyVariable

数据绑定使用**拉取（pull）模式**：RmlUi 不轮询数据，只有在你调用 `DirtyVariable` 后才重新读取对应变量并更新 UI。

```cpp
// 修改数据
char_data_.hp -= 10;

// 通知 RmlUi 该变量已改变（只刷新与 hp 相关的元素）
model_handle_.DirtyVariable("hp");
```

```cpp
// 数组变化时，标记整个数组为脏
char_data_.skills.push_back({"Haste", "Magic", 5});
model_handle_.DirtyVariable("skills");

// 一次性标记所有变量（性能较差，谨慎使用）
model_handle_.DirtyAllVariables();
```

> 在 `BindEventCallback` 的回调函数里，第一个参数就是 `Rml::DataModelHandle`，可直接用来标记脏数据：
> ```cpp
> [this](Rml::DataModelHandle model, ...) {
>     char_data_.mp -= skill.mp_cost;
>     model.DirtyVariable("mp");   // 直接用回调内的 handle
> }
> ```

---

## 10. Transform 函数（管道）

Transform 函数在 RML 表达式中用 `|` 调用，用于格式化输出：

```cpp
constructor.RegisterTransformFunc("format_gold",
    [](const Rml::VariantList& args) -> Rml::Variant {
        if (args.empty()) return {};
        return Rml::Variant(Rml::String(std::to_string(args[0].Get<int>()) + " G"));
    });
```

```xml
<span>{{gold | format_gold}}</span>       <!-- 输出：9876 G -->
```

可以链式调用多个函数：

```xml
{{value | clamp | format_number}}
```

---

## 11. 销毁数据模型

数据模型生命周期绑定在 `Rml::Context` 上。场景退出时必须手动删除，否则下次重进场景时 `CreateDataModel` 同名会失败：

```cpp
void DataBindingScene::clean() {
    unloadAllRmlDocuments();   // 先卸载文档

    // 再移除数据模型
    if (auto* runtime = context_.getRmlUi()) {
        if (auto* rml_ctx = runtime->getContext()) {
            rml_ctx->RemoveDataModel("character");
        }
    }

    Scene::clean();
}
```

> 与 L05 事件监听器不同，数据绑定**不需要**手动反注册回调，`RemoveDataModel` 会一并清理。

---

## 12. 配套代码

### 场景代码

`learn/rmlui_data_binding/data_binding_scene.cpp` 演示了：

1. **setupDataModel()** — 完整的数据模型建立流程
2. **update()** — 定时修改 HP/MP + 调用 `DirtyVariable` 实现自动刷新
3. **clean()** — 卸载文档 + `RemoveDataModel`

### RML 文档

`ui/rmlui/learn/learn_data_binding.rml` 包含两个区块：

| 区块 | 展示特性 |
|------|---------|
| 角色面板（左） | `{{name}}`、`data-style-width`、`{{gold \| format_gold}}`、`data-if`、`data-event-click="toggle_poison"` |
| 技能列表（右） | `data-for`、结构体成员访问、`data-class-mp-low`、`data-event-click="use_skill(it_index)"` |

---

## 13. 构建与运行

```bash
ninja -C build/debug learn_rmlui_data_binding
./build/debug/learn/learn_rmlui_data_binding
```

运行后可观察：
- HP / MP 数值和进度条每 3 秒自动更新（无任何手动 DOM 操作）
- 点击技能按钮 → MP 减少 → MP 不足时技能变灰（`data-class-mp-low`）
- 点击 "Toggle Poison" → 状态徽章出现/消失（`data-if`）

---

## 14. 练习

### 练习 1：角色切换

在场景中定义 2 个不同角色（Warrior / Mage），添加切换按钮，点击后更新模型中的 `name`、`job`、`hp`、`mp` 等字段并调用 `DirtyAllVariables()`，观察整个界面同步刷新。

### 练习 2：物品列表

创建一个 `Item` 结构体（name、count、price），用 `data-for` 渲染列表，
实现 "Buy" 按钮：点击后 `count++`、`gold -= price`，只标记 `items` 和 `gold` 为脏。
用 `data-if="count > 0"` 控制 "Use" 按钮是否显示。

### 练习 3：HP 条颜色

在练习 1 / 2 基础上，用 `data-style-color` 让 HP 数值在：
- HP > 50% → 绿色（`#9ece6a`）
- HP 在 25%~50% → 黄色（`#e0af68`）
- HP < 25% → 红色（`#f7768e`）

提示：可以用嵌套三元运算 `a ? b : (c ? d : e)` 实现。

---

## 15. 要点回顾

| 概念 | 要点 |
|------|------|
| 模型创建 | `context->CreateDataModel("name")`，必须在加载文档前调用 |
| 标量绑定 | `constructor.Bind("var", &data)` |
| 数组绑定 | 先 `RegisterArray<vector<T>>()`，再 `Bind("list", &vec)` |
| 结构体绑定 | `RegisterStruct<T>()` + `RegisterMember`，顺序须在数组注册前 |
| 文本插值 | `{{expr}}` — 支持运算和字符串拼接 |
| 条件显示 | `data-if="expr"` — 从 DOM 移除；`data-visible` — 保留布局 |
| 动态样式 | `data-style-PROP="expr"` |
| 动态 class | `data-class-NAME="bool_expr"` |
| 列表渲染 | `data-for="item : list"`；`it_index` 为当前索引 |
| 事件回调 | `BindEventCallback("fn", lambda)`；RML 用 `data-event-click="fn(arg)"` |
| 脏标记 | 修改数据后调用 `model_handle_.DirtyVariable("var")` |
| Transform | `RegisterTransformFunc("name", fn)`；RML 用 `{{val \| name}}` |
| 模型销毁 | `context->RemoveDataModel("name")`，在 `clean()` 中调用 |

---

**上一课 <-** [L05: 事件系统](L05-events.md)
**下一课 ->** [L07: 表单控件](L07-forms.md)
