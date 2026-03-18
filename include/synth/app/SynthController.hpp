#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "synth/app/ControllerCommands.hpp"
#include "synth/app/InstrumentState.hpp"
#include "synth/app/Robin.hpp"
#include "synth/app/TestSynth.hpp"
#include "synth/core/Logger.hpp"
#include "synth/core/CrashDiagnostics.hpp"
#include "synth/graph/FxRackNode.hpp"
#include "synth/graph/LiveGraph.hpp"
#include "synth/graph/OutputMixerNode.hpp"
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
    std::filesystem::path logDirectory;
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
    friend struct SynthControllerTestAccess;

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
    RealtimeParamResult tryEnqueueRealtimeNumericParam(std::string_view path, double value, std::string* errorMessage);
    RealtimeParamResult tryEnqueueRealtimeStringParam(std::string_view path, std::string_view value, std::string* errorMessage);
    void submitRealtimeCommandOrApply(RealtimeCommand command);
    void enqueueRealtimeCommand(RealtimeCommand command);
    void drainRealtimeCommandsLocked();
    void applySnapshotRealtimeCommandLocked(const RealtimeCommand& command);
    void applyRealtimeCommandLocked(const RealtimeCommand& command);
    std::string buildStateJsonLocked() const;
    void markStateSnapshotDirty() const;
    void buildLiveGraphLocked();
    void buildDefaultStateLocked();
    void resizeScaffoldStateLocked();
    void syncTestSourceLocked();
    void syncRenderStateFromSnapshotLocked();
    void syncOutputDeviceSelectionLocked(const std::vector<interfaces::OutputDeviceInfo>& outputDevices);
    void syncMidiRoutesLocked();
    MidiSourceRouteState* findMidiRouteLocked(std::uint32_t sourceIndex);
    const MidiSourceRouteState* findMidiRouteLocked(std::uint32_t sourceIndex) const;
    MidiSourceRouteState* findRenderMidiRouteLocked(std::uint32_t sourceIndex);
    const MidiSourceRouteState* findRenderMidiRouteLocked(std::uint32_t sourceIndex) const;
    void syncOutputProcessorNodesLocked();
    void renderAudioLocked(float* output, std::uint32_t frames, std::uint32_t channels);
    bool applyOutputEngineConfig(std::optional<std::string> outputDeviceId,
                                 std::optional<std::uint32_t> outputChannels,
                                 std::string* errorMessage);
    void applyRobinLevelLocked(float level);
    void applyTestLevelLocked(float level);
    void reconfigureStructureLocked(std::uint32_t voiceCount, std::uint32_t oscillatorsPerVoice);
    void handleNoteOnLocked(int noteNumber, float velocity);
    void handleNoteOffLocked(int noteNumber);
    void handleMidiNoteOnLocked(std::uint32_t sourceIndex, int noteNumber, float velocity);
    void handleMidiNoteOffLocked(std::uint32_t sourceIndex, int noteNumber);
    void handleTestNoteOnLocked(int noteNumber, float velocity);
    void handleTestNoteOffLocked(int noteNumber);

    RuntimeConfig config_;
    core::Logger logger_;
    core::CrashDiagnostics crashDiagnostics_;
    std::unique_ptr<interfaces::IAudioDriver> driver_;
    std::unique_ptr<io::MidiInput> midiInput_;
    std::unique_ptr<io::OscServer> oscServer_;
    Robin robin_;
    TestSynth test_;
    Robin renderRobin_;
    TestSynth renderTest_;
    graph::FxRackNode fxRackNode_;
    graph::OutputMixerNode outputMixerNode_;
    graph::LiveGraph liveGraph_;
    SourceMixerSlotState robinMixerState_{true, true, false, 0.15f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState testMixerState_{true, true, true, 0.15f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState decorMixerState_{true, false, false, 0.0f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState piecesMixerState_{true, false, false, 0.0f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState renderRobinMixerState_{true, true, false, 0.15f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState renderTestMixerState_{true, true, true, 0.15f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState renderDecorMixerState_{true, false, false, 0.0f, SourceMixerSlotState::RouteTarget::Dry};
    SourceMixerSlotState renderPiecesMixerState_{true, false, false, 0.0f, SourceMixerSlotState::RouteTarget::Dry};
    std::vector<OutputMixerChannelState> outputMixerChannels_;
    std::vector<OutputMixerChannelState> renderOutputMixerChannels_;
    PlaceholderSourceState decorState_{false, true, 0};
    PlaceholderSourceState piecesState_{false, true, 0};
    SaturatorState saturatorState_;
    ChorusState chorusState_;
    SidechainState sidechainState_;
    std::vector<MidiSourceRouteState> midiSourceRoutes_;
    std::vector<MidiSourceRouteState> renderMidiSourceRoutes_;
    std::string audioBackendName_ = "Unknown";
    std::string outputDeviceName_ = "Unknown";
    std::uint16_t oscPort_ = 9000;
    bool midiEnabled_ = false;
    bool oscEnabled_ = false;
    std::atomic<int> activeMidiNote_{-1};
    mutable std::mutex midiNoteStateMutex_;
    std::vector<int> heldMidiNotes_;
    bool debugRobinOscillatorParams_ = false;
    bool debugCrashBreadcrumbs_ = false;
    mutable std::mutex mutex_;
    mutable std::mutex realtimeCommandMutex_;
    std::deque<RealtimeCommand> pendingRealtimeCommands_;
    mutable std::mutex stateSnapshotMutex_;
    mutable std::string stateJsonCache_;
    mutable std::atomic<bool> stateSnapshotDirty_{true};
    bool initialized_ = false;
};

}  // namespace synth::app
