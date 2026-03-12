#include "synth/audio/AudioEngine.hpp"

#include "synth/core/Logger.hpp"

namespace synth::audio {

AudioEngine::AudioEngine(core::Logger& logger, dsp::ModuleHost& moduleHost)
    : logger_(logger),
      driver_(interfaces::createAudioDriver(logger_)),
      synth_(moduleHost) {}

bool AudioEngine::start(const interfaces::AudioConfig& config) {
    if (!driver_) {
        logger_.error("Audio driver was not created.");
        return false;
    }

    synth_.setSampleRate(config.sampleRate);

    const bool started = driver_->start(config, [this](float* output, std::uint32_t frames, std::uint32_t channels) {
        synth_.render(output, frames, channels);
    });

    if (started) {
        logger_.info("Audio engine started.");
    } else {
        logger_.error("Audio engine failed to start.");
    }

    return started;
}

void AudioEngine::stop() {
    if (driver_) {
        driver_->stop();
        logger_.info("Audio engine stopped.");
    }
}

SynthEngine& AudioEngine::synth() {
    return synth_;
}

}  // namespace synth::audio
