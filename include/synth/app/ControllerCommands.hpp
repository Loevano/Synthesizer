#pragma once

#include <cstdint>

namespace synth::app {

enum class RealtimeCommandType : std::uint8_t {
    SourceLevelRobin,
    SourceLevelTest,
    SourceMixerEnabled,
    SourceMixerRouteTarget,
    OutputLevel,
    OutputDelay,
    OutputEqLow,
    OutputEqMid,
    OutputEqHigh,
    SaturatorEnabled,
    SaturatorInputLevel,
    SaturatorOutputLevel,
    ChorusEnabled,
    ChorusDepth,
    ChorusSpeedHz,
    ChorusPhaseSpreadDegrees,
    SidechainEnabled,
    RobinLfoEnabled,
    RobinLfoDepth,
    RobinLfoPhaseSpreadDegrees,
    RobinLfoPolarityFlip,
    RobinLfoUnlinkedOutputs,
    RobinLfoClockLinked,
    RobinLfoTempoBpm,
    RobinLfoRateMultiplier,
    RobinLfoFixedFrequencyHz,
    RobinLfoWaveform,
    RobinMasterVcfCutoffHz,
    RobinMasterVcfResonance,
    RobinMasterEnvVcfAttackMs,
    RobinMasterEnvVcfDecayMs,
    RobinMasterEnvVcfSustain,
    RobinMasterEnvVcfReleaseMs,
    RobinMasterEnvVcfAmount,
    RobinMasterEnvelopeAttackMs,
    RobinMasterEnvelopeDecayMs,
    RobinMasterEnvelopeSustain,
    RobinMasterEnvelopeReleaseMs,
    RobinMasterFrequency,
    RobinMasterGain,
    RobinTransposeSemitones,
    RobinFineTuneCents,
    TestActive,
    TestMidiEnabled,
    TestFrequency,
    TestGain,
    TestEnvelopeAttackMs,
    TestEnvelopeDecayMs,
    TestEnvelopeSustain,
    TestEnvelopeReleaseMs,
    TestOutputEnabled,
    TestWaveform,
    RobinVoiceGain,
    RobinVoiceFrequency,
    RobinVoiceVcfCutoffHz,
    RobinVoiceVcfResonance,
    RobinVoiceEnvVcfAttackMs,
    RobinVoiceEnvVcfDecayMs,
    RobinVoiceEnvVcfSustain,
    RobinVoiceEnvVcfReleaseMs,
    RobinVoiceEnvVcfAmount,
    RobinVoiceEnvelopeAttackMs,
    RobinVoiceEnvelopeDecayMs,
    RobinVoiceEnvelopeSustain,
    RobinVoiceEnvelopeReleaseMs,
    RobinMasterOscillatorEnabled,
    RobinMasterOscillatorGain,
    RobinMasterOscillatorRelative,
    RobinMasterOscillatorFrequency,
    RobinMasterOscillatorWaveform,
    RobinVoiceOscillatorEnabled,
    RobinVoiceOscillatorGain,
    RobinVoiceOscillatorRelative,
    RobinVoiceOscillatorFrequency,
    RobinVoiceOscillatorWaveform,
    GlobalNoteOn,
    GlobalNoteOff,
    MidiNoteOn,
    MidiNoteOff,
};

struct RealtimeCommand {
    RealtimeCommandType type = RealtimeCommandType::SourceLevelRobin;
    std::uint32_t index = 0;
    int noteNumber = -1;
    float value = 0.0f;
    std::uint32_t subIndex = 0;
    std::uint32_t code = 0;
};

enum class RealtimeParamResult : std::uint8_t {
    NotHandled,
    Applied,
    Failed,
};

}  // namespace synth::app
