#include "synth/graph/RobinSourceNode.hpp"

#include <algorithm>

namespace synth::graph {

void RobinSourceNode::configure(const audio::PolySynthConfig& config) {
    engine_.configure(config);
    engine_.setGain(1.0f);
}

void RobinSourceNode::prepare(double sampleRate, std::uint32_t outputChannels) {
    engine_.setSampleRate(sampleRate);
    engine_.setOutputChannelCount(outputChannels);
    engine_.setGain(1.0f);
}

void RobinSourceNode::setLevel(bool enabled, float level) {
    targetGain_.store(enabled ? std::clamp(level, 0.0f, 1.0f) : 0.0f);
}

void RobinSourceNode::process(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    if (renderBuffer_.size() < sampleCount) {
        renderBuffer_.resize(sampleCount);
    }
    std::fill(renderBuffer_.begin(), renderBuffer_.begin() + sampleCount, 0.0f);
    engine_.process(renderBuffer_.data(), frames, channels);

    const float targetGain = targetGain_.load();
    const float gainStep = frames > 1
        ? (targetGain - currentGain_) / static_cast<float>(frames - 1)
        : 0.0f;

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const float gain = frames > 1 ? currentGain_ + (gainStep * static_cast<float>(frame)) : targetGain;
        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            output[frameOffset + channel] += renderBuffer_[frameOffset + channel] * gain;
        }
    }

    currentGain_ = targetGain;
}

audio::PolySynth& RobinSourceNode::engine() {
    return engine_;
}

const audio::PolySynth& RobinSourceNode::engine() const {
    return engine_;
}

}  // namespace synth::graph
