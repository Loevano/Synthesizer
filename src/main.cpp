#include "synth/app/SynthController.hpp"
#include "synth/core/CrashDiagnostics.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::atomic<bool> gRunning{true};

void handleSignal(int /*signal*/) {
    gRunning.store(false);
}

struct CliConfig {
    synth::app::RuntimeConfig runtime;
    bool showHelp = false;
    std::string errorMessage;
};

void printUsage(const char* programName) {
    std::cout
        << "Usage: " << programName << " [options]\n"
        << "Options:\n"
        << "  --help                 Show this help text\n"
        << "  --sample-rate <hz>     Sample rate, default 48000\n"
        << "  --channels <count>     Output channel count, default 2\n"
        << "  --buffer <frames>      Requested frames per buffer, default 256\n"
        << "  --voices <count>       Voice capacity, default 8\n"
        << "  --oscillators-per-voice <count>\n"
        << "                         Oscillator slots per voice, default 6\n"
        << "  --frequency <hz>       Oscillator frequency, default 400\n"
        << "  --gain <0..1>          Output gain, default 0.15\n";
}

CliConfig parseArgs(int argc, char** argv) {
    CliConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.showHelp = true;
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            config.runtime.sampleRate = std::stod(argv[++i]);
        } else if (arg == "--channels" && i + 1 < argc) {
            config.runtime.channels = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--buffer" && i + 1 < argc) {
            config.runtime.framesPerBuffer = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--voices" && i + 1 < argc) {
            config.runtime.voiceCount = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--oscillators-per-voice" && i + 1 < argc) {
            config.runtime.oscillatorsPerVoice = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--frequency" && i + 1 < argc) {
            config.runtime.frequency = std::stof(argv[++i]);
        } else if (arg == "--gain" && i + 1 < argc) {
            config.runtime.gain = std::stof(argv[++i]);
        }
    }

    return config;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const CliConfig config = parseArgs(argc, argv);
    if (!config.errorMessage.empty()) {
        std::cerr << config.errorMessage << '\n';
        printUsage(argv[0]);
        return 1;
    }
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    synth::app::SynthController controller(config.runtime);
    if (!controller.startAudio()) {
        return 1;
    }

    controller.crashDiagnostics().breadcrumb("CLI host started.");
    controller.logger().info("Running. Press Ctrl+C to stop.");
    while (gRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    controller.stopAudio();
    return 0;
}
