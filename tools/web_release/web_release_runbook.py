#!/usr/bin/env python3
"""Repeatable Web release automation and manual-preview entry points."""

from __future__ import annotations

import argparse
import datetime as dt
import gzip
import hashlib
import json
import os
import plistlib
import platform
import shlex
import shutil
import socket
import socketserver
import subprocess
import sys
import time
import webbrowser
from pathlib import Path
from typing import Any, Iterable

from serve_web_release import cache_value, cmake_bool, make_handler


ARTIFACTS = (
    "TinyFarmRPG-Web.html",
    "TinyFarmRPG-Web.js",
    "TinyFarmRPG-Web.wasm",
    "TinyFarmRPG-Web.data",
    "favicon.ico",
)

METADATA_ARTIFACTS = (
    "web-boot-preload.args",
    "web-packages/web-package-index.json",
)

MIME_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript",
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
    ".tfpack": "application/octet-stream",
    ".json": "application/json",
    ".args": "text/plain; charset=utf-8",
    ".ico": "image/vnd.microsoft.icon",
}

PRODUCTION_CACHE_POLICY = {
    "entry": "no-cache",
    "binary": "public, max-age=31536000, immutable when deployed under a versioned release directory; otherwise no-cache",
    "metadata": "no-cache",
}

SCRIPT_CHECKS = (
    "tools/web_release/package_web_assets.py",
    "tools/web_release/serve_web_release.py",
    "tools/web_release/validate_web_release.py",
    "tools/web_release/web_smoke.py",
    "tools/web_release/web_release_runbook.py",
    "tools/asset_audit/audit_assets.py",
)

MANUAL_CHECKLIST = (
    "Title page renders Start / Load / Exit without console errors.",
    "Start reaches player setup and then home_exterior.",
    "Network shows shared-ui, rpg-core, home-map, town-map, school-map, battle-core, vfx-core, and audio-core packages fetched on demand.",
    "Travel home_exterior -> home_interior -> home_exterior -> town -> school, then return to town and trigger a battle encounter.",
    "Win a skill-based battle and confirm Effekseer VFX diagnostics report the effekseer backend.",
    "Complete shop buy/sell/failure feedback, quest accept/turn-in, recruit, rest, and wardrobe flows.",
    "Save slot0, reload the page, then Load slot0 and confirm quest, party, appearance, settings, and map state persist.",
    "Confirm render diagnostics show HDR/Bloom enabled, or concrete fallback reasons on unsupported WebGL2 devices.",
    "Confirm TinyFarmRPG-Web.data stays boot-only sized and no COOP/COEP headers are required for single-thread builds.",
)

DEBUG_CHECKLIST = (
    "Confirm no persistent Debug UI toolbar is visible after the runtime starts.",
    "Press F5 and confirm Engine Debug Panels opens without reloading the browser page.",
    "Press F6 after entering gameplay and confirm Game Debug Panels opens.",
    "Confirm Ctrl+Shift+5 and Ctrl+Shift+6 toggle the same panels.",
    "Open with ?debug-ui=all and confirm both debug hubs start visible.",
    "Interact with ImGui controls and confirm RmlUi/game input is not double-triggered while captured.",
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).astimezone().isoformat(timespec="seconds")


def quote_command(command: Iterable[str]) -> str:
    return " ".join(shlex.quote(str(part)) for part in command)


def human_size(value: int) -> str:
    size = float(value)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if size < 1024.0 or unit == "GiB":
            return f"{size:.1f} {unit}" if unit != "B" else f"{value} B"
        size /= 1024.0
    return f"{value} B"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def gzip_file_bytes(path: Path) -> int:
    return len(gzip.compress(path.read_bytes(), compresslevel=9, mtime=0))


def brotli_file_bytes(path: Path, env: dict[str, str]) -> int | None:
    try:
        import brotli  # type: ignore[import-not-found]
    except ImportError:
        brotli = None
    if brotli is not None:
        return len(brotli.compress(path.read_bytes(), quality=11))

    brotli_cli = shutil.which("brotli", path=env.get("PATH"))
    if brotli_cli is None:
        return None
    try:
        completed = subprocess.run(
            [brotli_cli, "--stdout", "--quality=11", str(path)],
            env=env,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=30,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return None
    return len(completed.stdout)


def artifact_mime(path: Path) -> str:
    return MIME_TYPES.get(path.suffix, "application/octet-stream")


def artifact_kind(relative_path: str) -> str:
    if relative_path in ARTIFACTS:
        return "entry" if relative_path.endswith(".html") else "core"
    if relative_path.endswith(".tfpack"):
        return "runtime_package"
    if relative_path.endswith("web-package-index.json"):
        return "release_manifest"
    return "build_metadata"


def artifact_cache_policy(kind: str) -> str:
    if kind == "entry":
        return PRODUCTION_CACHE_POLICY["entry"]
    if kind in {"runtime_package", "core"}:
        return PRODUCTION_CACHE_POLICY["binary"]
    return PRODUCTION_CACHE_POLICY["metadata"]


def default_build_dir(root: Path) -> Path:
    current_phase = root / "build" / "web-gameplay-phase11"
    if current_phase.exists():
        return current_phase
    return root / "build" / "web-release"


def tmp_pycache_dir() -> str:
    private_tmp = Path("/private/tmp")
    if private_tmp.is_dir() and os.access(private_tmp, os.W_OK):
        return str(private_tmp / "tinyfarm-pycache")
    return str(Path(os.environ.get("TMPDIR", "/tmp")) / "tinyfarm-pycache")


def prepend_path(env: dict[str, str], path: Path) -> None:
    if path.is_dir():
        env["PATH"] = f"{path}{os.pathsep}{env.get('PATH', '')}"


def command_env() -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("PYTHONPYCACHEPREFIX", tmp_pycache_dir())

    emsdk = Path.home() / ".local" / "emsdk"
    if emsdk.is_dir():
        upstream = emsdk / "upstream"
        env.setdefault("EMSDK", str(emsdk))
        env.setdefault("EMSCRIPTEN", str(upstream / "emscripten"))
        env.setdefault("BINARYEN_ROOT", str(upstream))
        env.setdefault("LLVM_ROOT", str(upstream / "bin"))
        if (Path.home() / ".emscripten").exists():
            env.setdefault("EM_CONFIG", str(Path.home() / ".emscripten"))
        prepend_path(env, upstream / "bin")
        prepend_path(env, upstream / "emscripten")

        for node_bin in sorted((emsdk / "node").glob("*/bin"), reverse=True):
            prepend_path(env, node_bin)
        for python_bin in sorted((emsdk / "python").glob("*/bin"), reverse=True):
            prepend_path(env, python_bin)

        if "EMSDK_PYTHON" not in env:
            for candidate in sorted((emsdk / "python").glob("*/bin/python3*"), reverse=True):
                if candidate.name.endswith("-config"):
                    continue
                if candidate.is_file() and os.access(candidate, os.X_OK):
                    env["EMSDK_PYTHON"] = str(candidate)
                    break

    return env


def resolve_executable(name: str, env: dict[str, str]) -> str | None:
    return shutil.which(name, path=env.get("PATH"))


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def read_json(path: Path) -> Any:
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def command_version(command: list[str], env: dict[str, str]) -> str:
    try:
        completed = subprocess.run(
            [str(part) for part in command],
            env=env,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=8,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return f"unavailable: {exc}"
    first_line = completed.stdout.strip().splitlines()
    return first_line[0] if first_line else f"exit {completed.returncode}"


def collect_tool_versions(env: dict[str, str]) -> dict[str, str]:
    tools: dict[str, list[str]] = {
        "python": [sys.executable, "--version"],
        "cmake": ["cmake", "--version"],
        "ninja": ["ninja", "--version"],
        "emcc": ["emcc", "--version"],
        "emcmake": ["emcmake", "--version"],
    }
    return {name: command_version(command, env) for name, command in tools.items()}


def collect_git(root: Path) -> dict[str, Any]:
    def git(args: list[str]) -> str:
        try:
            return subprocess.check_output(["git", *args], cwd=root, text=True, stderr=subprocess.STDOUT).strip()
        except (OSError, subprocess.CalledProcessError) as exc:
            return f"unavailable: {exc}"

    status = git(["status", "--short"])
    status_lines = [] if status.startswith("unavailable:") or not status else status.splitlines()
    return {
        "branch": git(["branch", "--show-current"]),
        "commit": git(["rev-parse", "--short", "HEAD"]),
        "dirty_count": len(status_lines),
        "status_head": status_lines[:80],
    }


def collect_artifacts(build_dir: Path) -> dict[str, Any]:
    artifacts: dict[str, Any] = {}
    for name in ARTIFACTS:
        path = build_dir / name
        if path.exists():
            size = path.stat().st_size
            artifacts[name] = {"bytes": size, "human": human_size(size)}
        else:
            artifacts[name] = {"missing": True}

    package_dir = build_dir / "web-packages"
    packages: dict[str, Any] = {}
    for path in sorted(package_dir.glob("*.tfpack")):
        size = path.stat().st_size
        packages[path.name] = {"bytes": size, "human": human_size(size)}
    if packages:
        artifacts["runtime_packages"] = packages

    index = package_dir / "web-package-index.json"
    if index.exists():
        artifacts["runtime_package_index"] = str(index)
    return artifacts


def release_artifact_paths(build_dir: Path) -> list[tuple[Path, bool]]:
    paths: list[tuple[Path, bool]] = []
    for name in ARTIFACTS:
        paths.append((build_dir / name, True))
    for path in sorted((build_dir / "web-packages").glob("*.tfpack")):
        paths.append((path, True))
    for name in METADATA_ARTIFACTS:
        paths.append((build_dir / name, name.endswith("web-package-index.json")))
    return paths


def artifact_entry(build_dir: Path, path: Path, deploy: bool, env: dict[str, str]) -> dict[str, Any]:
    try:
        relative = path.relative_to(build_dir).as_posix()
    except ValueError:
        relative = path.as_posix()
    kind = artifact_kind(relative)
    if not path.exists():
        return {
            "path": relative,
            "deploy": deploy,
            "kind": kind,
            "missing": True,
        }

    size = path.stat().st_size
    gzip_bytes = gzip_file_bytes(path)
    brotli_bytes = brotli_file_bytes(path, env)
    return {
        "path": relative,
        "deploy": deploy,
        "kind": kind,
        "mime": artifact_mime(path),
        "cache_control": artifact_cache_policy(kind),
        "bytes": size,
        "size": human_size(size),
        "gzip_bytes": gzip_bytes,
        "gzip_size": human_size(gzip_bytes),
        "brotli_bytes": brotli_bytes,
        "brotli_size": human_size(brotli_bytes) if brotli_bytes is not None else None,
        "sha256": sha256_file(path),
    }


def build_artifact_manifest(root: Path, build_dir: Path, output_dir: Path, env: dict[str, str]) -> dict[str, Any]:
    files = [artifact_entry(build_dir, path, deploy, env) for path, deploy in release_artifact_paths(build_dir)]
    present = [file for file in files if not file.get("missing")]
    deploy_files = [file for file in present if file.get("deploy")]
    brotli_available = any(file.get("brotli_bytes") is not None for file in present)

    def total(key: str, rows: list[dict[str, Any]]) -> int | None:
        values = [row.get(key) for row in rows]
        if not values or any(not isinstance(value, int) for value in values):
            return None
        return int(sum(values))

    by_kind: dict[str, dict[str, Any]] = {}
    for file in present:
        kind = str(file.get("kind", "unknown"))
        bucket = by_kind.setdefault(kind, {"files": 0, "bytes": 0, "gzip_bytes": 0, "brotli_bytes": 0})
        bucket["files"] += 1
        bucket["bytes"] += int(file.get("bytes", 0))
        bucket["gzip_bytes"] += int(file.get("gzip_bytes", 0))
        if isinstance(file.get("brotli_bytes"), int):
            bucket["brotli_bytes"] += int(file["brotli_bytes"])
    for bucket in by_kind.values():
        bucket["size"] = human_size(int(bucket["bytes"]))
        bucket["gzip_size"] = human_size(int(bucket["gzip_bytes"]))
        bucket["brotli_size"] = human_size(int(bucket["brotli_bytes"])) if brotli_available else None

    deploy_bytes = total("bytes", deploy_files) or 0
    deploy_gzip_bytes = total("gzip_bytes", deploy_files) or 0
    deploy_brotli_bytes = total("brotli_bytes", deploy_files)
    package_index = read_json(build_dir / "web-packages" / "web-package-index.json")
    return {
        "schema_version": 1,
        "generated_at": now_iso(),
        "repo_root": str(root),
        "build_dir": str(build_dir),
        "output_dir": str(output_dir),
        "git": collect_git(root),
        "deployment": {
            "root": str(build_dir),
            "entry": "TinyFarmRPG-Web.html",
            "required_files": [file["path"] for file in deploy_files],
            "local_preview_headers": {
                "Cache-Control": "no-cache",
                "Cross-Origin-Opener-Policy": "not required for the default single-thread build",
                "Cross-Origin-Embedder-Policy": "not required for the default single-thread build",
            },
            "production_cache_policy": PRODUCTION_CACHE_POLICY,
        },
        "summary": {
            "files": len(present),
            "deploy_files": len(deploy_files),
            "bytes": sum(int(file.get("bytes", 0)) for file in present),
            "size": human_size(sum(int(file.get("bytes", 0)) for file in present)),
            "deploy_bytes": deploy_bytes,
            "deploy_size": human_size(deploy_bytes),
            "deploy_gzip_bytes": deploy_gzip_bytes,
            "deploy_gzip_size": human_size(deploy_gzip_bytes),
            "deploy_brotli_bytes": deploy_brotli_bytes,
            "deploy_brotli_size": human_size(deploy_brotli_bytes) if deploy_brotli_bytes is not None else None,
            "brotli_available": brotli_available,
            "by_kind": by_kind,
        },
        "files": files,
        "runtime_package_index": package_index,
    }


def smoke_screenshots(output_dir: Path) -> list[str]:
    smoke_dir = output_dir / "smoke"
    if not smoke_dir.exists():
        return []
    return [path.relative_to(output_dir).as_posix() for path in sorted(smoke_dir.glob("*.png"))]


def markdown_table(rows: list[list[str]]) -> str:
    if not rows:
        return ""
    header = rows[0]
    separator = ["---"] * len(header)
    lines = [
        "| " + " | ".join(header) + " |",
        "| " + " | ".join(separator) + " |",
    ]
    for row in rows[1:]:
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def table_or_na(rows: list[list[str]]) -> str:
    return markdown_table(rows) if len(rows) > 1 else "n/a"


def bool_text(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "n/a"
    return str(value)


def runtime_package_rows(manifest: dict[str, Any]) -> list[list[str]]:
    package_index = manifest.get("runtime_package_index")
    packages = package_index.get("packages") if isinstance(package_index, dict) else None
    rows = [["Package", "Delivery", "Files", "Bytes", "Artifact", "Dependencies"]]
    if not isinstance(packages, dict):
        return rows
    for package_id in sorted(packages):
        package = packages.get(package_id)
        if not isinstance(package, dict):
            continue
        dependencies = package.get("dependencies")
        rows.append([
            str(package_id),
            str(package.get("delivery", "")),
            str(package.get("files", "n/a")),
            str(package.get("size") or human_size(int(package.get("bytes") or 0))),
            str(package.get("artifact") or package.get("preload_manifest") or "n/a"),
            ", ".join(str(item) for item in dependencies) if isinstance(dependencies, list) and dependencies else "-",
        ])
    return rows


def smoke_gameplay(report: dict[str, Any]) -> dict[str, Any]:
    smoke = report.get("smoke")
    gameplay = smoke.get("gameplay") if isinstance(smoke, dict) else None
    return gameplay if isinstance(gameplay, dict) else {}


def smoke_profile_status(report: dict[str, Any]) -> str:
    smoke = report.get("smoke")
    if not isinstance(smoke, dict) or not smoke:
        return "n/a"
    if smoke.get("skipped"):
        return "skipped"
    return str(smoke.get("profile") or report.get("smoke_profile") or "demo")


def smoke_timing_rows(gameplay: dict[str, Any]) -> list[list[str]]:
    timings = gameplay.get("timings_ms")
    rows = [["Metric", "Actual"]]
    if not isinstance(timings, dict):
        return rows
    for name in sorted(timings):
        rows.append([str(name), f"{timings.get(name, 0)} ms"])
    return rows


def package_response_rows(gameplay: dict[str, Any]) -> list[list[str]]:
    rows = [["Package", "Status", "MIME"]]
    responses = gameplay.get("package_responses")
    if not isinstance(responses, list):
        return rows
    seen: set[str] = set()
    for response in responses:
        if not isinstance(response, dict):
            continue
        url = str(response.get("url", ""))
        package_name = url.rsplit("/", 1)[-1]
        if not package_name.endswith(".tfpack") or package_name in seen:
            continue
        seen.add(package_name)
        rows.append([
            package_name,
            str(response.get("status", "n/a")),
            str(response.get("mimeType", "n/a")),
        ])
    return rows


def diagnostic_package_rows(gameplay: dict[str, Any]) -> list[list[str]]:
    rows = [["Package", "Files", "Ready time", "Source"]]
    load_events = gameplay.get("package_load_events")
    latest = load_events.get("latest_by_package") if isinstance(load_events, dict) else None
    if isinstance(latest, dict) and latest:
        for package_id in sorted(latest):
            event = latest.get(package_id)
            if not isinstance(event, dict):
                continue
            rows.append([
                str(package_id),
                str(event.get("files", "n/a")),
                f"{event.get('ready_ms', 'n/a')} ms",
                "smoke log",
            ])
        return rows

    rows = [["Package", "Loaded", "Attempts", "Last load", "Last error"]]
    diagnostics = gameplay.get("diagnostics")
    packages = diagnostics.get("packages") if isinstance(diagnostics, dict) else None
    if not isinstance(packages, dict):
        return rows
    for package_id in sorted(packages):
        package = packages.get(package_id)
        if not isinstance(package, dict):
            continue
        rows.append([
            str(package_id),
            bool_text(package.get("loaded")),
            str(package.get("attempts", "n/a")),
            f"{package.get('lastLoadMs', 'n/a')} ms",
            str(package.get("lastError") or "-"),
        ])
    return rows


def render_rows(gameplay: dict[str, Any]) -> list[list[str]]:
    diagnostics = gameplay.get("diagnostics")
    capabilities = gameplay.get("render_capabilities")
    if not isinstance(capabilities, dict) and isinstance(diagnostics, dict):
        capabilities = diagnostics.get("renderCapabilities")
    rows = [["Capability", "Value"]]
    if not isinstance(capabilities, dict):
        return rows
    for key in (
        "platform",
        "floatColorFramebuffers",
        "rgba16fColorRenderable",
        "linearFloatFiltering",
        "hdrPostProcessing",
        "emissive",
        "bloom",
        "hdrFallbackReason",
        "emissiveFallbackReason",
        "bloomFallbackReason",
        "emissiveDrawCalls",
        "emissiveSprites",
        "bloomDrawCalls",
        "bloomLevels",
    ):
        value = capabilities.get(key)
        rows.append([key, bool_text(value) if value not in {"", None} else "-"])
    return rows


def vfx_rows(gameplay: dict[str, Any]) -> list[list[str]]:
    rows = [["Field", "Value"]]
    policy = gameplay.get("vfx_policy")
    diagnostics = gameplay.get("diagnostics")
    vfx = diagnostics.get("vfx") if isinstance(diagnostics, dict) else None
    if isinstance(policy, dict):
        rows.append(["policy_backend", str(policy.get("backend", "n/a"))])
        rows.append(["policy_effekseer", bool_text(policy.get("effekseer"))])
        rows.append(["policy_log", str(policy.get("log", ""))])
    if isinstance(vfx, dict):
        for key in ("backend", "status", "effekseerEnabled", "lastDrawCallCount", "lastInstanceCount"):
            rows.append([key, bool_text(vfx.get(key))])
    return rows


def gameplay_coverage_lines(gameplay: dict[str, Any]) -> list[str]:
    lines: list[str] = []
    covered = gameplay.get("covered_flows")
    if isinstance(covered, list) and covered:
        lines.append("Covered flows:")
        lines.extend(f"- `{item}`" for item in covered)
    full_profile = gameplay.get("full_rpg_profile")
    if isinstance(full_profile, dict):
        lines.append("")
        lines.append(f"Full RPG profile status: `{full_profile.get('status', 'n/a')}`")
        pending = full_profile.get("pending_flows")
        if isinstance(pending, list) and pending:
            lines.append("")
            lines.append("Uncovered non-basic flows:")
            lines.extend(f"- `{item}`" for item in pending)
    return lines or ["n/a"]


def write_release_markdown(path: Path, report: dict[str, Any], manifest: dict[str, Any]) -> None:
    summary = manifest.get("summary", {})
    release_gate = report.get("release_gate") or {}
    smoke = report.get("smoke") or {}
    gameplay = smoke_gameplay(report)
    if not isinstance(smoke, dict) or not smoke:
        smoke_status = "n/a"
    elif smoke.get("skipped"):
        smoke_status = "skipped"
    else:
        smoke_status = str(smoke.get("status", "passed"))
    files = [file for file in manifest.get("files", []) if not file.get("missing") and file.get("deploy")]
    artifact_rows = [["Path", "Kind", "Size", "Gzip", "Brotli", "MIME"]]
    for file in files:
        artifact_rows.append([
            str(file.get("path", "")),
            str(file.get("kind", "")),
            str(file.get("size", "")),
            str(file.get("gzip_size", "")),
            str(file.get("brotli_size") or "n/a"),
            str(file.get("mime", "")),
        ])

    timing_rows = [["Metric", "Actual", "Budget", "Status"]]
    budget = gameplay.get("performance_budget", {}) if isinstance(gameplay, dict) else {}
    for name, result in (budget.get("results", {}) if isinstance(budget, dict) else {}).items():
        timing_rows.append([
            str(name),
            f"{result.get('actual_ms', 0)} ms",
            f"{result.get('budget_ms', 0)} ms",
            str(result.get("status", "")),
        ])

    screenshots = smoke_screenshots(Path(str(report.get("output_dir", ""))))
    screenshot_lines = "\n".join(f"- `{item}`" for item in screenshots) if screenshots else "- n/a"
    checklist_lines = "\n".join(f"- {item}" for item in MANUAL_CHECKLIST)
    lines = [
        "# TinyFarmRPG Web Release Report",
        "",
        f"- Status: `{report.get('status', '<unknown>')}`",
        f"- Generated at: `{manifest.get('generated_at', '')}`",
        f"- Build dir: `{manifest.get('build_dir', '')}`",
        f"- Deploy entry: `{manifest.get('deployment', {}).get('entry', '')}`",
        f"- Release gate failures: `{len(release_gate.get('failures', [])) if isinstance(release_gate, dict) else 'n/a'}`",
        f"- Smoke status: `{smoke_status}`",
        f"- Smoke profile: `{smoke_profile_status(report)}`",
        "",
        "## Size Summary",
        "",
        f"- Deploy files: `{summary.get('deploy_files', 0)}`",
        f"- Deploy size: `{summary.get('deploy_size', '0 B')}`",
        f"- Deploy gzip size: `{summary.get('deploy_gzip_size', '0 B')}`",
        f"- Deploy brotli size: `{summary.get('deploy_brotli_size') or 'n/a'}`",
        "",
        "## Deployment Files",
        "",
        markdown_table(artifact_rows),
        "",
        "## Runtime Package Index",
        "",
        table_or_na(runtime_package_rows(manifest)),
        "",
        "## Runtime Package Responses",
        "",
        table_or_na(package_response_rows(gameplay)),
        "",
        "## Runtime Package Load Diagnostics",
        "",
        table_or_na(diagnostic_package_rows(gameplay)),
        "",
        "## Performance Budget",
        "",
        markdown_table(timing_rows) if len(timing_rows) > 1 else "n/a",
        "",
        "## Smoke Timings",
        "",
        table_or_na(smoke_timing_rows(gameplay)),
        "",
        "## Render Capabilities",
        "",
        table_or_na(render_rows(gameplay)),
        "",
        "## VFX Policy",
        "",
        table_or_na(vfx_rows(gameplay)),
        "",
        "## Gameplay Coverage",
        "",
        "\n".join(gameplay_coverage_lines(gameplay)),
        "",
        "## Manual Checklist",
        "",
        checklist_lines,
        "",
        "## Screenshots",
        "",
        screenshot_lines,
        "",
        "## Commands",
        "",
    ]
    for command in report.get("commands", []):
        lines.append("- `" + quote_command(command.get("command", [])) + f"` -> `{command.get('exit_code')}`")
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def write_release_outputs(report: dict[str, Any], root: Path, build_dir: Path, output_dir: Path, env: dict[str, str]) -> None:
    artifact_manifest_path = output_dir / "artifact-manifest.json"
    release_report_path = output_dir / "release-report.md"
    artifact_manifest = build_artifact_manifest(root, build_dir, output_dir, env)
    write_json(artifact_manifest_path, artifact_manifest)
    report["artifact_manifest"] = str(artifact_manifest_path)
    report["release_report"] = str(release_report_path)
    report["artifact_summary"] = artifact_manifest.get("summary", {})
    write_release_markdown(release_report_path, report, artifact_manifest)


def try_write_release_outputs(
    report: dict[str, Any],
    root: Path,
    build_dir: Path,
    output_dir: Path,
    env: dict[str, str],
) -> bool:
    try:
        write_release_outputs(report, root, build_dir, output_dir, env)
    except Exception as exc:
        report["release_output_error"] = str(exc)
        return False
    return True


def mac_app_version(executable: Path) -> str | None:
    parts = executable.resolve().parts
    for index, part in enumerate(parts):
        if part.endswith(".app"):
            app_path = Path(*parts[: index + 1])
            plist_path = app_path / "Contents" / "Info.plist"
            if not plist_path.exists():
                return None
            try:
                with plist_path.open("rb") as handle:
                    info = plistlib.load(handle)
            except (OSError, plistlib.InvalidFileException):
                return None
            name = str(info.get("CFBundleName") or app_path.stem)
            version = info.get("CFBundleShortVersionString") or info.get("CFBundleVersion")
            return f"{name} {version}" if version else name
    return None


def browser_version(browser: Path, env: dict[str, str]) -> str:
    app_version = mac_app_version(browser)
    if app_version is not None:
        return app_version
    return command_version([str(browser), "--version"], env)


def browser_candidates(headless: bool) -> list[Path]:
    candidates: list[Path] = []
    mac_chrome = Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
    mac_edge = Path("/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge")
    mac_chromium = Path("/Applications/Chromium.app/Contents/MacOS/Chromium")

    if not headless:
        candidates.extend([mac_chrome, mac_edge, mac_chromium])

    for executable in (
        "chrome-headless-shell",
        "chromium",
        "google-chrome",
        "google-chrome-stable",
        "msedge",
    ):
        resolved = shutil.which(executable)
        if resolved:
            candidates.append(Path(resolved))

    candidates.extend([mac_chrome, mac_edge, mac_chromium])
    seen: set[Path] = set()
    result: list[Path] = []
    for candidate in candidates:
        if candidate.exists() and candidate not in seen:
            result.append(candidate)
            seen.add(candidate)
    return result


def default_browser(headless: bool) -> Path:
    candidates = browser_candidates(headless)
    if not candidates:
        raise RuntimeError("No Chromium-family browser found. Pass --browser explicitly.")
    return candidates[0]


class CommandRunner:
    def __init__(self, root: Path, log_path: Path) -> None:
        self.root = root
        self.log_path = log_path
        self.commands: list[dict[str, Any]] = []
        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        self._log = self.log_path.open("w", encoding="utf-8")
        self.note(f"Log started at {now_iso()}")

    def close(self) -> None:
        self.note(f"Log finished at {now_iso()}")
        self._log.close()

    def note(self, message: str) -> None:
        print(message)
        self._log.write(message + "\n")
        self._log.flush()

    def run(self, command: list[str], env: dict[str, str]) -> None:
        command = [str(part) for part in command]
        started = time.monotonic()
        entry: dict[str, Any] = {
            "command": command,
            "cwd": str(self.root),
            "started_at": now_iso(),
        }
        self.note("")
        self.note("+ " + quote_command(command))
        process = subprocess.Popen(
            command,
            cwd=self.root,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        tail: list[str] = []
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            self._log.write(line)
            tail.append(line.rstrip("\n"))
            if len(tail) > 200:
                tail = tail[-200:]
        exit_code = process.wait()
        duration = round(time.monotonic() - started, 3)
        entry.update(
            {
                "finished_at": now_iso(),
                "duration_seconds": duration,
                "exit_code": exit_code,
                "output_tail": tail,
            }
        )
        self.commands.append(entry)
        self._log.flush()
        if exit_code != 0:
            raise RuntimeError(f"Command failed with exit code {exit_code}: {quote_command(command)}")


def run_script_check(root: Path, runner: CommandRunner, env: dict[str, str]) -> None:
    runner.run([sys.executable, "-m", "py_compile", *[str(root / path) for path in SCRIPT_CHECKS]], env)


def run_asset_audit(root: Path, runner: CommandRunner, env: dict[str, str]) -> None:
    runner.run([sys.executable, str(root / "tools" / "asset_audit" / "audit_assets.py")], env)


def cmake_on_off(value: bool) -> str:
    return "ON" if value else "OFF"


def configure_web(
    root: Path,
    build_dir: Path,
    runner: CommandRunner,
    env: dict[str, str],
    *,
    build_type: str = "Release",
    enable_debug_ui: bool = False,
    enable_rmlui_debugger: bool = False,
) -> None:
    emcmake = resolve_executable("emcmake", env)
    if not emcmake:
        raise RuntimeError("emcmake not found. Source emsdk_env.sh or install emsdk before using --configure.")
    runner.run(
        [
            emcmake,
            "cmake",
            "-S",
            str(root),
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-DTF_BUILD_WEB=ON",
            "-DTF_WEB_ENABLE_RUNTIME_PACKAGES=ON",
            "-DTF_WEB_BOOT_ONLY_PRELOAD=ON",
            "-DTF_WEB_ENABLE_PTHREADS=OFF",
            "-DTF_ENABLE_RUNTIME_THREADS=OFF",
            f"-DENABLE_DEBUG_UI={cmake_on_off(enable_debug_ui)}",
            f"-DENABLE_RMLUI_DEBUGGER={cmake_on_off(enable_rmlui_debugger)}",
            "-DBUILD_TOOLS=OFF",
            "-DBUILD_TESTING=OFF",
            "-DBUILD_LEARN=OFF",
        ],
        env,
    )


def build_web(root: Path, build_dir: Path, jobs: int, runner: CommandRunner, env: dict[str, str]) -> None:
    runner.run(["cmake", "--build", str(build_dir), "-j", str(jobs)], env)


def package_web_assets(root: Path, build_dir: Path, runner: CommandRunner, env: dict[str, str]) -> None:
    runner.run(
        [
            sys.executable,
            str(root / "tools" / "web_release" / "package_web_assets.py"),
            "--manifest",
            str(root / "manifests" / "assets" / "web-release-full.args"),
            "--output-dir",
            str(build_dir / "web-packages"),
            "--boot-preload-output",
            str(build_dir / "web-boot-preload.args"),
            "--json-output",
            str(build_dir / "web-packages" / "web-package-index.json"),
        ],
        env,
    )


def validate_web(root: Path, build_dir: Path, output: Path, runner: CommandRunner, env: dict[str, str]) -> None:
    runner.run(
        [
            sys.executable,
            str(root / "tools" / "web_release" / "validate_web_release.py"),
            "--build-dir",
            str(build_dir),
            "--json-output",
            str(output),
        ],
        env,
    )


def base_report(mode: str, root: Path, build_dir: Path, output_dir: Path, env: dict[str, str]) -> dict[str, Any]:
    return {
        "status": "running",
        "mode": mode,
        "started_at": now_iso(),
        "repo_root": str(root),
        "build_dir": str(build_dir),
        "output_dir": str(output_dir),
        "platform": {
            "system": platform.platform(),
            "machine": platform.machine(),
            "python": sys.version,
        },
        "environment": {
            "PYTHONPYCACHEPREFIX": env.get("PYTHONPYCACHEPREFIX"),
            "EMSDK": env.get("EMSDK"),
            "EMSDK_PYTHON": env.get("EMSDK_PYTHON"),
            "EM_CONFIG": env.get("EM_CONFIG"),
        },
        "tool_versions": collect_tool_versions(env),
        "git": collect_git(root),
    }


def finish_report(
    report: dict[str, Any],
    status: str,
    runner: CommandRunner,
    build_dir: Path,
    error: str | None = None,
) -> dict[str, Any]:
    report["status"] = status
    report["finished_at"] = now_iso()
    report["commands"] = runner.commands
    report["artifacts"] = collect_artifacts(build_dir)
    if error is not None:
        report["error"] = error
    return report


def auto_output_dir(args: argparse.Namespace, build_dir: Path) -> Path:
    return (args.output_dir or build_dir / "web-release-auto").resolve()


def run_auto(args: argparse.Namespace) -> int:
    root = repo_root()
    build_dir = (args.build_dir or default_build_dir(root)).resolve()
    output_dir = auto_output_dir(args, build_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / "auto-check.json"
    log_path = output_dir / "auto-check.log"
    smoke_dir = output_dir / "smoke"
    smoke_json = output_dir / "chromium-smoke.json"
    gate_json = output_dir / "release-gate.json"
    env = command_env()

    if args.configure and args.skip_build and not args.skip_smoke:
        raise SystemExit("auto: --configure already builds through web_smoke.py; do not combine it with --skip-build.")

    browser = None if args.skip_smoke else (args.browser.resolve() if args.browser else default_browser(args.headless))
    runner = CommandRunner(root, log_path)
    report = base_report("auto", root, build_dir, output_dir, env)
    report["smoke_profile"] = args.smoke_profile
    if browser is not None:
        report["browser"] = {
            "path": str(browser),
            "version": browser_version(browser, env),
            "mode": "headless" if args.headless else "headed",
        }
    else:
        report["browser"] = {"skipped": True}
    report["log"] = str(log_path)

    exit_code = 0
    try:
        if not args.skip_script_check:
            run_script_check(root, runner, env)
        if not args.skip_build:
            run_asset_audit(root, runner, env)

        if args.skip_smoke:
            if args.configure:
                configure_web(root, build_dir, runner, env)
            if not args.skip_build:
                build_web(root, build_dir, args.jobs, runner, env)
                package_web_assets(root, build_dir, runner, env)
            if not args.skip_gate:
                validate_web(root, build_dir, gate_json, runner, env)
        else:
            command = [
                sys.executable,
                str(root / "tools" / "web_release" / "web_smoke.py"),
                "--build-dir",
                str(build_dir),
                "--jobs",
                str(args.jobs),
                "--host",
                args.host,
                "--port",
                str(args.port),
                "--browser",
                str(browser),
                "--output-dir",
                str(smoke_dir),
                "--json-output",
                str(smoke_json),
                "--profile",
                args.smoke_profile,
            ]
            if args.configure:
                command.append("--configure")
            elif args.skip_build:
                command.append("--skip-build")
            if args.skip_gate:
                command.append("--skip-gate")
            if not args.headless:
                command.append("--headed")
            runner.run(command, env)

        if args.skip_gate:
            report["release_gate"] = {"skipped": True}
        else:
            report["release_gate"] = read_json(gate_json) or read_json(smoke_dir / "release-gate.json")
        report["smoke"] = {"skipped": True} if args.skip_smoke else read_json(smoke_json)
        finish_report(report, "passed", runner, build_dir)
    except Exception as exc:
        exit_code = 1
        finish_report(report, "failed", runner, build_dir, str(exc))
    finally:
        runner.close()
        if not try_write_release_outputs(report, root, build_dir, output_dir, env):
            exit_code = 1
            if report.get("status") == "passed":
                report["status"] = "failed"
        write_json(report_path, report)

    print("")
    print(f"Auto check {report['status']}: {report_path}")
    if "artifact_manifest" in report:
        print(f"Artifact manifest: {report['artifact_manifest']}")
    if "release_report" in report:
        print(f"Release report: {report['release_report']}")
    print(f"Log: {log_path}")
    return exit_code


def manual_output_dir(args: argparse.Namespace, build_dir: Path) -> Path:
    return (args.output_dir or build_dir / "web-release-manual").resolve()


def required_artifacts_present(build_dir: Path) -> None:
    missing = [name for name in ARTIFACTS if not (build_dir / name).exists()]
    if missing:
        raise RuntimeError(f"Web artifacts missing in {build_dir}: {', '.join(missing)}")


def run_manual(args: argparse.Namespace) -> int:
    root = repo_root()
    build_dir = (args.build_dir or default_build_dir(root)).resolve()
    output_dir = manual_output_dir(args, build_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / "manual-preview.json"
    log_path = output_dir / "manual-preview.log"
    gate_json = output_dir / "release-gate.json"
    env = command_env()
    runner = CommandRunner(root, log_path)
    report = base_report("manual", root, build_dir, output_dir, env)
    report["log"] = str(log_path)
    report["checklist"] = list(MANUAL_CHECKLIST)

    try:
        if not args.skip_script_check:
            run_script_check(root, runner, env)
        if not args.skip_build:
            run_asset_audit(root, runner, env)
        if args.configure:
            configure_web(root, build_dir, runner, env)
        if not args.skip_build:
            build_web(root, build_dir, args.jobs, runner, env)
            package_web_assets(root, build_dir, runner, env)
        if not args.skip_gate:
            validate_web(root, build_dir, gate_json, runner, env)
        required_artifacts_present(build_dir)

        cache_path = build_dir / "CMakeCache.txt"
        cross_origin_isolated = args.cross_origin_isolated or cmake_bool(cache_value(cache_path, "TF_WEB_ENABLE_PTHREADS"))
        port = args.port if args.port != 0 else find_free_port()
        display_host = "127.0.0.1" if args.host in {"0.0.0.0", "::"} else args.host
        url = f"http://{display_host}:{port}/TinyFarmRPG-Web.html?manual={int(time.time())}"
        report["server"] = {
            "url": url,
            "host": args.host,
            "port": port,
            "cross_origin_isolated": cross_origin_isolated,
        }
        report["release_gate"] = read_json(gate_json)
        finish_report(report, "prepared", runner, build_dir)
        if not try_write_release_outputs(report, root, build_dir, output_dir, env):
            report["status"] = "failed"
        write_json(report_path, report)
        if report["status"] == "failed":
            runner.close()
            print("")
            print(f"Manual preview failed: {report_path}")
            print(f"Log: {log_path}")
            return 1

        if args.check_only:
            runner.note(f"Manual preview prepared: {url}")
            runner.close()
            print("")
            print(f"Manual preview report: {report_path}")
            if "artifact_manifest" in report:
                print(f"Artifact manifest: {report['artifact_manifest']}")
            if "release_report" in report:
                print(f"Release report: {report['release_report']}")
            print(f"Log: {log_path}")
            return 0 if report["status"] == "prepared" else 1

        handler_base = make_handler(build_dir, cross_origin_isolated)

        class LoggingHandler(handler_base):
            def log_message(self, fmt: str, *values: object) -> None:
                runner.note("HTTP " + (fmt % values))

        socketserver.TCPServer.allow_reuse_address = True
        with socketserver.TCPServer((args.host, port), LoggingHandler) as httpd:
            report["status"] = "serving"
            write_json(report_path, report)
            runner.note("")
            runner.note(f"Serving {build_dir}")
            runner.note(f"Open {url}")
            runner.note("Manual checklist:")
            for item in MANUAL_CHECKLIST:
                runner.note(f"- {item}")
            runner.note(f"Report: {report_path}")
            runner.note(f"Log: {log_path}")
            if args.open:
                webbrowser.open(url)
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                runner.note("Stopped.")
            report["status"] = "stopped"
            report["finished_at"] = now_iso()
            write_json(report_path, report)
    except Exception as exc:
        finish_report(report, "failed", runner, build_dir, str(exc))
        try_write_release_outputs(report, root, build_dir, output_dir, env)
        write_json(report_path, report)
        runner.close()
        print("")
        print(f"Manual preview failed: {report_path}")
        print(f"Log: {log_path}")
        return 1

    runner.close()
    print("")
    print(f"Manual preview report: {report_path}")
    if "artifact_manifest" in report:
        print(f"Artifact manifest: {report['artifact_manifest']}")
    if "release_report" in report:
        print(f"Release report: {report['release_report']}")
    print(f"Log: {log_path}")
    return 0


def debug_output_dir(args: argparse.Namespace, build_dir: Path) -> Path:
    return (args.output_dir or build_dir / "web-debug-manual").resolve()


def run_debug(args: argparse.Namespace) -> int:
    root = repo_root()
    build_dir = (args.build_dir or root / "build" / "web-debug").resolve()
    output_dir = debug_output_dir(args, build_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / "web-debug-preview.json"
    log_path = output_dir / "web-debug-preview.log"
    env = command_env()
    runner = CommandRunner(root, log_path)
    report = base_report("debug", root, build_dir, output_dir, env)
    report["log"] = str(log_path)
    report["checklist"] = list(DEBUG_CHECKLIST)
    report["release_gate"] = {
        "skipped": True,
        "reason": "Web debug UI builds intentionally use Release optimization with ENABLE_DEBUG_UI=ON.",
    }

    try:
        if not args.skip_script_check:
            run_script_check(root, runner, env)
        if not args.skip_build:
            run_asset_audit(root, runner, env)
        if args.configure:
            configure_web(
                root,
                build_dir,
                runner,
                env,
                build_type="Release",
                enable_debug_ui=True,
                enable_rmlui_debugger=False,
            )
        if not args.skip_build:
            build_web(root, build_dir, args.jobs, runner, env)
            package_web_assets(root, build_dir, runner, env)
        required_artifacts_present(build_dir)

        cache_path = build_dir / "CMakeCache.txt"
        cross_origin_isolated = args.cross_origin_isolated or cmake_bool(cache_value(cache_path, "TF_WEB_ENABLE_PTHREADS"))
        port = args.port if args.port != 0 else find_free_port()
        display_host = "127.0.0.1" if args.host in {"0.0.0.0", "::"} else args.host
        query = f"debug={int(time.time())}"
        if args.debug_ui_start != "none":
            query = f"debug-ui={args.debug_ui_start}&{query}"
        url = f"http://{display_host}:{port}/TinyFarmRPG-Web.html?{query}"
        report["server"] = {
            "url": url,
            "host": args.host,
            "port": port,
            "cross_origin_isolated": cross_origin_isolated,
        }
        finish_report(report, "prepared", runner, build_dir)
        if not try_write_release_outputs(report, root, build_dir, output_dir, env):
            report["status"] = "failed"
        write_json(report_path, report)
        if report["status"] == "failed":
            runner.close()
            print("")
            print(f"Web debug preview failed: {report_path}")
            print(f"Log: {log_path}")
            return 1

        if args.check_only:
            runner.note(f"Web debug preview prepared: {url}")
            runner.close()
            print("")
            print(f"Web debug preview report: {report_path}")
            if "artifact_manifest" in report:
                print(f"Artifact manifest: {report['artifact_manifest']}")
            if "release_report" in report:
                print(f"Release report: {report['release_report']}")
            print(f"Log: {log_path}")
            return 0 if report["status"] == "prepared" else 1

        handler_base = make_handler(build_dir, cross_origin_isolated)

        class LoggingHandler(handler_base):
            def log_message(self, fmt: str, *values: object) -> None:
                runner.note("HTTP " + (fmt % values))

        socketserver.TCPServer.allow_reuse_address = True
        with socketserver.TCPServer((args.host, port), LoggingHandler) as httpd:
            report["status"] = "serving"
            write_json(report_path, report)
            runner.note("")
            runner.note(f"Serving Web debug build {build_dir}")
            runner.note(f"Open {url}")
            runner.note("Web debug checklist:")
            for item in DEBUG_CHECKLIST:
                runner.note(f"- {item}")
            runner.note(f"Report: {report_path}")
            runner.note(f"Log: {log_path}")
            if args.open:
                webbrowser.open(url)
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                runner.note("Stopped.")
            report["status"] = "stopped"
            report["finished_at"] = now_iso()
            write_json(report_path, report)
    except Exception as exc:
        finish_report(report, "failed", runner, build_dir, str(exc))
        try_write_release_outputs(report, root, build_dir, output_dir, env)
        write_json(report_path, report)
        runner.close()
        print("")
        print(f"Web debug preview failed: {report_path}")
        print(f"Log: {log_path}")
        return 1

    runner.close()
    print("")
    print(f"Web debug preview report: {report_path}")
    if "artifact_manifest" in report:
        print(f"Artifact manifest: {report['artifact_manifest']}")
    if "release_report" in report:
        print(f"Release report: {report['release_report']}")
    print(f"Log: {log_path}")
    return 0


def add_common_build_args(
    parser: argparse.ArgumentParser,
    *,
    include_gate: bool = True,
    build_dir_help: str = "Build directory. Defaults to existing build/web-gameplay-phase11, otherwise build/web-release.",
) -> None:
    parser.add_argument(
        "--build-dir",
        type=Path,
        help=build_dir_help,
    )
    parser.add_argument("--configure", action="store_true", help="Run emcmake CMake configure before building.")
    parser.add_argument("--skip-build", action="store_true", help="Skip cmake --build.")
    if include_gate:
        parser.add_argument("--skip-gate", action="store_true", help="Skip validate_web_release.py.")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--skip-script-check", action="store_true", help="Skip Python py_compile checks.")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="TinyFarmRPG Web release runbook.")
    subparsers = parser.add_subparsers(dest="mode", required=True)

    auto = subparsers.add_parser("auto", help="Run Python checks, Web build/gate, server, and Chrome smoke.")
    add_common_build_args(auto)
    auto.add_argument("--skip-smoke", action="store_true", help="Only run checks/build/gate; do not launch Chrome.")
    auto.add_argument("--headless", action="store_true", help="Use headless Chromium. Default is headed Chrome when available.")
    auto.add_argument("--browser", type=Path, help="Chromium-family browser executable.")
    auto.add_argument(
        "--profile",
        "--smoke-profile",
        dest="smoke_profile",
        choices=("demo", "full-rpg"),
        default="demo",
        help="Smoke profile: demo is quick; full-rpg is the complete Web migration acceptance profile.",
    )
    auto.add_argument("--host", default="127.0.0.1")
    auto.add_argument("--port", type=int, default=0)
    auto.set_defaults(func=run_auto)

    manual = subparsers.add_parser("manual", help="Build/gate and serve artifacts for manual browser testing.")
    add_common_build_args(manual)
    manual.add_argument("--host", default="127.0.0.1")
    manual.add_argument("--port", type=int, default=8787)
    manual.add_argument("--cross-origin-isolated", action="store_true", help="Force COOP/COEP preview headers.")
    manual.add_argument("--open", action="store_true", help="Open the preview URL in the default browser.")
    manual.add_argument("--check-only", action="store_true", help="Prepare and record the preview without starting the server.")
    manual.set_defaults(func=run_manual)

    debug = subparsers.add_parser("debug", help="Build and serve a Web debug preview with ImGui Debug UI enabled.")
    add_common_build_args(debug, include_gate=False, build_dir_help="Build directory. Defaults to build/web-debug.")
    debug.add_argument("--host", default="127.0.0.1")
    debug.add_argument("--port", type=int, default=8787)
    debug.add_argument("--cross-origin-isolated", action="store_true", help="Force COOP/COEP preview headers.")
    debug.add_argument("--open", action="store_true", help="Open the preview URL in the default browser.")
    debug.add_argument("--check-only", action="store_true", help="Prepare and record the preview without starting the server.")
    debug.add_argument(
        "--debug-ui-start",
        choices=("none", "engine", "game", "all"),
        default="none",
        help="Add a debug-ui query parameter so selected ImGui hubs start visible.",
    )
    debug.set_defaults(func=run_debug)

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
