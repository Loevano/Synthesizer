#include "synth/audio/Voice.hpp"

#include <algorithm>
#include <cmath>

namespace synth::audio {

namespace {

constexpr float kMinOscillatorRatio = 0.01f;
constexpr float kMinFilterCutoffHz = 20.0f;
constexpr float kMaxFilterCutoffHz = 20000.0f;
constexpr float kMinFilterResonance = 0.1f;
constexpr float kMaxFilterResonance = 10.0f;
constexpr float kMaxFilterEnvelopeOctaves = 6.0f;

}

void Voice::configure(std::uint32_t oscillatorCount) {
    const std::uint32_t slotCount = std::max<std::uint32_t>(1, oscillatorCount);

    oscillators_.clear();
    oscillators_.reserve(slotCount);

    for (std::uint32_t i = 0; i < slotCount; ++i) {
        OscillatorSlot slot;
        slot.enabled = (i == 0);
        slot.gain = 1.0f;
        slot.relativeToVoice = true;
        slot.frequencyValue = 1.0f;
        slot.oscillator.setSampleRate(sampleRate_);
        slot.oscillator.setWaveform(waveform_);
        updateOscillatorFrequency(slot);
        oscillators_.push_back(slot);
    }

    envelope_.setSampleRate(sampleRate_);
    filterEnvelope_.setSampleRate(sampleRate_);
    filter_.prepare(sampleRate_);
    filter_.setCutoffHz(filterCutoffHz_);
    filter_.setResonance(filterResonance_);

    if (outputEnabled_.empty()) {
        outputEnabled_.push_back(true);
    }
}

void Voice::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    envelope_.setSampleRate(sampleRate_);
    filterEnvelope_.setSampleRate(sampleRate_);
    filter_.prepare(sampleRate_);
    filter_.setCutoffHz(filterCutoffHz_);
    filter_.setResonance(filterResonance_);
    for (auto& slot : oscillators_) {
        slot.oscillator.setSampleRate(sampleRate_);
        updateOscillatorFrequency(slot);
    }
}

void Voice::setFrequency(float frequencyHz) {
    frequencyHz_ = std::max(1.0f, frequencyHz);
    for (auto& slot : oscillators_) {
        updateOscillatorFrequency(slot);
    }
}

void Voice::setWaveform(dsp::Waveform waveform) {
    waveform_ = waveform;
    for (auto& slot : oscillators_) {
        slot.oscillator.setWaveform(waveform_);
    }
}

void Voice::setOscillatorEnabled(std::uint32_t oscillatorIndex, bool enabled) {
    if (oscillatorIndex >= oscillators_.size()) {
        return;
    }

    oscillators_[oscillatorIndex].enabled = enabled;
}

void Voice::setOscillatorGain(std::uint32_t oscillatorIndex, float gain) {
    if (oscillatorIndex >= oscillators_.size()) {
        return;
    }

    oscillators_[oscillatorIndex].gain = std::clamp(gain, 0.0f, 1.0f);
}

void Voice::setOscillatorFrequency(std::uint32_t oscillatorIndex, float frequencyValue) {
    if (oscillatorIndex >= oscillators_.size()) {
        return;
    }

    auto& slot = oscillators_[oscillatorIndex];
    if (slot.relativeToVoice) {
        slot.frequencyValue = std::max(kMinOscillatorRatio, frequencyValue);
    } else {
        slot.frequencyValue = std::clamp(frequencyValue, 1.0f, 20000.0f);
    }
    updateOscillatorFrequency(slot);
}

void Voice::setOscillatorRelativeToVoice(std::uint32_t oscillatorIndex, bool relativeToVoice) {
    if (oscillatorIndex >= oscillators_.size()) {
        return;
    }

    auto& slot = oscillators_[oscillatorIndex];
    if (slot.relativeToVoice == relativeToVoice) {
        return;
    }

    slot.relativeToVoice = relativeToVoice;
    if (slot.relativeToVoice) {
        slot.frequencyValue = std::max(kMinOscillatorRatio, slot.frequencyValue / frequencyHz_);
    } else {
        slot.frequencyValue = std::clamp(slot.frequencyValue * frequencyHz_, 1.0f, 20000.0f);
    }
    updateOscillatorFrequency(slot);
}

void Voice::setOscillatorWaveform(std::uint32_t oscillatorIndex, dsp::Waveform waveform) {
    if (oscillatorIndex >= oscillators_.size()) {
        return;
    }

    oscillators_[oscillatorIndex].oscillator.setWaveform(waveform);
}

void Voice::setEnvelopeAttackSeconds(float attackSeconds) {
    envelope_.setAttackSeconds(attackSeconds);
}

void Voice::setEnvelopeDecaySeconds(float decaySeconds) {
    envelope_.setDecaySeconds(decaySeconds);
}

void Voice::setEnvelopeSustainLevel(float sustainLevel) {
    envelope_.setSustainLevel(sustainLevel);
}

void Voice::setEnvelopeReleaseSeconds(float releaseSeconds) {
    envelope_.setReleaseSeconds(releaseSeconds);
}

void Voice::setFilterCutoffHz(float cutoffHz) {
    filterCutoffHz_ = std::clamp(cutoffHz, kMinFilterCutoffHz, kMaxFilterCutoffHz);
    filter_.setCutoffHz(filterCutoffHz_);
}

void Voice::setFilterResonance(float resonance) {
    filterResonance_ = std::clamp(resonance, kMinFilterResonance, kMaxFilterResonance);
    filter_.setResonance(filterResonance_);
}

void Voice::setFilterEnvelopeAttackSeconds(float attackSeconds) {
    filterEnvelope_.setAttackSeconds(attackSeconds);
}

void Voice::setFilterEnvelopeDecaySeconds(float decaySeconds) {
    filterEnvelope_.setDecaySeconds(decaySeconds);
}

void Voice::setFilterEnvelopeSustainLevel(float sustainLevel) {
    filterEnvelope_.setSustainLevel(sustainLevel);
}

void Voice::setFilterEnvelopeReleaseSeconds(float releaseSeconds) {
    filterEnvelope_.setReleaseSeconds(releaseSeconds);
}

void Voice::setFilterEnvelopeAmount(float amount) {
    filterEnvelopeAmount_ = std::clamp(amount, 0.0f, 1.0f);
}

void Voice::setGain(float gain) {
    gain_ = std::clamp(gain, 0.0f, 1.0f);
}

void Voice::setOutputChannelCount(std::uint32_t outputChannelCount) {
    const std::size_t channelCount = std::max<std::uint32_t>(1, outputChannelCount);
    const bool hadEnabledOutput =
        std::find(outputEnabled_.begin(), outputEnabled_.end(), true) != outputEnabled_.end();

    outputEnabled_.resize(channelCount, false);
    if (!hadEnabledOutput) {
        outputEnabled_[0] = true;
    }
}

void Voice::setOutputEnabled(std::uint32_t outputChannel, bool enabled) {
    if (outputChannel >= outputEnabled_.size()) {
        return;
    }

    outputEnabled_[outputChannel] = enabled;
}

void Voice::setActive(bool active) {
    if (active) {
        active_ = true;
        pendingDeactivate_ = false;
        return;
    }

    if (envelope_.isActive()) {
        pendingDeactivate_ = true;
        return;
    }

    active_ = false;
    pendingDeactivate_ = false;
    if (!active_) {
        envelope_.reset();
        filterEnvelope_.reset();
        filter_.reset();
    }
}

void Voice::noteOn() {
    if (!active_) {
        return;
    }

    const bool retriggeringActiveNote = envelope_.isActive() || filterEnvelope_.isActive();
    pendingDeactivate_ = false;
    if (!retriggeringActiveNote) {
        filter_.reset();
    }
    envelope_.noteOn();
    filterEnvelope_.noteOn();
}

void Voice::noteOff() {
    envelope_.noteOff();
    filterEnvelope_.noteOff();
}

void Voice::clearNote() {
    pendingDeactivate_ = false;
    envelope_.reset();
    filterEnvelope_.reset();
    filter_.reset();
}

void Voice::updateOscillatorFrequency(OscillatorSlot& slot) {
    const float resolvedFrequency = slot.relativeToVoice
        ? std::max(1.0f, frequencyHz_ * std::max(kMinOscillatorRatio, slot.frequencyValue))
        : std::clamp(slot.frequencyValue, 1.0f, 20000.0f);
    slot.oscillator.setFrequency(resolvedFrequency);
}

void Voice::renderAdd(float* output,
                      std::uint32_t frames,
                      std::uint32_t channels,
                      float masterGain,
                      const float* outputModulation) {
    if (output == nullptr || channels == 0 || oscillators_.empty()) {
        return;
    }

    if ((!active_ && !pendingDeactivate_) || !envelope_.isActive()) {
        return;
    }

    const std::uint32_t routedChannelCount =
        static_cast<std::uint32_t>(std::min<std::size_t>(channels, outputEnabled_.size()));
    bool hasEnabledOutput = false;
    for (std::uint32_t channel = 0; channel < routedChannelCount; ++channel) {
        if (outputEnabled_[channel]) {
            hasEnabledOutput = true;
            break;
        }
    }

    if (!hasEnabledOutput) {
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

        sample /= static_cast<float>(activeOscillatorCount);

        const float filterEnvelopeValue = filterEnvelope_.nextValue();
        const float filterCutoffHz =
            std::clamp(
                filterCutoffHz_ * std::pow(2.0f, filterEnvelopeAmount_ * filterEnvelopeValue * kMaxFilterEnvelopeOctaves),
                kMinFilterCutoffHz,
                kMaxFilterCutoffHz);
        filter_.setCutoffHz(filterCutoffHz);
        sample = filter_.processSample(sample);

        const float envelopeValue = envelope_.nextValue();
        if (envelopeValue <= 0.0f && !envelope_.isActive()) {
            if (pendingDeactivate_) {
                active_ = false;
                pendingDeactivate_ = false;
                filterEnvelope_.reset();
                filter_.reset();
            }
            continue;
        }

        sample *= gain_ * masterGain * envelopeValue;

        const std::uint32_t frameOffset = frame * channels;
        for (std::uint32_t channel = 0; channel < routedChannelCount; ++channel) {
            if (!outputEnabled_[channel]) {
                continue;
            }

            const float modulation = outputModulation != nullptr
                ? outputModulation[frameOffset + channel]
                : 1.0f;
            output[frameOffset + channel] += sample * modulation;
        }
    }

    if (pendingDeactivate_ && !envelope_.isActive()) {
        active_ = false;
        pendingDeactivate_ = false;
        filterEnvelope_.reset();
        filter_.reset();
    }
}

}  // namespace synth::audio
