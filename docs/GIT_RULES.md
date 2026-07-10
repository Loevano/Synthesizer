# Git Rules

These rules are meant to keep collaboration predictable when multiple people and coding agents work in the repo.

The short version:

- branch from `dev`
- make one focused change
- avoid protected core paths unless the work was explicitly agreed
- stage files intentionally
- open PRs into `dev`
- promote `dev` into `main` only after testing

## Branches

Protected long-lived branches:

- `main`: stable, release-ready, tagged release source
- `dev`: tested integration branch

Never do normal work directly on `main` or `dev`.

Allowed short-lived branch prefixes:

- `feature/<name>` or `feat/<name>` for normal feature work
- `fix/<name>` for focused bug fixes
- `docs/<name>` for documentation-only changes
- `ui/<name>` for web UI-only work
- `test/<name>` for tests
- `chore/<name>` for small maintenance
- `infra/<name>` for scripts, CI, packaging, or repo workflow
- `core/<name>`, `dsp/<name>`, `routing/<name>`, or `architecture/<name>` for agreed protected-path work
- `hotfix/<name>` for urgent fixes from `main`

## Protected Core Paths

The following paths are protected because changes here can alter audio behavior, routing, state ownership, build behavior, or CI behavior:

- `src/audio/`
- `include/synth/audio/`
- `src/dsp/`
- `include/synth/dsp/`
- `src/graph/`
- `include/synth/graph/`
- `src/app/SynthHost.cpp`
- `include/synth/app/SynthHost.hpp`
- `include/synth/app/RealtimeCommands.hpp`
- `CMakeLists.txt`
- `.github/workflows/`

Default rule:

- do not change protected core paths in normal `feature/`, `ui/`, `docs/`, or `chore/` branches
- use a dedicated `core/`, `dsp/`, `routing/`, `architecture/`, `infra/`, or `hotfix/` branch when that work is intentional
- explain the protected-path change in the PR
- add or update regression tests when behavior changes
- update [ARCHITECTURE.md](ARCHITECTURE.md) or [Data flow.md](Data%20flow.md) when a contract changes

## Scope Rules

Each branch should own one coherent slice.

Good branch scopes:

- one UI page
- one bug fix
- one DSP behavior
- one routing behavior
- one documentation cleanup
- one build or release workflow change

Avoid mixing:

- UI redesign plus DSP changes
- routing changes plus docs-only cleanup
- formatting-only churn plus behavior changes
- broad file moves plus feature work
- many unrelated agent requests in one branch

## Staging And Commits

Stage explicit files only.

Use:

```bash
git add docs/GIT_RULES.md scripts/check-git-rules.sh
```

Avoid:

```bash
git add .
```

Commit messages should describe the actual change:

```bash
git commit -m "Add strict collaboration git rules"
```

Do not use vague messages such as:

- `misc`
- `changes`
- `update stuff`
- `wip`

## Local Hooks

Install the repo hooks once per clone:

```bash
./scripts/install-git-hooks.sh
```

The hooks enforce:

- no commits on `main` or `dev`
- approved branch-name prefixes
- no accidental protected-path edits from ordinary branches
- a maximum default staged file count
- a maximum default staged line churn
- no direct push to `main` or `dev`
- basic commit-message quality

Emergency override variables exist, but should be rare and mentioned in the PR:

- `SYNTH_ALLOW_CORE_CHANGE=1`
- `SYNTH_ALLOW_LARGE_CHANGE=1`
- `SYNTH_ALLOW_PROTECTED_PUSH=1`
- `SYNTH_ALLOW_WEAK_COMMIT_MSG=1`

## Pull Requests

Normal PR target:

- feature/fix/docs/ui/test/chore/infra branch -> `dev`

Release promotion target:

- `dev` -> `main`

Every PR should include:

- what changed
- why it changed
- whether protected core paths changed
- how it was verified
- known risk or follow-up

Before opening a PR, run at least:

```bash
./scripts/build.sh
```

If `src/ui_web/app.js` changed:

```bash
node --check src/ui_web/app.js
```

For behavior changes, run the app and verify the actual workflow manually.

## Agent Rules

When asking an agent to work in this repo:

- tell it the branch and exact scope
- tell it which files or subsystems are off limits
- ask it to read [GIT_RULES.md](GIT_RULES.md), [CONTRIBUTING.md](CONTRIBUTING.md), and the relevant architecture docs first
- keep the request small enough to review
- do not ask one agent to make broad code, docs, and formatting changes at the same time

If an agent discovers that a protected core path is required, it should stop and explain why before changing it.
