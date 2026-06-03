#!/usr/bin/env python3
"""Repeatable Web release automation and manual-preview entry points."""

from __future__ import annotations

import argparse
import datetime as dt
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
    "Network shows shared-ui.tfpack, audio-core.tfpack, and home-map.tfpack fetched on demand.",
    "Move the player, save slot0, reload the page, then Load slot0 back into home_exterior.",
    "Confirm TinyFarmRPG-Web.data stays boot-only sized and no COOP/COEP headers are required for single-thread builds.",
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


def configure_web(root: Path, build_dir: Path, runner: CommandRunner, env: dict[str, str]) -> None:
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
            "-DCMAKE_BUILD_TYPE=Release",
            "-DTF_BUILD_WEB=ON",
            "-DBUILD_TOOLS=OFF",
            "-DBUILD_TESTING=OFF",
            "-DBUILD_LEARN=OFF",
        ],
        env,
    )


def build_web(root: Path, build_dir: Path, jobs: int, runner: CommandRunner, env: dict[str, str]) -> None:
    runner.run(["cmake", "--build", str(build_dir), "-j", str(jobs)], env)


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

        if args.skip_smoke:
            if args.configure:
                configure_web(root, build_dir, runner, env)
            if not args.skip_build:
                build_web(root, build_dir, args.jobs, runner, env)
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
        write_json(report_path, report)

    print("")
    print(f"Auto check {report['status']}: {report_path}")
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
        if args.configure:
            configure_web(root, build_dir, runner, env)
        if not args.skip_build:
            build_web(root, build_dir, args.jobs, runner, env)
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
        write_json(report_path, report)

        if args.check_only:
            runner.note(f"Manual preview prepared: {url}")
            runner.close()
            print("")
            print(f"Manual preview report: {report_path}")
            print(f"Log: {log_path}")
            return 0

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
        write_json(report_path, report)
        runner.close()
        print("")
        print(f"Manual preview failed: {report_path}")
        print(f"Log: {log_path}")
        return 1

    runner.close()
    print("")
    print(f"Manual preview report: {report_path}")
    print(f"Log: {log_path}")
    return 0


def add_common_build_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Build directory. Defaults to existing build/web-gameplay-phase11, otherwise build/web-release.",
    )
    parser.add_argument("--configure", action="store_true", help="Run emcmake CMake configure before building.")
    parser.add_argument("--skip-build", action="store_true", help="Skip cmake --build.")
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

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
