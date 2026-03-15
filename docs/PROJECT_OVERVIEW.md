# Project Overview

This document is the high-level description of the project.

It answers one question:

**What is this project, and what does each part do?**

## What the project is

This project is a **multichannel audio instrument framework**.

It is not meant to be a normal stereo synthesizer. The main idea is that the **number of output channels changes how the instrument behaves**. Sound sources, routing, modulation, and effects are all designed around a speaker array instead of a left/right output pair.

The framework is being built in stages:

- first the structure
- then the DSP details

That means some parts already process audio, while other parts are scaffolded so they have a clear place in the system before their DSP is implemented.

## Main concept

The system is organized like this:

`Audio Engine -> Source Mixer -> Output Mixer -> Sources -> FX -> outputs`

This means:

- the engine owns CoreAudio and buffer processing
- the source mixer controls whole devices
- the output mixer fine-tunes each speaker output
- the sources generate sound
- the FX process sound per output

## Top-level parts

### 1. Audio Engine

The audio engine is the system layer that talks to CoreAudio and drives rendering.

Its job is to:

- start and stop audio
- manage sample rate
- manage buffer size
- manage output channel count
- ask the DSP graph to fill the output buffer
- expose system state to the UI
- host MIDI and OSC input
- expose selectable MIDI input devices to the UI

In practical terms, this is the top layer of the whole app. Everything below it depends on the engine configuration.

## 2. Source Mixer

The source mixer is the **performance mixer**.

Its job is to control the level of complete sound sources:

- `Test`
- `Robin`
- `Decor`
- `Pieces`

This mixer is not about balancing individual outputs. It is about balancing instruments during performance.

Example:

- bring `Test` up to check channel mapping
- turn `Robin` down
- bring `Decor` up
- mute `Pieces`

This happens before output correction.

## 3. Output Mixer

The output mixer is the **system engineer mixer**.

Its job is to fine-tune each output channel independently.

Per output, it is intended to control:

- level
- delay
- EQ

This is where the system is adjusted for the actual speaker setup.

Current live status:

- level is active
- delay is active
- fixed-band EQ is active

Example:

- lower output 7 because that speaker is too loud
- delay output 3 to improve alignment
- cut some high end on one speaker

This layer is not about musical performance. It is about system correction and tuning.

## 4. Sound Sources

The framework is designed to host multiple kinds of playable sound sources.

All synth-style sources are expected to share some common ideas:

- per-voice oscillators
- envelopes
- filters
- linked controls for modulation and automation

The sources mainly differ in how they allocate voices and route them across outputs.

### Test

`Test` is the simplified debug synth.

Its role is:

- provide a minimal source for testing the engine
- provide a clear signal for checking output routing
- provide a simple speaker-check tone that still uses the shared oscillator resources

Its defining idea is **debugging without source complexity**.

Typical use cases:

- confirm output numbering
- verify speaker wiring
- check output mixer trims
- test MIDI pitch updates without the full Robin voice model

Test is intentionally much simpler than Robin. It is a single-oscillator source with manual output routing and a small control surface.
It now also has a reusable envelope so MIDI note-on and note-off can be tested with a proper gate shape.

### Robin

`Robin` is the current playable synth.

Its role is:

- allocate a user-defined number of voices
- let those voices move across the output system
- keep the oscillator stack linked across voices
- keep one shared ADSR contour while each voice runs its own envelope instance

Its defining idea is **algorithmic voice routing**.

Typical routing behaviors:

- first output
- round robin
- all outputs
- custom routing

Robin is the current reference source. It is the first full source the rest of the framework is being designed around.

### Decor

`Decor` is the immersive source.

Its role is:

- create a large decorrelated sound field
- use one voice per output
- keep each voice locked to one speaker

Its defining idea is that voice count follows output count.

Example:

- 8 outputs -> 8 voices
- each voice feeds one output directly

Decor is not fully implemented yet, but its role in the system is already defined.

### Pieces

`Pieces` is the future granular source.

Its role is:

- generate playable granular sound
- route grains or voices algorithmically across the outputs

Its defining idea is to combine granular playback with spatial routing behavior.

Pieces is scaffolded now and should come after the source and graph architecture is more mature.

## 5. Sound Processors

The framework treats FX differently from normal stereo effects.

Each effect is intended to be **discrete per output**.

That means:

- if there are 8 outputs, there are 8 effect instances
- controls stay linked across those instances
- each output can route through its own insert path

This keeps the behavior multichannel-native instead of pretending the whole system is stereo.

### FX routing model

Each output has:

- a direct path
- an insert route through the FX chain

For now, the project assumes:

- a signal either goes through the effect or not
- no parallel wet/dry path for now

The reason is latency complexity. Parallel wet/dry handling is not part of the current scaffold.

### Saturator

The saturator is planned as the first real output processor.

Its role is:

- add color or distortion per output
- provide algorithm selection later
- provide input and output level control
- optionally link those levels

This is expected to be easier to implement and validate than chorus.

### Chorus

The chorus is now the first live modulation-heavy processor.

Its role is:

- modulate each output independently
- keep global controls linked
- spread modulation phase across outputs
- validate the per-output insert FX path before more processors are added

Its current live controls are:

- enable
- depth
- speed
- phase spread

### Sidechain

Sidechain is intentionally undefined for now.

It is a separate processor from chorus and should stay represented that way in the scaffold.

It needs a design pass before implementation because several questions are still open:

- what is the detector source
- is it global or per output
- how should routing work
- what latency behavior is acceptable

## Control layer

The project also has a control layer that sits between the engine and the UI.

Its role is to expose structured state and parameter paths.

The current state model is grouped as:

- `engine`
- `graph`
- `sourceMixer`
- `outputMixer`
- `sources`
- `processors`

This is important because the project should not collapse back into one flat synth state.

## Live graph modules

The current live DSP path is now built from a small set of reusable concrete modules instead of being hardcoded
directly inside `SynthController`.

Current live modules:

- `LiveGraph`
- `RobinSourceNode`
- `TestSourceNode`
- `OutputMixerNode`

This is intentionally simple. The project now has graph structure without committing to a large inheritance tree.

## UI layer

The app currently uses a native macOS shell with a bundled web UI.

The UI exists to make the framework readable while it is being built.

Current pages:

- `Program`
- `Source Mixer`
- `Output Mixer`
- `Robin`
- `Test`
- `Decor`
- `Pieces`
- `FX`
- `LFO`

These pages are not only for control. They also act as visible scaffolding so every major subsystem has a place in the interface.

## MIDI and OSC

The system is intended to be playable and controllable from outside the app.

Current external control responsibilities:

- MIDI note input
- OSC parameter control

This makes the framework usable as an instrument, not only as a local prototype.

## Current live vs scaffold status

### Live now

- Audio engine / CoreAudio host
- `LiveGraph`
- `RobinSourceNode`
- `TestSourceNode`
- `FxRackNode`
- `OutputMixerNode`
- `Test`
- `Robin`
- source-level gain for `Robin` and `Test`
- per-output chorus insert processing
- output trim
- output delay
- output EQ
- global LFO for Robin
- MIDI input
- OSC parameter updates

### Scaffolded now

- `Decor`
- `Pieces`
- FX routing
- saturator state
- sidechain placeholder

This means the architecture is ahead of the DSP in several areas on purpose.

## Why the project is being built this way

The main risk in this project is not only writing oscillators or filters.

The real risk is building the wrong structure for:

- multichannel routing
- source roles
- output correction
- per-output FX
- linked control behavior

That is why the project is being scaffolded first.

The goal is:

1. define where each feature belongs
2. keep the state model stable
3. keep the UI readable
4. add DSP only after the structure is clear

## In one sentence

This project is a multichannel audio framework where sound sources, routing, mixing, and effects are all designed around the number of output channels, with `Robin` as the main live source, `Test` as the simplified debug source, and the rest of the system scaffolded into place for later DSP implementation.
