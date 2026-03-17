# Data Flow

This document describes how data currently moves through the app, from UI input to DSP output, and where the main improvement opportunities are.

It reflects the code as it exists now, not an ideal future design.

## High-level view

There are two main flows:

1. Control flow
   `Web UI -> native bridge -> SynthController -> graph/synth state`
2. Audio flow
   `CoreAudio callback -> SynthController -> LiveGraph -> sources -> FX/output processing -> hardware outputs`

## 1. App shell and bridge

The macOS app shell lives in:

- `src/platform/macos/AppMain.mm`

At launch it:

- creates the `SynthController`
- starts audio
- creates a `WKWebView`
- injects the JS bridge helpers
- loads the bundled UI from `src/ui_web/`

The bridge currently exposes:

- `getState()`
- `setParam(path, value)`
- `setParamFast(path, value)`

Current bridge shape:

- the web UI sends a request through `window.webkit.messageHandlers.synth`
- `AppMain.mm` receives it in `userContentController:didReceiveScriptMessage:`
- it calls `controller_->stateJson()` or `controller_->setParam(...)`
- it returns the result back into JS through `window.__synthNativeReceive(...)`

Important current behavior:

- bridge work is still handled synchronously in the native app shell
- `setParam` returns full state JSON
- `setParamFast` returns only success/failure

## 2. Web UI flow

The web UI lives in:

- `src/ui_web/index.html`
- `src/ui_web/styles.css`
- `src/ui_web/app.js`

The UI has two parameter-update paths.

### Full-state path

Used for structural or slower updates.

Path:

`UI event -> dispatchParam() -> window.synth.setParam() -> native setParam -> full state JSON back -> renderState()`

This path is used when the UI wants the controller to be the source of truth immediately.

### Temp/live path

Used for most live control drags.

Path:

`UI event -> setParamTemp()/setParamTempLive() -> dispatchParamFast() -> native setParam -> local optimistic UI mutation`

This is the path that avoids a full JSON round-trip on every drag.

That logic is important because many earlier UI crashes were caused by full state sync happening in the middle of live edits.

## 3. SynthController role

The controller lives in:

- `include/synth/app/SynthController.hpp`
- `src/app/SynthController.cpp`

It is currently the central orchestrator for almost everything:

- runtime config
- grouped app state
- JSON serialization
- bridge-facing parameter mutation
- graph construction
- source routing
- Robin voice allocation
- MIDI input hookup
- OSC input hookup
- audio start/stop

This is the current state contract exposed to the UI:

- `engine`
- `graph`
- `sourceMixer`
- `outputMixer`
- `sources`
- `processors`

That grouping is strong and should be preserved even if the controller gets split later.

## 4. Audio startup flow

Startup currently looks like this:

1. `AppMain.mm` constructs `SynthController`
2. `SynthController::startAudio()` calls `initialize()` if needed
3. `initialize()`:
   - sets up logging and crash diagnostics
   - creates the audio driver
   - builds the live graph
   - configures Robin/Test/default state
4. `startAudio()` asks the driver to start with the current config
5. CoreMIDI and OSC are started after audio starts successfully

Relevant files:

- `src/platform/macos/AppMain.mm`
- `src/app/SynthController.cpp`
- `src/interfaces/AudioDriverCoreAudio.cpp`

## 5. Live audio render flow

The real-time render path is:

`CoreAudio -> SynthController::renderAudioLocked -> LiveGraph::render -> source nodes -> FX bus -> dry/fx sum -> OutputMixerNode -> hardware outputs`

### CoreAudio

The CoreAudio backend lives in:

- `src/interfaces/AudioDriverCoreAudio.cpp`

It owns the HAL output unit and calls back into the app for each audio block.

### Controller render entry

The driver callback calls:

- `SynthController::renderAudioLocked(...)`

That function hands off directly to:

- `LiveGraph::render(...)`

### LiveGraph

The graph lives in:

- `include/synth/graph/LiveGraph.hpp`
- `src/graph/LiveGraph.cpp`

Current graph model:

- a list of source nodes
- a list of output processor nodes
- one explicit FX bus buffer

Current render order:

1. clear main output buffer
2. clear FX bus buffer
3. render each enabled source into:
   - main output, or
   - FX bus
4. run FX-bus processors on the FX bus
5. sum FX bus back into the main output
6. run main output processors on the main output

This is one of the cleanest parts of the architecture.

## 6. Source node flow

Current live sources:

- `RobinSourceNode`
- `TestSourceNode`

Files:

- `src/graph/RobinSourceNode.cpp`
- `src/graph/TestSourceNode.cpp`

### RobinSourceNode

Responsibilities:

- own the `audio::Synth` used by Robin
- apply source-level smoothing
- render Robin into the graph target buffer

Current behavior:

- Robin renders into an internal temp buffer first
- source-mixer gain is ramped across the block
- the smoothed result is added into the target graph buffer

### TestSourceNode

Responsibilities:

- own the simplified test synth
- apply source-level smoothing
- render into the graph target buffer

It follows the same broad pattern as Robin, but with a simpler synth underneath.

## 7. Robin internal flow

Robin's DSP path lives mainly in:

- `src/audio/Synth.cpp`
- `src/audio/Voice.cpp`

### Controller side

The controller owns Robin's product-facing state:

- master oscillator bank
- per-voice linked/local state
- routing preset
- note-to-voice assignments
- release-tail reuse timing

On note-on:

1. the controller allocates a Robin voice
2. it syncs the tuned frequency for that voice
3. it assigns the output routing
4. it calls `robinSource_.synth().noteOn(voiceIndex)`

On note-off:

1. the controller calls `noteOff(voiceIndex)`
2. it stamps a release-until time for reuse protection
3. it removes the live note assignment

### Synth side

`audio::Synth` is mostly a per-voice dispatcher.

It owns:

- `voices_`
- one LFO
- output modulation buffer
- shared setters that fan out to voices

Current render flow in `Synth::renderAdd(...)`:

1. resize/fill the LFO modulation buffer
2. render one frame of LFO modulation per output
3. ask every voice to render into the final output buffer

### Voice side

Each `Voice` owns:

- oscillator slots
- AMP envelope
- filter envelope
- low-pass filter
- output enable map

Current `Voice::renderAdd(...)` flow:

1. skip if inactive
2. sum enabled oscillators
3. compute filter envelope and cutoff
4. filter the sample
5. apply AMP envelope
6. write only to enabled outputs
7. multiply by per-output modulation if present

This is a straightforward model and matches the current Robin UI well.

## 8. MIDI and OSC flow

Both are wired through the controller.

### MIDI

After audio starts, the controller starts `MidiInput`.

Incoming MIDI callbacks go to:

- `handleMidiNoteOnLocked(...)`
- `handleMidiNoteOffLocked(...)`

The controller then routes MIDI per connected source according to its MIDI-source routing state.

### OSC

After audio starts, the controller starts `OscServer`.

Incoming OSC messages are turned into:

- numeric `setParam(...)`
- string `setParam(...)`
- note on/off events

This means the controller is already the shared mutation surface for:

- web UI
- OSC
- MIDI-triggered synth behavior

## 9. Current architectural strengths

These parts are already solid:

- The state grouping is intentional and useful.
- The dry/fx split is clear.
- The graph order is easy to reason about.
- Robin's master-linked/local-override model maps cleanly into the DSP structure.
- The temp/live UI path is the right direction for performance-sensitive controls.

## 10. Current architectural weaknesses

These are the main areas worth improving.

### A. The controller does too much

`SynthController` currently owns too many responsibilities.

It would be healthier to split it eventually into smaller units such as:

- engine/session control
- state serialization
- Robin-specific logic
- external input routing

This is the biggest maintainability issue in the current codebase.

### B. One mutex sits on too much traffic

The same controller mutex is used across:

- audio callback path
- MIDI callbacks
- OSC callbacks
- many UI param updates

That means control traffic and render traffic are more tightly coupled than they should be.

Longer term, the better direction is:

- audio-thread-safe render snapshot
- queued control updates
- less direct shared locking on the real-time path

### C. Per-block buffer allocation is still happening

Current examples:

- `fxBusBuffer_.assign(...)` in `LiveGraph`
- `renderBuffer_.assign(...)` in `RobinSourceNode`
- `renderBuffer_.assign(...)` in `TestSourceNode`
- `lfoModulationBuffer_.assign(...)` in `Synth`

These buffers should ideally be reused and only grown when needed.

This is one of the clearest DSP-side scalability improvements available now.

### D. Manual JSON assembly is fragile

`stateJson()` is still hand-built with string streams.

That works, but it is easy to make shape mistakes when the state evolves.

This is not the first thing to optimize for performance, but it is a good correctness and maintainability target.

### E. Structural engine changes are not isolated enough

Output-device and other engine-structural changes still live close to ordinary param mutation behavior.

Conceptually, these should probably be treated as a separate command class:

- not ordinary synth params
- not live-drag params
- explicitly restart-sensitive operations

## 11. Best next internal improvements

If the goal is to improve the architecture without a huge rewrite, the most logical order is:

1. Remove per-block buffer churn in render paths.
2. Reduce audio-thread coupling to the controller mutex.
3. Split `SynthController` responsibilities gradually.
4. Make structural engine commands more explicit.
5. Replace or wrap manual JSON assembly with a safer serialization layer.

## 12. Summary

The current architecture is already coherent:

- UI talks to one controller
- controller owns product state
- graph owns render order
- sources render to dry or FX
- output processors finish the path
- Robin is the main reference instrument

The next improvements should focus less on inventing new flow and more on reducing coupling:

- less controller centralization
- less real-time locking
- less per-block allocation
- clearer separation between live params and structural engine commands
