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
- [ ] Security checks (CodeQL, dependency scan, dependency review) pass, or every finding and ignore is listed with its justification
