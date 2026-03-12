# Architecture (Current MVP)

## Signal flow
`SynthEngine -> AudioDriver callback -> speakers`

## Runtime pieces
- `Logger`: writes to console and `logs/`
- `SynthEngine`: oscillator + gain
- `IAudioDriver` + `CoreAudioDriver`: audio device output
- `main.cpp`: wiring and process lifecycle

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
