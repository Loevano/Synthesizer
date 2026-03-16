#include "synth/graph/FxRackNode.hpp"

#include <algorithm>

namespace synth::graph {

void FxRackNode::resize(std::uint32_t outputChannels) {
    const std::size_t channelCount = std::max<std::uint32_t>(1, outputChannels);
    chorusStages_.resize(channelCount);
    syncChorusState();
}

void FxRackNode::prepare(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    for (auto& stage : chorusStages_) {
        stage.prepare(sampleRate_);
    }
    syncChorusState();
}

void FxRackNode::setChorusEnabled(bool enabled) {
    chorusEnabled_ = enabled;
}

void FxRackNode::setChorusDepth(float depth) {
    chorusDepth_ = std::clamp(depth, 0.0f, 1.0f);
    for (auto& stage : chorusStages_) {
        stage.setDepth(chorusDepth_);
    }
}

void FxRackNode::setChorusSpeedHz(float speedHz) {
    chorusSpeedHz_ = std::clamp(speedHz, 0.01f, 20.0f);
    for (auto& stage : chorusStages_) {
        stage.setRateHz(chorusSpeedHz_);
    }
}

void FxRackNode::setChorusPhaseSpreadDegrees(float phaseSpreadDegrees) {
    chorusPhaseSpreadDegrees_ = std::clamp(phaseSpreadDegrees, 0.0f, 360.0f);
    updateChorusPhaseOffsets();
}

void FxRackNode::process(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0 || chorusStages_.empty()) {
        return;
    }

    const std::uint32_t appliedChannels =
        static_cast<std::uint32_t>(std::min<std::size_t>(channels, chorusStages_.size()));

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (std::uint32_t channel = 0; channel < appliedChannels; ++channel) {
            if (chorusEnabled_) {
                output[frameOffset + channel] = chorusStages_[channel].processSample(output[frameOffset + channel]);
            }
        }
    }
}

void FxRackNode::syncChorusState() {
    for (auto& stage : chorusStages_) {
        stage.setDepth(chorusDepth_);
        stage.setRateHz(chorusSpeedHz_);
    }
    updateChorusPhaseOffsets();
}

void FxRackNode::updateChorusPhaseOffsets() {
    if (chorusStages_.empty()) {
        return;
    }

    const float denominator = chorusStages_.size() > 1 ? static_cast<float>(chorusStages_.size() - 1) : 1.0f;
    for (std::size_t channel = 0; channel < chorusStages_.size(); ++channel) {
        const float phaseOffset = chorusPhaseSpreadDegrees_ * (static_cast<float>(channel) / denominator);
        chorusStages_[channel].setPhaseOffsetDegrees(phaseOffset);
    }
}

}  // namespace synth::graph
