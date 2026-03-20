#include "synth/app/SynthController.hpp"
#include "synth/app/Robin.hpp"
#include "synth/core/Logger.hpp"
#include "synth/graph/LiveGraph.hpp"
#include "synth/interfaces/IAudioDriver.hpp"
#include "synth/io/MidiInput.hpp"

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

struct RobinTestAccess {
    static std::size_t assignmentCount(const Robin& robin) {
        return robin.voiceAssignments_.size();
    }

    static std::size_t heldNoteCount(const Robin& robin) {
        return robin.heldNotes_.size();
    }
};

}  // namespace synth::app

namespace synth::io {

struct MidiInputTestAccess {
    static void setCallback(MidiInput& midiInput, MidiMessageCallback callback) {
        midiInput.callback_ = std::move(callback);
    }
};

}  // namespace synth::io

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

void testQueuedRepeatedGlobalNoteRetainsLaterSamePitchInstance() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    driverPtr->forceRunning(true);

    synth::app::SynthControllerTestAccess::submitRealtimeCommandOrApply(
        controller,
        {synth::app::RealtimeCommandType::GlobalNoteOn, 0, 60, 1.0f});
    synth::app::SynthControllerTestAccess::submitRealtimeCommandOrApply(
        controller,
        {synth::app::RealtimeCommandType::GlobalNoteOn, 0, 60, 1.0f});

    const std::string repeatedNoteOnStateJson = controller.stateJson();
    expect(
        repeatedNoteOnStateJson.find("\"activeMidiNote\":60") != std::string::npos,
        "repeated same-pitch note-on keeps note active");

    synth::app::SynthControllerTestAccess::submitRealtimeCommandOrApply(
        controller,
        {synth::app::RealtimeCommandType::GlobalNoteOff, 0, 60, 0.0f});

    const std::string firstNoteOffStateJson = controller.stateJson();
    expect(
        firstNoteOffStateJson.find("\"activeMidiNote\":60") != std::string::npos,
        "first same-pitch note-off preserves later note instance");

    synth::app::SynthControllerTestAccess::submitRealtimeCommandOrApply(
        controller,
        {synth::app::RealtimeCommandType::GlobalNoteOff, 0, 60, 0.0f});

    const std::string secondNoteOffStateJson = controller.stateJson();
    expect(
        secondNoteOffStateJson.find("\"activeMidiNote\":-1") != std::string::npos,
        "second same-pitch note-off releases final note instance");
}

void testRobinRetainsIndependentAssignmentsForRepeatedSamePitchNotes() {
    synth::app::Robin robin;
    robin.configureStructure(1, 1, 2);

    robin.noteOn(60, 1.0f);
    robin.noteOn(60, 1.0f);
    expectEqual(
        synth::app::RobinTestAccess::assignmentCount(robin),
        static_cast<std::size_t>(1),
        "single-voice robin keeps only one active assignment");
    expectEqual(
        synth::app::RobinTestAccess::heldNoteCount(robin),
        static_cast<std::size_t>(2),
        "single-voice robin tracks both same-pitch note instances");

    robin.noteOff(60);
    expectEqual(
        synth::app::RobinTestAccess::assignmentCount(robin),
        static_cast<std::size_t>(1),
        "first same-pitch note-off preserves retriggered voice");
    expectEqual(
        synth::app::RobinTestAccess::heldNoteCount(robin),
        static_cast<std::size_t>(1),
        "first same-pitch note-off removes only the older held note");

    robin.noteOff(60);
    expectEqual(
        synth::app::RobinTestAccess::assignmentCount(robin),
        static_cast<std::size_t>(0),
        "robin releases final same-pitch assignment");
    expectEqual(
        synth::app::RobinTestAccess::heldNoteCount(robin),
        static_cast<std::size_t>(0),
        "robin releases final same-pitch held note");
}

void testMidiInputParsesMultipleMessagesPerPacketAndTransportStop() {
    synth::core::Logger logger(testLogDirectory());
    synth::io::MidiInput midiInput(logger);
    std::vector<synth::io::MidiMessage> messages;
    synth::io::MidiInputTestAccess::setCallback(
        midiInput,
        [&messages](const synth::io::MidiMessage& message) {
            messages.push_back(message);
        });

    const unsigned char repeatedNotePacket[] = {0x90, 60, 100, 0x80, 60, 0, 0x90, 60, 110};
    midiInput.handlePacketData(3, repeatedNotePacket, sizeof(repeatedNotePacket));

    expectEqual(messages.size(), static_cast<std::size_t>(3), "all MIDI messages in packet dispatched");
    expect(messages[0].type == synth::io::MidiMessageType::NoteOn, "first packet message is note-on");
    expectEqual(messages[0].sourceIndex, static_cast<std::uint32_t>(3), "first packet message source index");
    expectEqual(messages[0].noteNumber, 60, "first packet message note number");
    expect(messages[1].type == synth::io::MidiMessageType::NoteOff, "second packet message is note-off");
    expectEqual(messages[1].noteNumber, 60, "second packet message note number");
    expect(messages[2].type == synth::io::MidiMessageType::NoteOn, "third packet message is note-on");
    expectEqual(messages[2].noteNumber, 60, "third packet message note number");

    const unsigned char stopPacket[] = {0xFC};
    midiInput.handlePacketData(3, stopPacket, sizeof(stopPacket));
    expectEqual(messages.size(), static_cast<std::size_t>(4), "transport stop dispatches all-notes-off");
    expect(messages[3].type == synth::io::MidiMessageType::AllNotesOff, "stop packet becomes all-notes-off");

    const unsigned char mmcStopPacket[] = {0xF0, 0x7F, 0x7F, 0x06, 0x01, 0xF7};
    midiInput.handlePacketData(3, mmcStopPacket, sizeof(mmcStopPacket));
    expectEqual(messages.size(), static_cast<std::size_t>(5), "mmc stop dispatches all-notes-off");
    expect(messages[4].type == synth::io::MidiMessageType::AllNotesOff, "mmc stop becomes all-notes-off");
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
        controller.setParam("sources.robin.gain", 0.34, &errorMessage),
        "queued robin master gain update succeeds");
    expect(errorMessage.empty(), "no robin master gain error");

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

    expect(
        controller.setParam("sources.robin.oscillator.0.waveform", "triangle", &errorMessage),
        "queued robin master oscillator waveform update succeeds");
    expect(errorMessage.empty(), "no robin master oscillator waveform error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(refreshedStateJson.find("\"cutoffHz\":1200") != std::string::npos, "queued robin master cutoff flushed");
    expect(refreshedStateJson.find("\"amount\":0.45") != std::string::npos, "queued robin master env vcf amount flushed");
    expect(refreshedStateJson.find("\"releaseMs\":350") != std::string::npos, "queued robin master envelope release flushed");
    expect(
        refreshedStateJson.find("\"frequency\":400,\"gain\":0.34,\"transposeSemitones\":3") != std::string::npos,
        "queued robin master gain flushed");
    expect(refreshedStateJson.find("\"transposeSemitones\":3") != std::string::npos, "queued robin master transpose flushed");
    expect(refreshedStateJson.find("\"fineTuneCents\":17") != std::string::npos, "queued robin master fine tune flushed");
    expect(
        refreshedStateJson.find("\"frequencyValue\":333,\"waveform\":\"triangle\"") != std::string::npos,
        "queued robin master oscillator waveform flushed");
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

    expect(
        controller.setParam("sources.robin.lfo.waveform", "saw-up", &errorMessage),
        "queued robin lfo waveform update succeeds");
    expect(errorMessage.empty(), "no robin lfo waveform error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(
        refreshedStateJson.find(
            "\"lfo\":{\"enabled\":true,\"depth\":0.72,\"phaseSpreadDegrees\":210,"
            "\"polarityFlip\":false,\"unlinkedOutputs\":false,\"clockLinked\":false,"
            "\"tempoBpm\":120,\"rateMultiplier\":3,\"fixedFrequencyHz\":5.5,\"waveform\":\"saw-up\"")
            != std::string::npos,
        "queued robin lfo state flushed");
}

void testQueuedRobinSpreadParamsRefreshStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    driverPtr->forceRunning(true);

    std::string errorMessage;
    expect(
        controller.setParam("sources.robin.spread.0.enabled", 1.0, &errorMessage),
        "queued robin spread enabled update succeeds");
    expect(errorMessage.empty(), "no robin spread enabled error");

    expect(
        controller.setParam("sources.robin.spread.0.target", "osc-detune", &errorMessage),
        "queued robin spread target update succeeds");
    expect(errorMessage.empty(), "no robin spread target error");

    expect(
        controller.setParam("sources.robin.spread.0.algorithm", "random", &errorMessage),
        "queued robin spread algorithm update succeeds");
    expect(errorMessage.empty(), "no robin spread algorithm error");

    expect(
        controller.setParam("sources.robin.spread.0.depth", 0.8, &errorMessage),
        "queued robin spread depth update succeeds");
    expect(errorMessage.empty(), "no robin spread depth error");

    expect(
        controller.setParam("sources.robin.spread.0.start", -12.0, &errorMessage),
        "queued robin spread start update succeeds");
    expect(errorMessage.empty(), "no robin spread start error");

    expect(
        controller.setParam("sources.robin.spread.0.end", 9.0, &errorMessage),
        "queued robin spread end update succeeds");
    expect(errorMessage.empty(), "no robin spread end error");

    expect(
        controller.setParam("sources.robin.spread.0.seed", 91.0, &errorMessage),
        "queued robin spread seed update succeeds");
    expect(errorMessage.empty(), "no robin spread seed error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(
        refreshedStateJson.find(
            "\"spreadSlots\":[{\"index\":0,\"enabled\":true,\"target\":\"osc-detune\","
            "\"algorithm\":\"random\",\"depth\":0.8,\"start\":-12,\"end\":9,\"seed\":91}")
            != std::string::npos,
        "queued robin spread state flushed");
}

void testQueuedTestAndMixerFxParamsRefreshStateWhileRunning() {
    synth::app::RuntimeConfig config;
    config.logDirectory = testLogDirectory();

    auto driver = std::make_unique<FakeAudioDriver>(makeTestDevices());
    auto* driverPtr = driver.get();
    synth::app::SynthController controller(config, std::move(driver));
    expect(controller.initialize(), "controller initializes");

    driverPtr->forceRunning(true);

    std::string errorMessage;
    expect(
        controller.setParam("sources.test.active", 0.0, &errorMessage),
        "queued test active update succeeds");
    expect(errorMessage.empty(), "no test active error");

    expect(
        controller.setParam("sources.test.midiEnabled", 0.0, &errorMessage),
        "queued test midi update succeeds");
    expect(errorMessage.empty(), "no test midi error");

    expect(
        controller.setParam("sources.test.frequency", 330.0, &errorMessage),
        "queued test frequency update succeeds");
    expect(errorMessage.empty(), "no test frequency error");

    expect(
        controller.setParam("sources.test.gain", 0.65, &errorMessage),
        "queued test gain update succeeds");
    expect(errorMessage.empty(), "no test gain error");

    expect(
        controller.setParam("sources.test.envelope.releaseMs", 640.0, &errorMessage),
        "queued test envelope release update succeeds");
    expect(errorMessage.empty(), "no test envelope release error");

    expect(
        controller.setParam("sources.test.output.0", 0.0, &errorMessage),
        "queued test output update succeeds");
    expect(errorMessage.empty(), "no test output error");

    expect(
        controller.setParam("sources.test.waveform", "square", &errorMessage),
        "queued test waveform update succeeds");
    expect(errorMessage.empty(), "no test waveform error");

    expect(
        controller.setParam("sourceMixer.test.enabled", 1.0, &errorMessage),
        "queued source mixer enabled update succeeds");
    expect(errorMessage.empty(), "no source mixer enabled error");

    expect(
        controller.setParam("sourceMixer.test.routeTarget", "fx", &errorMessage),
        "queued source mixer route target update succeeds");
    expect(errorMessage.empty(), "no source mixer route target error");

    expect(
        controller.setParam("processors.fx.saturator.enabled", 1.0, &errorMessage),
        "queued saturator enabled update succeeds");
    expect(errorMessage.empty(), "no saturator enabled error");

    expect(
        controller.setParam("processors.fx.saturator.inputLevel", 1.4, &errorMessage),
        "queued saturator input update succeeds");
    expect(errorMessage.empty(), "no saturator input error");

    expect(
        controller.setParam("processors.fx.saturator.outputLevel", 0.8, &errorMessage),
        "queued saturator output update succeeds");
    expect(errorMessage.empty(), "no saturator output error");

    expect(
        controller.setParam("processors.fx.sidechain.enabled", 1.0, &errorMessage),
        "queued sidechain enabled update succeeds");
    expect(errorMessage.empty(), "no sidechain enabled error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(
        refreshedStateJson.find(
            "\"test\":{\"available\":true,\"implemented\":true,\"enabled\":true,\"level\":0.15,\"routeTarget\":\"fx\"}")
            != std::string::npos,
        "queued source mixer state flushed");
    expect(
        refreshedStateJson.find(
            "\"test\":{\"implemented\":true,\"playable\":true,\"active\":false,\"midiEnabled\":false,"
            "\"frequency\":330,\"gain\":0.65,\"waveform\":\"square\"")
            != std::string::npos,
        "queued test source state flushed");
    expect(refreshedStateJson.find("\"releaseMs\":640") != std::string::npos, "queued test envelope release flushed");
    expect(refreshedStateJson.find("\"outputs\":[false,true]") != std::string::npos, "queued test outputs flushed");
    expect(
        refreshedStateJson.find("\"saturator\":{\"enabled\":true,\"inputLevel\":1.4,\"outputLevel\":0.8}") != std::string::npos,
        "queued saturator state flushed");
    expect(
        refreshedStateJson.find("\"sidechain\":{\"enabled\":true}") != std::string::npos,
        "queued sidechain state flushed");
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

    expect(
        controller.setParam("sources.robin.voice.0.oscillator.0.waveform", "noise", &errorMessage),
        "queued robin voice oscillator waveform update succeeds");
    expect(errorMessage.empty(), "no robin voice oscillator waveform error");

    const std::string refreshedStateJson = controller.stateJson();
    expect(refreshedStateJson.find("\"linkedToMaster\":false") != std::string::npos, "voice remains unlinked in state");
    expect(refreshedStateJson.find("\"frequency\":555") != std::string::npos, "queued robin voice frequency flushed");
    expect(refreshedStateJson.find("\"gain\":0.42") != std::string::npos, "queued robin voice gain flushed");
    expect(refreshedStateJson.find("\"resonance\":1.2") != std::string::npos, "queued robin voice resonance flushed");
    expect(refreshedStateJson.find("\"amount\":0.33") != std::string::npos, "queued robin voice env vcf amount flushed");
    expect(refreshedStateJson.find("\"releaseMs\":420") != std::string::npos, "queued robin voice envelope release flushed");
    expect(
        refreshedStateJson.find("\"frequencyValue\":777,\"waveform\":\"noise\"") != std::string::npos,
        "queued robin voice oscillator waveform flushed");
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
        {"repeated same-pitch global notes retain later instance", testQueuedRepeatedGlobalNoteRetainsLaterSamePitchInstance},
        {"robin keeps independent assignments for repeated same-pitch notes", testRobinRetainsIndependentAssignmentsForRepeatedSamePitchNotes},
        {"midi input parses multi-message packets and transport stop", testMidiInputParsesMultipleMessagesPerPacketAndTransportStop},
        {"queued robin master params refresh state while running", testQueuedRobinMasterParamsRefreshStateWhileRunning},
        {"queued robin lfo params refresh state while running", testQueuedRobinLfoParamsRefreshStateWhileRunning},
        {"queued robin spread params refresh state while running", testQueuedRobinSpreadParamsRefreshStateWhileRunning},
        {"queued test and mixer/fx params refresh state while running", testQueuedTestAndMixerFxParamsRefreshStateWhileRunning},
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
