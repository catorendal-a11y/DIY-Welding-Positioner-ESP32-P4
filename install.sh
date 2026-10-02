#!/usr/bin/env bash
# Set up this project's PlatformIO environment on Linux/macOS or Git Bash.
set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if command -v python3 >/dev/null 2>&1; then
  PROJECT_PYTHON="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
  PROJECT_PYTHON="$(command -v python)"
else
  printf 'Python is required. Install Python 3.11 and rerun this script.\n' >&2
  exit 1
fi

case "${1:-}" in
  --check)
    "$PROJECT_PYTHON" --version
    if command -v pio >/dev/null 2>&1; then
      pio --version
    elif [[ -x "$PROJECT_DIR/.venv/bin/pio" ]]; then
      "$PROJECT_DIR/.venv/bin/pio" --version
    elif [[ -f "$PROJECT_DIR/.venv/Scripts/platformio.exe" ]]; then
      "$PROJECT_DIR/.venv/Scripts/platformio.exe" --version
    else
      printf 'PlatformIO is not installed in PATH or the project environment.\n' >&2
      exit 1
    fi
    exit 0
    ;;
  '' ) ;;
  *) printf 'Usage: ./install.sh [--check]\n' >&2; exit 2 ;;
esac

"$PROJECT_PYTHON" -m venv "$PROJECT_DIR/.venv"
if [[ -x "$PROJECT_DIR/.venv/bin/python" ]]; then
  PROJECT_ENV_PYTHON="$PROJECT_DIR/.venv/bin/python"
else
  PROJECT_ENV_PYTHON="$PROJECT_DIR/.venv/Scripts/python.exe"
fi
"$PROJECT_ENV_PYTHON" -m pip install --upgrade pip platformio
cd -- "$PROJECT_DIR"
"$PROJECT_ENV_PYTHON" -m platformio pkg install -e esp32p4-release
printf '\nProject dependencies installed. Next commands:\n'
printf '  "%s" -m platformio test -e native\n' "$PROJECT_ENV_PYTHON"
printf '  "%s" -m platformio run -e esp32p4-release\n' "$PROJECT_ENV_PYTHON"
printf 'Read README.md before connecting or uploading to motor hardware.\n'
