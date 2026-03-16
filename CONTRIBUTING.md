# Contributing

This project is currently a macOS-first multichannel synth app with a native C++ audio engine and a bundled web UI.

## Prerequisites

Required:

- macOS
- Xcode Command Line Tools
- CMake 3.21+
- Git

Optional but useful:

- Node.js, for `node --check src/ui_web/app.js`
- GitHub CLI (`gh`), for remote/repo setup

Install Xcode Command Line Tools if needed:

```bash
xcode-select --install
```

## Clone and build

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
```

## Run

CLI host:

```bash
./scripts/run.sh
```

macOS app:

```bash
./scripts/run-app.sh
```

If the app is already open, relaunch it after UI changes so the rebuilt bundled assets are loaded.

## Useful verification steps

Minimum expectation before committing:

```bash
./scripts/build.sh
```

If you changed `src/ui_web/app.js`, also run:

```bash
node --check src/ui_web/app.js
```

## Debugging

Robin and bridge logging:

```bash
SYNTH_DEBUG_ROBIN=1 SYNTH_DEBUG_BRIDGE=1 ./scripts/run-app.sh
```

Logs are written under `logs/`.

## Project structure

- `src/app/`
  - controller, state model, parameter handling, bridge-facing logic
- `src/audio/`
  - `Synth`, `Voice`, `TestSynth`
- `src/dsp/`
  - oscillator, envelope, filter, delay, chorus, LFO, EQ
- `src/graph/`
  - live graph, source nodes, FX rack, output mixer node
- `src/platform/macos/`
  - app shell and webview bridge
- `src/ui_web/`
  - bundled HTML/CSS/JS UI
- `docs/`
  - overview, architecture, roadmap, implementation notes

## Current contribution expectations

- Keep the project in one repo.
- Preserve the current controller state shape unless there is a strong reason to change it.
- Prefer simple, explicit DSP and graph code over abstraction for its own sake.
- If you change UI behavior, update the relevant docs.
- If you change routing, DSP, or bridge behavior, update the architecture docs and changelog.

## Current workflow

Create a devlog entry if you want to track a slice of work:

```bash
./scripts/new-devlog-entry.sh
```

Commit manually:

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
git push origin <branch>
```

## Remote setup

If you are creating a fresh GitHub repo and use GitHub CLI:

```bash
./scripts/github-create.sh your-github-username Synthesizer
```

## Areas that still need care

- automated tests are still light
- the app target is macOS-first
- `Decor`, `Pieces`, `Saturator`, and `Sidechain` are still partly scaffolded
- Robin still needs a deeper modulation system beyond the current LFO and local voice overrides
