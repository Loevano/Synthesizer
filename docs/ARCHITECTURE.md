# Architecture (Current MVP)

## Signal flow
`Synth -> AudioDriver callback -> speakers`

## Runtime pieces
- `Logger`: writes to console and `logs/`
- `Synth`: top-level instrument object
- `Oscillator`: first DSP building block inside the synth
- `IAudioDriver` + `CoreAudioDriver`: audio device output
- `main.cpp`: wiring and process lifecycle

## Deliberate simplification
Old module-host, MIDI, OSC, and extra wrapper code has been removed from the active project so the learning path stays focused on the audio callback and DSP basics.

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
