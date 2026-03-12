# Synthesizer (C++)

This is now a beginner-friendly MVP focused on learning:
- C++ classes and project structure
- Basic DSP (oscillator + gain)
- Audio callback flow

## Current minimal architecture
- `src/main.cpp`: app lifecycle and audio start/stop
- `src/audio/SynthEngine.cpp`: generates audio samples
- `src/interfaces/AudioDriverCoreAudio.cpp`: CoreAudio driver backend
- `src/core/Logger.cpp`: console + file logging

## Build
```bash
cd /Users/jens/Documents/Coding/Synthesizer
./scripts/build.sh
```

## Run
```bash
./scripts/run.sh
```

## Optional run args
```bash
./scripts/run.sh --frequency 110 --gain 0.2 --sample-rate 48000 --buffer 256 --channels 2
```

## Learning roadmap (next)
1. Add ADSR envelope class
2. Add MIDI note input
3. Add one filter module
4. Add polyphony (multiple voices)

## Note
Advanced files from the earlier scaffold are still in the repo, but they are not part of the default build path now.
