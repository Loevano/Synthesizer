#include "synth/audio/SamplePlayerEngine.hpp"

#include <algorithm>
#include <cmath>

namespace synth::audio {

void SamplePlayerEngine::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    applyEnvelopeSettingsToVoices();
}

void SamplePlayerEngine::setOutputChannelCount(std::uint32_t outputChannelCount) {
    const std::size_t channelCount = std::max<std::uint32_t>(1, outputChannelCount);
    const bool hadEnabledOutput =
        std::find(outputEnabled_.begin(), outputEnabled_.end(), true) != outputEnabled_.end();

    outputEnabled_.resize(channelCount, false);
    if (!hadEnabledOutput && !outputEnabled_.empty()) {
        outputEnabled_[0] = true;
        if (outputEnabled_.size() > 1) {
            outputEnabled_[1] = true;
        }
    }
}

void SamplePlayerEngine::setVoiceCount(std::uint32_t voiceCount) {
    const std::uint32_t clampedVoiceCount = std::clamp<std::uint32_t>(voiceCount, 1, 64);
    voices_.resize(clampedVoiceCount);
    applyEnvelopeSettingsToVoices();
    if (nextVoiceIndex_ >= voices_.size()) {
        nextVoiceIndex_ = 0;
    }
}

void SamplePlayerEngine::setSampleBuffer(std::shared_ptr<const SampleBuffer> sampleBuffer) {
    sampleBuffer_ = std::move(sampleBuffer);
    clearNotes();
}

void SamplePlayerEngine::setMidiEnabled(bool enabled) {
    midiEnabled_ = enabled;
    if (!midiEnabled_) {
        clearNotes();
    }
}

void SamplePlayerEngine::setGain(float gain) {
    gain_ = std::clamp(gain, 0.0f, 1.0f);
}

void SamplePlayerEngine::setRootNote(int rootNote) {
    rootNote_ = std::clamp(rootNote, 0, 127);
}

void SamplePlayerEngine::setTransposeSemitones(float transposeSemitones) {
    transposeSemitones_ = std::clamp(transposeSemitones, -48.0f, 48.0f);
}

void SamplePlayerEngine::setFineTuneCents(float fineTuneCents) {
    fineTuneCents_ = std::clamp(fineTuneCents, -100.0f, 100.0f);
}

void SamplePlayerEngine::setStartPosition(float startPosition) {
    startPosition_ = std::clamp(startPosition, 0.0f, 0.999f);
    if (endPosition_ <= startPosition_) {
        endPosition_ = std::min(1.0f, startPosition_ + 0.001f);
    }
}

void SamplePlayerEngine::setEndPosition(float endPosition) {
    endPosition_ = std::clamp(endPosition, 0.001f, 1.0f);
    if (endPosition_ <= startPosition_) {
        startPosition_ = std::max(0.0f, endPosition_ - 0.001f);
    }
}

void SamplePlayerEngine::setLoopEnabled(bool enabled) {
    setPlaybackMode(enabled ? SamplePlaybackMode::Loop : SamplePlaybackMode::Gate);
}

void SamplePlayerEngine::setPlaybackMode(SamplePlaybackMode mode) {
    playbackMode_ = mode;
}

void SamplePlayerEngine::setReverse(bool reverse) {
    reverse_ = reverse;
}

void SamplePlayerEngine::setEnvelopeAttackSeconds(float attackSeconds) {
    attackSeconds_ = std::max(0.0f, attackSeconds);
    applyEnvelopeSettingsToVoices();
}

void SamplePlayerEngine::setEnvelopeDecaySeconds(float decaySeconds) {
    decaySeconds_ = std::max(0.0f, decaySeconds);
    applyEnvelopeSettingsToVoices();
}

void SamplePlayerEngine::setEnvelopeSustainLevel(float sustainLevel) {
    sustainLevel_ = std::clamp(sustainLevel, 0.0f, 1.0f);
    applyEnvelopeSettingsToVoices();
}

void SamplePlayerEngine::setEnvelopeReleaseSeconds(float releaseSeconds) {
    releaseSeconds_ = std::max(0.0f, releaseSeconds);
    applyEnvelopeSettingsToVoices();
}

void SamplePlayerEngine::setOutputEnabled(std::uint32_t outputChannel, bool enabled) {
    if (outputChannel >= outputEnabled_.size()) {
        return;
    }

    outputEnabled_[outputChannel] = enabled;
}

void SamplePlayerEngine::clearNotes() {
    for (auto& voice : voices_) {
        resetVoice(voice);
    }
}

void SamplePlayerEngine::noteOn(int noteNumber, float velocity) {
    if (!midiEnabled_ || sampleBuffer_ == nullptr || sampleBuffer_->empty() || voices_.empty()) {
        return;
    }

    auto [startFrame, endFrame] = playbackWindow();
    if (endFrame <= startFrame + 1.0) {
        return;
    }

    Voice& voice = allocateVoice();
    applyEnvelopeSettings(voice);
    voice.active = true;
    voice.gateOpen = true;
    voice.noteNumber = noteNumber;
    voice.velocity = std::clamp(velocity, 0.0f, 1.0f);
    voice.direction = reverse_ ? -1 : 1;
    voice.position = voice.direction > 0 ? startFrame : endFrame - 1.0;
    voice.startFrame = startFrame;
    voice.endFrame = endFrame;
    const double semitones =
        static_cast<double>(noteNumber - rootNote_) + transposeSemitones_ + (fineTuneCents_ / 100.0);
    voice.step = (sampleBuffer_->sampleRate / sampleRate_) * pitchRatioForSemitones(semitones);
    voice.envelope.reset();
    voice.envelope.noteOn();
}

void SamplePlayerEngine::noteOff(int noteNumber) {
    if (!midiEnabled_) {
        return;
    }
    if (playbackMode_ == SamplePlaybackMode::OneShot) {
        return;
    }

    for (auto& voice : voices_) {
        if (!voice.active || !voice.gateOpen || voice.noteNumber != noteNumber) {
            continue;
        }

        voice.gateOpen = false;
        voice.envelope.noteOff();
        return;
    }
}

void SamplePlayerEngine::process(float* output, std::uint32_t frames, std::uint32_t channels, float masterGain) {
    if (output == nullptr
        || channels == 0
        || sampleBuffer_ == nullptr
        || sampleBuffer_->empty()
        || voices_.empty()
        || !hasEnabledOutput(channels)) {
        return;
    }

    const std::uint32_t routedChannelCount =
        static_cast<std::uint32_t>(std::min<std::size_t>(channels, outputEnabled_.size()));
    const float appliedGain = gain_ * masterGain;

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        float voiceSum = 0.0f;
        for (auto& voice : voices_) {
            if (!voice.active) {
                continue;
            }

            const bool outsidePlaybackWindow = voice.direction > 0
                ? voice.position >= voice.endFrame
                : voice.position < voice.startFrame;
            if (outsidePlaybackWindow) {
                if (playbackMode_ == SamplePlaybackMode::Loop) {
                    const double loopLength = std::max(1.0, voice.endFrame - voice.startFrame);
                    if (voice.direction > 0) {
                        while (voice.position >= voice.endFrame) {
                            voice.position -= loopLength;
                        }
                    } else {
                        while (voice.position < voice.startFrame) {
                            voice.position += loopLength;
                        }
                        while (voice.position >= voice.endFrame) {
                            voice.position -= loopLength;
                        }
                    }
                } else {
                    resetVoice(voice);
                    continue;
                }
            }

            const float envelopeValue = voice.envelope.nextValue();
            if (envelopeValue <= 0.0f && !voice.envelope.isActive()) {
                resetVoice(voice);
                continue;
            }

            voiceSum += readSampleLinear(*sampleBuffer_, voice.position)
                * appliedGain
                * voice.velocity
                * envelopeValue;
            voice.position += voice.step * static_cast<double>(voice.direction);
        }

        if (voiceSum == 0.0f) {
            continue;
        }

        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < routedChannelCount; ++channel) {
            if (!outputEnabled_[channel]) {
                continue;
            }

            output[frameOffset + channel] += voiceSum;
        }
    }
}

double SamplePlayerEngine::pitchRatioForSemitones(double semitones) {
    return std::exp2(semitones / 12.0);
}

float SamplePlayerEngine::readSampleLinear(const SampleBuffer& sampleBuffer, double position) {
    if (sampleBuffer.samples.empty()) {
        return 0.0f;
    }

    const double clampedPosition =
        std::clamp(position, 0.0, static_cast<double>(sampleBuffer.samples.size() - 1));
    const auto baseIndex = static_cast<std::size_t>(clampedPosition);
    const std::size_t nextIndex = std::min(baseIndex + 1, sampleBuffer.samples.size() - 1);
    const float fraction = static_cast<float>(clampedPosition - static_cast<double>(baseIndex));
    return sampleBuffer.samples[baseIndex] + ((sampleBuffer.samples[nextIndex] - sampleBuffer.samples[baseIndex]) * fraction);
}

void SamplePlayerEngine::applyEnvelopeSettings(Voice& voice) {
    voice.envelope.setSampleRate(sampleRate_);
    voice.envelope.setAttackSeconds(attackSeconds_);
    voice.envelope.setDecaySeconds(decaySeconds_);
    voice.envelope.setSustainLevel(sustainLevel_);
    voice.envelope.setReleaseSeconds(releaseSeconds_);
}

void SamplePlayerEngine::applyEnvelopeSettingsToVoices() {
    for (auto& voice : voices_) {
        applyEnvelopeSettings(voice);
    }
}

void SamplePlayerEngine::resetVoice(Voice& voice) {
    voice.active = false;
    voice.gateOpen = false;
    voice.noteNumber = -1;
    voice.velocity = 1.0f;
    voice.position = 0.0;
    voice.step = 1.0;
    voice.direction = 1;
    voice.startFrame = 0.0;
    voice.endFrame = 0.0;
    voice.envelope.reset();
}

SamplePlayerEngine::Voice& SamplePlayerEngine::allocateVoice() {
    const auto inactiveVoice = std::find_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
        return !voice.active || !voice.envelope.isActive();
    });
    if (inactiveVoice != voices_.end()) {
        return *inactiveVoice;
    }

    Voice& voice = voices_[nextVoiceIndex_ % voices_.size()];
    nextVoiceIndex_ = (nextVoiceIndex_ + 1) % static_cast<std::uint32_t>(voices_.size());
    return voice;
}

std::pair<double, double> SamplePlayerEngine::playbackWindow() const {
    if (sampleBuffer_ == nullptr || sampleBuffer_->samples.size() < 2) {
        return {0.0, 0.0};
    }

    const double frameCount = static_cast<double>(sampleBuffer_->samples.size());
    const double startFrame = std::floor(startPosition_ * frameCount);
    const double endFrame = std::max(startFrame + 1.0, std::ceil(endPosition_ * frameCount));
    return {startFrame, std::min(endFrame, frameCount)};
}

bool SamplePlayerEngine::hasEnabledOutput(std::uint32_t channels) const {
    const std::uint32_t routedChannelCount =
        static_cast<std::uint32_t>(std::min<std::size_t>(channels, outputEnabled_.size()));
    for (std::uint32_t channel = 0; channel < routedChannelCount; ++channel) {
        if (outputEnabled_[channel]) {
            return true;
        }
    }
    return false;
}

}  // namespace synth::audio
