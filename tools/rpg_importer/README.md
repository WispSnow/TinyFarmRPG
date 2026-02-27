# RPG Importer

`tools/rpg_importer/import_rpgmaker.py` 是独立导入工具，用于把 `for_agent/ref/data` 的 RPGMaker JSON 映射到项目内部格式 `assets/data/rpg`。

## 用法

```bash
python3 tools/rpg_importer/import_rpgmaker.py \
  --input-dir for_agent/ref/data \
  --output-dir assets/data/rpg
```

可选：通过 ID 别名文件生成更易读的语义 ID（默认读取 `tools/rpg_importer/id_aliases.json`）：

```bash
python3 tools/rpg_importer/import_rpgmaker.py \
  --id-alias-file tools/rpg_importer/id_aliases.json
```

仅预览（不写文件）：

```bash
python3 tools/rpg_importer/import_rpgmaker.py --dry-run
```

## 产物

导入后会生成：

1. `assets/data/rpg/*.json`（主数据）
2. `assets/data/rpg/import_report.json`（导入统计与警告）
3. `assets/data/rpg/validation_report.json`（基础引用校验）

## 说明

1. 本工具是一次性/离线导入，不进入游戏运行时路径。
2. 语义 ID 会以 `table.slug_numericId` 形式生成（例如 `skill.attack_1`）。
3. 若 `name` 无法 slug 化（例如中文），可通过 `id_aliases.json` 覆盖为项目语义 ID（如 `skill.attack`）。
4. 占位名称（如 `-----保留`）会被跳过。
