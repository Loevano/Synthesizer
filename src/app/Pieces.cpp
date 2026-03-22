#include "synth/app/Pieces.hpp"

#include <algorithm>
#include <sstream>

namespace synth::app {

void Pieces::prepare(double sampleRate, std::uint32_t outputChannels) {
    engine_.prepare(sampleRate, outputChannels);
    syncEngineState();
}

void Pieces::process(float* output,
                     std::uint32_t frames,
                     std::uint32_t channels,
                     bool enabled,
                     float level) {
    engine_.process(output, frames, channels, enabled ? level : 0.0f);
}

void Pieces::clearAllNotes() {
    engine_.clearNotes();
}

void Pieces::noteOn(int noteNumber, float velocity) {
    engine_.noteOn(noteNumber, velocity);
}

void Pieces::noteOff(int noteNumber) {
    engine_.noteOff(noteNumber);
}

RealtimeParamResult Pieces::applyNumericParam(const std::vector<std::string_view>& parts,
                                              double value,
                                              std::string* errorMessage) {
    RealtimeCommand command;
    const auto result = tryBuildRealtimeNumericCommand(parts, value, command, errorMessage);
    if (result != RealtimeParamResult::Applied) {
        return result;
    }

    applyRealtimeCommand(command);
    return RealtimeParamResult::Applied;
}

RealtimeParamResult Pieces::applyStringParam(const std::vector<std::string_view>& parts,
                                             std::string_view value,
                                             std::string* errorMessage) {
    RealtimeCommand command;
    const auto result = tryBuildRealtimeStringCommand(parts, value, command, errorMessage);
    if (result != RealtimeParamResult::Applied) {
        return result;
    }

    applyRealtimeCommand(command);
    return RealtimeParamResult::Applied;
}

RealtimeParamResult Pieces::tryBuildRealtimeNumericCommand(const std::vector<std::string_view>& parts,
                                                           double value,
                                                           RealtimeCommand& command,
                                                           std::string* errorMessage) const {
    if (parts.size() < 3 || parts[0] != "sources" || parts[1] != "pieces") {
        return RealtimeParamResult::NotHandled;
    }

    if (parts.size() == 3 && parts[2] == "grainSizeMs") {
        command.type = RealtimeCommandType::PiecesGrainSizeMs;
        command.value = static_cast<float>(std::clamp(value, 15.0, 1000.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "grainDensityHz") {
        command.type = RealtimeCommandType::PiecesGrainDensityHz;
        command.value = static_cast<float>(std::clamp(value, 0.25, 40.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "activeGrains") {
        command.type = RealtimeCommandType::PiecesActiveGrains;
        command.value = static_cast<float>(std::clamp(value, 1.0, 32.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "position") {
        command.type = RealtimeCommandType::PiecesPosition;
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "positionJitter") {
        command.type = RealtimeCommandType::PiecesPositionJitter;
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "pitchSemitones") {
        command.type = RealtimeCommandType::PiecesPitchSemitones;
        command.value = static_cast<float>(std::clamp(value, -24.0, 24.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "pitchJitterCents") {
        command.type = RealtimeCommandType::PiecesPitchJitterCents;
        command.value = static_cast<float>(std::clamp(value, 0.0, 120.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "outputSpread") {
        command.type = RealtimeCommandType::PiecesOutputSpread;
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 4 && parts[2] == "feedback") {
        if (parts[3] == "amount") {
            command.type = RealtimeCommandType::PiecesFeedbackAmount;
            command.value = static_cast<float>(std::clamp(value, 0.0, 0.97));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "highPassHz") {
            command.type = RealtimeCommandType::PiecesFeedbackHighPassHz;
            command.value = static_cast<float>(std::clamp(value, 20.0, 12000.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "lowPassHz") {
            command.type = RealtimeCommandType::PiecesFeedbackLowPassHz;
            command.value = static_cast<float>(std::clamp(value, 120.0, 18000.0));
            return RealtimeParamResult::Applied;
        }
    }

    if (errorMessage != nullptr && parts.size() >= 3 && parts[0] == "sources" && parts[1] == "pieces") {
        *errorMessage = "Unsupported Pieces numeric parameter path.";
    }
    return RealtimeParamResult::NotHandled;
}

RealtimeParamResult Pieces::tryBuildRealtimeStringCommand(const std::vector<std::string_view>& parts,
                                                          std::string_view value,
                                                          RealtimeCommand& command,
                                                          std::string* errorMessage) const {
    if (parts.size() == 3 && parts[0] == "sources" && parts[1] == "pieces" && parts[2] == "outputAlgorithm") {
        PiecesOutputAlgorithm outputAlgorithm;
        if (!tryParseOutputAlgorithm(value, outputAlgorithm)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid Pieces output algorithm.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::PiecesOutputAlgorithm;
        command.code = static_cast<std::uint32_t>(outputAlgorithm);
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

bool Pieces::handlesRealtimeCommand(RealtimeCommandType type) const {
    switch (type) {
        case RealtimeCommandType::PiecesGrainSizeMs:
        case RealtimeCommandType::PiecesGrainDensityHz:
        case RealtimeCommandType::PiecesActiveGrains:
        case RealtimeCommandType::PiecesPosition:
        case RealtimeCommandType::PiecesPositionJitter:
        case RealtimeCommandType::PiecesPitchSemitones:
        case RealtimeCommandType::PiecesPitchJitterCents:
        case RealtimeCommandType::PiecesOutputAlgorithm:
        case RealtimeCommandType::PiecesOutputSpread:
        case RealtimeCommandType::PiecesFeedbackAmount:
        case RealtimeCommandType::PiecesFeedbackHighPassHz:
        case RealtimeCommandType::PiecesFeedbackLowPassHz:
            return true;
        default:
            return false;
    }
}

void Pieces::applyRealtimeCommand(const RealtimeCommand& command) {
    switch (command.type) {
        case RealtimeCommandType::PiecesGrainSizeMs:
            state_.grainSizeMs = std::clamp(command.value, 15.0f, 1000.0f);
            engine_.setGrainSizeMs(state_.grainSizeMs);
            break;
        case RealtimeCommandType::PiecesGrainDensityHz:
            state_.grainDensityHz = std::clamp(command.value, 0.25f, 40.0f);
            engine_.setGrainDensityHz(state_.grainDensityHz);
            break;
        case RealtimeCommandType::PiecesActiveGrains:
            state_.activeGrains = static_cast<std::uint32_t>(std::clamp(command.value, 1.0f, 32.0f));
            engine_.setActiveGrains(state_.activeGrains);
            break;
        case RealtimeCommandType::PiecesPosition:
            state_.position = std::clamp(command.value, 0.0f, 1.0f);
            engine_.setPosition(state_.position);
            break;
        case RealtimeCommandType::PiecesPositionJitter:
            state_.positionJitter = std::clamp(command.value, 0.0f, 1.0f);
            engine_.setPositionJitter(state_.positionJitter);
            break;
        case RealtimeCommandType::PiecesPitchSemitones:
            state_.pitchSemitones = std::clamp(command.value, -24.0f, 24.0f);
            engine_.setPitchSemitones(state_.pitchSemitones);
            break;
        case RealtimeCommandType::PiecesPitchJitterCents:
            state_.pitchJitterCents = std::clamp(command.value, 0.0f, 120.0f);
            engine_.setPitchJitterCents(state_.pitchJitterCents);
            break;
        case RealtimeCommandType::PiecesOutputAlgorithm:
            state_.outputAlgorithm = static_cast<PiecesOutputAlgorithm>(command.code);
            engine_.setOutputAlgorithm(state_.outputAlgorithm);
            break;
        case RealtimeCommandType::PiecesOutputSpread:
            state_.outputSpread = std::clamp(command.value, 0.0f, 1.0f);
            engine_.setOutputSpread(state_.outputSpread);
            break;
        case RealtimeCommandType::PiecesFeedbackAmount:
            state_.feedback.amount = std::clamp(command.value, 0.0f, 0.97f);
            engine_.setFeedbackAmount(state_.feedback.amount);
            break;
        case RealtimeCommandType::PiecesFeedbackHighPassHz:
            state_.feedback.highPassHz = std::clamp(command.value, 20.0f, 12000.0f);
            engine_.setFeedbackHighPassHz(state_.feedback.highPassHz);
            break;
        case RealtimeCommandType::PiecesFeedbackLowPassHz:
            state_.feedback.lowPassHz = std::clamp(command.value, 120.0f, 18000.0f);
            engine_.setFeedbackLowPassHz(state_.feedback.lowPassHz);
            break;
        default:
            break;
    }
}

void Pieces::appendStateJson(std::ostringstream& json) const {
    json << "{"
         << "\"implemented\":" << (state_.implemented ? "true" : "false") << ","
         << "\"playable\":" << (state_.playable ? "true" : "false") << ","
         << "\"sourceMode\":\"sample\","
         << "\"liveInputSupported\":" << (state_.liveInputSupported ? "true" : "false") << ","
         << "\"sampleName\":\"Built In Texture\","
         << "\"voiceCount\":" << state_.activeGrains << ","
         << "\"activeGrains\":" << state_.activeGrains << ","
         << "\"grainSizeMs\":" << state_.grainSizeMs << ","
         << "\"grainDensityHz\":" << state_.grainDensityHz << ","
         << "\"position\":" << state_.position << ","
         << "\"positionJitter\":" << state_.positionJitter << ","
         << "\"pitchSemitones\":" << state_.pitchSemitones << ","
         << "\"pitchJitterCents\":" << state_.pitchJitterCents << ","
         << "\"outputAlgorithm\":\"" << outputAlgorithmToString(state_.outputAlgorithm) << "\","
         << "\"outputSpread\":" << state_.outputSpread << ","
         << "\"feedback\":{"
         << "\"amount\":" << state_.feedback.amount << ","
         << "\"highPassHz\":" << state_.feedback.highPassHz << ","
         << "\"lowPassHz\":" << state_.feedback.lowPassHz
         << "}"
         << "}";
}

bool Pieces::implemented() const {
    return state_.implemented;
}

bool Pieces::playable() const {
    return state_.playable;
}

const PiecesSourceState& Pieces::state() const {
    return state_;
}

void Pieces::applyState(const PiecesSourceState& state) {
    state_ = state;
    syncEngineState();
}

const char* Pieces::outputAlgorithmToString(PiecesOutputAlgorithm outputAlgorithm) {
    switch (outputAlgorithm) {
        case PiecesOutputAlgorithm::Random:
            return "random";
        case PiecesOutputAlgorithm::PingPong:
            return "ping-pong";
        case PiecesOutputAlgorithm::CenterOut:
            return "center-out";
        case PiecesOutputAlgorithm::Spray:
            return "spray";
        case PiecesOutputAlgorithm::RoundRobin:
        default:
            return "round-robin";
    }
}

bool Pieces::tryParseOutputAlgorithm(std::string_view value, PiecesOutputAlgorithm& outputAlgorithm) {
    if (value == "round-robin") {
        outputAlgorithm = PiecesOutputAlgorithm::RoundRobin;
        return true;
    }
    if (value == "random") {
        outputAlgorithm = PiecesOutputAlgorithm::Random;
        return true;
    }
    if (value == "ping-pong") {
        outputAlgorithm = PiecesOutputAlgorithm::PingPong;
        return true;
    }
    if (value == "center-out") {
        outputAlgorithm = PiecesOutputAlgorithm::CenterOut;
        return true;
    }
    if (value == "spray") {
        outputAlgorithm = PiecesOutputAlgorithm::Spray;
        return true;
    }
    return false;
}

void Pieces::syncEngineState() {
    engine_.setGrainSizeMs(state_.grainSizeMs);
    engine_.setGrainDensityHz(state_.grainDensityHz);
    engine_.setActiveGrains(state_.activeGrains);
    engine_.setPosition(state_.position);
    engine_.setPositionJitter(state_.positionJitter);
    engine_.setPitchSemitones(state_.pitchSemitones);
    engine_.setPitchJitterCents(state_.pitchJitterCents);
    engine_.setOutputAlgorithm(state_.outputAlgorithm);
    engine_.setOutputSpread(state_.outputSpread);
    engine_.setFeedbackAmount(state_.feedback.amount);
    engine_.setFeedbackHighPassHz(state_.feedback.highPassHz);
    engine_.setFeedbackLowPassHz(state_.feedback.lowPassHz);
}

}  // namespace synth::app
