#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "synth/core/Logger.hpp"
#include "synth/core/CrashDiagnostics.hpp"
#include "synth/graph/FxRackNode.hpp"
#include "synth/graph/LiveGraph.hpp"
#include "synth/graph/OutputMixerNode.hpp"
#include "synth/graph/RobinSourceNode.hpp"
#include "synth/graph/TestSourceNode.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

namespace synth::io {
class MidiInput;
class OscServer;
}

namespace synth::app {

struct RuntimeConfig {
    double sampleRate = 48000.0;
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
    std::string outputDeviceId;
    std::uint32_t voiceCount = 8;
    std::uint32_t oscillatorsPerVoice = 6;
    float frequency = 400.0f;
    float gain = 0.15f;
    dsp::Waveform waveform = dsp::Waveform::Sine;
    std::filesystem::path logDirectory;
};

struct OscillatorState {
    bool enabled = false;
    float gain = 1.0f;
    bool relativeToVoice = true;
    float frequencyValue = 1.0f;
    dsp::Waveform waveform = dsp::Waveform::Sine;
};

struct LfoState {
    bool enabled = false;
    float depth = 0.5f;
    float phaseSpreadDegrees = 0.0f;
    bool polarityFlip = false;
    bool unlinkedOutputs = false;
    bool clockLinked = false;
    float tempoBpm = 120.0f;
    float rateMultiplier = 1.0f;
    float fixedFrequencyHz = 2.0f;
    dsp::LfoWaveform waveform = dsp::LfoWaveform::Sine;
};

struct SourceMixerSlotState {
    enum class RouteTarget : std::uint8_t {
        Dry,
        Fx,
    };

    bool available = false;
    bool implemented = false;
    bool enabled = false;
    float level = 0.0f;
    RouteTarget routeTarget = RouteTarget::Dry;
};

struct OutputEqState {
    float lowDb = 0.0f;
    float midDb = 0.0f;
    float highDb = 0.0f;
};

struct OutputMixerChannelState {
    float level = 1.0f;
    float delayMs = 0.0f;
    OutputEqState eq;
};

struct PlaceholderSourceState {
    bool implemented = false;
    bool playable = true;
    std::uint32_t voiceCount = 0;
};

struct EnvelopeState {
    float attackMs = 10.0f;
    float decayMs = 80.0f;
    float sustain = 0.8f;
    float releaseMs = 200.0f;
};

struct RobinPitchState {
    float transposeSemitones = 0.0f;
    float fineTuneCents = 0.0f;
};

struct RobinVcfState {
    float cutoffHz = 18000.0f;
    float resonance = 0.707f;
};

struct RobinEnvVcfState {
    float attackMs = 20.0f;
    float decayMs = 250.0f;
    float sustain = 0.0f;
    float releaseMs = 220.0f;
    float amount = 0.0f;
};

struct VoiceState {
    bool active = false;
    bool linkedToMaster = true;
    float frequency = 400.0f;
    float gain = 1.0f;
    RobinVcfState vcf;
    RobinEnvVcfState envVcf;
    EnvelopeState envelope;
    std::vector<bool> outputs;
    std::vector<OscillatorState> oscillators;
};

struct TestSourceState {
    bool implemented = true;
    bool playable = true;
    bool active = false;
    bool midiEnabled = true;
    float frequency = 220.0f;
    float gain = 0.4f;
    dsp::Waveform waveform = dsp::Waveform::Sine;
    EnvelopeState envelope;
    std::vector<bool> outputs;
};

struct SaturatorState {
    bool enabled = false;
    float inputLevel = 1.0f;
    float outputLevel = 1.0f;
};

struct ChorusState {
    bool enabled = false;
    float depth = 0.5f;
    float speedHz = 0.25f;
    float phaseSpreadDegrees = 360.0f;
};

struct SidechainState {
    bool enabled = false;
};

struct MidiSourceRouteState {
    std::uint32_t index = 0;
    bool robin = true;
    bool test = true;
    bool decor = false;
    bool pieces = false;
};

struct RobinVoiceAssignment {
    int noteNumber = -1;
    std::uint32_t voiceIndex = 0;
};

enum class RoutingPreset {
    Forward,
    Backward,
    Random,
    RoundRobin,
    AllOutputs,
    Custom,
};

class SynthController {
public:
    explicit SynthController(
        RuntimeConfig config = {},
        std::unique_ptr<interfaces::IAudioDriver> driver = {});
    ~SynthController();

    bool initialize();
    bool startAudio();
    void stopAudio();
    bool isRunning() const;
    std::string stateJson() const;
    bool setParam(std::string_view path, double value, std::string* errorMessage);
    bool setParam(std::string_view path, std::string_view value, std::string* errorMessage);

    audio::Synth& synth();
    core::Logger& logger();
    core::CrashDiagnostics& crashDiagnostics();
    const RuntimeConfig& config() const;

private:
    static const char* waveformToString(dsp::Waveform waveform);
    static bool tryParseWaveform(std::string_view value, dsp::Waveform& waveform);
    static const char* lfoWaveformToString(dsp::LfoWaveform waveform);
    static bool tryParseLfoWaveform(std::string_view value, dsp::LfoWaveform& waveform);
    static const char* sourceRouteTargetToString(SourceMixerSlotState::RouteTarget routeTarget);
    static bool tryParseSourceRouteTarget(std::string_view value, SourceMixerSlotState::RouteTarget& routeTarget);
    static const char* routingPresetToString(RoutingPreset preset);
    static bool tryParseRoutingPreset(std::string_view value, RoutingPreset& preset);
    static const interfaces::OutputDeviceInfo* findOutputDevice(
        const std::vector<interfaces::OutputDeviceInfo>& outputDevices,
        std::string_view outputDeviceId);
    static std::string escapeJson(std::string_view value);
    static bool tryParseIndex(std::string_view value, std::uint32_t& index);
    static float midiNoteToFrequency(int noteNumber);
    void logRobinMasterOscillatorUpdateLocked(std::string_view path, std::string_view valueDescription);
    float tunedRobinFrequencyLocked(float baseFrequencyHz) const;
    void copyMasterStateToVoiceLocked(VoiceState& voice) const;
    void syncLinkedRobinVoicesLocked(bool syncFrequency = false);
    void syncRobinVoiceFrequencyLocked(std::uint32_t voiceIndex);
    void syncAllRobinVoiceFrequenciesLocked();
    void syncAssignedRobinVoiceFrequenciesLocked();

    void buildLiveGraphLocked();
    void buildDefaultStateLocked();
    void syncVoiceStateLocked(std::uint32_t voiceIndex, bool syncFrequency = true);
    void syncAllVoicesLocked();
    void syncLfoLocked();
    void syncRobinEnvelopeLocked();
    void resizeScaffoldStateLocked();
    void syncTestSourceLocked();
    void syncOutputDeviceSelectionLocked(const std::vector<interfaces::OutputDeviceInfo>& outputDevices);
    void syncMidiRoutesLocked();
    MidiSourceRouteState* findMidiRouteLocked(std::uint32_t sourceIndex);
    const MidiSourceRouteState* findMidiRouteLocked(std::uint32_t sourceIndex) const;
    void syncOutputProcessorNodesLocked();
    void renderAudioLocked(float* output, std::uint32_t frames, std::uint32_t channels);
    bool applyOutputEngineConfig(std::optional<std::string> outputDeviceId,
                                 std::optional<std::uint32_t> outputChannels,
                                 std::string* errorMessage);
    void applyRobinLevelLocked(float level);
    void applyTestLevelLocked(float level);
    void applyGlobalFrequencyLocked(float frequencyHz);
    void applyGlobalWaveformLocked(dsp::Waveform waveform);
    void applyRoutingPresetLocked(RoutingPreset preset);
    void resetRobinRoutingStateLocked();
    std::uint32_t computeNextRobinTriggerOutputLocked();
    void routeRobinVoiceToOutputLocked(std::uint32_t voiceIndex, std::uint32_t outputIndex);
    void routeRobinVoiceToAllOutputsLocked(std::uint32_t voiceIndex);
    std::uint32_t allocateRobinVoiceLocked();
    void reconfigureStructureLocked(std::uint32_t voiceCount, std::uint32_t oscillatorsPerVoice);
    void handleNoteOnLocked(int noteNumber, float velocity);
    void handleNoteOffLocked(int noteNumber);
    void handleMidiNoteOnLocked(std::uint32_t sourceIndex, int noteNumber, float velocity);
    void handleMidiNoteOffLocked(std::uint32_t sourceIndex, int noteNumber);
    void handleRobinNoteOnLocked(int noteNumber, float velocity);
    void handleRobinNoteOffLocked(int noteNumber);
    void handleTestNoteOnLocked(int noteNumber, float velocity);
    void handleTestNoteOffLocked(int noteNumber);

    RuntimeConfig config_;
    core::Logger logger_;
    core::CrashDiagnostics crashDiagnostics_;
    std::unique_ptr<interfaces::IAudioDriver> driver_;
    std::unique_ptr<io::MidiInput> midiInput_;
    std::unique_ptr<io::OscServer> oscServer_;
    graph::RobinSourceNode robinSource_;
    graph::TestSourceNode testSource_;
    graph::FxRackNode fxRackNode_;
    graph::OutputMixerNode outputMixerNode_;
    graph::LiveGraph liveGraph_;
    std::vector<VoiceState> voices_;
    std::vector<std::chrono::steady_clock::time_point> robinVoiceReleaseUntil_;
    std::vector<OscillatorState> masterOscillators_;
    RobinPitchState robinPitchState_;
    LfoState lfoState_;
    RoutingPreset routingPreset_ = RoutingPreset::Forward;
    SourceMixerSlotState robinMixerState_{true, true, false, 0.15f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState testMixerState_{true, true, true, 0.15f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState decorMixerState_{true, false, false, 0.0f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState piecesMixerState_{true, false, false, 0.0f, SourceMixerSlotState::RouteTarget::Dry};
    std::vector<OutputMixerChannelState> outputMixerChannels_;
    EnvelopeState robinEnvelopeState_;
    float robinMasterVoiceGain_ = 1.0f;
    RobinVcfState robinVcfState_;
    RobinEnvVcfState robinEnvVcfState_;
    TestSourceState testState_;
    PlaceholderSourceState decorState_{false, true, 0};
    PlaceholderSourceState piecesState_{false, true, 0};
    SaturatorState saturatorState_;
    ChorusState chorusState_;
    SidechainState sidechainState_;
    std::vector<MidiSourceRouteState> midiSourceRoutes_;
    std::string audioBackendName_ = "Unknown";
    std::string outputDeviceName_ = "Unknown";
    std::uint16_t oscPort_ = 9000;
    bool midiEnabled_ = false;
    bool oscEnabled_ = false;
    int activeMidiNote_ = -1;
    std::vector<int> heldMidiNotes_;
    std::uint32_t robinForwardOutputCursor_ = 0;
    std::uint32_t robinBackwardOutputCursor_ = 0;
    std::vector<std::uint32_t> robinRoundRobinPool_;
    std::optional<std::uint32_t> robinNextTriggerOutputIndex_;
    std::vector<RobinVoiceAssignment> robinVoiceAssignments_;
    std::uint32_t robinNextVoiceCursor_ = 0;
    std::minstd_rand robinRoutingRandom_{std::random_device{}()};
    bool autoActivatedVoice0_ = false;
    bool debugRobinOscillatorParams_ = false;
    bool debugCrashBreadcrumbs_ = false;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

}  // namespace synth::app
