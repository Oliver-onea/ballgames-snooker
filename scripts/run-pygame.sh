#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

export PYGAME_HIDE_SUPPORT_PROMPT=1

if [ ! -d .venv ]; then
  python3 -m venv .venv
fi

. .venv/bin/activate

python - <<'PY' >/dev/null 2>&1 || python -m pip install -r native/pygame/requirements.txt
import pygame
PY

python native/pygame/snooker_pygame.py "$@"
