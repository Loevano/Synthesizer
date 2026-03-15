#include "synth/audio/Synth.hpp"

#include <algorithm>

namespace synth::audio {

Synth::Synth() {
    configure({});
}

void Synth::configure(const SynthConfig& config) {
    const std::uint32_t voiceCount = std::max<std::uint32_t>(1, config.voiceCount);
    const std::uint32_t oscillatorsPerVoice = std::max<std::uint32_t>(1, config.oscillatorsPerVoice);
    oscillatorsPerVoice_ = oscillatorsPerVoice;

    voices_.clear();
    voices_.reserve(voiceCount);

    for (std::uint32_t i = 0; i < voiceCount; ++i) {
        voices_.emplace_back();
        auto& voice = voices_.back();
        voice.configure(oscillatorsPerVoice);
        voice.setSampleRate(sampleRate_);
        voice.setFrequency(frequencyHz_.load());
        voice.setWaveform(waveform_);
        voice.setGain(1.0f);
        voice.setEnvelopeAttackSeconds(attackSeconds_);
        voice.setEnvelopeDecaySeconds(decaySeconds_);
        voice.setEnvelopeSustainLevel(sustainLevel_);
        voice.setEnvelopeReleaseSeconds(releaseSeconds_);
        voice.setOutputChannelCount(outputChannelCount_);
        for (std::uint32_t outputIndex = 0; outputIndex < outputChannelCount_; ++outputIndex) {
            voice.setOutputEnabled(outputIndex, outputIndex == (i % outputChannelCount_));
        }
        voice.setActive(true);
    }

    lfo_.setSampleRate(sampleRate_);
    lfo_.setOutputChannelCount(outputChannelCount_);
}

void Synth::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    lfo_.setSampleRate(sampleRate_);
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

void Synth::setVoiceActive(std::uint32_t voiceIndex, bool active) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setActive(active);
}

void Synth::setVoiceFrequency(std::uint32_t voiceIndex, float frequencyHz) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFrequency(frequencyHz);
}

void Synth::setVoiceGain(std::uint32_t voiceIndex, float gain) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setGain(gain);
}

void Synth::setOutputChannelCount(std::uint32_t outputChannelCount) {
    outputChannelCount_ = std::max<std::uint32_t>(1, outputChannelCount);
    lfo_.setOutputChannelCount(outputChannelCount_);
    for (auto& voice : voices_) {
        voice.setOutputChannelCount(outputChannelCount_);
    }
}

void Synth::setVoiceOutputEnabled(std::uint32_t voiceIndex, std::uint32_t outputChannel, bool enabled) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOutputEnabled(outputChannel, enabled);
}

void Synth::setOscillatorEnabled(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, bool enabled) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorEnabled(oscillatorIndex, enabled);
}

void Synth::setOscillatorGain(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, float gain) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorGain(oscillatorIndex, gain);
}

void Synth::setOscillatorFrequency(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, float frequencyValue) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorFrequency(oscillatorIndex, frequencyValue);
}

void Synth::setOscillatorRelativeToVoice(std::uint32_t voiceIndex,
                                         std::uint32_t oscillatorIndex,
                                         bool relativeToVoice) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorRelativeToVoice(oscillatorIndex, relativeToVoice);
}

void Synth::setOscillatorWaveform(std::uint32_t voiceIndex,
                                  std::uint32_t oscillatorIndex,
                                  dsp::Waveform waveform) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorWaveform(oscillatorIndex, waveform);
}

void Synth::setEnvelopeAttackSeconds(float attackSeconds) {
    attackSeconds_ = std::max(0.0f, attackSeconds);
    for (auto& voice : voices_) {
        voice.setEnvelopeAttackSeconds(attackSeconds_);
    }
}

void Synth::setEnvelopeDecaySeconds(float decaySeconds) {
    decaySeconds_ = std::max(0.0f, decaySeconds);
    for (auto& voice : voices_) {
        voice.setEnvelopeDecaySeconds(decaySeconds_);
    }
}

void Synth::setEnvelopeSustainLevel(float sustainLevel) {
    sustainLevel_ = std::clamp(sustainLevel, 0.0f, 1.0f);
    for (auto& voice : voices_) {
        voice.setEnvelopeSustainLevel(sustainLevel_);
    }
}

void Synth::setEnvelopeReleaseSeconds(float releaseSeconds) {
    releaseSeconds_ = std::max(0.0f, releaseSeconds);
    for (auto& voice : voices_) {
        voice.setEnvelopeReleaseSeconds(releaseSeconds_);
    }
}

void Synth::setLfoEnabled(bool enabled) {
    lfo_.setEnabled(enabled);
}

void Synth::setLfoWaveform(dsp::LfoWaveform waveform) {
    lfo_.setWaveform(waveform);
}

void Synth::setLfoDepth(float depth) {
    lfo_.setDepth(depth);
}

void Synth::setLfoPhaseSpreadDegrees(float phaseSpreadDegrees) {
    lfo_.setPhaseSpreadDegrees(phaseSpreadDegrees);
}

void Synth::setLfoPolarityFlip(bool polarityFlip) {
    lfo_.setPolarityFlip(polarityFlip);
}

void Synth::setLfoUnlinkedOutputs(bool unlinkedOutputs) {
    lfo_.setUnlinkedOutputs(unlinkedOutputs);
}

void Synth::setLfoClockLinked(bool clockLinked) {
    lfo_.setClockLinked(clockLinked);
}

void Synth::setLfoTempoBpm(float tempoBpm) {
    lfo_.setTempoBpm(tempoBpm);
}

void Synth::setLfoRateMultiplier(float rateMultiplier) {
    lfo_.setRateMultiplier(rateMultiplier);
}

void Synth::setLfoFixedFrequencyHz(float frequencyHz) {
    lfo_.setFixedFrequencyHz(frequencyHz);
}

void Synth::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    std::fill(output, output + sampleCount, 0.0f);

    renderAdd(output, frames, channels);
}

void Synth::renderAdd(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const float gain = gain_.load();
    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    lfoModulationBuffer_.assign(sampleCount, 1.0f);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        lfo_.renderFrame(lfoModulationBuffer_.data() + (frame * channels), channels);
    }

    for (auto& voice : voices_) {
        voice.renderAdd(output, frames, channels, gain, lfoModulationBuffer_.data());
    }
}

void Synth::noteOn(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].noteOn();
}

void Synth::noteOff(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].noteOff();
}

void Synth::clearNotes() {
    for (auto& voice : voices_) {
        voice.clearNote();
    }
}

}  // namespace synth::audio
