# GitHub Workflow

This project should normally land changes through short-lived branches into `dev`, then promote `dev` into `main`.

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
- `hotfix/<name>`
  Urgent fix branch created from `main`.

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

## PR expectations

A good PR should say:
- what changed
- why it changed
- how it was verified
- any known risk or remaining gap

Keep PRs focused. Avoid mixing unrelated UI polish, DSP fixes, routing changes, and docs cleanup unless they are directly tied to one issue.

## If You Are Using A Coding Agent

Recommended workflow:
1. make a branch first
2. ask for one focused change
3. verify the result
4. ask for commit and push
5. open a PR into `dev`

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
