#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "synth/audio/SampleBuffer.hpp"

namespace synth::audio {

struct SampleLoadResult {
    std::shared_ptr<const SampleBuffer> buffer;
    std::string errorMessage;

    bool ok() const {
        return buffer != nullptr && errorMessage.empty();
    }
};

SampleLoadResult loadSampleFile(const std::filesystem::path& path);

}  // namespace synth::audio
