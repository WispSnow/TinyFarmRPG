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

REQUIRED_PRELOAD_PATHS = {
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
    "ui/rmlui/scenes/appearance_customize.rml",
    "ui/rmlui/scenes/pause_menu.rml",
    "ui/rmlui/scenes/save_slot_select.rml",
    "ui/rmlui/scenes/title.rml",
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
    manifest_path: Path,
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

    preload_cache = cache.get("TF_WEB_PRELOAD_ARGS")
    if preload_cache is None:
        gate.fail("TF_WEB_PRELOAD_ARGS missing from CMake cache")
    else:
        try:
            if Path(preload_cache).resolve() != manifest_path.resolve():
                gate.fail(f"TF_WEB_PRELOAD_ARGS does not match manifest: {preload_cache}")
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


def validate_preload_budget(
    entries: list[PreloadEntry],
    budget_path: Path,
    gate: Gate,
) -> dict[str, Any]:
    budget = load_json(budget_path, gate)
    if not budget:
        return {}

    web_budget = budget.get("web_poc_assets", {})
    used_budget = budget.get("used_assets", {})
    actual_bytes = sum(entry.source_path.stat().st_size for entry in entries if entry.source_path.exists())
    actual_files = len(entries)
    expected_files = web_budget.get("files")
    expected_bytes = web_budget.get("bytes")

    if expected_files != actual_files:
        gate.fail(f"web_poc_assets.files expected {expected_files}, manifest has {actual_files}")
    if expected_bytes != actual_bytes:
        gate.fail(f"web_poc_assets.bytes expected {expected_bytes}, manifest has {actual_bytes}")

    used_files = used_budget.get("files")
    used_bytes = used_budget.get("bytes")
    if isinstance(used_files, int) and used_files <= actual_files:
        gate.fail(f"First-screen manifest is not smaller by file count: {actual_files} >= {used_files}")
    if isinstance(used_bytes, int) and used_bytes <= actual_bytes:
        gate.fail(f"First-screen manifest is not smaller by byte count: {actual_bytes} >= {used_bytes}")

    source_paths = {entry.source_rel for entry in entries}
    missing_required = sorted(REQUIRED_PRELOAD_PATHS - source_paths)
    for rel in missing_required:
        gate.fail(f"Required first-screen preload asset missing: {rel}")

    shader_assets = sorted(path for path in source_paths if path.startswith("assets/shaders/"))
    if not shader_assets:
        gate.fail("No shader assets are included in the Web POC preload manifest")

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
    parser.add_argument("--manifest", type=Path, default=root / "manifests" / "assets" / "web-poc-preload.args")
    parser.add_argument("--asset-budget", type=Path, default=root / "manifests" / "assets" / "asset-budget.json")
    parser.add_argument("--json-output", type=Path)
    parser.add_argument(
        "--allow-pthreads",
        action="store_true",
        help="Validate the pthreads release variant instead of the default single-thread build.",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    manifest_path = args.manifest.resolve()
    budget_path = args.asset_budget.resolve()
    gate = Gate()

    entries = parse_preload_manifest(root, manifest_path, gate)
    summary: dict[str, Any] = {
        "build_dir": str(build_dir),
        "manifest": str(manifest_path),
        "asset_budget": str(budget_path),
        "artifacts": validate_artifacts(build_dir, gate),
        "cmake": validate_cmake_cache(build_dir, manifest_path, args.allow_pthreads, gate),
        "preload": validate_preload_budget(entries, budget_path, gate),
        "staged_preload": validate_staged_preload(build_dir, entries, gate),
        "shader_boundary": validate_shader_boundary(root, gate),
        "warnings": gate.warnings,
        "notes": gate.notes,
        "failures": gate.failures,
    }

    if args.json_output is not None:
        write_json(args.json_output.resolve(), summary)

    print("TinyFarmRPG Web release gate")
    print(f"- build dir: {build_dir}")
    print(f"- preload: {summary['preload'].get('files', 0)} files, {summary['preload'].get('size', '0 B')}")
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
