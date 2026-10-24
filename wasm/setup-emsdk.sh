#!/usr/bin/env bash
# Installs a project-local Emscripten SDK (version below) into .emsdk/ (or $MF_EMSDK_DIR).
# Needs git, python3 and network access once (several hundred MB). Nothing is installed system-wide and no shell
# profile is changed (no --permanent). Safe to re-run.
set -euo pipefail

EMSDK_VERSION=5.0.7
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
emsdk_dir="${MF_EMSDK_DIR:-$repo_root/.emsdk}"

if [ ! -d "$emsdk_dir" ]; then
  git clone https://github.com/emscripten-core/emsdk.git "$emsdk_dir"
fi
git -C "$emsdk_dir" fetch --tags
git -C "$emsdk_dir" checkout "$EMSDK_VERSION"
"$emsdk_dir/emsdk" install "$EMSDK_VERSION"
"$emsdk_dir/emsdk" activate "$EMSDK_VERSION"

echo "emsdk $EMSDK_VERSION ready in $emsdk_dir"
