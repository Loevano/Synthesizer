#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

#include "synth/app/Instrument.hpp"
#include "synth/app/InstrumentState.hpp"
#include "synth/graph/RobinSourceNode.hpp"

namespace synth::core {
class Logger;
}

namespace synth::app {

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

enum class RobinSpreadTarget {
    VcfCutoff,
    VcfResonance,
    EnvVcfAmount,
    EnvVcfAttack,
    EnvVcfDecay,
    EnvVcfRelease,
    AmpAttack,
    AmpDecay,
    AmpRelease,
    OscLevel,
    OscDetune,
};

enum class RobinSpreadAlgorithm {
    Linear,
    Random,
    Alternating,
};

struct RobinSpreadSlot {
    bool enabled = false;
    RobinSpreadTarget target = RobinSpreadTarget::VcfCutoff;
    RobinSpreadAlgorithm algorithm = RobinSpreadAlgorithm::Linear;
    float depth = 0.0f;
    float start = 0.0f;
    float end = 0.0f;
    std::uint32_t seed = 1;
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

struct RobinStateSnapshot {
    std::vector<VoiceState> voices;
    std::vector<OscillatorState> masterOscillators;
    RobinPitchState pitch;
    LfoState lfo;
    RoutingPreset routingPreset = RoutingPreset::Forward;
    EnvelopeState envelope;
    float masterVoiceGain = 1.0f;
    RobinVcfState vcf;
    RobinEnvVcfState envVcf;
    std::array<RobinSpreadSlot, 6> spreadSlots {{
        {false, RobinSpreadTarget::VcfCutoff, RobinSpreadAlgorithm::Linear, 0.0f, 0.0f, 0.0f, 17},
        {false, RobinSpreadTarget::OscDetune, RobinSpreadAlgorithm::Random, 0.0f, 0.0f, 0.0f, 37},
        {false, RobinSpreadTarget::AmpRelease, RobinSpreadAlgorithm::Alternating, 0.0f, 0.0f, 0.0f, 73},
        {false, RobinSpreadTarget::OscLevel, RobinSpreadAlgorithm::Linear, 0.0f, 0.0f, 0.0f, 101},
        {false, RobinSpreadTarget::EnvVcfAmount, RobinSpreadAlgorithm::Random, 0.0f, 0.0f, 0.0f, 131},
        {false, RobinSpreadTarget::AmpAttack, RobinSpreadAlgorithm::Alternating, 0.0f, 0.0f, 0.0f, 167},
    }};
    float baseFrequencyHz = 400.0f;
    std::uint32_t oscillatorsPerVoice = 6;
    std::uint32_t outputChannelCount = 2;
};

class Robin final : public Instrument {
public:
    Robin() = default;

    void setLogger(core::Logger* logger);
    void setDebugOscillatorParams(bool enabled);
    void configureStructure(std::uint32_t voiceCount,
                            std::uint32_t oscillatorsPerVoice,
                            std::uint32_t outputChannels);
    void clearAllNotes();
    void setBaseFrequency(float frequencyHz);
    float baseFrequencyHz() const;
    std::uint32_t voiceCount() const;
    std::uint32_t oscillatorsPerVoice() const;

    void prepare(double sampleRate, std::uint32_t outputChannels) override;
    void renderAdd(float* output,
                   std::uint32_t frames,
                   std::uint32_t channels,
                   bool enabled,
                   float level) override;
    void noteOn(int noteNumber, float velocity) override;
    void noteOff(int noteNumber) override;

    RealtimeParamResult applyNumericParam(const std::vector<std::string_view>& parts,
                                          double value,
                                          std::string* errorMessage) override;
    RealtimeParamResult applyStringParam(const std::vector<std::string_view>& parts,
                                         std::string_view value,
                                         std::string* errorMessage) override;
    RealtimeParamResult tryBuildRealtimeNumericCommand(const std::vector<std::string_view>& parts,
                                                       double value,
                                                       RealtimeCommand& command,
                                                       std::string* errorMessage) const override;
    RealtimeParamResult tryBuildRealtimeStringCommand(const std::vector<std::string_view>& parts,
                                                      std::string_view value,
                                                      RealtimeCommand& command,
                                                      std::string* errorMessage) const override;
    bool handlesRealtimeCommand(RealtimeCommandType type) const override;
    void applyRealtimeCommand(const RealtimeCommand& command) override;

    void appendStateJson(std::ostringstream& json) const override;
    bool implemented() const override;
    bool playable() const override;
    RobinStateSnapshot stateSnapshot() const;
    void applyStateSnapshot(const RobinStateSnapshot& state);

    audio::Synth& synth();
    const audio::Synth& synth() const;

private:
    using VoiceSyncMask = std::uint32_t;
    static constexpr VoiceSyncMask kSyncNone = 0u;
    static constexpr VoiceSyncMask kSyncActive = 1u << 0;
    static constexpr VoiceSyncMask kSyncFrequency = 1u << 1;
    static constexpr VoiceSyncMask kSyncGain = 1u << 2;
    static constexpr VoiceSyncMask kSyncEnvelope = 1u << 3;
    static constexpr VoiceSyncMask kSyncVcf = 1u << 4;
    static constexpr VoiceSyncMask kSyncEnvVcf = 1u << 5;
    static constexpr VoiceSyncMask kSyncOutputs = 1u << 6;
    static constexpr VoiceSyncMask kSyncOscillators = 1u << 7;
    static constexpr VoiceSyncMask kSyncAll = 0xffffffffu;

    static const char* waveformToString(dsp::Waveform waveform);
    static bool tryParseWaveform(std::string_view value, dsp::Waveform& waveform);
    static const char* lfoWaveformToString(dsp::LfoWaveform waveform);
    static bool tryParseLfoWaveform(std::string_view value, dsp::LfoWaveform& waveform);
    static const char* routingPresetToString(RoutingPreset preset);
    static bool tryParseRoutingPreset(std::string_view value, RoutingPreset& preset);
    static const char* spreadTargetToString(RobinSpreadTarget target);
    static bool tryParseSpreadTarget(std::string_view value, RobinSpreadTarget& target);
    static const char* spreadAlgorithmToString(RobinSpreadAlgorithm algorithm);
    static bool tryParseSpreadAlgorithm(std::string_view value, RobinSpreadAlgorithm& algorithm);
    static std::string escapeJson(std::string_view value);
    static bool tryParseIndex(std::string_view value, std::uint32_t& index);
    static float midiNoteToFrequency(int noteNumber);
    static float clampSpreadSlotValue(RobinSpreadTarget target, float value);
    static bool syncMaskIncludes(VoiceSyncMask mask, VoiceSyncMask flag);
    static VoiceSyncMask spreadTargetSyncMask(RobinSpreadTarget target);

    void logMasterOscillatorUpdate(std::string_view path, std::string_view valueDescription) const;
    float tunedFrequency(float baseFrequencyHz) const;
    void copyMasterStateToVoice(VoiceState& voice) const;
    void syncLinkedVoices(bool syncFrequency = false, VoiceSyncMask syncMask = kSyncAll);
    void syncVoiceFrequency(std::uint32_t voiceIndex);
    void syncAssignedVoiceFrequencies();
    void syncVoiceState(std::uint32_t voiceIndex,
                        bool syncFrequency = true,
                        VoiceSyncMask syncMask = kSyncAll,
                        const std::vector<int>* linkedPositions = nullptr,
                        std::uint32_t linkedCount = 0);
    void syncAllVoices();
    void syncLfo();
    void resetRoutingState();
    std::uint32_t computeNextTriggerOutput();
    void routeVoiceToOutput(std::uint32_t voiceIndex, std::uint32_t outputIndex);
    void routeVoiceToAllOutputs(std::uint32_t voiceIndex);
    std::uint32_t allocateVoice();
    void applyLinkedVoiceSpread(int linkedPosition,
                                std::uint32_t linkedCount,
                                VoiceSyncMask syncMask,
                                float& voiceGain,
                                RobinVcfState& vcf,
                                RobinEnvVcfState& envVcf,
                                EnvelopeState& envelope,
                                std::vector<OscillatorState>& oscillators) const;
    void buildLinkedVoiceSpreadIndex(std::vector<int>& linkedPositions, std::uint32_t& linkedCount) const;

    core::Logger* logger_ = nullptr;
    bool debugOscillatorParams_ = false;
    graph::RobinSourceNode sourceNode_;
    std::vector<VoiceState> voices_;
    std::vector<std::chrono::steady_clock::time_point> voiceReleaseUntil_;
    std::vector<OscillatorState> masterOscillators_;
    RobinPitchState pitchState_;
    LfoState lfoState_;
    RoutingPreset routingPreset_ = RoutingPreset::Forward;
    EnvelopeState envelopeState_;
    float masterVoiceGain_ = 1.0f;
    RobinVcfState vcfState_;
    RobinEnvVcfState envVcfState_;
    std::array<RobinSpreadSlot, 6> spreadSlots_ {{
        {false, RobinSpreadTarget::VcfCutoff, RobinSpreadAlgorithm::Linear, 0.0f, 0.0f, 0.0f, 17},
        {false, RobinSpreadTarget::OscDetune, RobinSpreadAlgorithm::Random, 0.0f, 0.0f, 0.0f, 37},
        {false, RobinSpreadTarget::AmpRelease, RobinSpreadAlgorithm::Alternating, 0.0f, 0.0f, 0.0f, 73},
        {false, RobinSpreadTarget::OscLevel, RobinSpreadAlgorithm::Linear, 0.0f, 0.0f, 0.0f, 101},
        {false, RobinSpreadTarget::EnvVcfAmount, RobinSpreadAlgorithm::Random, 0.0f, 0.0f, 0.0f, 131},
        {false, RobinSpreadTarget::AmpAttack, RobinSpreadAlgorithm::Alternating, 0.0f, 0.0f, 0.0f, 167},
    }};
    float baseFrequencyHz_ = 400.0f;
    std::uint32_t oscillatorsPerVoice_ = 6;
    std::uint32_t outputChannelCount_ = 2;
    std::uint32_t forwardOutputCursor_ = 0;
    std::uint32_t backwardOutputCursor_ = 0;
    std::vector<std::uint32_t> roundRobinPool_;
    std::optional<std::uint32_t> nextTriggerOutputIndex_;
    std::vector<RobinVoiceAssignment> voiceAssignments_;
    std::uint32_t nextVoiceCursor_ = 0;
    std::minstd_rand routingRandom_{std::random_device{}()};
    bool autoActivatedVoice0_ = false;
};

}  // namespace synth::app
