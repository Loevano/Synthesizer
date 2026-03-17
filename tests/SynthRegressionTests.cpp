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
