# Web Release Phase 26 Report

日期：2026-06-05

## 结论

Phase 26 已完成 Web `full-rpg` profile 的基础 RPG 玩法闭环。Chrome smoke 覆盖商店买入/卖出/失败反馈、任务领取、3 次真实 slime 战斗推进、任务交付奖励、Lyria 招募与后续战斗识别、休息恢复与时间推进、衣柜换装、保存刷新读档恢复。

本阶段不声明完整战斗命令矩阵完成。Attack / Item / Guard / Escape、失败流程、defeated encounter 存档矩阵在 Phase 26 时作为后续战斗扩展项记录。该扩展项已在 `2026-06-05-web-release-battle-depth-completion-report.md` 中补齐。

## 主要变更

- `tools/web_release/web_smoke.py`
  - 扩展 `full-rpg` profile，新增 shop / quest / recruit / rest / wardrobe / save-reload 基础流程。
  - quest flow 恢复真实 `required_count = 3`，通过 `troop.slime` 与可重生 `troop.slime_single` 完成 3 次 slime 目标。
  - package 等待改为“新日志或 diagnostics 已加载”双路径，支持重复进 town / battle。
  - battle 自动化支持多成员队伍下的直接 `TargetSelect` 状态。
- `src/game/scene/game_scene.cpp`
  - Web gameplay diagnostics 增加 inventory、party、quest progress、runtime actor state、appearance、time 等状态。
- `src/game/scene/*` 与 `src/game/system/*`
  - 商店、任务、招募、休息、衣柜流程增加稳定日志与 keyboard confirm 支持。
- `assets/maps/home_exterior.tmj`
  - 调整 Lyria 招募 NPC 站位并设置 `wander_radius_override = 0`，避免关键招募入口随机游走。
- `assets/maps/home_interior.tmj`
  - 增加 player actor，修复 home interior 存档读档后缺少 Player entity 的问题。
- `manifests/assets/asset-budget.json`
  - 更新 full Web manifest 预算到 `23667850` bytes。

## 验证

```bash
PYTHONPYCACHEPREFIX=/private/tmp/tinyfarm-pycache python3 -m py_compile tools/web_release/web_smoke.py
python3 tools/web_release/validate_web_release.py --build-dir build/web-release-final --json-output /private/tmp/tinyfarm-phase26-validate-final.json
EMSDK_PYTHON=/Users/ziyu/.local/emsdk/python/3.13.3_64bit/bin/python3.13 python3 tools/web_release/web_smoke.py --build-dir build/web-release-final --skip-build --profile full-rpg --jobs 8 --output-dir /private/tmp/tinyfarm-phase26-full-rpg-smoke-45 --json-output /private/tmp/tinyfarm-phase26-full-rpg-smoke-45/chromium-smoke.json
ninja -C build/debug engine_tests game_tests
./build/debug/tests/game_tests
./build/debug/tests/engine_tests --gtest_filter=WebGameplayTargetSourceTest.Phase25FullRpgBattleFlowIsReachableOnWeb:WebGameplayTargetSourceTest.Phase26FullRpgBasicGameplayFlowsArePresent:WebGameplayTargetSourceTest.Phase23WebDiagnosticsAndSmokeProfilesArePresent
git diff --check
```

结果：

- Python compile 通过。
- Web release gate 通过。
- Chrome `full-rpg` smoke 通过。
- `game_tests` 通过：784 passed，11 skipped。
- Phase 23/25/26 source guard 通过。
- `git diff --check` 通过。

关键 full-rpg smoke 结果：

| Field | Value |
|---|---:|
| browser | Chrome 148.0.7778.216 |
| title interactive | 564 ms |
| new game to map | 2579 ms |
| reload load to map | 2940 ms |
| full RPG basic flows | 191851 ms |
| performance budget | passed |
| vfx backend | effekseer |

关键状态断言：

- Shop：金币 `300 -> 270 -> 285`，potion `3 -> 4 -> 3`，失败购买已断言。
- Quest：`kill_slimes` 进度达到 `3`，任务完成后金币 `344`，potion `5`。
- Recruit：`actor.lyria` 写入 active / recruited party。
- Rest：时间 `2770 -> 3293`，玩家 HP/MP 恢复。
- Wardrobe：appearance signature 变化并在保存刷新读档后保持。
- Save reload：读档后仍在 `home_interior`，完成任务和招募状态保留。

## 后续

- Phase 27：恢复 Bloom / HDR emissive / 高级后处理。
- Battle-depth 扩展已在 `2026-06-05-web-release-battle-depth-completion-report.md` 中完成：Attack / Item / Guard / Escape、失败流程、HP/MP 与背包写回、defeated encounter 保存刷新恢复矩阵。
