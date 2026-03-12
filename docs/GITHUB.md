# GitHub Workflow

## Initial local setup
```bash
git init
git add .
git commit -m "Initial synthesizer scaffold"
```

## Create remote repo with GitHub CLI
```bash
./scripts/github-create.sh your-github-username Synthesizer
```

## Daily workflow
```bash
./scripts/new-devlog-entry.sh
./scripts/git-commit.sh "Describe the change"
git push -u origin main
```
