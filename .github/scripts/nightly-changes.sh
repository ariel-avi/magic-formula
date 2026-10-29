#!/usr/bin/env bash
# Decide whether the nightly sanitizer analyses must run.
#
# Usage: nightly-changes.sh <event-name> [<baseline-sha>]
#   <event-name>    $GITHUB_EVENT_NAME; workflow_dispatch always runs
#   <baseline-sha>  head_sha of the last successful run of this workflow on this branch; empty = none
# stdout : markdown decision record
# $GITHUB_OUTPUT (when set): run=true|false
# exit   : 0 for every decision; 2 on a usage error; non-zero if git fails on HEAD
set -euo pipefail

# The single list of paths whose change triggers the analyses.
RELEVANT_PATHS=(src include tests cmake CMakeLists.txt bench/CMakeLists.txt python/CMakeLists.txt wasm/CMakeLists.txt CMakePresets.json conanfile.py .github/workflows/nightly-sanitizers.yml .github/scripts/nightly-changes.sh .github/scripts/nightly-findings.sh)

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: nightly-changes.sh <event-name> [<baseline-sha>]" >&2
    exit 2
fi

event="$1"
baseline="${2:-}"
head=$(git rev-parse HEAD)

run=true
reason=""
count=""
files=""
not_computed=""

if [ "$event" = "workflow_dispatch" ]; then
    reason="manual run: changed-files check bypassed"
    not_computed="$reason"
elif [ -z "$baseline" ]; then
    reason="no previous successful run: changed-files check skipped"
    not_computed="$reason"
elif ! [[ "$baseline" =~ ^[0-9a-f]{7,64}$ ]] ||
    ! git rev-parse --verify --quiet "${baseline}^{commit}" >/dev/null; then
    reason="baseline not in the history: running all analyses"
    not_computed="$reason"
else
    files=$(git diff --name-only "$baseline" HEAD -- "${RELEVANT_PATHS[@]}")
    if [ -n "$files" ]; then
        count=$(printf '%s\n' "$files" | wc -l | tr -d ' ')
        reason="${count} relevant file(s) changed since ${baseline}"
    else
        count=0
        run=false
        reason="no relevant change since ${baseline}"
    fi
fi

if [ "$run" = true ]; then decision="run"; else decision="skip"; fi
if [ -n "$baseline" ]; then baseline_cell="\`${baseline}\`"; else baseline_cell="none"; fi

echo "## Nightly sanitizers: changed-files decision"
echo
echo "| Item | Value |"
echo "|------|-------|"
echo "| Event | \`${event}\` |"
echo "| Baseline | ${baseline_cell} |"
echo "| Head | \`${head}\` |"
echo "| Decision | ${decision} |"
echo "| Reason | ${reason} |"
echo
if [ -n "$not_computed" ]; then
    echo "### Relevant changed files"
    echo
    echo "Not computed: ${not_computed}."
else
    echo "### Relevant changed files (${count})"
    echo
    if [ -n "$files" ]; then
        printf '%s\n' "$files" | while IFS= read -r f; do
            echo "- \`${f}\`"
        done
    else
        echo "None."
    fi
fi

if [ -n "${GITHUB_OUTPUT:-}" ]; then
    echo "run=${run}" >>"$GITHUB_OUTPUT"
fi
