# Roadmap

This file separates platform work from instrument work so infrastructure, DSP, and UI changes do not blur together.

## Current baseline

Already in place:

- CoreAudio-backed host on macOS
- native macOS app shell with embedded `WKWebView`
- JS/native bridge with fast and full-state parameter paths
- `SynthHost` with explicit snapshot-state vs render-state handling
- reusable graph modules:
  - `LiveGraph`
  - `RobinSourceNode`
  - `TestSourceNode`
  - `FxRackNode`
  - `OutputMixerNode`
- app-level `Synth` base
- `audio::PolySynth` as Robin's render engine
- `Effects` base for output DSP
- live `Robin`
- live `TestSynth`
- source-level dry/fx routing
- per-output level, delay, and EQ
- live chorus
- CoreMIDI input
- OSC control surface
- patch save/load flow

Still missing:

- deeper modulation system
- `Decor` DSP
- `Pieces` DSP
- full saturator
- sidechain design
- broader automated regression coverage

## Platform roadmap

### 1. Keep the parameter thread model tight

Goal:

- make state ownership and command handoff harder to break

Scope:

- keep snapshot state authoritative for UI and patch I/O
- keep render changes block-boundary driven
- add focused tests around queued realtime commands and note bookkeeping

### 2. Expand regression coverage

Goal:

- make routing, allocation, and state changes safer to evolve

Scope:

- source enable/disable behavior
- dry/fx routing behavior
- output mixer processing behavior
- Robin allocator behavior and release-tail handling

### 3. Improve smoothing policy

Goal:

- apply consistent anti-zipper handling where live drags can click

Scope:

- any remaining abrupt source/process gains
- modulation-depth changes where needed
- future routing-sensitive controls

### 4. Keep docs and contributor ergonomics current

Goal:

- make the repo easier to understand and extend

Scope:

- keep architecture and overview docs current
- maintain contributor setup instructions
- keep helper scripts accurate

## Feature roadmap

### A. Finish Robin as the reference instrument

Goal:

- make Robin solid enough to carry the architecture while other sources remain scaffolded

Scope:

- keep the master-first voice design
- keep local voice override behavior predictable
- improve remaining edge cases around voice stealing and release tails

### B. Expand modulation carefully

Goal:

- grow beyond the current LFO/spread layer without jumping into a messy matrix too early

Scope:

- section-level modulators
- voice spread algorithms
- routing-aware modulation
- clearer destination naming such as `VCF ENV` and `VCA ENV`

### C. Expand the FX rack

Goal:

- make the FX path more than a chorus proof-of-concept

Scope:

- saturator implementation
- sidechain design
- future processors built on the shared `Effects` lifecycle

### D. Implement Decor

Goal:

- add the first source whose voice count is tightly coupled to output count

Scope:

- one voice per output
- speaker-locked behavior
- immersive decorrelated playback design

### E. Implement Pieces

Goal:

- add a granular or algorithmic source once the graph and routing model are more stable

Scope:

- playable grain triggering
- spatial grain or voice distribution

### F. Improve oscillator quality

Goal:

- scale oscillator CPU cost and audio quality more gracefully as voice count rises

Scope:

- lookup-table or band-limited strategies where useful
- reduce aliasing on non-sine waveforms
- keep the oscillator model scalable for larger Robin patches

## Recommended order

1. Keep the parameter thread model explicit and tested.
2. Expand routing and allocator regression coverage.
3. Keep Robin stable as the reference live source.
4. Expand modulation one coherent layer at a time.
5. Expand the FX rack beyond chorus.
6. Implement `Decor`.
7. Implement `Pieces`.
