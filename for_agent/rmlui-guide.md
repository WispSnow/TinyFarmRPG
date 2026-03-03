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

## UI 资源路径

- RML/RCSS 文件放在 `ui/`（项目根目录），不是 `assets/ui/`
- 教程用 UI 文件放在 `ui/rmlui/learn/` 子目录，与游戏 UI 文件隔离
- 代码中加载路径示例：`loadRmlDocument("ui/rmlui/learn/xxx.rml")`
