#include "synth/graph/TestSourceNode.hpp"

#include <algorithm>

namespace synth::graph {

void TestSourceNode::prepare(double sampleRate, std::uint32_t outputChannels) {
    engine_.setSampleRate(sampleRate);
    engine_.setOutputChannelCount(outputChannels);
}

void TestSourceNode::setActive(bool active) {
    engine_.setActive(active);
}

void TestSourceNode::setMidiEnabled(bool enabled) {
    engine_.setMidiEnabled(enabled);
}

void TestSourceNode::setFrequency(float frequencyHz) {
    engine_.setFrequency(frequencyHz);
}

void TestSourceNode::setGain(float gain) {
    engine_.setGain(gain);
}

void TestSourceNode::setWaveform(dsp::Waveform waveform) {
    engine_.setWaveform(waveform);
}

void TestSourceNode::setEnvelopeAttackSeconds(float attackSeconds) {
    engine_.setEnvelopeAttackSeconds(attackSeconds);
}

void TestSourceNode::setEnvelopeDecaySeconds(float decaySeconds) {
    engine_.setEnvelopeDecaySeconds(decaySeconds);
}

void TestSourceNode::setEnvelopeSustainLevel(float sustainLevel) {
    engine_.setEnvelopeSustainLevel(sustainLevel);
}

void TestSourceNode::setEnvelopeReleaseSeconds(float releaseSeconds) {
    engine_.setEnvelopeReleaseSeconds(releaseSeconds);
}

void TestSourceNode::setOutputEnabled(std::uint32_t outputChannel, bool enabled) {
    engine_.setOutputEnabled(outputChannel, enabled);
}

void TestSourceNode::clearNotes() {
    engine_.clearNotes();
}

void TestSourceNode::noteOn(int noteNumber, float velocity) {
    engine_.noteOn(noteNumber, velocity);
}

void TestSourceNode::noteOff(int noteNumber) {
    engine_.noteOff(noteNumber);
}

void TestSourceNode::process(float* output,
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
