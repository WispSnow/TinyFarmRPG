# Docs Refresh Long-Term Plan

> Purpose: persistent execution reference for the TinyFarmRPG teaching-demo documentation refresh. This plan is written so a future agent turn can resume the work without relying on chat history.

## Status

Completed on 2026-05-27.

- P0 stale-doc fixes completed.
- P1 documentation navigation and student learning path completed.
- P2 high-value missing topic docs completed.
- P3 consistency verification completed.

Verification performed:

- Internal Markdown link check for `docs/**/*.md`.
- Inline path reference check for common project paths in `docs/**/*.md`.
- Mermaid block check for literal `\n` line breaks.
- Stale-pattern search for the original high-risk drift areas.
- `git diff --check`.

## Scope

- Update and supplement project documentation for students.
- Prefer changes under `docs/`; this plan itself lives under `plans/`.
- Do not modify C++/Lua/RmlUi/game assets unless the user explicitly expands the scope.
- Treat current source, resource, UI, scripts, tests, and tools directories as authoritative.
- Follow `AGENTS.md` and `for_agent/docs-guide.md`.

## Documentation Rules

- Use clear Chinese explanations for student-facing docs unless a local file is already intentionally English.
- Use Mermaid diagrams where they help understanding.
- In Mermaid diagrams, use `<br/>` for line breaks, not `\n`.
- Avoid Markdown syntax inside Mermaid node labels.
- Keep docs practical: explain purpose, key files, runtime flow, extension points, common pitfalls, and recommended code-reading paths.

## Audit Snapshot

Current `docs/` coverage is substantial, but uneven:

- Strong coverage: engine basics, ECS, input, rendering, RmlUi runtime, map pipeline, GameScene, map/world state, farm loop, inventory/hotbar, quests, shops, battle, Lua authoring, multithreading tutorials.
- Weak or missing entry points: global docs navigation, student learning path, runtime assembly, data catalog overview, UI scene/tabs overview, localization, tools/testing usage.
- Original highest-risk stale areas were resolved during this refresh:
  - Save docs now match the current schema and mention script state.
  - Lua binding docs now match the current engine/game script module split.
  - Debugging docs now match the Ninja preset build layout.
  - Stale root-level docs references were moved to their current section paths.
  - Mermaid diagrams now use `<br/>` for line breaks.
  - Tool naming now uses the current RmlUi tester name.

## Execution Phases

### P0 - Correct Stale Or Misleading Existing Docs

1. Update save documentation to schema v7.
2. Update Lua binding implementation guide to match current engine/game split:
   - `src/engine/script/script_host.*`
   - `src/game/script/script_game_api.*`
   - `src/game/script/tinyfarm_script_module.*`
   - `src/game/script/script_event_bridge.*`
3. Update debugging/build instructions to match `CMakePresets.json`:
   - generator: Ninja
   - build directory: `build/<preset>`
   - executable paths under `build/<preset>/`
4. Fix stale internal document paths.
5. Replace Mermaid `\n` labels with `<br/>` where they are inside Mermaid diagrams.
6. Fix outdated UI tester naming to the current RmlUi tester.

### P1 - Add Navigation And Student Learning Path

1. Add `docs/README.md` as the main documentation index.
2. Add `docs/tutorial/learning-path.md` for staged student reading.
3. Add compact section indexes if needed for:
   - `docs/engine/`
   - `docs/game/`
   - `docs/gameplay/`

### P2 - Fill High-Value Missing Topic Docs

Recommended new docs:

1. `docs/game/runtime-assembly.md`
   - `GameRuntimeAssembler`
   - `RuntimeServiceFactory`
   - `ContentCatalogLoader`
   - `SystemFactory`
   - service/system ownership
2. `docs/game/data-catalogs.md`
   - `assets/data/*.json`
   - `assets/data/rpg/*.json`
   - `GameContentManifest`
   - catalog loading and reference validation
3. `docs/gameplay/party-equipment-rest-recruitment.md`
   - party membership
   - actor runtime stats
   - actor progression
   - equipment
   - recruitment
   - rest recovery
4. `docs/game/ui-scenes.md`
   - RmlUi scene patterns
   - Inventory tabs
   - Map tab
   - Options tab
   - generated images
   - modal overlay scenes
5. `docs/game/localization.md`
   - `assets/i18n`
   - `LocalizationService`
   - `data-i18n`
   - `LanguageChangedEvent`
   - user setting persistence
6. `docs/testing/tools.md`
   - `visual_tester`
   - `rmlui_tester`
   - `battle_tester`
   - `scheduler_dot_dump`
   - `rpg_importer`

### P3 - Consistency And Verification

1. Check internal Markdown links that target docs.
2. Check common path references for files that no longer exist.
3. Check Mermaid blocks for literal `\n`.
4. Check docs for direct contradictions against source constants and manifest paths.
5. Run lightweight text searches; no code build is required unless documentation claims are tied to build commands.

## Suggested Work Order

1. P0 fixes.
2. `docs/README.md`.
3. `docs/tutorial/learning-path.md`.
4. Runtime assembly and data catalog docs.
5. UI/localization docs.
6. Party/equipment/rest/recruitment doc.
7. Tools/testing doc.
8. Final consistency pass.

## Completion Criteria

- Existing misleading docs from P0 are corrected.
- Students have a clear entry point and staged reading path.
- Every major project area has either a topic doc or an index pointing to the relevant existing docs.
- New docs point to current files and current data/resource locations.
- Mermaid diagrams follow the project docs guide.
- Final audit shows no known stale references from the original documentation audit remain.
