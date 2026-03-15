#pragma once

#include <cstdint>

#include "synth/audio/Synth.hpp"

namespace synth::graph {

class RobinSourceNode {
public:
    void configure(const audio::SynthConfig& config);
    void prepare(double sampleRate, std::uint32_t outputChannels);
    void setLevel(bool enabled, float level);
    void renderAdd(float* output, std::uint32_t frames, std::uint32_t channels);

    audio::Synth& synth();
    const audio::Synth& synth() const;

private:
    audio::Synth synth_;
};

}  // namespace synth::graph
