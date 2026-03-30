#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace synth::core {
class Logger;
}

namespace synth::interfaces {

struct OutputDeviceInfo {
    std::string id;
    std::string name;
    std::uint32_t outputChannels = 0;
    std::uint32_t currentBufferFrames = 0;
    std::uint32_t minBufferFrames = 0;
    std::uint32_t maxBufferFrames = 0;
    bool isDefault = false;
};

struct AudioConfig {
    double sampleRate = 48000.0;
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
    std::string outputDeviceId;
};

using AudioCallback = std::function<void(float* interleavedOutput, std::uint32_t frames, std::uint32_t channels)>;

class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;

    virtual bool start(const AudioConfig& config, AudioCallback callback) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual std::vector<OutputDeviceInfo> availableOutputDevices() const = 0;
};

std::unique_ptr<IAudioDriver> createAudioDriver(core::Logger& logger);

}  // namespace synth::interfaces
