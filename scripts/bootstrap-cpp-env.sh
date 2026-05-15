#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)
    MAMBA_PLATFORM="linux-64"
    ;;
  Darwin-x86_64)
    MAMBA_PLATFORM="osx-64"
    ;;
  Darwin-arm64 | Darwin-aarch64)
    MAMBA_PLATFORM="osx-arm64"
    ;;
  *)
    printf 'Unsupported bootstrap platform: %s-%s\n' "$(uname -s)" "$(uname -m)" >&2
    exit 1
    ;;
esac

if [ ! -x .tools/bin/micromamba ]; then
  mkdir -p .tools
  ARCHIVE="${TMPDIR:-/tmp}/ballgames-micromamba-${MAMBA_PLATFORM}.tar.bz2"
  curl -L "https://micro.mamba.pm/api/micromamba/${MAMBA_PLATFORM}/latest" -o "$ARCHIVE"
  python3 - "$ARCHIVE" <<'PY'
import sys
import tarfile
from pathlib import Path

archive = sys.argv[1]
Path(".tools").mkdir(exist_ok=True)
with tarfile.open(archive, "r:bz2") as tf:
    tf.extract("bin/micromamba", ".tools")
Path(".tools/bin/micromamba").chmod(0o755)
PY
fi

if [ ! -d .cpp-env ]; then
  MAMBA_ROOT_PREFIX="$PWD/.micromamba" .tools/bin/micromamba create -y -p "$PWD/.cpp-env" -c conda-forge \
    cmake make pkg-config cxx-compiler sdl2 sdl2_ttf
fi
