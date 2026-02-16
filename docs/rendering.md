# 渲染约定：2D Sprite、排序与可见性（TinyFarm）

> 用途：统一项目内 2D 渲染的心智模型与约定（数据从哪来、如何排序、如何隐藏/剔除），统一项目内 2D 渲染的心智模型与约定。

TinyFarm 的 2D 渲染可以用一句话概括：
> **ECS 里存“要画什么/画到哪”，System 决定“按什么顺序提交绘制”，Renderer/GLRenderer 负责“怎么画”。**

---

## 1) 一帧渲染调用链（从入口到 drawSprite）

线索（只看文件/函数名即可）：
- `src/engine/core/game_app.cpp`：`GameApp::render()`：clear → `scene_manager.render()` → present（Debug UI 也在这帧的 render 阶段插入）
- `src/engine/scene/scene_manager.cpp`：`SceneManager::render()`：从栈底到栈顶渲染（叠加渲染）
- `src/game/scene/game_scene.cpp`：`GameScene::render()`：组织 world 渲染系统（YSort → Render → Lighting → UI/Debug）
- `src/engine/system/render_system.cpp`：`RenderSystem::update()`：排序并遍历 view，逐个 `drawSprite`
- `src/engine/render/renderer.cpp`：`Renderer::drawSprite()`：纹理获取 + 视口剔除 + 提交给 GLRenderer
- `src/engine/render/opengl/gl_renderer.cpp`：底层绘制与 pass（更深入内容见 `docs/resolution_and_viewport.md` 等相关文档）

---

## 2) 最小闭环的三份数据：Transform / Sprite / Render

TinyFarm 的 Sprite 渲染最小数据组合是：
- `engine::component::TransformComponent`（`src/engine/component/transform_component.h`）：位置/缩放/旋转（rotation 为弧度）
- `engine::component::SpriteComponent`（`src/engine/component/sprite_component.h`）：纹理引用 + src rect + size + pivot（锚点）
- `engine::component::RenderComponent`（`src/engine/component/render_component.h`）：layer（跨层顺序）+ depth（层内顺序）+ color（颜色调整）

在 `RenderSystem` 中，位置的解释是：
- `Transform.position_` 表示“锚点位置”
- `Sprite.pivot_` 表示“锚点在精灵上的位置”（建议范围 0..1；(0,0)=左上，(0.5,0.5)=中心，(0.5,1)=底部中心）

---

## 3) 渲染排序：layer + depth（越小越先画）

排序规则在 `RenderComponent::operator<`：
- 先比 `layer_`（越小越先绘制）
- 再比 `depth_`（越小越先绘制）

`RenderSystem` 的关键点：
- 每帧会 `registry.sort<RenderComponent>()`（按上述规则排序）
- 必须对 view 调用 `view.use<RenderComponent>()`，强制按已排序的 `RenderComponent storage` 迭代，否则 EnTT 可能选择“更小的 storage”迭代导致排序看起来无效  
  - 线索：`src/engine/system/render_system.cpp`
  - 对应测试：`tests/engine/system/render_system_view_use_test.cpp`

---

## 4) YSort：用 y 坐标驱动 depth（遮挡在前）

`engine::system::YSortSystem`（`src/engine/system/ysort_system.cpp`）的约定：
- `Render.depth_ = Transform.position_.y`

如果世界坐标 y 越大表示越靠下，则：
- y 越大 → depth 越大 → 越晚绘制 → 视觉上更“在前面”遮挡其他对象

常见坑：
> **YSort 的“排序点”是 `Transform.position_`，不是精灵的底边。**

想要遮挡自然，通常需要：
- `Transform.position_` 代表“脚底/接地位置”
- `Sprite.pivot_` 设为接近底部（如 (0.5, 1.0)）

否则高个子精灵容易出现“不自然遮挡”（例如头在前、脚在后）。

练习建议：
- 给需要 ysort 的实体加 `YSortTag`（或 RenderSortMode），只对这些实体写 depth，避免覆盖手动 depth 的对象。

---

## 5) 可见性：InvisibleTag（逻辑隐藏）vs Viewport Clipping（视口剔除）

TinyFarm 对“看不见就不画”分两层：

### 5.1 InvisibleTag：逻辑层面的“不参与渲染”
- `engine::component::InvisibleTag`（`src/engine/component/tags.h`）
- `RenderSystem`/`LightSystem` 等会用 `entt::exclude<InvisibleTag>` 过滤

适用：临时隐藏目标光标/昼夜条件隐藏某些对象/调试开关等。

### 5.2 Viewport Clipping：渲染层面的“在视野外就不提交 draw”
- `Renderer::drawSprite()` 在提交绘制前会做 rect 剔除
- view rect 由 `GLRenderer` 在 `beginFrame(camera)` 计算（可开关）

调试线索：
- `Engine Debug Panels` → `OpenGL Renderer` 面板：
  - 可切 `Viewport Clipping`
  - 可查看当前 `View Rect (world)`
  - 可查看 pass 的 draw calls/vertices（用于理解“提交绘制”的成本）
  - 线索：`src/engine/debug/panels/gl_renderer_debug_panel.cpp`

---

## 6) 渲染层级命名（减少“魔法数字”）

项目里会用到少量“约定层级”表达 gameplay 对象的相对顺序（尤其是存档恢复/作物阶段）：
- `src/game/defs/render_layers.h`：`game::defs::render_layer::*`
  - `ACTOR`：角色/成熟作物等主要动态实体层
  - `CROP_SEED`：种子阶段作物层（在角色下方）
  - `SOIL` / `SOIL_WET`：存档恢复时的耕地/湿润耕地层（介于地图与角色之间）

> 注意：地图由 Tiled 加载的 tile layer 会使用较小的 layer index（从 0 开始递增）。上述命名常量用于 gameplay 动态对象，通常应高于 Tiled 图层。

---

## 7) SpriteBatch/UV/Pixel Snap 速查

### 7.1 SpriteBatch：减少 draw call 的“批处理”，但不能改渲染顺序
TinyFarm 的精灵批处理发生在 OpenGL 后端各个 pass 内部：
- `ScenePass`：世界精灵（FBO@logical），内部持有 `SpriteBatch`
- `UIPass`：UI 精灵（default framebuffer@viewport），内部持有 `SpriteBatch`
- `EmissivePass`：自发光精灵（FBO@logical），内部持有 `SpriteBatch`

批处理的核心思想是：
> CPU 端先把一堆精灵（quad）写进同一块顶点/索引缓冲，再用更少的 `glDrawElements` 把它们一次性提交给 GPU。

但要注意边界：
> 因为 draw order（遮挡/叠加）语义不能随意改变，SpriteBatch 只能合并“提交顺序中连续使用同一纹理”的精灵；不能全局按纹理排序来追求更少 draw call。

调试线索：
- `Engine Debug Panels` → `OpenGL Renderer` → `Pass Stats`
  - `Sprites`：本帧提交的精灵数量（每精灵 4 顶点 / 6 索引）
  - `Draw Calls`：实际的绘制调用次数（在 SpriteBatch 中≈“连续纹理段”的数量）

### 7.2 `src_rect（像素）→ uv（0..1）`：图集裁剪的数学
Sprite 的纹理裁剪通常以“像素矩形”表达（更直观），最终会被转换为 UV（0..1）交给 shader 采样：
- 线索：`Renderer::getSrcRectUV()`（`src/engine/render/renderer.cpp`）
- 公式（以纹理像素宽高为分母）：
  - `u0 = x / texW`, `v0 = y / texH`
  - `u1 = (x + w) / texW`, `v1 = (y + h) / texH`

这也是“图集（Atlas）”能减少纹理切换的原因：
> 只要精灵来自同一张大纹理（同一个 OpenGL texture），切换 `uv_rect` 不会引发纹理绑定变化；很多精灵可在更少 draw call 内完成。

（选读）文字渲染也是同一套路：glyph 通过 atlas 里的 `uv_rect` 取样。

### 7.3 像素风稳定性：NEAREST + Pixel Snap + 逻辑分辨率
像素风常见的“糊边/抖动/闪烁”，通常来自 3 类问题：
1) **子像素相机**：相机位置/缩放导致采样落在半像素，画面边缘会“抖”。  
   - 线索：`GLRenderer` 的 Pixel Snap（`src/engine/render/opengl/gl_renderer.cpp`）
   - 调试：`OpenGL Renderer` 面板切 `Pixel Snap`，对比缓慢移动时的稳定性
2) **非整数缩放**：logical 到 window 的缩放不是整数倍时，像素会出现“大小不均”。  
   - 线索：逻辑分辨率与 letterbox（`docs/resolution_and_viewport.md`）
   - 实践建议：优先选择窗口尺寸为 logical 的整数倍（或用 `window_scale` 控制初始倍数）
3) **线性采样与边缘渗色**：贴图/图集用 LINEAR 时，边缘会采到相邻像素产生 bleeding。  
   - 线索：`TextureManager` 默认使用 `GL_NEAREST + GL_CLAMP_TO_EDGE`（`src/engine/resource/texture_manager.cpp`）
   - 经验：图集边缘可加 padding；或对 UV 做半像素收缩（本项目目前以 NEAREST 为主）

---

## 8) 光照与后处理管线速查：Lighting/Emissive/Bloom/Composite

把"只会画 Sprite 的 2D 渲染"扩展为"多 pass 离屏渲染 + 合成"的流水线：
> **先把各类信息分别画到离屏纹理（@Logical），再把这些纹理合成到屏幕（@Window Pixels / viewport）。**

### 8.1 一帧管线顺序（以 GLRenderer 为准）
线索：`src/engine/render/opengl/gl_renderer.cpp`：`GLRenderer::present()`

1. `ScenePass`：世界精灵 → `scene_color_tex`（FBO@Logical）
2. `LightingPass`：点光/聚光/方向光 → `light_color_tex`（FBO@Logical，additive accumulate）
3. `EmissivePass`：发光遮罩 → `emissive_color_tex`（FBO@Logical）
4. `BloomPass`：对 emissive 做降采样 + 高斯模糊 + 上采样叠加 → `bloom_tex`（FBO@half/quarter …）
5. `CompositePass`：合成到默认帧缓冲的 letterbox viewport（@Window Pixels）
6. `UIPass`：UI 精灵绘制到默认帧缓冲的 viewport（@Window Pixels）
7. `ImGui`：Debug UI 覆盖绘制到整窗（@Window Pixels）

### 8.2 坐标空间：为什么要“离屏@Logical → 合成@Viewport”
这套管线同时存在 2 个关键空间：
- **Logical**：离屏渲染目标的固定分辨率（与 UI 设计坐标一致）
- **Window Pixels**：实际 drawable 像素尺寸（高 DPI 下可能与窗口逻辑尺寸不同），letterbox viewport 定义在此空间

你可以把它理解为：
> Scene/Lighting/Emissive 在同一张“逻辑画布（Logical）”上各画各的；Composite 再把这张画布缩放/信箱到窗口 viewport。

前置文档：`docs/resolution_and_viewport.md`

### 8.3 光照数据从哪来（ECS → Renderer → Pass）
核心链路：
- `game::system::DayNightSystem`：根据 `GameTime` 与 `assets/data/light_config.json` 写入 `registry.ctx<engine::render::GlobalLightingState>()`（环境光 + 方向光）
- `engine::system::LightSystem`：从 ECS 收集点光/聚光/发光组件，并读取 `GlobalLightingState`，统一调用 `Renderer` 的高层 API
- `engine::render::Renderer`：外观层（Facade），把上层“我要一盏灯/一块发光”转成对 `GLRenderer` 的提交
- `GLRenderer`：把提交分发到 `LightingPass/EmissivePass` 等具体 pass，并在 `present()` 里统一执行

线索：
- `src/engine/system/light_system.cpp`
- `src/game/system/day_night_system.cpp`
- `src/engine/render/renderer.h`

### 8.4 Debug 线索：先看“中间纹理 + Pass Stats”
排查建议从 DebugPanel 入手（不需要读 shader 细节）：
- 入口：运行后按 `F5` → `Engine Debug Panels` → `OpenGL Renderer`
- `Pass Stats`：观察 Scene/Lighting/Emissive/Bloom/UI 的 draw calls/vertices（建立“效果 ↔ 成本”的直觉）
- `Pass Preview`：直接看 Scene/Light/Emissive/Bloom 的中间纹理（建立“每个 pass 产出是什么”的直觉）
- `Lighting/Bloom/Ambient`：通过开关与滑杆验证效果变化（先验证再深入代码）

### 8.5 常见坑
1) **方向光不是“跟随相机的世界物体”**：本项目的方向光是 screen-space 渐变遮罩（类似“昼夜滤镜”），实现上需要临时切换到屏幕正交投影。  
2) **Bloom 不是免费效果**：它会引入多次全屏采样与模糊；建议用 DebugPanel 看 Bloom 的 draw calls，并避免“无 emissive 也跑 Bloom”。  
3) **合成阶段的颜色空间/混合**：项目目标是“足够好且可演示”，但一旦你开始追求更真实的光照/后处理，就会遇到线性空间、tone mapping、HDR 缓冲等话题（可作为进阶练习与扩展方向）。  
