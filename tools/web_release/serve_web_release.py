#!/usr/bin/env python3
"""Serve the TinyFarmRPG Web build with release-preview headers."""

from __future__ import annotations

import argparse
import http.server
import mimetypes
import os
import socketserver
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def cache_value(cache_path: Path, key: str) -> str | None:
    if not cache_path.exists():
        return None
    for raw_line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw_line.strip()
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        lhs, value = line.split("=", 1)
        cache_key = lhs.split(":", 1)[0]
        if cache_key == key:
            return value.strip()
    return None


def cmake_bool(value: str | None) -> bool:
    return value is not None and value.upper() in {"1", "ON", "TRUE", "YES"}


def make_handler(directory: Path, cross_origin_isolated: bool) -> type[http.server.SimpleHTTPRequestHandler]:
    class ReleasePreviewHandler(http.server.SimpleHTTPRequestHandler):
        extensions_map = {
            **mimetypes.types_map,
            ".data": "application/octet-stream",
            ".tfpack": "application/octet-stream",
            ".wasm": "application/wasm",
        }

        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=os.fspath(directory), **kwargs)

        def end_headers(self) -> None:
            self.send_header("Cache-Control", "no-cache")
            if cross_origin_isolated:
                self.send_header("Cross-Origin-Opener-Policy", "same-origin")
                self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            super().end_headers()

    return ReleasePreviewHandler


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve TinyFarmRPG Web release artifacts.")
    parser.add_argument("--build-dir", type=Path, default=repo_root() / "build" / "web-release")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument(
        "--cross-origin-isolated",
        action="store_true",
        help="Force COOP/COEP headers. Auto-enabled when TF_WEB_ENABLE_PTHREADS=ON.",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    if not build_dir.exists():
        raise SystemExit(f"Build directory does not exist: {build_dir}")

    cache_path = build_dir / "CMakeCache.txt"
    needs_isolation = cmake_bool(cache_value(cache_path, "TF_WEB_ENABLE_PTHREADS"))
    cross_origin_isolated = args.cross_origin_isolated or needs_isolation

    handler = make_handler(build_dir, cross_origin_isolated)
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer((args.host, args.port), handler) as httpd:
        url = f"http://{args.host}:{args.port}/TinyFarmRPG-Web.html"
        header_note = "with COOP/COEP" if cross_origin_isolated else "without COOP/COEP"
        print(f"Serving {build_dir}")
        print(f"Open {url} ({header_note})")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
