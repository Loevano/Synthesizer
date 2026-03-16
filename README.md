# Synthesizer

Multichannel synth host and macOS app shell for building speaker-aware instruments.

The project is centered around a native CoreAudio host, a reusable C++ DSP/controller layer, and a bundled web UI inside a `WKWebView`. The current reference source is `Robin`, a multivoice spatial synth with a master-first control model and local per-voice overrides.

## Current live signal path

`Audio Engine -> LiveGraph sources -> dry/fx split -> FX Rack -> dry + fx sum -> Output Mixer -> hardware outputs`

## Current UI model

`Program -> MIDI -> Source Mixer -> Output Mixer -> Robin -> Test -> Decor -> Pieces -> FX -> LFO`

## Current live features

- `Robin` is the main playable source:
  - configurable voice count and oscillator slots
  - all configured voices enabled by default
  - `Enabled` and `Linked to Master` voice states
  - one master template plus one expanded local voice editor at a time
  - per-voice oscillator bank, `VCF`, `ENV VCF`, `AMP`
  - routing presets including `All Outputs`
- `Test` is a simplified debug source for routing, MIDI, and output checks.
- `Source Mixer` controls source enable, source level, and whether each source feeds the dry path or the FX path.
- `Output Mixer` controls per-output level, delay, and fixed-band EQ.
- `Chorus` is live as a per-output FX processor.
- `Saturator` and `Sidechain` are scaffolded in the controller and UI but are not fully live yet.
- `CoreMIDI` input is live.
- `OSC` control is live on UDP port `9000`.
- Robin source-mixer level changes are smoothed in the DSP path to avoid pops.
- Robin voice allocation now tries to preserve release tails before reusing a voice.

## Robin in one sentence

Robin is a multivoice spatial synth: dial in one strong master timbre, keep most voices linked, then unlink only the voices you want to turn into local accents, spatial deviations, or rhythmic contrast.

## Build requirements

The current app target is macOS-first.

Required:

- macOS
- Xcode Command Line Tools
- CMake 3.21+

Optional:

- Node.js, for quick `app.js` syntax checks
- GitHub CLI (`gh`), if you want to create/push repos with the helper script

## Build

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
```

## Clean generated files

```bash
./scripts/clean.sh
```

## Run the CLI host

```bash
./scripts/run.sh
```

## Run the macOS app

```bash
./scripts/run-app.sh
```

## Help / optional CLI args

```bash
./scripts/run.sh --help
./scripts/run.sh --frequency 110 --gain 0.2 --waveform square --sample-rate 48000 --buffer 256 --channels 2 --voices 16 --oscillators-per-voice 6
```

Supported waveforms:

- `sine`
- `square`
- `triangle`
- `saw`
- `noise`

## Debug logging

Useful while tracing Robin and bridge issues:

```bash
SYNTH_DEBUG_ROBIN=1 SYNTH_DEBUG_BRIDGE=1 ./scripts/run-app.sh
```

For crash-oriented tracing, use:

```bash
./scripts/run-app.sh --debug-crash
```

This enables:
- bridge timing logs
- Robin parameter logs
- controller and bridge breadcrumbs for recent param changes
- fatal signal, terminate, and uncaught Objective-C exception logging

On macOS app runs, logs are written under `~/Library/Logs/Synthesizer/` by default.
You can override that with `SYNTH_LOG_DIR`.

Log files:
- `synth_*.log` for normal runtime logs
- `crash_*.log` for crash diagnostics and breadcrumbs

## Project layout

- `src/app/`: controller, state model, native bridge surface
- `src/audio/`: `Synth`, `Voice`, and `TestSynth`
- `src/dsp/`: oscillators, envelopes, filter, delay, chorus, LFO, EQ
- `src/graph/`: `LiveGraph`, source nodes, FX rack, output mixer node
- `src/interfaces/`: platform audio backends
- `src/platform/macos/`: app shell with `WKWebView`
- `src/ui_web/`: bundled web UI
- `docs/`: roadmap, architecture, overview, GitHub workflow, devlog

## Documentation

- Overview: [docs/PROJECT_OVERVIEW.md](docs/PROJECT_OVERVIEW.md)
- Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- Roadmap: [docs/ROADMAP.md](docs/ROADMAP.md)
- Implementation plan: [docs/IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md)
- Feature notes: [What should it do?.md](What%20should%20it%20do%3F.md)
- GitHub workflow: [docs/GITHUB.md](docs/GITHUB.md)

## Contributing

Contributor setup and workflow notes are in [CONTRIBUTING.md](CONTRIBUTING.md).
