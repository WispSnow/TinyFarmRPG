#!/usr/bin/env python3
"""Validate TinyFarmRPG Web release artifacts and resource gates."""

from __future__ import annotations

import argparse
import gzip
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ARTIFACTS = (
    "TinyFarmRPG-Web.html",
    "TinyFarmRPG-Web.js",
    "TinyFarmRPG-Web.wasm",
    "TinyFarmRPG-Web.data",
    "favicon.ico",
)

BOOT_DATA_BUDGET_BYTES = 4 * 1024 * 1024

REQUIRED_BOOT_PRELOAD_PATHS = {
    "assets/data/cursor_config.json",
    "assets/data/resource_mapping.json",
    "assets/fonts/VonwaonBitmap-16px.ttf",
    "assets/shaders/composite.frag",
    "assets/shaders/composite.vert",
    "assets/shaders/texture.frag",
    "assets/textures/UI/farm-rpg-bg.png",
    "assets/textures/UI/farm-rpg-logo.png",
    "assets/farm-rpg/UI/button.png",
    "assets/i18n/en-US.json",
    "assets/i18n/languages.json",
    "assets/i18n/zh-Hans.json",
    "config/audio.json",
    "config/input.json",
    "config/render.json",
    "config/user_settings.default.json",
    "config/window.json",
    "ui/rmlui/scenes/title.rcss",
    "ui/rmlui/scenes/title.rml",
    "ui/rmlui/scenes/title_widgets.rcss",
    "ui/rmlui/theme/base.rcss",
    "ui/rmlui/theme/nav.rcss",
    "ui/rmlui/theme/reset.rcss",
}

FORBIDDEN_BOOT_PRELOAD_PATHS = {
    "assets/audio/01_spring_journey.ogg",
    "assets/audio/02_spring_fairy_tale.ogg",
    "assets/audio/pop.mp3",
    "assets/maps/farm-rpg.world",
    "assets/maps/home_exterior.tmj",
    "assets/maps/home_interior.tmj",
    "scripts/bootstrap.lua",
    "ui/rmlui/hud/hotbar.rml",
    "ui/rmlui/scenes/appearance_customize.rml",
    "ui/rmlui/scenes/inventory_menu.rml",
    "ui/rmlui/scenes/pause_menu.rml",
    "ui/rmlui/scenes/save_slot_select.rml",
}

REQUIRED_FULL_PACKAGE_PATHS = {
    "assets/audio/01_spring_journey.ogg",
    "assets/audio/02_spring_fairy_tale.ogg",
    "assets/audio/pop.mp3",
    "assets/data/audio_cues.json",
    "assets/data/map_loading_config.json",
    "assets/fonts/LXGWBright-Regular.ttf",
    "assets/fonts/VonwaonBitmap-16px.ttf",
    "assets/maps/farm-rpg.world",
    "assets/maps/home_exterior.tmj",
    "assets/maps/home_interior.tmj",
    "config/audio.json",
    "config/render.json",
    "scripts/bootstrap.lua",
    "ui/rmlui/hud/hotbar.rml",
    "ui/rmlui/scenes/inventory_menu.rcss",
    "ui/rmlui/scenes/inventory_menu.rml",
    "ui/rmlui/scenes/appearance_customize.rml",
    "ui/rmlui/scenes/pause_menu.rml",
    "ui/rmlui/scenes/save_slot_select.rml",
    "ui/rmlui/scenes/title.rml",
}

REQUIRED_RUNTIME_PACKAGES = {
    "boot",
    "shared-ui",
    "home-map",
    "audio-core",
}

REQUIRED_HOME_MAP_PACKAGE_PATHS = {
    "assets/maps/farm-rpg.world",
    "assets/maps/home_exterior.tmj",
    "assets/maps/home_interior.tmj",
}

REQUIRED_AUDIO_CORE_PACKAGE_PATHS = {
    "assets/audio/01_spring_journey.ogg",
    "assets/audio/02_spring_fairy_tale.ogg",
    "assets/audio/pop.mp3",
}

FORBIDDEN_SINGLE_THREAD_FLAGS = (
    "-sUSE_PTHREADS=1",
    "-sPTHREAD_POOL_SIZE",
    "PTHREAD_POOL_SIZE",
    "-pthread",
)


@dataclass(frozen=True)
class PreloadEntry:
    source_rel: str
    source_path: Path
    mount_path: str


class Gate:
    def __init__(self) -> None:
        self.failures: list[str] = []
        self.warnings: list[str] = []
        self.notes: list[str] = []

    def fail(self, message: str) -> None:
        self.failures.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    def note(self, message: str) -> None:
        self.notes.append(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def human_size(value: int) -> str:
    size = float(value)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if size < 1024.0 or unit == "GiB":
            return f"{size:.1f} {unit}" if unit != "B" else f"{int(size)} B"
        size /= 1024.0
    return f"{value} B"


def gzip_bytes(path: Path) -> int:
    data = path.read_bytes()
    return len(gzip.compress(data, compresslevel=9, mtime=0))


def parse_cmake_cache(cache_path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not cache_path.exists():
        return values
    for raw_line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw_line.strip()
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        lhs, value = line.split("=", 1)
        key = lhs.split(":", 1)[0]
        values[key] = value.strip()
    return values


def cmake_bool(value: str | None) -> bool:
    return value is not None and value.upper() in {"1", "ON", "TRUE", "YES"}


def parse_preload_manifest(root: Path, manifest_path: Path, gate: Gate) -> list[PreloadEntry]:
    entries: list[PreloadEntry] = []
    seen_mounts: set[str] = set()
    seen_sources: set[str] = set()

    if not manifest_path.exists():
        gate.fail(f"Preload manifest missing: {manifest_path}")
        return entries

    for line_number, raw_line in enumerate(manifest_path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        if line.startswith("--preload-file="):
            payload = line[len("--preload-file=") :]
        elif line.startswith("--preload-file "):
            payload = line[len("--preload-file ") :]
        else:
            gate.fail(f"{manifest_path}:{line_number} is not a --preload-file entry")
            continue

        if "@" in payload:
            source_text, mount_path = payload.split("@", 1)
        else:
            source_text = payload
            mount_path = f"/{source_text}"

        source_path = Path(source_text)
        resolved_source = source_path if source_path.is_absolute() else root / source_path
        resolved_source = resolved_source.resolve()
        if source_path.is_absolute():
            try:
                source_rel = resolved_source.relative_to(root).as_posix()
            except ValueError:
                source_rel = resolved_source.as_posix()
                gate.fail(f"{manifest_path}:{line_number} preload source is outside the project root: {source_text}")
        else:
            source_rel = source_text

        if not mount_path.startswith("/"):
            gate.fail(f"{manifest_path}:{line_number} mount path must be absolute: {mount_path}")
        if source_rel in {"assets", "ui", "scripts", "config"} or source_rel.endswith("/"):
            gate.fail(f"{manifest_path}:{line_number} preloads a whole tree instead of a file: {source_rel}")
        if source_rel in seen_sources:
            gate.fail(f"{manifest_path}:{line_number} duplicates preload source: {source_rel}")
        if mount_path in seen_mounts:
            gate.fail(f"{manifest_path}:{line_number} duplicates preload mount: {mount_path}")
        if not resolved_source.exists():
            gate.fail(f"{manifest_path}:{line_number} preload source missing: {source_rel}")
        elif not resolved_source.is_file():
            gate.fail(f"{manifest_path}:{line_number} preload source is not a file: {source_rel}")
        if mount_path != f"/{source_rel}":
            gate.fail(f"{manifest_path}:{line_number} mount does not mirror source path: {source_rel} -> {mount_path}")

        seen_sources.add(source_rel)
        seen_mounts.add(mount_path)
        entries.append(PreloadEntry(source_rel=source_rel, source_path=resolved_source, mount_path=mount_path))

    if not entries:
        gate.fail(f"Preload manifest is empty: {manifest_path}")
    return entries


def load_json(path: Path, gate: Gate) -> dict[str, Any]:
    if not path.exists():
        gate.fail(f"JSON file missing: {path}")
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        gate.fail(f"Invalid JSON in {path}: {exc}")
        return {}
    if not isinstance(data, dict):
        gate.fail(f"Expected a JSON object in {path}")
        return {}
    return data


def validate_artifacts(build_dir: Path, gate: Gate) -> dict[str, dict[str, int | str]]:
    artifacts: dict[str, dict[str, int | str]] = {}
    for name in ARTIFACTS:
        path = build_dir / name
        if not path.exists():
            gate.fail(f"Build artifact missing: {path}")
            continue
        if not path.is_file():
            gate.fail(f"Build artifact is not a file: {path}")
            continue
        size = path.stat().st_size
        artifacts[name] = {
            "bytes": size,
            "gzip_bytes": gzip_bytes(path),
            "size": human_size(size),
        }
    return artifacts


def validate_cmake_cache(
    build_dir: Path,
    full_manifest_path: Path,
    boot_manifest_path: Path,
    allow_pthreads: bool,
    gate: Gate,
) -> dict[str, str]:
    cache_path = build_dir / "CMakeCache.txt"
    cache = parse_cmake_cache(cache_path)
    if not cache:
        gate.fail(f"CMake cache missing or empty: {cache_path}")
        return cache

    expected_values = {
        "TF_BUILD_WEB": True,
        "TF_ENABLE_RUNTIME_THREADS": False,
        "TF_WEB_ENABLE_PTHREADS": False,
        "TF_WEB_BOOT_ONLY_PRELOAD": True,
        "TF_WEB_ENABLE_RUNTIME_PACKAGES": True,
    }
    if allow_pthreads:
        expected_values["TF_ENABLE_RUNTIME_THREADS"] = True
        expected_values["TF_WEB_ENABLE_PTHREADS"] = True

    for key, expected in expected_values.items():
        actual = cmake_bool(cache.get(key))
        if actual != expected:
            gate.fail(f"{key} expected {'ON' if expected else 'OFF'}, got {cache.get(key, '<missing>')}")

    if cache.get("CMAKE_BUILD_TYPE") != "Release":
        gate.fail(f"CMAKE_BUILD_TYPE expected Release, got {cache.get('CMAKE_BUILD_TYPE', '<missing>')}")

    toolchain = cache.get("CMAKE_TOOLCHAIN_FILE", "")
    if "Emscripten.cmake" not in toolchain:
        gate.fail(f"CMAKE_TOOLCHAIN_FILE does not look like Emscripten: {toolchain or '<missing>'}")

    full_preload_cache = cache.get("TF_WEB_FULL_PRELOAD_ARGS")
    if full_preload_cache is None:
        gate.fail("TF_WEB_FULL_PRELOAD_ARGS missing from CMake cache")
    else:
        try:
            if Path(full_preload_cache).resolve() != full_manifest_path.resolve():
                gate.fail(f"TF_WEB_FULL_PRELOAD_ARGS does not match full manifest: {full_preload_cache}")
        except OSError:
            gate.fail(f"TF_WEB_FULL_PRELOAD_ARGS is not a valid path: {full_preload_cache}")

    preload_cache = cache.get("TF_WEB_PRELOAD_ARGS")
    if preload_cache is None:
        gate.fail("TF_WEB_PRELOAD_ARGS missing from CMake cache")
    else:
        try:
            if Path(preload_cache).resolve() != boot_manifest_path.resolve():
                gate.fail(f"TF_WEB_PRELOAD_ARGS must point at the boot preload manifest: {preload_cache}")
        except OSError:
            gate.fail(f"TF_WEB_PRELOAD_ARGS is not a valid path: {preload_cache}")

    build_ninja = build_dir / "build.ninja"
    if build_ninja.exists():
        text = build_ninja.read_text(encoding="utf-8", errors="ignore")
        if not allow_pthreads:
            for flag in FORBIDDEN_SINGLE_THREAD_FLAGS:
                if flag in text:
                    gate.fail(f"Single-thread Web build contains pthread flag in build.ninja: {flag}")
        elif "-sUSE_PTHREADS=1" not in text:
            gate.fail("Pthreads build is allowed but build.ninja does not contain -sUSE_PTHREADS=1")
    else:
        gate.warn(f"build.ninja missing; skipped link flag scan: {build_ninja}")

    if allow_pthreads:
        gate.note("Pthreads release previews require COOP/COEP headers and SharedArrayBuffer support.")
    return cache


def validate_full_manifest_budget(
    entries: list[PreloadEntry],
    budget_path: Path,
    gate: Gate,
) -> dict[str, Any]:
    budget = load_json(budget_path, gate)
    if not budget:
        return {}

    web_budget = budget.get("web_release_full_assets") or budget.get("web_poc_assets", {})
    used_budget = budget.get("used_assets", {})
    actual_bytes = sum(entry.source_path.stat().st_size for entry in entries if entry.source_path.exists())
    actual_files = len(entries)
    expected_files = web_budget.get("files")
    expected_bytes = web_budget.get("bytes")

    if expected_files != actual_files:
        gate.fail(f"web release full manifest files expected {expected_files}, manifest has {actual_files}")
    if expected_bytes != actual_bytes:
        gate.fail(f"web release full manifest bytes expected {expected_bytes}, manifest has {actual_bytes}")

    used_files = used_budget.get("files")
    used_bytes = used_budget.get("bytes")
    if isinstance(used_files, int) and used_files <= actual_files:
        gate.fail(f"First-screen manifest is not smaller by file count: {actual_files} >= {used_files}")
    if isinstance(used_bytes, int) and used_bytes <= actual_bytes:
        gate.fail(f"First-screen manifest is not smaller by byte count: {actual_bytes} >= {used_bytes}")

    source_paths = {entry.source_rel for entry in entries}
    missing_required = sorted(REQUIRED_FULL_PACKAGE_PATHS - source_paths)
    for rel in missing_required:
        gate.fail(f"Required Web release full asset missing: {rel}")

    shader_assets = sorted(path for path in source_paths if path.startswith("assets/shaders/"))
    if not shader_assets:
        gate.fail("No shader assets are included in the Web release full manifest")

    ratio = 0.0
    if isinstance(used_bytes, int) and used_bytes > 0:
        ratio = actual_bytes / used_bytes

    return {
        "files": actual_files,
        "bytes": actual_bytes,
        "size": human_size(actual_bytes),
        "used_assets_files": used_files,
        "used_assets_bytes": used_bytes,
        "used_assets_size": human_size(used_bytes) if isinstance(used_bytes, int) else "",
        "ratio_of_used_assets": round(ratio, 4),
        "shader_assets": len(shader_assets),
    }


def validate_boot_preload_budget(
    boot_entries: list[PreloadEntry],
    full_entries: list[PreloadEntry],
    artifacts: dict[str, dict[str, int | str]],
    gate: Gate,
) -> dict[str, Any]:
    boot_paths = {entry.source_rel for entry in boot_entries}
    full_paths = {entry.source_rel for entry in full_entries}
    boot_bytes = sum(entry.source_path.stat().st_size for entry in boot_entries if entry.source_path.exists())
    full_bytes = sum(entry.source_path.stat().st_size for entry in full_entries if entry.source_path.exists())

    unknown_boot_paths = sorted(boot_paths - full_paths)
    for path in unknown_boot_paths:
        gate.fail(f"Boot preload path is outside the Web release full manifest: {path}")

    missing_required = sorted(REQUIRED_BOOT_PRELOAD_PATHS - boot_paths)
    for path in missing_required:
        gate.fail(f"Required boot preload asset missing: {path}")

    forbidden = sorted(FORBIDDEN_BOOT_PRELOAD_PATHS & boot_paths)
    for path in forbidden:
        gate.fail(f"Runtime package asset leaked into boot preload: {path}")

    if full_bytes > 0 and boot_bytes >= full_bytes:
        gate.fail(f"Boot preload is not smaller than the full manifest: {boot_bytes} >= {full_bytes}")

    data_artifact = artifacts.get("TinyFarmRPG-Web.data", {})
    data_bytes = data_artifact.get("bytes")
    if isinstance(data_bytes, int) and data_bytes > BOOT_DATA_BUDGET_BYTES:
        gate.fail(
            "TinyFarmRPG-Web.data exceeds the boot-only budget: "
            f"{human_size(data_bytes)} > {human_size(BOOT_DATA_BUDGET_BYTES)}"
        )

    return {
        "files": len(boot_entries),
        "bytes": boot_bytes,
        "size": human_size(boot_bytes),
        "full_manifest_files": len(full_entries),
        "full_manifest_bytes": full_bytes,
        "full_manifest_size": human_size(full_bytes),
        "ratio_of_full_manifest": round(boot_bytes / full_bytes, 4) if full_bytes > 0 else 0.0,
        "data_budget_bytes": BOOT_DATA_BUDGET_BYTES,
        "data_budget_size": human_size(BOOT_DATA_BUDGET_BYTES),
    }


def validate_staged_preload(build_dir: Path, entries: list[PreloadEntry], gate: Gate) -> dict[str, Any]:
    stage_root_candidates = (
        build_dir / "TinyFarmRPG-Web-preload-root",
        build_dir / "src" / "web" / "TinyFarmRPG-Web-preload-root",
    )
    stage_root = next((candidate for candidate in stage_root_candidates if candidate.exists()), stage_root_candidates[0])
    if not stage_root.exists():
        candidates = ", ".join(str(candidate) for candidate in stage_root_candidates)
        gate.fail(f"Staged preload root missing. Checked: {candidates}")
        return {"stage_root": str(stage_root), "files_checked": 0}

    checked = 0
    for entry in entries:
        staged = stage_root / entry.mount_path.lstrip("/")
        if not staged.exists():
            gate.fail(f"Staged preload file missing: {staged}")
            continue
        if staged.stat().st_size != entry.source_path.stat().st_size:
            gate.fail(f"Staged preload file size mismatch: {entry.source_rel}")
            continue
        checked += 1

    return {"stage_root": str(stage_root), "files_checked": checked}


def validate_runtime_packages(
    build_dir: Path,
    entries: list[PreloadEntry],
    boot_entries: list[PreloadEntry],
    package_index_path: Path,
    gate: Gate,
) -> dict[str, Any]:
    index = load_json(package_index_path, gate)
    if not index:
        return {}

    if index.get("strategy") != "custom_sync_xhr_fs_writefile":
        gate.fail(f"Runtime package strategy must be custom_sync_xhr_fs_writefile: {package_index_path}")

    packages = index.get("packages")
    if not isinstance(packages, dict):
        gate.fail(f"Runtime package index missing packages object: {package_index_path}")
        return index

    missing_packages = sorted(REQUIRED_RUNTIME_PACKAGES - set(packages.keys()))
    for package_id in missing_packages:
        gate.fail(f"Runtime package missing from index: {package_id}")

    source_paths = {entry.source_rel for entry in entries}
    packaged_paths: set[str] = set()
    package_summaries: dict[str, Any] = {}

    for package_id, package in packages.items():
        if not isinstance(package, dict):
            gate.fail(f"Runtime package '{package_id}' must be an object")
            continue

        paths = package.get("paths")
        if not isinstance(paths, list) or not all(isinstance(path, str) for path in paths):
            gate.fail(f"Runtime package '{package_id}' missing string paths list")
            continue
        path_set = set(paths)
        duplicate_count = len(paths) - len(path_set)
        if duplicate_count:
            gate.fail(f"Runtime package '{package_id}' contains {duplicate_count} duplicate paths")
        packaged_paths.update(path_set)

        unknown_paths = sorted(path_set - source_paths)
        for path in unknown_paths:
            gate.fail(f"Runtime package '{package_id}' references path outside preload manifest: {path}")

        delivery = package.get("delivery")
        if package_id == "boot":
            if delivery != "emscripten-preload":
                gate.fail("Runtime package 'boot' must use emscripten-preload delivery")
            preload_manifest = package.get("preload_manifest")
            if not isinstance(preload_manifest, str) or not (build_dir / preload_manifest).is_file():
                gate.fail(f"Boot runtime preload manifest missing: {preload_manifest}")
        else:
            if delivery != "tfpack":
                gate.fail(f"Runtime package '{package_id}' must use tfpack delivery")
            artifact = package.get("artifact")
            if not isinstance(artifact, str):
                gate.fail(f"Runtime package '{package_id}' missing artifact path")
            else:
                artifact_path = build_dir / artifact
                if not artifact_path.is_file():
                    gate.fail(f"Runtime package artifact missing: {artifact_path}")
                elif artifact_path.stat().st_size <= 12:
                    gate.fail(f"Runtime package artifact too small: {artifact_path}")

        package_summaries[package_id] = {
            "files": package.get("files"),
            "bytes": package.get("bytes"),
            "size": package.get("size"),
            "delivery": delivery,
        }

    missing_packaged_paths = sorted(source_paths - packaged_paths)
    for path in missing_packaged_paths:
        gate.fail(f"Preload manifest path is not assigned to a runtime package: {path}")

    boot = packages.get("boot", {})
    if isinstance(boot, dict):
        if isinstance(boot.get("paths"), list):
            boot_package_paths = set(boot["paths"])
            boot_preload_paths = {entry.source_rel for entry in boot_entries}
            if boot_package_paths != boot_preload_paths:
                missing = sorted(boot_package_paths - boot_preload_paths)
                extra = sorted(boot_preload_paths - boot_package_paths)
                if missing:
                    gate.fail(f"Boot package paths missing from boot preload manifest: {missing}")
                if extra:
                    gate.fail(f"Boot preload manifest contains paths outside boot package: {extra}")

        boot_bytes = boot.get("bytes")
        full_bytes = sum(entry.source_path.stat().st_size for entry in entries if entry.source_path.exists())
        if not isinstance(boot_bytes, int):
            gate.fail("Runtime package 'boot' missing byte count")
        elif boot_bytes >= full_bytes:
            gate.fail(f"Runtime boot package is not smaller than the current single package: {boot_bytes} >= {full_bytes}")

    home_map = packages.get("home-map", {})
    if isinstance(home_map, dict) and isinstance(home_map.get("paths"), list):
        missing = sorted(REQUIRED_HOME_MAP_PACKAGE_PATHS - set(home_map["paths"]))
        for path in missing:
            gate.fail(f"home-map package missing required path: {path}")

    audio_core = packages.get("audio-core", {})
    if isinstance(audio_core, dict) and isinstance(audio_core.get("paths"), list):
        missing = sorted(REQUIRED_AUDIO_CORE_PACKAGE_PATHS - set(audio_core["paths"]))
        for path in missing:
            gate.fail(f"audio-core package missing required path: {path}")

    return {
        "index": str(package_index_path),
        "strategy": index.get("strategy"),
        "packages": package_summaries,
    }


def validate_shader_boundary(root: Path, gate: Gate) -> dict[str, Any]:
    web_main = root / "src" / "web" / "web_main.cpp"
    gl_platform = root / "src" / "engine" / "platform" / "gl_platform.h"
    shader_program = root / "src" / "engine" / "render" / "opengl" / "shader_program.cpp"

    web_main_text = web_main.read_text(encoding="utf-8", errors="ignore")
    gl_platform_text = gl_platform.read_text(encoding="utf-8", errors="ignore")
    shader_program_text = shader_program.read_text(encoding="utf-8", errors="ignore")

    inline_es_versions = len(re.findall(r"#version\s+300\s+es", web_main_text))
    if inline_es_versions < 2:
        gate.fail("Web walking skeleton must keep both vertex and fragment shaders at #version 300 es")

    required_gl_platform_snippets = (
        "TF_GL_PLATFORM_WEBGL",
        "kTextureColorInternalFormat = kIsWebGL ? GL_RGBA8 : GL_SRGB8_ALPHA8",
        'kIsWebGL ? "#version 300 es" : "#version 330 core"',
    )
    for snippet in required_gl_platform_snippets:
        if snippet not in gl_platform_text:
            gate.fail(f"gl_platform.h missing WebGL boundary snippet: {snippet}")

    required_shader_program_snippets = (
        "prepareShaderSource",
        'desktop_version = "#version 330 core"',
        "prepared.replace",
        "precision mediump float",
        "precision highp float",
        "glCompileShader",
    )
    for snippet in required_shader_program_snippets:
        if snippet not in shader_program_text:
            gate.fail(f"shader_program.cpp missing Web shader conversion snippet: {snippet}")

    shader_sources = sorted((root / "assets" / "shaders").glob("*"))
    desktop_shader_count = 0
    for shader_path in shader_sources:
        if shader_path.suffix not in {".vert", ".frag"}:
            continue
        first_line = shader_path.read_text(encoding="utf-8", errors="ignore").splitlines()[0]
        if first_line.strip() == "#version 330 core":
            desktop_shader_count += 1

    return {
        "web_inline_300_es_shaders": inline_es_versions,
        "desktop_shader_assets_rewritten_at_runtime": desktop_shader_count,
    }


def write_json(path: Path, summary: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Validate TinyFarmRPG Web release gate.")
    parser.add_argument("--build-dir", type=Path, default=root / "build" / "web-release")
    parser.add_argument("--manifest", type=Path, help="Alias for --full-manifest.")
    parser.add_argument("--full-manifest", type=Path, default=root / "manifests" / "assets" / "web-release-full.args")
    parser.add_argument("--boot-manifest", type=Path)
    parser.add_argument("--asset-budget", type=Path, default=root / "manifests" / "assets" / "asset-budget.json")
    parser.add_argument("--package-index", type=Path)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument(
        "--allow-pthreads",
        action="store_true",
        help="Validate the pthreads release variant instead of the default single-thread build.",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    full_manifest_path = (args.manifest or args.full_manifest).resolve()
    cache_for_defaults = parse_cmake_cache(build_dir / "CMakeCache.txt")
    boot_manifest_path = (
        args.boot_manifest.resolve()
        if args.boot_manifest is not None
        else Path(cache_for_defaults.get("TF_WEB_PRELOAD_ARGS", build_dir / "web-boot-preload.args")).resolve()
    )
    budget_path = args.asset_budget.resolve()
    package_index_path = (
        args.package_index.resolve()
        if args.package_index is not None
        else build_dir / "web-packages" / "web-package-index.json"
    )
    gate = Gate()

    full_entries = parse_preload_manifest(root, full_manifest_path, gate)
    boot_entries = parse_preload_manifest(root, boot_manifest_path, gate)
    artifacts = validate_artifacts(build_dir, gate)
    summary: dict[str, Any] = {
        "build_dir": str(build_dir),
        "full_manifest": str(full_manifest_path),
        "boot_manifest": str(boot_manifest_path),
        "asset_budget": str(budget_path),
        "artifacts": artifacts,
        "cmake": validate_cmake_cache(
            build_dir,
            full_manifest_path,
            boot_manifest_path,
            args.allow_pthreads,
            gate,
        ),
        "full_package_manifest": validate_full_manifest_budget(full_entries, budget_path, gate),
        "preload": validate_boot_preload_budget(boot_entries, full_entries, artifacts, gate),
        "staged_preload": validate_staged_preload(build_dir, boot_entries, gate),
        "runtime_packages": validate_runtime_packages(build_dir, full_entries, boot_entries, package_index_path, gate),
        "shader_boundary": validate_shader_boundary(root, gate),
        "warnings": gate.warnings,
        "notes": gate.notes,
        "failures": gate.failures,
    }

    if args.json_output is not None:
        write_json(args.json_output.resolve(), summary)

    print("TinyFarmRPG Web release gate")
    print(f"- build dir: {build_dir}")
    print(
        "- boot preload: "
        f"{summary['preload'].get('files', 0)} files, {summary['preload'].get('size', '0 B')}"
    )
    print(
        "- full package manifest: "
        f"{summary['full_package_manifest'].get('files', 0)} files, "
        f"{summary['full_package_manifest'].get('size', '0 B')}"
    )
    if summary["runtime_packages"]:
        print(f"- runtime packages: {summary['runtime_packages'].get('strategy', '<missing>')}")
        for package_id, package in summary["runtime_packages"].get("packages", {}).items():
            print(f"  - {package_id}: {package.get('files', 0)} files, {package.get('size', '0 B')}")
    for name, artifact in summary["artifacts"].items():
        print(f"- {name}: {artifact['size']} (gzip {human_size(int(artifact['gzip_bytes']))})")
    if gate.notes:
        for note in gate.notes:
            print(f"- note: {note}")
    if gate.warnings:
        for warning in gate.warnings:
            print(f"- warning: {warning}")
    if gate.failures:
        print("Failures:", file=sys.stderr)
        for failure in gate.failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("Gate passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
