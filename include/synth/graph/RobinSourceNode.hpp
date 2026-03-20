#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "synth/audio/PolySynth.hpp"

namespace synth::graph {

class RobinSourceNode {
public:
    void configure(const audio::PolySynthConfig& config);
    void prepare(double sampleRate, std::uint32_t outputChannels);
    void setLevel(bool enabled, float level);
    void process(float* output, std::uint32_t frames, std::uint32_t channels);

    audio::PolySynth& engine();
    const audio::PolySynth& engine() const;

private:
    audio::PolySynth engine_;
    std::atomic<float> targetGain_{0.0f};
    float currentGain_ = 0.0f;
    std::vector<float> renderBuffer_;
};

}  // namespace synth::graph
