# Pieces

This document defines the first scaffold for `Pieces`.

`Pieces` should become the project's granular source: playable, multichannel-aware, and deliberately more experimental than `Robin`.

## Role

`Pieces` should be:

- a granular synth source
- able to use either live input or sample-based material
- able to distribute grains across outputs through routing algorithms
- simple enough in the first pass to make the graph, state, and UI behavior clear before the DSP becomes ambitious

Within the product model, `Pieces` is a source in the same category as `Robin`, `Test`, and `Decor`:

- it lives in the Source Mixer
- it can be enabled or disabled
- it chooses `Dry` or `Fx`
- it owns its own synth state
- it should inherit from `synth::app::Synth`
- it renders into the multichannel graph

## Core sound model

`Pieces` reads from one shared grain source buffer and emits many short grains from it.

The source buffer should support two modes:

### 1. Live input mode

- incoming live audio is captured into a circular buffer
- grains read from that rolling buffer
- this mode is for immediate, reactive granular playback
- freeze/hold behavior is useful later, but not required for the first implementation

### 2. Sample mode

- grains read from a loaded sample buffer
- playback position can stay fixed, scan, or jitter around a region
- this mode is the easiest way to get stable first-pass behavior and testability

Rule for the first implementation:

- sample mode should land first
- live input mode should be designed now so state and DSP boundaries do not need to be rewritten later

## Multichannel behavior

`Pieces` should not think in stereo. Grain distribution is part of the instrument.

Each emitted grain should choose one output destination, or a small output span, through a selectable routing algorithm.

Initial algorithm set:

- `Round Robin`
  - each new grain moves to the next output
- `Random`
  - each grain picks an output randomly
- `Ping Pong`
  - grains scan forward and backward through the output list
- `Center Out`
  - grains begin near the center outputs and spread outward
- `Spray`
  - grains cluster around a moving center with adjustable spread

Later candidates:

- weighted outputs
- Euclidean or pattern-based routing
- grain duplication across neighboring speakers
- motion trajectories driven by LFO or transport

For the first pass, keep one grain assigned to one output. That keeps the render path, gain logic, and testing simpler.

## Grain controls

The first control layer should stay small and obvious.

Required controls:

- `Grain Size`
  - duration of each grain
- `Grain Density`
  - how many grains are emitted over time
- `Active Grains`
  - max simultaneous grains
- `Position`
  - base read point in the buffer
- `Position Jitter`
  - random offset around the base position
- `Pitch`
  - transposition of grain playback
- `Pitch Jitter`
  - random pitch variation per grain
- `Output Algorithm`
  - routing strategy across outputs
- `Output Spread`
  - how wide the algorithm can move or cluster grains
- `Level`
  - source output trim

Useful but optional first-pass controls:

- `Grain Envelope`
  - choose a simple grain window shape
- `Reverse Probability`
  - occasional reversed grains
- `Pan Width`
  - only relevant if grain assignment later expands beyond one output

## Feedback section

`Pieces` should include an internal feedback path.

The purpose is not classic delay feedback. The goal is to let grains re-enter the source material path and create evolving clouds.

Minimum controls:

- `Feedback Amount`
- `Feedback High-Pass`
- `Feedback Low-Pass`

Behavior:

- feedback is written back into the capture/sample working buffer or a dedicated feedback buffer
- the feedback path is filtered before re-entry
- high-pass prevents low-frequency build-up
- low-pass prevents harsh high-frequency accumulation

First-pass safety rules:

- clamp feedback to a stable range
- reset feedback state cleanly on transport/source reset
- avoid hidden runaway gain

## Signal flow

Conceptually:

`Input or Sample Buffer -> Grain Scheduler -> Grain Voices -> Output Routing Algorithm -> Source Output`

With feedback:

`Input or Sample Buffer -> Grain Scheduler -> Grain Voices -> Output Routing Algorithm -> Source Output`

`Source Output or Grain Sum -> Feedback HP/LP -> Feedback Buffer Writeback`

Implementation detail can change, but the product behavior should stay readable:

- one source buffer
- one scheduler
- many short-lived grain voices
- one routing decision per grain
- one feedback loop with tone shaping

## State and UI scaffold

The UI page for `Pieces` should move from placeholder text to a minimal but complete control surface.

Suggested first page sections:

### Source

- enable
- level
- dry/fx route
- source mode: `Sample` or `Live Input`

### Buffer

- sample selector or sample status
- live input status
- position
- position jitter

### Grains

- grain size
- grain density
- active grains
- pitch
- pitch jitter
- envelope shape later if needed

### Output Distribution

- output algorithm
- output spread
- optional small output activity map

### Feedback

- feedback amount
- feedback high-pass
- feedback low-pass

## Render/state architecture fit

`Pieces` should follow the same high-level rule as the rest of the app:

- controller-side snapshot state for UI and patch save/load
- render-side state updated at block boundaries

Recommended structure:

- app-level `Pieces` source class alongside `Robin` and `TestSynth`
- that class should derive from `synth::app::Synth` so it follows the existing source lifecycle, realtime command flow, and state export contract
- dedicated render engine for grain scheduling and DSP
- small reusable source-state struct in `SourceState.hpp`
- realtime commands for parameter updates, mode changes, and sample/capture state

Important split:

- sample loading and live-input setup are host-side concerns
- grain scheduling and playback are render-side concerns

## First implementation slice

To keep scope sane, the first real `Pieces` milestone should be:

1. sample-mode only
2. mono source buffer is acceptable
3. one grain maps to one output
4. `Round Robin` and `Random` routing only
5. controls for grain size, grain density, active grains, position, pitch, level
6. feedback amount plus high-pass and low-pass

That is enough to prove:

- the source architecture
- the routing behavior across many outputs
- the UI/state contract
- the feedback design

After that, add:

1. live input capture
2. more routing algorithms
3. jitter controls
4. stronger visualization
5. richer grain envelope and scan behavior

## Open design questions

- whether live input should capture from a dedicated hardware input or from an internal bus
- whether feedback should feed the shared source buffer directly or a separate resampling buffer
- whether output algorithms choose one destination per grain or support multi-output grain spreading
- how sample loading should appear in the current app shell
- whether `Pieces` should be note-triggered, free-running, or support both

## Working definition

If `Robin` is the precise playable synth and `Decor` is the speaker-locked texture source, `Pieces` should be the playable granular cloud instrument:

- source material from sample or live input
- grains emitted from that material
- grains distributed through multichannel algorithms
- direct controls for grain behavior
- an internal feedback loop shaped by high-pass and low-pass filtering
