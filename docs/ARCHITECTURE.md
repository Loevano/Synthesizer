# Architecture (Current MVP)

## Signal flow
`Synth -> Voice -> Oscillator -> AudioDriver callback -> speakers`

## Runtime pieces
- `Logger`: writes to console and `logs/`
- `Synth`: top-level instrument object with a voice pool
- `Voice`: one playable lane with its own oscillator slots
- `Oscillator`: first DSP building block inside each voice
- `IAudioDriver` + `CoreAudioDriver`: audio device output
- `main.cpp`: wiring and process lifecycle

## Deliberate simplification
Old module-host, MIDI, OSC, and extra wrapper code has been removed from the active project so the learning path stays focused on the audio callback and DSP basics.

## Defaults
- Voice capacity defaults to `16`
- Oscillator slots per voice default to `6`
- Only the first voice and first oscillator slot are active by default

## Why this is minimal
This layout keeps the first learning loop small:
- edit one DSP class
- rebuild
- run
- hear result

## Next step candidates
- ADSR envelope
- MIDI note input
- Simple low-pass filter
- Polyphony
