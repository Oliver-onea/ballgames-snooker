#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

make build

MAMBA_ROOT_PREFIX="$PWD/.micromamba" .tools/bin/micromamba run -p "$PWD/.cpp-env" \
  "$PWD/build/sdl/snooker_sdl" "$@"
