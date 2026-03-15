#include "synth/graph/OutputMixerNode.hpp"

#include <algorithm>

namespace synth::graph {

void OutputMixerNode::resize(std::uint32_t outputChannels) {
    const std::size_t channelCount = std::max<std::uint32_t>(1, outputChannels);
    channels_.resize(channelCount);
    delayLines_.resize(channelCount);
    eqStages_.resize(channelCount);
}

void OutputMixerNode::prepare(double sampleRate) {
    for (std::size_t outputIndex = 0; outputIndex < channels_.size(); ++outputIndex) {
        delayLines_[outputIndex].prepare(sampleRate, kMaxDelayMs);
        delayLines_[outputIndex].setDelayMs(channels_[outputIndex].delayMs);
        eqStages_[outputIndex].prepare(sampleRate);
        eqStages_[outputIndex].setGainsDb(channels_[outputIndex].lowDb,
                                          channels_[outputIndex].midDb,
                                          channels_[outputIndex].highDb);
    }
}

void OutputMixerNode::setLevel(std::uint32_t outputChannel, float level) {
    if (outputChannel >= channels_.size()) {
        return;
    }

    channels_[outputChannel].level = std::clamp(level, 0.0f, 1.0f);
}

void OutputMixerNode::setDelayMs(std::uint32_t outputChannel, float delayMs) {
    if (outputChannel >= channels_.size()) {
        return;
    }

    channels_[outputChannel].delayMs = std::clamp(delayMs, 0.0f, kMaxDelayMs);
    delayLines_[outputChannel].setDelayMs(channels_[outputChannel].delayMs);
}

void OutputMixerNode::setEq(std::uint32_t outputChannel, float lowDb, float midDb, float highDb) {
    if (outputChannel >= channels_.size()) {
        return;
    }

    channels_[outputChannel].lowDb = std::clamp(lowDb, -24.0f, 24.0f);
    channels_[outputChannel].midDb = std::clamp(midDb, -24.0f, 24.0f);
    channels_[outputChannel].highDb = std::clamp(highDb, -24.0f, 24.0f);
    eqStages_[outputChannel].setGainsDb(channels_[outputChannel].lowDb,
                                        channels_[outputChannel].midDb,
                                        channels_[outputChannel].highDb);
}

void OutputMixerNode::setEqLow(std::uint32_t outputChannel, float lowDb) {
    if (outputChannel >= channels_.size()) {
        return;
    }

    channels_[outputChannel].lowDb = std::clamp(lowDb, -24.0f, 24.0f);
    eqStages_[outputChannel].setLowGainDb(channels_[outputChannel].lowDb);
}

void OutputMixerNode::setEqMid(std::uint32_t outputChannel, float midDb) {
    if (outputChannel >= channels_.size()) {
        return;
    }

    channels_[outputChannel].midDb = std::clamp(midDb, -24.0f, 24.0f);
    eqStages_[outputChannel].setMidGainDb(channels_[outputChannel].midDb);
}

void OutputMixerNode::setEqHigh(std::uint32_t outputChannel, float highDb) {
    if (outputChannel >= channels_.size()) {
        return;
    }

    channels_[outputChannel].highDb = std::clamp(highDb, -24.0f, 24.0f);
    eqStages_[outputChannel].setHighGainDb(channels_[outputChannel].highDb);
}

void OutputMixerNode::process(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const std::uint32_t appliedChannels =
        static_cast<std::uint32_t>(std::min<std::size_t>(channels, channels_.size()));
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < appliedChannels; ++channel) {
            float sample = output[frameOffset + channel];
            sample = delayLines_[channel].processSample(sample);
            sample = eqStages_[channel].processSample(sample);
            output[frameOffset + channel] = sample * channels_[channel].level;
        }
    }
}

}  // namespace synth::graph
