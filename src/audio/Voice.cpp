#include "synth/audio/Voice.hpp"

#include <algorithm>

namespace synth::audio {

void Voice::configure(std::uint32_t oscillatorCount) {
    const std::uint32_t slotCount = std::max<std::uint32_t>(1, oscillatorCount);

    oscillators_.clear();
    oscillators_.reserve(slotCount);

    for (std::uint32_t i = 0; i < slotCount; ++i) {
        OscillatorSlot slot;
        slot.enabled = (i == 0);
        slot.gain = 1.0f;
        slot.oscillator.setSampleRate(sampleRate_);
        slot.oscillator.setFrequency(frequencyHz_);
        slot.oscillator.setWaveform(waveform_);
        oscillators_.push_back(slot);
    }
}

void Voice::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    for (auto& slot : oscillators_) {
        slot.oscillator.setSampleRate(sampleRate_);
    }
}

void Voice::setFrequency(float frequencyHz) {
    frequencyHz_ = std::max(1.0f, frequencyHz);
    for (auto& slot : oscillators_) {
        slot.oscillator.setFrequency(frequencyHz_);
    }
}

void Voice::setWaveform(dsp::Waveform waveform) {
    waveform_ = waveform;
    for (auto& slot : oscillators_) {
        slot.oscillator.setWaveform(waveform_);
    }
}

void Voice::setActive(bool active) {
    active_ = active;
}

void Voice::renderAdd(float* output, std::uint32_t frames, std::uint32_t channels, float voiceGain) {
    if (!active_ || output == nullptr || channels == 0 || oscillators_.empty()) {
        return;
    }

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        float sample = 0.0f;
        std::uint32_t activeOscillatorCount = 0;

        for (auto& slot : oscillators_) {
            if (!slot.enabled) {
                continue;
            }

            sample += slot.oscillator.nextSample() * slot.gain;
            ++activeOscillatorCount;
        }

        if (activeOscillatorCount == 0) {
            continue;
        }

        sample = (sample / static_cast<float>(activeOscillatorCount)) * voiceGain;

        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            output[(frame * channels) + channel] += sample;
        }
    }
}

}  // namespace synth::audio
