#!/usr/bin/env python3
"""Run the TinyFarmRPG Web release gate and Chromium gameplay smoke."""

from __future__ import annotations

import argparse
import base64
import http.client
import json
import os
import plistlib
import shutil
import socket
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from serve_web_release import cache_value, cmake_bool, make_handler


LOGICAL_WIDTH = 640.0
LOGICAL_HEIGHT = 360.0
SAVE_PATH = "/persistent/saves/slot0.json"
CORRUPT_SAVE_PATH = "/persistent/saves/slot1.json"
USER_SETTINGS_PATH = "/persistent/config/user_settings.json"
KEY_ESCAPE = ("Escape", "Escape", 27)
KEY_INTERACT = ("KeyF", "f", 70)
KEY_INVENTORY = ("KeyI", "i", 73)
KEY_HOTBAR = ("Tab", "Tab", 9)
KEY_PAUSE = ("KeyP", "p", 80)
KEY_HOTBAR_1 = ("Digit1", "1", 49)
KEY_MENU_CONFIRM = ("Enter", "Enter", 13)
KEY_MENU_UP = ("KeyW", "w", 87)
KEY_MENU_DOWN = ("KeyS", "s", 83)
KEY_MENU_LEFT = ("KeyA", "a", 65)
KEY_MENU_RIGHT = ("KeyD", "d", 68)

DEMO_RUNTIME_PACKAGE_IDS = (
    "audio-core",
    "shared-ui",
    "rpg-core",
    "home-map",
)
FULL_RPG_RUNTIME_PACKAGE_IDS = (
    "audio-core",
    "shared-ui",
    "rpg-core",
    "home-map",
    "town-map",
    "battle-core",
    "vfx-core",
)

PERFORMANCE_BUDGET_MS = {
    "title_interactive": 45000,
    "new_game_to_map": 30000,
    "gameplay_flow": 120000,
    "reload_load_to_map": 30000,
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def now_ms() -> int:
    return int(time.monotonic() * 1000)


def human_ms(start: int, end: int) -> int:
    return max(0, end - start)


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def run_command(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, env=env, check=True)


def configure_web_build(root: Path, build_dir: Path, jobs: int) -> None:
    emcmake = shutil.which("emcmake")
    if not emcmake:
        raise RuntimeError("emcmake not found. Source emsdk_env.sh before running --configure.")
    run_command(
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
        root,
    )
    build_web(root, build_dir, jobs)


def build_web(root: Path, build_dir: Path, jobs: int) -> None:
    run_command(["cmake", "--build", str(build_dir), "-j", str(jobs)], root)


def package_web_assets(root: Path, build_dir: Path) -> None:
    package_dir = build_dir / "web-packages"
    run_command(
        [
            sys.executable,
            str(root / "tools" / "web_release" / "package_web_assets.py"),
            "--manifest",
            str(root / "manifests" / "assets" / "web-release-full.args"),
            "--output-dir",
            str(package_dir),
            "--boot-preload-output",
            str(build_dir / "web-boot-preload.args"),
            "--json-output",
            str(package_dir / "web-package-index.json"),
        ],
        root,
    )


def validate_web(root: Path, build_dir: Path, json_output: Path) -> None:
    run_command(
        [
            sys.executable,
            str(root / "tools" / "web_release" / "validate_web_release.py"),
            "--build-dir",
            str(build_dir),
            "--json-output",
            str(json_output),
        ],
        root,
    )


def browser_candidates() -> list[Path]:
    candidates: list[Path] = []
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

    candidates.extend(
        [
            Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"),
            Path("/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"),
            Path("/Applications/Chromium.app/Contents/MacOS/Chromium"),
        ]
    )
    return [candidate for candidate in candidates if candidate.exists()]


def default_browser() -> Path:
    candidates = browser_candidates()
    if not candidates:
        raise RuntimeError("No Chromium-family browser found for CDP smoke.")
    return candidates[0]


class WebSocket:
    def __init__(self, url: str, timeout: float = 5.0) -> None:
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != "ws":
            raise RuntimeError(f"Only ws:// CDP endpoints are supported: {url}")
        self.host = parsed.hostname or "127.0.0.1"
        self.port = parsed.port or 80
        path = parsed.path or "/"
        if parsed.query:
            path += "?" + parsed.query

        self.sock = socket.create_connection((self.host, self.port), timeout=timeout)
        self.sock.settimeout(timeout)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        request = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {self.host}:{self.port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(request.encode("ascii"))
        response = self._read_until(b"\r\n\r\n")
        if b" 101 " not in response.splitlines()[0]:
            raise RuntimeError(f"CDP websocket handshake failed: {response[:160]!r}")

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass

    def set_timeout(self, timeout: float) -> None:
        self.sock.settimeout(timeout)

    def send_text(self, payload: str) -> None:
        data = payload.encode("utf-8")
        header = bytearray([0x81])
        length = len(data)
        if length < 126:
            header.append(0x80 | length)
        elif length <= 0xFFFF:
            header.append(0x80 | 126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack("!Q", length))
        mask = os.urandom(4)
        header.extend(mask)
        masked = bytearray(data)
        for index in range(length):
            masked[index] ^= mask[index % 4]
        self.sock.sendall(bytes(header) + bytes(masked))

    def recv_text(self, timeout: float | None = None) -> str | None:
        old_timeout = self.sock.gettimeout()
        if timeout is not None:
            self.sock.settimeout(timeout)
        try:
            while True:
                header = self._read_exact(2)
                first, second = header[0], header[1]
                opcode = first & 0x0F
                length = second & 0x7F
                masked = (second & 0x80) != 0
                if length == 126:
                    length = struct.unpack("!H", self._read_exact(2))[0]
                elif length == 127:
                    length = struct.unpack("!Q", self._read_exact(8))[0]
                mask = self._read_exact(4) if masked else b""
                data = bytearray(self._read_exact(length)) if length else bytearray()
                if masked:
                    for index in range(length):
                        data[index] ^= mask[index % 4]
                if opcode == 0x1:
                    return data.decode("utf-8")
                if opcode == 0x8:
                    return None
                if opcode == 0x9:
                    self._send_pong(bytes(data))
        except socket.timeout:
            return None
        finally:
            if timeout is not None:
                self.sock.settimeout(old_timeout)

    def _send_pong(self, data: bytes) -> None:
        header = bytearray([0x8A])
        header.append(0x80 | len(data))
        mask = os.urandom(4)
        header.extend(mask)
        masked = bytearray(data)
        for index in range(len(masked)):
            masked[index] ^= mask[index % 4]
        self.sock.sendall(bytes(header) + bytes(masked))

    def _read_until(self, marker: bytes) -> bytes:
        data = bytearray()
        while marker not in data:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("Socket closed while reading websocket handshake.")
            data.extend(chunk)
        return bytes(data)

    def _read_exact(self, size: int) -> bytes:
        data = bytearray()
        while len(data) < size:
            chunk = self.sock.recv(size - len(data))
            if not chunk:
                raise RuntimeError("Socket closed while reading websocket frame.")
            data.extend(chunk)
        return bytes(data)


@dataclass
class BrowserState:
    logs: list[dict[str, Any]] = field(default_factory=list)
    responses: list[dict[str, Any]] = field(default_factory=list)
    exceptions: list[str] = field(default_factory=list)

    def log_texts(self) -> list[str]:
        return [str(entry.get("text", "")) for entry in self.logs]


class CdpClient:
    def __init__(self, ws_url: str) -> None:
        self.socket = WebSocket(ws_url)
        self.next_id = 1
        self.state = BrowserState()

    def close(self) -> None:
        self.socket.close()

    def call(self, method: str, params: dict[str, Any] | None = None, timeout: float = 10.0) -> dict[str, Any]:
        message_id = self.next_id
        self.next_id += 1
        self.socket.send_text(json.dumps({"id": message_id, "method": method, "params": params or {}}))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self.socket.recv_text(timeout=0.25)
            if raw is None:
                continue
            message = json.loads(raw)
            if message.get("id") == message_id:
                if "error" in message:
                    raise RuntimeError(f"CDP {method} failed: {message['error']}")
                return message
            self._handle_event(message)
        raise TimeoutError(f"Timed out waiting for CDP response: {method}")

    def pump(self, timeout: float = 0.2) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self.socket.recv_text(timeout=max(0.01, deadline - time.monotonic()))
            if raw is None:
                return
            self._handle_event(json.loads(raw))

    def evaluate(self, expression: str, timeout: float = 10.0) -> Any:
        result = self.call(
            "Runtime.evaluate",
            {
                "expression": expression,
                "awaitPromise": True,
                "returnByValue": True,
            },
            timeout=timeout,
        )["result"]
        if "exceptionDetails" in result:
            raise RuntimeError(f"Runtime.evaluate failed: {result['exceptionDetails']}")
        return result.get("result", {}).get("value")

    def screenshot(self, path: Path) -> None:
        result = self.call("Page.captureScreenshot", {"format": "png", "fromSurface": True}, timeout=15.0)
        data = base64.b64decode(result["result"]["data"])
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def click_logical(self, x: float, y: float, hold_ms: int = 0) -> None:
        point = self.evaluate(
            f"""(() => {{
                const canvas = document.querySelector("canvas");
                if (!canvas) return null;
                const rect = canvas.getBoundingClientRect();
                return {{
                    x: rect.left + ({x} / {LOGICAL_WIDTH}) * rect.width,
                    y: rect.top + ({y} / {LOGICAL_HEIGHT}) * rect.height
                }};
            }})()"""
        )
        if not isinstance(point, dict):
            raise RuntimeError("Canvas is not available for logical click.")
        self.click(float(point["x"]), float(point["y"]), hold_ms=hold_ms)

    def click(self, x: float, y: float, hold_ms: int = 0) -> None:
        params = {"x": x, "y": y, "button": "left", "clickCount": 1}
        self.call("Input.dispatchMouseEvent", {"type": "mousePressed", **params})
        if hold_ms > 0:
            self.wait_ms(hold_ms)
        self.call("Input.dispatchMouseEvent", {"type": "mouseReleased", **params})

    def key(self, code: str, key: str, windows_key_code: int, hold_ms: int = 0) -> None:
        base = {
            "key": key,
            "code": code,
            "windowsVirtualKeyCode": windows_key_code,
            "nativeVirtualKeyCode": windows_key_code,
        }
        if len(key) == 1:
            base["text"] = key
            base["unmodifiedText"] = key
        self.call("Input.dispatchKeyEvent", {"type": "keyDown", **base})
        if hold_ms > 0:
            self.wait_ms(hold_ms)
        self.call("Input.dispatchKeyEvent", {"type": "keyUp", **base})

    def wait_ms(self, milliseconds: int) -> None:
        end = time.monotonic() + milliseconds / 1000.0
        while time.monotonic() < end:
            self.pump(timeout=min(0.2, max(0.01, end - time.monotonic())))

    def wait_for(self, label: str, predicate, timeout_ms: int) -> None:
        end = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < end:
            if predicate():
                return
            self.pump(timeout=0.2)
        raise TimeoutError(f"Timed out waiting for {label}")

    def wait_for_log(self, label: str, needle: str, timeout_ms: int) -> None:
        self.wait_for(label, lambda: any(needle in text for text in self.state.log_texts()), timeout_ms)

    def wait_for_new_log(self, label: str, needle: str, first_index: int, timeout_ms: int) -> None:
        self.wait_for(
            label,
            lambda: any(needle in str(entry.get("text", "")) for entry in self.state.logs[first_index:]),
            timeout_ms,
        )

    def _handle_event(self, message: dict[str, Any]) -> None:
        method = message.get("method")
        params = message.get("params", {})
        if method == "Runtime.consoleAPICalled":
            values = []
            for arg in params.get("args", []):
                if "value" in arg:
                    values.append(str(arg["value"]))
                elif "description" in arg:
                    values.append(str(arg["description"]))
            self.state.logs.append({"level": params.get("type", "log"), "text": " ".join(values)})
        elif method == "Runtime.exceptionThrown":
            details = params.get("exceptionDetails", {})
            text = details.get("text") or str(details)
            self.state.exceptions.append(text)
            self.state.logs.append({"level": "error", "text": text})
        elif method == "Log.entryAdded":
            entry = params.get("entry", {})
            self.state.logs.append({"level": entry.get("level", "log"), "text": entry.get("text", "")})
        elif method == "Network.responseReceived":
            response = params.get("response", {})
            self.state.responses.append(
                {
                    "url": response.get("url", ""),
                    "status": response.get("status"),
                    "mimeType": response.get("mimeType", ""),
                }
            )


class ReleaseServer:
    def __init__(self, build_dir: Path, host: str, port: int, cross_origin_isolated: bool) -> None:
        self.build_dir = build_dir
        self.host = host
        self.port = port
        self.httpd: socketserver.TCPServer | None = None
        self.thread: threading.Thread | None = None
        self.handler = make_handler(build_dir, cross_origin_isolated)

    def __enter__(self) -> "ReleaseServer":
        socketserver.TCPServer.allow_reuse_address = True
        self.httpd = socketserver.TCPServer((self.host, self.port), self.handler)
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.httpd:
            self.httpd.shutdown()
            self.httpd.server_close()
        if self.thread:
            self.thread.join(timeout=2.0)


class Chromium:
    def __init__(self, executable: Path, port: int, user_data_dir: Path, headless: bool) -> None:
        self.executable = executable
        self.port = port
        self.user_data_dir = user_data_dir
        self.headless = headless
        self.process: subprocess.Popen[str] | None = None

    def __enter__(self) -> "Chromium":
        launch_args = [
            str(self.executable),
            f"--remote-debugging-port={self.port}",
            f"--user-data-dir={self.user_data_dir}",
            "--disable-dev-shm-usage",
            "--disable-background-networking",
            "--disable-extensions",
            "--disable-sync",
            "--force-device-scale-factor=1",
            "--hide-scrollbars",
            "--mute-audio",
            "--no-default-browser-check",
            "--no-first-run",
            "--remote-allow-origins=*",
            "--window-size=1900,1120",
            "about:blank",
        ]
        if self.executable.name == "chrome-headless-shell":
            launch_args.insert(1, "--no-sandbox")
            launch_args.insert(2, "--enable-unsafe-swiftshader")
            launch_args.insert(3, "--use-angle=swiftshader")
        elif self.headless:
            launch_args.insert(1, "--headless=new")

        self.process = subprocess.Popen(
            launch_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self._wait_for_cdp()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()

    def _wait_for_cdp(self) -> None:
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline:
            if self.process and self.process.poll() is not None:
                stderr = self.process.stderr.read() if self.process.stderr else ""
                raise RuntimeError(f"Chromium exited before CDP became ready:\n{stderr}")
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{self.port}/json/list", timeout=0.5).read()
                return
            except OSError:
                time.sleep(0.1)
        raise TimeoutError("Timed out waiting for Chromium CDP endpoint.")

    def page_websocket_url(self) -> str:
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            with urllib.request.urlopen(f"http://127.0.0.1:{self.port}/json/list", timeout=1.0) as response:
                targets = json.loads(response.read().decode("utf-8"))
            for target in targets:
                if target.get("type") == "page" and target.get("webSocketDebuggerUrl"):
                    return str(target["webSocketDebuggerUrl"])
            time.sleep(0.1)
        raise RuntimeError("No page CDP target found.")


def served_headers(url: str, paths: list[str]) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    parsed = urllib.parse.urlparse(url)
    connection = http.client.HTTPConnection(parsed.hostname or "127.0.0.1", parsed.port or 80, timeout=5)
    try:
        for path in paths:
            connection.request("HEAD", path)
            response = connection.getresponse()
            response.read()
            result[path] = {
                "status": str(response.status),
                "content_type": response.getheader("Content-Type", ""),
                "cache_control": response.getheader("Cache-Control", ""),
                "coop": response.getheader("Cross-Origin-Opener-Policy", ""),
                "coep": response.getheader("Cross-Origin-Embedder-Policy", ""),
            }
    finally:
        connection.close()
    return result


def validate_headers(headers: dict[str, dict[str, str]], cross_origin_isolated: bool) -> list[str]:
    failures: list[str] = []
    expected_types = {
        "/TinyFarmRPG-Web.html": "text/html",
        "/TinyFarmRPG-Web.js": "javascript",
        "/TinyFarmRPG-Web.wasm": "application/wasm",
        "/TinyFarmRPG-Web.data": "application/octet-stream",
        "/web-packages/shared-ui.tfpack": "application/octet-stream",
        "/web-packages/rpg-core.tfpack": "application/octet-stream",
        "/web-packages/home-map.tfpack": "application/octet-stream",
        "/web-packages/town-map.tfpack": "application/octet-stream",
        "/web-packages/battle-core.tfpack": "application/octet-stream",
        "/web-packages/vfx-core.tfpack": "application/octet-stream",
        "/web-packages/audio-core.tfpack": "application/octet-stream",
    }
    for path, expected in expected_types.items():
        actual = headers.get(path, {})
        if actual.get("status") != "200":
            failures.append(f"{path} returned {actual.get('status', '<missing>')}")
        content_type = actual.get("content_type", "")
        if expected not in content_type:
            failures.append(f"{path} Content-Type expected {expected}, got {content_type or '<missing>'}")
        if not actual.get("cache_control"):
            failures.append(f"{path} missing Cache-Control")

    html = headers.get("/TinyFarmRPG-Web.html", {})
    if cross_origin_isolated:
        if html.get("coop") != "same-origin" or html.get("coep") != "require-corp":
            failures.append("Cross-origin isolated preview missing COOP/COEP headers")
    elif html.get("coop") or html.get("coep"):
        failures.append("Single-thread preview must not require COOP/COEP headers")
    return failures


def read_save_player(cdp: CdpClient) -> dict[str, Any] | None:
    return cdp.evaluate(
        f"""(() => {{
            if (typeof FS === "undefined") return null;
            const path = "{SAVE_PATH}";
            if (!FS.analyzePath(path).exists) return null;
            const text = new TextDecoder("utf-8").decode(FS.readFile(path));
            const json = JSON.parse(text);
            if (!json.player) return null;
            return {{
                map_name: json.player.map_name || "",
                position: json.player.position || null
            }};
        }})()""",
        timeout=10.0,
    )


def persistent_file_exists(cdp: CdpClient, path: str) -> bool:
    return bool(
        cdp.evaluate(
            f"""(() => {{
                if (typeof FS === "undefined") return false;
                return FS.analyzePath({json.dumps(path)}).exists;
            }})()""",
            timeout=10.0,
        )
    )


def read_json_file(cdp: CdpClient, path: str) -> dict[str, Any] | None:
    data = cdp.evaluate(
        f"""(() => {{
            if (typeof FS === "undefined") return null;
            const path = {json.dumps(path)};
            if (!FS.analyzePath(path).exists) return null;
            const text = new TextDecoder("utf-8").decode(FS.readFile(path));
            return JSON.parse(text);
        }})()""",
        timeout=10.0,
    )
    return data if isinstance(data, dict) else None


def read_user_settings_file(cdp: CdpClient) -> dict[str, Any] | None:
    return read_json_file(cdp, USER_SETTINGS_PATH)


def read_user_settings_diagnostics(cdp: CdpClient) -> dict[str, Any] | None:
    diagnostics = cdp.evaluate(
        """(() => {
            const value = globalThis.TinyFarmRPGWebReleaseDiagnostics?.userSettings;
            return value ? {...value} : null;
        })()""",
        timeout=10.0,
    )
    return diagnostics if isinstance(diagnostics, dict) else None


def read_persistent_storage_diagnostics(cdp: CdpClient) -> dict[str, Any] | None:
    diagnostics = cdp.evaluate(
        """(() => {
            const value = globalThis.TinyFarmRPGWebReleaseDiagnostics?.persistentStorage;
            return value ? {...value} : null;
        })()""",
        timeout=10.0,
    )
    return diagnostics if isinstance(diagnostics, dict) else None


def read_web_release_diagnostics(cdp: CdpClient) -> dict[str, Any] | None:
    diagnostics = cdp.evaluate(
        """(() => {
            const value = globalThis.TinyFarmRPGWebReleaseDiagnostics;
            return value ? JSON.parse(JSON.stringify(value)) : null;
        })()""",
        timeout=10.0,
    )
    return diagnostics if isinstance(diagnostics, dict) else None


def read_battle_diagnostics(cdp: CdpClient) -> dict[str, Any] | None:
    diagnostics = read_web_release_diagnostics(cdp)
    if not isinstance(diagnostics, dict):
        return None
    battle = diagnostics.get("battle")
    return battle if isinstance(battle, dict) else None


def read_gameplay_diagnostics(cdp: CdpClient) -> dict[str, Any] | None:
    diagnostics = read_web_release_diagnostics(cdp)
    if not isinstance(diagnostics, dict):
        return None
    gameplay = diagnostics.get("gameplay")
    return gameplay if isinstance(gameplay, dict) else None


def gameplay_player(gameplay: dict[str, Any] | None) -> dict[str, Any]:
    if not isinstance(gameplay, dict):
        return {}
    player = gameplay.get("player")
    return player if isinstance(player, dict) else {}


def inventory_count(gameplay: dict[str, Any] | None, item_id: str) -> int:
    inventory = gameplay_player(gameplay).get("inventory")
    if not isinstance(inventory, dict):
        return 0
    items = inventory.get("items")
    if not isinstance(items, dict):
        return 0
    return int(items.get(item_id) or 0)


def player_gold(gameplay: dict[str, Any] | None) -> int:
    return int(gameplay_player(gameplay).get("gold") or 0)


def quest_progress(gameplay: dict[str, Any] | None, key: str) -> int:
    quests = gameplay_player(gameplay).get("quests")
    if not isinstance(quests, dict):
        return 0
    progress = quests.get("objectiveProgress")
    if not isinstance(progress, dict):
        return 0
    return int(progress.get(key) or 0)


def active_quest_ids(gameplay: dict[str, Any] | None) -> list[str]:
    quests = gameplay_player(gameplay).get("quests")
    if not isinstance(quests, dict):
        return []
    values = quests.get("activeQuestIds")
    return [str(value) for value in values] if isinstance(values, list) else []


def completed_quest_ids(gameplay: dict[str, Any] | None) -> list[str]:
    quests = gameplay_player(gameplay).get("quests")
    if not isinstance(quests, dict):
        return []
    values = quests.get("completedQuestIds")
    return [str(value) for value in values] if isinstance(values, list) else []


def party_actor_ids(gameplay: dict[str, Any] | None, field: str) -> list[str]:
    party = gameplay_player(gameplay).get("party")
    if not isinstance(party, dict):
        return []
    values = party.get(field)
    return [str(value) for value in values] if isinstance(values, list) else []


def actor_runtime_state(gameplay: dict[str, Any] | None, actor_id: str) -> dict[str, int]:
    party = gameplay_player(gameplay).get("party")
    if not isinstance(party, dict):
        return {}
    states = party.get("runtimeStates")
    if not isinstance(states, dict):
        return {}
    state = states.get(actor_id)
    if not isinstance(state, dict):
        return {}
    return {
        "currentHp": int(state.get("currentHp") or 0),
        "currentMp": int(state.get("currentMp") or 0),
        "level": int(state.get("level") or 0),
        "totalExp": int(state.get("totalExp") or 0),
    }


def gameplay_time_minutes(gameplay: dict[str, Any] | None) -> int:
    if not isinstance(gameplay, dict):
        return 0
    time_state = gameplay.get("time")
    if not isinstance(time_state, dict):
        return 0
    day = int(time_state.get("day") or 0)
    hour = float(time_state.get("hour") or 0.0)
    minute = float(time_state.get("minute") or 0.0)
    return day * 24 * 60 + int(hour * 60.0 + minute)


def appearance_signature(gameplay: dict[str, Any] | None) -> str:
    appearance = gameplay_player(gameplay).get("appearance")
    if not isinstance(appearance, dict):
        return ""
    return str(appearance.get("signature") or "")


def wait_for_gameplay_condition(
    cdp: CdpClient,
    label: str,
    predicate,
    timeout_ms: int = 10000,
) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    last_gameplay: dict[str, Any] | None = None
    while time.monotonic() < end:
        gameplay = read_gameplay_diagnostics(cdp)
        if isinstance(gameplay, dict):
            last_gameplay = gameplay
            if predicate(gameplay):
                return gameplay
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for {label}; last={last_gameplay}")


def validate_web_release_diagnostics(
    diagnostics: dict[str, Any] | None,
    expected_map: str = "home_exterior",
    required_package_ids: tuple[str, ...] = DEMO_RUNTIME_PACKAGE_IDS,
) -> list[str]:
    if not isinstance(diagnostics, dict):
        return ["TinyFarmRPGWebReleaseDiagnostics is missing."]

    failures: list[str] = []
    render_failures = validate_render_capabilities(diagnostics.get("renderCapabilities"))
    failures.extend(render_failures)

    gameplay = diagnostics.get("gameplay")
    if not isinstance(gameplay, dict):
        failures.append("diagnostics.gameplay is missing.")
    else:
        player = gameplay.get("player")
        if gameplay.get("currentScene") != "GameScene":
            failures.append(f"diagnostics.gameplay.currentScene expected GameScene, got {gameplay.get('currentScene')!r}")
        if gameplay.get("map") != expected_map:
            failures.append(f"diagnostics.gameplay.map expected {expected_map}, got {gameplay.get('map')!r}")
        if not isinstance(player, dict):
            failures.append("diagnostics.gameplay.player is missing.")
        else:
            if not isinstance(player.get("gold"), (int, float)):
                failures.append("diagnostics.gameplay.player.gold is missing.")
            inventory = player.get("inventory")
            if not isinstance(inventory, dict) or not isinstance(inventory.get("occupiedSlots"), (int, float)):
                failures.append("diagnostics.gameplay.player.inventory.occupiedSlots is missing.")
            party = player.get("party")
            if not isinstance(party, dict) or int(party.get("activeCount", 0)) <= 0:
                failures.append(f"diagnostics.gameplay.player.party.activeCount is invalid: {party!r}")
            quests = player.get("quests")
            if not isinstance(quests, dict) or not isinstance(quests.get("activeQuestIds"), list):
                failures.append("diagnostics.gameplay.player.quests.activeQuestIds is missing.")

    packages = diagnostics.get("packages")
    if not isinstance(packages, dict):
        failures.append("diagnostics.packages is missing.")
    else:
        for package_id in required_package_ids:
            package = packages.get(package_id)
            if not isinstance(package, dict):
                failures.append(f"diagnostics.packages.{package_id} is missing.")
                continue
            if package.get("loaded") is not True:
                failures.append(f"diagnostics.packages.{package_id}.loaded expected true, got {package.get('loaded')!r}")
            if not isinstance(package.get("attempts"), (int, float)):
                failures.append(f"diagnostics.packages.{package_id}.attempts is missing.")
            if not isinstance(package.get("dependencies"), list):
                failures.append(f"diagnostics.packages.{package_id}.dependencies is missing.")

    vfx = diagnostics.get("vfx")
    if not isinstance(vfx, dict):
        failures.append("diagnostics.vfx is missing.")
    else:
        if vfx.get("effekseerEnabled") is not True:
            failures.append(f"diagnostics.vfx.effekseerEnabled expected true, got {vfx.get('effekseerEnabled')!r}.")
        if vfx.get("backend") != "effekseer":
            failures.append(f"diagnostics.vfx.backend expected effekseer, got {vfx.get('backend')!r}.")
        if vfx.get("status") != "enabled":
            failures.append(f"diagnostics.vfx.status expected enabled, got {vfx.get('status')!r}.")

    if not isinstance(diagnostics.get("persistentStorage"), dict):
        failures.append("diagnostics.persistentStorage is missing.")
    if not isinstance(diagnostics.get("userSettings"), dict):
        failures.append("diagnostics.userSettings is missing.")

    return failures


def write_corrupt_save_slot(cdp: CdpClient) -> None:
    result = cdp.evaluate(
        f"""(() => new Promise((resolve) => {{
            if (typeof FS === "undefined") {{
                resolve({{success: false, error: "FS unavailable"}});
                return;
            }}
            const path = {json.dumps(CORRUPT_SAVE_PATH)};
            FS.mkdirTree("/persistent/saves");
            FS.writeFile(path, "{{ invalid save json");
            FS.syncfs(false, (err) => {{
                resolve({{success: !err, error: err ? String(err) : ""}});
            }});
        }}))()""",
        timeout=15000,
    )
    if not isinstance(result, dict) or not result.get("success"):
        raise RuntimeError(f"Failed to write corrupt save slot: {result}")


def read_save_position(cdp: CdpClient) -> dict[str, float] | None:
    player = read_save_player(cdp)
    if not isinstance(player, dict):
        return None
    position = player.get("position")
    return position if isinstance(position, dict) else None


def read_runtime_player(cdp: CdpClient) -> dict[str, Any] | None:
    player = cdp.evaluate(
        """(() => {
            const state = globalThis.TinyFarmRPGSmokeState;
            if (!state || !state.player) return null;
            return {
                map: state.player.map || "",
                x: Number(state.player.x),
                y: Number(state.player.y)
            };
        })()"""
    )
    return player if isinstance(player, dict) else None


def wait_for_runtime_player(cdp: CdpClient, map_name: str, timeout_ms: int) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < end:
        player = read_runtime_player(cdp)
        if isinstance(player, dict) and player.get("map") == map_name:
            return player
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for runtime player on {map_name}")


def position_distance(first: dict[str, float], second: dict[str, float]) -> float:
    return abs(second["x"] - first["x"]) + abs(second["y"] - first["y"])


def wait_for_save_position(
    cdp: CdpClient,
    timeout_ms: int,
    changed_from: dict[str, float] | None = None,
) -> dict[str, float]:
    end = time.monotonic() + timeout_ms / 1000.0
    last_position: dict[str, float] | None = None
    while time.monotonic() < end:
        position = read_save_position(cdp)
        if isinstance(position, dict) and "x" in position and "y" in position:
            normalized = {"x": float(position["x"]), "y": float(position["y"])}
            last_position = normalized
            if changed_from is None or position_distance(changed_from, normalized) >= 1.0:
                return normalized
        cdp.wait_ms(200)
    if last_position is None:
        raise TimeoutError(f"Timed out waiting for {SAVE_PATH}")
    raise TimeoutError(f"Timed out waiting for {SAVE_PATH} to change from {changed_from}; last={last_position}")


def save_slot0(
    cdp: CdpClient,
    output_dir: Path,
    label: str,
    overwrite: bool = False,
    changed_from: dict[str, float] | None = None,
) -> dict[str, float]:
    sync_log_start = len(cdp.state.logs)
    pause_log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_PAUSE)
    cdp.wait_for_new_log("save pause menu", "GameScene: pause menu opened.", pause_log_start, 10000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / f"phase14-{label}-pause.png")
    cdp.click_logical(312, 91, hold_ms=80)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / f"phase14-{label}-slot-select.png")
    cdp.click_logical(236, 75, hold_ms=80)
    if overwrite:
        cdp.wait_ms(500)
        cdp.screenshot(output_dir / f"phase14-{label}-overwrite.png")
        cdp.click_logical(240, 185, hold_ms=80)
    position = wait_for_save_position(cdp, 10000, changed_from=changed_from)
    cdp.wait_for_new_log(
        "persistent save sync",
        "SaveService: Web persistent storage sync completed after async save.",
        sync_log_start,
        10000,
    )
    return position


def press_game_key(
    cdp: CdpClient,
    code: str,
    key: str,
    windows_key_code: int,
    hold_ms: int = 120,
    settle_ms: int = 250,
) -> None:
    cdp.evaluate('document.querySelector("canvas")?.focus()')
    cdp.key(code, key, windows_key_code, hold_ms=hold_ms)
    cdp.wait_ms(settle_ms)


def focus_gameplay_canvas(cdp: CdpClient) -> None:
    cdp.click_logical(320, 180)
    cdp.wait_ms(250)


def move_player(cdp: CdpClient, code: str, key: str, windows_key_code: int, hold_ms: int = 1400) -> None:
    cdp.evaluate('document.querySelector("canvas")?.focus()')
    cdp.key(code, key, windows_key_code, hold_ms=hold_ms)
    cdp.wait_ms(500)


def move_player_down(cdp: CdpClient, hold_ms: int = 1400) -> None:
    move_player(cdp, "KeyS", "s", 83, hold_ms=hold_ms)


def move_player_up(cdp: CdpClient, hold_ms: int = 1400) -> None:
    move_player(cdp, "KeyW", "w", 87, hold_ms=hold_ms)


def move_player_left(cdp: CdpClient, hold_ms: int = 1400) -> None:
    move_player(cdp, "KeyA", "a", 65, hold_ms=hold_ms)


def move_player_right(cdp: CdpClient, hold_ms: int = 1400) -> None:
    move_player(cdp, "KeyD", "d", 68, hold_ms=hold_ms)


def move_player_to(
    cdp: CdpClient,
    x: float,
    y: float,
    map_name: str = "home_exterior",
    tolerance: float = 5.0,
    timeout_ms: int = 12000,
) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    player = wait_for_runtime_player(cdp, map_name, timeout_ms)
    while time.monotonic() < end:
        dx = x - float(player["x"])
        dy = y - float(player["y"])
        if abs(dx) <= tolerance and abs(dy) <= tolerance:
            return player

        if abs(dx) > tolerance:
            hold_ms = max(70, min(180, int(abs(dx) * 6.0)))
            if dx > 0.0:
                move_player_right(cdp, hold_ms=hold_ms)
            else:
                move_player_left(cdp, hold_ms=hold_ms)
        else:
            hold_ms = max(70, min(180, int(abs(dy) * 6.0)))
            if dy > 0.0:
                move_player_down(cdp, hold_ms=hold_ms)
            else:
                move_player_up(cdp, hold_ms=hold_ms)

        player = wait_for_runtime_player(cdp, map_name, 3000)

    raise TimeoutError(f"Timed out moving player on {map_name} to ({x:.1f}, {y:.1f}); last={player}")


def move_player_through_home_exterior(cdp: CdpClient, waypoints: list[tuple[float, float]], timeout_ms: int = 24000) -> None:
    for x, y in waypoints:
        move_player_to(cdp, x, y, map_name="home_exterior", tolerance=10.0, timeout_ms=timeout_ms)


def approach_home_exterior_quest_giver(cdp: CdpClient, timeout_ms: int = 30000) -> None:
    wait_for_runtime_player(cdp, "home_exterior", timeout_ms)
    move_player_through_home_exterior(cdp, [(296.0, 206.0)], timeout_ms=timeout_ms)


def approach_home_exterior_recruit(cdp: CdpClient, timeout_ms: int = 30000) -> None:
    move_player_through_home_exterior(cdp, [(248.0, 168.0)], timeout_ms=timeout_ms)


def approach_home_exterior_town_gate(cdp: CdpClient, timeout_ms: int = 30000) -> None:
    player = wait_for_runtime_player(cdp, "home_exterior", timeout_ms)
    if float(player["y"]) <= 170.0:
        move_player_through_home_exterior(cdp, [(360.0, 138.0)], timeout_ms=timeout_ms)
        return
    move_player_through_home_exterior(cdp, [(350.0, 138.0), (360.0, 138.0)], timeout_ms=timeout_ms)


def has_new_log(cdp: CdpClient, needle: str, first_index: int) -> bool:
    return any(needle in str(entry.get("text", "")) for entry in cdp.state.logs[first_index:])


def wait_for_new_log_optional(
    cdp: CdpClient,
    label: str,
    needle: str,
    first_index: int,
    timeout_ms: int,
) -> bool:
    try:
        cdp.wait_for_new_log(label, needle, first_index, timeout_ms)
        return True
    except TimeoutError:
        return False


def is_web_package_loaded(cdp: CdpClient, package_id: str) -> bool:
    diagnostics = read_web_release_diagnostics(cdp) or {}
    packages = diagnostics.get("packages") if isinstance(diagnostics.get("packages"), dict) else {}
    package = packages.get(package_id) if isinstance(packages, dict) else None
    return isinstance(package, dict) and package.get("loaded") is True


def wait_for_web_package_loaded(
    cdp: CdpClient,
    package_id: str,
    label: str,
    loaded_log: str,
    first_index: int,
    timeout_ms: int = 30000,
) -> None:
    if is_web_package_loaded(cdp, package_id):
        return
    if wait_for_new_log_optional(cdp, label, loaded_log, first_index, timeout_ms):
        return
    if is_web_package_loaded(cdp, package_id):
        return
    raise RuntimeError(f"Timed out waiting for web package '{package_id}' to load.")


def available_gameplay_encounters(gameplay: dict[str, Any]) -> list[dict[str, Any]]:
    encounters = gameplay.get("encounters")
    if not isinstance(encounters, list):
        return []

    normalized: list[dict[str, Any]] = []
    for encounter in encounters:
        if not isinstance(encounter, dict):
            continue
        if encounter.get("defeated") is True or encounter.get("engaged") is True:
            continue
        troop_id = str(encounter.get("troopId") or "")
        if not troop_id:
            continue
        try:
            normalized.append(
                {
                    "encounterId": int(encounter.get("encounterId") or 0),
                    "troopId": troop_id,
                    "x": float(encounter.get("x") or 0.0),
                    "y": float(encounter.get("y") or 0.0),
                }
            )
        except (TypeError, ValueError):
            continue
    return normalized


def select_nearest_encounter(gameplay: dict[str, Any], preferred_troop_id: str | None = "troop.slime_single") -> dict[str, Any]:
    encounters = available_gameplay_encounters(gameplay)
    if not encounters:
        raise RuntimeError(f"No available encounters in gameplay diagnostics: {gameplay}")

    player = gameplay.get("player") if isinstance(gameplay.get("player"), dict) else {}
    player_x = float(player.get("x") or 0.0)
    player_y = float(player.get("y") or 0.0)
    preferred = [encounter for encounter in encounters if encounter.get("troopId") == preferred_troop_id] if preferred_troop_id else []
    candidates = preferred or encounters
    return min(candidates, key=lambda encounter: abs(float(encounter["x"]) - player_x) + abs(float(encounter["y"]) - player_y))


def trigger_town_encounter_from_diagnostics(
    cdp: CdpClient,
    output_dir: Path,
    battle_start: int,
    preferred_troop_id: str | None = "troop.slime_single",
    screenshot_prefix: str = "phase25",
) -> dict[str, Any]:
    gameplay = read_gameplay_diagnostics(cdp) or {}
    if gameplay.get("map") != "town":
        raise RuntimeError(f"Expected town gameplay diagnostics before battle, got: {gameplay}")

    encounter = select_nearest_encounter(gameplay, preferred_troop_id)
    approach_x = max(0.0, float(encounter["x"]) - 14.0)
    approach_y = float(encounter["y"])
    try:
        move_player_to(cdp, approach_x, approach_y, map_name="town", tolerance=10.0, timeout_ms=24000)
    except TimeoutError:
        if has_new_log(cdp, "EnemyEncounterSystem: triggering battle", battle_start):
            cdp.screenshot(output_dir / f"{screenshot_prefix}-encounter-approach.png")
            return encounter
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict) and battle.get("currentScene") == "BattleScene":
            cdp.screenshot(output_dir / f"{screenshot_prefix}-encounter-approach.png")
            return encounter
        raise
    cdp.wait_ms(400)
    cdp.screenshot(output_dir / f"{screenshot_prefix}-encounter-approach.png")

    movement_sweep = (
        move_player_right,
        move_player_left,
        move_player_right,
        move_player_up,
        move_player_down,
        move_player_right,
    )
    hold_times = (260, 360, 520, 240, 480, 420)
    for move, hold_ms in zip(movement_sweep, hold_times):
        move(cdp, hold_ms=hold_ms)
        if has_new_log(cdp, "EnemyEncounterSystem: triggering battle", battle_start):
            return encounter
        if wait_for_new_log_optional(cdp, "battle encounter trigger", "EnemyEncounterSystem: triggering battle", battle_start, 1200):
            return encounter
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict) and battle.get("currentScene") == "BattleScene":
            return encounter

    latest_gameplay = read_gameplay_diagnostics(cdp) or {}
    raise TimeoutError(f"Timed out triggering town encounter {encounter}; gameplay={latest_gameplay}")


def wait_for_battle_diagnostics(cdp: CdpClient, timeout_ms: int = 30000) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    last_battle: dict[str, Any] | None = None
    while time.monotonic() < end:
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict) and battle.get("currentScene") == "BattleScene":
            return battle
        last_battle = battle
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for BattleScene diagnostics; last={last_battle}")


def wait_for_battle_menu(cdp: CdpClient, state: str, timeout_ms: int = 30000) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    last_battle: dict[str, Any] | None = None
    while time.monotonic() < end:
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict):
            last_battle = battle
            if battle.get("menuState") == state:
                return battle
            if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
                return battle
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for battle menu {state}; last={last_battle}")


def wait_for_battle_menu_any(cdp: CdpClient, states: set[str], timeout_ms: int = 30000) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    last_battle: dict[str, Any] | None = None
    while time.monotonic() < end:
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict):
            last_battle = battle
            if battle.get("menuState") in states:
                return battle
            if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
                return battle
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for battle menu {sorted(states)}; last={last_battle}")


def wait_for_battle_player_input(cdp: CdpClient, timeout_ms: int = 30000) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    last_battle: dict[str, Any] | None = None
    while time.monotonic() < end:
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict):
            last_battle = battle
            if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
                return battle
            if battle.get("menuState") in {"PartyCommand", "ActorCommand", "SkillList", "TargetSelect"}:
                return battle
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for battle player input; last={last_battle}")


def battle_cursor_for_state(battle: dict[str, Any], state: str) -> int:
    cursors = battle.get("cursors")
    if not isinstance(cursors, dict):
        return -1
    field = {
        "PartyCommand": "partyCommand",
        "ActorCommand": "actorCommand",
        "SkillList": "listEntry",
        "ItemList": "listEntry",
        "TargetSelect": "targetEntry",
    }.get(state)
    if field is None:
        return -1
    value = cursors.get(field)
    return int(value) if isinstance(value, (int, float)) else -1


def move_battle_cursor_to(cdp: CdpClient, state: str, target_index: int, timeout_ms: int = 10000) -> dict[str, Any]:
    end = time.monotonic() + timeout_ms / 1000.0
    battle = wait_for_battle_menu(cdp, state, timeout_ms)
    while time.monotonic() < end and battle.get("menuState") == state:
        cursor = battle_cursor_for_state(battle, state)
        if cursor == target_index:
            return battle
        if cursor < 0:
            raise RuntimeError(f"Battle cursor for {state} is unavailable: {battle}")
        if cursor < target_index:
            press_game_key(cdp, *KEY_MENU_DOWN)
        else:
            press_game_key(cdp, *KEY_MENU_UP)
        battle = wait_for_battle_menu(cdp, state, 3000)
    raise TimeoutError(f"Timed out moving {state} cursor to {target_index}; last={battle}")


def confirm_battle_target_selection(
    cdp: CdpClient,
    battle: dict[str, Any],
    output_dir: Path | None = None,
    capture_vfx: bool = False,
) -> tuple[dict[str, Any], bool]:
    if battle_cursor_for_state(battle, "TargetSelect") < 0:
        raise RuntimeError(f"TargetSelect cursor is unavailable before confirming target: {battle}")

    actor_id = battle.get("currentActorId")
    expected_actor_id = int(actor_id) if isinstance(actor_id, (int, float)) else None
    action_start = time.monotonic()
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    end = time.monotonic() + 30000 / 1000.0
    last_battle: dict[str, Any] | None = None
    saw_vfx = False
    captured_vfx = False
    while time.monotonic() < end:
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict):
            last_battle = battle
            vfx = battle.get("vfx")
            if capture_vfx and isinstance(vfx, dict) and (
                int(vfx.get("scheduledEvents") or 0) > 0 or
                int(vfx.get("pendingRequests") or 0) > 0 or
                int(vfx.get("lastDrawCallCount") or 0) > 0 or
                int(vfx.get("lastInstanceCount") or 0) > 0
            ):
                saw_vfx = True
                if output_dir is not None and not captured_vfx:
                    cdp.screenshot(output_dir / "phase25-battle-skill-vfx.png")
                    captured_vfx = True
            last_action = battle.get("lastAction")
            if isinstance(last_action, dict) and last_action.get("type") == "Skill" and last_action.get("status") == "Applied":
                action_actor_id = last_action.get("actorId")
                if expected_actor_id is None or action_actor_id == expected_actor_id:
                    return battle, saw_vfx
            if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
                return battle, saw_vfx
        cdp.wait_ms(100)
    raise TimeoutError(f"Timed out waiting for target action after {time.monotonic() - action_start:.1f}s; last={last_battle}")


def submit_battle_skill_action(cdp: CdpClient, output_dir: Path | None = None) -> tuple[dict[str, Any], bool]:
    battle = wait_for_battle_player_input(cdp)
    if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
        return battle, False
    if battle.get("menuState") == "PartyCommand":
        move_battle_cursor_to(cdp, "PartyCommand", 0)
        press_game_key(cdp, *KEY_MENU_CONFIRM)
        battle = wait_for_battle_menu_any(cdp, {"ActorCommand", "TargetSelect"}, 10000)
    if battle.get("menuState") == "TargetSelect":
        return confirm_battle_target_selection(cdp, battle)
    if battle.get("menuState") == "ActorCommand":
        move_battle_cursor_to(cdp, "ActorCommand", 1)
        press_game_key(cdp, *KEY_MENU_CONFIRM)
        battle = wait_for_battle_menu_any(cdp, {"SkillList", "TargetSelect"}, 10000)
        if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
            return battle, False

    if battle.get("menuState") == "TargetSelect":
        return confirm_battle_target_selection(cdp, battle)
    if battle.get("menuState") != "SkillList":
        raise RuntimeError(f"Expected SkillList before skill action, got {battle}")

    move_battle_cursor_to(cdp, "SkillList", 0)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    battle = wait_for_battle_menu_any(cdp, {"TargetSelect", "PartyCommand", "ActorCommand", "SkillList"}, 10000)
    if battle.get("outcome") in {"Victory", "Defeat", "Escaped"}:
        return battle, False
    if battle.get("menuState") != "TargetSelect":
        return battle, False

    return confirm_battle_target_selection(cdp, battle, output_dir, capture_vfx=True)


def run_battle_to_victory(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    saw_skill = False
    saw_vfx = False
    last_battle = wait_for_battle_diagnostics(cdp)
    for action_index in range(12):
        if last_battle.get("outcome") == "Victory":
            break
        if last_battle.get("outcome") in {"Defeat", "Escaped"}:
            raise RuntimeError(f"Battle ended unexpectedly: {last_battle}")

        last_battle, action_saw_vfx = submit_battle_skill_action(cdp, output_dir if action_index == 0 else None)
        saw_vfx = saw_vfx or action_saw_vfx
        cdp.wait_ms(900)
        last_battle = wait_for_battle_diagnostics(cdp, 10000)
        last_action = last_battle.get("lastAction")
        if isinstance(last_action, dict) and last_action.get("type") == "Skill" and last_action.get("status") == "Applied":
            saw_skill = True
        vfx = last_battle.get("vfx")
        if isinstance(vfx, dict) and (
            int(vfx.get("lastDrawCallCount") or 0) > 0 or
            int(vfx.get("lastInstanceCount") or 0) > 0 or
            int(vfx.get("scheduledEvents") or 0) > 0
        ):
            saw_vfx = True
        if action_index == 0 and not saw_vfx:
            cdp.screenshot(output_dir / "phase25-battle-skill-vfx.png")

    end = time.monotonic() + 45000 / 1000.0
    while time.monotonic() < end:
        battle = read_battle_diagnostics(cdp)
        if isinstance(battle, dict):
            last_battle = battle
            if battle.get("outcome") == "Victory":
                if not saw_skill:
                    raise RuntimeError(f"Battle reached victory without observed Skill action: {battle}")
                if not saw_vfx:
                    raise RuntimeError(f"Battle reached victory without observed VFX diagnostics: {battle}")
                if not battle.get("victoryContinueEnabled"):
                    cdp.wait_ms(200)
                    continue
                cdp.screenshot(output_dir / "phase25-battle-victory.png")
                press_game_key(cdp, *KEY_MENU_CONFIRM, settle_ms=1200)
                return battle
            if battle.get("outcome") in {"Defeat", "Escaped"}:
                raise RuntimeError(f"Battle ended unexpectedly: {battle}")
        cdp.wait_ms(200)
    raise TimeoutError(f"Timed out waiting for battle victory; last={last_battle}")


def exercise_menu_controls(cdp: CdpClient, output_dir: Path) -> None:
    focus_gameplay_canvas(cdp)
    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_INVENTORY)
    cdp.wait_for_new_log("inventory menu", "GameScene: inventory menu opened.", log_start, 10000)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / "phase17-inventory-open.png")
    press_game_key(cdp, *KEY_ESCAPE, settle_ms=700)
    cdp.screenshot(output_dir / "phase17-inventory-closed.png")

    focus_gameplay_canvas(cdp)
    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_HOTBAR)
    cdp.wait_for_new_log("hotbar open", "GameScene: hotbar toggle accepted.", log_start, 5000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / "phase17-hotbar-open.png")
    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_HOTBAR)
    cdp.wait_for_new_log("hotbar close", "GameScene: hotbar toggle accepted.", log_start, 5000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / "phase17-hotbar-closed.png")

    focus_gameplay_canvas(cdp)
    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_PAUSE)
    cdp.wait_for_new_log("pause menu", "GameScene: pause menu opened.", log_start, 10000)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / "phase17-pause-open.png")
    press_game_key(cdp, *KEY_ESCAPE, settle_ms=700)
    cdp.screenshot(output_dir / "phase17-pause-closed.png")


def trigger_tool_action(cdp: CdpClient, output_dir: Path) -> None:
    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_HOTBAR_1)
    cdp.wait_for_new_log("tool selection", "尝试切换工具: Hoe", log_start, 5000)

    log_start = len(cdp.state.logs)
    cdp.click_logical(320, 180, hold_ms=160)
    cdp.wait_for_new_log("tool action", "触发工具动作", log_start, 5000)
    cdp.wait_ms(900)
    cdp.screenshot(output_dir / "phase17-tool-action.png")


def exercise_home_round_trip(cdp: CdpClient, output_dir: Path) -> None:
    enter_home_interior(cdp, output_dir, "phase17")
    leave_home_interior(cdp, output_dir, "phase17")
    cdp.screenshot(output_dir / "phase17-home-exterior-return.png")


def trigger_merchant_dialogue(cdp: CdpClient, output_dir: Path) -> None:
    move_player_to(cdp, 350.0, 170.0, tolerance=10.0)
    move_player_to(cdp, 350.0, 206.0, tolerance=10.0)
    move_player_right(cdp, hold_ms=80)
    cdp.wait_ms(300)
    cdp.screenshot(output_dir / "phase17-merchant-approach.png")

    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_INTERACT)
    cdp.wait_for_new_log(
        "scripted merchant dialogue",
        "DialoguePresentationController: conversation dialogue shown.",
        log_start,
        10000,
    )
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / "phase17-merchant-dialogue.png")

    shop_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_INTERACT, settle_ms=700)
    cdp.wait_for_new_log("scripted merchant shop", "ShopMenuScene: opened", shop_start, 10000)
    cdp.screenshot(output_dir / "phase17-merchant-shop-open.png")
    press_game_key(cdp, *KEY_ESCAPE, settle_ms=700)

    move_player_left(cdp, hold_ms=900)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / "phase17-after-dialogue-distance.png")


def press_interact_until_log(
    cdp: CdpClient,
    label: str,
    expected_log: str,
    presses: int,
    timeout_ms: int = 15000,
) -> None:
    log_start = len(cdp.state.logs)
    for _ in range(presses):
        press_game_key(cdp, *KEY_INTERACT, settle_ms=450)
        if has_new_log(cdp, expected_log, log_start):
            return
    cdp.wait_for_new_log(label, expected_log, log_start, timeout_ms)


def ensure_home_exterior_from_town(cdp: CdpClient, output_dir: Path, label: str) -> None:
    if (read_gameplay_diagnostics(cdp) or {}).get("map") == "home_exterior":
        return
    transition_start = len(cdp.state.logs)
    move_player_to(cdp, 20.0, 200.0, map_name="town", tolerance=10.0, timeout_ms=60000)
    move_player_left(cdp, hold_ms=700)
    cdp.wait_for_new_log(
        f"{label} town to home exterior",
        "MapTransitionSystem: map transition 'town' -> 'home_exterior'.",
        transition_start,
        30000,
    )
    cdp.wait_for_new_log("home_exterior loaded", "MapManager: 已加载地图 'home_exterior'", transition_start, 30000)
    wait_for_runtime_player(cdp, "home_exterior", 20000)
    cdp.wait_ms(800)
    cdp.screenshot(output_dir / f"{label}-home-exterior-return.png")


def ensure_town_from_home_exterior(cdp: CdpClient, output_dir: Path, label: str) -> None:
    if (read_gameplay_diagnostics(cdp) or {}).get("map") == "town":
        return
    approach_home_exterior_town_gate(cdp, timeout_ms=30000)
    transition_start = len(cdp.state.logs)
    move_player_right(cdp, hold_ms=500)
    cdp.wait_for_new_log(
        f"{label} home exterior to town",
        "MapTransitionSystem: map transition 'home_exterior' -> 'town'.",
        transition_start,
        30000,
    )
    wait_for_web_package_loaded(
        cdp,
        "town-map",
        "town package",
        "WebAssetPackage: package 'town-map' loaded",
        transition_start,
        30000,
    )
    cdp.wait_for_new_log("town loaded", "MapManager: 已加载地图 'town'", transition_start, 30000)
    wait_for_runtime_player(cdp, "town", 20000)
    cdp.wait_ms(900)
    cdp.screenshot(output_dir / f"{label}-town-entry.png")


def enter_home_interior(cdp: CdpClient, output_dir: Path, label: str) -> None:
    if (read_gameplay_diagnostics(cdp) or {}).get("map") == "home_interior":
        return
    move_player_to(cdp, 323.0, 142.0, map_name="home_exterior", tolerance=3.0, timeout_ms=18000)
    cdp.screenshot(output_dir / f"{label}-home-exterior-entry.png")
    transition_start = len(cdp.state.logs)
    move_player_up(cdp, hold_ms=520)
    cdp.wait_for_new_log(
        f"{label} home exterior to interior",
        "MapTransitionSystem: map transition 'home_exterior' -> 'home_interior'.",
        transition_start,
        30000,
    )
    cdp.wait_for_new_log("home_interior loaded", "MapManager: 已加载地图 'home_interior'", transition_start, 30000)
    wait_for_runtime_player(cdp, "home_interior", 20000)
    cdp.wait_ms(800)
    cdp.screenshot(output_dir / f"{label}-home-interior.png")


def leave_home_interior(cdp: CdpClient, output_dir: Path, label: str) -> None:
    current_map = (read_gameplay_diagnostics(cdp) or {}).get("map")
    if current_map == "home_exterior":
        return
    if current_map != "home_interior":
        raise RuntimeError(f"Expected home_interior before leaving house, got {current_map!r}")

    transition_start = len(cdp.state.logs)
    move_player_down(cdp, hold_ms=260)
    cdp.wait_for_new_log(
        f"{label} home interior to exterior",
        "MapTransitionSystem: map transition 'home_interior' -> 'home_exterior'.",
        transition_start,
        30000,
    )
    cdp.wait_for_new_log("home_exterior loaded", "MapManager: 已加载地图 'home_exterior'", transition_start, 30000)
    wait_for_runtime_player(cdp, "home_exterior", 20000)
    cdp.wait_ms(800)
    cdp.screenshot(output_dir / f"{label}-home-exterior.png")


def reset_home_exterior_via_home_interior(cdp: CdpClient, output_dir: Path, label: str) -> None:
    if (read_gameplay_diagnostics(cdp) or {}).get("map") == "home_exterior":
        enter_home_interior(cdp, output_dir, label)
    leave_home_interior(cdp, output_dir, label)


def open_scripted_shop(cdp: CdpClient, output_dir: Path) -> None:
    focus_gameplay_canvas(cdp)
    move_player_to(cdp, 350.0, 206.0, map_name="home_exterior", tolerance=10.0, timeout_ms=24000)
    move_player_right(cdp, hold_ms=80)
    cdp.wait_ms(300)
    cdp.screenshot(output_dir / "phase26-shop-merchant-approach.png")
    press_interact_until_log(cdp, "shop menu opened", "ShopMenuScene: opened", presses=2, timeout_ms=15000)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / "phase26-shop-open.png")


def close_shop_for_diagnostics(cdp: CdpClient) -> None:
    press_game_key(cdp, *KEY_ESCAPE, settle_ms=900)
    focus_gameplay_canvas(cdp)


def exercise_full_rpg_shop_flow(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    gameplay_before = read_gameplay_diagnostics(cdp) or {}
    gold_before = player_gold(gameplay_before)
    potion_before = inventory_count(gameplay_before, "potion")

    open_scripted_shop(cdp, output_dir)

    buy_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    cdp.wait_for_new_log("shop buy completed", "ShopMenuScene: buy completed", buy_start, 10000)
    close_shop_for_diagnostics(cdp)
    gameplay_after_buy = wait_for_gameplay_condition(
        cdp,
        "shop buy diagnostics",
        lambda gameplay: player_gold(gameplay) < gold_before and inventory_count(gameplay, "potion") == potion_before + 1,
        10000,
    )

    open_scripted_shop(cdp, output_dir)
    sell_start = len(cdp.state.logs)
    for key in (
        KEY_MENU_LEFT,
        KEY_MENU_UP,
        KEY_MENU_RIGHT,
        KEY_MENU_DOWN,
        KEY_MENU_DOWN,
    ):
        press_game_key(cdp, *key)
    for _ in range(5):
        press_game_key(cdp, *KEY_MENU_DOWN, settle_ms=120)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    cdp.wait_for_new_log("shop sell completed", "ShopMenuScene: sell completed", sell_start, 10000)
    close_shop_for_diagnostics(cdp)
    gold_after_buy = player_gold(gameplay_after_buy)
    gameplay_after_sell = wait_for_gameplay_condition(
        cdp,
        "shop sell diagnostics",
        lambda gameplay: player_gold(gameplay) > gold_after_buy and inventory_count(gameplay, "potion") == potion_before,
        10000,
    )

    open_scripted_shop(cdp, output_dir)
    fail_start = len(cdp.state.logs)
    for key in (
        KEY_MENU_RIGHT,
    ):
        press_game_key(cdp, *key)
    for _ in range(9):
        press_game_key(cdp, *KEY_MENU_RIGHT, settle_ms=120)
    press_game_key(cdp, *KEY_MENU_DOWN)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    cdp.wait_for_new_log("shop buy failed", "ShopMenuScene: buy failed", fail_start, 10000)
    close_shop_for_diagnostics(cdp)
    gameplay_after_failure = read_gameplay_diagnostics(cdp) or {}
    if player_gold(gameplay_after_failure) != player_gold(gameplay_after_sell):
        raise RuntimeError(
            f"Shop failed buy changed gold: before={player_gold(gameplay_after_sell)} after={player_gold(gameplay_after_failure)}"
        )

    cdp.screenshot(output_dir / "phase26-shop-after-transactions.png")
    return {
        "gold_before": gold_before,
        "potion_before": potion_before,
        "gold_after_buy": player_gold(gameplay_after_buy),
        "potion_after_buy": inventory_count(gameplay_after_buy, "potion"),
        "gold_after_sell": player_gold(gameplay_after_sell),
        "potion_after_sell": inventory_count(gameplay_after_sell, "potion"),
        "failure_checked": True,
    }


def exercise_full_rpg_quest_accept_flow(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    focus_gameplay_canvas(cdp)
    approach_home_exterior_quest_giver(cdp, timeout_ms=30000)
    move_player_down(cdp, hold_ms=80)
    cdp.wait_ms(300)
    cdp.screenshot(output_dir / "phase26-quest-giver-approach.png")
    press_interact_until_log(cdp, "quest offer opened", "QuestOfferScene: opened", presses=3, timeout_ms=15000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / "phase26-quest-offer-open.png")
    accept_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    cdp.wait_for_new_log(
        "quest accepted",
        "QuestInteractionSystem: quest accepted quest_id='quest.village.goblin_cleanup'.",
        accept_start,
        10000,
    )
    gameplay = wait_for_gameplay_condition(
        cdp,
        "accepted quest diagnostics",
        lambda state: "quest.village.goblin_cleanup" in active_quest_ids(state),
        10000,
    )
    return {
        "accepted": True,
        "progress": quest_progress(gameplay, "quest.village.goblin_cleanup::kill_slimes"),
    }


def exercise_full_rpg_recruit_flow(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    focus_gameplay_canvas(cdp)
    approach_home_exterior_recruit(cdp, timeout_ms=30000)
    move_player_down(cdp, hold_ms=80)
    cdp.wait_ms(300)
    cdp.screenshot(output_dir / "phase26-recruit-approach.png")
    press_interact_until_log(cdp, "recruit offer opened", "RecruitOfferScene: opened", presses=5, timeout_ms=20000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / "phase26-recruit-offer-open.png")
    recruit_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    cdp.wait_for_new_log(
        "party recruited",
        "PartyRecruitmentSystem: recruited actor_id='actor.lyria'.",
        recruit_start,
        10000,
    )
    gameplay = wait_for_gameplay_condition(
        cdp,
        "recruit diagnostics",
        lambda state: "actor.lyria" in party_actor_ids(state, "recruitedActorIds"),
        10000,
    )
    return {
        "recruited_actor": "actor.lyria",
        "active_actor_ids": party_actor_ids(gameplay, "activeActorIds"),
        "recruited_actor_ids": party_actor_ids(gameplay, "recruitedActorIds"),
    }


def exercise_full_rpg_battle_flow(
    cdp: CdpClient,
    output_dir: Path,
    preferred_troop_id: str | None = "troop.slime_single",
    screenshot_prefix: str = "phase25",
) -> dict[str, Any]:
    focus_gameplay_canvas(cdp)
    gameplay_before = read_gameplay_diagnostics(cdp) or {}
    player_before = gameplay_before.get("player") if isinstance(gameplay_before.get("player"), dict) else {}
    gold_before = int(player_before.get("gold") or 0) if isinstance(player_before, dict) else 0

    ensure_town_from_home_exterior(cdp, output_dir, screenshot_prefix)

    battle_start = len(cdp.state.logs)
    encounter = trigger_town_encounter_from_diagnostics(
        cdp,
        output_dir,
        battle_start,
        preferred_troop_id=preferred_troop_id,
        screenshot_prefix=screenshot_prefix,
    )
    cdp.wait_for_new_log("battle encounter trigger", "EnemyEncounterSystem: triggering battle", battle_start, 30000)
    wait_for_web_package_loaded(
        cdp,
        "battle-core",
        "battle core package",
        "WebAssetPackage: package 'battle-core' loaded",
        battle_start,
        30000,
    )
    wait_for_web_package_loaded(
        cdp,
        "vfx-core",
        "vfx core package",
        "WebAssetPackage: package 'vfx-core' loaded",
        battle_start,
        30000,
    )
    battle = wait_for_battle_diagnostics(cdp, 30000)
    enemy = battle.get("enemy")
    if not isinstance(enemy, dict) or int(enemy.get("total") or 0) <= 0:
        raise RuntimeError(f"Battle diagnostics did not report enemies: {battle}")
    cdp.wait_ms(1000)
    cdp.screenshot(output_dir / f"{screenshot_prefix}-battle-entry.png")

    victory_battle = run_battle_to_victory(cdp, output_dir)
    cdp.wait_for_new_log("battle returned to town", "GameScene: Battle ended, outcome=Victory", battle_start, 30000)
    wait_for_runtime_player(cdp, "town", 30000)
    cdp.wait_ms(1000)
    cdp.screenshot(output_dir / f"{screenshot_prefix}-town-after-victory.png")

    gameplay_after = read_gameplay_diagnostics(cdp) or {}
    player_after = gameplay_after.get("player") if isinstance(gameplay_after.get("player"), dict) else {}
    gold_after = int(player_after.get("gold") or 0) if isinstance(player_after, dict) else 0
    if gold_after <= gold_before:
        raise RuntimeError(f"Battle victory did not increase gold: before={gold_before} after={gold_after}")

    return {
        "town_reached": True,
        "encounter_id": encounter.get("encounterId"),
        "encounter_troop_id": encounter.get("troopId"),
        "battle_outcome": victory_battle.get("outcome"),
        "gold_before": gold_before,
        "gold_after": gold_after,
        "last_battle": victory_battle,
    }


def exercise_full_rpg_quest_battle_and_turn_in_flow(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    before = read_gameplay_diagnostics(cdp) or {}
    if "quest.village.goblin_cleanup" not in active_quest_ids(before):
        raise RuntimeError(f"Quest battle flow requires accepted quest: {before}")

    required_slime_kills = 3
    battle_flows: list[dict[str, Any]] = []
    gameplay_after_battle = before
    for battle_index in range(required_slime_kills):
        if quest_progress(gameplay_after_battle, "quest.village.goblin_cleanup::kill_slimes") >= required_slime_kills:
            break
        preferred_troop_id = "troop.slime" if battle_index == 0 else "troop.slime_single"
        battle_flows.append(
            exercise_full_rpg_battle_flow(
                cdp,
                output_dir,
                preferred_troop_id=preferred_troop_id,
                screenshot_prefix=f"phase26-quest-battle-{battle_index + 1}",
            )
        )
        gameplay_after_battle = wait_for_gameplay_condition(
            cdp,
            "quest battle progress",
            lambda state: quest_progress(state, "quest.village.goblin_cleanup::kill_slimes") >= battle_index + 1,
            10000,
        )
        if quest_progress(gameplay_after_battle, "quest.village.goblin_cleanup::kill_slimes") < required_slime_kills:
            ensure_home_exterior_from_town(cdp, output_dir, f"phase26-quest-battle-{battle_index + 1}-reload")
    if quest_progress(gameplay_after_battle, "quest.village.goblin_cleanup::kill_slimes") < required_slime_kills:
        raise RuntimeError(f"Quest battle flow did not reach kill_slimes target: {gameplay_after_battle}")

    ensure_home_exterior_from_town(cdp, output_dir, "phase26-quest")
    focus_gameplay_canvas(cdp)
    approach_home_exterior_quest_giver(cdp, timeout_ms=30000)
    move_player_down(cdp, hold_ms=80)
    cdp.wait_ms(300)
    cdp.screenshot(output_dir / "phase26-quest-turn-in-approach.png")
    press_interact_until_log(
        cdp,
        "quest completed",
        "QuestInteractionSystem: quest completed quest_id='quest.village.goblin_cleanup'",
        presses=3,
        timeout_ms=15000,
    )
    gameplay_after_turn_in = wait_for_gameplay_condition(
        cdp,
        "completed quest diagnostics",
        lambda state: "quest.village.goblin_cleanup" in completed_quest_ids(state),
        10000,
    )
    return {
        "battle_flows": battle_flows,
        "progress_after_battle": quest_progress(gameplay_after_battle, "quest.village.goblin_cleanup::kill_slimes"),
        "completed": True,
        "gold_after_turn_in": player_gold(gameplay_after_turn_in),
        "potion_after_turn_in": inventory_count(gameplay_after_turn_in, "potion"),
    }


def exercise_full_rpg_rest_and_wardrobe_flow(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    enter_home_interior(cdp, output_dir, "phase26-rest")

    gameplay_before_rest = read_gameplay_diagnostics(cdp) or {}
    player_before_rest = actor_runtime_state(gameplay_before_rest, "actor.player")
    time_before_rest = gameplay_time_minutes(gameplay_before_rest)

    move_player_to(cdp, 193.0, 215.0, map_name="home_interior", tolerance=6.0, timeout_ms=24000)
    move_player_right(cdp, hold_ms=90)
    rest_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_INTERACT)
    cdp.wait_for_new_log("rest dialog opened", "RestDialogScene: opened.", rest_start, 10000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / "phase26-rest-open.png")
    press_game_key(cdp, *KEY_MENU_CONFIRM)
    cdp.wait_for_new_log("rest confirmed", "RestSystem: rest confirmed hours=", rest_start, 10000)
    gameplay_after_rest = wait_for_gameplay_condition(
        cdp,
        "rest diagnostics",
        lambda state: gameplay_time_minutes(state) > time_before_rest,
        10000,
    )
    player_after_rest = actor_runtime_state(gameplay_after_rest, "actor.player")
    if player_before_rest and player_after_rest:
        if player_after_rest["currentHp"] < player_before_rest["currentHp"]:
            raise RuntimeError(f"Rest reduced player HP: before={player_before_rest} after={player_after_rest}")
        if player_after_rest["currentMp"] < player_before_rest["currentMp"]:
            raise RuntimeError(f"Rest reduced player MP: before={player_before_rest} after={player_after_rest}")

    gameplay_before_wardrobe = read_gameplay_diagnostics(cdp) or {}
    signature_before = appearance_signature(gameplay_before_wardrobe)
    move_player_to(cdp, 181.0, 72.0, map_name="home_interior", tolerance=10.0, timeout_ms=24000)
    move_player_up(cdp, hold_ms=90)
    wardrobe_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_INTERACT)
    cdp.wait_for_new_log("wardrobe opened", "AppearanceCustomizeScene: opened mode=closet.", wardrobe_start, 10000)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / "phase26-wardrobe-open.png")
    cdp.click_logical(373, 248, hold_ms=80)
    cdp.wait_for_new_log("wardrobe randomized", "AppearanceCustomizeScene: randomized closet appearance.", wardrobe_start, 10000)
    cdp.wait_ms(300)
    cdp.click_logical(373, 283, hold_ms=80)
    cdp.wait_for_new_log("wardrobe confirmed", "AppearanceCustomizeScene: confirmed closet appearance.", wardrobe_start, 10000)
    gameplay_after_wardrobe = wait_for_gameplay_condition(
        cdp,
        "wardrobe appearance diagnostics",
        lambda state: appearance_signature(state) and appearance_signature(state) != signature_before,
        10000,
    )
    cdp.wait_ms(800)
    cdp.screenshot(output_dir / "phase26-wardrobe-after-confirm.png")

    return {
        "time_before_rest": time_before_rest,
        "time_after_rest": gameplay_time_minutes(gameplay_after_rest),
        "player_before_rest": player_before_rest,
        "player_after_rest": player_after_rest,
        "appearance_before": signature_before,
        "appearance_after": appearance_signature(gameplay_after_wardrobe),
    }


def exercise_full_rpg_save_reload_verify(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    gameplay_before_save = read_gameplay_diagnostics(cdp) or {}
    pre_reload_diagnostics = read_web_release_diagnostics(cdp)
    expected_signature = appearance_signature(gameplay_before_save)
    expected_completed = completed_quest_ids(gameplay_before_save)
    expected_recruited = party_actor_ids(gameplay_before_save, "recruitedActorIds")
    expected_map = str(gameplay_before_save.get("map") or "home_interior")

    pre_reload_failures = validate_web_release_diagnostics(
        pre_reload_diagnostics,
        expected_map=expected_map,
        required_package_ids=FULL_RPG_RUNTIME_PACKAGE_IDS,
    )
    if pre_reload_failures:
        raise RuntimeError(f"Full RPG diagnostics gate failed before save reload: {pre_reload_failures}")

    save_slot0(cdp, output_dir, "phase26-full-rpg-save", overwrite=True)

    reload_log_start = len(cdp.state.logs)
    cdp.call("Page.reload", {"ignoreCache": True}, timeout=5.0)
    cdp.wait_for_new_log(
        "phase26 persistent storage after reload",
        "GameApp: Web persistent storage is mounted and populated.",
        reload_log_start,
        30000,
    )
    cdp.wait_ms(800)
    cdp.screenshot(output_dir / "phase26-reload-title.png")
    cdp.click_logical(320, 259)
    cdp.wait_ms(1400)
    cdp.screenshot(output_dir / "phase26-reload-slot-select.png")
    cdp.click_logical(236, 75)
    cdp.wait_for_log("phase26 save load", f"SaveService: 已载入存档 '{expected_map}'", 20000)
    cdp.wait_for_log("phase26 map after load", f"MapManager: 已加载地图 '{expected_map}'", 20000)
    wait_for_runtime_player(cdp, expected_map, 20000)
    cdp.wait_ms(1000)
    cdp.screenshot(output_dir / "phase26-reload-after-load.png")

    gameplay_after_load = wait_for_gameplay_condition(
        cdp,
        "phase26 save reload diagnostics",
        lambda state: (
            state.get("map") == expected_map and
            appearance_signature(state) == expected_signature and
            "quest.village.goblin_cleanup" in completed_quest_ids(state) and
            "actor.lyria" in party_actor_ids(state, "recruitedActorIds")
        ),
        15000,
    )
    return {
        "map": expected_map,
        "appearance_signature": appearance_signature(gameplay_after_load),
        "completed_quests": completed_quest_ids(gameplay_after_load),
        "recruited_actor_ids": party_actor_ids(gameplay_after_load, "recruitedActorIds"),
        "expected_completed_quests": expected_completed,
        "expected_recruited_actor_ids": expected_recruited,
        "pre_reload_diagnostic_gate": {
            "status": "passed",
            "required_packages": list(FULL_RPG_RUNTIME_PACKAGE_IDS),
        },
    }


def exercise_full_rpg_basic_flows(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    reset_home_exterior_via_home_interior(cdp, output_dir, "phase26-rpg-reset")
    shop_flow = exercise_full_rpg_shop_flow(cdp, output_dir)
    quest_accept_flow = exercise_full_rpg_quest_accept_flow(cdp, output_dir)
    reset_home_exterior_via_home_interior(cdp, output_dir, "phase26-rpg-post-quest-reset")
    recruit_flow = exercise_full_rpg_recruit_flow(cdp, output_dir)
    quest_turn_in_flow = exercise_full_rpg_quest_battle_and_turn_in_flow(cdp, output_dir)
    rest_wardrobe_flow = exercise_full_rpg_rest_and_wardrobe_flow(cdp, output_dir)
    save_reload_flow = exercise_full_rpg_save_reload_verify(cdp, output_dir)
    return {
        "status": "full-rpg-basic-flows-passed",
        "shop_flow": shop_flow,
        "quest_accept_flow": quest_accept_flow,
        "recruit_flow": recruit_flow,
        "quest_turn_in_flow": quest_turn_in_flow,
        "rest_wardrobe_flow": rest_wardrobe_flow,
        "save_reload_flow": save_reload_flow,
    }


def resume_gameplay(cdp: CdpClient, output_dir: Path, label: str) -> None:
    cdp.wait_ms(1400)
    cdp.click_logical(312, 57)
    cdp.wait_ms(900)
    cdp.screenshot(output_dir / f"phase14-{label}-after-resume.png")
    cdp.evaluate('document.querySelector("canvas")?.focus()')


def exercise_settings_persistence(cdp: CdpClient, output_dir: Path) -> dict[str, Any]:
    focus_gameplay_canvas(cdp)
    log_start = len(cdp.state.logs)
    press_game_key(cdp, *KEY_PAUSE)
    cdp.wait_for_new_log("settings pause menu", "GameScene: pause menu opened.", log_start, 10000)
    cdp.wait_ms(500)
    cdp.screenshot(output_dir / "phase19-settings-before.png")

    cdp.click_logical(247, 257, hold_ms=80)
    cdp.wait_ms(400)
    cdp.screenshot(output_dir / "phase19-settings-music-down.png")
    press_game_key(cdp, *KEY_ESCAPE, settle_ms=700)
    cdp.wait_for_new_log(
        "user settings persistent sync",
        "UserSettingsService: Web persistent settings sync completed.",
        log_start,
        10000,
    )
    settings = read_user_settings_file(cdp)
    if not isinstance(settings, dict):
        raise RuntimeError("User settings file was not written to persistent storage.")
    music = float(settings.get("audio", {}).get("music_volume", 1.0))
    if music >= 0.5:
        raise RuntimeError(f"Music volume setting did not decrease: {settings}")
    return settings


def verify_user_settings_restored(cdp: CdpClient, expected: dict[str, Any]) -> dict[str, Any]:
    cdp.wait_for("user settings diagnostics", lambda: read_user_settings_diagnostics(cdp) is not None, 10000)
    restored = read_user_settings_file(cdp)
    diagnostics = read_user_settings_diagnostics(cdp)
    if not isinstance(restored, dict) or not isinstance(diagnostics, dict):
        raise RuntimeError(f"User settings were not restored after reload: file={restored} diagnostics={diagnostics}")

    expected_music = float(expected.get("audio", {}).get("music_volume", -1.0))
    restored_music = float(restored.get("audio", {}).get("music_volume", -2.0))
    diagnostic_music = float(diagnostics.get("musicVolume", -3.0))
    if abs(restored_music - expected_music) > 0.001 or abs(diagnostic_music - expected_music) > 0.001:
        raise RuntimeError(
            "User settings music volume did not survive reload: "
            f"expected={expected_music} restored={restored_music} diagnostics={diagnostics}"
        )
    return {"file": restored, "diagnostics": diagnostics}


def read_render_capabilities(cdp: CdpClient) -> dict[str, Any] | None:
    capabilities = cdp.evaluate(
        """(() => {
            const diagnostics = globalThis.TinyFarmRPGWebReleaseDiagnostics;
            const render = diagnostics && diagnostics.renderCapabilities;
            if (!render) return null;
            return {
                platform: String(render.platform || ""),
                defaultFramebufferSrgb: !!render.defaultFramebufferSrgb,
                floatColorFramebuffers: !!render.floatColorFramebuffers,
                linearFloatFiltering: !!render.linearFloatFiltering,
                hdrPostProcessing: !!render.hdrPostProcessing,
                bloom: !!render.bloom,
                emissive: !!render.emissive,
                maxTextureSize: Number(render.maxTextureSize || 0),
                maxRenderbufferSize: Number(render.maxRenderbufferSize || 0),
                maxSamples: Number(render.maxSamples || 0)
            };
        })()""",
        timeout=10.0,
    )
    return capabilities if isinstance(capabilities, dict) else None


def validate_render_capabilities(capabilities: dict[str, Any] | None) -> list[str]:
    if not capabilities:
        return ["Web release render capabilities were not published to JS diagnostics."]

    failures: list[str] = []
    expected_false = {
        "defaultFramebufferSrgb": capabilities.get("defaultFramebufferSrgb"),
        "floatColorFramebuffers": capabilities.get("floatColorFramebuffers"),
        "linearFloatFiltering": capabilities.get("linearFloatFiltering"),
        "hdrPostProcessing": capabilities.get("hdrPostProcessing"),
        "bloom": capabilities.get("bloom"),
        "emissive": capabilities.get("emissive"),
    }
    if capabilities.get("platform") != "webgl2":
        failures.append(f"render platform expected webgl2, got {capabilities.get('platform')!r}")
    for key, actual in expected_false.items():
        if actual is not False:
            failures.append(f"{key} expected false for the current Web release policy, got {actual!r}")
    if int(capabilities.get("maxTextureSize") or 0) < 1024:
        failures.append(f"maxTextureSize is unexpectedly low: {capabilities.get('maxTextureSize')!r}")
    if int(capabilities.get("maxRenderbufferSize") or 0) < 1024:
        failures.append(f"maxRenderbufferSize is unexpectedly low: {capabilities.get('maxRenderbufferSize')!r}")
    return failures


def latest_log_text(logs: list[dict[str, Any]], needle: str) -> str | None:
    for entry in reversed(logs):
        text = str(entry.get("text", ""))
        if needle in text:
            return text
    return None


def collect_webgl_error_logs(logs: list[dict[str, Any]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for entry in logs:
        text = str(entry.get("text", ""))
        lower = text.lower()
        if "gl_invalid" in lower or "invalid_framebuffer_operation" in lower:
            result.append(entry)
        elif "webgl" in lower and ("invalid" in lower or "error" in lower):
            result.append(entry)
        elif "opengl" in lower and "error" in lower:
            result.append(entry)
    return result


def summarize_warnings(logs: list[dict[str, Any]]) -> dict[str, Any]:
    warnings = [entry for entry in logs if entry.get("level") in {"warning", "warn"}]
    categories = {
        "font_fallback": 0,
        "audio": 0,
        "optional_assets": 0,
        "other": 0,
    }
    unknown: list[dict[str, Any]] = []
    for entry in warnings:
        text = str(entry.get("text", ""))
        lower = text.lower()
        if "font" in lower or "字体" in text:
            categories["font_fallback"] += 1
        elif "audio" in lower or "音频" in text or "miniaudio" in lower:
            categories["audio"] += 1
        elif "optional" in lower or "town.tmj" in lower or "资源映射" in text:
            categories["optional_assets"] += 1
        else:
            categories["other"] += 1
            unknown.append(entry)
    return {
        "total": len(warnings),
        "categories": categories,
        "unknown_tail": unknown[-20:],
    }


def summarize_persistent_storage_logs(logs: list[dict[str, Any]]) -> dict[str, Any]:
    summary = {
        "sync_started": 0,
        "sync_completed": 0,
        "sync_failed": 0,
        "from_browser_completed": 0,
        "to_browser_completed": 0,
        "settings_sync_completed": 0,
        "save_sync_completed": 0,
    }
    for entry in logs:
        text = str(entry.get("text", ""))
        if "TinyFarmRPG persistent FS sync started" in text:
            summary["sync_started"] += 1
        if "TinyFarmRPG persistent FS sync completed" in text:
            summary["sync_completed"] += 1
            if "success=false" in text:
                summary["sync_failed"] += 1
            if "direction=from_browser" in text and "success=true" in text:
                summary["from_browser_completed"] += 1
            if "direction=to_browser" in text and "success=true" in text:
                summary["to_browser_completed"] += 1
        if "UserSettingsService: Web persistent settings sync completed." in text:
            summary["settings_sync_completed"] += 1
        if "SaveService: Web persistent storage sync completed after async save." in text:
            summary["save_sync_completed"] += 1
    return summary


def summarize_performance_budget(timings: dict[str, int]) -> dict[str, Any]:
    results: dict[str, Any] = {}
    exceeded: list[str] = []
    for name, budget in PERFORMANCE_BUDGET_MS.items():
        actual = int(timings.get(name, 0))
        passed = actual <= budget
        results[name] = {
            "actual_ms": actual,
            "budget_ms": budget,
            "status": "passed" if passed else "warning",
        }
        if not passed:
            exceeded.append(name)
    return {
        "status": "passed" if not exceeded else "warning",
        "results": results,
        "exceeded": exceeded,
    }


def run_gameplay_smoke(cdp: CdpClient, url: str, output_dir: Path, profile: str) -> dict[str, Any]:
    started = now_ms()
    screenshots = {
        "title": output_dir / "phase14-chromium-title.png",
        "map": output_dir / "phase14-chromium-map.png",
        "after_load": output_dir / "phase14-chromium-after-load.png",
    }

    for method in ("Page.enable", "Runtime.enable", "Log.enable", "Network.enable"):
        cdp.call(method)

    cdp.call("Page.navigate", {"url": url}, timeout=5.0)
    cdp.wait_for("canvas", lambda: bool(cdp.evaluate('!!document.querySelector("canvas")')), 120000)
    cdp.wait_for_log("persistent storage", "GameApp: Web persistent storage is mounted and populated.", 30000)
    cdp.wait_for("render capabilities", lambda: read_render_capabilities(cdp) is not None, 30000)
    render_capabilities = read_render_capabilities(cdp)
    render_failures = validate_render_capabilities(render_capabilities)
    if render_failures:
        raise RuntimeError(f"WebGL2 render capability gate failed: {render_failures}")
    title_ready = now_ms()
    cdp.screenshot(screenshots["title"])

    cdp.click_logical(320, 213)
    cdp.wait_for_log("audio core package", "WebAssetPackageRegistry: package 'audio-core' ready", 20000)
    cdp.wait_for_log("registered audio preload", "ResourceManager: registered audio preload complete", 20000)
    cdp.wait_for_log("shared UI package", "WebAssetPackageRegistry: package 'shared-ui' ready", 20000)
    cdp.wait_ms(1500)
    cdp.click_logical(374, 274)
    cdp.wait_for_log("RPG core package", "WebAssetPackageRegistry: package 'rpg-core' ready", 20000)
    cdp.wait_for_log("home map package", "WebAssetPackage: package 'home-map' loaded", 20000)
    cdp.wait_for_log("home_exterior", "MapManager: 已加载地图 'home_exterior'", 20000)
    cdp.wait_for_log("gameplay ready", "GameScene: gameplay ready.", 20000)
    map_ready = now_ms()
    cdp.screenshot(screenshots["map"])

    saved_settings = exercise_settings_persistence(cdp, output_dir)
    exercise_menu_controls(cdp, output_dir)
    trigger_tool_action(cdp, output_dir)
    exercise_home_round_trip(cdp, output_dir)
    trigger_merchant_dialogue(cdp, output_dir)
    interaction_ready = now_ms()

    move_player_down(cdp, hold_ms=250)
    cdp.screenshot(output_dir / "phase14-chromium-after-dialogue-dismiss.png")
    first_position = save_slot0(cdp, output_dir, "initial-save", overwrite=False)
    resume_gameplay(cdp, output_dir, "initial-save")
    approach_home_exterior_town_gate(cdp, timeout_ms=30000)
    cdp.screenshot(output_dir / "phase14-moved-save-position.png")
    moved_position = save_slot0(cdp, output_dir, "moved-save", overwrite=True, changed_from=first_position)
    movement_delta = {
        "x": moved_position["x"] - first_position["x"],
        "y": moved_position["y"] - first_position["y"],
    }
    if abs(movement_delta["x"]) + abs(movement_delta["y"]) < 1.0:
        raise RuntimeError(f"Player did not move far enough: {first_position} -> {moved_position}")

    reload_log_start = len(cdp.state.logs)
    reload_started = now_ms()
    cdp.call("Page.reload", {"ignoreCache": True}, timeout=5.0)
    cdp.wait_for_new_log(
        "persistent storage after reload",
        "GameApp: Web persistent storage is mounted and populated.",
        reload_log_start,
        30000,
    )
    cdp.screenshot(output_dir / "phase14-reload-title.png")
    restored_settings = verify_user_settings_restored(cdp, saved_settings)
    write_corrupt_save_slot(cdp)
    cdp.click_logical(320, 259)
    cdp.wait_for_new_log("corrupt save slot skipped", "SaveSlotSelectScene: slot 1 summary 读取失败", reload_log_start, 10000)
    cdp.wait_ms(1500)
    cdp.screenshot(output_dir / "phase14-reload-slot-select.png")
    cdp.click_logical(236, 75)
    cdp.wait_for_log("save load", "SaveService: 已载入存档 'home_exterior'", 20000)
    cdp.wait_for_log("home map after load", "MapManager: 已加载地图 'home_exterior'", 20000)
    load_ready = now_ms()
    cdp.screenshot(screenshots["after_load"])
    loaded_player = read_save_player(cdp)
    if not isinstance(loaded_player, dict) or loaded_player.get("map_name") != "home_exterior":
        raise RuntimeError(f"Loaded save did not report home_exterior: {loaded_player}")

    full_rpg_basic_flows: dict[str, Any] | None = None
    full_rpg_started: int | None = None
    full_rpg_finished: int | None = None
    if profile == "full-rpg":
        full_rpg_started = now_ms()
        full_rpg_basic_flows = exercise_full_rpg_basic_flows(cdp, output_dir)
        full_rpg_finished = now_ms()

    package_responses = [
        response for response in cdp.state.responses
        if str(response.get("url", "")).endswith(".tfpack")
    ]
    required_packages = {
        "shared-ui.tfpack": False,
        "rpg-core.tfpack": False,
        "home-map.tfpack": False,
        "audio-core.tfpack": False,
    }
    if profile == "full-rpg":
        required_packages.update({
            "town-map.tfpack": False,
            "battle-core.tfpack": False,
            "vfx-core.tfpack": False,
        })
    for response in package_responses:
        url_text = str(response.get("url", ""))
        status = response.get("status")
        for package_name in required_packages:
            if package_name in url_text and status in {200, 304}:
                required_packages[package_name] = True
    missing_packages = sorted(name for name, seen in required_packages.items() if not seen)
    if missing_packages:
        raise RuntimeError(f"Package responses missing from smoke: {missing_packages}")

    render_capabilities = read_render_capabilities(cdp) or render_capabilities
    render_failures = validate_render_capabilities(render_capabilities)
    if render_failures:
        raise RuntimeError(f"WebGL2 render capability gate failed after reload: {render_failures}")

    webgl_errors = collect_webgl_error_logs(cdp.state.logs)
    if webgl_errors:
        raise RuntimeError(f"Browser smoke saw WebGL error logs: {webgl_errors[-20:]}")

    vfx_policy_log = latest_log_text(cdp.state.logs, "Web release VFX policy")
    if not vfx_policy_log or "backend=" not in vfx_policy_log:
        raise RuntimeError("Web release VFX policy log is missing or malformed.")

    audio_preload_log = latest_log_text(cdp.state.logs, "ResourceManager: registered audio preload complete")
    if not audio_preload_log or "failed=0" not in audio_preload_log:
        raise RuntimeError(f"Registered audio preload did not converge cleanly: {audio_preload_log!r}")

    audio_policy_log = latest_log_text(cdp.state.logs, "Web audio release policy")
    if not audio_policy_log:
        raise RuntimeError("Web audio release policy log is missing.")

    errors = [
        entry for entry in cdp.state.logs
        if entry.get("level") in {"error", "pageerror"} and "favicon.ico" not in str(entry.get("text", ""))
    ]
    if errors or cdp.state.exceptions:
        raise RuntimeError(f"Browser smoke saw console errors: {errors} exceptions={cdp.state.exceptions}")

    timings = {
        "title_interactive": human_ms(started, title_ready),
        "new_game_to_map": human_ms(title_ready, map_ready),
        "gameplay_flow": human_ms(map_ready, interaction_ready),
        "reload_load_to_map": human_ms(reload_started, load_ready),
    }
    if full_rpg_started is not None and full_rpg_finished is not None:
        timings["full_rpg_basic_flows"] = human_ms(full_rpg_started, full_rpg_finished)
    warning_summary = summarize_warnings(cdp.state.logs)
    performance_budget = summarize_performance_budget(timings)
    persistent_storage = read_persistent_storage_diagnostics(cdp)
    persistent_storage_logs = summarize_persistent_storage_logs(cdp.state.logs)
    if persistent_storage_logs["save_sync_completed"] < 2:
        raise RuntimeError(f"Expected at least two async save sync completions: {persistent_storage_logs}")
    if persistent_storage_logs["settings_sync_completed"] < 1:
        raise RuntimeError(f"Expected user settings sync completion: {persistent_storage_logs}")

    diagnostics = read_web_release_diagnostics(cdp)
    current_gameplay = read_gameplay_diagnostics(cdp) or {}
    expected_diagnostic_map = str(current_gameplay.get("map") or "home_exterior")
    required_diagnostic_packages = DEMO_RUNTIME_PACKAGE_IDS
    diagnostic_failures = validate_web_release_diagnostics(
        diagnostics,
        expected_map=expected_diagnostic_map,
        required_package_ids=required_diagnostic_packages,
    )
    if diagnostic_failures:
        raise RuntimeError(f"Web release diagnostics gate failed: {diagnostic_failures}")

    covered_flows = [
        "new_game_character_confirm",
        "home_exterior_movement",
        "home_exterior_to_home_interior_round_trip",
        "inventory_open_close",
        "hotbar_open_close",
        "pause_open_close",
        "settings_change_reload_restore",
        "primary_tool_action",
        "scripted_merchant_dialogue",
        "save_reload_load",
        "corrupt_save_slot_skip",
        "web_release_diagnostics_snapshot",
    ]
    full_rpg_profile: dict[str, Any] | None = None
    if profile == "full-rpg":
        covered_flows.extend([
            "full_rpg_profile_diagnostics_gate",
            "home_exterior_to_town",
            "town_enemy_encounter",
            "battle_skill_vfx",
            "battle_victory_return_to_map",
            "battle_reward_writeback",
            "shop_buy_sell_failure_feedback",
            "quest_accept_progress_turn_in_reward",
            "recruit_accept_party_writeback",
            "rest_recovery_time_advance",
            "wardrobe_appearance_change",
            "full_rpg_save_reload_verify",
        ])
        full_rpg_profile = {
            "status": "basic-rpg-flows-ready",
            "basic_flows": full_rpg_basic_flows,
            "pending_flows": [
                "battle_attack_item_guard_escape_matrix",
                "battle_defeat_flow",
                "battle_defeated_encounter_save_reload_matrix",
            ],
        }

    return {
        "profile": profile,
        "timings_ms": timings,
        "performance_budget": performance_budget,
        "render_capabilities": render_capabilities,
        "diagnostics": diagnostics,
        "diagnostic_gate": {
            "status": "passed",
            "required_packages": list(required_diagnostic_packages),
        },
        "full_rpg_profile": full_rpg_profile,
        "persistent_storage": persistent_storage,
        "persistent_storage_logs": persistent_storage_logs,
        "user_settings": {
            "saved": saved_settings,
            "restored": restored_settings,
        },
        "vfx_policy": {
            "log": vfx_policy_log,
            "effekseer": "effekseer_enabled=true" in vfx_policy_log,
            "backend": "null_vfx_backend" if "backend=null_vfx_backend" in vfx_policy_log else "effekseer",
        },
        "audio_policy": {
            "preload_log": audio_preload_log,
            "deferred_log": audio_policy_log,
        },
        "positions": {
            "before_move": first_position,
            "after_move": moved_position,
            "delta": movement_delta,
        },
        "covered_flows": covered_flows,
        "package_responses": package_responses,
        "screenshots": {key: str(path) for key, path in screenshots.items()},
        "warning_count": sum(1 for entry in cdp.state.logs if entry.get("level") in {"warning", "warn"}),
        "warning_summary": warning_summary,
        "warning_logs": [
            entry for entry in cdp.state.logs
            if entry.get("level") in {"warning", "warn"}
        ][-40:],
        "interesting_logs": [
            entry for entry in cdp.state.logs
            if any(
                needle in str(entry.get("text", ""))
                for needle in (
                    "WebAssetPackage",
                    "WebAssetPackageRegistry",
                    "GameApp: Web persistent",
                    "TinyFarmRPG persistent FS sync",
                    "UserSettingsService",
                    "AudioPlayer",
                    "ResourceManager: registered audio preload",
                    "Web audio release policy",
                    "GLRenderer: Web release render capabilities",
                    "Web release VFX policy",
                    "HDR post-processing disabled",
                    "DialoguePresentationController",
                    "GameScene: inventory menu opened",
                    "GameScene: hotbar toggle accepted",
                    "GameScene: pause menu opened",
                    "GameScene: gameplay ready",
                    "MapTransitionSystem",
                    "触发工具动作",
                    "尝试切换工具",
                    "MapManager",
                    "SaveService",
                    "SaveSlotSelectScene",
                    "home_exterior",
                    "home_interior",
                )
            )
        ][-80:],
    }


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


def browser_version(executable: Path) -> str:
    app_version = mac_app_version(executable)
    if app_version is not None:
        return app_version
    try:
        return subprocess.check_output([str(executable), "--version"], text=True, stderr=subprocess.STDOUT).strip()
    except (OSError, subprocess.CalledProcessError) as exc:
        return f"unavailable: {exc}"


def chromium_profile_parent() -> Path | None:
    private_tmp = Path("/private/tmp")
    if private_tmp.is_dir() and os.access(private_tmp, os.W_OK):
        return private_tmp
    return None


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Build, validate, serve, and smoke-test TinyFarmRPG Web.")
    parser.add_argument("--build-dir", type=Path, default=root / "build" / "web-release")
    parser.add_argument("--configure", action="store_true", help="Run emcmake configure before building.")
    parser.add_argument("--skip-build", action="store_true", help="Skip cmake --build.")
    parser.add_argument("--skip-gate", action="store_true", help="Skip validate_web_release.py.")
    parser.add_argument("--profile", choices=("demo", "full-rpg"), default="demo")
    parser.add_argument("--browser", type=Path, help="Chromium-family browser executable.")
    parser.add_argument("--headed", action="store_true", help="Run a visible browser window instead of headless Chrome.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    output_dir = (args.output_dir or build_dir / "web-smoke").resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    json_output = (args.json_output or output_dir / "chromium-smoke.json").resolve()
    stale_failure_output = output_dir / "chromium-smoke-failed.json"
    if stale_failure_output.exists():
        stale_failure_output.unlink()

    if args.configure:
        configure_web_build(root, build_dir, args.jobs)
    elif not args.skip_build:
        build_web(root, build_dir, args.jobs)

    package_web_assets(root, build_dir)

    if not args.skip_gate:
        validate_web(root, build_dir, output_dir / "release-gate.json")

    cache_path = build_dir / "CMakeCache.txt"
    cross_origin_isolated = cmake_bool(cache_value(cache_path, "TF_WEB_ENABLE_PTHREADS"))
    port = args.port if args.port != 0 else find_free_port()
    browser = args.browser.resolve() if args.browser else default_browser()
    url = f"http://{args.host}:{port}/TinyFarmRPG-Web.html?phase14={int(time.time())}"

    profile_parent = chromium_profile_parent()
    temp_kwargs: dict[str, str] = {"prefix": "tinyfarm-web-smoke-"}
    if profile_parent is not None:
        temp_kwargs["dir"] = str(profile_parent)

    with tempfile.TemporaryDirectory(**temp_kwargs) as profile_dir:
        with ReleaseServer(build_dir, args.host, port, cross_origin_isolated):
            header_paths = [
                "/TinyFarmRPG-Web.html",
                "/TinyFarmRPG-Web.js",
                "/TinyFarmRPG-Web.wasm",
                "/TinyFarmRPG-Web.data",
                "/web-packages/shared-ui.tfpack",
                "/web-packages/rpg-core.tfpack",
                "/web-packages/home-map.tfpack",
                "/web-packages/town-map.tfpack",
                "/web-packages/battle-core.tfpack",
                "/web-packages/vfx-core.tfpack",
                "/web-packages/audio-core.tfpack",
            ]
            headers = served_headers(f"http://{args.host}:{port}", header_paths)
            header_failures = validate_headers(headers, cross_origin_isolated)
            if header_failures:
                raise RuntimeError("; ".join(header_failures))

            with Chromium(browser, find_free_port(), Path(profile_dir), headless=not args.headed) as chromium:
                cdp = CdpClient(chromium.page_websocket_url())
                try:
                    gameplay = run_gameplay_smoke(cdp, url, output_dir, args.profile)
                except Exception as exc:
                    failure_output = output_dir / "chromium-smoke-failed.json"
                    failure_output.write_text(
                        json.dumps(
                            {
                                "status": "failed",
                                "error": str(exc),
                                "logs": cdp.state.logs,
                                "responses": cdp.state.responses,
                                "exceptions": cdp.state.exceptions,
                            },
                            indent=2,
                            ensure_ascii=False,
                        )
                        + "\n",
                        encoding="utf-8",
                    )
                    print(f"Chromium smoke failure details: {failure_output}")
                    raise
                finally:
                    cdp.close()

    summary = {
        "status": "passed",
        "build_dir": str(build_dir),
        "url": url,
        "browser": {
            "path": str(browser),
            "version": browser_version(browser),
        },
        "headers": headers,
        "cross_origin_isolated": cross_origin_isolated,
        "profile": args.profile,
        "gameplay": gameplay,
    }
    json_output.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print("TinyFarmRPG Chromium Web smoke passed")
    print(f"- browser: {summary['browser']['version']}")
    print(f"- profile: {summary['profile']}")
    print(f"- title interactive: {gameplay['timings_ms']['title_interactive']} ms")
    print(f"- new game to map: {gameplay['timings_ms']['new_game_to_map']} ms")
    print(f"- performance budget: {gameplay['performance_budget']['status']}")
    print(f"- movement delta: {gameplay['positions']['delta']}")
    print(f"- report: {json_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
