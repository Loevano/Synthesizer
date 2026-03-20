#include "synth/app/Robin.hpp"

#include "synth/core/Logger.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>
#include <string>

namespace synth::app {

namespace {

std::vector<std::string_view> splitPath(std::string_view path) {
    std::vector<std::string_view> parts;

    while (!path.empty()) {
        const std::size_t separator = path.find('.');
        if (separator == std::string_view::npos) {
            parts.push_back(path);
            break;
        }

        parts.push_back(path.substr(0, separator));
        path.remove_prefix(separator + 1);
    }

    return parts;
}

float lerp(float start, float end, float t) {
    return start + ((end - start) * t);
}

float stableRandomUnit(std::uint32_t seed, std::uint32_t position) {
    std::uint32_t value = seed + 0x9e3779b9u + (position * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x01000000u - 1u);
}

}  // namespace

void Robin::setLogger(core::Logger* logger) {
    logger_ = logger;
}

void Robin::setDebugOscillatorParams(bool enabled) {
    debugOscillatorParams_ = enabled;
}

void Robin::configureStructure(std::uint32_t voiceCount,
                               std::uint32_t oscillatorsPerVoice,
                               std::uint32_t outputChannels) {
    const auto previousVoices = voices_;
    const auto previousMasterOscillators = masterOscillators_;

    sourceNode_.synth().clearNotes();
    heldNotes_.clear();
    voiceAssignments_.clear();
    voiceReleaseUntil_.clear();
    nextHeldNoteId_ = 1;
    nextVoiceCursor_ = 0;
    autoActivatedVoice0_ = false;
    resetRoutingState();

    outputChannelCount_ = std::max<std::uint32_t>(1, outputChannels);
    oscillatorsPerVoice_ = std::max<std::uint32_t>(1, oscillatorsPerVoice);

    audio::SynthConfig synthConfig;
    synthConfig.voiceCount = std::max<std::uint32_t>(1, voiceCount);
    synthConfig.oscillatorsPerVoice = oscillatorsPerVoice_;
    sourceNode_.configure(synthConfig);

    masterOscillators_.clear();
    masterOscillators_.resize(synthConfig.oscillatorsPerVoice);
    for (std::uint32_t oscillatorIndex = 0; oscillatorIndex < synthConfig.oscillatorsPerVoice; ++oscillatorIndex) {
        auto& oscillator = masterOscillators_[oscillatorIndex];
        oscillator.enabled = (oscillatorIndex == 0);
        oscillator.gain = 1.0f;
        oscillator.relativeToVoice = true;
        oscillator.frequencyValue = 1.0f;
        oscillator.waveform = dsp::Waveform::Sine;
    }

    const std::size_t masterOscillatorCountToCopy =
        std::min(previousMasterOscillators.size(), masterOscillators_.size());
    for (std::size_t oscillatorIndex = 0; oscillatorIndex < masterOscillatorCountToCopy; ++oscillatorIndex) {
        masterOscillators_[oscillatorIndex] = previousMasterOscillators[oscillatorIndex];
    }

    voices_.clear();
    voices_.resize(synthConfig.voiceCount);
    voiceReleaseUntil_.assign(synthConfig.voiceCount, std::chrono::steady_clock::time_point::min());

    const std::size_t voiceCountToCopy = std::min(previousVoices.size(), voices_.size());
    for (std::size_t voiceIndex = 0; voiceIndex < voiceCountToCopy; ++voiceIndex) {
        voices_[voiceIndex] = previousVoices[voiceIndex];
    }

    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        auto& voice = voices_[voiceIndex];
        if (voiceIndex >= voiceCountToCopy) {
            voice.active = true;
            voice.linkedToMaster = true;
            voice.frequency = baseFrequencyHz_;
            voice.gain = masterVoiceGain_;
            voice.vcf = vcfState_;
            voice.envVcf = envVcfState_;
            voice.envelope = envelopeState_;
        }

        voice.outputs.resize(outputChannelCount_, false);
        if (std::none_of(voice.outputs.begin(), voice.outputs.end(), [](bool enabled) { return enabled; })
            && !voice.outputs.empty()) {
            voice.outputs[voiceIndex % voice.outputs.size()] = true;
        }

        voice.oscillators.resize(oscillatorsPerVoice_);
        if (voice.linkedToMaster) {
            copyMasterStateToVoice(voice);
        }
    }

    if (routingPreset_ != RoutingPreset::Custom) {
        resetRoutingState();
        for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
            if (routingPreset_ == RoutingPreset::AllOutputs) {
                routeVoiceToAllOutputs(voiceIndex);
            } else {
                routeVoiceToOutput(voiceIndex, computeNextTriggerOutput());
            }
        }
    }

    syncLfo();
    syncAllVoices();
}

void Robin::clearAllNotes() {
    sourceNode_.synth().clearNotes();
    heldNotes_.clear();
    voiceAssignments_.clear();
    nextHeldNoteId_ = 1;
    nextVoiceCursor_ = 0;
    autoActivatedVoice0_ = false;
    resetRoutingState();
}

void Robin::setBaseFrequency(float frequencyHz) {
    baseFrequencyHz_ = std::clamp(frequencyHz, 20.0f, 20000.0f);
    syncLinkedVoices(true, kSyncFrequency);
}

float Robin::baseFrequencyHz() const {
    return baseFrequencyHz_;
}

std::uint32_t Robin::voiceCount() const {
    return static_cast<std::uint32_t>(voices_.size());
}

std::uint32_t Robin::oscillatorsPerVoice() const {
    return oscillatorsPerVoice_;
}

void Robin::prepare(double sampleRate, std::uint32_t outputChannels) {
    sourceNode_.prepare(sampleRate, outputChannels);
}

void Robin::renderAdd(float* output,
                      std::uint32_t frames,
                      std::uint32_t channels,
                      bool enabled,
                      float level) {
    sourceNode_.setLevel(enabled, level);
    sourceNode_.renderAdd(output, frames, channels);
}

void Robin::noteOn(int noteNumber, float /*velocity*/) {
    if (voices_.empty()) {
        return;
    }

    const std::uint64_t noteId = nextHeldNoteId_++;
    const std::uint32_t voiceIndex = allocateVoice();
    if (voiceIndex < voiceReleaseUntil_.size()) {
        voiceReleaseUntil_[voiceIndex] = std::chrono::steady_clock::time_point::min();
    }
    heldNotes_.push_back({noteId, noteNumber, voiceIndex});
    voiceAssignments_.push_back({noteId, noteNumber, voiceIndex});
    syncVoiceFrequency(voiceIndex);

    if (routingPreset_ == RoutingPreset::AllOutputs) {
        routeVoiceToAllOutputs(voiceIndex);
    } else if (routingPreset_ != RoutingPreset::Custom) {
        routeVoiceToOutput(voiceIndex, computeNextTriggerOutput());
    }

    sourceNode_.synth().noteOn(voiceIndex);
}

void Robin::noteOff(int noteNumber) {
    const auto heldNote = std::find_if(
        heldNotes_.begin(),
        heldNotes_.end(),
        [noteNumber](const RobinHeldNote& currentNote) {
            return currentNote.noteNumber == noteNumber;
        });

    if (heldNote == heldNotes_.end()) {
        return;
    }

    const std::uint64_t noteId = heldNote->noteId;
    const std::optional<std::uint32_t> voiceIndex = heldNote->voiceIndex;
    heldNotes_.erase(heldNote);

    if (voiceIndex.has_value()) {
        sourceNode_.synth().noteOff(*voiceIndex);
        if (*voiceIndex < voiceReleaseUntil_.size()) {
            const auto& voice = voices_[*voiceIndex];
            const float releaseMs = voice.linkedToMaster ? envelopeState_.releaseMs : voice.envelope.releaseMs;
            voiceReleaseUntil_[*voiceIndex] =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(std::ceil(releaseMs)));
        }

        const auto assignment = std::find_if(
            voiceAssignments_.begin(),
            voiceAssignments_.end(),
            [noteId](const RobinVoiceAssignment& currentAssignment) {
                return currentAssignment.noteId == noteId;
            });
        if (assignment != voiceAssignments_.end()) {
            voiceAssignments_.erase(assignment);
        }
    }

    if (autoActivatedVoice0_ && voiceAssignments_.empty() && !voices_.empty()) {
        voices_[0].active = false;
        sourceNode_.synth().setVoiceActive(0, false);
        autoActivatedVoice0_ = false;
        nextVoiceCursor_ = 0;
    }
}

RealtimeParamResult Robin::applyNumericParam(const std::vector<std::string_view>& parts,
                                             double value,
                                             std::string* errorMessage) {
    RealtimeCommand command;
    const auto realtimeResult = tryBuildRealtimeNumericCommand(parts, value, command, errorMessage);
    if (realtimeResult == RealtimeParamResult::Applied) {
        applyRealtimeCommand(command);
        return RealtimeParamResult::Applied;
    }
    if (realtimeResult == RealtimeParamResult::Failed) {
        return RealtimeParamResult::Failed;
    }

    if ((parts.size() == 1 && parts[0] == "gain")
        || (parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "gain")) {
        masterVoiceGain_ = static_cast<float>(std::clamp(value, 0.0, 1.0));
        syncLinkedVoices(false, kSyncGain);
        return RealtimeParamResult::Applied;
    }

    if ((parts.size() == 1 && parts[0] == "frequency")
        || (parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "frequency")) {
        setBaseFrequency(static_cast<float>(value));
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 5 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "voice") {
        std::uint32_t voiceIndex = 0;
        if (!tryParseIndex(parts[3], voiceIndex) || voiceIndex >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return RealtimeParamResult::Failed;
        }

        auto& voice = voices_[voiceIndex];
        if (parts[4] == "active") {
            voice.active = value >= 0.5;
            syncAllVoices();
            return RealtimeParamResult::Applied;
        }
        if (parts[4] == "linkedToMaster") {
            voice.linkedToMaster = value >= 0.5;
            syncAllVoices();
            return RealtimeParamResult::Applied;
        }
        if (parts[4] == "resetToMasterState") {
            copyMasterStateToVoice(voice);
            if (!voice.linkedToMaster) {
                syncVoiceState(voiceIndex);
            }
            return RealtimeParamResult::Applied;
        }
    }

    if (parts.size() == 6 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "voice" && parts[4] == "output") {
        std::uint32_t voiceIndex = 0;
        std::uint32_t outputIndex = 0;
        if (!tryParseIndex(parts[3], voiceIndex) || voiceIndex >= voices_.size()
            || !tryParseIndex(parts[5], outputIndex) || outputIndex >= voices_[voiceIndex].outputs.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output index.";
            }
            return RealtimeParamResult::Failed;
        }
        voices_[voiceIndex].outputs[outputIndex] = value >= 0.5;
        routingPreset_ = RoutingPreset::Custom;
        resetRoutingState();
        sourceNode_.synth().setVoiceOutputEnabled(voiceIndex, outputIndex, voices_[voiceIndex].outputs[outputIndex]);
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

RealtimeParamResult Robin::applyStringParam(const std::vector<std::string_view>& parts,
                                            std::string_view value,
                                            std::string* errorMessage) {
    RealtimeCommand command;
    const auto realtimeResult = tryBuildRealtimeStringCommand(parts, value, command, errorMessage);
    if (realtimeResult == RealtimeParamResult::Applied) {
        applyRealtimeCommand(command);
        return RealtimeParamResult::Applied;
    }
    if (realtimeResult == RealtimeParamResult::Failed) {
        return RealtimeParamResult::Failed;
    }

    if ((parts.size() == 1 && parts[0] == "routingPreset")
        || (parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "routingPreset")) {
        RoutingPreset preset;
        if (!tryParseRoutingPreset(value, preset)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid routing preset.";
            }
            return RealtimeParamResult::Failed;
        }
        routingPreset_ = preset;
        resetRoutingState();
        syncAllVoices();
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

RealtimeParamResult Robin::tryBuildRealtimeNumericCommand(const std::vector<std::string_view>& parts,
                                                          double value,
                                                          RealtimeCommand& command,
                                                          std::string* errorMessage) const {
    if ((parts.size() == 1 && parts[0] == "lfo")
        || (parts.size() == 1 && parts[0] == "gain")) {
        return RealtimeParamResult::NotHandled;
    }

    if ((parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "transposeSemitones")) {
        command.type = RealtimeCommandType::RobinTransposeSemitones;
        command.value = static_cast<float>(std::clamp(value, -12.0, 12.0));
        return RealtimeParamResult::Applied;
    }
    if ((parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "fineTuneCents")) {
        command.type = RealtimeCommandType::RobinFineTuneCents;
        command.value = static_cast<float>(std::clamp(value, -100.0, 100.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 5 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "spread") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= spreadSlots_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid spread slot index.";
            }
            return RealtimeParamResult::Failed;
        }
        const auto target = spreadSlots_[command.index].target;
        if (parts[4] == "enabled") {
            command.type = RealtimeCommandType::RobinSpreadEnabled;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[4] == "depth") {
            command.type = RealtimeCommandType::RobinSpreadDepth;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[4] == "start") {
            command.type = RealtimeCommandType::RobinSpreadStart;
            command.value = clampSpreadSlotValue(target, static_cast<float>(value));
        } else if (parts[4] == "end") {
            command.type = RealtimeCommandType::RobinSpreadEnd;
            command.value = clampSpreadSlotValue(target, static_cast<float>(value));
        } else if (parts[4] == "seed") {
            command.type = RealtimeCommandType::RobinSpreadSeed;
            command.value = static_cast<float>(std::clamp(value, 1.0, 9999.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
        return RealtimeParamResult::Applied;
    }
    if ((parts.size() == 3 && parts[0] == "lfo")
        || (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "lfo")) {
        const std::size_t fieldIndex = parts.size() - 1;
        if (parts[fieldIndex] == "enabled") {
            command.type = RealtimeCommandType::RobinLfoEnabled;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[fieldIndex] == "depth") {
            command.type = RealtimeCommandType::RobinLfoDepth;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[fieldIndex] == "phaseSpreadDegrees") {
            command.type = RealtimeCommandType::RobinLfoPhaseSpreadDegrees;
            command.value = static_cast<float>(std::clamp(value, 0.0, 360.0));
        } else if (parts[fieldIndex] == "polarityFlip") {
            command.type = RealtimeCommandType::RobinLfoPolarityFlip;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[fieldIndex] == "unlinkedOutputs") {
            command.type = RealtimeCommandType::RobinLfoUnlinkedOutputs;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[fieldIndex] == "clockLinked") {
            command.type = RealtimeCommandType::RobinLfoClockLinked;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[fieldIndex] == "tempoBpm") {
            command.type = RealtimeCommandType::RobinLfoTempoBpm;
            command.value = static_cast<float>(std::clamp(value, 20.0, 300.0));
        } else if (parts[fieldIndex] == "rateMultiplier") {
            command.type = RealtimeCommandType::RobinLfoRateMultiplier;
            command.value = static_cast<float>(std::clamp(value, 0.125, 8.0));
        } else if (parts[fieldIndex] == "fixedFrequencyHz") {
            command.type = RealtimeCommandType::RobinLfoFixedFrequencyHz;
            command.value = static_cast<float>(std::clamp(value, 0.01, 40.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "gain") {
        command.type = RealtimeCommandType::RobinMasterGain;
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        return RealtimeParamResult::Applied;
    }

    if ((parts.size() == 1 && parts[0] == "frequency")
        || (parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "frequency")) {
        command.type = RealtimeCommandType::RobinMasterFrequency;
        command.value = static_cast<float>(std::clamp(value, 20.0, 20000.0));
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "vcf") {
        if (parts[3] == "cutoffHz") {
            command.type = RealtimeCommandType::RobinMasterVcfCutoffHz;
            command.value = static_cast<float>(std::clamp(value, 20.0, 20000.0));
        } else if (parts[3] == "resonance") {
            command.type = RealtimeCommandType::RobinMasterVcfResonance;
            command.value = static_cast<float>(std::clamp(value, 0.1, 10.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "envVcf") {
        if (parts[3] == "attackMs") {
            command.type = RealtimeCommandType::RobinMasterEnvVcfAttackMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
        } else if (parts[3] == "decayMs") {
            command.type = RealtimeCommandType::RobinMasterEnvVcfDecayMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
        } else if (parts[3] == "sustain") {
            command.type = RealtimeCommandType::RobinMasterEnvVcfSustain;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[3] == "releaseMs") {
            command.type = RealtimeCommandType::RobinMasterEnvVcfReleaseMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
        } else if (parts[3] == "amount") {
            command.type = RealtimeCommandType::RobinMasterEnvVcfAmount;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "envelope") {
        if (parts[3] == "attackMs") {
            command.type = RealtimeCommandType::RobinMasterEnvelopeAttackMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
        } else if (parts[3] == "decayMs") {
            command.type = RealtimeCommandType::RobinMasterEnvelopeDecayMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
        } else if (parts[3] == "sustain") {
            command.type = RealtimeCommandType::RobinMasterEnvelopeSustain;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[3] == "releaseMs") {
            command.type = RealtimeCommandType::RobinMasterEnvelopeReleaseMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 5 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "oscillator") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= masterOscillators_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return RealtimeParamResult::Failed;
        }
        if (parts[4] == "enabled") {
            command.type = RealtimeCommandType::RobinMasterOscillatorEnabled;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[4] == "gain") {
            command.type = RealtimeCommandType::RobinMasterOscillatorGain;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[4] == "relative") {
            command.type = RealtimeCommandType::RobinMasterOscillatorRelative;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[4] == "frequency") {
            command.type = RealtimeCommandType::RobinMasterOscillatorFrequency;
            command.value = static_cast<float>(value);
        } else {
            return RealtimeParamResult::NotHandled;
        }
        return RealtimeParamResult::Applied;
    }

    if (parts.size() >= 5 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "voice") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return RealtimeParamResult::Failed;
        }

        if (parts.size() == 5 && parts[4] == "active") {
            command.type = RealtimeCommandType::RobinVoiceActive;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 5 && parts[4] == "linkedToMaster") {
            command.type = RealtimeCommandType::RobinVoiceLinkedToMaster;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 5 && parts[4] == "resetToMasterState") {
            command.type = RealtimeCommandType::RobinVoiceResetToMasterState;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
            return RealtimeParamResult::Applied;
        }

        if (parts.size() == 5 && parts[4] == "frequency") {
            if (voices_[command.index].linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return RealtimeParamResult::Failed;
            }
            command.type = RealtimeCommandType::RobinVoiceFrequency;
            command.value = static_cast<float>(std::clamp(value, 20.0, 20000.0));
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 5 && parts[4] == "gain") {
            if (voices_[command.index].linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return RealtimeParamResult::Failed;
            }
            command.type = RealtimeCommandType::RobinVoiceGain;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 6 && parts[4] == "vcf") {
            if (voices_[command.index].linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return RealtimeParamResult::Failed;
            }
            if (parts[5] == "cutoffHz") {
                command.type = RealtimeCommandType::RobinVoiceVcfCutoffHz;
                command.value = static_cast<float>(std::clamp(value, 20.0, 20000.0));
            } else if (parts[5] == "resonance") {
                command.type = RealtimeCommandType::RobinVoiceVcfResonance;
                command.value = static_cast<float>(std::clamp(value, 0.1, 10.0));
            } else {
                return RealtimeParamResult::NotHandled;
            }
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 6 && parts[4] == "envVcf") {
            if (voices_[command.index].linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return RealtimeParamResult::Failed;
            }
            if (parts[5] == "attackMs") {
                command.type = RealtimeCommandType::RobinVoiceEnvVcfAttackMs;
                command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            } else if (parts[5] == "decayMs") {
                command.type = RealtimeCommandType::RobinVoiceEnvVcfDecayMs;
                command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            } else if (parts[5] == "sustain") {
                command.type = RealtimeCommandType::RobinVoiceEnvVcfSustain;
                command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            } else if (parts[5] == "releaseMs") {
                command.type = RealtimeCommandType::RobinVoiceEnvVcfReleaseMs;
                command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            } else if (parts[5] == "amount") {
                command.type = RealtimeCommandType::RobinVoiceEnvVcfAmount;
                command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            } else {
                return RealtimeParamResult::NotHandled;
            }
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 6 && parts[4] == "envelope") {
            if (voices_[command.index].linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return RealtimeParamResult::Failed;
            }
            if (parts[5] == "attackMs") {
                command.type = RealtimeCommandType::RobinVoiceEnvelopeAttackMs;
                command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            } else if (parts[5] == "decayMs") {
                command.type = RealtimeCommandType::RobinVoiceEnvelopeDecayMs;
                command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            } else if (parts[5] == "sustain") {
                command.type = RealtimeCommandType::RobinVoiceEnvelopeSustain;
                command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            } else if (parts[5] == "releaseMs") {
                command.type = RealtimeCommandType::RobinVoiceEnvelopeReleaseMs;
                command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            } else {
                return RealtimeParamResult::NotHandled;
            }
            return RealtimeParamResult::Applied;
        }
        if (parts.size() == 7 && parts[4] == "oscillator") {
            if (voices_[command.index].linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return RealtimeParamResult::Failed;
            }
            if (!tryParseIndex(parts[5], command.subIndex)
                || command.subIndex >= voices_[command.index].oscillators.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid oscillator index.";
                }
                return RealtimeParamResult::Failed;
            }
            if (parts[6] == "enabled") {
                command.type = RealtimeCommandType::RobinVoiceOscillatorEnabled;
                command.value = value >= 0.5 ? 1.0f : 0.0f;
            } else if (parts[6] == "gain") {
                command.type = RealtimeCommandType::RobinVoiceOscillatorGain;
                command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            } else if (parts[6] == "relative") {
                command.type = RealtimeCommandType::RobinVoiceOscillatorRelative;
                command.value = value >= 0.5 ? 1.0f : 0.0f;
            } else if (parts[6] == "frequency") {
                command.type = RealtimeCommandType::RobinVoiceOscillatorFrequency;
                command.value = static_cast<float>(value);
            } else {
                return RealtimeParamResult::NotHandled;
            }
            return RealtimeParamResult::Applied;
        }
    }

    if (parts.size() == 6 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "voice" && parts[4] == "output") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return RealtimeParamResult::Failed;
        }
        if (!tryParseIndex(parts[5], command.subIndex) || command.subIndex >= voices_[command.index].outputs.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output index.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::RobinVoiceOutputEnabled;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

RealtimeParamResult Robin::tryBuildRealtimeStringCommand(const std::vector<std::string_view>& parts,
                                                         std::string_view value,
                                                         RealtimeCommand& command,
                                                         std::string* errorMessage) const {
    if (parts.size() == 5 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "spread") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= spreadSlots_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid spread slot index.";
            }
            return RealtimeParamResult::Failed;
        }

        if (parts[4] == "target") {
            RobinSpreadTarget target;
            if (!tryParseSpreadTarget(value, target)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid spread target.";
                }
                return RealtimeParamResult::Failed;
            }
            command.type = RealtimeCommandType::RobinSpreadTarget;
            command.code = static_cast<std::uint32_t>(target);
            return RealtimeParamResult::Applied;
        }

        if (parts[4] == "algorithm") {
            RobinSpreadAlgorithm algorithm;
            if (!tryParseSpreadAlgorithm(value, algorithm)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid spread algorithm.";
                }
                return RealtimeParamResult::Failed;
            }
            command.type = RealtimeCommandType::RobinSpreadAlgorithm;
            command.code = static_cast<std::uint32_t>(algorithm);
            return RealtimeParamResult::Applied;
        }
    }

    if ((parts.size() == 1 && parts[0] == "routingPreset")
        || (parts.size() == 3 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "routingPreset")) {
        RoutingPreset preset;
        if (!tryParseRoutingPreset(value, preset)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid routing preset.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::RobinRoutingPreset;
        command.code = static_cast<std::uint32_t>(preset);
        return RealtimeParamResult::Applied;
    }

    if ((parts.size() == 2 && parts[0] == "lfo" && parts[1] == "waveform")
        || (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "lfo" && parts[3] == "waveform")) {
        dsp::LfoWaveform waveform;
        if (!tryParseLfoWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid LFO waveform value.";
            }
            return RealtimeParamResult::Failed;
        }
        command.type = RealtimeCommandType::RobinLfoWaveform;
        command.code = static_cast<std::uint32_t>(waveform);
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 5 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "oscillator" && parts[4] == "waveform") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= masterOscillators_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return RealtimeParamResult::Failed;
        }
        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return RealtimeParamResult::Failed;
        }
        command.type = RealtimeCommandType::RobinMasterOscillatorWaveform;
        command.code = static_cast<std::uint32_t>(waveform);
        return RealtimeParamResult::Applied;
    }

    if (parts.size() == 7
        && parts[0] == "sources"
        && parts[1] == "robin"
        && parts[2] == "voice"
        && parts[4] == "oscillator"
        && parts[6] == "waveform") {
        if (!tryParseIndex(parts[3], command.index) || command.index >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return RealtimeParamResult::Failed;
        }
        if (voices_[command.index].linkedToMaster) {
            if (errorMessage != nullptr) {
                *errorMessage = "Voice is linked to the master template.";
            }
            return RealtimeParamResult::Failed;
        }
        if (!tryParseIndex(parts[5], command.subIndex)
            || command.subIndex >= voices_[command.index].oscillators.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return RealtimeParamResult::Failed;
        }
        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return RealtimeParamResult::Failed;
        }
        command.type = RealtimeCommandType::RobinVoiceOscillatorWaveform;
        command.code = static_cast<std::uint32_t>(waveform);
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

bool Robin::handlesRealtimeCommand(RealtimeCommandType type) const {
    switch (type) {
        case RealtimeCommandType::RobinLfoEnabled:
        case RealtimeCommandType::RobinLfoDepth:
        case RealtimeCommandType::RobinLfoPhaseSpreadDegrees:
        case RealtimeCommandType::RobinLfoPolarityFlip:
        case RealtimeCommandType::RobinLfoUnlinkedOutputs:
        case RealtimeCommandType::RobinLfoClockLinked:
        case RealtimeCommandType::RobinLfoTempoBpm:
        case RealtimeCommandType::RobinLfoRateMultiplier:
        case RealtimeCommandType::RobinLfoFixedFrequencyHz:
        case RealtimeCommandType::RobinLfoWaveform:
        case RealtimeCommandType::RobinMasterVcfCutoffHz:
        case RealtimeCommandType::RobinMasterVcfResonance:
        case RealtimeCommandType::RobinMasterEnvVcfAttackMs:
        case RealtimeCommandType::RobinMasterEnvVcfDecayMs:
        case RealtimeCommandType::RobinMasterEnvVcfSustain:
        case RealtimeCommandType::RobinMasterEnvVcfReleaseMs:
        case RealtimeCommandType::RobinMasterEnvVcfAmount:
        case RealtimeCommandType::RobinMasterEnvelopeAttackMs:
        case RealtimeCommandType::RobinMasterEnvelopeDecayMs:
        case RealtimeCommandType::RobinMasterEnvelopeSustain:
        case RealtimeCommandType::RobinMasterEnvelopeReleaseMs:
        case RealtimeCommandType::RobinMasterFrequency:
        case RealtimeCommandType::RobinMasterGain:
        case RealtimeCommandType::RobinRoutingPreset:
        case RealtimeCommandType::RobinTransposeSemitones:
        case RealtimeCommandType::RobinFineTuneCents:
        case RealtimeCommandType::RobinSpreadEnabled:
        case RealtimeCommandType::RobinSpreadTarget:
        case RealtimeCommandType::RobinSpreadAlgorithm:
        case RealtimeCommandType::RobinSpreadDepth:
        case RealtimeCommandType::RobinSpreadStart:
        case RealtimeCommandType::RobinSpreadEnd:
        case RealtimeCommandType::RobinSpreadSeed:
        case RealtimeCommandType::RobinVoiceActive:
        case RealtimeCommandType::RobinVoiceLinkedToMaster:
        case RealtimeCommandType::RobinVoiceResetToMasterState:
        case RealtimeCommandType::RobinVoiceOutputEnabled:
        case RealtimeCommandType::RobinVoiceGain:
        case RealtimeCommandType::RobinVoiceFrequency:
        case RealtimeCommandType::RobinVoiceVcfCutoffHz:
        case RealtimeCommandType::RobinVoiceVcfResonance:
        case RealtimeCommandType::RobinVoiceEnvVcfAttackMs:
        case RealtimeCommandType::RobinVoiceEnvVcfDecayMs:
        case RealtimeCommandType::RobinVoiceEnvVcfSustain:
        case RealtimeCommandType::RobinVoiceEnvVcfReleaseMs:
        case RealtimeCommandType::RobinVoiceEnvVcfAmount:
        case RealtimeCommandType::RobinVoiceEnvelopeAttackMs:
        case RealtimeCommandType::RobinVoiceEnvelopeDecayMs:
        case RealtimeCommandType::RobinVoiceEnvelopeSustain:
        case RealtimeCommandType::RobinVoiceEnvelopeReleaseMs:
        case RealtimeCommandType::RobinMasterOscillatorEnabled:
        case RealtimeCommandType::RobinMasterOscillatorGain:
        case RealtimeCommandType::RobinMasterOscillatorRelative:
        case RealtimeCommandType::RobinMasterOscillatorFrequency:
        case RealtimeCommandType::RobinMasterOscillatorWaveform:
        case RealtimeCommandType::RobinVoiceOscillatorEnabled:
        case RealtimeCommandType::RobinVoiceOscillatorGain:
        case RealtimeCommandType::RobinVoiceOscillatorRelative:
        case RealtimeCommandType::RobinVoiceOscillatorFrequency:
        case RealtimeCommandType::RobinVoiceOscillatorWaveform:
            return true;
        default:
            return false;
    }
}

void Robin::applyRealtimeCommand(const RealtimeCommand& command) {
    switch (command.type) {
        case RealtimeCommandType::RobinLfoEnabled:
            lfoState_.enabled = command.value >= 0.5f;
            sourceNode_.synth().setLfoEnabled(lfoState_.enabled);
            break;
        case RealtimeCommandType::RobinLfoDepth:
            lfoState_.depth = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.synth().setLfoDepth(lfoState_.depth);
            break;
        case RealtimeCommandType::RobinLfoPhaseSpreadDegrees:
            lfoState_.phaseSpreadDegrees = std::clamp(command.value, 0.0f, 360.0f);
            sourceNode_.synth().setLfoPhaseSpreadDegrees(lfoState_.phaseSpreadDegrees);
            break;
        case RealtimeCommandType::RobinLfoPolarityFlip:
            lfoState_.polarityFlip = command.value >= 0.5f;
            sourceNode_.synth().setLfoPolarityFlip(lfoState_.polarityFlip);
            break;
        case RealtimeCommandType::RobinLfoUnlinkedOutputs:
            lfoState_.unlinkedOutputs = command.value >= 0.5f;
            sourceNode_.synth().setLfoUnlinkedOutputs(lfoState_.unlinkedOutputs);
            break;
        case RealtimeCommandType::RobinLfoClockLinked:
            lfoState_.clockLinked = command.value >= 0.5f;
            sourceNode_.synth().setLfoClockLinked(lfoState_.clockLinked);
            break;
        case RealtimeCommandType::RobinLfoTempoBpm:
            lfoState_.tempoBpm = std::clamp(command.value, 20.0f, 300.0f);
            sourceNode_.synth().setLfoTempoBpm(lfoState_.tempoBpm);
            break;
        case RealtimeCommandType::RobinLfoRateMultiplier:
            lfoState_.rateMultiplier = std::clamp(command.value, 0.125f, 8.0f);
            sourceNode_.synth().setLfoRateMultiplier(lfoState_.rateMultiplier);
            break;
        case RealtimeCommandType::RobinLfoFixedFrequencyHz:
            lfoState_.fixedFrequencyHz = std::clamp(command.value, 0.01f, 40.0f);
            sourceNode_.synth().setLfoFixedFrequencyHz(lfoState_.fixedFrequencyHz);
            break;
        case RealtimeCommandType::RobinLfoWaveform:
            lfoState_.waveform = static_cast<dsp::LfoWaveform>(command.code);
            sourceNode_.synth().setLfoWaveform(lfoState_.waveform);
            break;
        case RealtimeCommandType::RobinMasterGain:
            masterVoiceGain_ = std::clamp(command.value, 0.0f, 1.0f);
            syncLinkedVoices(false, kSyncGain);
            break;
        case RealtimeCommandType::RobinMasterFrequency:
            setBaseFrequency(command.value);
            break;
        case RealtimeCommandType::RobinRoutingPreset:
            routingPreset_ = static_cast<RoutingPreset>(command.code);
            resetRoutingState();
            syncAllVoices();
            break;
        case RealtimeCommandType::RobinMasterVcfCutoffHz:
            vcfState_.cutoffHz = std::clamp(command.value, 20.0f, 20000.0f);
            syncLinkedVoices(false, kSyncVcf);
            break;
        case RealtimeCommandType::RobinMasterVcfResonance:
            vcfState_.resonance = std::clamp(command.value, 0.1f, 10.0f);
            syncLinkedVoices(false, kSyncVcf);
            break;
        case RealtimeCommandType::RobinMasterEnvVcfAttackMs:
            envVcfState_.attackMs = std::clamp(command.value, 0.0f, 5000.0f);
            syncLinkedVoices(false, kSyncEnvVcf);
            break;
        case RealtimeCommandType::RobinMasterEnvVcfDecayMs:
            envVcfState_.decayMs = std::clamp(command.value, 0.0f, 5000.0f);
            syncLinkedVoices(false, kSyncEnvVcf);
            break;
        case RealtimeCommandType::RobinMasterEnvVcfSustain:
            envVcfState_.sustain = std::clamp(command.value, 0.0f, 1.0f);
            syncLinkedVoices(false, kSyncEnvVcf);
            break;
        case RealtimeCommandType::RobinMasterEnvVcfReleaseMs:
            envVcfState_.releaseMs = std::clamp(command.value, 0.0f, 5000.0f);
            syncLinkedVoices(false, kSyncEnvVcf);
            break;
        case RealtimeCommandType::RobinMasterEnvVcfAmount:
            envVcfState_.amount = std::clamp(command.value, 0.0f, 1.0f);
            syncLinkedVoices(false, kSyncEnvVcf);
            break;
        case RealtimeCommandType::RobinMasterEnvelopeAttackMs:
            envelopeState_.attackMs = std::clamp(command.value, 0.0f, 5000.0f);
            syncLinkedVoices(false, kSyncEnvelope);
            break;
        case RealtimeCommandType::RobinMasterEnvelopeDecayMs:
            envelopeState_.decayMs = std::clamp(command.value, 0.0f, 5000.0f);
            syncLinkedVoices(false, kSyncEnvelope);
            break;
        case RealtimeCommandType::RobinMasterEnvelopeSustain:
            envelopeState_.sustain = std::clamp(command.value, 0.0f, 1.0f);
            syncLinkedVoices(false, kSyncEnvelope);
            break;
        case RealtimeCommandType::RobinMasterEnvelopeReleaseMs:
            envelopeState_.releaseMs = std::clamp(command.value, 0.0f, 5000.0f);
            syncLinkedVoices(false, kSyncEnvelope);
            break;
        case RealtimeCommandType::RobinTransposeSemitones:
            pitchState_.transposeSemitones = std::clamp(command.value, -12.0f, 12.0f);
            syncAssignedVoiceFrequencies();
            break;
        case RealtimeCommandType::RobinFineTuneCents:
            pitchState_.fineTuneCents = std::clamp(command.value, -100.0f, 100.0f);
            syncAssignedVoiceFrequencies();
            break;
        case RealtimeCommandType::RobinSpreadEnabled:
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            spreadSlots_[command.index].enabled = command.value >= 0.5f;
            syncLinkedVoices(false, spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        case RealtimeCommandType::RobinSpreadTarget: {
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            const VoiceSyncMask previousTargetMask = spreadTargetSyncMask(spreadSlots_[command.index].target);
            spreadSlots_[command.index].target = static_cast<RobinSpreadTarget>(command.code);
            spreadSlots_[command.index].start =
                clampSpreadSlotValue(spreadSlots_[command.index].target, spreadSlots_[command.index].start);
            spreadSlots_[command.index].end =
                clampSpreadSlotValue(spreadSlots_[command.index].target, spreadSlots_[command.index].end);
            syncLinkedVoices(false, previousTargetMask | spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        }
        case RealtimeCommandType::RobinSpreadAlgorithm:
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            spreadSlots_[command.index].algorithm = static_cast<RobinSpreadAlgorithm>(command.code);
            syncLinkedVoices(false, spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        case RealtimeCommandType::RobinSpreadDepth:
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            spreadSlots_[command.index].depth = std::clamp(command.value, 0.0f, 1.0f);
            syncLinkedVoices(false, spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        case RealtimeCommandType::RobinSpreadStart:
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            spreadSlots_[command.index].start =
                clampSpreadSlotValue(spreadSlots_[command.index].target, command.value);
            syncLinkedVoices(false, spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        case RealtimeCommandType::RobinSpreadEnd:
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            spreadSlots_[command.index].end =
                clampSpreadSlotValue(spreadSlots_[command.index].target, command.value);
            syncLinkedVoices(false, spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        case RealtimeCommandType::RobinSpreadSeed:
            if (command.index >= spreadSlots_.size()) {
                return;
            }
            spreadSlots_[command.index].seed = static_cast<std::uint32_t>(std::clamp(command.value, 1.0f, 9999.0f));
            syncLinkedVoices(false, spreadTargetSyncMask(spreadSlots_[command.index].target));
            break;
        case RealtimeCommandType::RobinVoiceActive:
            if (command.index >= voices_.size()) {
                return;
            }
            voices_[command.index].active = command.value >= 0.5f;
            syncAllVoices();
            break;
        case RealtimeCommandType::RobinVoiceLinkedToMaster:
            if (command.index >= voices_.size()) {
                return;
            }
            voices_[command.index].linkedToMaster = command.value >= 0.5f;
            syncAllVoices();
            break;
        case RealtimeCommandType::RobinVoiceResetToMasterState:
            if (command.index >= voices_.size() || command.value < 0.5f) {
                return;
            }
            copyMasterStateToVoice(voices_[command.index]);
            if (!voices_[command.index].linkedToMaster) {
                syncVoiceState(command.index);
            }
            break;
        case RealtimeCommandType::RobinVoiceOutputEnabled:
            if (command.index >= voices_.size() || command.subIndex >= voices_[command.index].outputs.size()) {
                return;
            }
            voices_[command.index].outputs[command.subIndex] = command.value >= 0.5f;
            routingPreset_ = RoutingPreset::Custom;
            resetRoutingState();
            sourceNode_.synth().setVoiceOutputEnabled(
                command.index,
                command.subIndex,
                voices_[command.index].outputs[command.subIndex]);
            break;
        case RealtimeCommandType::RobinVoiceFrequency:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].frequency = std::clamp(command.value, 20.0f, 20000.0f);
            syncVoiceFrequency(command.index);
            break;
        case RealtimeCommandType::RobinVoiceGain:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].gain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.synth().setVoiceGain(command.index, voices_[command.index].gain);
            break;
        case RealtimeCommandType::RobinVoiceVcfCutoffHz:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].vcf.cutoffHz = std::clamp(command.value, 20.0f, 20000.0f);
            sourceNode_.synth().setVoiceFilterCutoffHz(command.index, voices_[command.index].vcf.cutoffHz);
            break;
        case RealtimeCommandType::RobinVoiceVcfResonance:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].vcf.resonance = std::clamp(command.value, 0.1f, 10.0f);
            sourceNode_.synth().setVoiceFilterResonance(command.index, voices_[command.index].vcf.resonance);
            break;
        case RealtimeCommandType::RobinVoiceEnvVcfAttackMs:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envVcf.attackMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.synth().setVoiceFilterEnvelopeAttackSeconds(command.index, voices_[command.index].envVcf.attackMs / 1000.0f);
            break;
        case RealtimeCommandType::RobinVoiceEnvVcfDecayMs:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envVcf.decayMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.synth().setVoiceFilterEnvelopeDecaySeconds(command.index, voices_[command.index].envVcf.decayMs / 1000.0f);
            break;
        case RealtimeCommandType::RobinVoiceEnvVcfSustain:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envVcf.sustain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.synth().setVoiceFilterEnvelopeSustainLevel(command.index, voices_[command.index].envVcf.sustain);
            break;
        case RealtimeCommandType::RobinVoiceEnvVcfReleaseMs:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envVcf.releaseMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.synth().setVoiceFilterEnvelopeReleaseSeconds(command.index, voices_[command.index].envVcf.releaseMs / 1000.0f);
            break;
        case RealtimeCommandType::RobinVoiceEnvVcfAmount:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envVcf.amount = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.synth().setVoiceFilterEnvelopeAmount(command.index, voices_[command.index].envVcf.amount);
            break;
        case RealtimeCommandType::RobinVoiceEnvelopeAttackMs:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envelope.attackMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.synth().setVoiceEnvelopeAttackSeconds(command.index, voices_[command.index].envelope.attackMs / 1000.0f);
            break;
        case RealtimeCommandType::RobinVoiceEnvelopeDecayMs:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envelope.decayMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.synth().setVoiceEnvelopeDecaySeconds(command.index, voices_[command.index].envelope.decayMs / 1000.0f);
            break;
        case RealtimeCommandType::RobinVoiceEnvelopeSustain:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envelope.sustain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.synth().setVoiceEnvelopeSustainLevel(command.index, voices_[command.index].envelope.sustain);
            break;
        case RealtimeCommandType::RobinVoiceEnvelopeReleaseMs:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            voices_[command.index].envelope.releaseMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.synth().setVoiceEnvelopeReleaseSeconds(command.index, voices_[command.index].envelope.releaseMs / 1000.0f);
            break;
        case RealtimeCommandType::RobinMasterOscillatorEnabled:
        case RealtimeCommandType::RobinMasterOscillatorGain:
        case RealtimeCommandType::RobinMasterOscillatorRelative:
        case RealtimeCommandType::RobinMasterOscillatorFrequency: {
            if (command.index >= masterOscillators_.size()) {
                return;
            }
            auto& oscillator = masterOscillators_[command.index];
            if (command.type == RealtimeCommandType::RobinMasterOscillatorEnabled) {
                oscillator.enabled = command.value >= 0.5f;
            } else if (command.type == RealtimeCommandType::RobinMasterOscillatorGain) {
                oscillator.gain = std::clamp(command.value, 0.0f, 1.0f);
            } else if (command.type == RealtimeCommandType::RobinMasterOscillatorRelative) {
                const bool nextRelative = command.value >= 0.5f;
                if (oscillator.relativeToVoice != nextRelative) {
                    if (nextRelative) {
                        oscillator.frequencyValue = static_cast<float>(std::clamp(
                            oscillator.frequencyValue / std::max(1.0f, baseFrequencyHz_),
                            0.01f,
                            8.0f));
                    } else {
                        oscillator.frequencyValue = static_cast<float>(std::clamp(
                            oscillator.frequencyValue * std::max(1.0f, baseFrequencyHz_),
                            1.0f,
                            20000.0f));
                    }
                }
                oscillator.relativeToVoice = nextRelative;
            } else if (command.type == RealtimeCommandType::RobinMasterOscillatorFrequency) {
                oscillator.frequencyValue = oscillator.relativeToVoice
                    ? std::clamp(command.value, 0.01f, 8.0f)
                    : std::clamp(command.value, 1.0f, 20000.0f);
            }
            syncLinkedVoices(false, kSyncOscillators);
            break;
        }
        case RealtimeCommandType::RobinMasterOscillatorWaveform: {
            if (command.index >= masterOscillators_.size()) {
                return;
            }
            const auto waveform = static_cast<dsp::Waveform>(command.code);
            masterOscillators_[command.index].waveform = waveform;
            syncLinkedVoices(false, kSyncOscillators);
            if (debugOscillatorParams_) {
                std::ostringstream pathBuilder;
                pathBuilder << "sources.robin.oscillator." << command.index << ".waveform";
                logMasterOscillatorUpdate(pathBuilder.str(), waveformToString(waveform));
            }
            break;
        }
        case RealtimeCommandType::RobinVoiceOscillatorEnabled:
        case RealtimeCommandType::RobinVoiceOscillatorGain:
        case RealtimeCommandType::RobinVoiceOscillatorRelative:
        case RealtimeCommandType::RobinVoiceOscillatorFrequency: {
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            if (command.subIndex >= voices_[command.index].oscillators.size()) {
                return;
            }
            auto& oscillator = voices_[command.index].oscillators[command.subIndex];
            if (command.type == RealtimeCommandType::RobinVoiceOscillatorEnabled) {
                oscillator.enabled = command.value >= 0.5f;
                sourceNode_.synth().setOscillatorEnabled(command.index, command.subIndex, oscillator.enabled);
            } else if (command.type == RealtimeCommandType::RobinVoiceOscillatorGain) {
                oscillator.gain = std::clamp(command.value, 0.0f, 1.0f);
                sourceNode_.synth().setOscillatorGain(command.index, command.subIndex, oscillator.gain);
            } else if (command.type == RealtimeCommandType::RobinVoiceOscillatorRelative) {
                const bool nextRelative = command.value >= 0.5f;
                if (oscillator.relativeToVoice != nextRelative) {
                    if (nextRelative) {
                        oscillator.frequencyValue = static_cast<float>(std::clamp(
                            oscillator.frequencyValue / std::max(1.0f, voices_[command.index].frequency),
                            0.01f,
                            8.0f));
                    } else {
                        oscillator.frequencyValue = static_cast<float>(std::clamp(
                            oscillator.frequencyValue * std::max(1.0f, voices_[command.index].frequency),
                            1.0f,
                            20000.0f));
                    }
                }
                oscillator.relativeToVoice = nextRelative;
                sourceNode_.synth().setOscillatorRelativeToVoice(command.index, command.subIndex, oscillator.relativeToVoice);
                sourceNode_.synth().setOscillatorFrequency(command.index, command.subIndex, oscillator.frequencyValue);
            } else if (command.type == RealtimeCommandType::RobinVoiceOscillatorFrequency) {
                oscillator.frequencyValue = oscillator.relativeToVoice
                    ? std::clamp(command.value, 0.01f, 8.0f)
                    : std::clamp(command.value, 1.0f, 20000.0f);
                sourceNode_.synth().setOscillatorFrequency(command.index, command.subIndex, oscillator.frequencyValue);
            }
            break;
        }
        case RealtimeCommandType::RobinVoiceOscillatorWaveform:
            if (command.index >= voices_.size() || voices_[command.index].linkedToMaster) {
                return;
            }
            if (command.subIndex >= voices_[command.index].oscillators.size()) {
                return;
            }
            voices_[command.index].oscillators[command.subIndex].waveform = static_cast<dsp::Waveform>(command.code);
            sourceNode_.synth().setOscillatorWaveform(
                command.index,
                command.subIndex,
                voices_[command.index].oscillators[command.subIndex].waveform);
            break;
        default:
            break;
    }
}

void Robin::appendStateJson(std::ostringstream& json) const {
    json << "{"
         << "\"implemented\":true,"
         << "\"playable\":true,"
         << "\"voiceCount\":" << voices_.size() << ","
         << "\"oscillatorsPerVoice\":" << oscillatorsPerVoice_ << ","
         << "\"frequency\":" << baseFrequencyHz_ << ","
         << "\"gain\":" << masterVoiceGain_ << ","
         << "\"transposeSemitones\":" << pitchState_.transposeSemitones << ","
         << "\"fineTuneCents\":" << pitchState_.fineTuneCents << ","
         << "\"routingPreset\":\"" << escapeJson(routingPresetToString(routingPreset_)) << "\","
         << "\"vcf\":{"
         << "\"cutoffHz\":" << vcfState_.cutoffHz << ","
         << "\"resonance\":" << vcfState_.resonance
         << "},"
         << "\"envVcf\":{"
         << "\"attackMs\":" << envVcfState_.attackMs << ","
         << "\"decayMs\":" << envVcfState_.decayMs << ","
         << "\"sustain\":" << envVcfState_.sustain << ","
         << "\"releaseMs\":" << envVcfState_.releaseMs << ","
         << "\"amount\":" << envVcfState_.amount
         << "},"
         << "\"envelope\":{"
         << "\"attackMs\":" << envelopeState_.attackMs << ","
         << "\"decayMs\":" << envelopeState_.decayMs << ","
         << "\"sustain\":" << envelopeState_.sustain << ","
         << "\"releaseMs\":" << envelopeState_.releaseMs
         << "},"
         << "\"spreadSlots\":[";

    for (std::size_t slotIndex = 0; slotIndex < spreadSlots_.size(); ++slotIndex) {
        if (slotIndex > 0) {
            json << ",";
        }

        const auto& slot = spreadSlots_[slotIndex];
        json << "{"
             << "\"index\":" << slotIndex << ","
             << "\"enabled\":" << (slot.enabled ? "true" : "false") << ","
             << "\"target\":\"" << escapeJson(spreadTargetToString(slot.target)) << "\","
             << "\"algorithm\":\"" << escapeJson(spreadAlgorithmToString(slot.algorithm)) << "\","
             << "\"depth\":" << slot.depth << ","
             << "\"start\":" << slot.start << ","
             << "\"end\":" << slot.end << ","
             << "\"seed\":" << slot.seed
             << "}";
    }

    json << "],"
         << "\"lfo\":{"
         << "\"enabled\":" << (lfoState_.enabled ? "true" : "false") << ","
         << "\"depth\":" << lfoState_.depth << ","
         << "\"phaseSpreadDegrees\":" << lfoState_.phaseSpreadDegrees << ","
         << "\"polarityFlip\":" << (lfoState_.polarityFlip ? "true" : "false") << ","
         << "\"unlinkedOutputs\":" << (lfoState_.unlinkedOutputs ? "true" : "false") << ","
         << "\"clockLinked\":" << (lfoState_.clockLinked ? "true" : "false") << ","
         << "\"tempoBpm\":" << lfoState_.tempoBpm << ","
         << "\"rateMultiplier\":" << lfoState_.rateMultiplier << ","
         << "\"fixedFrequencyHz\":" << lfoState_.fixedFrequencyHz << ","
         << "\"waveform\":\"" << escapeJson(lfoWaveformToString(lfoState_.waveform)) << "\""
         << "},"
         << "\"masterOscillators\":[";

    for (std::size_t oscillatorIndex = 0; oscillatorIndex < masterOscillators_.size(); ++oscillatorIndex) {
        if (oscillatorIndex > 0) {
            json << ",";
        }

        const auto& oscillator = masterOscillators_[oscillatorIndex];
        json << "{"
             << "\"index\":" << oscillatorIndex << ","
             << "\"enabled\":" << (oscillator.enabled ? "true" : "false") << ","
             << "\"gain\":" << oscillator.gain << ","
             << "\"relativeToVoice\":" << (oscillator.relativeToVoice ? "true" : "false") << ","
             << "\"frequencyValue\":" << oscillator.frequencyValue << ","
             << "\"waveform\":\"" << escapeJson(waveformToString(oscillator.waveform)) << "\""
             << "}";
    }

    json << "],\"voices\":[";
    for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (voiceIndex > 0) {
            json << ",";
        }

        const auto& voice = voices_[voiceIndex];
        json << "{"
             << "\"index\":" << voiceIndex << ","
             << "\"active\":" << (voice.active ? "true" : "false") << ","
             << "\"linkedToMaster\":" << (voice.linkedToMaster ? "true" : "false") << ","
             << "\"frequency\":" << voice.frequency << ","
             << "\"gain\":" << voice.gain << ","
             << "\"vcf\":{"
             << "\"cutoffHz\":" << voice.vcf.cutoffHz << ","
             << "\"resonance\":" << voice.vcf.resonance
             << "},"
             << "\"envVcf\":{"
             << "\"attackMs\":" << voice.envVcf.attackMs << ","
             << "\"decayMs\":" << voice.envVcf.decayMs << ","
             << "\"sustain\":" << voice.envVcf.sustain << ","
             << "\"releaseMs\":" << voice.envVcf.releaseMs << ","
             << "\"amount\":" << voice.envVcf.amount
             << "},"
             << "\"envelope\":{"
             << "\"attackMs\":" << voice.envelope.attackMs << ","
             << "\"decayMs\":" << voice.envelope.decayMs << ","
             << "\"sustain\":" << voice.envelope.sustain << ","
             << "\"releaseMs\":" << voice.envelope.releaseMs
             << "},"
             << "\"outputs\":[";

        for (std::size_t outputIndex = 0; outputIndex < voice.outputs.size(); ++outputIndex) {
            if (outputIndex > 0) {
                json << ",";
            }
            json << (voice.outputs[outputIndex] ? "true" : "false");
        }

        json << "],\"oscillators\":[";
        for (std::size_t oscillatorIndex = 0; oscillatorIndex < voice.oscillators.size(); ++oscillatorIndex) {
            if (oscillatorIndex > 0) {
                json << ",";
            }

            const auto& oscillator = voice.oscillators[oscillatorIndex];
            json << "{"
                 << "\"index\":" << oscillatorIndex << ","
                 << "\"enabled\":" << (oscillator.enabled ? "true" : "false") << ","
                 << "\"gain\":" << oscillator.gain << ","
                 << "\"relativeToVoice\":" << (oscillator.relativeToVoice ? "true" : "false") << ","
                 << "\"frequencyValue\":" << oscillator.frequencyValue << ","
                 << "\"waveform\":\"" << escapeJson(waveformToString(oscillator.waveform)) << "\""
                 << "}";
        }

        json << "]}";
    }
    json << "]}";
}

bool Robin::implemented() const {
    return true;
}

bool Robin::playable() const {
    return true;
}

RobinStateSnapshot Robin::stateSnapshot() const {
    RobinStateSnapshot state;
    state.voices = voices_;
    state.masterOscillators = masterOscillators_;
    state.pitch = pitchState_;
    state.lfo = lfoState_;
    state.routingPreset = routingPreset_;
    state.envelope = envelopeState_;
    state.masterVoiceGain = masterVoiceGain_;
    state.vcf = vcfState_;
    state.envVcf = envVcfState_;
    state.spreadSlots = spreadSlots_;
    state.baseFrequencyHz = baseFrequencyHz_;
    state.oscillatorsPerVoice = oscillatorsPerVoice_;
    state.outputChannelCount = outputChannelCount_;
    return state;
}

void Robin::applyStateSnapshot(const RobinStateSnapshot& state) {
    const std::uint32_t voiceCount = std::max<std::uint32_t>(
        1u,
        static_cast<std::uint32_t>(std::max<std::size_t>(1u, state.voices.size())));
    const std::uint32_t oscillatorCount = std::max<std::uint32_t>(1u, state.oscillatorsPerVoice);
    const std::uint32_t outputCount = std::max<std::uint32_t>(1u, state.outputChannelCount);

    configureStructure(voiceCount, oscillatorCount, outputCount);

    pitchState_ = state.pitch;
    lfoState_ = state.lfo;
    routingPreset_ = state.routingPreset;
    envelopeState_ = state.envelope;
    masterVoiceGain_ = std::clamp(state.masterVoiceGain, 0.0f, 1.0f);
    vcfState_ = state.vcf;
    envVcfState_ = state.envVcf;
    spreadSlots_ = state.spreadSlots;
    baseFrequencyHz_ = std::clamp(state.baseFrequencyHz, 20.0f, 20000.0f);

    const std::size_t masterOscillatorCount = std::min(masterOscillators_.size(), state.masterOscillators.size());
    for (std::size_t oscillatorIndex = 0; oscillatorIndex < masterOscillatorCount; ++oscillatorIndex) {
        masterOscillators_[oscillatorIndex] = state.masterOscillators[oscillatorIndex];
    }

    const std::size_t voiceCountToCopy = std::min(voices_.size(), state.voices.size());
    for (std::size_t voiceIndex = 0; voiceIndex < voiceCountToCopy; ++voiceIndex) {
        voices_[voiceIndex] = state.voices[voiceIndex];
    }

    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        auto& voice = voices_[voiceIndex];
        voice.outputs.resize(outputCount, false);
        if (std::none_of(voice.outputs.begin(), voice.outputs.end(), [](bool enabled) { return enabled; })
            && !voice.outputs.empty()) {
            voice.outputs[voiceIndex % voice.outputs.size()] = true;
        }
        voice.oscillators.resize(oscillatorCount);
    }

    sourceNode_.synth().clearNotes();
    voiceAssignments_.clear();
    voiceReleaseUntil_.assign(voices_.size(), std::chrono::steady_clock::time_point::min());
    nextVoiceCursor_ = 0;
    autoActivatedVoice0_ = false;
    resetRoutingState();
    syncLfo();
    syncAllVoices();
}

audio::Synth& Robin::synth() {
    return sourceNode_.synth();
}

const audio::Synth& Robin::synth() const {
    return sourceNode_.synth();
}

const char* Robin::waveformToString(dsp::Waveform waveform) {
    switch (waveform) {
        case dsp::Waveform::Square:
            return "square";
        case dsp::Waveform::Triangle:
            return "triangle";
        case dsp::Waveform::Saw:
            return "saw";
        case dsp::Waveform::Noise:
            return "noise";
        case dsp::Waveform::Sine:
        default:
            return "sine";
    }
}

bool Robin::tryParseWaveform(std::string_view value, dsp::Waveform& waveform) {
    if (value == "sine") {
        waveform = dsp::Waveform::Sine;
        return true;
    }
    if (value == "square") {
        waveform = dsp::Waveform::Square;
        return true;
    }
    if (value == "triangle") {
        waveform = dsp::Waveform::Triangle;
        return true;
    }
    if (value == "saw") {
        waveform = dsp::Waveform::Saw;
        return true;
    }
    if (value == "noise") {
        waveform = dsp::Waveform::Noise;
        return true;
    }
    return false;
}

const char* Robin::lfoWaveformToString(dsp::LfoWaveform waveform) {
    switch (waveform) {
        case dsp::LfoWaveform::Triangle:
            return "triangle";
        case dsp::LfoWaveform::SawDown:
            return "saw-down";
        case dsp::LfoWaveform::SawUp:
            return "saw-up";
        case dsp::LfoWaveform::Random:
            return "random";
        case dsp::LfoWaveform::Sine:
        default:
            return "sine";
    }
}

bool Robin::tryParseLfoWaveform(std::string_view value, dsp::LfoWaveform& waveform) {
    if (value == "sine") {
        waveform = dsp::LfoWaveform::Sine;
        return true;
    }
    if (value == "triangle") {
        waveform = dsp::LfoWaveform::Triangle;
        return true;
    }
    if (value == "saw-down") {
        waveform = dsp::LfoWaveform::SawDown;
        return true;
    }
    if (value == "saw-up") {
        waveform = dsp::LfoWaveform::SawUp;
        return true;
    }
    if (value == "random") {
        waveform = dsp::LfoWaveform::Random;
        return true;
    }
    return false;
}

const char* Robin::routingPresetToString(RoutingPreset preset) {
    switch (preset) {
        case RoutingPreset::Forward:
            return "forward";
        case RoutingPreset::Backward:
            return "backward";
        case RoutingPreset::Random:
            return "random";
        case RoutingPreset::AllOutputs:
            return "all-outputs";
        case RoutingPreset::Custom:
            return "custom";
        case RoutingPreset::RoundRobin:
        default:
            return "round-robin";
    }
}

bool Robin::tryParseRoutingPreset(std::string_view value, RoutingPreset& preset) {
    if (value == "forward") {
        preset = RoutingPreset::Forward;
        return true;
    }
    if (value == "backward") {
        preset = RoutingPreset::Backward;
        return true;
    }
    if (value == "random") {
        preset = RoutingPreset::Random;
        return true;
    }
    if (value == "round-robin") {
        preset = RoutingPreset::RoundRobin;
        return true;
    }
    if (value == "all-outputs") {
        preset = RoutingPreset::AllOutputs;
        return true;
    }
    if (value == "custom") {
        preset = RoutingPreset::Custom;
        return true;
    }
    return false;
}

const char* Robin::spreadTargetToString(RobinSpreadTarget target) {
    switch (target) {
        case RobinSpreadTarget::VcfResonance:
            return "vcf-resonance";
        case RobinSpreadTarget::EnvVcfAmount:
            return "env-vcf-amount";
        case RobinSpreadTarget::EnvVcfAttack:
            return "env-vcf-attack";
        case RobinSpreadTarget::EnvVcfDecay:
            return "env-vcf-decay";
        case RobinSpreadTarget::EnvVcfRelease:
            return "env-vcf-release";
        case RobinSpreadTarget::AmpAttack:
            return "amp-attack";
        case RobinSpreadTarget::AmpDecay:
            return "amp-decay";
        case RobinSpreadTarget::AmpRelease:
            return "amp-release";
        case RobinSpreadTarget::OscLevel:
            return "osc-level";
        case RobinSpreadTarget::OscDetune:
            return "osc-detune";
        case RobinSpreadTarget::VcfCutoff:
        default:
            return "vcf-cutoff";
    }
}

bool Robin::tryParseSpreadTarget(std::string_view value, RobinSpreadTarget& target) {
    if (value == "vcf-cutoff") {
        target = RobinSpreadTarget::VcfCutoff;
        return true;
    }
    if (value == "vcf-resonance") {
        target = RobinSpreadTarget::VcfResonance;
        return true;
    }
    if (value == "env-vcf-amount") {
        target = RobinSpreadTarget::EnvVcfAmount;
        return true;
    }
    if (value == "env-vcf-attack") {
        target = RobinSpreadTarget::EnvVcfAttack;
        return true;
    }
    if (value == "env-vcf-decay") {
        target = RobinSpreadTarget::EnvVcfDecay;
        return true;
    }
    if (value == "env-vcf-release") {
        target = RobinSpreadTarget::EnvVcfRelease;
        return true;
    }
    if (value == "amp-attack") {
        target = RobinSpreadTarget::AmpAttack;
        return true;
    }
    if (value == "amp-decay") {
        target = RobinSpreadTarget::AmpDecay;
        return true;
    }
    if (value == "amp-release") {
        target = RobinSpreadTarget::AmpRelease;
        return true;
    }
    if (value == "osc-level") {
        target = RobinSpreadTarget::OscLevel;
        return true;
    }
    if (value == "osc-detune") {
        target = RobinSpreadTarget::OscDetune;
        return true;
    }
    return false;
}

const char* Robin::spreadAlgorithmToString(RobinSpreadAlgorithm algorithm) {
    switch (algorithm) {
        case RobinSpreadAlgorithm::Random:
            return "random";
        case RobinSpreadAlgorithm::Alternating:
            return "alternating";
        case RobinSpreadAlgorithm::Linear:
        default:
            return "linear";
    }
}

bool Robin::tryParseSpreadAlgorithm(std::string_view value, RobinSpreadAlgorithm& algorithm) {
    if (value == "linear") {
        algorithm = RobinSpreadAlgorithm::Linear;
        return true;
    }
    if (value == "random") {
        algorithm = RobinSpreadAlgorithm::Random;
        return true;
    }
    if (value == "alternating") {
        algorithm = RobinSpreadAlgorithm::Alternating;
        return true;
    }
    return false;
}

std::string Robin::escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

bool Robin::tryParseIndex(std::string_view value, std::uint32_t& index) {
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, index);
    return result.ec == std::errc{} && result.ptr == end;
}

float Robin::midiNoteToFrequency(int noteNumber) {
    return 440.0f * std::pow(2.0f, static_cast<float>(noteNumber - 69) / 12.0f);
}

float Robin::clampSpreadSlotValue(RobinSpreadTarget target, float value) {
    switch (target) {
        case RobinSpreadTarget::VcfCutoff:
            return std::clamp(value, -36.0f, 36.0f);
        case RobinSpreadTarget::VcfResonance:
            return std::clamp(value, -4.0f, 4.0f);
        case RobinSpreadTarget::EnvVcfAmount:
            return std::clamp(value, -1.5f, 1.5f);
        case RobinSpreadTarget::EnvVcfAttack:
        case RobinSpreadTarget::EnvVcfDecay:
        case RobinSpreadTarget::EnvVcfRelease:
        case RobinSpreadTarget::AmpAttack:
        case RobinSpreadTarget::AmpDecay:
        case RobinSpreadTarget::AmpRelease:
            return std::clamp(value, -95.0f, 500.0f);
        case RobinSpreadTarget::OscLevel:
            return std::clamp(value, -24.0f, 24.0f);
        case RobinSpreadTarget::OscDetune:
            return std::clamp(value, -100.0f, 100.0f);
        default:
            return value;
    }
}

bool Robin::syncMaskIncludes(VoiceSyncMask mask, VoiceSyncMask flag) {
    return (mask & flag) != 0u;
}

Robin::VoiceSyncMask Robin::spreadTargetSyncMask(RobinSpreadTarget target) {
    switch (target) {
        case RobinSpreadTarget::VcfCutoff:
        case RobinSpreadTarget::VcfResonance:
            return kSyncVcf;
        case RobinSpreadTarget::EnvVcfAmount:
        case RobinSpreadTarget::EnvVcfAttack:
        case RobinSpreadTarget::EnvVcfDecay:
        case RobinSpreadTarget::EnvVcfRelease:
            return kSyncEnvVcf;
        case RobinSpreadTarget::AmpAttack:
        case RobinSpreadTarget::AmpDecay:
        case RobinSpreadTarget::AmpRelease:
            return kSyncEnvelope;
        case RobinSpreadTarget::OscLevel:
        case RobinSpreadTarget::OscDetune:
            return kSyncOscillators;
        default:
            return kSyncAll;
    }
}

void Robin::logMasterOscillatorUpdate(std::string_view path, std::string_view valueDescription) const {
    if (!debugOscillatorParams_ || logger_ == nullptr) {
        return;
    }

    logger_->debug(
        "Robin debug: "
        + std::string(path)
        + "=" + std::string(valueDescription)
        + " activeAssignments=" + std::to_string(voiceAssignments_.size()));
}

float Robin::tunedFrequency(float baseFrequencyHz) const {
    const float clampedBaseFrequency = std::clamp(baseFrequencyHz, 20.0f, 20000.0f);
    const float semitoneOffset = pitchState_.transposeSemitones + (pitchState_.fineTuneCents / 100.0f);
    const float tuned = clampedBaseFrequency * std::pow(2.0f, semitoneOffset / 12.0f);
    return std::clamp(tuned, 20.0f, 20000.0f);
}

void Robin::copyMasterStateToVoice(VoiceState& voice) const {
    voice.frequency = baseFrequencyHz_;
    voice.gain = masterVoiceGain_;
    voice.vcf = vcfState_;
    voice.envVcf = envVcfState_;
    voice.envelope = envelopeState_;
    voice.oscillators = masterOscillators_;
}

void Robin::syncLinkedVoices(bool syncFrequency, VoiceSyncMask syncMask) {
    std::vector<int> linkedPositions;
    std::uint32_t linkedCount = 0;
    buildLinkedVoiceSpreadIndex(linkedPositions, linkedCount);

    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (!voices_[voiceIndex].linkedToMaster) {
            continue;
        }
        syncVoiceState(voiceIndex, syncFrequency, syncMask, &linkedPositions, linkedCount);
    }
}

void Robin::syncVoiceFrequency(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    const auto& voice = voices_[voiceIndex];
    float baseFrequency = voice.linkedToMaster ? baseFrequencyHz_ : voice.frequency;

    const auto assignment = std::find_if(
        voiceAssignments_.begin(),
        voiceAssignments_.end(),
        [voiceIndex](const RobinVoiceAssignment& currentAssignment) {
            return currentAssignment.voiceIndex == voiceIndex;
        });
    if (assignment != voiceAssignments_.end()) {
        baseFrequency = midiNoteToFrequency(assignment->noteNumber);
    }

    sourceNode_.synth().setVoiceFrequency(voiceIndex, tunedFrequency(baseFrequency));
}

void Robin::syncAssignedVoiceFrequencies() {
    for (const auto& assignment : voiceAssignments_) {
        syncVoiceFrequency(assignment.voiceIndex);
    }
}

void Robin::buildLinkedVoiceSpreadIndex(std::vector<int>& linkedPositions, std::uint32_t& linkedCount) const {
    linkedPositions.assign(voices_.size(), -1);
    linkedCount = 0;

    for (std::uint32_t index = 0; index < voices_.size(); ++index) {
        const auto& currentVoice = voices_[index];
        if (!currentVoice.active || !currentVoice.linkedToMaster) {
            continue;
        }

        linkedPositions[index] = static_cast<int>(linkedCount);
        ++linkedCount;
    }
}

void Robin::applyLinkedVoiceSpread(int linkedPosition,
                                   std::uint32_t linkedCount,
                                   VoiceSyncMask syncMask,
                                   float& voiceGain,
                                   RobinVcfState& vcf,
                                   RobinEnvVcfState& envVcf,
                                   EnvelopeState& envelope,
                                   std::vector<OscillatorState>& oscillators) const {
    if (linkedPosition < 0 || linkedCount == 0) {
        return;
    }

    const float linearT = linkedCount == 1
        ? 0.5f
        : static_cast<float>(linkedPosition) / static_cast<float>(linkedCount - 1);

    for (const auto& slot : spreadSlots_) {
        if (!slot.enabled) {
            continue;
        }
        if (!syncMaskIncludes(syncMask, spreadTargetSyncMask(slot.target))) {
            continue;
        }

        float t = linearT;
        switch (slot.algorithm) {
            case RobinSpreadAlgorithm::Random:
                t = stableRandomUnit(slot.seed, static_cast<std::uint32_t>(linkedPosition));
                break;
            case RobinSpreadAlgorithm::Alternating:
                t = (linkedPosition % 2 == 0) ? 0.0f : 1.0f;
                break;
            case RobinSpreadAlgorithm::Linear:
            default:
                break;
        }

        const float spreadValue = lerp(slot.start, slot.end, t) * std::clamp(slot.depth, 0.0f, 1.0f);
        switch (slot.target) {
            case RobinSpreadTarget::VcfCutoff: {
                const float ratio = std::pow(2.0f, spreadValue / 12.0f);
                vcf.cutoffHz = std::clamp(vcf.cutoffHz * ratio, 20.0f, 20000.0f);
                break;
            }
            case RobinSpreadTarget::VcfResonance:
                vcf.resonance = std::clamp(vcf.resonance + spreadValue, 0.1f, 10.0f);
                break;
            case RobinSpreadTarget::EnvVcfAmount:
                envVcf.amount = std::clamp(envVcf.amount + spreadValue, 0.0f, 1.0f);
                break;
            case RobinSpreadTarget::EnvVcfAttack: {
                const float ratio = std::max(0.05f, 1.0f + (spreadValue / 100.0f));
                envVcf.attackMs = std::clamp(envVcf.attackMs * ratio, 0.0f, 5000.0f);
                break;
            }
            case RobinSpreadTarget::EnvVcfDecay: {
                const float ratio = std::max(0.05f, 1.0f + (spreadValue / 100.0f));
                envVcf.decayMs = std::clamp(envVcf.decayMs * ratio, 0.0f, 5000.0f);
                break;
            }
            case RobinSpreadTarget::EnvVcfRelease: {
                const float ratio = std::max(0.05f, 1.0f + (spreadValue / 100.0f));
                envVcf.releaseMs = std::clamp(envVcf.releaseMs * ratio, 0.0f, 5000.0f);
                break;
            }
            case RobinSpreadTarget::AmpAttack: {
                const float ratio = std::max(0.05f, 1.0f + (spreadValue / 100.0f));
                envelope.attackMs = std::clamp(envelope.attackMs * ratio, 0.0f, 5000.0f);
                break;
            }
            case RobinSpreadTarget::AmpDecay: {
                const float ratio = std::max(0.05f, 1.0f + (spreadValue / 100.0f));
                envelope.decayMs = std::clamp(envelope.decayMs * ratio, 0.0f, 5000.0f);
                break;
            }
            case RobinSpreadTarget::AmpRelease: {
                const float ratio = std::max(0.05f, 1.0f + (spreadValue / 100.0f));
                envelope.releaseMs = std::clamp(envelope.releaseMs * ratio, 0.0f, 5000.0f);
                break;
            }
            case RobinSpreadTarget::OscLevel: {
                const float gainScale = std::pow(10.0f, spreadValue / 20.0f);
                for (auto& oscillator : oscillators) {
                    oscillator.gain = std::clamp(oscillator.gain * gainScale, 0.0f, 1.0f);
                }
                break;
            }
            case RobinSpreadTarget::OscDetune: {
                const float ratio = std::pow(2.0f, spreadValue / 1200.0f);
                for (auto& oscillator : oscillators) {
                    oscillator.frequencyValue = oscillator.relativeToVoice
                        ? std::clamp(oscillator.frequencyValue * ratio, 0.01f, 8.0f)
                        : std::clamp(oscillator.frequencyValue * ratio, 1.0f, 20000.0f);
                }
                break;
            }
        }
    }

    voiceGain = std::clamp(voiceGain, 0.0f, 1.0f);
}

void Robin::syncVoiceState(std::uint32_t voiceIndex,
                           bool syncFrequency,
                           VoiceSyncMask syncMask,
                           const std::vector<int>* linkedPositions,
                           std::uint32_t linkedCount) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    const auto& voice = voices_[voiceIndex];
    const bool needsGain = syncMaskIncludes(syncMask, kSyncGain);
    const bool needsEnvelope = syncMaskIncludes(syncMask, kSyncEnvelope);
    const bool needsVcf = syncMaskIncludes(syncMask, kSyncVcf);
    const bool needsEnvVcf = syncMaskIncludes(syncMask, kSyncEnvVcf);
    const bool needsOscillators = syncMaskIncludes(syncMask, kSyncOscillators);

    auto envelope = needsEnvelope
        ? (voice.linkedToMaster ? envelopeState_ : voice.envelope)
        : EnvelopeState{};
    auto vcf = needsVcf
        ? (voice.linkedToMaster ? vcfState_ : voice.vcf)
        : RobinVcfState{};
    auto envVcf = needsEnvVcf
        ? (voice.linkedToMaster ? envVcfState_ : voice.envVcf)
        : RobinEnvVcfState{};
    auto oscillators = needsOscillators
        ? (voice.linkedToMaster ? masterOscillators_ : voice.oscillators)
        : std::vector<OscillatorState>{};
    float voiceGain = needsGain ? (voice.linkedToMaster ? masterVoiceGain_ : voice.gain) : 0.0f;

    if (voice.linkedToMaster && voice.active) {
        int linkedPosition = -1;
        std::uint32_t effectiveLinkedCount = linkedCount;
        std::vector<int> localLinkedPositions;
        if (linkedPositions != nullptr && voiceIndex < linkedPositions->size()) {
            linkedPosition = (*linkedPositions)[voiceIndex];
        } else {
            buildLinkedVoiceSpreadIndex(localLinkedPositions, effectiveLinkedCount);
            if (voiceIndex < localLinkedPositions.size()) {
                linkedPosition = localLinkedPositions[voiceIndex];
            }
        }
        applyLinkedVoiceSpread(linkedPosition, effectiveLinkedCount, syncMask, voiceGain, vcf, envVcf, envelope, oscillators);
    }

    if (syncMaskIncludes(syncMask, kSyncActive)) {
        sourceNode_.synth().setVoiceActive(voiceIndex, voice.active);
    }
    if (syncFrequency && syncMaskIncludes(syncMask, kSyncFrequency)) {
        syncVoiceFrequency(voiceIndex);
    }
    if (syncMaskIncludes(syncMask, kSyncGain)) {
        sourceNode_.synth().setVoiceGain(voiceIndex, voiceGain);
    }
    if (syncMaskIncludes(syncMask, kSyncEnvelope)) {
        sourceNode_.synth().setVoiceEnvelopeAttackSeconds(voiceIndex, envelope.attackMs / 1000.0f);
        sourceNode_.synth().setVoiceEnvelopeDecaySeconds(voiceIndex, envelope.decayMs / 1000.0f);
        sourceNode_.synth().setVoiceEnvelopeSustainLevel(voiceIndex, envelope.sustain);
        sourceNode_.synth().setVoiceEnvelopeReleaseSeconds(voiceIndex, envelope.releaseMs / 1000.0f);
    }
    if (syncMaskIncludes(syncMask, kSyncVcf)) {
        sourceNode_.synth().setVoiceFilterCutoffHz(voiceIndex, vcf.cutoffHz);
        sourceNode_.synth().setVoiceFilterResonance(voiceIndex, vcf.resonance);
    }
    if (syncMaskIncludes(syncMask, kSyncEnvVcf)) {
        sourceNode_.synth().setVoiceFilterEnvelopeAttackSeconds(voiceIndex, envVcf.attackMs / 1000.0f);
        sourceNode_.synth().setVoiceFilterEnvelopeDecaySeconds(voiceIndex, envVcf.decayMs / 1000.0f);
        sourceNode_.synth().setVoiceFilterEnvelopeSustainLevel(voiceIndex, envVcf.sustain);
        sourceNode_.synth().setVoiceFilterEnvelopeReleaseSeconds(voiceIndex, envVcf.releaseMs / 1000.0f);
        sourceNode_.synth().setVoiceFilterEnvelopeAmount(voiceIndex, envVcf.amount);
    }
    if (syncMaskIncludes(syncMask, kSyncOutputs)) {
        for (std::uint32_t outputIndex = 0; outputIndex < voice.outputs.size(); ++outputIndex) {
            sourceNode_.synth().setVoiceOutputEnabled(voiceIndex, outputIndex, voice.outputs[outputIndex]);
        }
    }
    if (syncMaskIncludes(syncMask, kSyncOscillators)) {
        for (std::uint32_t oscillatorIndex = 0; oscillatorIndex < oscillators.size(); ++oscillatorIndex) {
            sourceNode_.synth().setOscillatorEnabled(voiceIndex, oscillatorIndex, oscillators[oscillatorIndex].enabled);
            sourceNode_.synth().setOscillatorGain(voiceIndex, oscillatorIndex, oscillators[oscillatorIndex].gain);
            sourceNode_.synth().setOscillatorRelativeToVoice(voiceIndex, oscillatorIndex, oscillators[oscillatorIndex].relativeToVoice);
            sourceNode_.synth().setOscillatorFrequency(voiceIndex, oscillatorIndex, oscillators[oscillatorIndex].frequencyValue);
            sourceNode_.synth().setOscillatorWaveform(voiceIndex, oscillatorIndex, oscillators[oscillatorIndex].waveform);
        }
    }
}

void Robin::syncAllVoices() {
    std::vector<int> linkedPositions;
    std::uint32_t linkedCount = 0;
    buildLinkedVoiceSpreadIndex(linkedPositions, linkedCount);

    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        syncVoiceState(voiceIndex, true, kSyncAll, &linkedPositions, linkedCount);
    }
}

void Robin::syncLfo() {
    sourceNode_.synth().setLfoEnabled(lfoState_.enabled);
    sourceNode_.synth().setLfoWaveform(lfoState_.waveform);
    sourceNode_.synth().setLfoDepth(lfoState_.depth);
    sourceNode_.synth().setLfoPhaseSpreadDegrees(lfoState_.phaseSpreadDegrees);
    sourceNode_.synth().setLfoPolarityFlip(lfoState_.polarityFlip);
    sourceNode_.synth().setLfoUnlinkedOutputs(lfoState_.unlinkedOutputs);
    sourceNode_.synth().setLfoClockLinked(lfoState_.clockLinked);
    sourceNode_.synth().setLfoTempoBpm(lfoState_.tempoBpm);
    sourceNode_.synth().setLfoRateMultiplier(lfoState_.rateMultiplier);
    sourceNode_.synth().setLfoFixedFrequencyHz(lfoState_.fixedFrequencyHz);
}

void Robin::resetRoutingState() {
    const std::uint32_t outputCount = std::max<std::uint32_t>(1, outputChannelCount_);
    forwardOutputCursor_ = 0;
    backwardOutputCursor_ = outputCount - 1;
    roundRobinPool_.clear();
    nextTriggerOutputIndex_.reset();
}

std::uint32_t Robin::computeNextTriggerOutput() {
    const std::uint32_t outputCount = std::max<std::uint32_t>(1, outputChannelCount_);

    switch (routingPreset_) {
        case RoutingPreset::Forward: {
            const std::uint32_t outputIndex = forwardOutputCursor_;
            forwardOutputCursor_ = (forwardOutputCursor_ + 1) % outputCount;
            return outputIndex;
        }
        case RoutingPreset::Backward: {
            const std::uint32_t outputIndex = backwardOutputCursor_;
            backwardOutputCursor_ = backwardOutputCursor_ == 0 ? outputCount - 1 : backwardOutputCursor_ - 1;
            return outputIndex;
        }
        case RoutingPreset::Random:
            return std::uniform_int_distribution<std::uint32_t>(0, outputCount - 1)(routingRandom_);
        case RoutingPreset::RoundRobin:
        default: {
            if (roundRobinPool_.empty()) {
                roundRobinPool_.resize(outputCount);
                for (std::uint32_t outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
                    roundRobinPool_[outputIndex] = outputIndex;
                }
                std::shuffle(roundRobinPool_.begin(), roundRobinPool_.end(), routingRandom_);
            }

            const std::uint32_t outputIndex = roundRobinPool_.back();
            roundRobinPool_.pop_back();
            return outputIndex;
        }
    }
}

void Robin::routeVoiceToOutput(std::uint32_t voiceIndex, std::uint32_t outputIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    auto& outputs = voices_[voiceIndex].outputs;
    std::fill(outputs.begin(), outputs.end(), false);
    if (!outputs.empty() && outputIndex < outputs.size()) {
        outputs[outputIndex] = true;
    }

    for (std::uint32_t channelIndex = 0; channelIndex < outputs.size(); ++channelIndex) {
        sourceNode_.synth().setVoiceOutputEnabled(voiceIndex, channelIndex, outputs[channelIndex]);
    }
}

void Robin::routeVoiceToAllOutputs(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    auto& outputs = voices_[voiceIndex].outputs;
    std::fill(outputs.begin(), outputs.end(), true);
    for (std::uint32_t channelIndex = 0; channelIndex < outputs.size(); ++channelIndex) {
        sourceNode_.synth().setVoiceOutputEnabled(voiceIndex, channelIndex, true);
    }
}

std::uint32_t Robin::allocateVoice() {
    if (voices_.empty()) {
        return 0;
    }

    std::vector<std::uint32_t> activeVoiceIndices;
    activeVoiceIndices.reserve(voices_.size());
    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (voices_[voiceIndex].active) {
            activeVoiceIndices.push_back(voiceIndex);
        }
    }

    if (activeVoiceIndices.empty()) {
        voices_[0].active = true;
        sourceNode_.synth().setVoiceActive(0, true);
        autoActivatedVoice0_ = true;
        activeVoiceIndices.push_back(0);
    }

    const auto now = std::chrono::steady_clock::now();
    const auto isAssigned = [this](std::uint32_t voiceIndex) {
        return std::find_if(
                   voiceAssignments_.begin(),
                   voiceAssignments_.end(),
                   [voiceIndex](const RobinVoiceAssignment& currentAssignment) {
                       return currentAssignment.voiceIndex == voiceIndex;
                   })
            != voiceAssignments_.end();
    };
    const auto isReleaseBusy = [this, now](std::uint32_t voiceIndex) {
        return voiceIndex < voiceReleaseUntil_.size()
            && now < voiceReleaseUntil_[voiceIndex];
    };

    for (std::uint32_t offset = 0; offset < activeVoiceIndices.size(); ++offset) {
        const std::uint32_t poolIndex = (nextVoiceCursor_ + offset) % activeVoiceIndices.size();
        const std::uint32_t voiceIndex = activeVoiceIndices[poolIndex];
        if (!isAssigned(voiceIndex) && !isReleaseBusy(voiceIndex)) {
            nextVoiceCursor_ = (poolIndex + 1) % activeVoiceIndices.size();
            return voiceIndex;
        }
    }

    for (std::uint32_t offset = 0; offset < activeVoiceIndices.size(); ++offset) {
        const std::uint32_t poolIndex = (nextVoiceCursor_ + offset) % activeVoiceIndices.size();
        const std::uint32_t voiceIndex = activeVoiceIndices[poolIndex];
        if (!isAssigned(voiceIndex)) {
            nextVoiceCursor_ = (poolIndex + 1) % activeVoiceIndices.size();
            return voiceIndex;
        }
    }

    const std::uint32_t selectedVoice = activeVoiceIndices[nextVoiceCursor_ % activeVoiceIndices.size()];
    nextVoiceCursor_ = (nextVoiceCursor_ + 1) % activeVoiceIndices.size();

    const auto voiceAssignment = std::find_if(
        voiceAssignments_.begin(),
        voiceAssignments_.end(),
        [selectedVoice](const RobinVoiceAssignment& assignment) {
            return assignment.voiceIndex == selectedVoice;
        });
    if (voiceAssignment != voiceAssignments_.end()) {
        sourceNode_.synth().noteOff(voiceAssignment->voiceIndex);
        if (selectedVoice < voiceReleaseUntil_.size()) {
            const auto& voice = voices_[selectedVoice];
            const float releaseMs = voice.linkedToMaster ? envelopeState_.releaseMs : voice.envelope.releaseMs;
            voiceReleaseUntil_[selectedVoice] =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(std::ceil(releaseMs)));
        }

        const auto heldNote = std::find_if(
            heldNotes_.begin(),
            heldNotes_.end(),
            [noteId = voiceAssignment->noteId](const RobinHeldNote& currentNote) {
                return currentNote.noteId == noteId;
            });
        if (heldNote != heldNotes_.end()) {
            heldNote->voiceIndex.reset();
        }
        voiceAssignments_.erase(voiceAssignment);
    }

    return selectedVoice;
}

}  // namespace synth::app
