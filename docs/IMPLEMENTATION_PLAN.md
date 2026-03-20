# Framework Implementation Plan

This file is the practical implementation order for the current multichannel framework.

## Core decisions

- Keep one repo.
- Keep one app with clear internal modules.
- Keep the controller-side snapshot state and render-side state separate.
- Do not hide unfinished areas; scaffold them clearly.
- Prefer concrete graph modules until abstraction pressure is real.

## Current status

Already in place:

- CoreAudio-backed host
- embedded webview app shell
- grouped snapshot state
- `LiveGraph`
- `RobinSourceNode`
- `TestSourceNode`
- `FxRackNode`
- `OutputMixerNode`
- app-level `Synth` base
- `audio::PolySynth` as Robin's concrete engine
- `Effects` base for output DSP
- live `Robin`
- live `TestSynth`
- source dry/fx routing
- live output mixer level/delay/EQ
- live chorus
- scaffolded `Decor`
- scaffolded `Pieces`
- scaffolded saturator and sidechain
- patch save/load flow

## Repository shape to keep

Keep the repo conceptually split into:

- `app`
- `audio`
- `dsp`
- `graph`
- `platform`
- `ui`
- `docs`

It does not need a major folder rewrite right now.

## Implementation order

## Step 1. Keep the host/render boundary explicit

Goal:

- make parameter and transport threading easy to reason about

Deliverables:

- keep snapshot state authoritative for UI and patch I/O
- keep render-side state block-boundary driven
- keep naming and comments explicit where host code fans work out to render code
- add regression checks around command queue and note bookkeeping behavior

## Step 2. Lock down routing and allocator behavior

Goal:

- make the current live graph dependable before adding more DSP

Deliverables:

- regression checks around:
  - source enable
  - dry/fx routing
  - output mixer behavior
  - Robin voice allocation
  - Robin release-tail reuse behavior

## Step 3. Keep Robin stable as the reference source

Goal:

- let Robin carry the architecture while other sources remain scaffolded

Deliverables:

- preserve the master-first control model
- keep linked/local voice behavior predictable
- refine overlap / steal behavior where needed
- keep the web UI and snapshot state aligned

## Step 4. Add section-level modulation to Robin

Goal:

- add movement and spread without discarding the master-template model

Deliverables:

- section spread algorithms
- offset-based modulators
- routing-aware voice variation

Rule:

- do not jump into a full matrix before the section model is stable

## Step 5. Broaden the FX rack

Goal:

- turn the current FX path into a real processor lane

Deliverables:

- keep chorus stable
- implement saturator
- design sidechain properly before coding it
- keep future DSP effects aligned with the `Effects` base lifecycle

Rules:

- controls stay globally linked
- the Source Mixer chooses whether a source feeds dry or FX

## Step 6. Implement Decor

Goal:

- add the first clearly output-count-native source

Deliverables:

- one voice per output
- speaker-locked routing
- decorrelated spatial behavior

## Step 7. Implement Pieces

Goal:

- add a more experimental source after graph behavior is already proven

Deliverables:

- granular playback
- algorithmic routing across outputs

## Step 8. Improve oscillator scalability

Goal:

- make larger Robin patches cheaper and cleaner

Deliverables:

- evaluate lookup-table or band-limited strategies
- reduce aliasing on non-sine waveforms
- keep the current voice/oscillator model intact while upgrading internals

## Step 9. Keep preset/state persistence clean

Goal:

- save and restore snapshot-driven state cleanly

Deliverables:

- stable serialization
- preset load/save flow
- clear distinction between persistent state and runtime-only state

## Step 10. Keep docs and contributor setup current

Goal:

- prevent the project from becoming easy to break and hard to onboard

Deliverables:

- keep README current
- keep architecture and overview docs current
- keep contributor instructions current
