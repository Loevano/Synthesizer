# Project Overview

This document answers one question:

**What is this project, and what does each major part do?**

## What the project is

This project is a multichannel instrument framework, not a normal stereo synth.

The important idea is that output count changes the instrument design:

- voices can be allocated across a speaker array
- sources can be routed to a dry path or an FX path
- FX are modeled as per-output processors
- output correction happens per speaker, not per stereo pair

The current reference source is `Robin`, a multivoice spatial synth built around one shared master voice template plus local per-voice overrides.

## Main concept

Two views of the app are useful:

Product model:

`Audio Engine -> Source Mixer -> FX -> Output Mixer -> outputs`

Current live render path:

`Audio driver -> SynthHost -> LiveGraph sources -> dry/fx split -> FxRackNode -> dry + fx sum -> OutputMixerNode -> hardware outputs`

## Top-level parts

## 1. App Host

`SynthHost` is the system layer that talks to CoreAudio and drives rendering.

Its job is to:

- start and stop audio
- manage sample rate
- manage buffer size
- manage output channel count
- select the output device
- ask the graph to fill the output buffer
- host MIDI and OSC input
- expose state to the UI
- mirror snapshot state and render state

## 2. Source Mixer

The source mixer is the performance-facing mixer.

Its job is to control whole sources:

- enable / disable the source
- set source level
- choose whether the source feeds the dry path or the FX path

Current sources shown there:

- `Robin`
- `Test`
- `Decor`
- `Pieces`

`Robin` and `Test` are live today. `Decor` and `Pieces` are scaffolded.

## 3. Output Mixer

The output mixer is the system-tuning layer.

Its job is to fine-tune each hardware output independently.

Current live controls per output:

- level
- delay
- fixed-band EQ

This layer is for speaker correction and alignment, not source design.

## 4. Sound Sources

## TestSynth

`TestSynth` is the simplest playable source.

It exists to:

- verify speaker routing
- verify MIDI note handling
- verify output mixer behavior
- debug the host without Robin complexity

It is intentionally small: one oscillator, one `VCA ENV`, manual output routing.

## Robin

`Robin` is the current main instrument.

Its role is to:

- allocate a user-defined number of voices
- distribute those voices across outputs
- let most voices follow one master timbre
- allow selected voices to diverge locally

Current Robin model:

- all configured voices start enabled
- every voice has:
  - `Enabled`
  - `Linked to Master`
- linked voices follow the master voice template
- unlinked voices open a full local editor
- master pitch is `Transpose` plus `Fine Tune`
- master and local sections currently include:
  - oscillator bank
  - `VCF`
  - `VCF ENV`
  - `VCA ENV`

Routing presets currently include:

- `Forward`
- `Backward`
- `Random`
- `Round Robin`
- `All Outputs`
- `Custom`

## Decor

`Decor` is the future immersive source.

Its intended role is:

- one voice per output
- speaker-locked playback
- big decorrelated spatial sound

It is still scaffolded.

## Pieces

`Pieces` is the future granular source.

Its intended role is:

- granular or algorithmic playback
- voice or grain movement across the outputs

It is still scaffolded.

## 5. Sound Processors

FX are modeled as multichannel-native processors, not stereo afterthoughts.

Current routing rule:

- every output has a dry path
- every output also has an FX-chain path
- sources choose in the Source Mixer whether they feed dry or FX
- the FX bus is processed, then summed back with the dry bus

Current processor status:

- `Chorus`: live
- `Saturator`: scaffolded
- `Sidechain`: scaffolded

At the DSP layer, output effects now share a small `Effects` base so future processors follow one lifecycle.

## 6. External control

Current control surfaces:

- CoreMIDI input on macOS
- OSC server on UDP port `9000`
- native bridge between the macOS app shell and the web UI

## Current implementation shape

The backend exposes state grouped as:

- `engine`
- `graph`
- `sourceMixer`
- `outputMixer`
- `sources`
- `processors`

That structure is intentional. It matches the product model more closely than the older flat synth state.

The code split is also clearer now:

- `synth::app::Synth` is the shared app-level source base
- `audio::PolySynth` is the concrete polyphonic engine under Robin
- `SourceState.hpp` holds small reusable controller-side structs
- `RealtimeCommands.hpp` is the shared command contract between UI-facing state and render-side state

## Why this matters

The project is not just "a synth with more channels". The multichannel model changes:

- what a voice means
- where routing belongs
- how modulation should spread
- where FX live in the signal path

That is why the scaffolding, state shape, and graph model are treated as first-class design work.
