#!/usr/bin/env python3
"""Summarize TinyFarmRPG Web runtime package resource coverage."""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


PACKAGE_SCOPE = {
    "boot": "Boot shell: title scene, core config, shaders, cursor, i18n, and title-only UI.",
    "shared-ui": "Runtime UI: RmlUi documents, menu/HUD skins, fonts, icons, portraits, and character UI assets.",
    "rpg-core": "Gameplay catalogs, Lua bootstrap, enemy sprites, quest/shop/VFX data, and RPG-specific i18n.",
    "home-map": "Home gameplay maps, shared world file, tilesets, and home map textures.",
    "town-map": "Town map content, including encounter entry points.",
    "school-map": "School map content loaded after the town-to-school transition.",
    "battle-core": "Battle scene UI and battle background textures.",
    "vfx-core": "Effekseer effects and dependent VFX resources.",
    "audio-core": "Audio cues loaded after the first user gesture.",
}

REQUIRED_GAMEPLAY_SURFACES = {
    "boot": [
        "title scene",
        "appearance catalog",
        "core config",
    ],
    "shared-ui": [
        "appearance customization",
        "HUD and hotbar",
        "inventory/menu/pause/save UI",
        "dialogue and floating notices",
    ],
    "home-map": [
        "home_exterior",
        "home_interior",
        "map transitions",
    ],
    "town-map": [
        "town",
        "battle encounters",
    ],
    "school-map": [
        "school map",
        "school background and foreground textures",
    ],
    "rpg-core": [
        "scripted interactions",
        "item/shop/quest/RPG catalogs",
        "Lua bootstrap and map/NPC scripts",
    ],
    "battle-core": [
        "battle UI",
        "battle backgrounds",
    ],
    "vfx-core": [
        "Effekseer combat effects",
    ],
    "audio-core": [
        "music cues",
        "menu confirmation sounds",
    ],
}

REQUIRED_PATHS = {
    "boot": {
        "ui/rmlui/scenes/title.rml",
        "ui/rmlui/scenes/title_widgets.rcss",
        "assets/data/appearance_catalog.json",
    },
    "shared-ui": {
        "ui/rmlui/scenes/appearance_customize.rml",
        "ui/rmlui/scenes/dialogue_choice.rml",
        "ui/rmlui/scenes/inventory_menu.rcss",
        "ui/rmlui/scenes/inventory_menu.rml",
        "ui/rmlui/scenes/pause_menu.rml",
        "ui/rmlui/scenes/quest_offer.rml",
        "ui/rmlui/scenes/recruit_offer.rml",
        "ui/rmlui/scenes/rest_dialog.rml",
        "ui/rmlui/scenes/save_slot_select.rml",
        "ui/rmlui/scenes/shop_menu.rml",
        "ui/rmlui/tests/05_data_binding.rml",
        "ui/rmlui/hud/hotbar.rml",
        "ui/rmlui/hud/dialogue_box.rml",
    },
    "rpg-core": {
        "assets/data/actor_blueprint.json",
        "assets/data/dialogue_script.json",
        "assets/data/quests.json",
        "assets/data/shops.json",
        "assets/data/vfx_catalog.json",
        "scripts/bootstrap.lua",
        "scripts/maps/home_exterior.lua",
    },
    "home-map": {
        "assets/maps/farm-rpg.world",
        "assets/maps/home_exterior.tmj",
        "assets/maps/home_interior.tmj",
    },
    "town-map": {
        "assets/maps/town.tmj",
    },
    "school-map": {
        "assets/maps/school.tmj",
        "assets/textures/school-bg.png",
        "assets/textures/school-fg.png",
    },
    "battle-core": {
        "assets/textures/BattleBg/battlebacks1/Grassland.png",
        "assets/textures/BattleBg/battlebacks2/Grassland.png",
        "ui/rmlui/scenes/battle.rml",
    },
    "vfx-core": {
        "assets/vfx/effects/HitEffect.efkefc",
    },
    "audio-core": {
        "assets/audio/01_spring_journey.ogg",
        "assets/audio/02_spring_fairy_tale.ogg",
        "assets/audio/pop.mp3",
    },
}


def asset_class(path: str) -> str:
    if path.startswith("assets/maps/"):
        return "map"
    if path.startswith("ui/rmlui/"):
        return "rmlui"
    if path.startswith("assets/audio/"):
        return "audio"
    if path.startswith("scripts/"):
        return "lua"
    if path.startswith("assets/data/"):
        return "data"
    if path.startswith("config/"):
        return "config"
    if path.startswith("assets/fonts/"):
        return "font"
    if path.startswith("assets/shaders/"):
        return "shader"
    if path.endswith((".png", ".jpg", ".jpeg", ".webp", ".aseprite")):
        return "texture"
    return "other"


def load_index(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def summarize(index: dict[str, Any]) -> dict[str, Any]:
    packages = index.get("packages", {})
    summary: dict[str, Any] = {
        "strategy": index.get("strategy"),
        "packages": {},
        "missing_required_paths": {},
    }

    for package_id, package in sorted(packages.items()):
        paths = [str(path) for path in package.get("paths", [])]
        counts = Counter(asset_class(path) for path in paths)
        examples: dict[str, list[str]] = defaultdict(list)
        for path in paths:
            key = asset_class(path)
            if len(examples[key]) < 5:
                examples[key].append(path)

        required = REQUIRED_PATHS.get(package_id, set())
        missing = sorted(required - set(paths))
        if missing:
            summary["missing_required_paths"][package_id] = missing

        summary["packages"][package_id] = {
            "scope": PACKAGE_SCOPE.get(package_id, "Unclassified runtime package."),
            "surfaces": REQUIRED_GAMEPLAY_SURFACES.get(package_id, []),
            "files": package.get("files", len(paths)),
            "bytes": package.get("bytes"),
            "size": package.get("size"),
            "artifact_size": package.get("artifact_size"),
            "asset_classes": dict(sorted(counts.items())),
            "examples": dict(sorted(examples.items())),
            "required_paths": sorted(required),
        }

    return summary


def markdown(summary: dict[str, Any]) -> str:
    lines = [
        "# Web Resource Coverage",
        "",
        f"- Strategy: `{summary.get('strategy', '<missing>')}`",
        f"- Missing required paths: `{json.dumps(summary.get('missing_required_paths', {}), ensure_ascii=False)}`",
        "",
        "| Package | Scope | Files | Size | Asset classes | Gameplay surfaces |",
        "|---|---|---:|---:|---|---|",
    ]

    for package_id, package in summary.get("packages", {}).items():
        classes = ", ".join(f"{name}:{count}" for name, count in package["asset_classes"].items())
        surfaces = ", ".join(package["surfaces"])
        lines.append(
            f"| `{package_id}` | {package['scope']} | {package['files']} | "
            f"{package.get('size') or '<unknown>'} | {classes} | {surfaces} |"
        )

    lines.extend(["", "## Required Paths", ""])
    for package_id, package in summary.get("packages", {}).items():
        lines.append(f"### `{package_id}`")
        for path in package.get("required_paths", []):
            lines.append(f"- `{path}`")
        lines.append("")

    lines.extend(["## Representative Paths", ""])
    for package_id, package in summary.get("packages", {}).items():
        lines.append(f"### `{package_id}`")
        for class_name, paths in package.get("examples", {}).items():
            sample = ", ".join(f"`{path}`" for path in paths)
            lines.append(f"- {class_name}: {sample}")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit Web runtime package resource coverage.")
    parser.add_argument("--package-index", type=Path, required=True)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()

    summary = summarize(load_index(args.package_index.resolve()))
    if args.json_output:
        write_json(args.json_output.resolve(), summary)
    rendered = markdown(summary)
    if args.markdown_output:
        write_text(args.markdown_output.resolve(), rendered)
    if not args.json_output and not args.markdown_output:
        print(rendered, end="")

    if summary.get("missing_required_paths"):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
