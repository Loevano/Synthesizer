#pragma once

#include <cstdint>
#include <vector>

#include "synth/dsp/Oscillator.hpp"

namespace synth::audio {

struct OscillatorSlot {
    dsp::Oscillator oscillator;
    bool enabled = false;
    float gain = 1.0f;
};

class Voice {
public:
    void configure(std::uint32_t oscillatorCount);
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setWaveform(dsp::Waveform waveform);
    void setActive(bool active);

    void renderAdd(float* output, std::uint32_t frames, std::uint32_t channels, float voiceGain);

private:
    std::vector<OscillatorSlot> oscillators_;
    double sampleRate_ = 48000.0;
    float frequencyHz_ = 440.0f;
    dsp::Waveform waveform_ = dsp::Waveform::Sine;
    bool active_ = false;
};

}  // namespace synth::audio
