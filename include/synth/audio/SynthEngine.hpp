#pragma once

#include <atomic>
#include <cstdint>

#include "synth/dsp/ModuleHost.hpp"
#include "synth/dsp/Oscillator.hpp"

namespace synth::audio {

class SynthEngine {
public:
    explicit SynthEngine(dsp::ModuleHost& moduleHost);

    void setSampleRate(double sampleRate);

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);

    void setMasterGain(float gain);

    void render(float* output, std::uint32_t frames, std::uint32_t channels);

private:
    static float midiNoteToFrequency(int midiNote);

    dsp::ModuleHost& moduleHost_;
    dsp::Oscillator oscillator_;

    std::atomic<float> frequencyHz_{440.0f};
    std::atomic<float> noteVelocity_{0.0f};
    std::atomic<bool> gate_{false};
    std::atomic<float> masterGain_{0.2f};

    float envelope_ = 0.0f;
    double sampleRate_ = 48000.0;
};

}  // namespace synth::audio
