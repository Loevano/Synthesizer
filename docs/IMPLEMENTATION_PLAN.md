# Framework Implementation Plan

This file is the practical implementation order for the current multichannel framework.

## Core decisions

- Keep one repo.
- Keep one app with clear internal modules.
- Keep the controller state shape intentional.
- Do not hide unfinished areas; scaffold them clearly.
- Prefer concrete graph modules until abstraction pressure is real.

## Current status

Already in place:

- CoreAudio-backed host
- embedded webview app shell
- grouped controller state
- `LiveGraph`
- `RobinSourceNode`
- `TestSourceNode`
- `FxRackNode`
- `OutputMixerNode`
- live `Robin`
- live `Test`
- source dry/fx routing
- live output mixer level/delay/EQ
- live chorus
- scaffolded `Decor`
- scaffolded `Pieces`
- scaffolded saturator and sidechain

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

## Step 1. Lock down routing and allocator behavior

Goal:

- make the current live graph dependable before adding more DSP

Deliverables:

- regression checks around:
  - source enable
  - dry/fx routing
  - output mixer behavior
  - Robin voice allocation
  - Robin release-tail reuse behavior

## Step 2. Keep Robin stable as the reference source

Goal:

- let Robin carry the architecture while other sources remain scaffolded

Deliverables:

- preserve the master-first control model
- keep linked/local voice behavior predictable
- refine overlap / steal behavior where needed
- keep the web UI and controller state aligned

## Step 3. Add section-level modulation to Robin

Goal:

- add movement and spread without discarding the master-template model

Deliverables:

- section spread algorithms
- offset-based modulators
- routing-aware voice variation

Do not do yet:

- do not jump into a huge general matrix if section-level behavior is still unclear

## Step 4. Broaden the FX rack

Goal:

- turn the current FX path into a real processor lane

Deliverables:

- keep chorus stable
- implement saturator
- design sidechain properly before coding it

Rules:

- controls stay globally linked
- the Source Mixer chooses whether a source feeds dry or FX

## Step 5. Implement Decor

Goal:

- add the first clearly output-count-native source

Deliverables:

- one voice per output
- speaker-locked routing
- decorrelated spatial behavior

## Step 6. Implement Pieces

Goal:

- add a more experimental source after graph behavior is already proven

Deliverables:

- granular playback
- algorithmic routing across outputs

## Step 7. Improve oscillator scalability

Goal:

- make larger Robin patches cheaper and cleaner

Deliverables:

- evaluate lookup-table or band-limited strategies
- reduce aliasing on non-sine waveforms
- keep the current voice/oscillator model intact while upgrading internals

## Step 8. Add preset/state persistence

Goal:

- save and restore controller-driven state cleanly

Deliverables:

- stable serialization
- preset load/save flow
- clear distinction between persistent state and runtime-only state

## Step 9. Keep docs and contributor setup current

Goal:

- prevent the project from becoming easy to break and hard to onboard

Deliverables:

- keep README current
- keep architecture/overview docs current
- keep contributor instructions current
