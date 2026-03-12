#include "synth/audio/SynthEngine.hpp"

#include <algorithm>

namespace synth::audio {

void SynthEngine::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    oscillator_.setSampleRate(sampleRate);
}

void SynthEngine::setFrequency(float frequencyHz) {
    frequencyHz_.store(std::max(1.0f, frequencyHz));
}

void SynthEngine::setGain(float gain) {
    gain_.store(std::clamp(gain, 0.0f, 1.0f));
}

void SynthEngine::setWaveform(dsp::Waveform waveform) {
    oscillator_.setWaveform(waveform);
}

void SynthEngine::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    oscillator_.setFrequency(frequencyHz_.load());
    const float gain = gain_.load();

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const float sample = oscillator_.nextSample() * gain;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            output[(frame * channels) + channel] = sample;
        }
    }
}

}  // namespace synth::audio
