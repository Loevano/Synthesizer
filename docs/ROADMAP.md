# Roadmap

This file separates two concerns:

- generic platform work needed to keep the app stable and understandable
- synth-feature work described in [What should it do?.md](/Users/jens/Documents/Coding/Synthesizer/What%20should%20it%20do%3F.md)

The goal is to avoid mixing infrastructure work, DSP work, and UI work in one step.

## Current baseline

The project already has:

- a C++ synth engine: `Synth -> Voice -> Oscillator`
- a simplified debug source: `TestSynth -> Oscillator`
- a CoreAudio backend on macOS
- a native macOS app shell with embedded `WKWebView`
- a small JS/native bridge:
  - `getState()`
  - `setParam(path, value)`
- a browser UI organized around:
  - `Program`
  - `Source Mixer`
  - `Output Mixer`
  - `Robin`
  - `Test`
  - `Decor`
  - `Pieces`
  - `FX`
  - `LFO`

What is still missing is a stable note/event model, a proper routing model, modulation, FX, and external control.

## Generic platform roadmap

These milestones are not synth-specific. They make the app easier to extend safely.

### 1. Program and system surface

Goal:
- keep system concerns in a dedicated `Program` layer instead of mixing them into source controls

Scope:
- current audio config view:
  - sample rate
  - output channel count
  - frames per buffer
- current play mode / routing mode selection
- app status:
  - audio running
  - current bridge status
- device section scaffold:
  - current output device name when available
  - current output count when available

Reason:
- this keeps synth controls separate from infrastructure controls

### 2. Stable parameter model

Goal:
- make the controller the single source of truth for UI state

Scope:
- explicit structs for:
  - synth state
  - voice state
  - oscillator state
  - settings state
- JSON shape becomes intentional rather than ad hoc
- parameter path naming becomes stable

Reason:
- MIDI, OSC, presets, and automation all need this later

### 3. Event and note layer

Goal:
- stop treating the synth as a static drone only

Scope:
- note on / note off events
- voice allocation API
- trigger source abstraction:
  - UI
  - MIDI
  - test events

Reason:
- round-robin and held-note routing modes only become meaningful once notes exist

### 4. Device and control I/O

Goal:
- make the host controllable from the outside

Scope:
- MIDI input
- OSC parameter map
- eventually audio device selection if needed

Reason:
- this is the bridge from a local prototype into a playable instrument

## Feature roadmap from `What should it do?.md`

These milestones are the synth-specific roadmap.

### A. Voice and routing foundation

Goal:
- define what a voice is and how it maps to outputs

Scope:
- configurable voice count
- configurable oscillators per voice
- target defaults:
  - `8` voices
  - `4` oscillator slots per voice
- voice root frequency
- voice toggle
- voice spread
- play modes:
  - locked to output
  - round robin by trigger
  - output based on number of notes held

Dependency:
- generic milestone 3 (`Event and note layer`) is required for full round-robin / held-note behavior

### B. Oscillator model

Goal:
- make oscillator slots independent building blocks

Scope:
- per-oscillator:
  - waveform
  - gain
  - frequency
  - enabled state
  - relative-to-root toggle
- efficient oscillator path
- later optimization candidate:
  - lookup table or band-limited strategy if aliasing/performance becomes an issue

### C. Modulation system

Goal:
- add controlled complexity without making routing unreadable

Scope:
- LFO bank
- modulation routing to:
  - oscillators
  - filters
- controls:
  - phase spread over outputs
  - polarity flip
  - waveform type
  - linked vs unlinked output behavior
  - clock-linked vs fixed frequency

### D. Global envelopes and filters

Goal:
- shape motion at the synth level

Scope:
- three global envelopes
- trigger from note or external event
- route to:
  - LFO parameters
  - filters

### E. Multichannel FX

Goal:
- design effects around multichannel output rather than stereo assumptions

Scope:
- saturation/distortion:
  - algorithm choice
  - pre gain
  - post gain
  - linked gain option
- chorus:
  - depth
  - speed
  - phase spread over outputs
- sidechain:
  - still undefined, needs a design pass before implementation

### F. External control

Goal:
- make the synth playable and automatable from outside the app

Scope:
- MIDI note input
- OSC map for all parameters

## Recommended implementation order

This is the sequence that keeps the project buildable while still moving toward the target instrument.

### Phase 1

- `Program` page
- stable host/system state in the controller
- play mode enum and controller surface

### Phase 2

- voice root frequency
- voice spread
- per-oscillator relative frequency model
- oscillator frequency UI

### Phase 3

- note event layer
- voice allocator
- proper implementation of:
  - round robin by trigger
  - output by held-note count

### Phase 4

- LFO system
- modulation routing

### Phase 5

- global envelopes
- filters

### Phase 6

- multichannel FX

### Phase 7

- MIDI input
- OSC map

## Immediate next slice

The next implementation slice should be:

1. keep `engine`, `sourceMixer`, `outputMixer`, `sources`, and `processors` as stable controller concepts
2. make `Source Mixer` an explicit graph stage
3. add routing and output-mixer regression tests around the live graph
4. keep `Robin` stable as the reference concrete source node
5. then implement `Decor`, `Pieces`, and FX on top of that graph

That keeps the app shell and mental model stable before adding deeper DSP features.
