#include "synth/app/SynthController.hpp"
#include "synth/graph/LiveGraph.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace synth::app {

struct SynthControllerTestAccess {
    static void submitRealtimeCommandOrApply(SynthController& controller,
                                             RealtimeCommand command) {
        controller.submitRealtimeCommandOrApply(std::move(command));
    }
};

}  // namespace synth::app

namespace {

using synth::interfaces::AudioCallback;
using synth::interfaces::AudioConfig;
using synth::interfaces::IAudioDriver;
using synth::interfaces::OutputDeviceInfo;

class FakeAudioDriver final : public IAudioDriver {
public:
    explicit FakeAudioDriver(std::vector<OutputDeviceInfo> devices)
        : devices_(std::move(devices)) {}

    bool start(const AudioConfig& config, AudioCallback callback) override {
        running_ = true;
        lastConfig_ = config;
        callback_ = std::move(callback);
        return true;
    }

    void stop() override {
        running_ = false;
        callback_ = nullptr;
    }

    bool isRunning() const override {
        return running_;
    }

    std::vector<OutputDeviceInfo> availableOutputDevices() const override {
        return devices_;
    }

    const AudioConfig& lastConfig() const {
        return lastConfig_;
    }

    void forceRunning(bool running) {
        running_ = running;
    }

private:
    std::vector<OutputDeviceInfo> devices_;
    AudioConfig lastConfig_{};
    AudioCallback callback_;
    bool running_ = false;
};

struct TestCase {
    const char* name;
    std::function<void()> run;
};

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        fail(std::string(message));
    }
}

void expectEqual(std::string_view actual, std::string_view expected, std::string_view label) {
    if (actual != expected) {
        fail(std::string(label) + ": expected '" + std::string(expected) + "', got '" + std::string(actual) + "'");
    }
}

template <typename T>
requires std::is_arithmetic_v<T>
void expectEqual(T actual, T expected, std::string_view label) {
    if (actual != expected) {
        fail(std::string(label) + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

void expectNear(float actual, float expected, float epsilon, std::string_view label) {
    if (std::fabs(actual - expected) > epsilon) {
        fail(std::string(label) + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

std::filesystem::path testLogDirectory() {
    return std::filesystem::temp_directory_path() / "synth-regression-tests";
}

std::vector<OutputDeviceInfo> makeTestDevices() {
    return {
        {"device-a", "Device A", 8, true},
        {"device-b", "Device B", 2, false},
    };
}

std::unique_ptr<IAudioDriver> makeFakeDriver() {
    return std::make_unique<FakeAudioDriver>(makeTestDevices());
}

void testControllerInitializesFromInjectedDriver() {
    synth::app::RuntimeConfig config;
    config.channels = 6;
    config.logDirectory = testLogDirectory();

    synth::app::SynthController controller(config, makeFakeDriver());
    expect(controller.initialize(), "controller initializes");
    expectEqual(controller.config().outputDeviceId, std::string("device-a"), "default selected device");
    expectEqual(controller.config().channels, static_cast<std::uint32_t>(6), "channels preserved on larger device");

    const std::string stateJson = controller.stateJson();
    expect(stateJson.find("\"availableOutputDevices\"") != std::string::npos, "output devices included in state");
    expect(stateJson.find("\"id\":\"device-a\"") != std::string::npos, "device-a present in state");
    expect(stateJson.find("\"id\":\"device-b\"") != std::string::npos, "device-b present in state");
}

void testControllerOutputDeviceSelectionClampsChannels() {
    synth::app::RuntimeConfig config;
    config.channels = 6;
    config.logDirectory = testLogDirectory();

    synth::app::SynthController controller(config, makeFakeDriver());
    expect(controller.initialize(), "controller initializes");

    std::string errorMessage;
    expect(controller.setParam("engine.outputDeviceId", "device-b", &errorMessage), "output device selection succeeds");
    expect(errorMessage.empty(), "no output device selection error");
    expectEqual(controller.config().outputDeviceId, std::string("device-b"), "selected device updated");
    expectEqual(controller.config().channels, static_cast<std::uint32_t>(2), "channels clamped to selected device");
}

void testControllerOutputChannelSelectionClampsToDeviceMaximum() {
    synth::app::RuntimeConfig config;
    config.channels = 2;
    config.logDirectory = testLogDirectory();

    synth::app::SynthController controller(config, makeFakeDriver());
    expect(controller.initialize(), "controller initializes");

    std::string errorMessage;
    expect(controller.setParam("engine.outputChannels", 99.0, &errorMessage), "output channel update succeeds");
    expect(errorMessage.empty(), "no output channel update error");
    expectEqual(controller.config().channels, static_cast<std::uint32_t>(8), "channels clamped to device maximum");
}

void testStateJsonRefreshesAfterParamMutation() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    synth::app::SynthController controller(config, makeFakeDriver());
    expect(controller.initialize(), "controller initializes");

    const std::string initialStateJson = controller.stateJson();
    expect(initialStateJson.find("\"speedHz\":0.25") != std::string::npos, "initial chorus speed in state");

    std::string errorMessage;
    expect(
        controller.setParam("processors.fx.chorus.speedHz", 1.5, &errorMessage),
        "chorus speed update succeeds");
    expect(errorMessage.empty(), "no chorus speed update error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(refreshedStateJson.find("\"speedHz\":1.5") != std::string::npos, "updated chorus speed in state");
}

void testQueuedRealtimeParamRefreshesStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    const std::string initialStateJson = controller.stateJson();
    expect(
        initialStateJson.find("\"chorus\":{\"enabled\":false,\"depth\":0.5") != std::string::npos,
        "initial chorus state cached");

    driverPtr->forceRunning(true);

    std::string errorMessage;
    expect(
        controller.setParam("processors.fx.chorus.depth", 0.9, &errorMessage),
        "queued chorus depth update succeeds");
    expect(errorMessage.empty(), "no queued chorus depth update error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(
        refreshedStateJson.find("\"chorus\":{\"enabled\":false,\"depth\":0.9") != std::string::npos,
        "queued chorus depth flushed into state");
}

void testQueuedGlobalNoteRefreshesStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    driverPtr->forceRunning(true);

    synth::app::SynthControllerTestAccess::submitRealtimeCommandOrApply(
        controller,
        {synth::app::RealtimeCommandType::GlobalNoteOn, 0, 64, 1.0f});

    const std::string noteOnStateJson = controller.stateJson();
    expect(noteOnStateJson.find("\"activeMidiNote\":64") != std::string::npos, "queued note-on flushed into state");

    synth::app::SynthControllerTestAccess::submitRealtimeCommandOrApply(
        controller,
        {synth::app::RealtimeCommandType::GlobalNoteOff, 0, 64, 0.0f});

    const std::string noteOffStateJson = controller.stateJson();
    expect(noteOffStateJson.find("\"activeMidiNote\":-1") != std::string::npos, "queued note-off flushed into state");
}

void testQueuedRobinMasterParamsRefreshStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    driverPtr->forceRunning(true);

    std::string errorMessage;
    expect(
        controller.setParam("sources.robin.vcf.cutoffHz", 1200.0, &errorMessage),
        "queued robin master vcf cutoff update succeeds");
    expect(errorMessage.empty(), "no robin master vcf cutoff error");

    expect(
        controller.setParam("sources.robin.envVcf.amount", 0.45, &errorMessage),
        "queued robin master env vcf amount update succeeds");
    expect(errorMessage.empty(), "no robin master env vcf amount error");

    expect(
        controller.setParam("sources.robin.envelope.releaseMs", 350.0, &errorMessage),
        "queued robin master envelope release update succeeds");
    expect(errorMessage.empty(), "no robin master envelope release error");

    expect(
        controller.setParam("sources.robin.transposeSemitones", 3.0, &errorMessage),
        "queued robin master transpose update succeeds");
    expect(errorMessage.empty(), "no robin master transpose error");

    expect(
        controller.setParam("sources.robin.fineTuneCents", 17.0, &errorMessage),
        "queued robin master fine tune update succeeds");
    expect(errorMessage.empty(), "no robin master fine tune error");

    expect(
        controller.setParam("sources.robin.oscillator.0.gain", 0.61, &errorMessage),
        "queued robin master oscillator gain update succeeds");
    expect(errorMessage.empty(), "no robin master oscillator gain error");

    expect(
        controller.setParam("sources.robin.oscillator.0.relative", 0.0, &errorMessage),
        "queued robin master oscillator relative update succeeds");
    expect(errorMessage.empty(), "no robin master oscillator relative error");

    expect(
        controller.setParam("sources.robin.oscillator.0.frequency", 333.0, &errorMessage),
        "queued robin master oscillator frequency update succeeds");
    expect(errorMessage.empty(), "no robin master oscillator frequency error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(refreshedStateJson.find("\"cutoffHz\":1200") != std::string::npos, "queued robin master cutoff flushed");
    expect(refreshedStateJson.find("\"amount\":0.45") != std::string::npos, "queued robin master env vcf amount flushed");
    expect(refreshedStateJson.find("\"releaseMs\":350") != std::string::npos, "queued robin master envelope release flushed");
    expect(refreshedStateJson.find("\"transposeSemitones\":3") != std::string::npos, "queued robin master transpose flushed");
    expect(refreshedStateJson.find("\"fineTuneCents\":17") != std::string::npos, "queued robin master fine tune flushed");
    expect(refreshedStateJson.find("\"frequencyValue\":333") != std::string::npos, "queued robin master oscillator frequency flushed");
    expect(refreshedStateJson.find("\"gain\":0.61") != std::string::npos, "queued robin master oscillator gain flushed");
}

void testQueuedRobinLfoParamsRefreshStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    driverPtr->forceRunning(true);

    std::string errorMessage;
    expect(
        controller.setParam("sources.robin.lfo.enabled", 1.0, &errorMessage),
        "queued robin lfo enabled update succeeds");
    expect(errorMessage.empty(), "no robin lfo enabled error");

    expect(
        controller.setParam("sources.robin.lfo.depth", 0.72, &errorMessage),
        "queued robin lfo depth update succeeds");
    expect(errorMessage.empty(), "no robin lfo depth error");

    expect(
        controller.setParam("sources.robin.lfo.phaseSpreadDegrees", 210.0, &errorMessage),
        "queued robin lfo phase spread update succeeds");
    expect(errorMessage.empty(), "no robin lfo phase spread error");

    expect(
        controller.setParam("sources.robin.lfo.clockLinked", 0.0, &errorMessage),
        "queued robin lfo clock linked update succeeds");
    expect(errorMessage.empty(), "no robin lfo clock linked error");

    expect(
        controller.setParam("sources.robin.lfo.rateMultiplier", 3.0, &errorMessage),
        "queued robin lfo rate multiplier update succeeds");
    expect(errorMessage.empty(), "no robin lfo rate multiplier error");

    expect(
        controller.setParam("sources.robin.lfo.fixedFrequencyHz", 5.5, &errorMessage),
        "queued robin lfo fixed frequency update succeeds");
    expect(errorMessage.empty(), "no robin lfo fixed frequency error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(
        refreshedStateJson.find(
            "\"lfo\":{\"enabled\":true,\"depth\":0.72,\"phaseSpreadDegrees\":210,"
            "\"polarityFlip\":false,\"unlinkedOutputs\":false,\"clockLinked\":false,"
            "\"tempoBpm\":120,\"rateMultiplier\":3,\"fixedFrequencyHz\":5.5")
            != std::string::npos,
        "queued robin lfo state flushed");
}

void testQueuedRobinVoiceParamsRefreshStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    std::string errorMessage;
    expect(
        controller.setParam("sources.robin.voice.0.linkedToMaster", 0.0, &errorMessage),
        "voice unlink succeeds");
    expect(errorMessage.empty(), "no voice unlink error");

    driverPtr->forceRunning(true);

    expect(
        controller.setParam("sources.robin.voice.0.gain", 0.42, &errorMessage),
        "queued robin voice gain update succeeds");
    expect(errorMessage.empty(), "no robin voice gain error");

    expect(
        controller.setParam("sources.robin.voice.0.frequency", 555.0, &errorMessage),
        "queued robin voice frequency update succeeds");
    expect(errorMessage.empty(), "no robin voice frequency error");

    expect(
        controller.setParam("sources.robin.voice.0.vcf.resonance", 1.2, &errorMessage),
        "queued robin voice vcf resonance update succeeds");
    expect(errorMessage.empty(), "no robin voice vcf resonance error");

    expect(
        controller.setParam("sources.robin.voice.0.envVcf.amount", 0.33, &errorMessage),
        "queued robin voice env vcf amount update succeeds");
    expect(errorMessage.empty(), "no robin voice env vcf amount error");

    expect(
        controller.setParam("sources.robin.voice.0.envelope.releaseMs", 420.0, &errorMessage),
        "queued robin voice envelope release update succeeds");
    expect(errorMessage.empty(), "no robin voice envelope release error");

    expect(
        controller.setParam("sources.robin.voice.0.oscillator.0.gain", 0.27, &errorMessage),
        "queued robin voice oscillator gain update succeeds");
    expect(errorMessage.empty(), "no robin voice oscillator gain error");

    expect(
        controller.setParam("sources.robin.voice.0.oscillator.0.relative", 0.0, &errorMessage),
        "queued robin voice oscillator relative update succeeds");
    expect(errorMessage.empty(), "no robin voice oscillator relative error");

    expect(
        controller.setParam("sources.robin.voice.0.oscillator.0.frequency", 777.0, &errorMessage),
        "queued robin voice oscillator frequency update succeeds");
    expect(errorMessage.empty(), "no robin voice oscillator frequency error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(refreshedStateJson.find("\"linkedToMaster\":false") != std::string::npos, "voice remains unlinked in state");
    expect(refreshedStateJson.find("\"frequency\":555") != std::string::npos, "queued robin voice frequency flushed");
    expect(refreshedStateJson.find("\"gain\":0.42") != std::string::npos, "queued robin voice gain flushed");
    expect(refreshedStateJson.find("\"resonance\":1.2") != std::string::npos, "queued robin voice resonance flushed");
    expect(refreshedStateJson.find("\"amount\":0.33") != std::string::npos, "queued robin voice env vcf amount flushed");
    expect(refreshedStateJson.find("\"releaseMs\":420") != std::string::npos, "queued robin voice envelope release flushed");
    expect(refreshedStateJson.find("\"frequencyValue\":777") != std::string::npos, "queued robin voice oscillator frequency flushed");
    expect(refreshedStateJson.find("\"gain\":0.27") != std::string::npos, "queued robin voice oscillator gain flushed");
}

void testLiveGraphDryFxRenderOrder() {
    synth::graph::LiveGraph graph;

    graph.addSourceNode({
        "dry-source",
        nullptr,
        nullptr,
        nullptr,
        []() { return synth::graph::LiveGraph::SourceRenderTarget::Dry; },
        [](float* output, std::uint32_t frames, std::uint32_t channels) {
            const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
            for (std::size_t index = 0; index < sampleCount; ++index) {
                output[index] += 1.0f;
            }
        },
        nullptr,
        nullptr,
    });

    graph.addSourceNode({
        "fx-source",
        nullptr,
        nullptr,
        nullptr,
        []() { return synth::graph::LiveGraph::SourceRenderTarget::FxBus; },
        [](float* output, std::uint32_t frames, std::uint32_t channels) {
            const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
            for (std::size_t index = 0; index < sampleCount; ++index) {
                output[index] += 2.0f;
            }
        },
        nullptr,
        nullptr,
    });

    graph.addOutputProcessorNode({
        "fx-gain",
        synth::graph::LiveGraph::OutputProcessTarget::FxBus,
        nullptr,
        [](float* output, std::uint32_t frames, std::uint32_t channels) {
            const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
            for (std::size_t index = 0; index < sampleCount; ++index) {
                output[index] *= 10.0f;
            }
        },
    });

    graph.addOutputProcessorNode({
        "main-gain",
        synth::graph::LiveGraph::OutputProcessTarget::Main,
        nullptr,
        [](float* output, std::uint32_t frames, std::uint32_t channels) {
            const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
            for (std::size_t index = 0; index < sampleCount; ++index) {
                output[index] *= 2.0f;
            }
        },
    });

    std::vector<float> output(8, 0.0f);
    graph.render(output.data(), 4, 2);

    for (float sample : output) {
        expectNear(sample, 42.0f, 0.0001f, "dry/fx render order");
    }
}

}  // namespace

int main() {
    const std::vector<TestCase> tests{
        {"controller initializes from injected driver", testControllerInitializesFromInjectedDriver},
        {"controller output device selection clamps channels", testControllerOutputDeviceSelectionClampsChannels},
        {"controller output channels clamp to selected device maximum", testControllerOutputChannelSelectionClampsToDeviceMaximum},
        {"state json refreshes after param mutation", testStateJsonRefreshesAfterParamMutation},
        {"queued realtime param refreshes state while running", testQueuedRealtimeParamRefreshesStateWhileRunning},
        {"queued global note refreshes state while running", testQueuedGlobalNoteRefreshesStateWhileRunning},
        {"queued robin master params refresh state while running", testQueuedRobinMasterParamsRefreshStateWhileRunning},
        {"queued robin lfo params refresh state while running", testQueuedRobinLfoParamsRefreshStateWhileRunning},
        {"queued robin voice params refresh state while running", testQueuedRobinVoiceParamsRefreshStateWhileRunning},
        {"live graph render order respects dry and fx routing", testLiveGraphDryFxRenderOrder},
    };

    std::size_t failedCount = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failedCount;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failedCount;
            std::cerr << "[FAIL] " << test.name << ": unknown error\n";
        }
    }

    if (failedCount > 0) {
        std::cerr << failedCount << " test(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << tests.size() << " test(s) passed.\n";
    return EXIT_SUCCESS;
}
