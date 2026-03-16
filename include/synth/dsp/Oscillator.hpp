#pragma once

#include <cstddef>

namespace synth::dsp {

enum class Waveform {
    Sine,
    Saw,
    Square,
    Triangle,
    Noise
};

class Oscillator {
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setWaveform(Waveform waveform);

    float nextSample();

private:
    void updateWavetable();
    float maxStableFrequencyHz() const;

    double sampleRate_ = 44100.0;
    float frequencyHz_ = 440.0f;
    Waveform waveform_ = Waveform::Sine;
    double phase_ = 0.0;
    std::size_t wavetableIndex_ = 0;
    float outputGain_ = 1.0f;
};

}  // namespace synth::dsp
