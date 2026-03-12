#include "synth/audio/SynthEngine.hpp"

#include <algorithm>
#include <cmath>

namespace synth::audio {

namespace {
constexpr float kAttackSeconds = 0.005f;
constexpr float kReleaseSeconds = 0.06f;
}

SynthEngine::SynthEngine(dsp::ModuleHost& moduleHost)
    : moduleHost_(moduleHost) {}

void SynthEngine::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    oscillator_.setSampleRate(sampleRate_);
    moduleHost_.prepare(sampleRate_);
}

void SynthEngine::noteOn(int midiNote, float velocity) {
    const float clampedVelocity = std::clamp(velocity, 0.0f, 1.0f);
    noteVelocity_.store(clampedVelocity);
    frequencyHz_.store(midiNoteToFrequency(midiNote));
    gate_.store(true);
}

void SynthEngine::noteOff(int /*midiNote*/) {
    gate_.store(false);
}

void SynthEngine::setMasterGain(float gain) {
    masterGain_.store(std::clamp(gain, 0.0f, 1.0f));
}

void SynthEngine::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    oscillator_.setFrequency(frequencyHz_.load());

    const float attackStep = kAttackSeconds <= 0.0f ? 1.0f : (1.0f / (kAttackSeconds * static_cast<float>(sampleRate_)));
    const float releaseStep = kReleaseSeconds <= 0.0f ? 1.0f : (1.0f / (kReleaseSeconds * static_cast<float>(sampleRate_)));

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        if (gate_.load()) {
            envelope_ = std::min(1.0f, envelope_ + attackStep);
        } else {
            envelope_ = std::max(0.0f, envelope_ - releaseStep);
        }

        float sample = oscillator_.nextSample();
        sample *= envelope_ * noteVelocity_.load() * masterGain_.load();
        sample = moduleHost_.processSample(sample);

        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            output[(frame * channels) + channel] = sample;
        }
    }
}

float SynthEngine::midiNoteToFrequency(int midiNote) {
    return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f);
}

}  // namespace synth::audio
