# GitHub Workflow

## Initial local setup

```bash
git init
git add .
git commit -m "Initial synthesizer scaffold"
```

## Create a remote repo with GitHub CLI

```bash
gh auth login
./scripts/github-create.sh your-github-username Synthesizer
```

This script creates a private repo, adds `origin`, and pushes the current branch.

## Daily workflow

Optional devlog:

```bash
./scripts/new-devlog-entry.sh
```

Commit:

```bash
git add .
git commit -m "Describe the change"
```

Or use the helper:

```bash
./scripts/git-commit.sh "Describe the change"
```

Push:

```bash
git push origin main
```

If you are working on a feature branch:

```bash
git push -u origin your-branch-name
```

## Before pushing

Recommended minimum verification:

```bash
./scripts/build.sh
```

If `src/ui_web/app.js` changed:

```bash
node --check src/ui_web/app.js
```

## Related docs

- Contributor setup: [CONTRIBUTING.md](/Users/jens/Documents/Coding/Synthesizer/CONTRIBUTING.md)
- Architecture: [docs/ARCHITECTURE.md](/Users/jens/Documents/Coding/Synthesizer/docs/ARCHITECTURE.md)
