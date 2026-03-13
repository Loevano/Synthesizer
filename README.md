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
- `src/audio/Voice.cpp`: one voice with a pool of oscillator slots
- `src/dsp/Oscillator.cpp`: waveform generator (`Sine`, `Square`, `Triangle`, `Saw`, `Noise`)
- `src/interfaces/AudioDriverCoreAudio.cpp`: CoreAudio driver backend
- `src/core/Logger.cpp`: console + file logging
- `src/platform/macos/AppMain.mm`: native macOS window with embedded `WKWebView`
- `src/ui_web/`: bundled web assets loaded by the macOS app shell

## Current code path
`main -> AudioDriver -> Synth -> Voice -> Oscillator -> output buffer`

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
  - `Settings`: audio/backend/device info, voice count, oscillator slots per voice, routing preset, MIDI/OSC status
  - `Synth`: global frequency, gain, waveform
  - `LFO`: waveform, depth, phase spread, polarity flip, linked/unlinked outputs, clock/fixed-rate controls
  - `Voices`: per-voice active, frequency, gain, output routing matrix
  - `Oscillators`: per-voice oscillator enable, waveform, gain, frequency, relative-to-voice-root toggle

## MIDI and OSC status
- CoreMIDI input is now started with the host on macOS
- current MIDI behavior:
  - note-on updates the current synth pitch
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
- Generic platform and synth-feature roadmap:
  - [docs/ROADMAP.md](/Users/jens/Documents/Coding/Synthesizer/docs/ROADMAP.md)
- Current architecture summary:
  - [docs/ARCHITECTURE.md](/Users/jens/Documents/Coding/Synthesizer/docs/ARCHITECTURE.md)
- Long-form target feature notes:
  - [What should it do?.md](/Users/jens/Documents/Coding/Synthesizer/What%20should%20it%20do%3F.md)
