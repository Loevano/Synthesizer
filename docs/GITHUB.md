# GitHub Workflow

This project should normally land changes through short-lived branches into `dev`, then promote `dev` into `main`.

For the strict branch and protected-path policy, read [GIT_RULES.md](GIT_RULES.md).

## Initial setup

If the repo is not connected to GitHub yet:

```bash
gh auth login
./scripts/github-create.sh your-github-username Synthesizer
```

This creates a private repo, adds `origin`, and pushes the current branch.

## Branch model

- `main`
  Stable branch. Keep this release-ready.
- `dev`
  Integration branch for tested ongoing work.
- `feature/<name>`
  Short-lived work branch created from `dev`.
- `fix/<name>`, `docs/<name>`, `ui/<name>`, `test/<name>`, `chore/<name>`, `infra/<name>`
  Short-lived focused work branches created from `dev`.
- `core/<name>`, `dsp/<name>`, `routing/<name>`, `architecture/<name>`
  Explicit protected-path branches created from `dev`.
- `hotfix/<name>`
  Urgent fix branch created from `main`.

Protected branches must not receive direct local commits or direct pushes. Use PRs.

## Standard branch flow

Create a branch for a focused change from `dev`:

```bash
git checkout dev
git pull --ff-only
git checkout -b fix/robin-crash-logs
```

Do the work, then verify:

```bash
./scripts/build.sh
node --check src/ui_web/app.js
```

Commit:

```bash
git add path/to/file
git add another/file
git commit -m "Add crash diagnostics and bridge breadcrumbs"
```

Do not use `git add .` for normal work. The repo hooks and helper script are designed around intentional staging.

Push:

```bash
git push -u origin fix/robin-crash-logs
```

Open the PR:

```bash
gh pr create --base dev --fill
```

Merge the feature branch into `dev` once checks pass.

When `dev` is stable, open a second PR:

```bash
gh pr create --base main --head dev --fill
```

That PR is the promotion step from tested integration work into stable `main`.

## Release flow

This repo supports public releases from `main` and source/integration work on `dev`.

Local packaging:

```bash
./scripts/package-app.sh
```

GitHub Actions release workflow:
- file: [.github/workflows/release.yml](../.github/workflows/release.yml)
- manual trigger: `workflow_dispatch`
- public release trigger: push a tag like `v1.0.0` on `main`

Branch intent:
- `dev`: source and integration work, validated by CI
- `main`: stable branch, eligible for signed/notarized tagged releases

Tagged main release behavior:
- verifies the tagged commit is on `origin/main`
- builds the macOS app bundle
- packages it into a `.zip`
- uploads the zip as a GitHub Release asset

Important note:
- this publishes an unsigned packaged app
- local or downloaded builds may hit Gatekeeper quarantine warnings
- see [RELEASING.md](RELEASING.md) for the workaround and future signing options

## PR expectations

A good PR should say:
- what changed
- why it changed
- whether protected core paths changed
- how it was verified
- any known risk or remaining gap

Keep PRs focused. Avoid mixing unrelated UI polish, DSP fixes, routing changes, and docs cleanup unless they are directly tied to one issue.

Install the local hooks before contributing:

```bash
./scripts/install-git-hooks.sh
```

## If You Are Using A Coding Agent

Recommended workflow:
1. make a branch first
2. give the agent one focused scope
3. verify the result
4. ask for commit and push
5. open a PR into `dev`

Tell the agent which protected paths are off limits. If a protected-path change becomes necessary, stop and move to an explicit `core/`, `dsp/`, `routing/`, `architecture/`, `infra/`, or `hotfix/` branch.

If the change is UI-heavy, include screenshots.
If the change is crash-related, run:

```bash
./scripts/run-app.sh --debug-crash
```

and attach the newest `~/Library/Logs/Synthesizer/synth_*.log` and `~/Library/Logs/Synthesizer/crash_*.log`.

## Optional devlog

If you want to keep a working note for a slice of work:

```bash
./scripts/new-devlog-entry.sh
```

## Related docs

- Contributor setup and prompting guide: [CONTRIBUTING.md](CONTRIBUTING.md)
- Architecture: [ARCHITECTURE.md](ARCHITECTURE.md)
- User manual: [User Manual.md](User%20Manual.md)

## Recommended branch protection

For `main`:
- require pull requests before merge
- require passing CI checks
- require branch to be up to date before merge
- block force pushes
- block deletion

For `dev`:
- require pull requests before merge
- require passing CI checks
- block force pushes
- block deletion
