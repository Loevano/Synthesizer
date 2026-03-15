# Architecture (Current Scaffold)

The project now has a clearer distinction between:

1. the live audio path that actually renders sound
2. the scaffold model that describes how the full multichannel framework should grow

## Live audio path today

`Audio Engine -> LiveGraph sources -> Source Mixer gains -> FX rack chorus inserts -> Output Mixer delay/EQ/trim -> hardware outputs`

Current reality:
- the engine is CoreAudio-backed
- `Robin` and `Test` are the current sources rendering audio
- source-level gain for `Robin` and `Test` is active
- per-output trim, delay, and EQ are active
- chorus is live as a per-output insert processor
- saturator and sidechain are still scaffold state only

## Scaffold model today

`Audio Engine -> Source Mixer -> Output Mixer -> Sources -> FX -> outputs`

This is the product model exposed by the controller and web UI.

## Internal graph today

Inside the controller, the live path is now built from reusable concrete modules:

- `LiveGraph`
- `RobinSourceNode`
- `TestSourceNode`
- `FxRackNode`
- `OutputMixerNode`

Within that graph, the live path is routed as explicit stages:

- source stage order:
  - `Robin`
  - `Test`
  - `Decor`
  - `Pieces`
- output processor stage order:
  - `FX Rack`
  - `Output Mixer`

This is still a simple concrete implementation, not a generic plugin framework. The main goal is to stop
hardcoding the render path as one special-case synth plus extra conditionals while avoiding unnecessary inheritance.

## Layer responsibilities

- `Audio Engine`
  - CoreAudio lifecycle
  - output device selection
  - sample rate, buffer size, output count
  - MIDI and OSC host state
  - MIDI source selection and per-device synth routing
  - native bridge surface
- `Source Mixer`
  - device-level VCA control
  - performance-oriented, not per-output correction
  - `Robin` and `Test` are live here today
- `Output Mixer`
  - per-output trim
  - per-output delay
  - per-output fixed-band EQ
- `Sources`
  - `Test`: implemented, simplified, manual output routing for debugging, note-triggered reusable envelope
  - `Robin`: implemented, playable, routed across outputs, with one shared envelope control set applied to per-voice envelopes
  - `Decor`: scaffold for one-voice-per-output immersive playback
  - `Pieces`: scaffold for granular algorithmic routing
- `FX`
  - modeled as per-output inserts
  - global controls stay linked across instances
  - per-output routing decides whether a given output enters the chain

## Controller state shape

The native bridge now returns state grouped as:

- `engine`
- `graph`
- `sourceMixer`
- `outputMixer`
- `sources`
- `processors`

This is intentionally better aligned with the target product than the old flat synth state.

## Why this scaffold matters

The important design problem in this project is not only DSP. It is where features belong when output count
changes the instrument design. By scaffolding `Decor`, `Pieces`, `Output Mixer`, and `FX` now, new work has a
stable landing zone before deeper DSP is written.

## Next backend step

The next structural step should be to make `Source Mixer` its own explicit graph stage and add a few routing tests
around `LiveGraph`, source enable/disable, and `Output Mixer` behavior before adding more synths.
