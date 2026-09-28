#!/usr/bin/env bash

set -euo pipefail

readonly FQBN="esp32:esp32:XIAO_ESP32S3:PSRAM=opi"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${TMPDIR:-/tmp}/InsectControllerDemo-build"

usage() {
  echo "Usage: $0 [serial-port]"
  echo "Example: $0 /dev/cu.usbmodem1101"
}

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "Error: arduino-cli is not installed or is not in PATH." >&2
  exit 1
fi

if [[ $# -gt 1 ]]; then
  usage >&2
  exit 2
fi

port="${1:-}"

if [[ -z "$port" ]]; then
  ports=()
  while IFS= read -r detected_port; do
    [[ -n "$detected_port" ]] && ports+=("$detected_port")
  done < <(arduino-cli board list | awk '$1 ~ /^\/dev\/cu\.usbmodem/ {print $1}')

  if [[ ${#ports[@]} -eq 0 ]]; then
    echo "Error: no XIAO USB serial port was found." >&2
    echo "Connect the board or enter download mode, then try again." >&2
    exit 3
  fi

  if [[ ${#ports[@]} -gt 1 ]]; then
    echo "Error: multiple USB serial ports were found:" >&2
    printf '  %s\n' "${ports[@]}" >&2
    echo "Run the script again with the desired port." >&2
    usage >&2
    exit 4
  fi

  port="${ports[0]}"
fi

if [[ ! -e "$port" ]]; then
  echo "Error: serial port does not exist: $port" >&2
  exit 5
fi

mkdir -p "$BUILD_DIR"

echo "Board: XIAO ESP32S3 ($FQBN)"
echo "Port:  $port"
echo "Compiling Controller firmware..."
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-path "$BUILD_DIR" \
  "$SCRIPT_DIR"

echo "Uploading Controller firmware..."
arduino-cli upload \
  --port "$port" \
  --fqbn "$FQBN" \
  --input-dir "$BUILD_DIR" \
  "$SCRIPT_DIR"

echo "Upload complete. The board has been reset and is running the Controller firmware."
