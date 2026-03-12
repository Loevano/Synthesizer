#include "synth/audio/SynthEngine.hpp"
#include "synth/core/Logger.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <csignal>
#include <string>
#include <thread>

// use namespace to make all function and variable names unique
namespace {

// use atomic cause variable is being used in two or more threads -> avoids data race and errors
std::atomic<bool> gRunning{true};

void handleSignal(int /*signal*/) {
    gRunning.store(false);
}

// Struct = recipe for an object. In this case it holds 5 different variables
// This struct is used set the config for the program and audio drivers.
struct RuntimeConfig {
    double sampleRate = 48000.0;
    // uint32_t = unsigned interger of 32 bits (max value of 4,294,967,295). Standard to declare in 32 bits.
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
    float frequency = 220.0f;
    float gain = 0.15f;
};

// Frist line is a function declaration. The function is named RuntimeConfig and will return a struct of type RuntimeConfig with argumens int argc and char** argv.
RuntimeConfig parseArgs(int argc, char** argv) {
    // Create a temp struct of type RuntimeConfig and name it 'config'.
    RuntimeConfig config;

    // Parses strings from shell at startup and stores them into the struct.
    for (int i = 1; i < argc; ++i) {
        // Converts inputs into a normal cpp string and fill the temp config.
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

// Main functions that gets called and gets handed the inputs of the OS
int main(int argc, char** argv) {
    // Handles Ctrl + c to quit.
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // Create a Runtime config and send the terminal inputs to the parseArgs function.
    const RuntimeConfig config = parseArgs(argc, argv);

    // Logs events for debugging -> no toggle at runtime yet.
    synth::core::Logger logger("logs");
    if (!logger.initialize()) {
        return 1;
    }

    // Creates a driver and logs if driver is not created.
    auto driver = synth::interfaces::createAudioDriver(logger);
    if (!driver) {
        logger.error("Could not create audio driver.");
        return 1;
    }

    // Create obj synthEngine and audioCongfig and initialize with set and get functions.
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
