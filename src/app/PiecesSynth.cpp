#include "synth/app/PiecesSynth.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace synth::app {

void PiecesSynth::prepare(double sampleRate, std::uint32_t outputChannels) {
    sourceNode_.prepare(sampleRate, outputChannels);
    resizeOutputs(outputChannels);
    syncNodeState();
}

void PiecesSynth::process(float* output,
                          std::uint32_t frames,
                          std::uint32_t channels,
                          bool enabled,
                          float level) {
    sourceNode_.process(output, frames, channels, enabled, level);
}

void PiecesSynth::clearAllNotes() {
    sourceNode_.clearNotes();
}

void PiecesSynth::noteOn(int noteNumber, float velocity) {
    if (!state_.midiEnabled) {
        return;
    }

    sourceNode_.noteOn(noteNumber, velocity);
}

void PiecesSynth::noteOff(int noteNumber) {
    if (!state_.midiEnabled) {
        return;
    }

    sourceNode_.noteOff(noteNumber);
}

RealtimeParamResult PiecesSynth::applyNumericParam(const std::vector<std::string_view>& parts,
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

RealtimeParamResult PiecesSynth::applyStringParam(const std::vector<std::string_view>& parts,
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

RealtimeParamResult PiecesSynth::tryBuildRealtimeNumericCommand(const std::vector<std::string_view>& parts,
                                                               double value,
                                                               RealtimeCommand& command,
                                                               std::string* errorMessage) const {
    if (parts.size() < 3 || parts[0] != "sources" || parts[1] != "pieces") {
        return RealtimeParamResult::NotHandled;
    }

    if (parts.size() == 3 && parts[2] == "midiEnabled") {
        command.type = RealtimeCommandType::PiecesMidiEnabled;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "gain") {
        command.type = RealtimeCommandType::PiecesGain;
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "rootNote") {
        command.type = RealtimeCommandType::PiecesRootNote;
        command.value = static_cast<float>(std::clamp(value, 0.0, 127.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "transposeSemitones") {
        command.type = RealtimeCommandType::PiecesTransposeSemitones;
        command.value = static_cast<float>(std::clamp(value, -48.0, 48.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "fineTuneCents") {
        command.type = RealtimeCommandType::PiecesFineTuneCents;
        command.value = static_cast<float>(std::clamp(value, -100.0, 100.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "start") {
        command.type = RealtimeCommandType::PiecesStart;
        command.value = static_cast<float>(std::clamp(value, 0.0, 0.999));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "end") {
        command.type = RealtimeCommandType::PiecesEnd;
        command.value = static_cast<float>(std::clamp(value, 0.001, 1.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "loopEnabled") {
        command.type = RealtimeCommandType::PiecesLoopEnabled;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "reverse") {
        command.type = RealtimeCommandType::PiecesReverse;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 4 && parts[2] == "output") {
        std::uint32_t outputIndex = 0;
        const char* begin = parts[3].data();
        const char* end = parts[3].data() + parts[3].size();
        auto parseResult = std::from_chars(begin, end, outputIndex);
        if (parseResult.ec != std::errc{} || parseResult.ptr != end || outputIndex >= state_.outputs.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid pieces output index.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::PiecesOutputEnabled;
        command.index = outputIndex;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 4 && parts[2] == "envelope") {
        if (parts[3] == "attackMs") {
            command.type = RealtimeCommandType::PiecesEnvelopeAttackMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "decayMs") {
            command.type = RealtimeCommandType::PiecesEnvelopeDecayMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "sustain") {
            command.type = RealtimeCommandType::PiecesEnvelopeSustain;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "releaseMs") {
            command.type = RealtimeCommandType::PiecesEnvelopeReleaseMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            return RealtimeParamResult::Applied;
        }
    }

    return RealtimeParamResult::NotHandled;
}

RealtimeParamResult PiecesSynth::tryBuildRealtimeStringCommand(const std::vector<std::string_view>& parts,
                                                              std::string_view value,
                                                              RealtimeCommand& command,
                                                              std::string* errorMessage) const {
    if (parts.size() < 3 || parts[0] != "sources" || parts[1] != "pieces") {
        return RealtimeParamResult::NotHandled;
    }

    if (parts.size() == 3 && parts[2] == "playbackMode") {
        audio::SamplePlaybackMode mode = audio::SamplePlaybackMode::Gate;
        if (!parsePlaybackMode(value, mode)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid pieces playback mode.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::PiecesPlaybackMode;
        command.code = static_cast<std::uint32_t>(mode);
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

bool PiecesSynth::handlesRealtimeCommand(RealtimeCommandType type) const {
    switch (type) {
        case RealtimeCommandType::PiecesSampleBuffer:
        case RealtimeCommandType::PiecesMidiEnabled:
        case RealtimeCommandType::PiecesGain:
        case RealtimeCommandType::PiecesRootNote:
        case RealtimeCommandType::PiecesTransposeSemitones:
        case RealtimeCommandType::PiecesFineTuneCents:
        case RealtimeCommandType::PiecesStart:
        case RealtimeCommandType::PiecesEnd:
        case RealtimeCommandType::PiecesLoopEnabled:
        case RealtimeCommandType::PiecesPlaybackMode:
        case RealtimeCommandType::PiecesReverse:
        case RealtimeCommandType::PiecesEnvelopeAttackMs:
        case RealtimeCommandType::PiecesEnvelopeDecayMs:
        case RealtimeCommandType::PiecesEnvelopeSustain:
        case RealtimeCommandType::PiecesEnvelopeReleaseMs:
        case RealtimeCommandType::PiecesOutputEnabled:
            return true;
        default:
            return false;
    }
}

void PiecesSynth::applyRealtimeCommand(const RealtimeCommand& command) {
    switch (command.type) {
        case RealtimeCommandType::PiecesSampleBuffer:
            state_.sampleBuffer = command.sampleBuffer;
            state_.sampleLoaded = state_.sampleBuffer != nullptr && !state_.sampleBuffer->empty();
            if (state_.sampleLoaded) {
                state_.samplePath = state_.sampleBuffer->sourcePath;
                state_.sampleName = state_.sampleBuffer->displayName;
                state_.sampleRate = state_.sampleBuffer->sampleRate;
                state_.sampleFrames = state_.sampleBuffer->frameCount();
                state_.samplePeaks = buildWaveformPeaks(*state_.sampleBuffer);
            } else {
                state_.samplePath.clear();
                state_.sampleName.clear();
                state_.sampleRate = 0.0;
                state_.sampleFrames = 0;
                state_.samplePeaks.clear();
            }
            sourceNode_.setSampleBuffer(state_.sampleBuffer);
            break;
        case RealtimeCommandType::PiecesMidiEnabled:
            state_.midiEnabled = command.value >= 0.5f;
            sourceNode_.setMidiEnabled(state_.midiEnabled);
            break;
        case RealtimeCommandType::PiecesGain:
            state_.gain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.setGain(state_.gain);
            break;
        case RealtimeCommandType::PiecesRootNote:
            state_.rootNote = std::clamp(static_cast<int>(command.value), 0, 127);
            sourceNode_.setRootNote(state_.rootNote);
            break;
        case RealtimeCommandType::PiecesTransposeSemitones:
            state_.transposeSemitones = std::clamp(command.value, -48.0f, 48.0f);
            sourceNode_.setTransposeSemitones(state_.transposeSemitones);
            break;
        case RealtimeCommandType::PiecesFineTuneCents:
            state_.fineTuneCents = std::clamp(command.value, -100.0f, 100.0f);
            sourceNode_.setFineTuneCents(state_.fineTuneCents);
            break;
        case RealtimeCommandType::PiecesStart:
            state_.start = std::clamp(command.value, 0.0f, 0.999f);
            if (state_.end <= state_.start) {
                state_.end = std::min(1.0f, state_.start + 0.001f);
            }
            sourceNode_.setStartPosition(state_.start);
            sourceNode_.setEndPosition(state_.end);
            break;
        case RealtimeCommandType::PiecesEnd:
            state_.end = std::clamp(command.value, 0.001f, 1.0f);
            if (state_.end <= state_.start) {
                state_.start = std::max(0.0f, state_.end - 0.001f);
            }
            sourceNode_.setStartPosition(state_.start);
            sourceNode_.setEndPosition(state_.end);
            break;
        case RealtimeCommandType::PiecesLoopEnabled:
            state_.loopEnabled = command.value >= 0.5f;
            state_.playbackMode = state_.loopEnabled ? audio::SamplePlaybackMode::Loop : audio::SamplePlaybackMode::Gate;
            sourceNode_.setPlaybackMode(state_.playbackMode);
            break;
        case RealtimeCommandType::PiecesPlaybackMode:
            state_.playbackMode = static_cast<audio::SamplePlaybackMode>(command.code);
            if (state_.playbackMode != audio::SamplePlaybackMode::Gate
                && state_.playbackMode != audio::SamplePlaybackMode::OneShot
                && state_.playbackMode != audio::SamplePlaybackMode::Loop) {
                state_.playbackMode = audio::SamplePlaybackMode::Gate;
            }
            state_.loopEnabled = state_.playbackMode == audio::SamplePlaybackMode::Loop;
            sourceNode_.setPlaybackMode(state_.playbackMode);
            break;
        case RealtimeCommandType::PiecesReverse:
            state_.reverse = command.value >= 0.5f;
            sourceNode_.setReverse(state_.reverse);
            break;
        case RealtimeCommandType::PiecesEnvelopeAttackMs:
            state_.envelope.attackMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.setEnvelopeAttackSeconds(state_.envelope.attackMs / 1000.0f);
            break;
        case RealtimeCommandType::PiecesEnvelopeDecayMs:
            state_.envelope.decayMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.setEnvelopeDecaySeconds(state_.envelope.decayMs / 1000.0f);
            break;
        case RealtimeCommandType::PiecesEnvelopeSustain:
            state_.envelope.sustain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.setEnvelopeSustainLevel(state_.envelope.sustain);
            break;
        case RealtimeCommandType::PiecesEnvelopeReleaseMs:
            state_.envelope.releaseMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.setEnvelopeReleaseSeconds(state_.envelope.releaseMs / 1000.0f);
            break;
        case RealtimeCommandType::PiecesOutputEnabled:
            if (command.index >= state_.outputs.size()) {
                return;
            }
            state_.outputs[command.index] = command.value >= 0.5f;
            sourceNode_.setOutputEnabled(command.index, state_.outputs[command.index]);
            break;
        default:
            break;
    }
}

void PiecesSynth::appendStateJson(std::ostringstream& json) const {
    json << "{"
         << "\"implemented\":" << (state_.implemented ? "true" : "false") << ","
         << "\"playable\":" << (state_.playable ? "true" : "false") << ","
         << "\"mode\":\"sampler\","
         << "\"routingMode\":\"sample-playback\","
         << "\"voiceCount\":" << state_.voiceCount << ","
         << "\"midiEnabled\":" << (state_.midiEnabled ? "true" : "false") << ","
         << "\"gain\":" << state_.gain << ","
         << "\"rootNote\":" << state_.rootNote << ","
         << "\"transposeSemitones\":" << state_.transposeSemitones << ","
         << "\"fineTuneCents\":" << state_.fineTuneCents << ","
         << "\"start\":" << state_.start << ","
         << "\"end\":" << state_.end << ","
         << "\"playbackMode\":\"" << playbackModeId(state_.playbackMode) << "\","
         << "\"loopEnabled\":" << (state_.loopEnabled ? "true" : "false") << ","
         << "\"reverse\":" << (state_.reverse ? "true" : "false") << ","
         << "\"sample\":{"
         << "\"loaded\":" << (state_.sampleLoaded ? "true" : "false") << ","
         << "\"path\":\"" << escapeJson(state_.samplePath) << "\","
         << "\"name\":\"" << escapeJson(state_.sampleName) << "\","
         << "\"sampleRate\":" << state_.sampleRate << ","
         << "\"frames\":" << state_.sampleFrames << ","
         << "\"peaks\":[";

    for (std::size_t peakIndex = 0; peakIndex < state_.samplePeaks.size(); ++peakIndex) {
        if (peakIndex > 0) {
            json << ",";
        }
        json << state_.samplePeaks[peakIndex];
    }

    json << "]"
         << "},"
         << "\"envelope\":{"
         << "\"attackMs\":" << state_.envelope.attackMs << ","
         << "\"decayMs\":" << state_.envelope.decayMs << ","
         << "\"sustain\":" << state_.envelope.sustain << ","
         << "\"releaseMs\":" << state_.envelope.releaseMs
         << "},"
         << "\"outputs\":[";

    for (std::size_t outputIndex = 0; outputIndex < state_.outputs.size(); ++outputIndex) {
        if (outputIndex > 0) {
            json << ",";
        }
        json << (state_.outputs[outputIndex] ? "true" : "false");
    }

    json << "]}";
}

bool PiecesSynth::implemented() const {
    return state_.implemented;
}

bool PiecesSynth::playable() const {
    return state_.playable;
}

void PiecesSynth::resizeOutputs(std::uint32_t outputCount) {
    state_.outputs.resize(std::max<std::uint32_t>(1, outputCount), false);
    if (std::none_of(state_.outputs.begin(), state_.outputs.end(), [](bool enabled) { return enabled; })) {
        assignDefaultOutputs(state_.outputs);
    }
}

void PiecesSynth::setVoiceCount(std::uint32_t voiceCount) {
    state_.voiceCount = std::clamp<std::uint32_t>(voiceCount, 1, 64);
    sourceNode_.setVoiceCount(state_.voiceCount);
}

std::uint32_t PiecesSynth::outputCount() const {
    return static_cast<std::uint32_t>(state_.outputs.size());
}

const PiecesSourceState& PiecesSynth::state() const {
    return state_;
}

void PiecesSynth::applyState(const PiecesSourceState& state) {
    state_ = state;
    resizeOutputs(static_cast<std::uint32_t>(state_.outputs.size()));
    state_.loopEnabled = state_.playbackMode == audio::SamplePlaybackMode::Loop;
    if (state_.sampleBuffer != nullptr && !state_.sampleBuffer->empty()) {
        state_.samplePeaks = buildWaveformPeaks(*state_.sampleBuffer);
    } else {
        state_.samplePeaks.clear();
    }
    syncNodeState();
}

void PiecesSynth::assignDefaultOutputs(std::vector<bool>& outputs) {
    std::fill(outputs.begin(), outputs.end(), false);
    if (!outputs.empty()) {
        outputs[0] = true;
    }
    if (outputs.size() > 1) {
        outputs[1] = true;
    }
}

std::string PiecesSynth::escapeJson(std::string_view value) {
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

std::string_view PiecesSynth::playbackModeId(audio::SamplePlaybackMode mode) {
    switch (mode) {
        case audio::SamplePlaybackMode::OneShot:
            return "oneShot";
        case audio::SamplePlaybackMode::Loop:
            return "loop";
        case audio::SamplePlaybackMode::Gate:
        default:
            return "gate";
    }
}

bool PiecesSynth::parsePlaybackMode(std::string_view value, audio::SamplePlaybackMode& mode) {
    if (value == "gate") {
        mode = audio::SamplePlaybackMode::Gate;
        return true;
    }
    if (value == "oneShot") {
        mode = audio::SamplePlaybackMode::OneShot;
        return true;
    }
    if (value == "loop") {
        mode = audio::SamplePlaybackMode::Loop;
        return true;
    }
    return false;
}

std::vector<float> PiecesSynth::buildWaveformPeaks(const audio::SampleBuffer& sampleBuffer) {
    constexpr std::size_t kPeakCount = 96;
    std::vector<float> peaks;
    if (sampleBuffer.samples.empty()) {
        return peaks;
    }

    peaks.resize(kPeakCount, 0.0f);
    for (std::size_t peakIndex = 0; peakIndex < kPeakCount; ++peakIndex) {
        const std::size_t begin = (peakIndex * sampleBuffer.samples.size()) / kPeakCount;
        const std::size_t end = std::max(begin + 1, ((peakIndex + 1) * sampleBuffer.samples.size()) / kPeakCount);
        float peak = 0.0f;
        for (std::size_t sampleIndex = begin; sampleIndex < std::min(end, sampleBuffer.samples.size()); ++sampleIndex) {
            peak = std::max(peak, std::abs(sampleBuffer.samples[sampleIndex]));
        }
        peaks[peakIndex] = std::clamp(peak, 0.0f, 1.0f);
    }
    return peaks;
}

void PiecesSynth::syncNodeState() {
    sourceNode_.setVoiceCount(state_.voiceCount);
    sourceNode_.setSampleBuffer(state_.sampleBuffer);
    sourceNode_.setMidiEnabled(state_.midiEnabled);
    sourceNode_.setGain(state_.gain);
    sourceNode_.setRootNote(state_.rootNote);
    sourceNode_.setTransposeSemitones(state_.transposeSemitones);
    sourceNode_.setFineTuneCents(state_.fineTuneCents);
    sourceNode_.setStartPosition(state_.start);
    sourceNode_.setEndPosition(state_.end);
    sourceNode_.setPlaybackMode(state_.playbackMode);
    sourceNode_.setReverse(state_.reverse);
    sourceNode_.setEnvelopeAttackSeconds(state_.envelope.attackMs / 1000.0f);
    sourceNode_.setEnvelopeDecaySeconds(state_.envelope.decayMs / 1000.0f);
    sourceNode_.setEnvelopeSustainLevel(state_.envelope.sustain);
    sourceNode_.setEnvelopeReleaseSeconds(state_.envelope.releaseMs / 1000.0f);
    for (std::uint32_t outputIndex = 0; outputIndex < state_.outputs.size(); ++outputIndex) {
        sourceNode_.setOutputEnabled(outputIndex, state_.outputs[outputIndex]);
    }
}

}  // namespace synth::app
