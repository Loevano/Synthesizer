# Architecture

## Runtime graph
`MIDI + OSC -> SynthEngine params -> Oscillator -> DSP ModuleHost -> AudioDriver callback -> Output`

## Core layers
- `AudioEngine`: starts/stops audio backend and drives block rendering.
- `SynthEngine`: simple mono voice (oscillator + envelope + gain).
- `ModuleHost`: loads shared library modules and calls `processSample()`.
- `MidiInput`: receives hardware/controller MIDI messages.
- `OscServer`: listens on UDP and parses OSC messages.
- `Logger`: thread-safe console + file logs.

## DSP module contract
All modules export three C symbols:
- `createModule`
- `destroyModule`
- `getModuleName`

And implement `synth::dsp::IDspModule`:
- `prepare(sampleRate)`
- `processSample(input)`
- `setParameter(name, value)`

## Swapping module strategy
- Modules are separate dynamic libraries in `build/modules/`.
- Host tracks selected module file timestamp.
- If changed, host unloads old library and loads new one.
- This enables quick A/B filter iteration by rebuilding/replacing module binary.

## Next learning extensions
- Polyphony + voice allocation class
- ADSR class and modulation matrix
- Better realtime-safe lock-free parameter updates
- Anti-aliased oscillators
- More filters (SVF, ladder) and effects
