# Architecture

This document describes the current live scaffold, not the final target system.

## Live render path today

`Audio Engine -> LiveGraph sources -> dry/fx split -> FX Rack (fx bus only) -> dry + fx sum -> Output Mixer -> hardware outputs`

Current reality:

- CoreAudio is the active backend on macOS
- `Robin` and `Test` are the current live sources
- source-level gain is active
- source route target (`dry` or `fx`) is active
- chorus is live in the FX rack
- output mixer level, delay, and EQ are live
- `Decor` and `Pieces` remain scaffold-only
- `Saturator` and `Sidechain` remain scaffold-only

## Controller and graph model

The controller owns the product-facing state and builds a concrete `LiveGraph`.

Current source nodes:

- `RobinSourceNode`
- `TestSourceNode`
- `Decor` placeholder
- `Pieces` placeholder

Current output processor nodes:

- `FxRackNode` on the FX bus
- `OutputMixerNode` on the main summed path

## Source routing model

Each source has two routing decisions in practice:

- is it enabled?
- does it render to the dry bus or the FX bus?

`LiveGraph` handles this by giving each source a render target:

- `Dry`
- `FxBus`

The FX bus is processed separately, then summed back into the main output before the output mixer stage.

## Source Mixer responsibilities

The source mixer is a source-level performance layer.

Current active behavior:

- source enable
- source level
- dry/fx target selection

Implementation note:

- source-mixer level changes for `Robin` and `Test` are smoothed in the source-node layer to avoid zipper noise and pops while dragging

## Output Mixer responsibilities

The output mixer is a per-output correction stage.

Current active behavior:

- level
- delay
- fixed-band EQ

This stage runs after the dry and FX paths are summed.

## Robin architecture today

Robin is the most important concrete source in the current codebase.

Current model:

- configurable voice count
- configurable oscillator slots per voice
- all configured voices enabled by default
- each voice can stay linked to the master voice or become a local override
- linked voices follow the master oscillator bank, `VCF`, `ENV VCF`, and `AMP`
- unlinked voices keep their own local state
- routing presets assign voices across the output array

Runtime behavior note:

- Robin source level is smoothed at the source-node layer
- Robin note allocation now tries to avoid reusing a voice whose release tail is still active

## Test architecture today

`Test` is intentionally simpler:

- one oscillator
- one envelope
- manual output routing
- optional continuous tone
- optional MIDI response

It exists as the smallest useful signal source for debugging the engine and routing path.

## FX architecture today

FX are modeled as per-output processors, but their controls are globally linked by design.

Current live processor:

- `Chorus`

Current scaffold processors:

- `Saturator`
- `Sidechain`

The key routing rule now is:

- the source decides whether it goes to dry or FX in the Source Mixer
- outputs always receive the dry path plus the processed FX path sum

## Native/UI architecture

The app shell is a macOS bundle with an embedded `WKWebView`.

Current bridge surface:

- `getState()`
- `setParam(path, value)`

The web UI is bundled from:

- `src/ui_web/index.html`
- `src/ui_web/styles.css`
- `src/ui_web/app.js`

The UI uses a temp/live control path for ordinary parameter edits so it does not round-trip the full state JSON for every drag.

## State shape

The controller currently exposes state grouped as:

- `engine`
- `graph`
- `sourceMixer`
- `outputMixer`
- `sources`
- `processors`

This structure is intentionally more important than any one page layout. It is the contract the UI, OSC bridge, and future preset logic build on.

## What is still missing

Important missing pieces:

- deeper Robin modulation beyond the current LFO and local overrides
- `Decor` DSP
- `Pieces` DSP
- full saturator implementation
- real sidechain design
- broader regression testing around routing and allocator behavior

## Next architecture refactor

The next structural step should be to reduce how much `SynthController` owns directly.
Right now it still acts as engine host, state serializer, source router, Robin controller,
and Test source controller all at once. That made the lock-pressure refactor possible, but
it is also the main reason the file has become hard to read and harder to extend safely.

The recommended direction is shallow inheritance plus composition:

- keep `SynthController` responsible for engine lifecycle, device/output config, and top-level graph orchestration
- introduce a small shared instrument/source interface for common lifecycle only
- extract synth-specific logic into dedicated classes instead of growing one giant controller

The shared base should stay small. It should define only the parts that are actually stable
across instruments:

- note on/off
- render
- numeric param apply
- string param apply
- state serialization
- queued command drain, if needed

If a second-level base becomes useful, it should be for genuinely shared polyphonic synth
structure, not for every possible future feature. A `PolySynthInstrument` style base can
make sense for:

- voices
- oscillator bank
- envelopes
- filter section

It should not force unrelated details like routing presets, modulation systems, or allocator
behavior into the base type.

Shared helpers should prefer composition:

- realtime command queue
- allocator
- snapshot cache
- routing helpers

Target extraction order:

1. define the small base interface
2. extract `Test` first because it is simpler
3. extract `Robin` once the pattern is proven
4. simplify `SynthController` so it orchestrates instead of implementing every synth directly

Constraints for that refactor:

- preserve current behavior
- keep the queued live-param model introduced on `stability-refactor-foundation`
- do not rewrite the whole host at once
- move one synth at a time
- add regression tests as responsibilities move

The guiding principle is:

- yes to inheritance for a clear blueprint
- no to deep inheritance or a bloated god base class
