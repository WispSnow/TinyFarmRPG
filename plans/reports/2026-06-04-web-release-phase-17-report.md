# 2026-06-04 Web Release Phase 17 Report

## 结论

Phase 17 已完成。Chrome 自动 smoke 已从最小主路径扩展到 gameplay 覆盖：inventory、hotbar、pause、工具动作、`home_exterior` 与 `home_interior` 往返、商人对话、保存覆盖、刷新后读档。

```mermaid
flowchart LR
  A["Title"] --> B["Appearance"]
  B --> C["home_exterior"]
  C --> D["Inventory / Hotbar / Pause"]
  D --> E["Tool Action"]
  E --> F["home_interior"]
  F --> G["home_exterior"]
  G --> H["Merchant Dialogue"]
  H --> I["Save / Overwrite"]
  I --> J["Reload / Load"]
```

## 主要变更

- `tools/web_release/web_smoke.py` 扩展 gameplay smoke，并在 validate 前显式重建 runtime packages，避免 no-op build 留下陈旧 `.tfpack`。
- `tools/web_release/audit_web_resource_coverage.py` 新增资源覆盖审计，输出 JSON/Markdown。
- `inventory_menu.rml/rcss` 纳入 Web release manifest、asset audit、release gate 和 shared-ui 资源覆盖。
- gameplay 输入路由允许 Web gameplay context 下的 key down 与 mouse button down 穿过 RmlUi HUD，用于 hotbar/menu key 与主工具动作。
- Web smoke 增加 `TinyFarmRPGSmokeState` 运行时坐标快照，商人交互改为坐标闭环移动，减少固定时长漂移。

## 资源覆盖

审计输出：`build/web-gameplay-phase11/web-release-phase17-resource-coverage.md`

| Package | Files | Size | 覆盖面 |
|---|---:|---:|---|
| `boot` | 36 | 2.9 MiB | title、核心 config、shader、title UI |
| `shared-ui` | 164 | 13.3 MiB | HUD、hotbar、inventory、pause、save、dialogue UI |
| `home-map` | 78 | 647.1 KiB | `home_exterior`、`home_interior`、Lua、map/data catalogs |
| `audio-core` | 5 | 4.1 MiB | music cues、menu sounds |

缺失必需资源：`{}`

## 验证

- `python3 tools/web_release/web_release_runbook.py auto --output-dir build/web-gameplay-phase11/web-release-phase17-smoke`
  - 结果：通过。
  - Chrome：`Chrome 147.0.7727.102`
  - `covered_flows`：`new_game_character_confirm`、`home_exterior_movement`、`home_exterior_to_home_interior_round_trip`、`inventory_open_close`、`hotbar_open_close`、`pause_open_close`、`primary_tool_action`、`scripted_merchant_dialogue`、`save_reload_load`
- `./build/debug/tests/engine_tests '--gtest_filter=WebGameplayTargetSourceTest.*'`
  - 结果：15 tests passed。

## 后续未覆盖

Phase 18 起继续处理渲染、音频和 VFX parity。仍未纳入 Phase 17 smoke 的玩法包括完整战斗、商店交易、任务领取/交付、招募、休息和外观衣柜。
