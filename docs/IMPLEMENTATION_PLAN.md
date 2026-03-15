# Framework Implementation Plan

This file is the practical step-by-step plan for building the multichannel framework.

It is meant to answer:

- what already exists
- what order to implement things in
- what not to build too early

## Core decisions

- Keep **one git repo** for now.
- Keep **one app** with clear internal modules instead of splitting into multiple repos early.
- Do **scaffolding before DSP**.
- Keep placeholder pages and placeholder state visible so new work has a clear landing zone.

## Current status

Already in place:

- CoreAudio-backed audio engine
- controller state grouped as:
  - `engine`
  - `graph`
  - `sourceMixer`
  - `outputMixer`
  - `sources`
  - `processors`
- explicit internal source-stage order in the controller:
  - `Robin`
  - `Test`
  - `Decor`
  - `Pieces`
- explicit internal output-processor stage order in the controller:
  - `Output Mixer`
- reusable concrete graph modules:
  - `LiveGraph`
  - `RobinSourceNode`
  - `TestSourceNode`
  - `OutputMixerNode`
- `Robin` and `Test` as the live sources in the DSP path
- source-level gain for `Robin` and `Test`
- per-output trim in the output mixer
- per-output delay in the output mixer
- per-output fixed-band EQ in the output mixer
- scaffold state for:
  - `Decor`
  - `Pieces`
  - FX

Not implemented yet:

- `Decor` DSP
- `Pieces` DSP
- real FX processing
- sidechain design

## Repo structure to keep

Stay in one repo and evolve toward these internal areas:

- `engine`
- `graph`
- `sources`
- `processors`
- `controller`
- `ui`
- `tests`

This does not need to be a physical folder refactor immediately. It is the target architectural split.

## Step-by-step plan

### Step 1. Stabilize the scaffold state

Goal:
- keep the nested state model stable enough that UI and backend stop changing shape every session

Done when:
- `engine`, `sourceMixer`, `outputMixer`, `sources`, and `processors` stay stable
- new work extends that structure instead of bypassing it

Notes:
- this is mostly done now

### Step 2. Introduce explicit internal graph modules

Goal:
- stop treating the whole backend as one controller talking directly to one synth object

Add:
- graph wiring layer
- reusable concrete source nodes
- reusable concrete output processor nodes

Do not do yet:
- do not rewrite DSP at the same time
- do not force inheritance just to make the graph look more abstract

Current status:
- done at the concrete-module level:
  - `LiveGraph`
  - `RobinSourceNode`
  - `TestSourceNode`
  - `OutputMixerNode`
- formal interfaces can wait until duplication becomes real

### Step 3. Make Robin the first concrete source node

Goal:
- move the current synth into the new source abstraction without changing its behavior

Deliverables:
- `RobinSource`
- routing still works
- voice editing still works
- linked oscillator editing still works
- LFO still works

Done when:
- the graph renders `Robin` through an interface instead of directly through `SynthController`

Current status:
- done with a concrete node rather than a formal interface
- remaining work is to keep Robin stable while the surrounding graph matures

### Step 4. Formalize the Source Mixer

Goal:
- make the source mixer a real graph stage, not just controller state

Deliverables:
- one gain stage per source
- mute/enable behavior per source
- `Robin` active in audio
- `Decor` and `Pieces` ready to plug in later

Do not do yet:
- no solo logic unless it becomes necessary

### Step 5. Formalize the Output Mixer

Goal:
- make output correction a real graph stage

Current reality:
- output trim, delay, and EQ are already active

Next deliverables:
- keep trim, delay, and EQ
- refine the output mixer module structure if needed

Recommended order:
1. keep trim, delay, and EQ as-is
2. move the stage into reusable graph modules when the controller becomes too crowded

Reason:
- the output processor model is now proven enough to support further DSP work

Current status:
- trim, delay, and EQ live in `OutputMixerNode`
- the remaining work is mostly cleanup and verification, not basic structure

### Step 6. Finish Robin as the reference source

Goal:
- use `Robin` to prove the source architecture before adding more devices

Deliverables:
- stable note/event handling
- stable voice allocation
- routing algorithms:
  - first output
  - round robin
  - all outputs
  - custom
- linked source-wide timbre controls

Optional later additions:
- better envelopes
- filters
- more modulation routing

### Step 7. Implement Decor

Goal:
- add the first truly multichannel-native source

Rules:
- voice count matches output count
- one voice maps to one output
- direct speaker lock

Deliverables:
- `DecorSource`
- one voice per output
- shared control model
- decorrelated output behavior later

Do not do yet:
- do not over-design the synthesis model before the routing model is proven

### Step 8. Implement Pieces

Goal:
- add a granular source after the source/graph model is already stable

Deliverables:
- `PiecesSource`
- playable grain triggering
- algorithmic voice/output routing

Reason to delay:
- granular plus routing is more complex than `Robin` and `Decor`

### Step 9. Build the FX insert architecture

Goal:
- make FX a real per-output insert layer

Rules:
- one effect instance per output
- controls remain linked globally
- each output either routes through the insert or bypasses it
- no parallel wet/dry path for now

Deliverables:
- processor rack structure
- per-output route switches
- linked global parameter state

### Step 10. Implement Saturator

Goal:
- first real output processor

Deliverables:
- algorithm selection later
- input level
- output level
- linked in/out option

Why first:
- easier to validate than chorus

### Step 11. Implement Chorus

Goal:
- first modulation-heavy multichannel processor

Deliverables:
- depth
- speed
- phase spread across outputs

Reason:
- this validates output-aware modulation in the processor layer

### Step 12. Design sidechain before implementing it

Goal:
- avoid building the wrong control model

Questions to answer first:
- what is the detector input
- does it listen to one source or a bus
- is it per output or global
- does it need latency handling

Do not implement until these are answered.

### Step 13. Expand control and persistence

Goal:
- make the framework usable beyond one session

Deliverables:
- stable OSC map
- stable MIDI behavior
- preset save/load
- possibly scene/state recall

### Step 14. Add testing and profiling

Goal:
- keep the framework stable as the DSP grows

Add:
- state serialization tests
- parameter path tests
- graph routing tests
- output mixer behavior tests
- performance profiling for higher channel counts

## Recommended working order from here

If continuing immediately, do the next work in this order:

1. Make `Source Mixer` a real graph stage instead of per-source controller wiring.
2. Add routing tests for `LiveGraph`, source enable/disable, and `OutputMixerNode`.
3. Keep stabilizing `Robin` as the reference source.
4. Add `DecorSource`.
5. Add `PiecesSource`.
6. Add FX rack structure.
7. Implement saturator.
8. Implement chorus.
9. Design sidechain.

## Rules to keep during implementation

- Do not add new DSP without first deciding where it belongs in the graph.
- Do not flatten new state back into one giant synth blob.
- Do not split into multiple repos yet.
- Do not build `Pieces` before `Robin` and `Decor` prove the source model.
- Do not build sidechain before the routing/control design is clear.

## Short version

The safe order is:

1. stabilize structure
2. build concrete graph modules
3. make `Robin` the first real source node
4. finish source mixer and output mixer stages
5. add `Decor`
6. add `Pieces`
7. add FX architecture
8. implement saturator
9. implement chorus
10. design and implement sidechain later
