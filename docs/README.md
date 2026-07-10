# Documentation Index

This folder is the project handbook. Start here when you are onboarding a person or an agent.

## Read First

- [Project overview](PROJECT_OVERVIEW.md): what the app is and how the major parts fit together.
- [User manual](User%20Manual.md): how to run the app and use the current UI.
- [Contributing](CONTRIBUTING.md): setup, branch workflow, prompting guidance, and verification.
- [Git rules](GIT_RULES.md): strict collaboration rules for branches, protected paths, commits, and PRs.

## System Design

- [Architecture](ARCHITECTURE.md): current code structure, runtime layers, and protected boundaries.
- [Data flow](Data%20flow.md): bridge, snapshot/render state split, realtime command flow, and patch flow.
- [What should it do?](What%20should%20it%20do%3F.md): product/design direction and open questions.

## Planning And Release

- [Roadmap](ROADMAP.md): platform and feature direction.
- [Implementation plan](IMPLEMENTATION_PLAN.md): practical implementation order.
- [GitHub workflow](GITHUB.md): branch, PR, and release workflow.
- [Releasing](RELEASING.md): local packaging, tagged releases, signing, and notarization notes.
- [Changelog](CHANGELOG.md): user-visible and workflow changes by version.
- [Devlog](devlog/README.md): optional dated work notes.

## Documentation Rules

- Keep user-facing instructions in `README.md` and `User Manual.md`.
- Keep collaboration process in `CONTRIBUTING.md`, `GITHUB.md`, and `GIT_RULES.md`.
- Keep architecture contracts in `ARCHITECTURE.md` and `Data flow.md`.
- When a code change alters behavior, update the matching docs in the same PR.
- Do not let agents make broad wording passes across unrelated docs while also changing code.
