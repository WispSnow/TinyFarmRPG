#!/usr/bin/env python3
"""Import RPGMaker reference JSON data into TinyFarm RPG domain JSON files.

Usage:
  python3 tools/rpg_importer/import_rpgmaker.py \
      --input-dir for_agent/ref/data \
      --output-dir assets/data/rpg
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any


PLACEHOLDER_PREFIX = "-----"
PARAM_KEYS = ["mhp", "mmp", "atk", "def", "mat", "mdf", "agi", "luk"]


@dataclass
class TableReport:
    input_slots: int = 0
    imported: int = 0
    skipped: int = 0


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=4, ensure_ascii=False)
        f.write("\n")


def normalize_name(value: Any) -> str:
    if not isinstance(value, str):
        return ""
    return value.strip()


def is_placeholder_name(name: str) -> bool:
    return (not name) or name.startswith(PLACEHOLDER_PREFIX)


def slugify(name: str) -> str:
    lowered = name.lower()
    slug = re.sub(r"[^a-z0-9]+", "_", lowered).strip("_")
    slug = re.sub(r"_+", "_", slug)
    return slug


def make_semantic_id(prefix: str, name: str, numeric_id: int, used: set[str]) -> str:
    base = slugify(name)
    if not base:
        base = f"entry_{numeric_id}"
    candidate = f"{prefix}.{base}_{numeric_id}"
    index = 2
    while candidate in used:
        candidate = f"{prefix}.{base}_{numeric_id}_{index}"
        index += 1
    used.add(candidate)
    return candidate


def load_id_aliases(path: Path, warnings: list[str]) -> dict[str, dict[str, str]]:
    if not path.is_file():
        return {}

    try:
        root = load_json(path)
    except Exception as exc:  # pylint: disable=broad-except
        warnings.append(f"id_aliases: failed to parse '{path}': {exc}")
        return {}

    if not isinstance(root, dict):
        warnings.append(f"id_aliases: root must be object in '{path}'")
        return {}

    aliases: dict[str, dict[str, str]] = {}
    for table_name, table_aliases in root.items():
        if not isinstance(table_aliases, dict):
            continue
        normalized: dict[str, str] = {}
        for source_id, target_id in table_aliases.items():
            if not isinstance(source_id, str):
                continue
            target = normalize_name(target_id)
            if not target:
                continue
            normalized[source_id] = target
        if normalized:
            aliases[table_name] = normalized
    return aliases


def pick_semantic_id(
    *,
    prefix: str,
    name: str,
    numeric_id: int,
    used: set[str],
    aliases: dict[str, str],
    table_name: str,
    warnings: list[str],
) -> str:
    alias = normalize_name(aliases.get(str(numeric_id), ""))
    if not alias:
        return make_semantic_id(prefix, name, numeric_id, used)

    candidate = alias
    if "." not in candidate:
        candidate = f"{prefix}.{candidate}"
    elif not candidate.startswith(f"{prefix}."):
        suffix = candidate.split(".", 1)[1]
        if not suffix:
            suffix = f"id_{numeric_id}"
        warnings.append(
            f"{table_name}: alias '{alias}' for source #{numeric_id} should start with '{prefix}.'; auto-fixed"
        )
        candidate = f"{prefix}.{suffix}"

    candidate = re.sub(r"\s+", "_", candidate)
    deduped = candidate
    index = 2
    while deduped in used:
        deduped = f"{candidate}_{index}"
        index += 1
    used.add(deduped)
    return deduped


def map_scope(scope: int) -> str:
    if scope in (1, 3, 4, 5, 6):
        return "one_enemy"
    if scope == 2:
        return "all_enemies"
    if scope in (7, 9):
        return "one_ally"
    if scope in (8, 10):
        return "all_allies"
    if scope == 11:
        return "self"
    return "none"


def map_hit_type(hit_type: int) -> str:
    if hit_type == 1:
        return "physical"
    if hit_type == 2:
        return "magical"
    return "certain"


def map_damage_type(damage_type: int) -> str:
    if damage_type == 1:
        return "hp_damage"
    if damage_type == 2:
        return "mp_damage"
    if damage_type == 3:
        return "hp_recover"
    if damage_type == 4:
        return "mp_recover"
    if damage_type == 5:
        return "hp_drain"
    if damage_type == 6:
        return "mp_drain"
    return "none"


def map_trait_type(code: int) -> str | None:
    if code == 11:
        return "element_rate"
    if code == 13:
        return "state_rate"
    if code == 14:
        return "state_immunity"
    if code == 21:
        return "param_rate"
    return None


def map_effect_type(code: int) -> str | None:
    if code == 11:
        return "recover_hp"
    if code == 12:
        return "recover_mp"
    if code == 21:
        return "add_state"
    if code == 22:
        return "remove_state"
    return None


def to_int(value: Any, default: int = 0) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        return int(value)
    return default


def to_float(value: Any, default: float = 0.0) -> float:
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)):
        return float(value)
    return default


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, value))


def clampf(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def class_base_params_from_curve(raw_params: Any, class_id: int, warnings: list[str]) -> dict[str, int]:
    values = [0] * len(PARAM_KEYS)
    if not (isinstance(raw_params, list) and len(raw_params) >= len(PARAM_KEYS)):
        warnings.append(f"classes: source class #{class_id} has invalid params curve, fallback to zeros")
        return {key: values[idx] for idx, key in enumerate(PARAM_KEYS)}

    for param_index in range(len(PARAM_KEYS)):
        curve = raw_params[param_index]
        if isinstance(curve, list) and curve:
            # RPGMaker 的 class params 通常按等级存储，索引 1 对应 Lv1。
            level_index = 1 if len(curve) > 1 else 0
            values[param_index] = max(0, to_int(curve[level_index], 0))
            continue
        values[param_index] = max(0, to_int(curve, 0))

    return {key: values[idx] for idx, key in enumerate(PARAM_KEYS)}


def convert_inventory_table(
    entries: list[Any], prefix: str, warnings: list[str], aliases: dict[str, str], table_name: str
) -> tuple[list[dict[str, Any]], dict[int, str], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    id_map: dict[int, str] = {}
    report = TableReport(input_slots=len(entries))

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", ""))
        if is_placeholder_name(name):
            report.skipped += 1
            continue

        semantic_id = pick_semantic_id(
            prefix=prefix,
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name=table_name,
            warnings=warnings,
        )
        id_map[numeric_id] = semantic_id

        payload: dict[str, Any] = {
            "id": semantic_id,
            "display_name": name,
            "description": normalize_name(raw.get("description", "")),
            "price": to_int(raw.get("price", 0), 0),
        }

        params = raw.get("params")
        if isinstance(params, list) and len(params) >= 8:
            payload["params"] = [to_int(params[i], 0) for i in range(8)]
        mapped.append(payload)
        report.imported += 1

    if report.imported == 0:
        warnings.append(f"{prefix}: no rows imported from source")
    return mapped, id_map, report


def convert_states(
    entries: list[Any], warnings: list[str], aliases: dict[str, str]
) -> tuple[list[dict[str, Any]], dict[int, str], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    id_map: dict[int, str] = {}
    report = TableReport(input_slots=len(entries))

    param_targets = {
        0: "mhp",
        1: "mmp",
        2: "atk",
        3: "def",
        4: "mat",
        5: "mdf",
        6: "agi",
        7: "luk",
    }

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", ""))
        if is_placeholder_name(name):
            report.skipped += 1
            continue

        semantic_id = pick_semantic_id(
            prefix="state",
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name="states",
            warnings=warnings,
        )
        id_map[numeric_id] = semantic_id

        min_turns = max(1, to_int(raw.get("minTurns"), 1))
        max_turns = max(min_turns, to_int(raw.get("maxTurns"), min_turns))

        traits: list[dict[str, Any]] = []
        for trait in raw.get("traits", []):
            if not isinstance(trait, dict):
                continue
            trait_code = to_int(trait.get("code"), -1)
            trait_type = map_trait_type(trait_code)
            if trait_type is None:
                continue

            data_id = to_int(trait.get("dataId"), 0)
            target = ""
            if trait_type == "param_rate":
                target = param_targets.get(data_id, "")
            elif trait_type == "element_rate":
                target = f"element_{data_id}"
            else:
                target = str(data_id)

            if not target:
                continue
            traits.append(
                {
                    "type": trait_type,
                    "target": target,
                    "value": to_float(trait.get("value"), 0.0),
                }
            )

        mapped.append(
            {
                "id": semantic_id,
                "display_name": name,
                "priority": to_int(raw.get("priority"), 50),
                "min_turns": min_turns,
                "max_turns": max_turns,
                "traits": traits,
            }
        )
        report.imported += 1

    if report.imported == 0:
        warnings.append("states: no rows imported from source")
    return mapped, id_map, report


def convert_skills(
    entries: list[Any],
    state_id_map: dict[int, str],
    warnings: list[str],
    aliases: dict[str, str],
) -> tuple[list[dict[str, Any]], dict[int, str], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    id_map: dict[int, str] = {}
    report = TableReport(input_slots=len(entries))

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", ""))
        if is_placeholder_name(name):
            report.skipped += 1
            continue

        semantic_id = pick_semantic_id(
            prefix="skill",
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name="skills",
            warnings=warnings,
        )
        id_map[numeric_id] = semantic_id

        damage = raw.get("damage", {})
        if not isinstance(damage, dict):
            damage = {}

        effects: list[dict[str, Any]] = []
        for effect in raw.get("effects", []):
            if not isinstance(effect, dict):
                continue

            effect_type = map_effect_type(to_int(effect.get("code"), -1))
            if effect_type is None:
                continue

            mapped_effect: dict[str, Any] = {
                "type": effect_type,
                "value1": to_float(effect.get("value1"), 0.0),
                "value2": to_float(effect.get("value2"), 0.0),
            }

            data_id = to_int(effect.get("dataId"), 0)
            if effect_type in ("add_state", "remove_state"):
                mapped_state_id = state_id_map.get(data_id)
                if mapped_state_id is None:
                    warnings.append(
                        f"skills: state effect in source skill #{numeric_id} references missing state #{data_id}"
                    )
                    continue
                mapped_effect["target_id"] = mapped_state_id

            effects.append(mapped_effect)

        mapped.append(
            {
                "id": semantic_id,
                "display_name": name,
                "description": normalize_name(raw.get("description", "")),
                "scope": map_scope(to_int(raw.get("scope"), 0)),
                "hit_type": map_hit_type(to_int(raw.get("hitType"), 0)),
                "success_rate": clamp(to_int(raw.get("successRate"), 100), 0, 100),
                "repeats": max(1, to_int(raw.get("repeats"), 1)),
                "damage": {
                    "type": map_damage_type(to_int(damage.get("type"), 0)),
                    "formula": normalize_name(damage.get("formula", "0")),
                    "variance": max(0, to_int(damage.get("variance"), 0)),
                    "critical": bool(damage.get("critical", False)),
                },
                "effects": effects,
            }
        )
        report.imported += 1

    if report.imported == 0:
        warnings.append("skills: no rows imported from source")
    return mapped, id_map, report


def convert_enemies(
    entries: list[Any],
    skill_id_map: dict[int, str],
    item_id_map: dict[int, str],
    weapon_id_map: dict[int, str],
    armor_id_map: dict[int, str],
    warnings: list[str],
    aliases: dict[str, str],
) -> tuple[list[dict[str, Any]], dict[int, str], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    id_map: dict[int, str] = {}
    report = TableReport(input_slots=len(entries))

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", ""))
        if is_placeholder_name(name):
            report.skipped += 1
            continue

        semantic_id = pick_semantic_id(
            prefix="enemy",
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name="enemies",
            warnings=warnings,
        )
        id_map[numeric_id] = semantic_id

        params_raw = raw.get("params", [])
        params: list[int] = []
        if isinstance(params_raw, list):
            params = [to_int(v, 0) for v in params_raw[:8]]
        while len(params) < 8:
            params.append(0)

        drops: list[dict[str, Any]] = []
        for drop in raw.get("dropItems", []):
            if not isinstance(drop, dict):
                continue
            kind = to_int(drop.get("kind"), 0)
            data_id = to_int(drop.get("dataId"), 0)
            denominator = max(0, to_int(drop.get("denominator"), 0))
            if kind == 0 or data_id <= 0:
                continue

            if kind == 1:
                item_id = item_id_map.get(data_id, f"item.rm_{data_id}")
            elif kind == 2:
                item_id = weapon_id_map.get(data_id, f"weapon.rm_{data_id}")
            elif kind == 3:
                item_id = armor_id_map.get(data_id, f"armor.rm_{data_id}")
            else:
                continue

            chance = 0.0 if denominator <= 0 else clampf(1.0 / float(denominator), 0.0, 1.0)
            drops.append({"item_id": item_id, "chance": chance})

        actions: list[dict[str, Any]] = []
        for action in raw.get("actions", []):
            if not isinstance(action, dict):
                continue
            src_skill_id = to_int(action.get("skillId"), 0)
            mapped_skill_id = skill_id_map.get(src_skill_id)
            if mapped_skill_id is None:
                warnings.append(
                    f"enemies: source enemy #{numeric_id} action references missing skill #{src_skill_id}"
                )
                continue
            actions.append(
                {
                    "skill_id": mapped_skill_id,
                    "rating": max(1, to_int(action.get("rating"), 1)),
                }
            )

        mapped.append(
            {
                "id": semantic_id,
                "display_name": name,
                "params": params,
                "exp": max(0, to_int(raw.get("exp"), 0)),
                "gold": max(0, to_int(raw.get("gold"), 0)),
                "drops": drops,
                "actions": actions,
            }
        )
        report.imported += 1

    if report.imported == 0:
        warnings.append("enemies: no rows imported from source")
    return mapped, id_map, report


def convert_troops(
    entries: list[Any], enemy_id_map: dict[int, str], warnings: list[str], aliases: dict[str, str]
) -> tuple[list[dict[str, Any]], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    report = TableReport(input_slots=len(entries))

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", "")) or f"Troop {numeric_id}"

        semantic_id = pick_semantic_id(
            prefix="troop",
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name="troops",
            warnings=warnings,
        )
        members: list[dict[str, Any]] = []
        for member in raw.get("members", []):
            if not isinstance(member, dict):
                continue
            src_enemy_id = to_int(member.get("enemyId"), 0)
            mapped_enemy_id = enemy_id_map.get(src_enemy_id)
            if mapped_enemy_id is None:
                warnings.append(
                    f"troops: source troop #{numeric_id} member references missing enemy #{src_enemy_id}"
                )
                continue
            members.append(
                {
                    "enemy_id": mapped_enemy_id,
                    "x": to_float(member.get("x"), 0.0),
                    "y": to_float(member.get("y"), 0.0),
                }
            )

        if not members:
            report.skipped += 1
            continue

        mapped.append(
            {
                "id": semantic_id,
                "display_name": name,
                "members": members,
            }
        )
        report.imported += 1

    if report.imported == 0:
        warnings.append("troops: no rows imported from source")
    return mapped, report


def convert_classes(
    entries: list[Any], skill_id_map: dict[int, str], warnings: list[str], aliases: dict[str, str]
) -> tuple[list[dict[str, Any]], dict[int, str], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    id_map: dict[int, str] = {}
    report = TableReport(input_slots=len(entries))

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", ""))
        if is_placeholder_name(name):
            report.skipped += 1
            continue

        semantic_id = pick_semantic_id(
            prefix="class",
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name="classes",
            warnings=warnings,
        )
        id_map[numeric_id] = semantic_id
        base_params = class_base_params_from_curve(raw.get("params"), numeric_id, warnings)

        learnings: list[dict[str, Any]] = []
        for learning in raw.get("learnings", []):
            if not isinstance(learning, dict):
                continue
            src_skill_id = to_int(learning.get("skillId"), 0)
            mapped_skill_id = skill_id_map.get(src_skill_id)
            if mapped_skill_id is None:
                continue
            learnings.append(
                {
                    "level": max(1, to_int(learning.get("level"), 1)),
                    "skill_id": mapped_skill_id,
                }
            )

        mapped.append(
            {
                "id": semantic_id,
                "display_name": name,
                "base_params": base_params,
                "learnings": learnings,
            }
        )
        report.imported += 1

    if report.imported == 0:
        warnings.append("classes: no rows imported from source")
    return mapped, id_map, report


def convert_actors(
    entries: list[Any], class_id_map: dict[int, str], warnings: list[str], aliases: dict[str, str]
) -> tuple[list[dict[str, Any]], TableReport]:
    used: set[str] = set()
    mapped: list[dict[str, Any]] = []
    report = TableReport(input_slots=len(entries))

    for idx, raw in enumerate(entries):
        if idx == 0 or not isinstance(raw, dict):
            report.skipped += 1
            continue
        numeric_id = to_int(raw.get("id"), idx)
        name = normalize_name(raw.get("name", ""))
        if is_placeholder_name(name):
            report.skipped += 1
            continue

        semantic_id = pick_semantic_id(
            prefix="actor",
            name=name,
            numeric_id=numeric_id,
            used=used,
            aliases=aliases,
            table_name="actors",
            warnings=warnings,
        )
        class_id = class_id_map.get(to_int(raw.get("classId"), 0), "")
        if not class_id:
            warnings.append(
                f"actors: source actor #{numeric_id} references missing class #{to_int(raw.get('classId'), 0)}"
            )
            report.skipped += 1
            continue

        mapped.append(
            {
                "id": semantic_id,
                "display_name": name,
                "class_id": class_id,
                "initial_level": max(1, to_int(raw.get("initialLevel"), 1)),
                "max_level": max(1, to_int(raw.get("maxLevel"), 99)),
            }
        )
        report.imported += 1

    if report.imported == 0:
        warnings.append("actors: no rows imported from source")
    return mapped, report


def build_validation_report(
    skills: list[dict[str, Any]],
    states: list[dict[str, Any]],
    enemies: list[dict[str, Any]],
    troops: list[dict[str, Any]],
    classes: list[dict[str, Any]],
    actors: list[dict[str, Any]],
) -> dict[str, Any]:
    state_ids = {row["id"] for row in states}
    skill_ids = {row["id"] for row in skills}
    enemy_ids = {row["id"] for row in enemies}
    class_ids = {row["id"] for row in classes}

    issues: list[str] = []

    for skill in skills:
        for effect in skill.get("effects", []):
            if effect.get("type") in ("add_state", "remove_state"):
                target = effect.get("target_id", "")
                if target not in state_ids:
                    issues.append(f"skill '{skill['id']}' references missing state '{target}'")

    for enemy in enemies:
        for action in enemy.get("actions", []):
            target = action.get("skill_id", "")
            if target not in skill_ids:
                issues.append(f"enemy '{enemy['id']}' references missing skill '{target}'")

    for troop in troops:
        for member in troop.get("members", []):
            target = member.get("enemy_id", "")
            if target not in enemy_ids:
                issues.append(f"troop '{troop['id']}' references missing enemy '{target}'")

    for actor in actors:
        target = actor.get("class_id", "")
        if target not in class_ids:
            issues.append(f"actor '{actor['id']}' references missing class '{target}'")

    return {
        "ok": len(issues) == 0,
        "issue_count": len(issues),
        "issues": issues,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Import RPGMaker reference data into TinyFarm RPG schema")
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=Path("for_agent/ref/data"),
        help="RPGMaker JSON directory",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("assets/data/rpg"),
        help="Output RPG domain directory",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Do not write output files, only print report summary",
    )
    parser.add_argument(
        "--id-alias-file",
        type=Path,
        default=Path("tools/rpg_importer/id_aliases.json"),
        help="Optional JSON file for source-id to semantic-id aliases",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    input_dir: Path = args.input_dir
    output_dir: Path = args.output_dir

    required_files = [
        "Actors.json",
        "Classes.json",
        "Skills.json",
        "States.json",
        "Items.json",
        "Weapons.json",
        "Armors.json",
        "Enemies.json",
        "Troops.json",
    ]
    missing = [name for name in required_files if not (input_dir / name).is_file()]
    if missing:
        print(f"[ERROR] Missing required source files under {input_dir}: {', '.join(missing)}")
        return 1

    warnings: list[str] = []
    table_reports: dict[str, dict[str, int]] = {}
    id_aliases = load_id_aliases(args.id_alias_file, warnings)

    items, item_id_map, item_report = convert_inventory_table(
        load_json(input_dir / "Items.json"),
        "item",
        warnings,
        id_aliases.get("items", {}),
        "items",
    )
    weapons, weapon_id_map, weapon_report = convert_inventory_table(
        load_json(input_dir / "Weapons.json"),
        "weapon",
        warnings,
        id_aliases.get("weapons", {}),
        "weapons",
    )
    armors, armor_id_map, armor_report = convert_inventory_table(
        load_json(input_dir / "Armors.json"),
        "armor",
        warnings,
        id_aliases.get("armors", {}),
        "armors",
    )
    states, state_id_map, state_report = convert_states(
        load_json(input_dir / "States.json"),
        warnings,
        id_aliases.get("states", {}),
    )
    skills, skill_id_map, skill_report = convert_skills(
        load_json(input_dir / "Skills.json"),
        state_id_map,
        warnings,
        id_aliases.get("skills", {}),
    )
    enemies, enemy_id_map, enemy_report = convert_enemies(
        load_json(input_dir / "Enemies.json"),
        skill_id_map,
        item_id_map,
        weapon_id_map,
        armor_id_map,
        warnings,
        id_aliases.get("enemies", {}),
    )
    troops, troop_report = convert_troops(
        load_json(input_dir / "Troops.json"),
        enemy_id_map,
        warnings,
        id_aliases.get("troops", {}),
    )
    classes, class_id_map, class_report = convert_classes(
        load_json(input_dir / "Classes.json"),
        skill_id_map,
        warnings,
        id_aliases.get("classes", {}),
    )
    actors, actor_report = convert_actors(
        load_json(input_dir / "Actors.json"),
        class_id_map,
        warnings,
        id_aliases.get("actors", {}),
    )

    for name, report in [
        ("items", item_report),
        ("weapons", weapon_report),
        ("armors", armor_report),
        ("states", state_report),
        ("skills", skill_report),
        ("enemies", enemy_report),
        ("troops", troop_report),
        ("classes", class_report),
        ("actors", actor_report),
    ]:
        table_reports[name] = {
            "input_slots": report.input_slots,
            "imported": report.imported,
            "skipped": report.skipped,
        }

    manifest = {
        "schema_version": 1,
        "content_versions": {
            "actors": 1,
            "classes": 1,
            "items": 1,
            "weapons": 1,
            "armors": 1,
            "skills": 1,
            "states": 1,
            "enemies": 1,
            "troops": 1,
            "quests": 1,
            "shops": 1,
        },
        "features": {
            "quest": False,
            "shop": False,
        },
        "files": {
            "actors": "actors.json",
            "classes": "classes.json",
            "items": "items.json",
            "weapons": "weapons.json",
            "armors": "armors.json",
            "skills": "skills.json",
            "states": "states.json",
            "enemies": "enemies.json",
            "troops": "troops.json",
            "quests": "quests.json",
            "shops": "shops.json",
        },
    }

    validation_report = build_validation_report(skills, states, enemies, troops, classes, actors)
    import_report = {
        "source_dir": str(input_dir),
        "output_dir": str(output_dir),
        "tables": table_reports,
        "warning_count": len(warnings),
        "warnings": warnings,
        "validation_ok": validation_report["ok"],
    }

    print("[INFO] import summary:")
    for table_name, report in table_reports.items():
        print(
            f"  - {table_name}: imported={report['imported']}, skipped={report['skipped']}, "
            f"input_slots={report['input_slots']}"
        )
    print(f"[INFO] warnings: {len(warnings)}")
    print(f"[INFO] validation_ok: {validation_report['ok']}")

    if args.dry_run:
        return 0

    write_json(output_dir / "manifest.json", manifest)
    write_json(output_dir / "actors.json", {"actors": actors})
    write_json(output_dir / "classes.json", {"classes": classes})
    write_json(output_dir / "items.json", {"items": items})
    write_json(output_dir / "weapons.json", {"weapons": weapons})
    write_json(output_dir / "armors.json", {"armors": armors})
    write_json(output_dir / "skills.json", {"skills": skills})
    write_json(output_dir / "states.json", {"states": states})
    write_json(output_dir / "enemies.json", {"enemies": enemies})
    write_json(output_dir / "troops.json", {"troops": troops})
    write_json(output_dir / "quests.json", {"quests": []})
    write_json(output_dir / "shops.json", {"shops": []})
    write_json(output_dir / "import_report.json", import_report)
    write_json(output_dir / "validation_report.json", validation_report)

    print(f"[INFO] wrote files to: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
