#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build/web-release"}"
OUTPUT_DIR="${OUTPUT_DIR:-"${BUILD_DIR}/web-release-manual"}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8787}"

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
TinyFarmRPG Web manual test helper

Usage:
  $0 build             Configure, build, package, and validate build/web-release.
  $0 serve             Serve existing build/web-release at http://${HOST}:${PORT}/TinyFarmRPG-Web.html.
  $0 open              Serve existing build/web-release and open the page in the default browser.
  $0 rebuild-serve     Build first, then serve for manual testing.

Environment overrides:
  BUILD_DIR=${BUILD_DIR}
  OUTPUT_DIR=${OUTPUT_DIR}
  HOST=${HOST}
  PORT=${PORT}
  JOBS=${JOBS}

This script is for manual testing only. It does not run Chromium gameplay smoke tests.
EOF
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

build_release() {
  python3 "${ROOT_DIR}/tools/web_release/web_release_runbook.py" manual \
    --configure \
    --check-only \
    --build-dir "${BUILD_DIR}" \
    --output-dir "${OUTPUT_DIR}" \
    --jobs "${JOBS}"
}

serve_release() {
  local open_flag="${1:-}"
  require_artifacts
  if [[ "${open_flag}" == "--open" ]]; then
    python3 "${ROOT_DIR}/tools/web_release/web_release_runbook.py" manual \
      --skip-build \
      --skip-gate \
      --build-dir "${BUILD_DIR}" \
      --output-dir "${OUTPUT_DIR}" \
      --host "${HOST}" \
      --port "${PORT}" \
      --open
  else
    exec python3 -u "${ROOT_DIR}/tools/web_release/serve_web_release.py" \
      --build-dir "${BUILD_DIR}" \
      --host "${HOST}" \
      --port "${PORT}"
  fi
}

main() {
  local command="${1:-help}"
  case "${command}" in
    build)
      build_release
      ;;
    serve)
      serve_release
      ;;
    open)
      serve_release --open
      ;;
    rebuild-serve)
      build_release
      serve_release
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
