#include "synth/audio/Synth.hpp"

#include <algorithm>

namespace synth::audio {

void Synth::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    oscillator_.setSampleRate(sampleRate);
}

void Synth::setFrequency(float frequencyHz) {
    frequencyHz_.store(std::max(1.0f, frequencyHz));
}

void Synth::setGain(float gain) {
    gain_.store(std::clamp(gain, 0.0f, 1.0f));
}

void Synth::setWaveform(dsp::Waveform waveform) {
    oscillator_.setWaveform(waveform);
}

void Synth::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    // Grab the current synth settings once for this whole audio block.
    oscillator_.setFrequency(frequencyHz_.load());
    const float gain = gain_.load();

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        // Generate one sample for this moment in time...
        const float sample = oscillator_.nextSample() * gain;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            // ...then write that same sample into every channel for this frame.
            output[(frame * channels) + channel] = sample;
        }
    }
}

}  // namespace synth::audio
