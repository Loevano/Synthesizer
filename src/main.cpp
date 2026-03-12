#include "synth/audio/SynthEngine.hpp"
#include "synth/core/Logger.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <csignal>
#include <string>
#include <thread>

namespace {

std::atomic<bool> gRunning{true};

void handleSignal(int /*signal*/) {
    gRunning.store(false);
}

struct RuntimeConfig {
    double sampleRate = 48000.0;
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
    float frequency = 220.0f;
    float gain = 0.15f;
};

RuntimeConfig parseArgs(int argc, char** argv) {
    RuntimeConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sample-rate" && i + 1 < argc) {
            config.sampleRate = std::stod(argv[++i]);
        } else if (arg == "--channels" && i + 1 < argc) {
            config.channels = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--buffer" && i + 1 < argc) {
            config.framesPerBuffer = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--frequency" && i + 1 < argc) {
            config.frequency = std::stof(argv[++i]);
        } else if (arg == "--gain" && i + 1 < argc) {
            config.gain = std::stof(argv[++i]);
        }
    }

    return config;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const RuntimeConfig config = parseArgs(argc, argv);

    synth::core::Logger logger("logs");
    if (!logger.initialize()) {
        return 1;
    }

    auto driver = synth::interfaces::createAudioDriver(logger);
    if (!driver) {
        logger.error("Could not create audio driver.");
        return 1;
    }

    synth::audio::SynthEngine synthEngine;
    synthEngine.setSampleRate(config.sampleRate);
    synthEngine.setFrequency(config.frequency);
    synthEngine.setGain(config.gain);

    synth::interfaces::AudioConfig audioConfig;
    audioConfig.sampleRate = config.sampleRate;
    audioConfig.channels = config.channels;
    audioConfig.framesPerBuffer = config.framesPerBuffer;

    logger.info("Starting audio...");
    if (!driver->start(audioConfig, [&](float* output, std::uint32_t frames, std::uint32_t channels) {
            synthEngine.render(output, frames, channels);
        })) {
        logger.error("Audio failed to start.");
        return 1;
    }

    logger.info("Running. Press Ctrl+C to stop.");
    while (gRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    driver->stop();
    logger.info("Stopped.");
    return 0;
}
