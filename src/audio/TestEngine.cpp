#include "synth/audio/TestEngine.hpp"

#include <algorithm>
#include <cmath>

namespace synth::audio {

void TestEngine::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    oscillator_.setSampleRate(sampleRate_);
    envelope_.setSampleRate(sampleRate_);
}

void TestEngine::setOutputChannelCount(std::uint32_t outputChannelCount) {
    const std::size_t channelCount = std::max<std::uint32_t>(1, outputChannelCount);
    const bool hadEnabledOutput =
        std::find(outputEnabled_.begin(), outputEnabled_.end(), true) != outputEnabled_.end();

    outputEnabled_.resize(channelCount, false);
    if (!hadEnabledOutput && !outputEnabled_.empty()) {
        outputEnabled_[0] = true;
    }
}

void TestEngine::setFrequency(float frequencyHz) {
    frequencyHz_ = std::max(1.0f, frequencyHz);
    oscillator_.setFrequency(frequencyHz_);
}

void TestEngine::setGain(float gain) {
    gain_ = std::clamp(gain, 0.0f, 1.0f);
}

void TestEngine::setWaveform(dsp::Waveform waveform) {
    oscillator_.setWaveform(waveform);
}

void TestEngine::setOutputEnabled(std::uint32_t outputChannel, bool enabled) {
    if (outputChannel >= outputEnabled_.size()) {
        return;
    }

    outputEnabled_[outputChannel] = enabled;
}

void TestEngine::setActive(bool active) {
    active_ = active;
    if (active_ && heldNotes_.empty()) {
        velocityGain_ = 1.0f;
    } else if (!active_ && heldNotes_.empty()) {
        velocityGain_ = 1.0f;
    }
    syncGate();
}

void TestEngine::setMidiEnabled(bool enabled) {
    midiEnabled_ = enabled;
    if (!midiEnabled_) {
        heldNotes_.clear();
        if (active_) {
            velocityGain_ = 1.0f;
        }
    }
    syncGate();
}

void TestEngine::setEnvelopeAttackSeconds(float attackSeconds) {
    envelope_.setAttackSeconds(attackSeconds);
}

void TestEngine::setEnvelopeDecaySeconds(float decaySeconds) {
    envelope_.setDecaySeconds(decaySeconds);
}

void TestEngine::setEnvelopeSustainLevel(float sustainLevel) {
    envelope_.setSustainLevel(sustainLevel);
}

void TestEngine::setEnvelopeReleaseSeconds(float releaseSeconds) {
    envelope_.setReleaseSeconds(releaseSeconds);
}

void TestEngine::noteOn(int noteNumber, float velocity) {
    if (!midiEnabled_) {
        return;
    }

    heldNotes_.erase(std::remove(heldNotes_.begin(), heldNotes_.end(), noteNumber), heldNotes_.end());
    heldNotes_.push_back(noteNumber);
    setFrequency(midiNoteToFrequency(noteNumber));
    velocityGain_ = std::clamp(velocity, 0.0f, 1.0f);
    syncGate();
}

void TestEngine::noteOff(int noteNumber) {
    if (!midiEnabled_) {
        return;
    }

    const auto newEnd = std::remove(heldNotes_.begin(), heldNotes_.end(), noteNumber);
    if (newEnd == heldNotes_.end()) {
        return;
    }

    heldNotes_.erase(newEnd, heldNotes_.end());
    if (!heldNotes_.empty()) {
        setFrequency(midiNoteToFrequency(heldNotes_.back()));
    } else if (active_) {
        velocityGain_ = 1.0f;
    }
    syncGate();
}

void TestEngine::renderAdd(float* output, std::uint32_t frames, std::uint32_t channels, float masterGain) {
    if (output == nullptr || channels == 0 || outputEnabled_.empty()) {
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

    if (!gateOpen_ && !envelope_.isActive()) {
        return;
    }

    const float appliedGain = gain_ * masterGain * velocityGain_;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const float envelopeValue = envelope_.nextValue();
        if (envelopeValue <= 0.0f && !envelope_.isActive()) {
            continue;
        }

        const float sample = oscillator_.nextSample() * appliedGain * envelopeValue;
        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < routedChannelCount; ++channel) {
            if (!outputEnabled_[channel]) {
                continue;
            }

            output[frameOffset + channel] += sample;
        }
    }
}

float TestEngine::midiNoteToFrequency(int noteNumber) {
    return 440.0f * std::exp2((static_cast<float>(noteNumber) - 69.0f) / 12.0f);
}

void TestEngine::syncGate() {
    const bool shouldGate = shouldGateBeOpen();
    if (shouldGate == gateOpen_) {
        return;
    }

    gateOpen_ = shouldGate;
    if (gateOpen_) {
        envelope_.noteOn();
    } else {
        envelope_.noteOff();
    }
}

bool TestEngine::shouldGateBeOpen() const {
    return active_ || (midiEnabled_ && !heldNotes_.empty());
}

}  // namespace synth::audio
