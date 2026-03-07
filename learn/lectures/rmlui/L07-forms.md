# L07: 表单控件

> 配套代码：`learn/rmlui_forms/` | 构建目标：`learn_rmlui_forms`

---

## 1. RmlUi 表单控件总览

RmlUi 内置了一套表单控件系统，类似 HTML 表单但针对游戏 UI 进行了优化。
配合 L06 的数据绑定，可以实现表单值与 C++ 变量的**双向同步**。

| 控件 | RML 标签 | 用途 |
|------|----------|------|
| 文本框 | `<input type="text">` | 单行文字输入 |
| 密码框 | `<input type="password">` | 遮罩输入 |
| 复选框 | `<input type="checkbox">` | 布尔开关 |
| 单选按钮 | `<input type="radio">` | 多选一（同 name 分组） |
| 滑块 | `<input type="range">` | 数值范围拖拽 |
| 提交按钮 | `<input type="submit">` | 触发 form 提交 |
| 多行文本 | `<textarea>` | 多行文字输入 |
| 下拉选择 | `<select>` + `<option>` | 弹出列表选一 |
| 进度条 | `<progress>` | 只读数值可视化 |
| 标签页 | `<tabset>` + `<tab>` / `<panel>` | 分页显示 |

---

## 2. 文本输入：`<input type="text">` / `<input type="password">`

```xml
<!-- 单行文本 -->
<input type="text" name="player_name" maxlength="16"/>

<!-- 密码遮罩 -->
<input type="password" name="password" size="20"/>
```

**常用属性**：

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `value` | 当前文本内容 | `""` |
| `maxlength` | 最大字符数 | 无限制 |
| `size` | 控件宽度（字符数） | 20 |
| `disabled` | 禁用输入 | — |
| `name` | 表单提交时的键名 | — |

**事件**：文本改变时触发 `change` 事件。

---

## 3. 多行文本：`<textarea>`

```xml
<textarea name="notes" cols="30" rows="3"/>
```

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `cols` | 可见字符宽度 | 20 |
| `rows` | 可见行数 | 2 |
| `maxlength` | 最大字符数 | 无限制 |
| `wrap` | 设为 `"nowrap"` 禁用自动换行 | 自动换行 |

> **注意**：在 RML（XML）中，`<textarea>` 可以使用自闭合写法 `<textarea ... />`。

---

## 4. 复选框：`<input type="checkbox">`

```xml
<input type="checkbox" name="fullscreen"/>
```

- 点击切换选中状态
- 选中时拥有 `:checked` 伪类，可在 RCSS 中定义不同样式
- `change` 事件参数包含 `value`（选中时为 `"on"`、未选中为 `""`）

```css
input.checkbox {
    width: 14dp;
    height: 14dp;
    border: 1dp #414868;
    background-color: #1e2540;
}
input.checkbox:checked {
    background-color: #7aa2f7;
    border: 1dp #7aa2f7;
}
```

---

## 5. 单选按钮：`<input type="radio">`

```xml
<!-- 同一 name 组成互斥组 -->
<input type="radio" name="window_mode" value="windowed"/>
<input type="radio" name="window_mode" value="borderless"/>
<input type="radio" name="window_mode" value="exclusive"/>
```

- **同 `name`** 的 radio 自动互斥——选中一个时其余自动取消
- 选中的 radio 拥有 `:checked` 伪类
- 使用 `border-radius` 可以实现圆形外观

```css
input.radio {
    width: 14dp;
    height: 14dp;
    border: 1dp #414868;
    background-color: #1e2540;
    border-radius: 7dp;      /* 圆形 */
}
input.radio:checked {
    background-color: #7aa2f7;
}
```

---

## 6. 下拉选择：`<select>` + `<option>`

```xml
<select name="text_speed">
    <option value="slow">慢速</option>
    <option value="medium">中速</option>
    <option value="fast">快速</option>
</select>
```

点击后弹出选项列表。**内部结构**（可在 RCSS 中分别样式化）：

| 子元素 | 说明 |
|--------|------|
| `selectvalue` | 当前选中项的显示区域 |
| `selectarrow` | 右侧下拉箭头 |
| `selectbox` | 弹出的选项列表容器 |
| `option` | 列表中的每个选项 |

```css
select selectbox option:hover {
    background-color: #2d3354;
}
select selectbox option:checked {
    background-color: #7aa2f7;
    color: #1a1b26;
}
```

---

## 7. 滑块：`<input type="range">`

```xml
<input type="range" name="bgm_volume"
       min="0" max="100" step="5"/>
```

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `min` | 最小值 | 0 |
| `max` | 最大值 | 100 |
| `step` | 步进值 | 1 |
| `orientation` | `"horizontal"` 或 `"vertical"` | horizontal |

**内部结构**（可在 RCSS 中样式化）：

| 子元素 | 说明 |
|--------|------|
| `sliderarrowdec` | 减少按钮（可隐藏） |
| `sliderarrowinc` | 增加按钮（可隐藏） |
| `slidertrack` | 轨道 |
| `sliderbar` | 可拖拽的滑块手柄 |

```css
/* 隐藏箭头按钮 */
input.range sliderarrowdec,
input.range sliderarrowinc {
    width: 0;
    height: 0;
}
/* 轨道 */
input.range slidertrack {
    height: 6dp;
    background-color: #1e2540;
    border-radius: 3dp;
}
/* 手柄 */
input.range sliderbar {
    width: 12dp;
    height: 12dp;
    background-color: #7aa2f7;
    border-radius: 6dp;
}
```

---

## 8. 进度条：`<progress>`

`<progress>` 是只读的数值可视化控件，不参与表单提交。

```xml
<progress value="70" max="100"/>
```

| 属性 | 说明 | 默认值 |
|------|------|--------|
| `value` | 当前值（0 ~ max） | 0 |
| `max` | 最大值 | 1.0 |
| `direction` | 填充方向：`right` / `left` / `top` / `bottom` / `clockwise` / `counter-clockwise` | `right` |

内部有一个 `fill` 子元素，表示填充部分：

```css
progress {
    height: 6dp;
    background-color: #1e2540;
}
progress fill {
    background-color: #7aa2f7;
}
```

配合数据绑定，可用 `data-attr-value` 动态更新：

```xml
<!-- 单向属性绑定：bgm_volume 变化 → progress 更新 -->
<progress data-attr-value="bgm_volume" max="100"/>
```

---

## 9. 标签页：`<tabset>`

`<tabset>` 将多组内容以标签页形式组织，一次只显示一个面板。

```xml
<tabset>
    <tab>Audio</tab>
    <tab>Display</tab>
    <tab>Profile</tab>

    <panel>Audio 面板内容...</panel>
    <panel>Display 面板内容...</panel>
    <panel>Profile 面板内容...</panel>
</tabset>
```

- `<tab>` 和 `<panel>` 按顺序一一对应
- 当前激活的 tab/panel 拥有 `:selected` 伪类
- 内部会自动创建 `tabs`（标签容器）和 `panels`（面板容器）

```css
tabset tabs {
    display: flex;
    flex-direction: row;
    gap: 2dp;
}
tabset tab {
    display: inline-block;
    padding: 3dp 10dp;
    color: #565f89;
    background-color: #1e2540;
}
tabset tab:selected {
    color: #c0caf5;
    background-color: #2d3354;
    border: 1dp #7aa2f7;
}
tabset panel {
    display: block;
    padding: 6dp;
}
```

### C++ API

```cpp
#include <RmlUi/Core/Elements/ElementTabSet.h>

auto* tabset = rml_cast<Rml::ElementTabSet>(doc->GetElementById("my-tabset"));
tabset->SetActiveTab(1);       // 切换到第 2 个标签
int current = tabset->GetActiveTab();
int count   = tabset->GetNumTabs();
tabset->RemoveTab(2);          // 移除第 3 个标签
```

---

## 10. 表单提交：`<form>` + `submit` 事件

将控件包裹在 `<form>` 中，点击 `<input type="submit">` 触发提交。

```xml
<form data-event-submit="on_submit">
    <input type="text" name="player_name"/>
    <input type="range" name="bgm_volume" min="0" max="100"/>
    <input type="submit" value="Confirm"/>
</form>
```

提交时，RmlUi 收集所有 **有 `name` 属性、未 `disabled`** 的控件值，作为事件参数传递：

```cpp
constructor.BindEventCallback("on_submit",
    [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList&) {
        // 读取表单控件的值（键 = name 属性）
        auto name = ev.GetParameter<Rml::String>("player_name", "");
        auto bgm  = ev.GetParameter<Rml::String>("bgm_volume", "");
        spdlog::info("Submit: name={} bgm={}", name, bgm);
    });
```

> **注意**：`<input type="submit">` 的 `value` 属性是按钮显示文字，不是提交值。

---

## 11. 数据绑定与表单结合

L06 学过的数据绑定可以与表单控件**双向绑定**——用户操作表单时 C++ 变量自动更新，C++ 修改变量时控件也同步。

### data-value — 值绑定

适用于 **text / range / select** 控件：

```xml
<!-- 文本输入 ↔ player_name -->
<input type="text" data-value="player_name"/>

<!-- 滑块 ↔ bgm_volume -->
<input type="range" data-value="bgm_volume" min="0" max="100"/>

<!-- 下拉选择 ↔ text_speed -->
<select data-value="text_speed">
    <option value="slow">Slow</option>
    <option value="medium">Medium</option>
    <option value="fast">Fast</option>
</select>
```

### data-checked — 选中状态绑定

适用于 **checkbox / radio** 控件：

```xml
<!-- 复选框 ↔ bool fullscreen -->
<input type="checkbox" data-checked="fullscreen"/>

<!-- 单选按钮组 ↔ string window_mode -->
<!-- 当 window_mode == "windowed" 时该 radio 被选中 -->
<input type="radio" name="window_mode" value="windowed"
       data-checked="window_mode"/>
<input type="radio" name="window_mode" value="borderless"
       data-checked="window_mode"/>
```

### data-attr-* — 属性绑定

单向绑定到 HTML 属性，适用于任何元素：

```xml
<!-- value 属性绑定 -->
<progress data-attr-value="bgm_volume" max="100"/>

<!-- 条件禁用 -->
<input type="submit" data-attrif-disabled="player_name == ''"/>
```

### 双向绑定的自动刷新

使用 `data-value` / `data-checked` 时，用户操作控件后：
1. 控件值变化 → 自动写入绑定的 C++ 变量
2. 自动标记变量为脏（DirtyVariable）
3. 所有引用该变量的 `{{expr}}`、`data-if`、`data-style-*` 等自动刷新

因此，**预览面板**可以实时反映表单变化——无需手动调用 DirtyVariable：

```xml
<div class="preview-item">BGM: {{bgm_volume}}%</div>
```

---

## 12. 表单控件的 RCSS 样式化

RmlUi 的表单控件通过 **CSS 类名** 区分类型：

| 控件类型 | CSS 类名 |
|----------|----------|
| `<input type="text">` | `input.text` |
| `<input type="password">` | `input.password` |
| `<input type="checkbox">` | `input.checkbox` |
| `<input type="radio">` | `input.radio` |
| `<input type="range">` | `input.range` |
| `<input type="submit">` | `input.submit` |

伪类：

| 伪类 | 适用控件 |
|------|---------|
| `:checked` | checkbox、radio、select option |
| `:focus` | 所有可交互控件 |
| `:disabled` | 所有控件（设置 `disabled` 属性） |
| `:hover` | 所有控件 |
| `:active` | 按钮类控件 |
| `:selected` | tabset 中激活的 tab / panel |

---

## 13. 配套代码

### 场景代码

`learn/rmlui_forms/forms_scene.cpp` 演示了：

1. **setupDataModel()** — 绑定 8 个设置变量 + 注册 `on_submit` / `on_reset` 回调
2. **on_submit** — 读取 submit 事件中的表单参数字典
3. **on_reset** — 恢复默认值 + `DirtyAllVariables()` 一次性刷新

### RML 文档

`ui/rmlui/learn/learn_forms.rml` 包含三个标签页：

| 标签页 | 展示控件 |
|--------|---------|
| Audio | `<input type="range">` + `<progress>` + `data-value` + `data-attr-value` |
| Display | `<input type="checkbox">` + `<select>` + `<input type="radio">` + `data-checked` + `data-value` |
| Profile | `<input type="text">` + `<input type="password">` + `<textarea>` + `data-value` |

右侧预览面板通过 `{{variable}}` 文本插值实时显示所有绑定值。

---

## 14. 构建与运行

```bash
ninja -C build/debug learn_rmlui_forms
cd build/debug/learn && ./learn_rmlui_forms
```

运行后可观察：
- 拖动滑块 → 数值 + 进度条 + 预览面板同步更新（`data-value` 双向绑定）
- 勾选 Fullscreen → 预览显示 Yes（`data-checked` 绑定 bool）
- 切换 Window Mode radio → 预览显示当前模式（`data-checked` 绑定 string）
- 修改下拉选择 → 预览实时更新（`data-value`）
- 在文本框输入 → 预览同步（`data-value`）
- 点击 Confirm → 底部显示 submit 事件收集的表单值
- 点击 Reset → 所有控件恢复默认值

---

## 15. 练习

### 练习 1：动态禁用

给 Confirm 按钮添加 `data-attrif-disabled="player_name == ''"` 条件，
当玩家名为空时按钮不可点击。在 RCSS 中添加 `:disabled` 样式使其变灰。

### 练习 2：音量主控

添加一个 Master Volume 滑块，用 `data-value="master_volume"` 绑定。
在 C++ 端注册 Transform 函数 `format_volume`，显示为 `"70%"` 格式。
在 Audio 面板最上方显示主音量，并用 `<progress>` 可视化。

### 练习 3：确认弹窗

点击 Reset 后，用 `data-if="show_confirm"` 显示一个确认弹窗（overlay div），
包含 "确定重置?" 文字和 "是/否" 两个按钮。
点击 "是" 执行重置，"否" 关闭弹窗。

---

## 16. 要点回顾

| 概念 | 要点 |
|------|------|
| 文本输入 | `<input type="text">` / `<input type="password">`，`maxlength` 限制字符 |
| 多行文本 | `<textarea cols="" rows="">`，`wrap="nowrap"` 禁用换行 |
| 复选框 | `<input type="checkbox">`，`:checked` 伪类 |
| 单选按钮 | `<input type="radio" name="group">`，同 name 互斥 |
| 下拉选择 | `<select>` + `<option value="">`，子元素 `selectbox` / `selectarrow` 可样式化 |
| 滑块 | `<input type="range" min="" max="" step="">`，子元素 `slidertrack` / `sliderbar` |
| 进度条 | `<progress value="" max="">`，子元素 `fill`，`direction` 控制填充方向 |
| 标签页 | `<tabset>` + `<tab>` / `<panel>`，`:selected` 伪类，C++ `SetActiveTab()` |
| 表单提交 | `<form>` + `<input type="submit">`，`submit` 事件含所有控件 name:value |
| data-value | 双向绑定控件值（text / range / select） |
| data-checked | 双向绑定选中状态（checkbox → bool / radio → string） |
| data-attr-* | 单向属性绑定（`data-attr-value` / `data-attrif-disabled` 等） |

---

**上一课 <-** [L06: 数据绑定](L06-data-binding.md)
**下一课 ->** L08: 自定义元素与文档管理（待完成）
