#!/usr/bin/env bash
# Resolve the Python dependency closure of python/pyproject.toml into a pinned requirements file
# that OSV-Scanner can scan (OSV does not resolve version ranges for PyPI).
#
# Usage: bash .github/scripts/resolve-python-deps.sh <output-requirements.txt> [<pyproject.toml>]
#   <pyproject.toml> defaults to <repo>/python/pyproject.toml (repo root = two directories above the script)
# Requires: bash, python3 >= 3.11 (tomllib), uv on PATH
# Exit codes: 0 resolved | 1 resolution failed or empty result | 2 usage, missing tool or unsupported requires-python
#
# The resolution is universal and starts at the requires-python floor, so every supported Python
# version is covered (resolving only for one interpreter would hide exposure on the older ones).
set -euo pipefail

if [ "$#" -ne 1 ] && [ "$#" -ne 2 ]; then
  echo "Usage: bash .github/scripts/resolve-python-deps.sh <output-requirements.txt> [<pyproject.toml>]" >&2
  exit 2
fi

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/../.." && pwd)
out=$1
pyproject=${2:-${repo_root}/python/pyproject.toml}

if [ ! -f "${pyproject}" ]; then
  echo "pyproject file not found: ${pyproject}" >&2
  exit 2
fi

if ! command -v uv >/dev/null 2>&1; then
  echo 'uv not found on PATH (python -m pip install "uv>=0.11,<1")' >&2
  exit 2
fi
if ! python3 -c 'import tomllib' >/dev/null 2>&1; then
  echo 'python3 >= 3.11 (tomllib) required' >&2
  exit 2
fi

tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT

# Paths are passed as argv, never interpolated into the program.
floor=$(python3 -I - "${pyproject}" "${tmp}/build-requires.txt" <<'PY'
import re
import sys
import tomllib

pyproject, build_requires = sys.argv[1], sys.argv[2]
with open(pyproject, "rb") as handle:
    data = tomllib.load(handle)

requires = data.get("build-system", {}).get("requires", [])
with open(build_requires, "w", encoding="utf-8") as handle:
    for entry in requires:
        handle.write(entry + "\n")

value = data.get("project", {}).get("requires-python")
match = re.fullmatch(r">=\s*(3\.[0-9]+)", value.strip()) if isinstance(value, str) else None
if match is None:
    print(
        f"unsupported requires-python '{value}' in {pyproject}: expected '>=3.N'; "
        "update .github/scripts/resolve-python-deps.sh",
        file=sys.stderr,
    )
    sys.exit(2)
print(match.group(1))
PY
) || exit $?

mkdir -p "$(dirname "${out}")"

# Keep uv's "# via" annotations: the log shows which package pulls each pin.
uv pip compile "${pyproject}" "${tmp}/build-requires.txt" \
  --all-extras --universal --python-version "${floor}" --no-header --quiet \
  --output-file "${out}" || {
  echo 'uv pip compile failed' >&2
  exit 1
}

n=$(grep -cE '^[A-Za-z0-9][A-Za-z0-9._-]*(\[[^]]*\])?==' "${out}" || true)
if [ "${n}" -eq 0 ]; then
  echo "no pinned requirements resolved into ${out}" >&2
  exit 1
fi

echo "resolved ${n} pinned requirements for Python >= ${floor} (runtime, all extras, build-system requires)"
