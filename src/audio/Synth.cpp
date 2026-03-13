#include "synth/audio/Synth.hpp"

#include <algorithm>

namespace synth::audio {

Synth::Synth() {
    configure({});
}

void Synth::configure(const SynthConfig& config) {
    const std::uint32_t voiceCount = std::max<std::uint32_t>(1, config.voiceCount);
    const std::uint32_t oscillatorsPerVoice = std::max<std::uint32_t>(1, config.oscillatorsPerVoice);

    voices_.clear();
    voices_.reserve(voiceCount);

    for (std::uint32_t i = 0; i < voiceCount; ++i) {
        voices_.emplace_back();
        auto& voice = voices_.back();
        voice.configure(oscillatorsPerVoice);
        voice.setSampleRate(sampleRate_);
        voice.setFrequency(frequencyHz_.load());
        voice.setWaveform(waveform_);
        voice.setActive(i == 0);
    }
}

void Synth::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate_);
    }
}

void Synth::setFrequency(float frequencyHz) {
    const float clampedFrequency = std::max(1.0f, frequencyHz);
    frequencyHz_.store(clampedFrequency);
    for (auto& voice : voices_) {
        voice.setFrequency(clampedFrequency);
    }
}

void Synth::setGain(float gain) {
    gain_.store(std::clamp(gain, 0.0f, 1.0f));
}

void Synth::setWaveform(dsp::Waveform waveform) {
    waveform_ = waveform;
    for (auto& voice : voices_) {
        voice.setWaveform(waveform_);
    }
}

void Synth::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    std::fill(output, output + sampleCount, 0.0f);

    const float gain = gain_.load();

    for (auto& voice : voices_) {
        voice.renderAdd(output, frames, channels, gain);
    }
}

}  // namespace synth::audio
