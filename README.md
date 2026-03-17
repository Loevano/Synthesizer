# Synthesizer

`Synthesizer` is a multichannel synth host for building and testing speaker-aware instruments.

It combines:
- a native C++ audio engine
- a controller/state layer
- a bundled web UI inside a macOS app

## What Is It

This project is meant to be a playground and host for multichannel instrument design.

Right now it is focused on:
- routing sound to multiple outputs
- designing instruments that are aware of speaker layout
- testing live control, DSP, and UI ideas quickly

The current app is macOS-first. The main audio app runs as a native macOS bundle with a bundled web UI.

## How To Install And Run

### Requirements

- macOS
- Xcode Command Line Tools
- CMake 3.21+

Useful but optional:
- Node.js, for quick UI syntax checks
- GitHub CLI, if you want to use the GitHub helper scripts

Install Xcode Command Line Tools if needed:

```bash
xcode-select --install
```

Clone and build:

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
```

If you just want to try the app and a release is available, you do not need Xcode.
Download the packaged `.zip` from GitHub Releases, unzip it, and run `Synthesizer.app`.

Run the macOS app:

```bash
./scripts/run-app.sh
```

Run the CLI host:

```bash
./scripts/run.sh
```

Clean generated files:

```bash
./scripts/clean.sh
```

Create a packaged macOS app zip locally:

```bash
./scripts/package-app.sh
```

If the app is already open, relaunch it after UI changes so the rebuilt bundled assets are reloaded.

### Debug / Crash Logging

For crash-oriented tracing:

```bash
./scripts/run-app.sh --debug-crash
```

Useful while tracing Robin and bridge issues:

```bash
SYNTH_DEBUG_ROBIN=1 SYNTH_DEBUG_BRIDGE=1 ./scripts/run-app.sh
```

On macOS app runs, logs are written under:

```bash
~/Library/Logs/Synthesizer/
```

## What It Contains

### Instruments and Sources

- `Robin`
  The main playable instrument. A multivoice spatial synth with a master template and optional per-voice local overrides.
- `TestSynth`
  A simpler test source for checking routing, MIDI, output trims, and basic synthesis behavior.
- `Decor`
  Placeholder for a future source.
- `Pieces`
  Placeholder for a future source.

### Signal Path

Current live signal path:

`Audio Engine -> LiveGraph sources -> dry/fx split -> FX Rack -> dry + fx sum -> Output Mixer -> hardware outputs`

### Main UI Pages

- `Program`
- `MIDI`
- `Source Mixer`
- `Output Mixer`
- `Robin`
- `Test`
- `Decor`
- `Pieces`
- `FX`
- `LFO`

### Main Code Areas

- `src/app/`
  App orchestration, instrument wrappers, state model, bridge surface
- `src/audio/`
  DSP engines such as `Synth`, `Voice`, and `TestEngine`
- `src/dsp/`
  Oscillators, envelopes, filter, delay, chorus, LFO, EQ
- `src/graph/`
  Source nodes, FX rack, output mixer, live render graph
- `src/interfaces/`
  Platform audio backends
- `src/platform/macos/`
  macOS app shell with `WKWebView`
- `src/ui_web/`
  Bundled web UI

## What Is The State

The project is working, but still clearly in active development.

### Working Now

- `Robin` is playable and is the main reference instrument
- `Robin` supports configurable voices and oscillator slots
- all configured Robin voices start enabled by default
- Robin supports `Enabled` and `Linked to Master` voice states
- one expanded local voice editor can be open at a time
- Robin has per-voice oscillator bank, `VCF`, `ENV VCF`, and `AMP`
- source mixer supports dry vs FX path routing
- output mixer supports per-output level, delay, and EQ
- `Chorus` is live
- `CoreMIDI` input is live
- `OSC` control is live on UDP port `9000`
- crash diagnostics and regression tests are in place

### Still In Progress

- the app is macOS-first rather than fully cross-platform
- `Decor` and `Pieces` are still placeholders
- `Saturator` and `Sidechain` are scaffolded but not fully developed
- the architecture is still being cleaned up and split into clearer instrument boundaries
- Robin still has some older pitch-state concepts that may be simplified further

### In One Sentence

This is already usable as a multichannel synth sandbox, but it is still an evolving instrument host rather than a finished product.

## Documentation

- Overview: [docs/PROJECT_OVERVIEW.md](docs/PROJECT_OVERVIEW.md)
- User manual: [docs/User Manual.md](docs/User%20Manual.md)
- Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- Data flow: [docs/Data flow.md](docs/Data%20flow.md)
- Roadmap: [docs/ROADMAP.md](docs/ROADMAP.md)
- Implementation plan: [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md)
- Feature notes: [docs/What should it do?.md](docs/What%20should%20it%20do%3F.md)
- Changelog: [docs/CHANGELOG.md](docs/CHANGELOG.md)
- GitHub workflow: [docs/GITHUB.md](docs/GITHUB.md)
- Contributing: [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)

## Branch Workflow

This repo now uses:
- `main` for stable, shareable builds
- `dev` for ongoing tested integration work
- short-lived `feature/<name>` branches from `dev`

Normal flow:
1. create a feature branch from `dev`
2. open a PR into `dev`
3. merge `dev` into `main` when the batch is stable

## Releases

This repo now includes a release workflow for packaged macOS app builds.

- local packaging script: [scripts/package-app.sh](scripts/package-app.sh)
- GitHub Actions workflow: [.github/workflows/release.yml](.github/workflows/release.yml)

Release behavior:
- `workflow_dispatch` builds a packaged macOS artifact
- pushing a tag like `v0.2.0` builds the macOS app, zips it, and attaches it to a GitHub Release

Current release packaging is intended for testers first:
- the app can be downloaded and run without Xcode
- the app is not automatically signed or notarized by this workflow
