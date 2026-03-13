# Changelog

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
