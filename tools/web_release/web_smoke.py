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

    def click_logical(self, x: float, y: float) -> None:
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
        self.click(float(point["x"]), float(point["y"]))

    def click(self, x: float, y: float) -> None:
        params = {"x": x, "y": y, "button": "left", "clickCount": 1}
        self.call("Input.dispatchMouseEvent", {"type": "mousePressed", **params})
        self.call("Input.dispatchMouseEvent", {"type": "mouseReleased", **params})

    def key(self, code: str, key: str, windows_key_code: int, hold_ms: int = 0) -> None:
        base = {
            "key": key,
            "code": code,
            "windowsVirtualKeyCode": windows_key_code,
            "nativeVirtualKeyCode": windows_key_code,
        }
        self.call("Input.dispatchKeyEvent", {"type": "rawKeyDown", **base})
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
        "/web-packages/home-map.tfpack": "application/octet-stream",
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


def read_save_position(cdp: CdpClient) -> dict[str, float] | None:
    return cdp.evaluate(
        f"""(() => {{
            if (typeof FS === "undefined") return null;
            const path = "{SAVE_PATH}";
            if (!FS.analyzePath(path).exists) return null;
            const text = new TextDecoder("utf-8").decode(FS.readFile(path));
            const json = JSON.parse(text);
            return json.player && json.player.position ? json.player.position : null;
        }})()""",
        timeout=10.0,
    )


def position_distance(first: dict[str, float], second: dict[str, float]) -> float:
    return abs(second["x"] - first["x"]) + abs(second["y"] - first["y"])


def wait_for_save_position(
    cdp: CdpClient,
    timeout_ms: int,
    changed_from: dict[str, float] | None = None,
) -> dict[str, float]:
    end = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < end:
        position = read_save_position(cdp)
        if isinstance(position, dict) and "x" in position and "y" in position:
            normalized = {"x": float(position["x"]), "y": float(position["y"])}
            if changed_from is None or position_distance(changed_from, normalized) >= 1.0:
                return normalized
        cdp.wait_ms(200)
    raise TimeoutError(f"Timed out waiting for {SAVE_PATH}")


def save_slot0(
    cdp: CdpClient,
    output_dir: Path,
    label: str,
    overwrite: bool = False,
    changed_from: dict[str, float] | None = None,
) -> dict[str, float]:
    sync_log_start = len(cdp.state.logs)
    cdp.click_logical(616, 31)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / f"phase14-{label}-pause.png")
    cdp.click_logical(312, 91)
    cdp.wait_ms(700)
    cdp.screenshot(output_dir / f"phase14-{label}-slot-select.png")
    cdp.click_logical(236, 75)
    if overwrite:
        cdp.wait_ms(500)
        cdp.screenshot(output_dir / f"phase14-{label}-overwrite.png")
        cdp.click_logical(240, 185)
    position = wait_for_save_position(cdp, 10000, changed_from=changed_from)
    cdp.wait_for_new_log(
        "persistent save sync",
        "SaveService: Web persistent storage sync completed after async save.",
        sync_log_start,
        10000,
    )
    return position


def move_player(cdp: CdpClient, code: str, key: str, windows_key_code: int, hold_ms: int = 1400) -> None:
    cdp.evaluate('document.querySelector("canvas")?.focus()')
    cdp.key(code, key, windows_key_code, hold_ms=hold_ms)
    cdp.wait_ms(500)


def move_player_down(cdp: CdpClient, hold_ms: int = 1400) -> None:
    move_player(cdp, "KeyS", "s", 83, hold_ms=hold_ms)


def move_player_up(cdp: CdpClient, hold_ms: int = 1400) -> None:
    move_player(cdp, "KeyW", "w", 87, hold_ms=hold_ms)


def resume_gameplay(cdp: CdpClient, output_dir: Path, label: str) -> None:
    cdp.wait_ms(1400)
    cdp.click_logical(312, 57)
    cdp.wait_ms(900)
    cdp.screenshot(output_dir / f"phase14-{label}-after-resume.png")
    cdp.evaluate('document.querySelector("canvas")?.focus()')


def run_gameplay_smoke(cdp: CdpClient, url: str, output_dir: Path) -> dict[str, Any]:
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
    title_ready = now_ms()
    cdp.screenshot(screenshots["title"])

    cdp.click_logical(320, 213)
    cdp.wait_ms(1500)
    cdp.click_logical(374, 274)
    cdp.wait_for_log("home map package", "WebAssetPackage: package 'home-map' loaded", 20000)
    cdp.wait_for_log("home_exterior", "MapManager: 已加载地图 'home_exterior'", 20000)
    map_ready = now_ms()
    cdp.screenshot(screenshots["map"])

    move_player_down(cdp)
    cdp.screenshot(output_dir / "phase14-chromium-after-dialogue-dismiss.png")
    first_position = save_slot0(cdp, output_dir, "initial-save", overwrite=False)
    resume_gameplay(cdp, output_dir, "initial-save")
    move_player_up(cdp)
    moved_position = save_slot0(cdp, output_dir, "moved-save", overwrite=True, changed_from=first_position)
    movement_delta = {
        "x": moved_position["x"] - first_position["x"],
        "y": moved_position["y"] - first_position["y"],
    }
    if abs(movement_delta["x"]) + abs(movement_delta["y"]) < 1.0:
        raise RuntimeError(f"Player did not move far enough: {first_position} -> {moved_position}")

    reload_log_start = len(cdp.state.logs)
    cdp.call("Page.reload", {"ignoreCache": True}, timeout=5.0)
    cdp.wait_for_new_log(
        "persistent storage after reload",
        "GameApp: Web persistent storage is mounted and populated.",
        reload_log_start,
        30000,
    )
    cdp.screenshot(output_dir / "phase14-reload-title.png")
    cdp.click_logical(320, 259)
    cdp.wait_ms(1500)
    cdp.screenshot(output_dir / "phase14-reload-slot-select.png")
    cdp.click_logical(236, 75)
    cdp.wait_for_log("save load", "SaveService: 已载入存档 'home_exterior'", 20000)
    cdp.wait_for_log("home map after load", "MapManager: 已加载地图 'home_exterior'", 20000)
    load_ready = now_ms()
    cdp.screenshot(screenshots["after_load"])

    package_responses = [
        response for response in cdp.state.responses
        if str(response.get("url", "")).endswith(".tfpack")
    ]
    if not any("home-map.tfpack" in str(response.get("url", "")) and response.get("status") == 200 for response in package_responses):
        raise RuntimeError("home-map.tfpack was not observed as a 200 response.")

    errors = [
        entry for entry in cdp.state.logs
        if entry.get("level") in {"error", "pageerror"} and "favicon.ico" not in str(entry.get("text", ""))
    ]
    if errors or cdp.state.exceptions:
        raise RuntimeError(f"Browser smoke saw console errors: {errors} exceptions={cdp.state.exceptions}")

    return {
        "timings_ms": {
            "title_interactive": human_ms(started, title_ready),
            "new_game_to_map": human_ms(title_ready, map_ready),
            "reload_load_to_map": human_ms(map_ready, load_ready),
        },
        "positions": {
            "before_move": first_position,
            "after_move": moved_position,
            "delta": movement_delta,
        },
        "package_responses": package_responses,
        "screenshots": {key: str(path) for key, path in screenshots.items()},
        "warning_count": sum(1 for entry in cdp.state.logs if entry.get("level") in {"warning", "warn"}),
        "interesting_logs": [
            entry for entry in cdp.state.logs
            if any(
                needle in str(entry.get("text", ""))
                for needle in (
                    "WebAssetPackage",
                    "GameApp: Web persistent",
                    "AudioPlayer",
                    "MapManager",
                    "SaveService",
                    "home_exterior",
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

    if args.configure:
        configure_web_build(root, build_dir, args.jobs)
    elif not args.skip_build:
        build_web(root, build_dir, args.jobs)

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
                "/web-packages/home-map.tfpack",
            ]
            headers = served_headers(f"http://{args.host}:{port}", header_paths)
            header_failures = validate_headers(headers, cross_origin_isolated)
            if header_failures:
                raise RuntimeError("; ".join(header_failures))

            with Chromium(browser, find_free_port(), Path(profile_dir), headless=not args.headed) as chromium:
                cdp = CdpClient(chromium.page_websocket_url())
                try:
                    gameplay = run_gameplay_smoke(cdp, url, output_dir)
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
        "gameplay": gameplay,
    }
    json_output.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print("TinyFarmRPG Chromium Web smoke passed")
    print(f"- browser: {summary['browser']['version']}")
    print(f"- title interactive: {gameplay['timings_ms']['title_interactive']} ms")
    print(f"- new game to map: {gameplay['timings_ms']['new_game_to_map']} ms")
    print(f"- movement delta: {gameplay['positions']['delta']}")
    print(f"- report: {json_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
