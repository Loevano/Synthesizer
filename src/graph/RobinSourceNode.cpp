#include "synth/graph/RobinSourceNode.hpp"

#include <algorithm>

namespace synth::graph {

void RobinSourceNode::configure(const audio::SynthConfig& config) {
    synth_.configure(config);
}

void RobinSourceNode::prepare(double sampleRate, std::uint32_t outputChannels) {
    synth_.setSampleRate(sampleRate);
    synth_.setOutputChannelCount(outputChannels);
}

void RobinSourceNode::setLevel(bool enabled, float level) {
    synth_.setGain(enabled ? std::clamp(level, 0.0f, 1.0f) : 0.0f);
}

void RobinSourceNode::renderAdd(float* output, std::uint32_t frames, std::uint32_t channels) {
    synth_.renderAdd(output, frames, channels);
}

audio::Synth& RobinSourceNode::synth() {
    return synth_;
}

const audio::Synth& RobinSourceNode::synth() const {
    return synth_;
}

}  // namespace synth::graph
