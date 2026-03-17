# What should it do?

This is the product/design note for the project. It mixes current decisions with near-future intent.

## Core goals

- It should work on a multichannel setup.
- It should be able to make big, speaker-aware sound.
- Routing, modulation, and FX should make sense on a speaker array, not just on stereo outputs.

## Main playback/routing ideas

The project should support these broad behaviors:

1. voice locked to an output
2. voice round-robined across outputs when triggered
3. voice distributed across outputs according to an algorithm

Current routing direction:

- every output has:
  - a dry path
  - an FX-chain path
- the Source Mixer decides whether a source feeds dry or FX

## Voice and oscillator capacity

- Amount of voices can be allocated.
- Amount of oscillators per voice can be allocated.
- Voices should start enabled by default so the available voice count behaves like immediate polyphony.

Current default direction:

- `8` voices is still a good conceptual default
- `6` oscillator slots per voice is the current working app default

## Voice model

Voices should:

- be togglable
- have a routing/allocation behavior across outputs
- support local variation without rebuilding the whole patch from scratch

Current Robin rule:

- every voice has:
  - `Enabled`
  - `Linked to Master`
- linked voices hide local controls and follow the master voice
- unlinked voices open the full local voice editor
- relinking should not wipe local state
- there should be an explicit `Reset to Master State` action instead

## Oscillator model

Oscillators should:

- support `Sine`, `Saw`, `Square`, `Triangle`, `Noise`
- have frequency and gain
- be togglable
- support relative frequency to the voice/root behavior
- eventually support modulation

DSP note:

- the oscillator path should remain scalable as voice count rises
- lookup-table or band-limited strategies are good candidates if aliasing or CPU becomes the bottleneck

## Modulation

Modulators should create interesting multichannel movement in a controlled, simple way.

Current direction:

- Robin already has one live LFO system
- future modulation should sit on top of the master voice model, not replace it

LFO ideas:

- phase spread over outputs
- polarity flip
- selectable waveform:
  - `sine`
  - `triangle`
  - `saw-down`
  - `saw-up`
  - `random`
- linked-to-clock mode with relative rate
- fixed-frequency mode
- unlinked output behavior to introduce polyrhythms

## Envelopes and filters

Current Robin section model:

- `VCF`: cutoff, resonance
- `ENV VCF`: attack, decay, sustain, release, amount
- `AMP` / `ENV VCA`: attack, decay, sustain, release
- oscillator bank per voice

Future direction:

- more modulation destinations
- possibly more global envelopes beyond the current working set

## FX

Because the project is designed around many outputs, FX should be multichannel-native.

Rules:

- effects are effectively discrete per output
- controls are globally linked by design
- source routing decides whether a source reaches the FX chain

Current processor direction:

- `Chorus`
  - live now
  - depth
  - speed
  - phase spread
- `Saturator`
  - input level
  - output level
  - algorithm variety later
- `Sidechain`
  - still needs a separate design pass

## MIDI and OSC

MIDI:

- should recognize notes from USB MIDI devices
- should route per MIDI source into the synths

OSC:

- should expose a controllable parameter map

## Framework model

Current product model:

`Audio Engine -> Source Mixer -> Output Mixer -> Sources -> FX -> outputs`

Meanings:

- `Audio Engine`
  - CoreAudio
  - sample rate
  - buffer size
  - output count
  - resource allocation
- `Source Mixer`
  - performance mixer for whole sources
- `Output Mixer`
  - system-correction mixer for level, delay, EQ per output

## Sources

- `Robin`
  - main playable synth
  - algorithmic voice allocation across outputs
  - master-first design with local voice overrides
- `Decor`
  - future immersive source
  - one voice per output
- `Pieces`
  - future granular / algorithmic source

## Robin

Robin should be easy to program in two phases:

1. get one strong sound quickly
2. add aliveness and spatial variation without rebuilding the patch for every voice

Current working design:

- start with master controls
- all voices follow the master by default
- keep most voices linked
- unlink only the voices that need local accents or contrast

Next Robin layer:

1. Section modulators
   - preset algorithms that add offsets on top of the master values
   - example: a linear ramp that gradually opens the filter from voice 1 to voice 12
2. Section unlink
   - unlink a section or voice when a few voices need special local behavior

Rules:

- algorithms should be modulators, not automators
- master moves should still move the whole linked set
- unlinking should stop that local section from following the master modulation layer

## Open design questions still worth solving

- how broad the future modulation system should be before it becomes a real matrix
- what the exact `Decor` synthesis model should be
- what the exact `Pieces` granular model should be
- how sidechain should work in a multichannel speaker-aware context
