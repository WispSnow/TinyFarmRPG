#!/usr/bin/env python3
"""Build TinyFarmRPG release asset manifests.

The audit intentionally mirrors the current C++ runtime bootstrap where it can:
blueprint JSON, icon catalogs, appearance preload rules, world/map tilesets,
RmlUi documents, scripts, config, audio cues, battle backgrounds, and VFX
catalog entries.
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


RESOURCE_ROOTS = ("assets", "ui", "scripts", "config")
EXCLUDED_RESOURCE_PATHS = {
    "config/user_settings.json",
}
NON_RUNTIME_RESOURCE_ALLOWLIST = {
    "assets/data/dialogue_script_readme.md",
    "assets/data/light_config_readme.md",
    "assets/farm-rpg/CHANGELOG.txt",
    "assets/farm-rpg/Character and Portrait/Character/PNG/Important notice.txt",
    "assets/farm-rpg/Documentation.txt",
    "assets/farm-rpg/Paletta.txt",
    "assets/maps/.DS_Store",
    "assets/maps/farm-rpg.tiled-project",
    "assets/maps/farm-rpg.tiled-session",
    "assets/maps/tileset/.DS_Store",
}
TEXT_EXTENSIONS = {
    ".cpp",
    ".h",
    ".hpp",
    ".json",
    ".lua",
    ".md",
    ".rcss",
    ".rml",
    ".tmj",
    ".tsj",
    ".txt",
    ".world",
}
RESOURCE_EXTENSIONS = {
    ".efkefc",
    ".efk",
    ".efkmat",
    ".efkmodel",
    ".fbx",
    ".frag",
    ".gif",
    ".json",
    ".lua",
    ".mp3",
    ".ogg",
    ".png",
    ".rcss",
    ".rml",
    ".tmj",
    ".tsj",
    ".ttf",
    ".vert",
    ".wav",
    ".world",
}
PATH_TOKEN_RE = re.compile(
    r"(?P<path>(?:(?:assets|ui|scripts|config)/|\.\.?/)[^\"'<>;\n\r]+?"
    r"\.(?:efkefc|efkmodel|efkmat|efk|fbx|frag|gif|json|lua|mp3|ogg|png|rcss|rml|tmj|tsj|ttf|vert|wav|world))"
)


@dataclass
class AuditState:
    root: Path
    used: dict[str, set[str]] = field(default_factory=lambda: collections.defaultdict(set))
    missing: dict[str, set[str]] = field(default_factory=lambda: collections.defaultdict(set))
    queue: collections.deque[Path] = field(default_factory=collections.deque)
    scanned: set[str] = field(default_factory=set)

    def project_path(self, path: Path) -> str:
        return path.resolve().relative_to(self.root.resolve()).as_posix()

    def add(self, path: Path | str, reason: str, anchor: Path | None = None) -> None:
        resolved = self.resolve(path, anchor)
        if resolved is None:
            return

        rel = self.project_path(resolved)
        if rel in EXCLUDED_RESOURCE_PATHS:
            return
        if resolved.is_file():
            first_seen = rel not in self.used
            self.used[rel].add(reason)
            if first_seen and resolved.suffix.lower() in TEXT_EXTENSIONS:
                self.queue.append(resolved)
            return

        if resolved.exists() and resolved.is_dir():
            return

        self.missing[rel].add(reason)

    def add_tree(self, path: Path | str, reason: str, suffixes: set[str] | None = None) -> None:
        resolved = self.resolve(path, None)
        if resolved is None or not resolved.exists() or not resolved.is_dir():
            return
        for entry in sorted(resolved.rglob("*")):
            if entry.is_file() and (suffixes is None or entry.suffix.lower() in suffixes):
                self.add(entry, reason)

    def resolve(self, value: Path | str, anchor: Path | None) -> Path | None:
        path = Path(os.fspath(value))
        if path.is_absolute():
            candidate = path
        elif path.parts and path.parts[0] in RESOURCE_ROOTS:
            candidate = self.root / path
        elif anchor is not None:
            candidate = anchor.parent / path
        else:
            candidate = self.root / path
        try:
            return candidate.resolve()
        except OSError:
            return None


def load_json(path: Path) -> Any | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def walk_json(value: Any) -> Iterable[Any]:
    yield value
    if isinstance(value, dict):
        for nested in value.values():
            yield from walk_json(nested)
    elif isinstance(value, list):
        for nested in value:
            yield from walk_json(nested)


def strings_in_json(value: Any) -> Iterable[str]:
    for nested in walk_json(value):
        if isinstance(nested, str):
            yield nested


def normalize_json_path(value: str) -> str:
    return value.replace("\\/", "/").strip()


def path_has_resource_extension(value: str) -> bool:
    return Path(value).suffix.lower() in RESOURCE_EXTENSIONS


def scan_text_references(state: AuditState, path: Path) -> None:
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return
    for match in PATH_TOKEN_RE.finditer(text):
        token = normalize_json_path(match.group("path"))
        resolved = state.resolve(token, path)
        if resolved is not None and resolved.is_file():
            state.add(resolved, f"text-ref:{state.project_path(path)}")


def scan_json_references(state: AuditState, path: Path) -> Any | None:
    data = load_json(path)
    if data is None:
        return None

    for value in strings_in_json(data):
        token = normalize_json_path(value)
        has_root_prefix = token.startswith(tuple(f"{root}/" for root in RESOURCE_ROOTS))
        if has_root_prefix and not path_has_resource_extension(token):
            resolved = state.resolve(token, path)
            if resolved is None or not resolved.exists():
                continue
        if has_root_prefix or path_has_resource_extension(token):
            state.add(token, f"json-ref:{state.project_path(path)}", anchor=path)

    return data


def scan_rml_links(state: AuditState, path: Path) -> None:
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return
    for href in re.findall(r"\bhref\s*=\s*[\"']([^\"']+)[\"']", text):
        if path_has_resource_extension(href):
            state.add(href, f"rml-link:{state.project_path(path)}", anchor=path)


def scan_rcss_links(state: AuditState, path: Path) -> None:
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return
    for token in re.findall(r"(?:url\(|src\s*:)\s*[\"']?([^\"')};]+)", text):
        cleaned = token.strip().strip("\"'")
        if path_has_resource_extension(cleaned):
            state.add(cleaned, f"rcss-link:{state.project_path(path)}", anchor=path)


def scan_world_and_map_references(state: AuditState, path: Path, data: Any | None) -> None:
    if data is None:
        return
    if path.suffix == ".world":
        for nested in walk_json(data):
            if isinstance(nested, dict) and isinstance(nested.get("fileName"), str):
                state.add(nested["fileName"], f"world-map:{state.project_path(path)}", anchor=path)
        return

    if path.suffix != ".tmj":
        return

    for nested in walk_json(data):
        if not isinstance(nested, dict):
            continue
        source = nested.get("source")
        if isinstance(source, str):
            state.add(source, f"map-tileset:{state.project_path(path)}", anchor=path)
        image = nested.get("image")
        if isinstance(image, str):
            state.add(image, f"map-image:{state.project_path(path)}", anchor=path)
        if nested.get("name") == "target_map" and isinstance(nested.get("value"), str):
            state.add(f"assets/maps/{nested['value']}.tmj", f"map-trigger:{state.project_path(path)}")


def scan_tileset_references(state: AuditState, path: Path, data: Any | None) -> None:
    if path.suffix != ".tsj" or data is None:
        return
    for nested in walk_json(data):
        if isinstance(nested, dict):
            image = nested.get("image")
            if isinstance(image, str):
                state.add(image, f"tileset-image:{state.project_path(path)}", anchor=path)


def scan_battle_backgrounds(state: AuditState, path: Path, data: Any | None) -> None:
    if data is None:
        return
    for nested in walk_json(data):
        if isinstance(nested, dict) and isinstance(nested.get("battle_background_id"), str):
            bg_id = nested["battle_background_id"]
            state.add(f"assets/textures/BattleBg/battlebacks1/{bg_id}.png", f"battle-bg:{state.project_path(path)}")
            state.add(f"assets/textures/BattleBg/battlebacks2/{bg_id}.png", f"battle-bg:{state.project_path(path)}")


def first_png_in(path: Path) -> Path | None:
    if path.is_file() and path.suffix.lower() == ".png":
        try:
            for entry in path.parent.iterdir():
                if entry.is_file() and entry.name == path.name:
                    return entry
            for entry in path.parent.iterdir():
                if entry.is_file() and entry.name.lower() == path.name.lower():
                    return entry
        except OSError:
            return path
        return path
    if not path.is_dir():
        return None
    matches = sorted(entry for entry in path.iterdir() if entry.is_file() and entry.suffix.lower() == ".png")
    return matches[0] if matches else None


def alias_candidates(catalog: dict[str, Any], scope: str, slot: str, variant: str) -> list[str]:
    aliases: list[str] = []
    value = catalog.get("variant_path_aliases", {}).get(scope, {}).get(slot, {}).get(variant)
    if isinstance(value, str):
        aliases.append(value)
    elif isinstance(value, list):
        aliases.extend(item for item in value if isinstance(item, str))
    if variant not in aliases:
        aliases.append(variant)
    return aliases


def resolve_variant_path(base: Path, catalog: dict[str, Any], scope: str, slot: str, variant: str) -> Path | None:
    if not variant or variant == "none":
        return None
    for candidate in alias_candidates(catalog, scope, slot, variant):
        candidate_path = Path(candidate)
        checks = [base / candidate_path]
        if candidate_path.suffix == "":
            checks.append(base / f"{candidate}.png")
        for check in checks:
            resolved = first_png_in(check)
            if resolved is not None:
                return resolved
    return None


def collect_appearance_assets(state: AuditState) -> None:
    catalog_path = state.root / "assets/data/appearance_catalog.json"
    catalog = load_json(catalog_path)
    if not isinstance(catalog, dict):
        return

    texture_root = state.root / catalog.get("texture_root", "")
    portrait_root = state.root / catalog.get("portrait_texture_root", "")
    action_dirs = catalog.get("action_dirs", {})
    action_layouts = catalog.get("action_layouts", {})
    slot_dirs = catalog.get("slot_dirs", {})
    layer_order = catalog.get("layer_order", [])
    runtime_slots = set(catalog.get("runtime_switchable_slots", []))
    variants_by_slot = catalog.get("slot_variants", {})
    weapon_action_variants = catalog.get("weapon_action_variants", {})
    default_profile_id = catalog.get("default_profile", "")
    profile = catalog.get("profiles", {}).get(default_profile_id, {})
    gender = "female" if profile.get("gender") == "female" else "male"
    gender_variants = [item for item in catalog.get("gender_variants", []) if item in {"male", "female"}]
    if not gender_variants:
        gender_variants = [gender]
    profile_slots = profile.get("slots", {})

    for action_key, action_dir in sorted(action_dirs.items()):
        if action_key not in action_layouts:
            continue
        action_path = texture_root / action_dir
        for slot in layer_order:
            slot_dir = slot_dirs.get(slot)
            if not isinstance(slot_dir, str):
                continue

            runtime_candidates: list[str] = []
            default_variant = profile_slots.get(slot, "none")
            if slot == "weapon" and default_variant == "auto":
                default_variant = weapon_action_variants.get(action_key, "none")
            if default_variant != "none":
                runtime_candidates.append(default_variant)
            runtime_candidates.extend(
                variant for variant in variants_by_slot.get(slot, []) if isinstance(variant, str) and variant != "none"
            )

            preload_candidates: set[str] = set()
            if default_variant != "none":
                preload_candidates.add(default_variant)
            if slot in runtime_slots:
                preload_candidates.update(
                    variant
                    for variant in variants_by_slot.get(slot, [])[:3]
                    if isinstance(variant, str) and variant != "none"
                )

            for current_gender in gender_variants:
                base = action_path / slot_dir
                if slot == "eyes":
                    base = base / ("Female" if current_gender == "female" else "Male")
                if not base.is_dir():
                    continue

                for variant in dict.fromkeys(runtime_candidates):
                    resolved = resolve_variant_path(base, catalog, "character", slot, variant)
                    if resolved is not None:
                        state.add(resolved, "appearance-runtime")
                        if current_gender == gender and variant in preload_candidates:
                            state.add(resolved, "appearance-preload")

    state.add_tree(portrait_root, "appearance-portrait-runtime", {".png"})
    collect_default_portrait_assets(state, catalog, portrait_root, profile_slots, gender)


def collect_default_portrait_assets(
    state: AuditState,
    catalog: dict[str, Any],
    portrait_root: Path,
    profile_slots: dict[str, Any],
    gender: str,
) -> None:
    if not portrait_root.is_dir():
        return
    skin = str(profile_slots.get("skin", "1"))
    acc = str(profile_slots.get("acc", "none"))
    for layer in catalog.get("portrait_layer_order", []):
        slot = str(layer)
        variant = ""
        base = portrait_root
        if slot == "skin":
            base = base / "Skins" / ("Female" if gender == "female" else "Male")
            variant = skin
        elif slot == "ears":
            base = base / "Skins" / "Ears"
            variant = acc if acc.startswith("Elf/") else f"Human/{skin}"
            variant_path = Path(variant)
            if variant_path.parent != Path("."):
                base = base / variant_path.parent
                variant = variant_path.name
            else:
                base = base / "Human"
        elif slot == "clothes":
            base = base / "Clothers" / ("Female" if gender == "female" else "Male")
            variant = Path(str(profile_slots.get("clothes", ""))).name
        elif slot == "eyes":
            base = base / "Eyes"
            variant = str(profile_slots.get("eyes", ""))
        elif slot == "hair":
            base = base / "Hair"
            variant = str(profile_slots.get("hair", ""))
        elif slot == "acc":
            if acc == "none" or acc.startswith("Elf/"):
                continue
            base = base / "Acc"
            variant = acc
        else:
            continue

        resolved = resolve_variant_path(base, catalog, "portrait", slot, variant)
        if resolved is not None:
            state.add(resolved, "appearance-portrait-default")


def collect_vfx_dependencies(state: AuditState) -> None:
    vfx_catalog_path = state.root / "assets/data/vfx_catalog.json"
    data = load_json(vfx_catalog_path)
    if not isinstance(data, dict):
        return
    referenced: list[Path] = []
    for value in strings_in_json(data):
        if value.endswith((".efk", ".efkefc")):
            path = state.root / value
            state.add(path, "vfx-catalog")
            referenced.append(path)

    for effect_path in referenced:
        parent = effect_path.parent
        for dirname in ("Texture", "Model", "Material"):
            state.add_tree(parent / dirname, f"vfx-dependency:{state.project_path(effect_path)}")


def collect_seed_assets(state: AuditState) -> None:
    for subdir in ("config", "scripts"):
        state.add_tree(subdir, f"{subdir}-root")

    state.add_tree("assets/data", "data-root", {".json"})
    state.add_tree("assets/i18n", "i18n-root", {".json"})
    state.add_tree("assets/shaders", "desktop-shader-root", {".frag", ".vert"})

    state.add("assets/maps/farm-rpg.world", "content-manifest")
    for map_path in sorted((state.root / "assets/maps").glob("*.tmj")):
        state.add(map_path, "release-map")

    for ui_dir in ("hud", "overlay", "scenes", "theme"):
        state.add_tree(f"ui/rmlui/{ui_dir}", f"production-ui:{ui_dir}", {".rml", ".rcss"})

    for source_dir in ("src/engine", "src/game", "src/main.cpp"):
        source_path = state.root / source_dir
        if source_path.is_file():
            scan_text_references(state, source_path)
        elif source_path.is_dir():
            for path in sorted(source_path.rglob("*")):
                if path.is_file() and path.suffix.lower() in {".cpp", ".h", ".hpp"}:
                    scan_text_references(state, path)

    collect_appearance_assets(state)
    collect_vfx_dependencies(state)


def process_queue(state: AuditState) -> None:
    while state.queue:
        path = state.queue.popleft()
        rel = state.project_path(path)
        if rel in state.scanned:
            continue
        state.scanned.add(rel)

        data = None
        if path.suffix.lower() in {".json", ".tmj", ".tsj", ".world"}:
            data = scan_json_references(state, path)
            scan_world_and_map_references(state, path, data)
            scan_tileset_references(state, path, data)
            scan_battle_backgrounds(state, path, data)

        if path.suffix.lower() == ".rml":
            scan_rml_links(state, path)
        if path.suffix.lower() == ".rcss":
            scan_rcss_links(state, path)

        scan_text_references(state, path)


def all_resource_files(root: Path) -> list[str]:
    files: list[str] = []
    for resource_root in RESOURCE_ROOTS:
        base = root / resource_root
        if not base.exists():
            continue
        for entry in sorted(base.rglob("*")):
            if entry.is_file():
                files.append(entry.relative_to(root).as_posix())
    return files


def png_dimensions(path: Path) -> tuple[int, int] | None:
    try:
        with path.open("rb") as handle:
            header = handle.read(24)
    except OSError:
        return None
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", header[16:24])
    return int(width), int(height)


def summarize(paths: Iterable[str], root: Path) -> dict[str, Any]:
    path_list = list(paths)
    total_bytes = 0
    by_root: dict[str, dict[str, int]] = collections.defaultdict(lambda: {"files": 0, "bytes": 0})
    by_ext: dict[str, dict[str, int]] = collections.defaultdict(lambda: {"files": 0, "bytes": 0})
    texture_count = 0
    max_texture = {"path": "", "width": 0, "height": 0, "pixels": 0}
    texture_pixels = 0

    for rel in path_list:
        path = root / rel
        if not path.is_file():
            continue
        size = path.stat().st_size
        total_bytes += size
        top = rel.split("/", 1)[0]
        ext = path.suffix.lower() or "[none]"
        by_root[top]["files"] += 1
        by_root[top]["bytes"] += size
        by_ext[ext]["files"] += 1
        by_ext[ext]["bytes"] += size

        if ext == ".png":
            dims = png_dimensions(path)
            if dims is not None:
                width, height = dims
                pixels = width * height
                texture_count += 1
                texture_pixels += pixels
                if pixels > max_texture["pixels"]:
                    max_texture = {"path": rel, "width": width, "height": height, "pixels": pixels}

    return {
        "files": len(path_list),
        "bytes": total_bytes,
        "by_root": dict(sorted(by_root.items())),
        "by_extension": dict(sorted(by_ext.items())),
        "texture_count": texture_count,
        "texture_pixels": texture_pixels,
        "estimated_rgba8_bytes": texture_pixels * 4,
        "max_texture": max_texture,
    }


def select_web_poc_assets(used: dict[str, set[str]], root: Path) -> list[str]:
    selected: set[str] = set()
    allowed_prefixes = (
        "config/",
        "scripts/",
        "assets/data/",
        "assets/i18n/",
        "assets/fonts/",
        "assets/shaders/",
        "ui/rmlui/hud/",
        "ui/rmlui/overlay/",
        "ui/rmlui/scenes/appearance_customize.",
        "ui/rmlui/scenes/inventory_menu.",
        "ui/rmlui/scenes/pause_menu.",
        "ui/rmlui/scenes/save_slot_select.",
        "ui/rmlui/scenes/title.",
        "ui/rmlui/scenes/title_widgets.",
        "ui/rmlui/theme/",
    )
    excluded_prefixes = (
        "assets/vfx/",
        "assets/textures/BattleBg/",
        "assets/maps/town.tmj",
        "assets/maps/school.tmj",
        "assets/textures/school-",
    )
    selected_audio = {
        "assets/audio/01_spring_journey.ogg",
        "assets/audio/02_spring_fairy_tale.ogg",
        "assets/audio/Fantasy_UI (1).wav",
        "assets/audio/Fantasy_UI (10).wav",
        "assets/audio/pop.mp3",
    }

    for rel, reasons in used.items():
        if rel in selected_audio:
            selected.add(rel)
            continue
        if rel.startswith(excluded_prefixes):
            continue
        if rel.startswith(allowed_prefixes):
            selected.add(rel)
            continue
        if rel.startswith("assets/maps/"):
            if rel.endswith(("farm-rpg.world", "home_exterior.tmj", "home_interior.tmj", ".tsj")):
                selected.add(rel)
            continue
        if rel.startswith("assets/farm-rpg/") or rel.startswith("assets/textures/"):
            if any(
                reason.startswith(("tileset-image", "map-image", "json-ref", "rcss-link"))
                or reason in {"appearance-preload", "appearance-portrait-default"}
                for reason in reasons
            ):
                selected.add(rel)

    return sorted(path for path in selected if (root / path).is_file())


FULL_RPG_SCENE_PREFIXES = (
    "ui/rmlui/scenes/battle.",
    "ui/rmlui/scenes/dialogue_choice.",
    "ui/rmlui/scenes/quest_offer.",
    "ui/rmlui/scenes/recruit_offer.",
    "ui/rmlui/scenes/rest_dialog.",
    "ui/rmlui/scenes/shop_menu.",
)

FULL_RPG_REQUIRED_PATHS = {
    "assets/maps/school.tmj",
    "assets/maps/town.tmj",
    "assets/textures/school-bg.png",
    "assets/textures/school-fg.png",
    "ui/rmlui/scenes/battle.rml",
    "ui/rmlui/scenes/battle.rcss",
    "ui/rmlui/scenes/dialogue_choice.rml",
    "ui/rmlui/scenes/dialogue_choice.rcss",
    "ui/rmlui/scenes/quest_offer.rml",
    "ui/rmlui/scenes/quest_offer.rcss",
    "ui/rmlui/scenes/recruit_offer.rml",
    "ui/rmlui/scenes/recruit_offer.rcss",
    "ui/rmlui/scenes/rest_dialog.rml",
    "ui/rmlui/scenes/rest_dialog.rcss",
    "ui/rmlui/scenes/shop_menu.rml",
    "ui/rmlui/scenes/shop_menu.rcss",
}

WEB_FULL_APPEARANCE_REASONS = {
    "appearance-runtime",
    "appearance-portrait-runtime",
}


def select_web_full_rpg_assets(used: dict[str, set[str]], root: Path) -> list[str]:
    return sorted(
        rel
        for rel in used
        if (root / rel).is_file() and rel not in NON_RUNTIME_RESOURCE_ALLOWLIST
    )


def format_size(num_bytes: int) -> str:
    units = ("B", "KiB", "MiB", "GiB")
    value = float(num_bytes)
    for unit in units:
        if value < 1024.0 or unit == units[-1]:
            return f"{value:.1f} {unit}" if unit != "B" else f"{int(value)} B"
        value /= 1024.0
    return f"{num_bytes} B"


def write_lines(path: Path, lines: Iterable[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_preload_args(path: Path, paths: Iterable[str]) -> None:
    lines = [f"--preload-file {rel}@/{rel}" for rel in paths]
    write_lines(path, lines)


def write_budget(
    path: Path,
    root: Path,
    used: list[str],
    orphan: list[str],
    web_poc: list[str],
    web_release_full: list[str],
) -> None:
    budget = {
        "used_assets": summarize(used, root),
        "orphan_assets": summarize(orphan, root),
        "web_poc_assets": summarize(web_poc, root),
        "web_release_full_assets": summarize(web_release_full, root),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(budget, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_report(
    path: Path,
    root: Path,
    used: list[str],
    orphan: list[str],
    web_poc: list[str],
    web_release_full: list[str],
    state: AuditState,
) -> None:
    used_summary = summarize(used, root)
    orphan_summary = summarize(orphan, root)
    poc_summary = summarize(web_poc, root)
    full_summary = summarize(web_release_full, root)

    def row(name: str, summary: dict[str, Any]) -> str:
        return (
            f"| {name} | {summary['files']} | {format_size(summary['bytes'])} | "
            f"{summary['texture_count']} | {format_size(summary['estimated_rgba8_bytes'])} |"
        )

    largest_used = sorted(
        ((rel, (root / rel).stat().st_size) for rel in used if (root / rel).is_file()),
        key=lambda item: item[1],
        reverse=True,
    )[:12]

    lines = [
        "# Web Release Phase 1 Asset Audit",
        "",
        "Generated by `tools/asset_audit/audit_assets.py`.",
        "",
        "## Manifest Summary",
        "",
        "| Manifest | Files | Disk Size | PNG Textures | Estimated RGBA8 Memory |",
        "|---|---:|---:|---:|---:|",
        row("used-assets", used_summary),
        row("orphan-assets", orphan_summary),
        row("web-poc-assets", poc_summary),
        row("web-release-full-assets", full_summary),
        "",
        "## Largest Used Assets",
        "",
    ]
    lines.extend(f"- `{rel}`: {format_size(size)}" for rel, size in largest_used)
    lines.extend(
        [
            "",
            "## Texture Budget Notes",
            "",
            f"- Used max texture: `{used_summary['max_texture']['path']}` "
            f"({used_summary['max_texture']['width']}x{used_summary['max_texture']['height']}).",
            f"- Web POC max texture: `{poc_summary['max_texture']['path']}` "
            f"({poc_summary['max_texture']['width']}x{poc_summary['max_texture']['height']}).",
            f"- Web full RPG max texture: `{full_summary['max_texture']['path']}` "
            f"({full_summary['max_texture']['width']}x{full_summary['max_texture']['height']}).",
            "- PNG disk size is not GPU memory size; RGBA8 memory is estimated as width * height * 4.",
            "",
            "## Reproducibility",
            "",
            "```bash",
            "python3 tools/asset_audit/audit_assets.py",
            "```",
            "",
            "Outputs:",
            "",
            "- `manifests/assets/used-assets.txt`",
            "- `manifests/assets/orphan-assets.txt`",
            "- `manifests/assets/web-poc-assets.txt`",
            "- `manifests/assets/web-release-full-assets.txt`",
            "- `manifests/assets/web-poc-preload.args`",
            "- `manifests/assets/web-release-full.args`",
            "- `manifests/assets/asset-budget.json`",
            "",
        ]
    )

    if state.missing:
        lines.extend(["## Missing References", ""])
        for rel, reasons in sorted(state.missing.items()):
            lines.append(f"- `{rel}` via {', '.join(sorted(reasons))}")
        lines.append("")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def run(args: argparse.Namespace) -> int:
    root = Path(args.project_root).resolve()
    state = AuditState(root=root)
    collect_seed_assets(state)
    process_queue(state)

    used = sorted(rel for rel in state.used if (root / rel).is_file() and rel not in EXCLUDED_RESOURCE_PATHS)
    all_files = sorted(rel for rel in all_resource_files(root) if rel not in EXCLUDED_RESOURCE_PATHS)
    orphan = sorted(rel for rel in all_files if rel not in state.used)
    web_poc = select_web_poc_assets(state.used, root)
    web_release_full = select_web_full_rpg_assets(state.used, root)

    manifest_dir = root / args.manifest_dir
    write_lines(manifest_dir / "used-assets.txt", used)
    write_lines(manifest_dir / "orphan-assets.txt", orphan)
    write_lines(manifest_dir / "web-poc-assets.txt", web_poc)
    write_lines(manifest_dir / "web-release-full-assets.txt", web_release_full)
    write_preload_args(manifest_dir / "web-poc-preload.args", web_poc)
    write_preload_args(manifest_dir / "web-release-full.args", web_release_full)
    write_budget(manifest_dir / "asset-budget.json", root, used, orphan, web_poc, web_release_full)
    write_report(root / args.report, root, used, orphan, web_poc, web_release_full, state)

    print(f"used-assets: {len(used)} files")
    print(f"orphan-assets: {len(orphan)} files")
    print(f"web-poc-assets: {len(web_poc)} files")
    print(f"web-release-full-assets: {len(web_release_full)} files")
    if state.missing:
        print(f"missing references: {len(state.missing)}")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", default=".", help="Project root. Defaults to current directory.")
    parser.add_argument("--manifest-dir", default="manifests/assets", help="Manifest output directory.")
    parser.add_argument(
        "--report",
        default="plans/reports/2026-06-02-web-release-phase-1-asset-audit.md",
        help="Markdown report output path.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    raise SystemExit(run(parse_args()))
