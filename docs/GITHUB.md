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
- signs the app with `Developer ID`
- notarizes and staples the app
- packages it into a `.zip`
- uploads the zip as a GitHub Release asset

Required GitHub secrets for the release workflow:
- `APPLE_SIGNING_IDENTITY`
- `APPLE_DEVELOPER_ID_P12_BASE64`
- `APPLE_DEVELOPER_ID_P12_PASSWORD`
- `APPLE_API_KEY_ID`
- `APPLE_API_ISSUER_ID`
- `APPLE_API_PRIVATE_KEY`
- optional: `APPLE_KEYCHAIN_PASSWORD`

Important note:
- if the signing/notarization secrets are missing, tagged `main` releases should fail
- this is intentional so `main` does not silently publish unsigned public releases
- local or older unsigned tester builds may still hit Gatekeeper quarantine warnings
- see [RELEASING.md](RELEASING.md) for the full signing/notarization and fallback guidance

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
