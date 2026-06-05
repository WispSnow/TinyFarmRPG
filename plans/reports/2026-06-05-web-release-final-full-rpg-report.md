# Web Release Final Full RPG Report

## Status

Full RPG Web release is complete for the Chrome single-thread target.

The release covers the playable teaching-demo loop: title, character creation, home exterior/interior, town travel, battle Attack / Item / Guard / Escape, battle victory, battle defeat recovery, Effekseer VFX, shop buy/sell/failure feedback, quest accept/progress/turn-in/reward, recruit, rest, wardrobe appearance change, settings persistence, save, refresh, and load.

## Build And Acceptance

Primary acceptance command:

```bash
python3 tools/web_release/web_release_runbook.py auto --profile full-rpg --build-dir build/web-release-final --output-dir /private/tmp/tinyfarm-battle-depth-auto-final-20
```

Result:

- Status: passed.
- Browser: Chrome `148.0.7778.216`.
- Release gate failures: `0`.
- Smoke profile: `full-rpg`.
- Manual checklist record: `/private/tmp/tinyfarm-phase28-manual-check/manual-preview.json`.
- Release report: `/private/tmp/tinyfarm-battle-depth-auto-final-20/release-report.md`.
- Artifact manifest: `/private/tmp/tinyfarm-battle-depth-auto-final-20/artifact-manifest.json`.
- Smoke JSON: `/private/tmp/tinyfarm-battle-depth-auto-final-20/chromium-smoke.json`.
- Screenshots: `70` files in `/private/tmp/tinyfarm-battle-depth-auto-final-20/smoke`.

## Package Summary

| Package | Files | Size | Artifact | Ready time |
|---|---:|---:|---:|---:|
| boot | 38 | 2.9 MiB | web-boot-preload.args | boot preload |
| audio-core | 5 | 4.1 MiB | 4.1 MiB | 30 ms |
| shared-ui | 172 | 13.3 MiB | 13.3 MiB | 75 ms |
| rpg-core | 40 | 95.9 KiB | 103.2 KiB | 7 ms |
| home-map | 38 | 553.0 KiB | 561.1 KiB | 11 ms |
| town-map | 1 | 38.9 KiB | 39.2 KiB | 13 ms |
| battle-core | 4 | 434.0 KiB | 434.9 KiB | 21 ms |
| vfx-core | 83 | 1.2 MiB | 1.2 MiB | 28 ms |

Deploy summary:

- Deploy files: `13`.
- Deploy size: `31.2 MiB`.
- Deploy gzip size: `18.0 MiB`.
- Deploy brotli size: `14.8 MiB`.
- `TinyFarmRPG-Web.data`: `2.9 MiB`, still boot-only sized.

## Runtime Timings

| Metric | Actual | Budget | Status |
|---|---:|---:|---|
| title_interactive | 424 ms | 45000 ms | passed |
| new_game_to_map | 2602 ms | 30000 ms | passed |
| gameplay_flow | 27588 ms | 120000 ms | passed |
| reload_load_to_map | 2677 ms | 30000 ms | passed |
| full_rpg_basic_flows | 310279 ms | tracked | passed |

## Rendering And VFX

- WebGL platform: `webgl2`.
- `floatColorFramebuffers=true`.
- `rgba16fColorRenderable=true`.
- `linearFloatFiltering=true`.
- HDR post-processing: `true`.
- Emissive: `true`.
- Bloom: `true`.
- Fallback reasons: empty.
- Postprocessing activity: `emissiveSprites=12`, `emissiveDrawCalls=1`, `bloomDrawCalls=11`, `bloomLevels=4`.
- Effekseer policy: `effekseer_enabled=true backend=effekseer status=enabled`.

## Gameplay Coverage

Covered flows:

- `new_game_character_confirm`
- `home_exterior_movement`
- `home_exterior_to_home_interior_round_trip`
- `inventory_open_close`
- `hotbar_open_close`
- `pause_open_close`
- `settings_change_reload_restore`
- `primary_tool_action`
- `scripted_merchant_dialogue`
- `save_reload_load`
- `corrupt_save_slot_skip`
- `web_release_diagnostics_snapshot`
- `full_rpg_profile_diagnostics_gate`
- `home_exterior_to_town`
- `town_enemy_encounter`
- `battle_attack_item_guard_escape_matrix`
- `battle_skill_vfx`
- `battle_victory_return_to_map`
- `battle_reward_writeback`
- `battle_defeat_flow`
- `battle_hp_mp_inventory_writeback`
- `shop_buy_sell_failure_feedback`
- `quest_accept_progress_turn_in_reward`
- `recruit_accept_party_writeback`
- `rest_recovery_time_advance`
- `wardrobe_appearance_change`
- `hdr_bloom_postprocessing_smoke`
- `battle_defeated_encounter_save_reload_matrix`
- `full_rpg_save_reload_verify`

Battle-depth validation now records Attack, Guard, Item, Escape, defeat recovery, HP/MP writeback, potion stock writeback, and save/reload restoration of non-respawning defeated encounter `1001`.

## Current Release Procedure

Full release acceptance:

```bash
python3 tools/web_release/web_release_runbook.py auto \
  --configure \
  --profile full-rpg \
  --build-dir build/web-release \
  --output-dir build/web-release/web-release-auto
```

Existing artifact recheck:

```bash
python3 tools/web_release/web_release_runbook.py auto \
  --skip-build \
  --profile full-rpg \
  --build-dir build/web-release-final \
  --output-dir build/web-release-final/web-release-auto
```

Manual preview:

```bash
python3 tools/web_release/web_release_runbook.py manual \
  --skip-build \
  --build-dir build/web-release-final \
  --output-dir build/web-release-final/web-release-manual \
  --open
```
