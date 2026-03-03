# RmlUi / RCSS 编码规范

> 编写 RML/RCSS 或涉及 UI 层 C++ 代码时，请先阅读本文件。

RmlUi 的 RCSS 与标准 CSS 有若干关键差异，编写时必须注意：

## 默认 display 是 inline

所有元素（包括 `<div>`, `<p>`, `<h1>`~`<h4>`）的默认 `display` 值是 `inline`，不是 `block`。
每个 .rcss 文件开头**必须**添加重置规则：

```css
body, div, h1, h2, h3, h4, p, hr {
    display: block;
}
```

## border 简写不支持 style 关键字

RmlUi 没有 `border-style` 属性，`border` 简写语法为 `<width> <color>`：

- 正确：`border: 1dp #7aa2f7;`
- 错误：`border: 1dp solid #7aa2f7;`（`solid` 会导致解析失败）

## 位图字体无 italic 变体

项目使用的 "VonwaonBitmap 16px" 是位图字体，没有 italic 字形文件。
在 RCSS 中对该字体使用 `font-style: italic` 会导致 "No font face defined" 警告且文字不显示。
不要在演示代码中对位图字体使用 italic。

## 逻辑分辨率

窗口逻辑分辨率为 640×360dp（`window.json` 中 1280×720 * logical_scale 0.5）。
编写 learn 演示页面时注意内容不要超出此范围，合理使用多列布局。

## 事件监听器必须在卸载文档前移除

通过 `AddEventListener` 注册的监听器，必须在 `unloadAllRmlDocuments()` **之前**调用
`RemoveEventListener` 逐一移除。否则文档卸载过程中 RmlUi 会派发 `blur` 等事件，
回调可能访问正在销毁的元素导致 segfault。

正确顺序：
```cpp
void MyScene::clean() {
    removeAllListeners();      // 先移除监听器
    unloadAllRmlDocuments();   // 再卸载文档
    Scene::clean();
}
```

## position: absolute 不支持 left+right / top+bottom 隐式拉伸

标准 CSS 中 `position: absolute; left: 0; right: 0` 会隐式撑满宽度，但 RmlUi 中 `left` 和 `right` 是互斥的（`else if`），`top` 和 `bottom` 同理。要让绝对定位元素填满父容器，必须显式设置 `width` 和 `height`：

- 正确：`position: absolute; left: 0; top: 0; width: 100%; height: 100%;`
- 错误：`position: absolute; left: 0; top: 0; right: 0; bottom: 0;`（`right`/`bottom` 被忽略，元素尺寸退化为内容大小）

## UI 资源路径

- RML/RCSS 文件放在 `ui/`（项目根目录），不是 `assets/ui/`
- 教程用 UI 文件放在 `ui/rmlui/learn/` 子目录，与游戏 UI 文件隔离
- 代码中加载路径示例：`loadRmlDocument("ui/rmlui/learn/xxx.rml")`
