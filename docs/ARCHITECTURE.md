# Architecture

This document describes the current code structure after the naming cleanup.

## Live render path

`Audio driver -> SynthHost -> LiveGraph sources -> dry/fx split -> FxRackNode -> dry + fx sum -> OutputMixerNode -> hardware outputs`

Current live pieces:

- CoreAudio backend on macOS
- `Robin` and `TestSynth` as active sources
- source enable, source level, and dry/fx routing
- `Chorus` in the FX rack
- output mixer level, delay, and EQ

Current scaffold-only pieces:

- `Decor`
- `Pieces`
- `Saturator`
- `Sidechain`

## Main layers

### App host

`SynthHost` is the top-level orchestrator. It owns:

- runtime configuration
- bridge-facing state serialization
- MIDI and OSC lifecycle
- `LiveGraph` construction
- controller-side source state
- render-side source state

### App-level synths

The shared app-level base is `synth::app::Synth`.

It is intentionally small. It covers:

- prepare/process lifecycle
- note on/off
- realtime command building and application
- JSON state export

Current implementations:

- `Robin`
- `TestSynth`

`SourceState.hpp` exists so small controller-facing structs such as `EnvelopeState` and `TestSourceState` can be reused without dragging full synth classes into unrelated headers.

### Render engines

App-level synths do not render samples directly. They delegate to concrete audio engines:

- `audio::PolySynth` for Robin
- `audio::TestEngine` for TestSynth

That split keeps product-facing behavior such as routing presets and linked/local voice rules out of the raw DSP layer.

### DSP effects

The shared DSP effect base is `synth::dsp::Effects`.

Right now only `Chorus` derives from it, but it gives future output effects one consistent lifecycle:

- `prepare(sampleRate)`
- `reset()`
- `processSample(inputSample)`

## State ownership model

The important architectural split is:

- controller-side snapshot state
- render-side live state

`SynthHost` keeps both on purpose.

The controller-side snapshot exists so:

- the UI can read a coherent state JSON immediately
- patch save/load works from one predictable source of truth
- parameter edits do not need to round-trip full engine state for every drag

The render-side state exists so:

- the audio thread owns the data it reads while processing
- parameter changes can be applied at block boundaries instead of mutating live DSP structures from the UI thread

## Realtime parameter flow

When the UI changes a live parameter, the code follows this sequence:

1. `SynthHost::tryEnqueueRealtime*Param(...)` parses the path and builds a `RealtimeCommand`
2. `applyStateMirrorCommandLocked(...)` updates the controller-side snapshot immediately
3. `submitOrApplyRealtimeCommand(...)` decides whether to queue or apply
4. if audio is running, `queueRealtimeCommand(...)` hands the immutable command to the render side
5. at the next audio block, `drainQueuedRealtimeCommandsLocked(...)` applies every queued command through `applyRenderStateCommandLocked(...)`
6. `LiveGraph::render(...)` runs with that updated render copy

This is the core rule:

- snapshot state is updated under the main mutex
- render state is updated at audio block boundaries

That split is the current answer to the earlier parameter-thread churn. The naming now reflects that intent directly.

## Graph and routing model

Each source has two routing decisions:

- is it enabled?
- does it feed `Dry` or `FxBus`?

`LiveGraph` renders enabled sources into either the main output buffer or the FX bus. The FX bus is processed separately, then summed back into the main output before the output mixer stage.

Source-node gain smoothing still lives in the source nodes so live level drags do not click.

## Robin

`Robin` is the reference source and the most complex synth in the repo.

Current model:

- configurable voice count
- configurable oscillator count per voice
- linked vs local voice editing
- master `VCF`, `VCF ENV`, and `VCA ENV`
- routing presets across outputs
- note allocator that avoids immediate reuse of voices still in release

`Robin` owns the musical/product-facing rules. `audio::PolySynth` owns the actual per-voice DSP.

## TestSynth

`TestSynth` stays intentionally small:

- one oscillator
- one `VCA ENV`
- manual output routing
- optional continuous tone
- optional MIDI response

It is the simplest useful source for regression tests and routing checks.

## What still needs work

- `SynthHost` is still large and should shrink over time
- `Decor` and `Pieces` are still placeholders
- saturator and sidechain still need real DSP designs
- modulation can grow beyond the current Robin LFO/spread system
- more routing and thread-behavior regression coverage is still worth adding
