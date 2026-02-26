# RPG Importer

`tools/rpg_importer/import_rpgmaker.py` 是独立导入工具，用于把 `for_agent/ref/data` 的 RPGMaker JSON 映射到项目内部格式 `assets/data/rpg`。

## 用法

```bash
python3 tools/rpg_importer/import_rpgmaker.py \
  --input-dir for_agent/ref/data \
  --output-dir assets/data/rpg
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
3. 占位名称（如 `-----保留`）会被跳过。
