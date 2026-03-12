# Synthesizer (C++)

Personal learning project to practice:
- C++/OOP architecture
- DSP fundamentals
- Audio systems integration (driver callback + real-time path)
- MIDI + OSC control

## Project goals implemented
- Main host executable with audio driver callback path
- Swappable DSP modules built as dynamic libraries
- Logging infrastructure with file + console output
- MIDI input layer (CoreMIDI on macOS)
- OSC UDP server with basic OSC packet parser
- Git/GitHub workflow scaffolding

## Structure
- `src/main.cpp`: app lifecycle, argument parsing, runtime loop
- `src/audio`: synth rendering and engine orchestration
- `src/interfaces`: audio driver abstraction (`CoreAudio` implementation)
- `src/midi`: MIDI input handling
- `src/osc`: OSC UDP server + parser
- `src/dsp`: oscillator + dynamic DSP module host loader
- `modules/*`: swappable DSP modules (`lowpass`, `highpass`)
- `scripts`: helper scripts for build/log/commit/repo setup
- `docs`: architecture notes and dev log

## Build
```bash
cd /Users/jens/Documents/Coding/Synthesizer
./scripts/build.sh
```

## Run
```bash
./build/synth_host
```

Optional args:
```bash
./build/synth_host \
  --module build/modules/libhighpass_module.dylib \
  --sample-rate 48000 \
  --channels 2 \
  --buffer 256 \
  --osc-port 9000
```

## Dynamic module swapping
1. Start host with a module path (e.g. lowpass).
2. Rebuild a module: `cmake --build build --target highpass_module`.
3. Replace or point to different module file.
4. Host auto-reloads when the module file timestamp changes.
5. Shortcut script: `./scripts/swap-module.sh highpass_module`

Detailed workflow: `docs/MODULES.md`

## MIDI mappings
- Note on/off: plays oscillator
- CC7: master gain
- CC74: cutoff (maps 20Hz to 20kHz)
- CC71: resonance parameter passthrough

## OSC mappings
- `/noteOn` with `,if` (note, velocity)
- `/noteOff` with `,i` (note)
- `/cutoff` with `,f`
- `/gain` with `,f`
- `/resonance` with `,f`

## Notes
- Audio and MIDI runtime implementations are macOS-first in this scaffold.
- The architecture is intentionally simple and explicit for learning.
