What should it do?
- It should be able to play on a multichannel setup.
- You should be able to make 'big' sound
- 3 playing modes need to be easily configured -> 
1. voice locked to an output
2. voice round robin to an output when triggered
3. voice send to output based on how many notes held.

Amnount of voices can be allocated. (default = 8)
Amount of Oscs per voice can be allocated (default = 4, only first one enabled by default)

Voices:
- Need to have voice spread
- Have a root freq
- Can be toggled

Oscs:
- Sine, Saw, Square, Triangle, Noise.
- Have freq and gain.
- Can be toggled to have relative frequency to root freq of voice.
- Can be modulated.
- Need to be DSP efficient (maybe a lookup table?)



Modulators should modulate all outputs in an interesting way but controlled in a simple way.

LFO on all oscs.
Control over lfo:
- phase spread over outputs
- polarity flip
- type of lfo used (sine, triangle, saw, upw saw, random)
- have unlinked LFO's over all outputs to introduce polyrythms
- Should be able to link to a clock (and have relative speed), toggle to have fixed frequency. 

3 Different Global Envelopes:
- Should be able to be triggered on input (note or midi or anything)
- Should be able to route to LFO's.
- Should be able to route to filters.

FX:
Because we are designing on a lot of channels, it only makes sense to have multi FX based on the amount of outputs.

- Saturation. Have the option for multiple sat or dist algorithms. With a gain before and gain after. Also make the option to have them linked (the gains).
- Chorus. Modulating each output. Control over the depth (how far i modulates) and the speed. Also a control for 'phase spread'. This means that if you have a modulator at 5 hz, the phase gets distributed over each output.
- Sidechain (dont know how to implement that yet)


MIDI and OSC
MIDI:
Should be able to play notes from midi USB device and have them recognized and played.
OSC:
There should be an osc map for all the params.