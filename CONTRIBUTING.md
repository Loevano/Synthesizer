# Contributing

This repo works best when contributors treat it like a collaboration between:
- a human making product and sound-design decisions
- a coding agent implementing changes quickly
- a normal Git workflow that keeps experiments isolated

This guide is written for that style of contribution.

## What this project is

This is a macOS-first multichannel synth app with:
- a native C++ audio engine
- a controller/state layer
- a bundled web UI inside a `WKWebView`

The current reference instrument is `Robin`, a multivoice spatial synth with a master-first workflow and local per-voice overrides.

## Setup

Required:
- macOS
- Xcode Command Line Tools (basically an IDE like VSCode or Cursor)
- CMake 3.21+
- Git

Useful:
- Node.js, for `node --check src/ui_web/app.js`
- GitHub CLI (`gh`), for branch/PR workflow

Install Xcode Command Line Tools if needed:

```bash
xcode-select --install
```

To get tje project on your hard drive and start collaborating:

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
```

Run the app:

```bash
./scripts/run-app.sh
```

Run the CLI host:

```bash
./scripts/run.sh
```

If the app is already open, relaunch it after UI changes so the rebuilt bundled assets reload.

## Vibe Coding Workflow

The most effective workflow here is:
1. make one focused request
2. implement it on a branch
3. verify it
4. commit and push
5. open a PR into `main`

Do not batch unrelated UI, DSP, routing, and infrastructure changes into one vague request if you can avoid it. The tighter the slice, the better the result.

## How To Prompt Well

Best prompts have:
- one screen or subsystem at a time
- the goal
- the scope
- the behavior rules
- what must stay
- what must be removed

For UI work, screenshots help a lot. The best pattern is:
- screenshot for visual direction
- 3-6 bullets for interaction rules

Good example:
- Goal: make Robin voice overview denser
- Scope: Robin page only
- Keep: master-first workflow
- Remove: duplicated controls
- Behavior: linked voices stay collapsed, one unlinked voice editor open at a time
- Reference: attached screenshot

For synth behavior or DSP work, say:
- what you changed
- what you expected
- what actually happened
- whether audio was playing
- whether MIDI or OSC was active
- exact control path if possible

If you want to edit specific DSP Behaviour, discuss that first.

Good example:
- Hold a note in Robin
- Change `FX -> Chorus -> Depth`
- Pop happens during drag, not on release
- Reproducible every time

## Crash And Debug Workflow

If the app crashes or behaves strangely, run:

```bash
./scripts/run-app.sh --debug-crash
```

This enables:
- bridge timing logs
- Robin parameter logs
- controller and bridge breadcrumbs
- fatal signal logging
- `std::terminate` logging
- uncaught macOS exception logging

On macOS app runs, logs are written under `~/Library/Logs/Synthesizer/` by default.
You can override that with `SYNTH_LOG_DIR`.

Log files:
- `synth_*.log` for normal runtime logs
- `crash_*.log` for crash diagnostics and recent breadcrumbs

If you report a crash, include:
- the exact repro steps
- whether audio was playing
- whether MIDI or OSC was active
- the newest `~/Library/Logs/Synthesizer/synth_*.log`
- the newest `~/Library/Logs/Synthesizer/crash_*.log`

## Branch Workflow

Do not do feature work directly on `main`.

Create a branch for each focused change:

```bash
git checkout -b fix/chorus-pops
```

Examples:
- `fix/robin-crash-logs`
- `feat/robin-lfo-layout`
- `docs/vibe-contributing-guide`

Keep the branch scoped to one area of work. That makes review and rollback much easier.

## Verification Before Commit

Minimum expectation:

```bash
./scripts/build.sh
```

If `src/ui_web/app.js` changed:

```bash
node --check src/ui_web/app.js
```

If you changed audio behavior, also do a live repro pass in the app when possible.

## Commit Workflow

Stage only the files that belong to the change:

```bash
git add path/to/file
git add another/file
git commit -m "Fix chorus parameter pops"
```

Or use the helper:

```bash
./scripts/git-commit.sh "Fix chorus parameter pops"
```

Commit messages should describe the actual change, not the session.

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
git push -u origin fix/chorus-pops
```

Open a PR into `main`.

A good PR description includes:
- what changed
- why it changed
- how to verify it
- any known gaps or risks

Suggested PR template:

```md
## Summary
- what changed

## Why
- user-visible or technical reason

## Verification
- ./scripts/build.sh
- node --check src/ui_web/app.js
- manual repro steps

## Risks
- anything still uncertain
```

## GitHub CLI

If you use GitHub CLI:

```bash
gh auth login
gh pr create --base main --fill
```

If you are creating a fresh remote:

```bash
./scripts/github-create.sh your-github-username Synthesizer
```

## What To Update When You Change Things

Update docs when behavior or workflow changes.

Usually:
- `README.md` for user-facing setup, running, or debugging changes
- `CONTRIBUTING.md` for contributor workflow changes
- `docs/GITHUB.md` for branch/PR workflow changes
- `CHANGELOG.md` for notable shipped behavior changes

If you change routing, DSP, or architecture meaningfully, also update:
- `docs/ARCHITECTURE.md`
- `docs/PROJECT_OVERVIEW.md`

## Project Structure

- `src/app/`: controller, state model, parameter handling, bridge-facing logic
- `src/audio/`: `Synth`, `Voice`, `TestSynth`
- `src/dsp/`: oscillator, envelope, filter, delay, chorus, LFO, EQ
- `src/graph/`: live graph, source nodes, FX rack, output mixer node
- `src/platform/macos/`: app shell and webview bridge
- `src/ui_web/`: bundled HTML/CSS/JS UI
- `docs/`: overview, architecture, roadmap, implementation notes

## Current Areas That Need Extra Care

- automated tests are still light
- the app target is macOS-first
- `Decor`, `Pieces`, `Saturator`, and `Sidechain` are still partly scaffolded
- Robin still needs deeper modulation beyond the current LFO and local voice overrides
