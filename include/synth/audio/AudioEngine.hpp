#pragma once

#include <memory>

#include "synth/audio/SynthEngine.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

namespace synth::core {
class Logger;
}

namespace synth::audio {

class AudioEngine {
public:
    AudioEngine(core::Logger& logger, dsp::ModuleHost& moduleHost);

    bool start(const interfaces::AudioConfig& config);
    void stop();

    SynthEngine& synth();

private:
    core::Logger& logger_;
    std::unique_ptr<interfaces::IAudioDriver> driver_;
    SynthEngine synth_;
};

}  // namespace synth::audio
