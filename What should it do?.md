What should it do?
- It should be able to play on a multichannel setup.
- You should be able to make 'big' sound
- 3 playing modes need to be easily configured -> 
1. voice locked to an output
2. voice round robin to an output when triggered
3. voice send to output based on how many notes held.

Amnount of voices can be allocated. (default = 8)
Amount of Oscs per voice can be allocated (default = 4, only first oscillator enabled by default)

Voices:
- Need to have voice spread
- Have a root freq
- Can be toggled
- Should all start enabled by default so the available voice count behaves like immediate polyphony

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



- Per output FX. Should be
- Playable synth should have voices playing with the outputs.



Multi channel framework:

Audio Engine: interfaces with coreAudio -> triggers the dsp to fill the audio buffer.  Init amount of ch, samplerate, buffersize. (used to allocate mem and resources)

Source Mixer: VCA style mixer that controls the level at the source of each device. (performance mixer, used to make the performance)Output Mixer: Controls level, delay and eq of each individual output. (system engineer mixer, used to finetune and correct)

Sound Sources:
- Synths: All synths have per voice oscs, envelopes and filtering. Controls are in place to modulate and automate them linked.
    - Robin: Playable synth where you can algorithmicly route voices over the outputs.
    - Decor: Playable synth that has a voice per output. Creates ‘big’ sound -> actually immersed in the system
    - Pieces: Playable granular synth that algorithmicly routes voices over the outputs

Sound Processors:
- FX: All FX are discrete per output. This means that if you have 8 outputs, you also get 8 instances of the effect. Controlls of all effects are linked. Each output has a dry and wet path. Parallel is not possible due to latency. A signal either goes through the fx or not.
    - Saturator: Vartiety of algorothms. In and output level, option to be linked.
    - Chorus. Modulating each output. Control over the depth (how far i modulates) and the speed. Control for 'phase spread'. This means that if you have a modulator at 5 hz, the phase gets distributed over each output.
    - Sidechain (dont know how to implement that yet)

This is maybe a bit more detailed. How do you think we should implement this step by step? Should we make 1 git repo for the entire project? Or split it up?


Modulation:
The program has a clock that can be clocked externally (through CV, MIDI), or internally with a BPM setting.
Every Synth and Effect has 2 LFO's. You can choose to lock it to the main clock, or you can have it be internal.

Robin:
The idea of the synth is: easy to get a sound first, then add randomness and aliveness without rebuilding that
sound for every voice.

Start with master controls. Every voice follows those master sections by default.

Then add diversity in 2 layers:
1. Section modulators. Choose preset algorithms that add offsets on top of the master values. Example: a linear
   ramp that gradually opens the filter from voice 1 to voice 12.
2. Section unlink. If you need very specific local behavior, unlink a whole section for a voice and edit it locally.
   Example: unlink the filter section on voices 3 and 7 to create a polyrhythm.

Rules:
- Algorithms should be modulators, not automators, so moving the master still moves the whole set.
- Start with section unlink, not per-parameter unlink.
- If a section is unlinked, it should stop receiving section modulators.
- "Start" and "end" in an algorithm describe the offset range, not an absolute replacement.

Sections:
- VCF: cutoff, resonance
- ENV VCA: attack, decay, sustain, release, amount
- ENV VCF: attack, decay, sustain, release, amount
- OSC 1: frequency, shape
- OSC 2: frequency, shape
- SUB OSC: frequency, shape
