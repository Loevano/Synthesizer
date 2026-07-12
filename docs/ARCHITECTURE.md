# Architecture

This document describes the current code structure and the protected runtime boundaries.

Architecture docs are part of the collaboration contract. If a change alters render flow, state ownership, routing, or build structure, update this file or [Data flow.md](Data%20flow.md) in the same PR.

## Live render path

`Audio driver -> SynthHost -> LiveGraph sources -> dry/fx split -> FxRackNode -> dry + fx sum -> OutputMixerNode -> hardware outputs`

Current live pieces:

- CoreAudio backend on macOS
- `Robin`, `TestSynth`, and `Pieces` as implemented sources
- source enable, source level, and dry/fx routing
- `Chorus` in the FX rack
- output mixer level, delay, and EQ
- Robin LFO and spread modulation
- Pieces sample loading and pitch-shifted sampler playback
- patch save/load through the macOS bridge

Current scaffold-only pieces:

- `Decor`
- `Saturator` DSP
- `Sidechain` DSP

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
- `PiecesSynth`

`SourceState.hpp` exists so small controller-facing structs such as `EnvelopeState` and `TestSourceState` can be reused without dragging full synth classes into unrelated headers.

### Render engines

App-level synths do not render samples directly. They delegate to concrete audio engines:

- `audio::PolySynth` for Robin
- `audio::TestEngine` for TestSynth
- `audio::SamplePlayerEngine` for Pieces

That split keeps product-facing behavior such as routing presets and linked/local voice rules out of the raw DSP layer.

### DSP effects

The shared DSP effect base is `synth::dsp::Effects`.

Right now only `Chorus` derives from it. `Saturator` and `Sidechain` have UI/state scaffolds but no DSP implementation yet. The base gives future output effects one consistent lifecycle:

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

## Protected architecture boundaries

The following areas are protected collaboration boundaries:

- audio render engines: `src/audio/` and `include/synth/audio/`
- DSP primitives and effects: `src/dsp/` and `include/synth/dsp/`
- render graph and routing: `src/graph/` and `include/synth/graph/`
- host state and realtime command contracts: `SynthHost` and `RealtimeCommands`
- build and CI definitions: `CMakeLists.txt` and `.github/workflows/`

Normal UI, docs, and small feature branches should not edit these areas. Use a dedicated `core/`, `dsp/`, `routing/`, `architecture/`, `infra/`, or `hotfix/` branch when this boundary must change, and include focused verification.

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
- spread modulation slots with macro depth controls
- output-aware LFO modulation
- routing presets across outputs
- note allocator that avoids immediate reuse of voices still in release

`Robin` owns the musical/product-facing rules. `audio::PolySynth` owns the actual per-voice DSP.

## Patch library

Patch persistence is snapshot-driven.

- the web UI extracts a persistent patch snapshot from live state
- the native bridge lists, loads, and saves JSON patch files
- development checkouts use repo-local `./Patches` when it exists
- packaged builds use Application Support storage
- loading a patch sends normal parameter operations back through `SynthHost`

Runtime-only engine details are rebuilt rather than stored continuously in patch files.

## TestSynth

`TestSynth` stays intentionally small:

- one oscillator
- one `VCA ENV`
- manual output routing
- optional continuous tone
- optional MIDI response

It is the simplest useful source for regression tests and routing checks.

## Pieces

`Pieces` is the sampler source and the planned base for granulator mode.

Current model:

- one decoded mono sample buffer
- MIDI-triggered polyphonic sample voices
- root note, transpose, fine tune, gain, playback mode, reverse, start, and end controls
- waveform preview peaks with directly draggable playback-window markers in the browser UI
- `VCA ENV`
- manual output targets
- source mixer dry/FX routing

Sample file decoding happens on the control side before a realtime command is queued. The render side receives a prepared shared sample buffer and swaps it at a block boundary, so the audio callback does not open or decode files.

## What still needs work

- `SynthHost` is still large and should shrink over time
- `Decor` is still a placeholder
- Pieces still needs multisample, velocity zones, detailed waveform editing, and granulator mode
- saturator and sidechain still need real DSP designs
- modulation can grow beyond the current Robin LFO/spread system into a clearer destination model
- more routing and thread-behavior regression coverage is still worth adding
