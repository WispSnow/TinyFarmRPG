#!/usr/bin/env python3
"""Generate runtime Web asset packages for TinyFarmRPG."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


PACKAGE_MAGIC = b"TFPK0001"
PACKAGE_VERSION = 1
PACKAGE_STRATEGY = "custom_sync_xhr_fs_writefile"

AUDIO_CORE_PATHS = {
    "assets/audio/01_spring_journey.ogg",
    "assets/audio/02_spring_fairy_tale.ogg",
    "assets/audio/pop.mp3",
}

TITLE_BOOT_PATHS = {
    "assets/farm-rpg/UI/button.png",
    "assets/i18n/en-US.json",
    "assets/i18n/languages.json",
    "assets/i18n/zh-Hans.json",
    "assets/textures/UI/farm-rpg-bg.png",
    "assets/textures/UI/farm-rpg-logo.png",
    "ui/rmlui/theme/base.rcss",
    "ui/rmlui/theme/nav.rcss",
    "ui/rmlui/theme/reset.rcss",
    "ui/rmlui/scenes/title.rcss",
    "ui/rmlui/scenes/title.rml",
    "ui/rmlui/scenes/title_widgets.rcss",
}

BOOT_PATHS = {
    "assets/data/appearance_catalog.json",
    "assets/data/cursor_config.json",
    "assets/data/resource_mapping.json",
    "assets/farm-rpg/UI/Clock/Clock.png",
    "assets/farm-rpg/UI/Clock/clock hand.png",
    "assets/fonts/VonwaonBitmap-16px.ttf",
    "assets/shaders/blur.frag",
    "assets/shaders/color.frag",
    "assets/shaders/composite.frag",
    "assets/shaders/composite.vert",
    "assets/shaders/debug.frag",
    "assets/shaders/debug.vert",
    "assets/shaders/emissive.frag",
    "assets/shaders/emissive.vert",
    "assets/shaders/light.frag",
    "assets/shaders/light.vert",
    "assets/shaders/quad.vert",
    "assets/shaders/quad_uv.vert",
    "assets/shaders/texture.frag",
    "assets/textures/UI/circle.png",
    "config/audio.json",
    "config/input.json",
    "config/render.json",
    "config/text_render.json",
    "config/user_settings.default.json",
    "config/window.json",
    *TITLE_BOOT_PATHS,
}

GAMEPLAY_DATA_PREFIXES = (
    "assets/data/rpg/",
)

GAMEPLAY_DATA_PATHS = {
    "assets/data/actor_blueprint.json",
    "assets/data/animal_blueprint.json",
    "assets/data/audio_cues.json",
    "assets/data/crop_config.json",
    "assets/data/dialogue_script.json",
    "assets/data/game_time_config.json",
    "assets/data/icon_config.json",
    "assets/data/item_config.json",
    "assets/data/light_config.json",
    "assets/data/map_loading_config.json",
    "assets/data/quests.json",
    "assets/data/shops.json",
    "assets/data/vfx_catalog.json",
}

BATTLE_UI_PREFIXES = (
    "ui/rmlui/scenes/battle.",
)

SHARED_UI_PREFIXES = (
    "ui/rmlui/hud/",
    "ui/rmlui/overlay/",
    "ui/rmlui/scenes/appearance_customize.",
    "ui/rmlui/scenes/dialogue_choice.",
    "ui/rmlui/scenes/inventory_menu.",
    "ui/rmlui/scenes/pause_menu.",
    "ui/rmlui/scenes/quest_offer.",
    "ui/rmlui/scenes/recruit_offer.",
    "ui/rmlui/scenes/rest_dialog.",
    "ui/rmlui/scenes/save_slot_select.",
    "ui/rmlui/scenes/shop_menu.",
    "ui/rmlui/theme/",
)

PACKAGE_DEPENDENCIES = {
    "boot": [],
    "shared-ui": [],
    "rpg-core": [],
    "home-map": ["rpg-core"],
    "town-map": ["home-map"],
    "school-map": ["town-map"],
    "battle-core": ["shared-ui", "rpg-core", "town-map"],
    "vfx-core": ["battle-core"],
    "audio-core": [],
}


@dataclass(frozen=True)
class PreloadEntry:
    source_rel: str
    source_path: Path
    mount_path: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def human_size(value: int) -> str:
    size = float(value)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if size < 1024.0 or unit == "GiB":
            return f"{size:.1f} {unit}" if unit != "B" else f"{value} B"
        size /= 1024.0
    return f"{value} B"


def gzip_size(data: bytes) -> int:
    return len(gzip.compress(data, compresslevel=9, mtime=0))


def brotli_size(data: bytes) -> int | None:
    try:
        import brotli  # type: ignore[import-not-found]
    except ImportError:
        return None
    return len(brotli.compress(data, quality=11))


def parse_preload_manifest(root: Path, manifest_path: Path) -> list[PreloadEntry]:
    entries: list[PreloadEntry] = []
    for line_number, raw_line in enumerate(manifest_path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("--preload-file="):
            payload = line[len("--preload-file=") :]
        elif line.startswith("--preload-file "):
            payload = line[len("--preload-file ") :]
        else:
            raise ValueError(f"{manifest_path}:{line_number} is not a --preload-file entry")

        if "@" in payload:
            source_text, mount_path = payload.split("@", 1)
        else:
            source_text = payload
            mount_path = f"/{source_text}"

        source_path = Path(source_text)
        resolved_source = source_path if source_path.is_absolute() else root / source_path
        resolved_source = resolved_source.resolve()
        if source_path.is_absolute():
            source_rel = resolved_source.relative_to(root).as_posix()
        else:
            source_rel = source_text

        if not mount_path.startswith("/"):
            raise ValueError(f"{manifest_path}:{line_number} mount path must be absolute: {mount_path}")
        if not resolved_source.is_file():
            raise ValueError(f"{manifest_path}:{line_number} preload source missing or not a file: {source_rel}")

        entries.append(PreloadEntry(source_rel=source_rel, source_path=resolved_source, mount_path=mount_path))

    if not entries:
        raise ValueError(f"Preload manifest is empty: {manifest_path}")
    return entries


def classify(entry: PreloadEntry) -> str:
    path = entry.source_rel
    if path in BOOT_PATHS:
        return "boot"
    if path in AUDIO_CORE_PATHS or path.startswith("assets/audio/"):
        return "audio-core"
    if path.startswith("assets/vfx/"):
        return "vfx-core"
    if path.startswith("assets/textures/BattleBg/"):
        return "battle-core"
    if path.startswith(BATTLE_UI_PREFIXES):
        return "battle-core"
    if path == "assets/maps/school.tmj" or path.startswith("assets/textures/school-"):
        return "school-map"
    if path == "assets/maps/town.tmj":
        return "town-map"
    if path.startswith("assets/maps/"):
        return "home-map"
    if path.startswith("scripts/"):
        return "rpg-core"
    if path.startswith(GAMEPLAY_DATA_PREFIXES) or path in GAMEPLAY_DATA_PATHS:
        return "rpg-core"
    if path.startswith("assets/i18n/"):
        return "rpg-core"
    if path.startswith("assets/farm-rpg/Enemy/"):
        return "rpg-core"
    if path.startswith("assets/textures/Elements/"):
        return "home-map"
    if path.startswith("assets/farm-rpg/Exterior/"):
        return "home-map"
    if path.startswith("assets/farm-rpg/Farm Animals/"):
        return "home-map"
    if path.startswith("assets/farm-rpg/Farm Crops/"):
        return "home-map"
    if path.startswith("assets/farm-rpg/Farm/"):
        return "home-map"
    if path.startswith(SHARED_UI_PREFIXES) or path.startswith("ui/rmlui/"):
        return "shared-ui"
    if path.startswith("assets/fonts/"):
        return "shared-ui"
    if path.startswith("assets/textures/UI/"):
        return "shared-ui"
    if path.startswith("assets/farm-rpg/UI/"):
        return "shared-ui"
    if path.startswith("assets/farm-rpg/Character"):
        return "shared-ui"
    if path.startswith("assets/farm-rpg/Icons/"):
        return "shared-ui"
    return "boot"


def preload_line(entry: PreloadEntry) -> str:
    return f"--preload-file {entry.source_rel}@{entry.mount_path}"


def package_stats(entries: list[PreloadEntry], artifact_bytes: bytes | None = None) -> dict[str, Any]:
    raw_bytes = sum(entry.source_path.stat().st_size for entry in entries)
    result: dict[str, Any] = {
        "files": len(entries),
        "bytes": raw_bytes,
        "size": human_size(raw_bytes),
    }
    if artifact_bytes is not None:
        result["artifact_bytes"] = len(artifact_bytes)
        result["artifact_size"] = human_size(len(artifact_bytes))
        result["artifact_gzip_bytes"] = gzip_size(artifact_bytes)
        result["artifact_gzip_size"] = human_size(result["artifact_gzip_bytes"])
        brotli_bytes = brotli_size(artifact_bytes)
        if brotli_bytes is not None:
            result["artifact_brotli_bytes"] = brotli_bytes
            result["artifact_brotli_size"] = human_size(brotli_bytes)
    return result


def write_tfpack(package_id: str, entries: list[PreloadEntry], output_path: Path) -> bytes:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = bytearray()
    files: list[dict[str, Any]] = []
    for entry in sorted(entries, key=lambda item: item.source_rel):
        data = entry.source_path.read_bytes()
        offset = len(payload)
        payload.extend(data)
        files.append({
            "path": entry.mount_path,
            "source": entry.source_rel,
            "offset": offset,
            "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        })

    header = {
        "version": PACKAGE_VERSION,
        "package_id": package_id,
        "strategy": PACKAGE_STRATEGY,
        "files": files,
    }
    header_bytes = json.dumps(header, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    artifact = PACKAGE_MAGIC + len(header_bytes).to_bytes(4, "little") + header_bytes + bytes(payload)
    output_path.write_bytes(artifact)
    return artifact


def build_index(
    packages: dict[str, list[PreloadEntry]],
    artifacts: dict[str, bytes | None],
    output_dir: Path,
    boot_preload_output: Path,
    manifest_path: Path,
) -> dict[str, Any]:
    output_parent = output_dir.parent
    package_index: dict[str, Any] = {}
    for package_id, entries in sorted(packages.items()):
        entry_paths = [entry.source_rel for entry in sorted(entries, key=lambda item: item.source_rel)]
        if package_id == "boot":
            package_index[package_id] = {
                "delivery": "emscripten-preload",
                "preload_manifest": boot_preload_output.relative_to(output_parent).as_posix(),
                "dependencies": PACKAGE_DEPENDENCIES.get(package_id, []),
                "paths": entry_paths,
                **package_stats(entries),
            }
            continue

        artifact_path = output_dir / f"{package_id}.tfpack"
        artifact_bytes = artifacts.get(package_id)
        package_index[package_id] = {
            "delivery": "tfpack",
            "url": artifact_path.relative_to(output_parent).as_posix(),
            "artifact": artifact_path.relative_to(output_parent).as_posix(),
            "dependencies": PACKAGE_DEPENDENCIES.get(package_id, []),
            "paths": entry_paths,
            **package_stats(entries, artifact_bytes),
        }

    return {
        "version": PACKAGE_VERSION,
        "strategy": PACKAGE_STRATEGY,
        "source_manifest": manifest_path.as_posix(),
        "packages": package_index,
    }


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Generate TinyFarmRPG Web runtime asset packages.")
    parser.add_argument("--manifest", type=Path, default=root / "manifests" / "assets" / "web-release-full.args")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--boot-preload-output", type=Path, required=True)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument(
        "--skip-artifacts",
        action="store_true",
        help="Only write the package plan and boot preload manifest; do not emit .tfpack artifacts.",
    )
    args = parser.parse_args()

    manifest_path = args.manifest.resolve()
    output_dir = args.output_dir.resolve()
    boot_preload_output = args.boot_preload_output.resolve()
    json_output = args.json_output.resolve()

    entries = parse_preload_manifest(root, manifest_path)
    packages: dict[str, list[PreloadEntry]] = {package_id: [] for package_id in PACKAGE_DEPENDENCIES}
    for entry in entries:
        packages[classify(entry)].append(entry)

    output_dir.mkdir(parents=True, exist_ok=True)
    boot_preload_output.parent.mkdir(parents=True, exist_ok=True)
    boot_preload_output.write_text(
        "\n".join(preload_line(entry) for entry in sorted(packages["boot"], key=lambda item: item.source_rel)) + "\n",
        encoding="utf-8",
    )

    artifacts: dict[str, bytes | None] = {}
    for package_id, package_entries in packages.items():
        if package_id == "boot":
            continue
        artifacts[package_id] = (
            None
            if args.skip_artifacts
            else write_tfpack(package_id, package_entries, output_dir / f"{package_id}.tfpack")
        )

    index = build_index(packages, artifacts, output_dir, boot_preload_output, manifest_path)
    write_json(json_output, index)

    print("TinyFarmRPG Web package plan")
    for package_id, package in index["packages"].items():
        detail = f"{package['files']} files, {package['size']}"
        if package.get("artifact_size"):
            detail += f", artifact {package['artifact_size']}, gzip {package['artifact_gzip_size']}"
        print(f"- {package_id}: {detail}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
