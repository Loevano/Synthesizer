#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace synth::core {
class Logger;
}

namespace synth::interfaces {

struct AudioConfig {
    double sampleRate = 48000.0;
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
};

using AudioCallback = std::function<void(float* interleavedOutput, std::uint32_t frames, std::uint32_t channels)>;

class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;

    virtual bool start(const AudioConfig& config, AudioCallback callback) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

std::unique_ptr<IAudioDriver> createAudioDriver(core::Logger& logger);

}  // namespace synth::interfaces
