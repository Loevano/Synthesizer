#include "synth/graph/RobinSourceNode.hpp"

#include <algorithm>

namespace synth::graph {

void RobinSourceNode::configure(const audio::SynthConfig& config) {
    synth_.configure(config);
    synth_.setGain(1.0f);
}

void RobinSourceNode::prepare(double sampleRate, std::uint32_t outputChannels) {
    synth_.setSampleRate(sampleRate);
    synth_.setOutputChannelCount(outputChannels);
    synth_.setGain(1.0f);
}

void RobinSourceNode::setLevel(bool enabled, float level) {
    targetGain_.store(enabled ? std::clamp(level, 0.0f, 1.0f) : 0.0f);
}

void RobinSourceNode::renderAdd(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    renderBuffer_.resize(sampleCount);
    std::fill(renderBuffer_.begin(), renderBuffer_.end(), 0.0f);
    synth_.renderAdd(renderBuffer_.data(), frames, channels);

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

audio::Synth& RobinSourceNode::synth() {
    return synth_;
}

const audio::Synth& RobinSourceNode::synth() const {
    return synth_;
}

}  // namespace synth::graph
