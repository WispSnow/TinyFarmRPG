# 2026-06-03 Web Release Phase 16 Report

## 结论

Phase 16 已完成。Web runtime package 现在通过统一 registry 加载，`shared-ui`、`home-map`、`audio-core` 三个 `.tfpack` 都有真实运行时 gate，并被 Chrome smoke 观测到网络响应。

`audio-core` 在首次用户手势后加载，随后预热已注册音频缓存；音频资源未包含在当前 Web 包时降为 debug 诊断，不再以旧 preload 语义污染 release warning。

## 主要改动

- 新增 `src/engine/platform/web_asset_package_registry.{h,cpp}`。
  - 固定管理 `shared-ui`、`home-map`、`audio-core` 的 package id、URL、加载状态和 last error。
  - 加载日志包含 package id、URL 和耗时。
- `TitleScene`、`GameScene`、`MapManager` 改为通过 package registry 加载运行时包。
- `GameScene` 在 Web 下同时 gate `shared-ui` 和 `home-map`，避免直接进入 gameplay 时依赖标题页先加载 UI 包。
- `GameApp` 在首次用户手势时加载 `audio-core`，并调用 `ResourceManager::preloadRegisteredAudioResources()`。
- `ResourceManager` 新增已注册音频预热入口。
- `AudioPlayer` 在 Web 下将“注册但当前包未落盘”的音频播放请求降为 debug；真正已落盘但未加载仍保持 warning。
- `web_smoke.py` 扩展：
  - 预检 `shared-ui.tfpack`、`home-map.tfpack`、`audio-core.tfpack` 的 MIME/cache header。
  - 等待 `audio-core`、`shared-ui`、`home-map` package ready 日志。
  - 断言三个 `.tfpack` 都出现在网络响应中。
  - 记录 `warning_logs` 明细。
- `web_release_runbook.py` 修复 emsdk 环境自动注入。
  - 排除 `python*-config`，避免误设 `EMSDK_PYTHON`。
  - 自动注入 `upstream/bin`、`BINARYEN_ROOT`、`LLVM_ROOT`，避免新 shell 未 source `emsdk_env.sh` 时 reconfigure 失败。

## 验证命令

```bash
python3 tools/web_release/web_release_runbook.py auto \
  --skip-smoke \
  --output-dir build/web-gameplay-phase11/web-release-phase16-build-only

cmake --build build/debug --target engine_tests -j 10

./build/debug/tests/engine_tests '--gtest_filter=WebGameplayTargetSourceTest.*'

python3 tools/web_release/web_release_runbook.py auto \
  --skip-build \
  --output-dir build/web-gameplay-phase11/web-release-phase16-smoke
```

结果：

- Web build: passed.
- Release gate: passed.
- `WebGameplayTargetSourceTest.*`: 14 passed.
- headed Chrome smoke: passed.
- Browser: `Chrome 147.0.7727.102`.

## Smoke 证据

Chrome smoke 观察到：

- `audio-core.tfpack`: 200 on first run, 304 after reload.
- `shared-ui.tfpack`: 200 on first run, 304 after reload.
- `home-map.tfpack`: 200 on first run, 304 after reload.

关键日志：

- `WebAssetPackageRegistry: package 'audio-core' ready`
- `ResourceManager: registered audio preload complete (sounds=3, music=2, skipped_missing=14, failed=0)`
- `WebAssetPackageRegistry: package 'shared-ui' ready`
- `WebAssetPackageRegistry: package 'home-map' ready`

Smoke timings from latest run:

- title interactive: 465 ms.
- new game to map: 2352 ms.
- movement delta: `{"x": 0.0, "y": -139.9999542236328}`.

Remaining warnings:

- Chrome reports `ScriptProcessorNode is deprecated` from miniaudio WebAudio twice, once per page load. This is a dependency-level warning and not a missing package/resource warning.

## 后续

Phase 17 继续扩展 gameplay coverage。当前 `audio-core` 只覆盖 title/gameplay music、UI click/hover 和 `pop.mp3`；工具动作、动物、battle SFX 仍应在 Phase 17 的资源覆盖表中决定归属，而不是继续以“注册但未加载”形式隐式存在。
