#pragma once

#include <cstdint>
#include <vector>

#include "synth/dsp/DelayLine.hpp"
#include "synth/dsp/ThreeBandEq.hpp"

namespace synth::graph {

class OutputMixerNode {
public:
    void resize(std::uint32_t outputChannels);
    void prepare(double sampleRate);
    void setLevel(std::uint32_t outputChannel, float level);
    void setDelayMs(std::uint32_t outputChannel, float delayMs);
    void setEq(std::uint32_t outputChannel, float lowDb, float midDb, float highDb);
    void setEqLow(std::uint32_t outputChannel, float lowDb);
    void setEqMid(std::uint32_t outputChannel, float midDb);
    void setEqHigh(std::uint32_t outputChannel, float highDb);
    void process(float* output, std::uint32_t frames, std::uint32_t channels);

private:
    struct ChannelState {
        float level = 1.0f;
        float delayMs = 0.0f;
        float lowDb = 0.0f;
        float midDb = 0.0f;
        float highDb = 0.0f;
    };

    static constexpr float kMaxDelayMs = 250.0f;

    std::vector<ChannelState> channels_;
    std::vector<dsp::DelayLine> delayLines_;
    std::vector<dsp::ThreeBandEq> eqStages_;
};

}  // namespace synth::graph
