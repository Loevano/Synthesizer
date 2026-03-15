# Synthesizer (C++)

This is now a beginner-friendly MVP focused on learning:
- C++ classes and project structure
- Basic DSP (oscillator + gain)
- Audio callback flow
- A non-JUCE macOS app shell for the future web UI

## Current minimal architecture
- `src/main.cpp`: app lifecycle and audio start/stop
- `src/app/SynthController.cpp`: reusable host/controller layer shared by CLI and app shells
- `src/audio/Synth.cpp`: top-level synth instrument and voice pool
- `src/audio/TestSynth.cpp`: simplified single-oscillator debug source
- `src/audio/Voice.cpp`: one voice with a pool of oscillator slots
- `src/graph/`: concrete live graph owner and graph-stage modules
- `src/dsp/Oscillator.cpp`: waveform generator (`Sine`, `Square`, `Triangle`, `Saw`, `Noise`)
- `src/interfaces/AudioDriverCoreAudio.cpp`: CoreAudio driver backend
- `src/core/Logger.cpp`: console + file logging
- `src/platform/macos/AppMain.mm`: native macOS window with embedded `WKWebView`
- `src/ui_web/`: bundled web assets loaded by the macOS app shell

## Current code path
`main -> SynthController -> AudioDriver -> source graph -> output processor graph -> output buffer`

## Current interface model
`Audio Engine -> Source Mixer -> Output Mixer -> Sources -> FX -> outputs`

This interface model is intentionally ahead of the engine in two places:
- `Robin` is the main active source in the DSP path today
- `Test` is a simplified active debug source in the same path
- `Decor`, `Pieces`, and `FX` are scaffolded in controller state and the UI so the multichannel build order stays explicit

Current internal graph note:
- the controller now builds a `LiveGraph` from concrete graph modules instead of directly owning the live render path
- current live graph modules are `RobinSourceNode`, `TestSourceNode`, and `OutputMixerNode`
- the source-stage order remains `Robin -> Test -> Decor -> Pieces`
- output processing remains routed through an explicit output-processor stage order, currently just `Output Mixer`
- the raw bridge state now exposes this under `graph`

## Build
```bash
cd /Users/jens/Documents/Coding/Synthesizer
./scripts/build.sh
```

## Run
```bash
./scripts/run.sh
```

## Run app shell
```bash
./scripts/run-app.sh
```

## Help
```bash
./scripts/run.sh --help
```

## Optional run args
```bash
./scripts/run.sh --frequency 110 --gain 0.2 --waveform square --sample-rate 48000 --buffer 256 --channels 2 --voices 16 --oscillators-per-voice 6
```

Supported waveforms: `sine`, `square`, `triangle`, `saw`, `noise`

Default capacity:
- `16` voices
- `6` oscillator slots per voice

Default active sound:
- only voice `0` is active
- only oscillator slot `0` inside that voice is active

## Web UI status
- JUCE is not part of the active build path
- the macOS app embeds a bundled web UI inside `WKWebView`
- a minimal native bridge is now in place:
  - `getState()`
  - `setParam(path, value)`
- current web UI pages:
  - `Program`: engine/CoreAudio state, output device selection, output count selection, MIDI/OSC status, and raw scaffold state
  - `MIDI`: CoreMIDI device connection and per-device routing into the synths
  - `Source Mixer`: source-level VCA control for `Robin`, `Test`, `Decor`, and `Pieces`
  - `Output Mixer`: per-output trim, delay, and fixed-band EQ
  - `Robin`: playable voice source with master pitch, master `ENV VCA`, output routing, and one linked oscillator bank across all voices
  - `Test`: simplified single-oscillator debug source with manual output routing
  - `Decor`: scaffold for the future one-voice-per-output immersive source
  - `Pieces`: scaffold for the future granular algorithmic source
  - `FX`: linked-control per-output insert routing, with live `Chorus` plus scaffold `Saturator` and `Sidechain`
  - `LFO`: current Robin LFO controls

`Robin` note:
- the current control model is master-first: the amp envelope and oscillator bank are shared across all voices, while voice tuning, level, and output assignment remain local overrides
- section modulators and per-section unlink are planned next; they should layer on top of master values instead of replacing them

`Test` note:
- it is intentionally simple: one oscillator, one reusable ADSR-style envelope, manual output routing, optional continuous tone activation, and switchable MIDI response

Current scaffold note:
- output mixer `level` is active in the audio path now
- output mixer `delay` is active in the audio path now
- output mixer `EQ` is active in the audio path now
- `Robin` and `Test` do process audio now
- `Decor` and `Pieces` do not process audio yet
- `Chorus` is now live in the FX path
- `Saturator` and `Sidechain` remain scaffold-only

## MIDI and OSC status
- CoreMIDI input is now started with the host on macOS
- current MIDI behavior:
  - the MIDI page now exposes CoreMIDI sources and lets you enable them individually
  - each connected MIDI source can be routed independently to `Robin` and `Test`
  - note-on updates Robin pitch and drives the Test source note/envelope
  - if no voice is active, voice `0` is auto-enabled for preview
  - note-off disables that auto-enabled preview voice
- OSC UDP server is now started with the host on port `9000`
- current OSC messages:
  - `/noteOn` with `,i` or `,if`
  - `/noteOff` with `,i`
  - `/param/...` for controller parameter paths
    - example: `/param/frequency`
    - example: `/param/voice/0/oscillator/1/waveform`

## LFO status
- one global LFO is now implemented
- current LFO target:
  - summed oscillator output level per output channel
- current controls:
  - waveform: `sine`, `triangle`, `saw-down`, `saw-up`, `random`
  - depth
  - phase spread over outputs
  - polarity flip across outputs
  - unlinked outputs for per-output rate variation
  - clock-linked mode with tempo + rate multiplier
  - fixed-frequency mode in Hz
- current limitation:
  - the LFO is not yet part of a general modulation matrix, so it does not route to pitch or filters yet

## Roadmap
- Project overview:
  - [docs/PROJECT_OVERVIEW.md](/Users/jens/Documents/Coding/Synthesizer/docs/PROJECT_OVERVIEW.md)
- Generic platform and synth-feature roadmap:
  - [docs/ROADMAP.md](/Users/jens/Documents/Coding/Synthesizer/docs/ROADMAP.md)
- Implementation step plan:
  - [docs/IMPLEMENTATION_PLAN.md](/Users/jens/Documents/Coding/Synthesizer/docs/IMPLEMENTATION_PLAN.md)
- Current architecture summary:
  - [docs/ARCHITECTURE.md](/Users/jens/Documents/Coding/Synthesizer/docs/ARCHITECTURE.md)
- Long-form target feature notes:
  - [What should it do?.md](/Users/jens/Documents/Coding/Synthesizer/What%20should%20it%20do%3F.md)
