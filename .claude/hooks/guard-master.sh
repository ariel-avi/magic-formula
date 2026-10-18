#!/usr/bin/env bash
# PreToolUse hook (matcher: Bash). Enforces the repo git rules:
#   - no `git commit` / `git merge` while on master or main
#   - no `git push` that targets master or main (explicit refspec, or bare push while on master/main)
#   - no force pushes (--force, --force-with-lease, -f, +refspec) and no `git push --mirror` / `--all`
#     (both can update master/main as a side effect)
#   - no `gh pr merge` (agents never merge PRs; a human merges)
# `git` is recognised as a bare word or any path ending in `/git` (e.g. /usr/bin/git), also inside
# quoted strings such as `bash -c "git commit"`. A leading `cd <dir>` / `pushd <dir>` changes the
# directory used to resolve the current branch for the commands after it.
# Portable: bash + POSIX awk/sed/tr only (no GNU-only sed features).
# Reads the hook JSON on stdin. Exit 2 + stderr message blocks the call; exit 0 allows it.

set -u

input="$(cat)"

# --- extract tool_input.command and cwd (jq if present, else python3) ---------------------
command_str=""
cwd=""
if command -v jq >/dev/null 2>&1; then
  command_str="$(printf '%s' "$input" | jq -r '.tool_input.command // empty' 2>/dev/null)"
  cwd="$(printf '%s' "$input" | jq -r '.cwd // empty' 2>/dev/null)"
elif command -v python3 >/dev/null 2>&1; then
  command_str="$(printf '%s' "$input" | python3 -c 'import json,sys
try:
    d = json.load(sys.stdin)
    print(d.get("tool_input", {}).get("command", "") or "")
except Exception:
    pass' 2>/dev/null)"
  cwd="$(printf '%s' "$input" | python3 -c 'import json,sys
try:
    d = json.load(sys.stdin)
    print(d.get("cwd", "") or "")
except Exception:
    pass' 2>/dev/null)"
fi

# Nothing to inspect (not a Bash call, or no parser available): allow.
[ -n "$command_str" ] || exit 0
case "$command_str" in
  *git*|*gh*) ;;
  *) exit 0 ;;
esac

[ -n "$cwd" ] || cwd="${CLAUDE_PROJECT_DIR:-$PWD}"

block() {
  printf 'BLOCKED by guard-master.sh: %s\n' "$1" >&2
  printf 'Repo rule: never commit, merge or push on master/main, never force-push, never merge PRs as an agent. Use a branch + PR (see the pr-workflow skill).\n' >&2
  exit 2
}

is_protected() {
  case "$1" in
    master|main) return 0 ;;
    *) return 1 ;;
  esac
}

current_branch() {
  # $1 = directory. Works on unborn branches too. Empty output on detached HEAD / non-repo.
  git -C "$1" symbolic-ref --quiet --short HEAD 2>/dev/null || true
}

resolve_dir() {
  # $1 = base dir, $2 = path (absolute, relative or ~-prefixed).
  case "$2" in
    /*) printf '%s' "$2" ;;
    "~") printf '%s' "${HOME:-/}" ;;
    "~/"*) printf '%s/%s' "${HOME:-}" "${2#\~/}" ;;
    *) printf '%s/%s' "$1" "$2" ;;
  esac
}

is_git_word() {
  case "$1" in
    git|*/git) return 0 ;;
    *) return 1 ;;
  esac
}

is_gh_word() {
  case "$1" in
    gh|*/gh) return 0 ;;
    *) return 1 ;;
  esac
}

# Split the command line into simple commands on && || ; | & ( ) ` and newlines (POSIX awk).
segments="$(printf '%s\n' "$command_str" | awk '{ gsub(/&&|\|\||[;|&()`]/, "\n"); print }')"

curdir="$cwd"

while IFS= read -r segment; do
  [ -n "$segment" ] || continue
  # Drop quote characters so that quoted strings (bash -c "git commit", sh -c 'git push')
  # are scanned like plain words. Then word-split without globbing.
  segment="$(printf '%s' "$segment" | tr -d "\"'")"
  set -f
  # shellcheck disable=SC2206
  words=($segment)
  set +f
  n=${#words[@]}
  [ "$n" -gt 0 ] || continue

  # Track `cd <dir>` / `pushd <dir>` so later segments resolve the branch in the right repo.
  case "${words[0]}" in
    cd|pushd)
      if [ "$n" -ge 2 ] && [ "${words[1]}" != "-" ]; then
        curdir="$(resolve_dir "$curdir" "${words[1]}")"
      elif [ "$n" -eq 1 ]; then
        curdir="${HOME:-$curdir}"
      fi
      continue
      ;;
  esac

  # --- gh pr merge ---------------------------------------------------------------------------
  i=0
  while [ "$i" -lt "$n" ] && ! is_gh_word "${words[$i]}"; do i=$((i + 1)); done
  if [ "$i" -lt "$n" ]; then
    j=$((i + 1))
    while [ "$j" -lt "$n" ]; do
      case "${words[$j]}" in
        -R|--repo) j=$((j + 2)) ;;
        -*) j=$((j + 1)) ;;
        *) break ;;
      esac
    done
    if [ "$j" -lt "$n" ] && [ "${words[$j]}" = "pr" ]; then
      k=$((j + 1))
      if [ "$k" -lt "$n" ] && [ "${words[$k]}" = "merge" ]; then
        block "gh pr merge is not allowed; a human merges PRs."
      fi
    fi
  fi

  # --- git -------------------------------------------------------------------------------------
  i=0
  while [ "$i" -lt "$n" ] && ! is_git_word "${words[$i]}"; do i=$((i + 1)); done
  [ "$i" -lt "$n" ] || continue
  i=$((i + 1))

  # Skip git-level options, remembering -C <dir>.
  gitdir="$curdir"
  while [ "$i" -lt "$n" ]; do
    w="${words[$i]}"
    case "$w" in
      -C)
        i=$((i + 1))
        if [ "$i" -lt "$n" ]; then
          gitdir="$(resolve_dir "$gitdir" "${words[$i]}")"
        fi
        i=$((i + 1))
        ;;
      -c|--git-dir|--work-tree|--namespace) i=$((i + 2)) ;;
      -*) i=$((i + 1)) ;;
      *) break ;;
    esac
  done
  [ "$i" -lt "$n" ] || continue

  sub="${words[$i]}"
  i=$((i + 1))
  args=("${words[@]:$i}")

  case "$sub" in
    commit|merge)
      branch="$(current_branch "$gitdir")"
      if is_protected "$branch"; then
        block "git $sub on '$branch' is not allowed."
      fi
      ;;
    push)
      branch="$(current_branch "$gitdir")"
      positional=()
      skip_next=0
      for a in "${args[@]+"${args[@]}"}"; do
        if [ "$skip_next" -eq 1 ]; then skip_next=0; continue; fi
        case "$a" in
          --force|--force=*|--force-with-lease|--force-with-lease=*|--force-if-includes)
            block "force push is not allowed." ;;
          --mirror|--all|--branches)
            block "git push $a is not allowed (it can update master/main)." ;;
          -o|--push-option|--repo|--receive-pack|--exec) skip_next=1 ;;
          --*) ;;
          -*f*) block "force push is not allowed." ;;
          -*) ;;
          +*) block "force push (+refspec '$a') is not allowed." ;;
          *) positional+=("$a") ;;
        esac
      done

      if [ "${#positional[@]}" -le 1 ]; then
        # `git push` / `git push <remote>`: pushes the current branch (or its upstream).
        if is_protected "$branch"; then
          block "git push while on '$branch' is not allowed."
        fi
      else
        # positional[0] = remote, the rest are refspecs.
        for ref in "${positional[@]:1}"; do
          dst="${ref#*:}"           # text after the first ':' (or whole ref if none)
          dst="${dst#refs/heads/}"
          if [ "$dst" = "HEAD" ]; then dst="$branch"; fi
          if is_protected "$dst"; then
            block "git push targeting '$dst' is not allowed."
          fi
        done
      fi
      ;;
  esac
done <<EOF_SEGMENTS
$segments
EOF_SEGMENTS

exit 0
