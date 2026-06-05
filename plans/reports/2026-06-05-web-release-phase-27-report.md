# Web Release Phase 27 Report

日期：2026-06-05

## 结论

Phase 27 已完成 Chrome WebGL2 下 HDR emissive 与 Bloom 恢复。当前 Chrome 环境支持 `EXT_color_buffer_float`、`GL_RGBA16F` color-renderable 与 float linear filtering，Web release diagnostics 显示 HDR 后处理、Emissive、Bloom 均已启用，fallback reason 为空。

## 主要变更

- `GLRenderer`
  - 移除 Web 后处理编译期关闭路径，改为运行时 WebGL2 capability gate。
  - 新增 `EXT_color_buffer_float`、`OES_texture_float_linear`、`GL_RGBA16F + GL_HALF_FLOAT` FBO completeness 探测。
  - `TinyFarmRPGWebReleaseDiagnostics.renderCapabilities` 增加 `rgba16fColorRenderable`、fallback reason、Emissive/Bloom draw call 与 level 统计。
- `BloomPass` / `EmissivePass`
  - Bloom ping-pong render target 从 `GL_RGB16F` 改为 WebGL2 可渲染的 `GL_RGBA16F`。
  - Emissive FBO 与 capability probe 统一使用 `GL_RGBA16F + GL_HALF_FLOAT`。
- Shader / smoke
  - `emissive.frag` 避免 GLSL ES 300 保留字 `sample`。
  - `web_smoke.py` 自动设置 emsdk 环境中的 `EMSDK_PYTHON`，避免 em++ 误用 Xcode Python 3.9。
  - full-rpg smoke 增加 `hdr_bloom_postprocessing_smoke`，断言 Emissive/Bloom pass 在实际地图内容中产生 draw calls。
  - battle smoke 状态机补齐 `PartyCommand -> SkillList` 中间态，避免多角色队伍战斗自动化误判。
- `asset-budget.json`
  - 同步 shader 变更后的 full Web manifest 预算到 `23667866` bytes。

## 验证

```bash
ninja -C build/debug engine_tests game_tests
./build/debug/tests/engine_tests --gtest_filter='WebGameplayTargetSourceTest.Phase27HdrBloomRuntimeGateIsPresent:BloomPrecisionRegressionTest.PingPongTexturesUseHdrFormat:WebShellUiSourceTest.KeepsWebGlFeatureProbeForRuntimePostProcessingGate'
python3 -m py_compile tools/web_release/web_smoke.py tools/web_release/validate_web_release.py
python3 tools/web_release/web_smoke.py --build-dir build/web-release-final --profile full-rpg --jobs 8 --output-dir /private/tmp/tinyfarm-phase27-full-rpg-smoke --json-output /private/tmp/tinyfarm-phase27-full-rpg-smoke/chromium-smoke.json
git diff --check
```

结果：

- Debug build 通过。
- Phase 27 source guards 通过。
- Python compile 通过。
- Web release gate 通过。
- Chrome `full-rpg` smoke 通过。
- `git diff --check` 通过。

关键 Chrome smoke 结果：

| Field | Value |
|---|---:|
| browser | Chrome 148.0.7778.216 |
| title interactive | 530 ms |
| new game to map | 2583 ms |
| reload load to map | 2786 ms |
| full RPG basic flows | 193766 ms |
| performance budget | passed |

关键 render diagnostics：

| Field | Value |
|---|---:|
| platform | webgl2 |
| floatColorFramebuffers | true |
| rgba16fColorRenderable | true |
| linearFloatFiltering | true |
| hdrPostProcessing | true |
| emissive | true |
| bloom | true |
| maxTextureSize | 16384 |
| maxRenderbufferSize | 16384 |
| maxSamples | 8 |
| emissiveSprites | 12 |
| emissiveDrawCalls | 1 |
| bloomDrawCalls | 11 |
| bloomLevels | 4 |

Fallback reason 均为空：`hdrFallbackReason=""`、`bloomFallbackReason=""`、`emissiveFallbackReason=""`。

## 注意

- `./build/debug/tests/engine_tests` 全量仍有 4 个既有源码守卫失败，和本阶段无关：`web_asset_package.cpp` exception token、旧 Phase17 截图名、旧 depth-clear 文本、旧 Effekseer CMake 文本。本阶段使用 Phase 27 相关过滤集验证。
- World VFX 仍保持独立通道，未接入 Bloom source；本阶段恢复的是地图/灯光自发光与 Bloom 后处理路径。

## 后续

- Phase 28：完整发布验收、文档与 CI。
- 战斗增强项仍待后续覆盖：Attack / Item / Guard / Escape、失败流程、defeated encounter 保存刷新恢复矩阵。
