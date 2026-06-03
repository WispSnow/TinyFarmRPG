# 2026-06-03 Web Release Phase 15 Report

## 结论

Phase 15 已完成。Web release 现在默认使用 boot-only preload，`TinyFarmRPG-Web.data` 从完整 preload 的约 20.8 MiB 收敛到 2.9 MiB；完整资源通过 runtime package plan 覆盖。

headed Chrome 自动 smoke 通过，证明 boot-only `.data` 下标题页、`shared-ui.tfpack`、`home-map.tfpack`、保存和刷新读档主路径可用。

## 主要改动

- 新增 release manifest：`manifests/assets/web-release-full.args`。
- 新增 boot manifest：`manifests/assets/web-release-boot.args`。
- Web CMake 默认开启 `TF_WEB_BOOT_ONLY_PRELOAD=ON`，并要求 runtime packages 同时开启。
- `tf_target_web_preload()` 改为读取 boot manifest，full manifest 用于 runtime package coverage。
- `validate_web_release.py` 区分 boot preload、full package manifest 和 runtime packages。
- 标题页拆出 `title_widgets.rcss`，避免首屏拉入完整 `spritesheet.rcss` / `menu_widgets.rcss`。
- 首屏 boot 加入标题页字体、主题基础样式、按钮图集和 i18n 文件，标题按钮显示正常文本。
- `TitleScene` 在进入 appearance / load / menu 前加载 `shared-ui.tfpack`。
- LoadGameOptions 路径跳过默认初始地图加载，直接由 `SaveService::loadFromFile()` 加载存档地图。
- `web_smoke.py` 增加 headed Chrome 模式、失败 JSON、reload 阶段截图和保存持久化同步等待。

## 验证命令

后续优先使用 runbook wrapper，避免重新摸索构建目录、浏览器模式和日志位置：

```bash
# 自动验收：脚本检查 + release gate + 本地 server + headed Chrome gameplay smoke
python3 tools/web_release/web_release_runbook.py auto --skip-build

# 人工测试：脚本检查 + release gate + 本地 server；Ctrl+C 停止
python3 tools/web_release/web_release_runbook.py manual --skip-build --open

# 从零配置/编译标准 release build dir
python3 tools/web_release/web_release_runbook.py auto --configure --build-dir build/web-release
```

记录位置：

- 自动验收：`build/<web-build-dir>/web-release-auto/auto-check.json` 和 `auto-check.log`。
- 人工测试：`build/<web-build-dir>/web-release-manual/manual-preview.json` 和 `manual-preview.log`。

底层等价命令如下，保留用于排查 wrapper 外的问题：

```bash
env PYTHONPYCACHEPREFIX=/private/tmp/tinyfarm-pycache \
  python3 -m py_compile \
  tools/web_release/package_web_assets.py \
  tools/web_release/validate_web_release.py \
  tools/web_release/web_smoke.py \
  tools/asset_audit/audit_assets.py

EMSDK_PYTHON=/Users/ziyu/.local/emsdk/python/3.13.3_64bit/bin/python3.13 \
  cmake --build build/web-gameplay-phase11 -j 8

env PYTHONPYCACHEPREFIX=/private/tmp/tinyfarm-pycache \
  python3 tools/web_release/validate_web_release.py \
  --build-dir build/web-gameplay-phase11 \
  --json-output build/web-gameplay-phase11/web-smoke/release-gate-phase15.json

env PYTHONPYCACHEPREFIX=/private/tmp/tinyfarm-pycache \
  python3 tools/web_release/web_smoke.py \
  --build-dir build/web-gameplay-phase11 \
  --skip-build \
  --headed \
  --browser /Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome
```

结果：

- Python syntax check: passed.
- Web build: passed.
- Release gate: passed.
- headed Chrome smoke: passed.
- Browser: `Google Chrome 147.0.7727.102`.
- Runbook wrapper: `python3 tools/web_release/web_release_runbook.py auto --skip-build` passed; report written to `build/web-gameplay-phase11/web-release-auto/auto-check.json`.
- Runbook compile/gate path: `python3 tools/web_release/web_release_runbook.py auto --skip-smoke --output-dir build/web-gameplay-phase11/web-release-auto-build-only` passed; report written to `build/web-gameplay-phase11/web-release-auto-build-only/auto-check.json`.

## 产物体积

- boot preload: 36 files, 2.9 MiB.
- full package manifest: 281 files, 20.8 MiB.
- `TinyFarmRPG-Web.html`: 19.1 KiB, gzip 13.3 KiB.
- `TinyFarmRPG-Web.js`: 232.8 KiB, gzip 54.7 KiB.
- `TinyFarmRPG-Web.wasm`: 7.1 MiB, gzip 2.5 MiB.
- `TinyFarmRPG-Web.data`: 2.9 MiB, gzip 1.6 MiB.

Runtime packages:

- `boot`: 36 files, 2.9 MiB, delivered by Emscripten preload.
- `shared-ui`: 162 files, 13.2 MiB.
- `home-map`: 78 files, 647.1 KiB.
- `audio-core`: 5 files, 4.1 MiB.

## Smoke 覆盖

- 标题页加载并显示 `Start` / `Load` / `Exit`。
- 点击 `Start` 前加载 `shared-ui.tfpack`。
- 进入 appearance 并确认进入 `home_exterior`。
- 进入地图前加载 `home-map.tfpack`。
- 移动玩家并保存 slot0。
- 返回 gameplay 后再次移动并覆盖保存 slot0。
- 刷新页面，从标题页 `Load` slot0 回到 `home_exterior`。

本轮 smoke 使用 headed Chrome。`chrome-headless-shell` 可启动 CDP，但在 LoadGame 地图加载路径上仍有 headless/WebGL 稳定性问题；当前发布验收按用户要求以 Chrome 通过为准。

## 后续

Phase 16 继续接入 `shared-ui` / `audio-core` package registry 和更严格的 package ready gate，尤其需要处理音频核心包的真实加载和 warning 收敛。
