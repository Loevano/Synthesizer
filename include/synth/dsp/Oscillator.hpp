#pragma once

namespace synth::dsp {

enum class Waveform {
    Sine,
    Saw,
    Square,
    Triangle
};

class Oscillator {
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setWaveform(Waveform waveform);

    float nextSample();

private:
    double sampleRate_ = 44100.0;
    float frequencyHz_ = 440.0f;
    Waveform waveform_ = Waveform::Sine;
    double phase_ = 0.0;
};

}  // namespace synth::dsp
