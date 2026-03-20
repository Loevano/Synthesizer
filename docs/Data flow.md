# Data Flow

This document describes how control data and audio data move through the app after the host/snapshot rename cleanup.

## High-level view

There are two main flows:

1. Control flow
   `Web UI -> WKWebView bridge -> SynthHost -> snapshot state / realtime commands`
2. Audio flow
   `Audio callback -> SynthHost -> queued command drain -> LiveGraph -> source nodes -> FX/output processing -> hardware outputs`

## 1. App shell and bridge

The macOS app shell lives in:

- `src/platform/macos/AppMain.mm`

At launch it:

- constructs `SynthHost`
- starts audio
- creates a `WKWebView`
- injects the JS bridge
- loads the bundled UI from `src/ui_web/`

The bridge surface currently exposes:

- `getState()`
- `setParam(path, value)`
- `setParamFast(path, value)`
- patch list/load/save helpers

Bridge requests arrive in `AppMain.mm`, call into `SynthHost`, and return through `window.__synthNativeReceive(...)`.

## 2. Web UI flow

The web UI lives in:

- `src/ui_web/index.html`
- `src/ui_web/styles.css`
- `src/ui_web/app.js`

The UI uses two control paths.

### Full-state path

Used for structural changes or actions where the UI wants a fresh authoritative state:

`UI event -> dispatchParam() -> window.synth.setParam() -> SynthHost -> full state JSON -> renderState()`

### Temp/live path

Used for most slider drags and patch-edit gestures:

`UI event -> temp UI state mutation -> window.synth.setParamFast() -> SynthHost -> optimistic UI stays in place`

This avoids a full JSON round-trip on every live gesture.

## 3. Snapshot state vs render state

This is the most important data-flow rule in the current codebase.

`SynthHost` keeps two copies of mutable synth/process state:

- controller-side snapshot state
- render-side live state

The snapshot copy exists for:

- state JSON generation
- UI reads
- patch save/load
- deterministic bridge behavior

The render copy exists for:

- audio-thread ownership
- block-boundary parameter updates
- avoiding UI-thread mutation of live DSP objects

## 4. Realtime parameter flow

The live parameter path is:

1. `SynthHost::tryEnqueueRealtimeNumericParam(...)` or `tryEnqueueRealtimeStringParam(...)`
2. build a `RealtimeCommand`
3. `applyStateMirrorCommandLocked(...)` updates the snapshot copy
4. `submitOrApplyRealtimeCommand(...)` decides whether the command must be queued or can be applied immediately
5. if audio is running, `queueRealtimeCommand(...)` stores it for the render thread
6. at block start, `drainQueuedRealtimeCommandsLocked(...)` moves every queued command into the render copy through `applyRenderStateCommandLocked(...)`
7. `LiveGraph::render(...)` runs with that updated render state

The important consequence is:

- the UI-visible state updates immediately
- the audio-visible state updates at the next block boundary

That is the current threading contract.

## 4a. Industry reference point

This split is close to what common plugin frameworks do:

- JUCE exposes realtime-readable parameter values and warns that parameter callbacks may happen synchronously and must stay thread-safe and non-blocking
- VST3 passes parameter changes into the processing call as block-scoped queues

That is why this codebase now prefers:

- snapshot updates on the control side
- immutable command handoff to the render side
- queue drain at the start of each audio block
- local DSP smoothing where abrupt value jumps would click

## 5. Audio startup flow

Startup currently looks like this:

1. `AppMain.mm` constructs `SynthHost`
2. `SynthHost::startAudio()` calls `initialize()` if needed
3. `initialize()`:
   - sets up logging and crash diagnostics
   - creates the audio driver
   - builds the `LiveGraph`
   - builds default source and processor state
   - syncs the render copy from the snapshot copy
4. `startAudio()` starts the driver with the current runtime config
5. MIDI and OSC are started after audio comes up successfully

## 6. Audio render flow

The realtime render path is:

`Audio driver -> SynthHost::processAudioBlockLocked(...) -> drain queued commands -> LiveGraph::render(...) -> hardware outputs`

`LiveGraph` itself runs in this order:

1. clear main output buffer
2. clear FX bus buffer
3. render each enabled source into either the main output or the FX bus
4. process the FX bus with `FxRackNode`
5. sum the FX bus back into the main output
6. process the main output with `OutputMixerNode`

## 7. Source-node flow

Current live sources:

- `RobinSourceNode`
- `TestSourceNode`

`RobinSourceNode` owns the `audio::PolySynth` engine used by Robin. `TestSourceNode` owns `audio::TestEngine`.

Both source nodes apply source-level gain smoothing before adding audio into the graph buffer so mixer drags do not click.

## 8. Robin flow

`Robin` owns the product-facing rules:

- linked/local voice state
- routing presets
- note-to-voice assignments
- release-tail reuse protection

On note-on:

1. Robin allocates a voice
2. it syncs pitch and routing for that voice
3. it calls `sourceNode_.engine().noteOn(voiceIndex)`

On note-off:

1. Robin calls `sourceNode_.engine().noteOff(voiceIndex)`
2. it tracks the release window for voice reuse
3. it removes the active assignment

`audio::PolySynth` then fans those decisions out to `Voice` instances during `process(...)`.

## 9. MIDI and OSC flow

Incoming MIDI and OSC messages are normalized into host actions, then either:

- update held-note bookkeeping directly, or
- become `RealtimeCommand` objects

The same queue/drain rule still applies when those events must affect render-side DSP state.

## 10. Patch flow

Patch save/load is intentionally snapshot-driven.

- the UI keeps a temp patch state for editing
- `SynthHost::stateJson()` exposes the snapshot form
- native patch save/load serializes only the persistent state groups
- runtime-only engine details are rebuilt, not stored continuously
