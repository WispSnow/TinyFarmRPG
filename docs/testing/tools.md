# 调试与验证工具

`tools/` 下的程序用于手动验证渲染、RmlUi、战斗、调度图和 RPG 数据导入。它们不替代 GoogleTest，而是补充“需要看画面或离线生成结果”的检查。

## 构建

工具目标由 `tools/CMakeLists.txt` 定义。顶层默认关闭 `BUILD_TOOLS`，`dev` 和 `dev-full` 预设会自动打开。

- `visual_tester` 依赖 Debug UI，只有 `ENABLE_DEBUG_UI=ON` 时构建。
- `rmlui_tester` 依赖 Debug UI，且需要 `BUILD_RMLUI_TESTER=ON`。
- `battle_tester` 与 `scheduler_dot_dump` 不依赖 Debug UI；`ENABLE_DEBUG_UI=OFF` 时仍应构建。

```bash
cmake --preset dev
cmake --build --preset dev --target visual_tester
cmake --build --preset dev --target rmlui_tester
cmake --build --preset dev --target battle_tester
cmake --build --preset dev --target scheduler_dot_dump
```

macOS/Linux 下，工具通常位于：

```bash
./build/dev/tools/visual_tester
./build/dev/tools/rmlui_tester
./build/dev/tools/battle_tester
./build/dev/tools/scheduler_dot_dump
```

## visual_tester

用途：打开一组 engine 视觉测试场景，用肉眼检查 tile、auto-tile、YSort、相机、VFX 等表现。

```bash
./build/dev/tools/visual_tester
```

入口文件：

- `tools/visual_tester/main.cpp`
- `tools/visual_tester/visual_test_cases.cpp`
- `tools/visual_tester/visual_test_suite_scene.cpp`

适合场景：

- 渲染管线或 pass 改动后。
- AutoTile / tileset 采样出现缝隙时。
- VFX 或 camera 行为需要可视确认时。

## rmlui_tester

用途：独立打开 `ui/rmlui` 下的 RML 文档，支持文件列表、手动输入路径和热重载。

```bash
./build/dev/tools/rmlui_tester
```

操作：

- 控制面板会列出 `ui/rmlui/**/*.rml`。
- `F5` 或 `Cmd/Ctrl + R` 重新加载当前文档。
- `ui/rmlui/tests/` 里有静态测试文档，可用于主题、表单、滚动、overlay 和 data binding 基础检查。

适合场景：

- 修改 `.rml` / `.rcss` 后先做快速视觉检查。
- 调 RmlUi box model、字体、spritesheet、modal 样式。
- 复现 UI 布局问题，但不想启动完整游戏流程。

## battle_tester

用途：用指定 actor、troop、背景和药水数量直接打开战斗场景。

```bash
./build/dev/tools/battle_tester
./build/dev/tools/battle_tester --troop troop.slime
./build/dev/tools/battle_tester --actors actor.player,actor.lyria,actor.tori --troop troop.gnome_pair --battle-background Grassland
```

常用参数：

| 参数 | 说明 |
|------|------|
| `--actors` | 逗号分隔 actor id |
| `--troop` | troop id |
| `--battle-background` | 背景 id |
| `--potion-count` | 初始药水数量 |

适合场景：

- 调战斗 UI、输入路由、胜利结算。
- 验证新 enemy/troop/skill/state 数据。
- 验证战斗背景、VFX、伤害飘字、HP 条。

## scheduler_dot_dump

用途：导出 SystemScheduler 中 post-gate parallel island 的 DOT 图。当前工具只导出这一座岛，不是完整 tick 时序图。

```bash
./build/dev/tools/scheduler_dot_dump
./build/dev/tools/scheduler_dot_dump docs/tmp/post_gate_parallel_island.dot
```

默认输出 `post_gate_parallel_island.dot`。可以用 Graphviz 渲染：

```bash
dot -Tpng post_gate_parallel_island.dot -o post_gate_parallel_island.png
```

适合场景：

- 调整系统并行声明后，检查依赖图是否符合预期；当前默认图应包含 `SpatialIndex`、`CameraFollow`、`Animation` 三个无边节点。
- 给教学材料展示 ECS 并行波次。

## rpg_importer

用途：离线把 RPGMaker 风格 JSON 转成项目内部 `assets/data/rpg` 格式。

```bash
python3 tools/rpg_importer/import_rpgmaker.py \
  --input-dir for_agent/ref/data \
  --output-dir assets/data/rpg
```

仅预览：

```bash
python3 tools/rpg_importer/import_rpgmaker.py --dry-run
```

可选 ID 别名：

```bash
python3 tools/rpg_importer/import_rpgmaker.py \
  --id-alias-file tools/rpg_importer/id_aliases.json
```

产物：

- `assets/data/rpg/*.json`
- `assets/data/rpg/import_report.json`
- `assets/data/rpg/validation_report.json`

注意：这是离线导入工具，不进入游戏运行时路径。导入后仍应检查 catalog 测试和战斗 smoke test。

## 与自动化测试的关系

| 工具 | 自动化对应 |
|------|------------|
| visual_tester | `tests/engine/render/*`、`tests/engine/vfx/*` |
| rmlui_tester | `tests/engine/ui/*`、`tests/game/rmlui_architecture_regression_test.cpp`、UI checklist |
| battle_tester | `tests/game/battle/*`、`tests/game/battle/battle_scene_smoke_test.cpp` |
| scheduler_dot_dump | `tests/game/system_scheduler_*`、`tests/engine/system/parallel_wave_scheduler_test.cpp` |
| rpg_importer | `tests/game/rpg_catalog_test.cpp`、`tests/game/rpg_assets_catalog_test.cpp` |

如果是逻辑规则，优先补 GoogleTest；如果是视觉或手感，使用工具做人工确认并在回归清单记录结果。
