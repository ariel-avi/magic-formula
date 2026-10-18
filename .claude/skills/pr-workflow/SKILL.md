---
name: pr-workflow
description: Git and pull-request rules for magic-formula. Every change goes through a branch and a PR into master. Covers branch naming, Conventional Commits, PR body sections, CI-green requirement and forbidden git operations.
when_to_use: Creating a branch, committing, pushing, opening or updating a PR, or checking that delivered work landed through a PR.
allowed-tools: Bash(git status *), Bash(git branch *), Bash(git switch *), Bash(git pull --ff-only*), Bash(git log *), Bash(git diff *)
user-invocable: true
argument-hint: "[branch-name]"
---

# PR workflow

Only `general-developer` performs git/PR mechanics, and only when `orchestrator` explicitly asks, through a workflow step (the `feature` workflow's Branch step, or an ad-hoc PR workflow). Other agents never branch, commit, push or open PRs.

This skill deliberately does **not** pre-approve `git add`, `git commit`, `git push` or `gh pr` in its `allowed-tools`. `general-developer` runs them under the normal permission rules, so loading or invoking this skill never silently grants git writes (for example on the orchestrator's main thread).

## Hard rules

- **Never commit, merge or push on `master`** (or `main`). `.claude/hooks/guard-master.sh` blocks it, together with force pushes, `git push --mirror` / `--all` and `gh pr merge`. Do not try to bypass the hook.
- Every change reaches `master` through a branch + PR. Agents never merge PRs; a human merges.
- No force-push (`--force`, `-f`, `--force-with-lease`) and no `git reset --hard`, `git rebase` of pushed commits, or branch deletion on shared branches.
- One logical change per PR. Split unrelated changes into separate branches/PRs.
- CI must be green (build, all tests, coverage gate 100% line + branch on core, format/tidy) before the PR is reported as ready.
- No skipped or disabled tests, no unapproved coverage exclusions.

## Branches

Create from an up-to-date `master` (`git switch master && git pull --ff-only && git switch -c <branch>`). If the session has an assigned branch (e.g. `claude/<name>`), use that branch instead.

| Prefix | Use |
|--------|-----|
| `feat/<short-kebab>` | new functionality (`feat/pac2002-lateral-force`) |
| `fix/<short-kebab>` | bug fix (`fix/tir-parser-trailing-comment`) |
| `chore/<short-kebab>` | tooling, deps, refactors without behaviour change |
| `docs/<short-kebab>` | documentation and research notes only |
| `ci/<short-kebab>` | GitHub Actions / pipeline changes |
| `test/<short-kebab>` | test-only additions |

## Commits (Conventional Commits)

```
<type>(<scope>): <imperative summary, <= 72 chars>

<body: what and why, wrapped at 72>

<footer: BREAKING CHANGE: ..., Refs #123, required attribution lines>
```

- Types: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `build`, `ci`, `chore`.
- Scopes: `core`, `pac2002`, `mf52`, `io`, `app`, `bench`, `tests`, `cmake`, `conan`, `agents`, `skills`, `workflows`.
- Stage files explicitly (`git add <paths>`). Never `git add -A` blindly; check `git status` first and make sure no build outputs or secrets are included.
- Add any attribution lines the session requires at the end of the message.

## Opening the PR

```bash
git push -u origin <branch>
gh pr create --base master --head <branch> --title "<conventional title>" --body-file <body.md>
```

The body follows `.github/pull_request_template.md`, which is identical to the block below (keep the two in sync):

```markdown
## Summary
<1–3 sentences: what and why; link the user request / issue>

## Changes
- <file or component>: <change>

## Tests & coverage
- Tests added/changed: <names>
- ctest: <passed>/<total> (unit + module)
- Coverage (core): line <x>% / branch <y>% (target 100/100)
- Coverage exclusions: none | <file:line + reviewer-approved justification>

## Review verdict
<reviewer verdict: APPROVE | CHANGES_REQUIRED, open findings if any>

## Checklist
- [ ] Branch is not master; one logical change
- [ ] Conventional Commit messages
- [ ] All tests pass; none skipped or disabled
- [ ] 100% line + branch coverage on core sources
- [ ] clang-format / clang-tidy clean
- [ ] CLAUDE.md / skills updated if layout, commands or conventions changed
- [ ] docs/research updated if equations or conventions changed
```

## After opening

- Report the PR URL, branch and CI status to the orchestrator.
- To change a PR, push new commits to the same branch. Do not force-push.
- If CI fails, the failure goes back through the developer → test-runner loop. Fixes are new commits on the branch.
