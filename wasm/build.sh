#!/usr/bin/env bash
# Builds the WebAssembly module web/src/wasm/mf_wasm.js: Conan (wasm host profile) + the `wasm` CMake preset.
# Needs the project-local emsdk from wasm/setup-emsdk.sh.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
emsdk_dir="${MF_EMSDK_DIR:-$repo_root/.emsdk}"

if [ ! -f "$emsdk_dir/emsdk_env.sh" ]; then
  echo "error: $emsdk_dir/emsdk_env.sh not found; run wasm/setup-emsdk.sh (or npm run wasm:setup in web/) first" >&2
  exit 1
fi

export MF_EMSDK_DIR="$emsdk_dir"
# shellcheck disable=SC1091
source "$emsdk_dir/emsdk_env.sh" >/dev/null
cd "$repo_root"

conan install . --profile:host=conan/profiles/wasm --profile:build=default --build=missing
cmake --preset wasm
cmake --build --preset wasm

output="$repo_root/web/src/wasm/mf_wasm.js"
if [ ! -f "$output" ]; then
  echo "error: $output was not produced" >&2
  exit 1
fi
echo "built $output"
