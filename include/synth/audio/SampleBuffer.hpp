#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace synth::audio {

struct SampleBuffer {
    double sampleRate = 48000.0;
    std::vector<float> samples;
    std::string sourcePath;
    std::string displayName;

    bool empty() const {
        return samples.empty();
    }

    std::uint64_t frameCount() const {
        return static_cast<std::uint64_t>(samples.size());
    }
};

}  // namespace synth::audio
