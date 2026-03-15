#pragma once

#include <cstdint>
#include <vector>

#include "synth/dsp/Chorus.hpp"

namespace synth::graph {

class FxRackNode {
public:
    void resize(std::uint32_t outputChannels);
    void prepare(double sampleRate);
    void setOutputRoute(std::uint32_t outputChannel, bool routeThroughFx);
    void setChorusEnabled(bool enabled);
    void setChorusLinkedControls(bool linkedControls);
    void setChorusDepth(float depth);
    void setChorusSpeedHz(float speedHz);
    void setChorusPhaseSpreadDegrees(float phaseSpreadDegrees);
    void process(float* output, std::uint32_t frames, std::uint32_t channels);

private:
    void syncChorusState();
    void updateChorusPhaseOffsets();

    struct OutputState {
        bool routeThroughFx = false;
    };

    std::vector<OutputState> outputs_;
    std::vector<dsp::Chorus> chorusStages_;
    bool chorusEnabled_ = false;
    bool chorusLinkedControls_ = true;
    float chorusDepth_ = 0.5f;
    float chorusSpeedHz_ = 0.25f;
    float chorusPhaseSpreadDegrees_ = 0.0f;
    double sampleRate_ = 48000.0;
};

}  // namespace synth::graph
