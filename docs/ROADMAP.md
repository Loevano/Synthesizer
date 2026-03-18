# Roadmap

This file separates:

- platform and architecture work
- synth-feature work

The goal is to keep infrastructure, DSP, and UI changes from collapsing into one undifferentiated list.

## Current baseline

Already in place:

- CoreAudio-backed host on macOS
- native macOS app shell with embedded `WKWebView`
- JS/native bridge:
  - `getState()`
  - `setParam(path, value)`
- grouped controller state:
  - `engine`
  - `graph`
  - `sourceMixer`
  - `outputMixer`
  - `sources`
  - `processors`
- reusable concrete graph modules:
  - `LiveGraph`
  - `RobinSourceNode`
  - `TestSourceNode`
  - `FxRackNode`
  - `OutputMixerNode`
- live sources:
  - `Robin`
  - `Test`
- source-level dry/fx routing
- per-output level, delay, and EQ
- live chorus
- CoreMIDI input
- OSC control surface

Still missing:

- deeper modulation system / modulation matrix
- `Decor` DSP
- `Pieces` DSP
- full saturator
- sidechain design
- broader automated regression coverage

## Platform roadmap

## 1. Stabilize regression coverage

Goal:

- make routing, allocation, and state changes safer to evolve

Scope:

- source enable/disable behavior
- dry/fx routing behavior
- output mixer processing behavior
- Robin allocator behavior and release-tail handling

Reason:

- the app is now complex enough that silent regressions are more expensive than the initial setup cost of tests

## 2. Preset and state persistence

Goal:

- make the controller state saveable and reloadable without depending on UI state

Scope:

- stable serialization format
- load/save flow
- decide which controls are persistent versus runtime-only

## 3. Better parameter-smoothing policy

Goal:

- apply consistent anti-zipper handling where live drags can create audible clicks

Scope:

- output-mixer level
- any remaining abrupt source/process gains
- future modulation-depth and routing-sensitive controls

## 4. Broaden contributor ergonomics

Goal:

- make the repo easier to understand and extend

Scope:

- keep docs current
- maintain contributor setup instructions
- keep helper scripts accurate

## Feature roadmap

## A. Finish Robin as the reference instrument

Goal:

- make Robin solid enough that it can carry the architecture while other sources are scaffolded

Scope:

- keep master-first voice design
- add section-level modulation / spread tools
- keep local voice override behavior predictable
- improve remaining edge cases around true voice stealing

## B. Expand modulation

Goal:

- move from one live LFO to a broader modulation system

Scope:

- section modulators
- voice spread algorithms
- routing modulation
- eventual matrix-like destination model

## C. Expand FX rack

Goal:

- make the FX path more than a chorus proof-of-concept

Scope:

- saturator implementation
- sidechain design
- decide how future FX should share common control/state patterns

## D. Implement Decor

Goal:

- add the first source whose voice count is tightly coupled to output count

Scope:

- one voice per output
- speaker-locked behavior
- immersive decorrelated playback design

## E. Implement Pieces

Goal:

- add a granular / algorithmic source after the graph and routing model are more stable

Scope:

- playable grain triggering
- spatial grain or voice distribution

## F. Improve oscillator quality

Goal:

- scale oscillator CPU cost and audio quality more gracefully as voice count rises

Scope:

- lookup-table or band-limited strategies where useful
- reduce aliasing on non-sine waveforms
- keep the oscillator model scalable for larger Robin patches

## Recommended order

1. Add routing / allocator regression coverage.
2. Keep Robin stable as the reference live source.
3. Add section-level modulation on top of Robin’s current model.
4. Expand the FX rack beyond chorus.
5. Implement `Decor`.
6. Implement `Pieces`.
7. Add preset/state persistence once the state shape settles further.


Yes, I think we should split the idea in two.

For Robin:

keep spread as a light, coherent variation layer on top of the master
spread across linked + enabled voices
use voice order, not output order
do not replace values, only offset or scale them
For Decor:

put the heavier “every output/voice is its own decorated variation” logic there
that is where stronger divergence, decorrelation, and one-voice-per-output behavior belongs
So my recommendation is:

Robin logic

A spread modulator only affects linked voices.
Unlinked voices ignore spread completely.
If linked voices are 1, 2, 5, 8, they become the spread set in that order.
The spread position is computed over that linked set only.
Master stays the baseline.
Effective value is:
additive params: master + offset
multiplicative/time params: master * ratio
That preserves your original intent:

dial in one master sound
add controlled diversity on top
still move the whole set with the master
What is being spread
For Robin, spread should mean:

a static per-voice distribution function
not a time LFO yet
recomputed when:
master value changes
spread settings change
voice link/enabled state changes
voice count changes
So first version is cheap CPU-wise.

How to control spread
I would not start with “any parameter can be modulated by anything”.
That becomes a matrix too early.

Better:

use a small number of spread slots
each slot targets one parameter from a curated list
So:

flexible enough to assign
not fully open-ended
easier to understand and implement
Good first target list

VCF Cutoff
VCF Resonance
ENV VCF Amount
ENV VCF Attack
ENV VCF Decay
ENV VCF Release
AMP Attack
AMP Decay
AMP Release
Osc Level
Osc Detune
I would avoid waveform spread for now.

Good first algorithms

Linear
Random
Alternating
maybe Center-Out
Best parameter model
Each slot has:

Enabled
Target
Algorithm
Start
End
Seed for random only
Then:

Linear: interpolate from start to end
Random: stable random value between start and end per linked voice
Alternating: alternate between start and end
Center-Out: distribute symmetrically from center
Important: start and end are not absolute values.
They are offsets or ratios on top of the master.

Examples:

cutoff: use musical units, not raw Hz
detune: cents
envelope times: ratio or percent, not raw milliseconds offset
resonance: small additive offset
level: dB or gain ratio
Robin vs Decor
I agree with your instinct:

Robin should stay “one coherent instrument with optional variation”
Decor should become “speaker-decorrelated distributed texture”
So:

light spread belongs in Robin
strong per-output individuality belongs in Decor
My concrete recommendation:

Implement Robin spread as static linked-voice offsets.
Use 2-4 assignable spread slots.
Limit targets to a curated list.
Leave output-index decorrelation for Decor.
That is the cleanest first version.

If you agree, I’d implement Robin stage 1 like this:

3 spread slots
targets from a curated list
algorithms: Linear, Random, Alternating
applies only to linked enabled voices
offsets/ratios on top of master, never absolute replacement