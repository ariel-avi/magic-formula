#!/usr/bin/env bash
# Findings gate for one nightly analysis: proves the analysis was active and fails on any report.
#
# Usage: nightly-findings.sh [--clean] <preset>   preset in {asan, ubsan, tsan, valgrind}; build dir = build/<preset>
#   --clean  only remove build/<preset>/sanitizer.* and build/<preset>/valgrind.*.log (stale reports of an
#            earlier run), then exit 0; run it before ctest so the gate counts only this run's reports
# exit : 0 active and clean (or cleaned); 1 inactive or findings; 2 usage error
# Reports are one file per process (sanitizer log_path=...sanitizer with log_exe_name=1 appends .<exe>.<pid>;
# valgrind --log-file=...valgrind.%p.log), so they stay unique under parallel ctest.
set -uo pipefail

usage() {
    echo "Usage: nightly-findings.sh [--clean] <asan|ubsan|tsan|valgrind>" >&2
    exit 2
}

clean=0
if [ "${1:-}" = "--clean" ]; then
    clean=1
    shift
fi
[ "$#" -eq 1 ] || usage
preset="$1"
case "$preset" in
    asan) flag="-fsanitize=address" ;;
    ubsan) flag="-fsanitize=undefined" ;;
    tsan) flag="-fsanitize=thread" ;;
    valgrind) flag="" ;;
    *) usage ;;
esac

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build="$root/build/$preset"

if [ "$clean" -eq 1 ]; then
    rm -f "$build"/sanitizer.* "$build"/valgrind.*.log
    echo "removed stale reports in $build"
    exit 0
fi

group_start() {
    if [ "${GITHUB_ACTIONS:-}" = "true" ]; then echo "::group::$1"; else echo "--- $1 ---"; fi
}
group_end() {
    if [ "${GITHUB_ACTIONS:-}" = "true" ]; then echo "::endgroup::"; fi
}
report_error() {
    if [ "${GITHUB_ACTIONS:-}" = "true" ]; then echo "::error::$1"; else echo "error: $1"; fi
}

reasons=""
findings=0
detail=""

# Step 1: active check (positive control).
if [ "$preset" = "valgrind" ]; then
    if ! command -v jq >/dev/null 2>&1; then
        report_error "jq is required for the valgrind active check"
        reasons="inactive"
    elif ctest --test-dir "$build" --show-only=json-v1 2>/dev/null |
        jq -e '(.tests|length) > 0 and all(.tests[]; .command[0] | endswith("valgrind"))' >/dev/null; then
        echo "active check: $(ctest --test-dir "$build" --show-only=json-v1 2>/dev/null | jq '.tests|length') CTest commands, all launched by valgrind"
    else
        report_error "not every CTest command is launched by valgrind (build dir: $build)"
        reasons="inactive"
    fi
else
    if [ ! -f "$build/compile_commands.json" ]; then
        report_error "$build/compile_commands.json is missing: $flag cannot be verified"
        reasons="inactive"
    elif ! grep -F -q -- "$flag" "$build/compile_commands.json"; then
        report_error "$build/compile_commands.json does not contain $flag"
        reasons="inactive"
    else
        echo "active check: $flag is in the compile commands"
    fi
fi

# Step 2: findings.
shopt -s nullglob
if [ "$preset" = "valgrind" ]; then
    logs=("$build"/valgrind.*.log)
    if [ "${#logs[@]}" -eq 0 ]; then
        report_error "no Valgrind log files in build/valgrind: valgrind did not run"
        if [ -n "$reasons" ]; then reasons="$reasons, "; fi
        reasons="${reasons}no valgrind logs"
        findings=$((findings + 1))
    else
        clean=0
        for log in "${logs[@]}"; do
            if grep -F -q "ERROR SUMMARY: 0 errors from 0 contexts" "$log"; then
                clean=$((clean + 1))
            else
                findings=$((findings + 1))
                group_start "$log"
                cat "$log"
                group_end
                report_error "Valgrind reported errors: $log"
            fi
        done
        detail="${clean} Valgrind logs clean"
        echo "$clean of ${#logs[@]} Valgrind logs clean"
    fi
else
    reports=("$build"/sanitizer.*)
    for rep in ${reports[@]+"${reports[@]}"}; do
        findings=$((findings + 1))
        group_start "$rep"
        cat "$rep"
        group_end
        report_error "sanitizer report: $rep"
    done
    detail="no sanitizer reports"
fi

if [ "$findings" -gt 0 ]; then
    case "$reasons" in
        *"no valgrind logs"*) ;;
        *)
            if [ -n "$reasons" ]; then reasons="$reasons, "; fi
            reasons="${reasons}findings"
            ;;
    esac
fi

if [ -z "$reasons" ] && [ "$findings" -eq 0 ]; then
    verdict="nightly-findings $preset: PASS (active, $detail)"
    status=0
else
    verdict="nightly-findings $preset: FAIL (${reasons}; ${findings} finding file(s))"
    status=1
fi

echo "$verdict"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    echo "- **$preset**: $verdict" >>"$GITHUB_STEP_SUMMARY"
fi
exit "$status"
