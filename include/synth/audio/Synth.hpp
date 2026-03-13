#pragma once

#include <atomic>
#include <cstdint>

#include "synth/dsp/Oscillator.hpp"

namespace synth::audio {

// Synth is the instrument object. It owns the DSP building blocks that
// generate sound, starting with a single oscillator in this MVP.
class Synth {
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setGain(float gain);
    void setWaveform(dsp::Waveform waveform);

    void render(float* output, std::uint32_t frames, std::uint32_t channels);

private:
    dsp::Oscillator oscillator_;

    std::atomic<float> frequencyHz_{440.0f};
    std::atomic<float> gain_{0.2f};
};

}  // namespace synth::audio
