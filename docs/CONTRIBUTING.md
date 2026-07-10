# Contributing

This repo is built for collaboration between a human decision-maker, coding agents, and normal Git review.

Read this first:

- [Git rules](GIT_RULES.md)
- [Architecture](ARCHITECTURE.md)
- [Data flow](Data%20flow.md)
- [Project overview](PROJECT_OVERVIEW.md)

## Project Shape

`Synthesizer` is a macOS-first multichannel synth app with:

- a native C++ audio engine
- a controller/state layer
- a bundled web UI inside a `WKWebView`

The current reference instrument is `Robin`, a multivoice spatial synth with a master-first workflow and local per-voice overrides.

## Setup

Required:

- macOS
- Xcode Command Line Tools
- CMake 3.21+
- Git

Useful:

- Node.js, for `node --check src/ui_web/app.js`
- GitHub CLI, for PR workflow

Install Xcode Command Line Tools if needed:

```bash
xcode-select --install
```

Clone, build, and run:

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
./scripts/run-app.sh
```

Install the local Git hooks once per clone:

```bash
./scripts/install-git-hooks.sh
```

If the app is already open, relaunch it after UI changes so the rebuilt bundled assets reload.

## Collaboration Model

Use this workflow:

1. start from current `dev`
2. create one short-lived branch
3. make one focused change
4. verify it
5. commit only intentional files
6. open a PR into `dev`

Do not batch unrelated UI, DSP, routing, docs, and infrastructure work into one branch.

## Branch Workflow

Do not do feature work directly on `main` or `dev`.

```bash
git checkout dev
git pull --ff-only
git checkout -b docs/collaboration-rules
```

Branch prefixes:

- `feature/` or `feat/`
- `fix/`
- `docs/`
- `ui/`
- `test/`
- `chore/`
- `infra/`
- `core/`, `dsp/`, `routing/`, or `architecture/` for agreed protected-path work
- `hotfix/`

See [GIT_RULES.md](GIT_RULES.md) for the strict version.

## Protected Core Work

Core audio, DSP, graph, host state, build, and CI paths are protected.

Do not change these paths casually:

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

If a task needs these paths, make that explicit before coding. Use a dedicated branch such as `dsp/fix-chorus-depth` or `routing/source-fx-regression`, add focused tests when behavior changes, and update architecture docs if a contract changes.

## Prompting Agents

Good prompts include:

- the exact goal
- the branch/scope
- what files or subsystems are off limits
- expected behavior
- what must stay unchanged
- verification requirements

Good example:

- Goal: make Robin voice overview denser
- Scope: Robin page only
- Branch: `ui/robin-voice-overview`
- Keep: master-first workflow
- Off limits: `src/audio/`, `src/dsp/`, `src/graph/`, `SynthHost`
- Verification: build plus manual UI check

For DSP or synth behavior work, include:

- what changed
- what you expected
- what actually happened
- whether audio was playing
- whether MIDI or OSC was active
- exact control path if possible

If the requested change touches protected core behavior, discuss it first.

## Verification Before Commit

Minimum:

```bash
./scripts/build.sh
```

If `src/ui_web/app.js` changed:

```bash
node --check src/ui_web/app.js
```

For audio, routing, MIDI, OSC, or patch behavior changes, also run the app and verify the actual workflow.

## Commit Workflow

Stage only the files that belong to the change:

```bash
git add docs/GIT_RULES.md scripts/check-git-rules.sh
git commit -m "Add strict collaboration git rules"
```

The helper also requires intentional staging:

```bash
./scripts/git-commit.sh "Add strict collaboration git rules" docs/GIT_RULES.md scripts/check-git-rules.sh
```

Commit messages should describe the change.

Good:

- `Add crash diagnostics and bridge breadcrumbs`
- `Remove oscillator-count gain normalization`
- `Rework Robin voice editor layout`

Bad:

- `misc fixes`
- `update stuff`
- `changes`

## Push And PR Workflow

Push your branch:

```bash
git push -u origin docs/collaboration-rules
```

Open a PR into `dev`.

A good PR includes:

- what changed
- why it changed
- whether protected paths changed
- how to verify it
- known gaps or risks

Only open `dev -> main` when promoting a tested integration batch.

## Crash And Debug Workflow

If the app crashes or behaves strangely:

```bash
./scripts/run-app.sh --debug-crash
```

Use `./scripts/run-app.sh` or `open /path/to/Synthesizer.app` for app launches. Do not run `Synthesizer.app/Contents/MacOS/Synthesizer` directly from the terminal.

On macOS app runs, logs are written under:

```bash
~/Library/Logs/Synthesizer/
```

Crash reports should include:

- exact repro steps
- whether audio was playing
- whether MIDI or OSC was active
- newest `synth_*.log`
- newest `crash_*.log`

## What To Update

Update docs when behavior or workflow changes.

- `README.md`: user-facing setup, running, and top-level status
- `docs/User Manual.md`: app usage
- `docs/CONTRIBUTING.md`: contributor workflow
- `docs/GIT_RULES.md`: branch, PR, and local hook rules
- `docs/GITHUB.md`: GitHub-specific workflow
- `docs/ARCHITECTURE.md`: architecture contracts
- `docs/Data flow.md`: control/audio/patch flow
- `docs/CHANGELOG.md`: notable shipped changes

## Project Structure

- `src/app/`: host, source state, parameter handling, bridge-facing logic
- `src/audio/`: render engines such as `PolySynth`, `Voice`, and `TestEngine`
- `src/dsp/`: oscillators, envelopes, filter, delay, chorus, LFO, EQ
- `src/graph/`: live graph, source nodes, FX rack, output mixer node
- `src/platform/macos/`: app shell and webview bridge
- `src/ui_web/`: bundled HTML/CSS/JS UI
- `docs/`: product, architecture, workflow, release, and planning docs

## Current Areas That Need Extra Care

- automated tests are still light
- the app target is macOS-first
- `Decor`, `Pieces`, `Saturator`, and `Sidechain` are still partly scaffolded
- Robin LFO and spread modulation are live, but broader modulation destinations still need careful design
