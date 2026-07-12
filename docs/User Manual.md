# User Manual

## Overview

`Synthesizer` is a macOS-first multichannel instrument host for speaker-aware sound design.

The app combines:

- a native C++ audio engine
- a controller/state layer
- a bundled web UI inside a macOS app

The main instrument is `Robin`, a multivoice spatial synth built around one workflow:

1. build one strong master voice
2. keep most voices linked to that master
3. unlink only the voices that need local behavior

For contributor workflow, use [CONTRIBUTING.md](CONTRIBUTING.md), [GIT_RULES.md](GIT_RULES.md), and [GITHUB.md](GITHUB.md).

## Requirements

Current target:

- macOS
- Xcode Command Line Tools
- CMake 3.21+

Build and launch:

```bash
git clone <your-repo-url>
cd Synthesizer
./scripts/build.sh
./scripts/run-app.sh
```

If the app is already open and you rebuilt it, relaunch it so the bundled UI reloads.

## First Run

1. Open `Program`.
2. Confirm audio is running and the intended output device is selected.
3. Open `MIDI`.
4. Connect a MIDI source, or use macOS `IAC Driver` if you do not have a hardware keyboard.
5. Open `Sources`.
6. Confirm `Robin` is enabled and routed to `Dry` or `FX`.
7. Open `Robin`.
8. Play notes and shape the master voice.

If no notes reach the synth, check:

- MIDI source connection on the `MIDI` page
- MIDI route to `Robin`
- `Robin` enabled in `Sources`
- source and output levels

## Signal Flow

Current live signal flow:

`Audio Engine -> LiveGraph sources -> dry/fx split -> FX Rack -> dry + fx sum -> Output Mixer -> hardware outputs`

Important routing rules:

- sources choose `Dry` or `FX` in `Sources`
- the FX bus is processed, then summed back with the dry bus
- `Outputs` is the final per-speaker correction stage
- only implemented sources produce audio

## Program

The `Program` page is the system page.

It shows:

- audio backend
- output device
- audio running state
- sample rate
- output channel count
- buffer size
- MIDI and OSC status
- last MIDI note
- debug nested state when debug UI is enabled

Changing output device, output count, or buffer size restarts audio.

## Patch Menu

The `Patch Menu` on the `Program` page manages user patch state.

Current actions:

- `New Default`
- `Load`
- `Save`
- `Save As`
- `Set Default`

Patch files store user patch state, not continuous realtime engine internals.

Storage location:

- development checkout with `./Patches`: repo-local `Patches/`
- packaged app or no project patch directory: `~/Library/Application Support/Synthesizer/Patches/`

Default patch file:

- `default-patch.json`

The dirty badge shows whether the current patch has unsaved changes.

## MIDI

The `MIDI` page lists available MIDI sources and per-source routing.

Use it to:

- connect or disconnect MIDI sources
- route a source to `Robin`
- route a source to `Test`

Current route defaults:

- `Robin`: enabled
- `Test`: enabled
- `Decor`: disabled
- `Pieces`: disabled

Useful macOS fallback:

1. open `Audio MIDI Setup`
2. enable `IAC Driver`
3. send MIDI from another app into the IAC port
4. connect that source in the app

## Sources

The `Sources` page is the source mixer.

Each source strip controls:

- enabled state
- source level
- route target: `Dry` or `FX`

Current source status:

- `Robin`: implemented and live
- `Test`: implemented and live
- `Decor`: visible scaffold
- `Pieces`: implemented sampler

Use `Sources` for whole-source performance balance and dry/FX routing. It is not the per-voice editor.

## Outputs

The `Outputs` page is the per-speaker correction stage.

Each output currently supports:

- level
- delay
- three-band EQ: low, mid, high

Use it for speaker balance, distance compensation, and broad tonal correction. Source sound design belongs in `Robin`, `Test`, and the FX path.

## Robin

`Robin` is the main playable source.

It supports:

- configurable voice count
- configurable oscillator slots per voice
- linked and local voice states
- master pitch offsets
- master oscillator bank
- master `VCF`, `VCF ENV`, and `VCA ENV`
- spread modulation slots and macro depth controls
- routing presets across outputs
- a separate Robin LFO page

### Master Voice

Start in the master voice. Linked voices follow this template.

Master sections:

- `Setup`: voice count, oscillator slots per voice, routing preset
- `Pitch`: transpose and fine tune
- `Macros`: quick depth controls for spread slots
- `OSC Bank`: oscillator enable, waveform, level, and frequency behavior
- `VCF`: cutoff and resonance
- `VCF ENV`: attack, decay, sustain, release, amount
- `VCA ENV`: attack, decay, sustain, release
- `Modulation`: detailed spread slots

### Voice Bank

Every Robin voice has:

- `Enabled`
- `Linked to Master`

Linked voices:

- follow the master voice template
- stay compact in the voice overview
- receive master edits and enabled spread modulation

Local voices:

- can open in the selected voice editor
- have their own oscillator bank, pitch/gain, filter, envelopes, and output targets
- stop following master settings for the locally edited sections

Only one local voice editor is expanded at a time.

### Reset to Master State

`Reset to Master State` copies the current master settings into a local voice.

Use it when:

- a local voice has drifted too far
- you want to restart local editing from the current master patch

Relinking a voice does not wipe local settings by itself.

### Routing Presets

Current routing presets:

- `Forward`: advance through outputs in ascending order
- `Backward`: advance through outputs in descending order
- `Random`: choose output targets randomly
- `Round Robin`: rotate through a shuffled output pool
- `All Outputs`: route a voice to every output
- `Custom`: use manually edited per-voice output assignments

Editing voice output targets switches Robin to `Custom`.

### Spread Modulation

Robin has six live spread slots.

Each slot can set:

- enabled state
- target
- algorithm
- depth
- start value
- end value
- random seed

Current algorithms:

- `Linear`
- `Random`
- `Alternating`

Current targets include filter, filter envelope, amp envelope, oscillator level, and oscillator detune parameters.

Spread modulation applies to active linked voices as offsets around the master template. The `Macros` section gives quick depth access for the same slots.

## LFO

The `LFO` page controls Robin's output-aware LFO.

Current controls:

- enable
- waveform: sine, triangle, saw down, saw up, random
- rate mode: Hz or sync
- tempo and multiplier in sync mode
- fixed frequency in Hz mode
- amount
- phase spread
- polarity flip
- unlinked outputs

The current LFO renders per-output modulation multipliers for Robin voices. Use it for output-aware amplitude movement and phase-spread motion across the speaker layout.

## Test

`Test` is a small debug source.

It supports:

- continuous tone
- MIDI response
- frequency
- gain
- waveform
- `VCA ENV`
- manual output targets

Use it to verify MIDI, routing, output wiring, and output mixer behavior without Robin complexity.

## FX

The `FX` page shows the current FX chain.

Current processor status:

- `Chorus`: live audio processor
- `Saturator`: UI/state scaffold only
- `Sidechain`: UI/state scaffold only

Chorus controls:

- enabled state
- depth
- speed
- phase spread

The chain rows can be reordered in the UI/state, but only implemented processors affect audio. At the moment, the live FX audio behavior is chorus.

## Decor

`Decor` is visible as a scaffold for a future immersive source.

Current intent:

- one voice per output
- speaker-locked behavior
- decorrelated spatial sound

It does not currently produce its own DSP output.

## Pieces

`Pieces` is the sampler source and the planned base for granulator mode.

Current controls:

- sample loading
- MIDI response
- root note, transpose, and fine tune
- gain
- playback mode: `Gate`, `One-shot`, or `Loop`
- reverse playback
- start and end, using either the controls or the draggable waveform markers
- waveform preview with the selected playback window
- `VCA ENV`
- manual output targets

It plays one decoded sample across MIDI pitches. Multisample zones, velocity sample ranges, loop editing with crossfade, and granulator mode are planned next.

## Double-Click Reset

Many controls can be reset by double-clicking. Use this to return a slider or control to its default value quickly.

## Debug And Logs

Crash-oriented launch:

```bash
./scripts/run-app.sh --debug-crash
```

Bridge and Robin tracing:

```bash
./scripts/run-app.sh --debug-bridge --debug-robin
```

Launch the app through `./scripts/run-app.sh` or `open /path/to/Synthesizer.app`.
Do not run `Synthesizer.app/Contents/MacOS/Synthesizer` directly from the terminal.

Logs are written under:

```bash
~/Library/Logs/Synthesizer/
```

Useful files:

- `synth_*.log`
- `crash_*.log`

## Troubleshooting

### No Sound

Check:

- audio is running
- the correct output device is selected
- the source is enabled in `Sources`
- source level is up
- output level is up
- MIDI source is connected
- MIDI route to the source is enabled
- the source is routed to the intended `Dry` or `FX` path

### MIDI Is Not Reaching Robin

Check:

- MIDI source connection on the `MIDI` page
- route to `Robin`
- `Robin` enabled in `Sources`
- `Respond to MIDI` only matters for `Test`, not Robin's normal MIDI route

### Downloaded App Says It Is Damaged

Current packaged releases are unsigned. If macOS says the app is damaged, it is usually Gatekeeper quarantine.

Try:

- right-click `Synthesizer.app` and choose `Open`
- or remove quarantine manually:

```bash
xattr -dr com.apple.quarantine /path/to/Synthesizer.app
open /path/to/Synthesizer.app
```

See [RELEASING.md](RELEASING.md) for release and signing details.

### Pops While Adjusting Sound

If you hear a pop during live control:

- note the exact control
- note whether it happened during drag or on release
- note whether notes were playing
- note whether MIDI or OSC was active
- note whether the source was Robin, Test, or FX

That repro detail is more useful than a broad "it popped" report.

## Current Limitations

- macOS-first app target
- `Decor` is a placeholder
- `Pieces` is currently a basic one-sample sampler
- `Saturator` and `Sidechain` have UI/state but no DSP yet
- modulation is useful now, but not a full modulation matrix
- automated coverage is growing but still focused

## Recommended Mindset

- treat `Robin` as the main instrument
- use `Test` for routing and system checks
- use `Sources` for source-level balance and dry/FX routing
- use `Outputs` for speaker correction
- keep most Robin voices linked
- use spread slots and LFO for broad movement
- unlink voices only when they need local behavior
