# User Manual

## Overview

`Synthesizer` is a multichannel instrument host built for speaker-aware sound design.

The app combines:
- a native audio engine
- a controller/state layer
- a bundled web UI

The main instrument right now is `Robin`, a multivoice synth designed around a simple idea:

1. build one strong master sound
2. let most voices follow it
3. unlink only the voices that need special behavior

This makes it easier to build wide, alive, multichannel sounds without programming every voice from scratch.

## Who This Manual Is For

This manual is for the end user of the app:
- someone running the macOS app
- someone testing the audio engine
- someone designing sounds in Robin
- someone routing sources to multiple outputs

It is not mainly a developer setup guide. For development workflow, see:
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [GITHUB.md](GITHUB.md)
- [ARCHITECTURE.md](ARCHITECTURE.md)

## Requirements

Current app target:
- macOS

To build and run:
- Xcode Command Line Tools
- CMake 3.21+

Build and launch:

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
./scripts/run-app.sh
```

If the app is already open and you rebuilt it, relaunch it so the updated bundled UI is loaded.

## First Run

When you first launch the app, the most useful path is:

1. open `Program`
2. confirm the audio engine is running
3. open `MIDI`
4. connect a MIDI source, or use an `IAC Driver` source if you do not have a hardware keyboard
5. open `Source Mixer`
6. make sure `Robin` is enabled
7. open `Robin`
8. play notes and start shaping the master voice

If no notes are reaching the synth:
- confirm the MIDI source is connected in the `MIDI` page
- confirm its route to `Robin` is enabled
- confirm `Robin` is enabled in the `Source Mixer`

## Main Signal Flow

Current signal flow:

`Audio Engine -> Sources -> dry/fx split -> FX Rack -> dry + fx sum -> Output Mixer -> hardware outputs`

This means:
- sources do not automatically go through FX
- the `Source Mixer` decides whether each source feeds the dry path or the FX path
- every output has both a dry path and an FX-chain path available
- the `Output Mixer` is the last processing stage before hardware output

## Main Pages

## Program

The `Program` page is the engine-level page.

Use it for:
- starting and stopping the audio engine
- checking output configuration
- checking general runtime state

This is not the main sound-design page. Think of it as the system control page.

## MIDI

The `MIDI` page is where external note input is connected and routed.

Use it to:
- connect or disconnect MIDI sources
- route MIDI input into `Robin`
- route MIDI input into `Test`

Useful fallback on macOS:
- enable `IAC Driver` in `Audio MIDI Setup`
- send MIDI from another app into the IAC port
- connect that source in the `MIDI` page

Current rule:
- MIDI routing is source-based
- each MIDI source can be routed to one or more synth sources

## Source Mixer

The `Source Mixer` controls whole sources, not individual voices.

Each source strip controls:
- `Enabled`
- `Level`
- `Signal Path`

### Enabled

Turns the source on or off at the mixer level.

### Level

Sets the source’s overall contribution before final output mixing.

For Robin, this is usually the right place to adjust overall loudness.

### Signal Path

Selects whether the source feeds:
- `Dry`
- `FX`

Meaning:
- `Dry`: bypasses the FX bus and goes straight to the dry output path
- `FX`: enters the FX chain, then gets summed back at the output stage

This is one of the key routing decisions in the app.

## Output Mixer

The `Output Mixer` is the per-speaker correction and balancing stage.

Each output strip currently supports:
- level
- delay
- simple fixed-band EQ

Use it for:
- balancing speaker levels
- compensating distance differences with delay
- broad tonal correction

It is not meant to replace source sound design. It is the system-facing output stage.

## Robin

`Robin` is the main instrument.

It is a multivoice spatial synth with:
- multiple voices
- multiple oscillators per voice
- master-first programming
- optional local per-voice overrides
- routing behavior across outputs

### Core Idea

Robin is designed in two phases:

1. dial in one strong master sound
2. add variation only where needed

That is why the master voice matters so much.

The linked workflow is there to prevent you from rebuilding a patch voice by voice.

## Robin Layout

Robin currently has two main editing layers:

1. a `Master` editor
2. a `Voices` overview plus one selected local voice editor

### Master

The master section is the template followed by linked voices.

This is where you should start almost every patch.

Current master sections include:
- layout
- pitch offsets
- oscillator bank
- VCF
- ENV VCF
- AMP / ENV VCA

### Voices Overview

The voice overview is the compact management layer.

Each voice has:
- `Enabled`
- `Linked to Master`

When a voice is linked:
- it follows the master template
- local controls are hidden

When a voice is unlinked:
- it can open into the selected local voice editor
- it keeps its own local settings

Current design rule:
- relinking a voice should not wipe its local state
- if you want to copy master settings into it again, use `Reset to Master State`

### Selected Voice Editor

Only one unlinked voice is expanded at a time.

This keeps the overview readable while still giving full local edit access when needed.

The local editor can include:
- local root frequency
- local gain
- local oscillator bank
- local VCF
- local ENV VCF
- local AMP envelope

## How To Program Robin

Recommended workflow:

1. start in the master editor
2. build the oscillator blend
3. shape the filter and envelopes
4. confirm the basic sound while multiple voices are still linked
5. only then unlink selected voices
6. use local edits to create accents, spread, rhythmic contrast, or spatial variation

This is the intended Robin mindset:
- master first
- local exceptions second

## Robin Master Sections

## Master Pitch

Robin currently uses:
- `Transpose`
- `Fine Tune`

These are master offsets, not a separate playable root-note control in the GUI.

Use them to:
- shift the whole instrument
- tighten or loosen tuning slightly

## Oscillator Bank

The oscillator bank is the main tone source.

Each oscillator can define:
- enabled state
- waveform
- level
- relative or absolute frequency behavior

Practical use:
- start with one oscillator enabled
- add oscillators for thickness, detune, or layered tone
- use relative frequency for musically related stacks
- use absolute frequency carefully when you want more special behavior

## VCF

The `VCF` is the main filter stage.

Current controls:
- cutoff
- resonance

Use it to:
- darken or open the sound
- emphasize the cutoff region
- shape motion together with `ENV VCF`

## ENV VCF

The filter envelope shapes the movement of the filter over time.

Current controls:
- attack
- decay
- sustain
- release
- amount

Use it for:
- plucks
- swells
- percussive filter hits
- evolving filter motion

## AMP

The amplitude envelope shapes the loudness contour of the voice.

Current controls:
- attack
- decay
- sustain
- release

Use it for:
- sharp plucks
- pads
- swelling entrances
- long release tails

## Linked and Unlinked Voices

This is the most important Robin behavior to understand.

### Linked to Master

A linked voice:
- uses the master settings
- stays compact in the overview
- follows master edits

Use linked voices when:
- you want consistent tone
- you want quick polyphony
- you are still shaping the main sound

### Unlinked

An unlinked voice:
- becomes locally editable
- stops following the master in the same way
- can diverge in oscillator, filter, envelope, and local pitch behavior

Use unlinked voices when:
- one voice should stand out
- you want rhythmic or timbral contrast
- you want custom local filtering or envelope motion
- you want speaker-specific or voice-specific behavior

### Reset to Master State

This explicitly copies the current master state back into the unlinked voice.

Use it when:
- you drifted too far locally
- you want to restart local editing from the current master patch

## Robin Routing

Robin is speaker-aware.

Current routing presets include:
- `Forward`
- `Backward`
- `Random`
- `Round Robin`
- `All Outputs`
- `Custom`

### What these mean

- `Forward`
  advance through outputs in ascending order
- `Backward`
  advance through outputs in descending order
- `Random`
  choose outputs unpredictably
- `Round Robin`
  distribute triggers across outputs in a rotating pool
- `All Outputs`
  send the voice to every output
- `Custom`
  use manually edited output assignments

`All Outputs` is useful when:
- you want a very wide distributed sound
- the voice should exist everywhere at once

`Custom` is useful when:
- you want exact speaker assignment
- you are designing very intentional output behavior

## Robin LFO

Robin already has an LFO system.

Current LFO ideas supported by the UI/state model include:
- waveform
- depth
- phase spread
- polarity flip
- linked-to-clock behavior
- fixed-frequency behavior
- unlinked output behavior

Important note:
- there is still dedicated LFO work ahead
- some LFO presentation areas are still scaffolded for future expansion

## TestSynth

`TestSynth` is the simpler debug and verification source.

It is useful when you want to test:
- MIDI routing
- output routing
- speaker wiring
- output balance
- simple synth behavior without the full Robin complexity

Use `TestSynth` when:
- you are troubleshooting a routing problem
- you want to isolate whether the issue is Robin-specific
- you want a simpler audio source while checking levels and outputs

## FX

The FX section currently focuses on multichannel output-aware processing.

### Chorus

`Chorus` is the most developed live effect right now.

Current controls include:
- enable
- depth
- speed
- phase spread

Phase spread is distributed across outputs rather than being treated like a simple stereo parameter.

### Saturator

Current controls:
- enable
- input level
- output level

This is still more scaffold-like than finalized.

### Sidechain

Sidechain exists in the UI/controller model, but still needs a fuller design pass.

## Double-Click Reset

Many UI controls support reset by double-clicking.

This is useful when:
- you want to return to the default value quickly
- you overshot during tweaking
- you want to compare against the base/default setting

## Troubleshooting

## No Sound

Check:
- the audio engine is running
- the correct output configuration is selected
- the source is enabled in `Source Mixer`
- source level is up
- output level is up
- the MIDI source is connected
- MIDI routing to the source is enabled

## MIDI Is Not Reaching Robin

Check:
- the MIDI source is connected on the `MIDI` page
- the route to `Robin` is enabled
- `Robin` is enabled in the `Source Mixer`

If no keyboard is available:
- use macOS `IAC Driver`
- send MIDI from another app into that virtual port

## App Crash Or Strange Runtime Behavior

Run:

```bash
./scripts/run-app.sh --debug-crash
```

Logs are written under:

```bash
~/Library/Logs/Synthesizer/
```

Useful files:
- `synth_*.log`
- `crash_*.log`

## Downloaded App Says It Is Damaged

Official tagged `main` releases are packaged for download, but currently unsigned.

If macOS says the app is "damaged", you are most likely hitting Gatekeeper/quarantine rather than an actually broken app bundle.

Try one of these:
- right-click `Synthesizer.app` and choose `Open`
- remove quarantine manually:

```bash
xattr -dr com.apple.quarantine /path/to/Synthesizer.app
open /path/to/Synthesizer.app
```

For the full release explanation, see [RELEASING.md](RELEASING.md).

## Pops While Adjusting Sound

A lot of the live control path has already been hardened, but if you still hear a pop:
- note which control caused it
- note whether it happened during drag or on release
- note whether a note was playing
- note whether the source was Robin, TestSynth, or FX

That is the most useful repro information.

## Current Limitations

This app is usable, but it is still under active development.

Current limitations:
- macOS-first app target
- `Decor` and `Pieces` are still placeholders
- some modulation areas are scaffolded rather than finished
- not every concept in Robin is fully simplified yet
- architecture cleanup is still ongoing behind the scenes

## Recommended Mindset

The best way to use the app right now is:

- treat `Robin` as the main instrument
- use `TestSynth` to verify routing and system behavior
- use `Source Mixer` for source-level balance and dry/FX routing
- use `Output Mixer` for speaker correction
- use linked voices by default
- unlink only the voices that truly need special behavior

That workflow matches the app’s current strengths.
