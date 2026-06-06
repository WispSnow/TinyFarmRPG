#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build/web-debug"}"
OUTPUT_DIR="${OUTPUT_DIR:-"${BUILD_DIR}/web-debug-manual"}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8788}"
DEBUG_UI_START="${DEBUG_UI_START:-all}"

detect_jobs() {
  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN 2>/dev/null && return
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null && return
  fi
  echo 8
}

JOBS="${JOBS:-"$(detect_jobs)"}"

usage() {
  cat <<EOF
TinyFarmRPG Web debug test helper

Usage:
  $0 build             Configure, build, and package build/web-debug with ImGui Debug UI enabled.
  $0 serve             Serve existing build/web-debug at http://${HOST}:${PORT}/TinyFarmRPG-Web.html.
  $0 open              Serve existing build/web-debug and open the page in the default browser.
  $0 rebuild-serve     Build first, then serve for manual debug testing.

Environment overrides:
  BUILD_DIR=${BUILD_DIR}
  OUTPUT_DIR=${OUTPUT_DIR}
  HOST=${HOST}
  PORT=${PORT}
  JOBS=${JOBS}
  DEBUG_UI_START=${DEBUG_UI_START}  # none, engine, game, or all

Debug builds use RelWithDebInfo, ENABLE_DEBUG_UI=ON, and ENABLE_RMLUI_DEBUGGER=ON.
They intentionally skip the official Web release gate.
EOF
}

validate_debug_ui_start() {
  case "${DEBUG_UI_START}" in
    none|engine|game|all)
      ;;
    *)
      echo "Invalid DEBUG_UI_START='${DEBUG_UI_START}'. Expected none, engine, game, or all." >&2
      exit 2
      ;;
  esac
}

require_artifacts() {
  local missing=0
  for artifact in \
    TinyFarmRPG-Web.html \
    TinyFarmRPG-Web.js \
    TinyFarmRPG-Web.wasm \
    TinyFarmRPG-Web.data \
    web-packages/web-package-index.json; do
    if [[ ! -f "${BUILD_DIR}/${artifact}" ]]; then
      echo "Missing ${BUILD_DIR}/${artifact}" >&2
      missing=1
    fi
  done
  if [[ "${missing}" -ne 0 ]]; then
    echo "Run '$0 build' first." >&2
    exit 1
  fi
}

debug_url() {
  local display_host="${HOST}"
  if [[ "${display_host}" == "0.0.0.0" || "${display_host}" == "::" ]]; then
    display_host="127.0.0.1"
  fi

  local url="http://${display_host}:${PORT}/TinyFarmRPG-Web.html"
  if [[ "${DEBUG_UI_START}" != "none" ]]; then
    url="${url}?debug-ui=${DEBUG_UI_START}"
  fi
  echo "${url}"
}

open_url_later() {
  local url="$1"
  (
    sleep 1
    if command -v open >/dev/null 2>&1; then
      open "${url}" >/dev/null 2>&1
    elif command -v xdg-open >/dev/null 2>&1; then
      xdg-open "${url}" >/dev/null 2>&1
    else
      echo "No opener found. Open manually: ${url}" >&2
    fi
  ) &
}

build_debug() {
  python3 "${ROOT_DIR}/tools/web_release/web_release_runbook.py" debug \
    --configure \
    --check-only \
    --build-dir "${BUILD_DIR}" \
    --output-dir "${OUTPUT_DIR}" \
    --jobs "${JOBS}"
}

serve_debug() {
  local open_flag="${1:-}"
  require_artifacts
  local url
  url="$(debug_url)"
  echo "Debug URL: ${url}"
  if [[ "${open_flag}" == "--open" ]]; then
    open_url_later "${url}"
  fi

  exec python3 -u "${ROOT_DIR}/tools/web_release/serve_web_release.py" \
    --build-dir "${BUILD_DIR}" \
    --host "${HOST}" \
    --port "${PORT}"
}

main() {
  validate_debug_ui_start

  local command="${1:-help}"
  case "${command}" in
    build)
      build_debug
      ;;
    serve)
      serve_debug
      ;;
    open)
      serve_debug --open
      ;;
    rebuild-serve)
      build_debug
      serve_debug
      ;;
    help|--help|-h)
      usage
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
}

main "$@"
