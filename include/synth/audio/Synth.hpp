#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "synth/audio/Voice.hpp"

namespace synth::audio {

struct SynthConfig {
    std::uint32_t voiceCount = 16;
    std::uint32_t oscillatorsPerVoice = 6;
};

// Synth is the top-level instrument object. It owns a pool of voices, and each
// voice owns a pool of oscillator slots.
class Synth {
public:
    Synth();

    void configure(const SynthConfig& config);
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setGain(float gain);
    void setWaveform(dsp::Waveform waveform);

    void render(float* output, std::uint32_t frames, std::uint32_t channels);

private:
    std::vector<Voice> voices_;

    std::atomic<float> frequencyHz_{440.0f};
    std::atomic<float> gain_{0.2f};
    double sampleRate_ = 48000.0;
    dsp::Waveform waveform_ = dsp::Waveform::Sine;
};

}  // namespace synth::audio
