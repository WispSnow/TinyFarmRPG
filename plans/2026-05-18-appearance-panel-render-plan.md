# 外观自定义场景背景面板渲染计划

## 目标

修复外观自定义场景的两处 UI 表现问题：

- 为整个外观自定义 UI 区域添加居中的大背景面板，视觉资源与商店、物品菜单相同，使用 `Inventory/inventory.png` 中的 `menu-panel-bg` / `menu-panel-bg-inner` 九宫格区域。
- 将右侧 `appearance-panel` 改成透明面板，视觉资源使用 `Inventory/Banner.png` 中的 `menu-party-card-bg` / `menu-party-card-bg-inner` 九宫格区域。

关键约束：外观预览角色现在通过游戏内 sprite 渲染在 RmlUi 下方。如果大背景面板也用 RML 九宫格渲染，它会在最终 RmlUi 阶段盖住预览角色。因此大背景面板优先改为游戏内渲染路径绘制。

## 当前判断

游戏内渲染大背景面板是可行的，且是本问题的首选方案。

原因：

- `AppearanceCustomizeScene::render()` 已经在 RmlUi 渲染前绘制预览角色。
- `GameScene` 与覆盖式 scene 的渲染会按 scene stack 顺序追加到同一个 renderer，外观场景可以在预览角色之前先追加一张背景面板。
- 引擎已有 `engine::render::NineSlice` 和 `engine::render::Image` 的九宫格数据结构，可以复用九宫格切片逻辑。

需要补足的点：

- 目前高层 `Renderer` 没有直接绘制九宫格图片的 API，只有 `drawSprite()`、`drawFilledRect()` 等基础接口。
- 这次不应把大面板做回 RML，也不需要做离屏 framebuffer 或把角色转成 RML `<img>`。
- 普通 `Renderer::drawSprite()` 会进入 scene pass，最终经 `scene * light + emissive` 合成；衣柜模式下大面板与当前预览角色一样会受环境光影响。第一版先复用当前 scene-pass 路径以控制范围，但把「衣柜昼夜亮度」列为验收门槛；如果面板明显变暗或染色，则停止合并并改走本文下方的 unlit pre-Rml overlay 备选方案。

## 资源映射

大背景面板资源来自 `assets/farm-rpg/UI/Inventory/inventory.png`：

```text
texture id: ui/appearance/menu-panel
path: assets/farm-rpg/UI/Inventory/inventory.png
menu-panel-bg:        x=0,  y=64, w=48, h=48
menu-panel-bg-inner:  x=10, y=74, w=28, h=28
margins: left=10, top=10, right=10, bottom=10
```

右侧透明面板资源来自 `assets/farm-rpg/UI/Inventory/Banner.png`：

```text
RCSS spritesheet location: ui/rmlui/theme/spritesheet.rcss
menu-party-card-bg:       x=67, y=3,  w=42, h=42
menu-party-card-bg-inner: x=76, y=12, w=24, h=24
margins: left=9, top=9, right=9, bottom=9
```

## 实施方案

### 1. 增加屏幕空间九宫格绘制 helper

先在 `AppearanceCustomizeScene.cpp` 增加 scene-local helper，避免把 UI 坐标语义过早塞进通用 `Renderer` API：

```text
drawNineSliceImageInScreenSpace(renderer, camera, image, screen_rect)
```

实现思路：

- 使用 `Image::ensureNineSlice()` 取得 9 个 source rect。
- 必须先在 640×360 逻辑屏幕坐标中拆 9 个 destination rect，保证角块在屏幕上的尺寸保持 source 像素大小，不随相机 zoom 变化。
- 对每个 destination screen rect 分别调用 `Camera::screenToWorld()` 转成 world rect，然后构造临时 `Sprite` 并调用 `Renderer::drawSprite()`。
- 跳过 source 或 destination 宽高 `<= 0` 的切片；当 target 小于左右/上下边框和时，center/edge 尺寸夹到 0，不绘制退化切片。
- 纹理由调用方在 scene 初始化时通过 `ResourceManager::loadTexture()` 预加载，避免渲染阶段隐式加载。

后续如果其他 scene 也需要同类能力，再把 helper 抽到 `Renderer` 或独立 UI render utility；抽出时接口必须明确是 screen-space，而不是普通 world-space nine-slice。

### 2. 在 `AppearanceCustomizeScene` 中绘制大背景面板

新增 scene 内部资源状态：

- 背景面板 texture id/path。
- 背景面板 `Image`，source rect 为 `0,64,48,48`，nine-slice margins 为 `10,10,10,10`。
- `init()` 中加载：`ResourceManager::loadTexture("ui/appearance/menu-panel"_hs, "assets/farm-rpg/UI/Inventory/inventory.png")`。
- `clean()` 中卸载该唯一 id，避免为只属于外观场景的 game-rendered UI 纹理留下长期缓存。
- 背景面板目标屏幕矩形，建议沿用商店场景居中布局：

```text
left=30dp, top=26dp, width=580dp, height=308dp
```

渲染顺序：

```text
1. NewGame 模式：必要时 renderer.beginFrame(camera)
2. 绘制游戏内大背景面板
3. 绘制外观预览角色
4. RmlUi 最后绘制标题、预览边框、右侧控件和按钮
```

这样大背景面板会盖住黑色/世界底图，但预览角色会盖在大背景面板之上。

注意：这一版的背景面板和预览角色都位于 scene pass，因此在衣柜模式会共同受到当前世界光照影响。验收时必须在白天/夜晚各检查一次；若视觉上不像 UI 面板，则切换到「unlit pre-Rml overlay 备选方案」。

### 3. 调整 RML/RCSS 层职责

`appearance_customize.rml`：

- 不新增大背景 RML 节点。
- 保留标题、副标题、预览边框、右侧 `appearance-panel` 和按钮。

`appearance_customize.rcss`：

- `#appearance-overlay` 改为透明背景，只负责 modal 事件覆盖，不再叠加额外黑色遮罩。
- `#appearance-preview-frame` 保持透明或半透明边框，不能使用不透明填充遮挡预览角色。
- `#appearance-panel` 改为：

```css
decorator: ninepatch(menu-party-card-bg, menu-party-card-bg-inner, 1.0);
```

- 将 `menu-party-card-bg` / `menu-party-card-bg-inner` 加到 `ui/rmlui/theme/spritesheet.rcss` 的 `ui-tooltip` / `Banner.png` 附近，避免在 scene RCSS 中重复声明同一张 `Banner.png`。

### 4. 布局校准

需要重新校准这些坐标，使它们都落在大背景面板内：

- 标题：保留在面板左上区域。
- 预览框：左列，保持角色在框内居中。
- 右侧 `appearance-panel`：继续放在右列，但透明面板不能超出大背景边界。
- 按钮与 slot 行：沿用当前右侧布局，只根据透明面板内边距微调文字颜色。

预览角色 pivot 常量需要跟随预览框校准：

```cpp
PREVIEW_SCREEN_PIVOT_X
PREVIEW_SCREEN_PIVOT_Y
```

## 备选方案

如果 scene-pass 面板在衣柜模式下受光照影响明显，改为增加一个 unlit pre-Rml overlay 渲染点：

- 在 `GLRenderer::present()` 的 composite 之后、RmlUi hook 之前增加一个 screen-space sprite overlay queue/pass。
- 大背景面板和预览角色都提交到该 overlay pass，保持顺序为「大背景面板 -> 预览角色 -> RmlUi」。
- overlay pass 使用逻辑屏幕正交投影，不参与 lighting/emissive/bloom 合成。
- 这条路径能彻底避免夜晚 UI 变暗，但需要把当前 preview layered sprite 绘制从 scene pass 迁移到 screen-space overlay 提交，范围比第一版更大。

不推荐方案：

- 用 RML 大面板加透明预览洞。RmlUi 没有现成 mask/cutout 流程，且仍会干扰预览层级。
- 把预览角色转成 RML `<img>`。这会引入离屏渲染或生成动态图像注册，复杂度明显高于本次需求。

## 验证计划

- 使用 `ninja` 构建目标，至少保证主程序编译通过。
- 运行外观自定义场景，检查新建游戏入口：
  - 大背景面板居中，覆盖左侧黑底区域。
  - 预览角色显示在大背景面板之上，没有被 RML 遮挡。
  - 右侧 `appearance-panel` 使用透明 party card 面板。
- 检查衣柜入口：
  - 底层游戏画面被大背景面板遮住，外观预览仍可见。
  - 白天和夜晚各检查一次；如果面板明显变暗或染色，不接受 scene-pass 方案，改走 unlit pre-Rml overlay 备选方案。
  - Confirm / Cancel / Random / Reset 与方向按钮仍能点击和键盘导航。
- 如有截图测试工具可用，补一张 640x360 逻辑分辨率截图，确认面板边界和角色层级。

## 开发 Checklist

- [x] 增加屏幕空间九宫格图片绘制 helper。
- [x] 在 `AppearanceCustomizeScene` 预加载 inventory 面板纹理。
- [x] 在 `AppearanceCustomizeScene::render()` 中按顺序绘制大背景面板和预览角色。
- [x] 将 `menu-party-card-bg` spritesheet 定义补到 `ui/rmlui/theme/spritesheet.rcss`。
- [x] 调整 `appearance_customize.rcss`，让右侧面板使用 `menu-party-card-bg` 九宫格。
- [x] 校准预览框、标题、右侧面板和预览角色 pivot。
- [x] 构建验证通过。
- [x] 手动检查新游戏入口：大背景面板、预览角色层级、右侧透明面板均符合预期。
- [ ] 手动检查衣柜入口，需覆盖白天/夜晚两种光照。
