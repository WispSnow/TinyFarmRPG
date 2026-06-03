# 2026-06-03 Web Release Phase 14 Report

## 结论

Phase 14 的 Chromium 发布候选自动 smoke 已通过。当前固定命令会执行构建、release gate、本地 server、header 检查和浏览器 gameplay smoke。

Safari 已记录版本与自动化阻塞原因：本机 Safari 26.5 可用，但 `safaridriver` 需要在 Safari Settings 的 Developer 区域启用 `Allow remote automation` 后才能创建 WebDriver session。因此 Safari 手工 smoke 仍需补验；这不是 TinyFarmRPG Web 构建或 wasm 运行时失败。

## 验证命令

```bash
python3 -m py_compile \
  tools/web_release/serve_web_release.py \
  tools/web_release/validate_web_release.py \
  tools/web_release/package_web_assets.py \
  tools/web_release/web_smoke.py

cmake --build build/debug -j 8
ctest --test-dir build/debug -R "WebGameplayTargetSourceTest" --output-on-failure

python3 tools/web_release/web_smoke.py --build-dir build/web-gameplay-phase11
```

结果：

- `WebGameplayTargetSourceTest`: 12/12 passed.
- `web_smoke.py`: passed.
- Chrome: `Google Chrome 147.0.7727.102`.
- Safari: `Safari 26.5 (21624.2.5.11.4)`; WebDriver session blocked by Safari remote automation setting.

## Chromium Smoke 覆盖

自动流程覆盖：

- 标题页加载并截图。
- 点击 `Start` 进入 appearance 流程。
- 点击 `Confirm` 进入 `home_exterior`。
- 网络层观察到 `web-packages/home-map.tfpack` 返回 200，MIME 为 `application/octet-stream`。
- 进入地图后通过键盘输入关闭初始对话并移动玩家。
- 打开暂停菜单，保存 slot0。
- 返回 gameplay 后继续移动，覆盖保存 slot0。
- 刷新页面，从标题页点击 `Load`，读取 slot0 并回到 `home_exterior`。

坐标验收：

- 保存前：`{"x": 311.0, "y": 307.8624267578125}`
- 移动后：`{"x": 311.0, "y": 167.8624725341797}`
- delta：`{"x": 0.0, "y": -139.9999542236328}`

耗时：

- title interactive: `715 ms`
- new game to map: `1981 ms`
- reload load to map: `12935 ms`

产物与报告：

- JSON: `build/web-gameplay-phase11/web-smoke/chromium-smoke.json`
- release gate JSON: `build/web-gameplay-phase11/web-smoke/release-gate.json`
- title screenshot: `build/web-gameplay-phase11/web-smoke/phase14-chromium-title.png`
- map screenshot: `build/web-gameplay-phase11/web-smoke/phase14-chromium-map.png`
- after-load screenshot: `build/web-gameplay-phase11/web-smoke/phase14-chromium-after-load.png`
- additional save/menu screenshots are in `build/web-gameplay-phase11/web-smoke/`.

## Header 与发布检查

本地 preview server 已校验以下路径：

- `/TinyFarmRPG-Web.html`: `text/html`, `Cache-Control: no-cache`
- `/TinyFarmRPG-Web.js`: `application/javascript`, `Cache-Control: no-cache`
- `/TinyFarmRPG-Web.wasm`: `application/wasm`, `Cache-Control: no-cache`
- `/TinyFarmRPG-Web.data`: `application/octet-stream`, `Cache-Control: no-cache`
- `/web-packages/home-map.tfpack`: `application/octet-stream`, `Cache-Control: no-cache`

当前单线程 demo 的 `cross_origin_isolated=false`，HTML / JS / wasm / data / tfpack 均不要求 COOP / COEP。COOP / COEP 仅应作为未来 pthreads 变体 gate。

体积：

- preload: 280 files, 20.8 MiB
- `.html`: 19.1 KiB, gzip 13.3 KiB
- `.js`: 268.9 KiB, gzip 59.5 KiB
- `.wasm`: 7.1 MiB, gzip 2.5 MiB
- `.data`: 20.8 MiB, gzip 13.7 MiB
- `boot`: 28 files, 2.8 MiB
- `shared-ui`: 166 files, 13.3 MiB
- `home-map`: 81 files, 687.1 KiB
- `audio-core`: 5 files, 4.1 MiB

## Safari 记录

探测结果：

```bash
defaults read /Applications/Safari.app/Contents/Info CFBundleShortVersionString
# 26.5

safaridriver --version
# Included with Safari 26.5 (21624.2.5.11.4)

curl -X POST http://127.0.0.1:18444/session \
  -H 'Content-Type: application/json' \
  --data '{"capabilities":{"alwaysMatch":{"browserName":"safari"}}}'
# session not created: You must enable 'Allow remote automation' in the Developer section of Safari Settings to control Safari via WebDriver.
```

待补手工 smoke：

```bash
python3 tools/web_release/serve_web_release.py --build-dir build/web-gameplay-phase11 --port 8787
```

然后在 Safari 中打开 `http://127.0.0.1:8787/TinyFarmRPG-Web.html`，手工确认：

- WebGL2 canvas 非空。
- 标题页 `Start` 可进入 appearance，并能 `Confirm` 进入地图。
- 键盘移动可用。
- 暂停菜单保存 slot0。
- 刷新后 `Load` slot0 可回到 `home_exterior`。
- 音频用户手势后无 fatal。
- IndexedDB / persistent storage 刷新后保留 slot 文件。

## 当前禁用项

| 项目 | 当前状态 | 后续路线 |
|------|----------|----------|
| Effekseer | Web 默认关闭 | 单独建立 wasm / WebGL2 backend bring-up phase |
| Bloom / HDR emissive | Web 默认关闭或降级 | 先完成 WebGL2 float framebuffer capability gate，再恢复 shader 变体 |
| 完整地图资源 | 仅 `home-map` 完成运行时包 gate | 扩展 package index，并为每个地图包增加 ready gate |
| 完整音频包 | `audio-core` 已分包，仍有非核心音频 warning | 扩展音频资源分包或把非核心音频改为懒加载 |
| boot-only `.data` cutover | 当前仍保留完整 preload `.data` | 新增 boot-only build option，再接入 `shared-ui` / `audio-core` 运行时加载时序 |
| pthreads 构建 | 当前单线程发布候选不启用 | 独立 pthreads 变体，发布时强制 COOP / COEP |

## 后续建议

下一步不应继续扩大 Phase 14，而应进入发布收敛任务：

- 补 Safari 手工 smoke，确认是否有 WebGL2、IndexedDB、音频策略差异。
- 增加 boot-only `.data` 构建选项，把当前 20.8 MiB preload 收敛到 boot 包。
- 把 `shared-ui` 和 `audio-core` 接入运行时 package ready gate。
- 将 Chromium smoke 加入 CI 或 release job，至少在 Web release branch 上运行。
