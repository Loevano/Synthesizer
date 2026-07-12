#include "synth/graph/PiecesSourceNode.hpp"

#include <algorithm>

namespace synth::graph {

void PiecesSourceNode::prepare(double sampleRate, std::uint32_t outputChannels) {
    engine_.setSampleRate(sampleRate);
    engine_.setOutputChannelCount(outputChannels);
}

void PiecesSourceNode::setVoiceCount(std::uint32_t voiceCount) {
    engine_.setVoiceCount(voiceCount);
}

void PiecesSourceNode::setSampleBuffer(std::shared_ptr<const audio::SampleBuffer> sampleBuffer) {
    engine_.setSampleBuffer(std::move(sampleBuffer));
}

void PiecesSourceNode::setMidiEnabled(bool enabled) {
    engine_.setMidiEnabled(enabled);
}

void PiecesSourceNode::setGain(float gain) {
    engine_.setGain(gain);
}

void PiecesSourceNode::setRootNote(int rootNote) {
    engine_.setRootNote(rootNote);
}

void PiecesSourceNode::setTransposeSemitones(float transposeSemitones) {
    engine_.setTransposeSemitones(transposeSemitones);
}

void PiecesSourceNode::setFineTuneCents(float fineTuneCents) {
    engine_.setFineTuneCents(fineTuneCents);
}

void PiecesSourceNode::setStartPosition(float startPosition) {
    engine_.setStartPosition(startPosition);
}

void PiecesSourceNode::setEndPosition(float endPosition) {
    engine_.setEndPosition(endPosition);
}

void PiecesSourceNode::setLoopEnabled(bool enabled) {
    engine_.setLoopEnabled(enabled);
}

void PiecesSourceNode::setPlaybackMode(audio::SamplePlaybackMode mode) {
    engine_.setPlaybackMode(mode);
}

void PiecesSourceNode::setReverse(bool reverse) {
    engine_.setReverse(reverse);
}

void PiecesSourceNode::setEnvelopeAttackSeconds(float attackSeconds) {
    engine_.setEnvelopeAttackSeconds(attackSeconds);
}

void PiecesSourceNode::setEnvelopeDecaySeconds(float decaySeconds) {
    engine_.setEnvelopeDecaySeconds(decaySeconds);
}

void PiecesSourceNode::setEnvelopeSustainLevel(float sustainLevel) {
    engine_.setEnvelopeSustainLevel(sustainLevel);
}

void PiecesSourceNode::setEnvelopeReleaseSeconds(float releaseSeconds) {
    engine_.setEnvelopeReleaseSeconds(releaseSeconds);
}

void PiecesSourceNode::setOutputEnabled(std::uint32_t outputChannel, bool enabled) {
    engine_.setOutputEnabled(outputChannel, enabled);
}

void PiecesSourceNode::clearNotes() {
    engine_.clearNotes();
}

void PiecesSourceNode::noteOn(int noteNumber, float velocity) {
    engine_.noteOn(noteNumber, velocity);
}

void PiecesSourceNode::noteOff(int noteNumber) {
    engine_.noteOff(noteNumber);
}

void PiecesSourceNode::process(float* output,
                               std::uint32_t frames,
                               std::uint32_t channels,
                               bool enabled,
                               float level) {
    if (!enabled) {
        targetLevel_.store(0.0f);
        return;
    }

    if (output == nullptr || channels == 0) {
        return;
    }

    targetLevel_.store(std::clamp(level, 0.0f, 1.0f));
    const float targetLevel = targetLevel_.load();
    const float levelStep = frames > 1
        ? (targetLevel - currentLevel_) / static_cast<float>(frames - 1)
        : 0.0f;

    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    if (renderBuffer_.size() < sampleCount) {
        renderBuffer_.resize(sampleCount);
    }
    std::fill(renderBuffer_.begin(), renderBuffer_.begin() + sampleCount, 0.0f);
    engine_.process(renderBuffer_.data(), frames, channels, 1.0f);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const float frameLevel = frames > 1 ? currentLevel_ + (levelStep * static_cast<float>(frame)) : targetLevel;
        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            output[frameOffset + channel] += renderBuffer_[frameOffset + channel] * frameLevel;
        }
    }

    currentLevel_ = targetLevel;
}

}  // namespace synth::graph
