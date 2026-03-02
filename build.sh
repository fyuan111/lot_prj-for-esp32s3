#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"

# Ensure current directory is project root
if [[ "$PWD" != "$PROJECT_DIR" ]]; then
  cd "$PROJECT_DIR"
fi

# Ensure ESP-IDF environment is loaded
ensure_idf_env() {
  if command -v idf.py >/dev/null 2>&1 && [[ -n "${IDF_PATH:-}" ]] && [[ -d "${IDF_PATH}" ]]; then
    return 0
  fi

  local idf_export=""
  if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
    idf_export="${IDF_PATH}/export.sh"
  elif [[ -f "${PROJECT_DIR}/../esp-idf/export.sh" ]]; then
    idf_export="${PROJECT_DIR}/../esp-idf/export.sh"
  elif [[ -f "/home/fy/esp/esp-idf/export.sh" ]]; then
    idf_export="/home/fy/esp/esp-idf/export.sh"
  fi

  if [[ -z "$idf_export" ]]; then
    echo "[ERROR] esp-idf export.sh not found. Set IDF_PATH or install esp-idf at /home/fy/esp/esp-idf" >&2
    exit 1
  fi

  # shellcheck disable=SC1090
  source "$idf_export" >/dev/null
}

usage() {
  cat <<USAGE
Usage: ./build.sh [option] [port]

Options:
  (none)   Build only
  -f       Flash only
  -a       Build + flash + monitor

Port:
  Optional serial port, default: /dev/ttyACM0

Examples:
  ./build.sh
  ./build.sh -f
  ./build.sh -a /dev/ttyUSB0
USAGE
}

MODE="build"
PORT="/dev/ttyACM0"

if [[ $# -ge 1 ]]; then
  case "$1" in
    -f) MODE="flash" ;;
    -a) MODE="all" ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "[ERROR] Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
fi

if [[ $# -ge 2 ]]; then
  PORT="$2"
fi

ensure_idf_env

echo "[INFO] Project: $PROJECT_DIR"
echo "[INFO] Mode: $MODE"
[[ "$MODE" != "build" ]] && echo "[INFO] Port: $PORT"

case "$MODE" in
  build)
    idf.py -C "$PROJECT_DIR" build
    ;;
  flash)
    idf.py -C "$PROJECT_DIR" -p "$PORT" flash
    ;;
  all)
    idf.py -C "$PROJECT_DIR" -p "$PORT" build flash monitor
    ;;
esac
