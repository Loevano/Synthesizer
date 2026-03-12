# Changelog

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
