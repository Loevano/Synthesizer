# Changelog

## 0.1.7 - 2026-03-16
- Added Robin `VCF`, `ENV VCF`, and low-pass filter support to the live synth path
- Reworked Robin around the current master/local voice model:
  - `Enabled`
  - `Linked to Master`
  - one selected local editor at a time
  - explicit `Reset to Master State`
- Added `All Outputs` as a Robin routing preset
- Changed source routing so each source chooses `dry` or `fx`, while outputs sum dry plus FX-chain signal
- Removed redundant FX link controls from the UI and backend model
- Moved ordinary live UI edits onto a temp/live bridge path instead of full state round-trips
- Hardened Robin and source-mixer audio behavior:
  - smoothed source-mixer level changes
  - improved Robin voice reuse so release tails are less likely to be cut off early
  - reduced retrigger clicks in the Robin voice path
- Replaced naive oscillator waveform generation with band-limited wavetable playback for lower aliasing and better steady-state scalability
- Added crash diagnostics with:
  - fatal signal and terminate logging
  - uncaught macOS exception logging
  - optional bridge/controller breadcrumbs via `./scripts/run-app.sh --debug-crash`
- Added contributor setup documentation

## 0.1.6 - 2026-03-15
- Added the multichannel scaffold app structure around `LiveGraph`, `Robin`, `Test`, `Output Mixer`, `FX`, and the embedded web UI
- Reframed `Robin` around a master-first control model in the UI and docs
- Enabled all configured Robin voices by default so polyphony works immediately
- Moved master oscillator edits onto a single native controller path instead of one UI bridge round-trip per voice

## 0.1.5 - 2026-03-13
- Added a `Voice` layer between `Synth` and `Oscillator`
- Added configurable synth capacity with defaults of `16` voices and `6` oscillators per voice
- Added `--voices` and `--oscillators-per-voice` init-time flags
- Made only the first voice and first oscillator slot active by default

## 0.1.4 - 2026-03-13
- Added `--help` output for the current CLI flags
- Added `--waveform` flag with `sine`, `square`, `triangle`, `saw`, and `noise`
- Made waveform selection flow from runtime config into `Synth`

## 0.1.3 - 2026-03-13
- Removed leftover module-host, MIDI, OSC, and unused audio-wrapper code from the repo
- Removed module example files and module swap script to match the simplified architecture
- Updated docs to reflect the lean `main -> AudioDriver -> Synth -> Oscillator` path

## 0.1.2 - 2026-03-13
- Renamed `SynthEngine` to `Synth` to make the instrument role clearer
- Added `Noise` as an oscillator waveform option
- Updated comments and docs to reflect `main -> AudioDriver -> Synth -> Oscillator`

## 0.1.1 - 2026-03-12
- Simplified default project to a beginner MVP
- Reduced build path to logger, oscillator, synth engine, and CoreAudio driver
- Rewrote `main.cpp` to a minimal steady-tone callback flow
- Simplified run command to `./scripts/run.sh` with optional args passthrough
- Updated README to a learning-first structure

## 0.1.0 - 2026-03-12
- Initial scaffold for C++ synthesizer learning project
- Added modular DSP host with runtime-loadable modules
- Added lowpass/highpass module examples
- Added CoreAudio driver interface and callback render path
- Added CoreMIDI input integration (macOS)
- Added OSC UDP server and basic OSC parser
- Added logging system and runtime log file output
- Added build/devlog/git/GitHub helper scripts
- Added CI workflow scaffold
