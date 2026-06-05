# Web Release Phase 28 Report

## Summary

Phase 28 已完成。完整 Web release 现在有可重复的 full RPG runbook、CI 入口、文档、artifact manifest、release report、截图和 manual checklist 记录。

## Changes

- `tools/web_release/web_release_runbook.py`
  - `auto` 支持 `--profile demo|full-rpg`，并保留 `--smoke-profile` 兼容入口。
  - `auto-check.json` 记录 `smoke_profile`。
  - `release-report.md` 增加 Runtime Package Index、Runtime Package Responses、Runtime Package Load Diagnostics、Render Capabilities、VFX Policy、Gameplay Coverage 和 Manual Checklist。
- `tools/web_release/web_smoke.py`
  - 从完整浏览器日志汇总 `package_load_events`，记录每个 runtime package 的 file count 和 ready time。
- `.github/workflows/web-release.yml`
  - 手动 Chromium job 默认 `smoke_profile=full-rpg`。
  - Chromium job 通过 `--profile ${{ inputs.smoke_profile }}` 跑完整验收。
  - artifact 上传增加 `web-package-index.json`、`release-gate.json` 和 `chromium-smoke-failed.json`。
- `docs/web_release.md`
  - 更新为 full RPG Web release 说明。
  - 明确 Effekseer WebGL2、HDR emissive、Bloom 的恢复状态和 fallback 条件。
  - 增加完整人工测试 checklist 和常见失败处理。
- `src/web/web_shell_ui.cpp`、`src/web/CMakeLists.txt`
  - 清理旧的 walking skeleton / deferred 文案。
- `tests/engine/web_gameplay_target_source_test.cpp`
  - 新增 Phase 28 source guard，防止 runbook、docs、CI、shell 文案和最终报告退回 demo/降级表述。

## Validation

```bash
python3 -m py_compile tools/web_release/web_release_runbook.py tools/web_release/web_smoke.py tools/web_release/validate_web_release.py tools/web_release/package_web_assets.py tools/web_release/serve_web_release.py
ninja -C build/debug engine_tests game_tests
./build/debug/tests/engine_tests --gtest_filter='WebGameplayTargetSourceTest.Phase20ReleaseRunbookArtifactsDocsAndCiArePresent:WebGameplayTargetSourceTest.Phase23WebDiagnosticsAndSmokeProfilesArePresent:WebGameplayTargetSourceTest.Phase28FullRpgReleaseDocsRunbookCiAndReportAreCurrent:WebShellUiSourceTest.KeepsWebGlFeatureProbeForRuntimePostProcessingGate'
python3 tools/web_release/web_release_runbook.py auto --skip-build --profile full-rpg --build-dir build/web-release-final --output-dir /private/tmp/tinyfarm-phase28-auto-final
python3 tools/web_release/web_release_runbook.py manual --skip-build --check-only --build-dir build/web-release-final --output-dir /private/tmp/tinyfarm-phase28-manual-check
git diff --check
```

Results:

- Python py_compile: passed.
- `ninja -C build/debug engine_tests game_tests`: passed.
- Phase 20/23/28/Web shell source guard filter: passed.
- `auto --profile full-rpg`: passed.
- Chrome: `Chrome 148.0.7778.216`.
- Release gate failures: `0`.
- Smoke profile: `full-rpg`.
- Manual check-only: prepared and release gate passed.
- `git diff --check`: passed.
- Final runbook artifacts:
  - `/private/tmp/tinyfarm-phase28-auto-final/auto-check.json`
  - `/private/tmp/tinyfarm-phase28-auto-final/artifact-manifest.json`
  - `/private/tmp/tinyfarm-phase28-auto-final/release-report.md`
  - `/private/tmp/tinyfarm-phase28-auto-final/chromium-smoke.json`
  - `/private/tmp/tinyfarm-phase28-auto-final/smoke/*.png`
  - `/private/tmp/tinyfarm-phase28-manual-check/manual-preview.json`

## Key Evidence

- Deploy files: `13`.
- Deploy size: `31.2 MiB`; gzip `18.0 MiB`; brotli `14.8 MiB`.
- Runtime packages:
  - `audio-core`: 5 files, `4.1 MiB`, ready `32 ms`.
  - `battle-core`: 4 files, `434.0 KiB`, ready `20 ms`.
  - `home-map`: 38 files, `553.0 KiB`, ready `10 ms`.
  - `rpg-core`: 40 files, `95.9 KiB`, ready `8 ms`.
  - `shared-ui`: 172 files, `13.3 MiB`, ready `81 ms`.
  - `town-map`: 1 file, `38.9 KiB`, ready `20 ms`.
  - `vfx-core`: 83 files, `1.2 MiB`, ready `26 ms`.
- Performance:
  - title interactive: `579 ms`.
  - new game to map: `2349 ms`.
  - gameplay flow: `26890 ms`.
  - reload load to map: `2876 ms`.
  - full RPG basic flows: `197750 ms`.
- Render:
  - `hdrPostProcessing=true`
  - `emissive=true`
  - `bloom=true`
  - fallback reasons empty.
  - `emissiveSprites=12`, `emissiveDrawCalls=1`, `bloomDrawCalls=11`, `bloomLevels=4`.
- VFX:
  - `Web release VFX policy: effekseer_enabled=true backend=effekseer status=enabled.`
- Screenshots: `72` PNG files generated under `/private/tmp/tinyfarm-phase28-auto-final/smoke`.

## Notes

The release report now explicitly lists uncovered non-basic flows instead of treating basic RPG gameplay as future work. The remaining uncovered items are battle matrix extensions, not blockers for the full RPG basic release:

- `battle_attack_item_guard_escape_matrix`
- `battle_defeat_flow`
- `battle_defeated_encounter_save_reload_matrix`
