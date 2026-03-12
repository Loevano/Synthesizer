#include "synth/audio/AudioEngine.hpp"
#include "synth/core/Logger.hpp"
#include "synth/dsp/ModuleHost.hpp"
#include "synth/midi/MidiInput.hpp"
#include "synth/osc/OscServer.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::atomic<bool> gRunning{true};

void handleSignal(int /*signal*/) {
    gRunning.store(false);
}

std::filesystem::path defaultModulePath() {
#if defined(_WIN32)
    return std::filesystem::path("build/modules/lowpass_module.dll");
#elif defined(__APPLE__)
    return std::filesystem::path("build/modules/liblowpass_module.dylib");
#else
    return std::filesystem::path("build/modules/liblowpass_module.so");
#endif
}

struct RuntimeConfig {
    std::filesystem::path modulePath = defaultModulePath();
    double sampleRate = 48000.0;
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
    std::uint16_t oscPort = 9000;
};

RuntimeConfig parseArgs(int argc, char** argv) {
    RuntimeConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--module" && i + 1 < argc) {
            config.modulePath = argv[++i];
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            config.sampleRate = std::stod(argv[++i]);
        } else if (arg == "--channels" && i + 1 < argc) {
            config.channels = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--buffer" && i + 1 < argc) {
            config.framesPerBuffer = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--osc-port" && i + 1 < argc) {
            config.oscPort = static_cast<std::uint16_t>(std::stoul(argv[++i]));
        }
    }

    return config;
}

void handleMidiMessage(const synth::midi::MidiMessage& message,
                       synth::audio::AudioEngine& audioEngine,
                       synth::dsp::ModuleHost& moduleHost) {
    if (message.size < 2) {
        return;
    }

    const std::uint8_t status = message.bytes[0] & 0xF0;
    const int data1 = message.bytes[1];
    const int data2 = message.size > 2 ? message.bytes[2] : 0;

    switch (status) {
        case 0x90: {
            if (data2 == 0) {
                audioEngine.synth().noteOff(data1);
            } else {
                audioEngine.synth().noteOn(data1, static_cast<float>(data2) / 127.0f);
            }
            break;
        }
        case 0x80:
            audioEngine.synth().noteOff(data1);
            break;
        case 0xB0: {
            const float normalized = static_cast<float>(data2) / 127.0f;
            if (data1 == 7) {
                audioEngine.synth().setMasterGain(normalized);
            } else if (data1 == 74) {
                const float cutoff = 20.0f + normalized * 19980.0f;
                moduleHost.setParameter("cutoff", cutoff);
            } else if (data1 == 71) {
                moduleHost.setParameter("resonance", normalized);
            }
            break;
        }
        default:
            break;
    }
}

void handleOscMessage(const synth::osc::OscMessage& message,
                      synth::audio::AudioEngine& audioEngine,
                      synth::dsp::ModuleHost& moduleHost) {
    if (message.address == "/noteOn") {
        if (!message.ints.empty()) {
            const int note = message.ints.front();
            float velocity = 1.0f;
            if (!message.floats.empty()) {
                velocity = message.floats.front();
            }
            audioEngine.synth().noteOn(note, velocity);
        }
    } else if (message.address == "/noteOff") {
        if (!message.ints.empty()) {
            audioEngine.synth().noteOff(message.ints.front());
        }
    } else if (message.address == "/cutoff") {
        if (!message.floats.empty()) {
            moduleHost.setParameter("cutoff", message.floats.front());
        }
    } else if (message.address == "/gain") {
        if (!message.floats.empty()) {
            audioEngine.synth().setMasterGain(message.floats.front());
        }
    } else if (message.address == "/resonance") {
        if (!message.floats.empty()) {
            moduleHost.setParameter("resonance", message.floats.front());
        }
    }
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

    logger.info("Starting Synthesizer host");

    synth::dsp::ModuleHost moduleHost(logger);
    if (!moduleHost.load(config.modulePath)) {
        logger.warn("Starting without module processing. Build modules or pass --module <path>.");
    }

    synth::audio::AudioEngine audioEngine(logger, moduleHost);
    synth::interfaces::AudioConfig audioConfig;
    audioConfig.sampleRate = config.sampleRate;
    audioConfig.channels = config.channels;
    audioConfig.framesPerBuffer = config.framesPerBuffer;

    if (!audioEngine.start(audioConfig)) {
        logger.error("Could not start audio engine.");
        return 1;
    }

    synth::midi::MidiInput midiInput(logger);
    midiInput.start([&](const synth::midi::MidiMessage& message) {
        handleMidiMessage(message, audioEngine, moduleHost);
    });

    synth::osc::OscServer oscServer(logger);
    oscServer.start(config.oscPort, [&](const synth::osc::OscMessage& message) {
        handleOscMessage(message, audioEngine, moduleHost);
    });

    logger.info("Synth running. Press Ctrl+C to quit.");
    while (gRunning.load()) {
        moduleHost.reloadIfChanged();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    logger.info("Shutting down synth host...");
    oscServer.stop();
    midiInput.stop();
    audioEngine.stop();

    return 0;
}
