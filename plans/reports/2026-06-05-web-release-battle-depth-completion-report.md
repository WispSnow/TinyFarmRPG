# Web Release Battle Depth Completion Report

## Summary

Battle-depth coverage is complete for the Chrome single-thread full-rpg Web release target.

This pass closes the remaining Phase 25 items:

- Battle action matrix: Attack / Item / Guard / Escape.
- Battle defeat flow and recovery to `home_interior`.
- HP/MP runtime state writeback.
- Potion stock writeback after battle item use.
- Non-respawning defeated encounter save/reload restoration for `town` encounter `1001`.

## Implementation Notes

- `BattleScene` Web diagnostics now publishes `lastAction.sequence` and a bounded `lastActionHistory`, allowing the smoke to wait for specific action results instead of relying on coordinate-only clicks.
- Escape retry behavior is deterministic enough for automation: after the first failed escape attempt, a later escape attempt is guaranteed to resolve.
- `web_smoke.py` now refuses to silently fall back to a different encounter when a preferred troop is requested.
- The `troop.slime` path explicitly targets `encounter_id=1001` before the quest battle, because nearby `1002` and `1101` can otherwise satisfy generic movement but do not test non-respawning encounter persistence.

## Validation

```bash
python3 -m py_compile tools/web_release/web_smoke.py tools/web_release/web_release_runbook.py
ninja -C build/debug engine_tests game_tests
./build/debug/tests/game_tests --gtest_filter='BattleActionResolverTest.EscapeFailureAdvancesTurn:BattleActionResolverTest.EscapeSuccessEndsBattle:BattleActionResolverTest.EscapeBecomesGuaranteedAfterFirstFailure:GameSceneBattleRewardWritebackTest.DefeatOnlyWritesBackBattleItemDelta:GameSceneBattleRewardWritebackTest.EscapedOnlyWritesBackBattleItemDelta:SaveServiceAsyncBehaviorTest.RoundtripRestoresEquipmentAndPartyRuntimeState:SaveServiceAsyncBehaviorTest.LoadFromFileRestoresDefeatedEncounters'
python3 tools/web_release/web_release_runbook.py auto --profile full-rpg --build-dir build/web-release-final --output-dir /private/tmp/tinyfarm-battle-depth-auto-final-20
```

Results:

- Python py_compile: passed.
- Debug `engine_tests` and `game_tests`: passed.
- Targeted battle/save tests: passed.
- Chrome full-rpg runbook: passed.
- Browser: `Chrome 148.0.7778.216`.
- Smoke JSON: `/private/tmp/tinyfarm-battle-depth-auto-final-20/chromium-smoke.json`.
- Runbook report: `/private/tmp/tinyfarm-battle-depth-auto-final-20/release-report.md`.
- Screenshots: `70` PNG files under `/private/tmp/tinyfarm-battle-depth-auto-final-20/smoke`.

## Key Evidence

Covered flows now include:

- `battle_attack_item_guard_escape_matrix`
- `battle_defeat_flow`
- `battle_hp_mp_inventory_writeback`
- `battle_defeated_encounter_save_reload_matrix`

The full-rpg smoke recorded:

- Attack action damage: `19`.
- Guard action applied.
- Potion item action recovered HP: `50`.
- Escape ended with `outcome=Escaped`.
- Defeat flow returned to `home_interior` with player HP restored above zero.
- Quest battle used `encounter_id=1001`, `encounter_troop_id=troop.slime`, and ended with `outcome=Victory`.
- Save/reload verification persisted `defeated_town_encounters=[1001]`.
- Runtime state and inventory matched exactly before save and after load.

## Release State

The round 4 full-rpg Web release plan no longer has unchecked gameplay migration items. Remaining future work should be tracked as quality, browser matrix, deployment, or content-expansion work rather than as core Web migration completion.
