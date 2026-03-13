#include "synth/audio/Synth.hpp"
#include "synth/core/Logger.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <csignal>
#include <iostream>
#include <string>
#include <string_view>
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
    float frequency = 400.0f;
    float gain = 0.15f;
    synth::dsp::Waveform waveform = synth::dsp::Waveform::Sine;
    bool showHelp = false;
    std::string errorMessage;
};

bool tryParseWaveform(std::string_view value, synth::dsp::Waveform& waveform) {
    if (value == "sine") {
        waveform = synth::dsp::Waveform::Sine;
        return true;
    }
    if (value == "square") {
        waveform = synth::dsp::Waveform::Square;
        return true;
    }
    if (value == "triangle") {
        waveform = synth::dsp::Waveform::Triangle;
        return true;
    }
    if (value == "saw") {
        waveform = synth::dsp::Waveform::Saw;
        return true;
    }
    if (value == "noise") {
        waveform = synth::dsp::Waveform::Noise;
        return true;
    }
    return false;
}

void printUsage(const char* programName) {
    std::cout
        << "Usage: " << programName << " [options]\n"
        << "Options:\n"
        << "  --help                 Show this help text\n"
        << "  --sample-rate <hz>     Sample rate, default 48000\n"
        << "  --channels <count>     Output channel count, default 2\n"
        << "  --buffer <frames>      Requested frames per buffer, default 256\n"
        << "  --frequency <hz>       Oscillator frequency, default 400\n"
        << "  --gain <0..1>          Output gain, default 0.15\n"
        << "  --waveform <name>      sine, square, triangle, saw, noise\n";
}

// Frist line is a function declaration. The function is named RuntimeConfig and will return a struct of type RuntimeConfig with argumens int argc and char** argv.
RuntimeConfig parseArgs(int argc, char** argv) {
    // Create a temp struct of type RuntimeConfig and name it 'config'.
    RuntimeConfig config;

    // Parses strings from shell at startup and stores them into the struct.
    for (int i = 1; i < argc; ++i) {
        // Converts inputs into a normal cpp string and fill the temp config.
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.showHelp = true;
        } else if (arg == "--waveform" && i + 1 < argc) {
            if (!tryParseWaveform(argv[++i], config.waveform)) {
                config.errorMessage = "Invalid waveform. Use: sine, square, triangle, saw, or noise.";
                return config;
            }
        } else if (arg == "--waveform") {
            config.errorMessage = "Missing value for --waveform.";
            return config;
        } else if (arg == "--sample-rate" && i + 1 < argc) {
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

// Program entry point. The OS passes command-line arguments in argc/argv.
int main(int argc, char** argv) {
    // Register signal handlers so Ctrl+C can stop the app cleanly.
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // Parse command-line flags into one config object for this run.
    const RuntimeConfig config = parseArgs(argc, argv);
    if (!config.errorMessage.empty()) {
        std::cerr << config.errorMessage << '\n';
        printUsage(argv[0]);
        return 1;
    }
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    // Create logging before any audio setup so startup failures are visible.
    synth::core::Logger logger("logs");
    if (!logger.initialize()) {
        return 1;
    }

    // Create the audio backend wrapper (CoreAudio on macOS).
    auto driver = synth::interfaces::createAudioDriver(logger);
    if (!driver) {
        logger.error("Could not create audio driver.");
        return 1;
    }

    // Configure the synth object that will generate samples when the audio system asks for them.
    synth::audio::Synth synth;
    synth.setSampleRate(config.sampleRate);
    synth.setWaveform(config.waveform);
    synth.setFrequency(config.frequency);
    synth.setGain(config.gain);

    synth::interfaces::AudioConfig audioConfig;
    audioConfig.sampleRate = config.sampleRate;
    audioConfig.channels = config.channels;
    audioConfig.framesPerBuffer = config.framesPerBuffer;
    logger.info("Starting audio...");

    // Flow:
    // 1. main passes audio settings + a callback lambda into driver->start(...)
    // 2. the driver stores that callback and registers its CoreAudio render callback
    // 3. later, CoreAudio asks for audio and the driver calls this lambda
    // 4. the lambda forwards the buffer pointer, frame count, and channel count into Synth::render(...)
    if (!driver->start(
        audioConfig, [&](float* output, std::uint32_t frames, std::uint32_t channels) {
            synth.render(output, frames, channels);
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
