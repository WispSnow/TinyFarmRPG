# 文字渲染管线优化待办

> 日期: 2026-02-27
> 涉及模块: `engine::render::TextRenderer`, `engine::resource::Font/FontManager`, `engine::render::opengl::SpriteBatch`

---

## 高优先级

### 1. 字形图集改用 R8 单通道格式 -- DONE

- **现状**: 图集纹理使用 `GL_RGBA`，每个字形的 FreeType 灰度 alpha 被展开为 RGBA（RGB=255, A=alpha），浪费 75% 显存，且每个字形加载都分配临时 `std::vector<uint8_t>`。
- **涉及文件**:
  - `src/engine/resource/font_manager.cpp` — `createAtlasPage()`: `GL_RGBA` → `GL_R8`/`GL_RED`
  - `src/engine/resource/font_manager.cpp` — `loadGlyph()`: 去掉 RGBA 转换，直接上传 alpha buffer
  - `src/engine/resource/font_manager.cpp` — `fillDebugInfo()`: 显存统计从 `*4` 改为 `*1`
- **采样方案**: 使用 **OpenGL texture swizzle**，在 `createAtlasPage` 创建纹理时设置：
  ```cpp
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
  ```
  这样 `texture(uTex, vUV)` 自动返回 `vec4(1,1,1,r)`，**无需修改着色器，无需在 SpriteBatch/GLRenderer 中传递字形模式标志**，整条渲染链路（`SpriteBatch::Command`、`queueSprite`、`flush`、`drawTexture`/`drawUITexture`、`texture.frag`）均无需改动。
- **预期收益**:
  - 图集显存减少 75%（512x512: 1MB → 256KB）
  - 消除 per-glyph 的临时堆分配（`std::vector<uint8_t> rgba`）
- **注意事项**:
  - `GL_R8` 默认采样返回 `vec4(r, 0, 0, 1)`，若不设置 swizzle 会导致文字偏红/发黑
  - 调试面板 `fillDebugInfo` 中 `atlas_memory` 按 `width * height * 4` 计算（`font_manager.cpp:320`），需同步改为 `* 1`，否则优化后显存统计失真
- [x] 修改 `createAtlasPage` 使用 `GL_R8` 内部格式、`GL_RED` 外部格式，并设置 texture swizzle（RGBA → ONE,ONE,ONE,RED）
- [x] 修改 `loadGlyph` 直接上传 FreeType alpha buffer（处理 pitch 后直接 `glTexSubImage2D` 用 `GL_RED`），去掉 RGBA 转换的临时 vector
- [x] 修改 `fillDebugInfo` 中显存统计：`width * height * 4` → `width * height * 1`
- [ ] 验证中文/英文/特殊符号的渲染正确性（需运行时目视确认）

### 2. 复用 HarfBuzz Buffer -- DONE

- **现状**: `TextRenderer::shapeLine()` 每次调用都 `hb_buffer_create()` / `hb_buffer_destroy()`，涉及堆分配。多行文本会触发 N 次分配。
- **涉及文件**:
  - `src/engine/render/text_renderer.h` — 新增 `hb_buffer_t*` 成员
  - `src/engine/render/text_renderer.cpp` — `shapeLine()` 改用 `hb_buffer_reset()` 复用；构造/析构管理生命周期
- **预期收益**: 消除 per-line 的 HarfBuzz buffer 堆分配/释放
- [x] 在 `TextRenderer` 构造时创建 `hb_buffer_t*`，析构时销毁
- [x] `shapeLine` 开头调用 `hb_buffer_reset()` 代替 `hb_buffer_create()`
- [x] 移除 `shapeLine` 末尾的 `hb_buffer_destroy()`

### 3. 消除 per-glyph GL 状态查询 -- DONE

- **现状**: `Font::loadGlyph()` 每次调用 `glGetIntegerv(GL_TEXTURE_BINDING_2D)` 和 `glGetIntegerv(GL_UNPACK_ALIGNMENT)` 来保存/恢复 GL 状态。`glGetIntegerv` 是 GPU→CPU 同步调用，在渲染循环中频繁触发会造成管线阻塞。
- **涉及文件**:
  - `src/engine/resource/font_manager.cpp` — `loadGlyph()`, `createAtlasPage()`
  - `src/engine/render/opengl/gl_helper.h` — 已有 `ScopedGLUnpackAlignment` RAII
- **预期收益**: 减少 per-glyph 的 2 次 GPU 同步查询
- **实际方案**:
  - 纹理绑定：上传完成后直接 `glBindTexture(GL_TEXTURE_2D, 0)` 恢复，无需查询旧值
  - Unpack 对齐：直接 `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` 设置，上传后恢复默认值 4。不使用 `ScopedGLUnpackAlignment` RAII（其内部仍调用 `glGetIntegerv`，会引入同步开销），因为 Font 内部的 unpack 上下文已知且固定
- [x] 移除 `loadGlyph` 和 `createAtlasPage` 中所有 `glGetIntegerv` 查询
- [x] 纹理上传后 bind 回 0，unpack alignment 裸设 1 / 恢复 4
- **备注**: `createAtlasPage` 中的 GL 状态查询也一并处理

---

## 低优先级

### 4. Layout Cache LRU 淘汰改为 O(1)

- **现状**: `trimLayoutCache()` 使用 `std::min_element` 遍历整个 map 查找最旧条目，复杂度 O(n)。
- **涉及文件**:
  - `src/engine/render/text_renderer.h` — 新增 `std::list` LRU 链表
  - `src/engine/render/text_renderer.cpp` — `buildLayout()` / `trimLayoutCache()`
- **预期收益**: 缓存满时淘汰从 O(n) 降为 O(1)
- **备注**: 当前默认容量 256，实际影响有限
- [ ] 用 `std::list<LayoutKey>` 维护访问顺序
- [ ] map value 中存储 list iterator，命中时 splice 到头部
- [ ] 淘汰时取 list 尾部，O(1)

### 5. SpriteBatch Buffer Orphaning

- **现状**: `SpriteBatch::flush()` 使用 `glBufferSubData` 上传数据，若 GPU 仍在消费上一帧的 buffer，会产生隐式同步等待。
- **涉及文件**:
  - `src/engine/render/opengl/sprite_batch.cpp` — `flush()`
- **预期收益**: 减少 CPU/GPU 同步等待
- [ ] 方案 A: flush 前调用 `glBufferData(target, size, nullptr, GL_DYNAMIC_DRAW)` 做 buffer orphaning
- [ ] 方案 B (可选): 使用 persistent mapped buffer（需 OpenGL 4.4+）
