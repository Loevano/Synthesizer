#include "synth/app/Test.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace synth::app {

void Test::prepare(double sampleRate, std::uint32_t outputChannels) {
    sourceNode_.prepare(sampleRate, outputChannels);
    resizeOutputs(outputChannels);
    syncNodeState();
}

void Test::renderAdd(float* output,
                     std::uint32_t frames,
                     std::uint32_t channels,
                     bool enabled,
                     float level) {
    sourceNode_.renderAdd(output, frames, channels, enabled, level);
}

void Test::noteOn(int noteNumber, float velocity) {
    if (!state_.midiEnabled) {
        return;
    }

    state_.frequency = midiNoteToFrequency(noteNumber);
    sourceNode_.noteOn(noteNumber, velocity);
}

void Test::noteOff(int noteNumber) {
    if (!state_.midiEnabled) {
        return;
    }

    sourceNode_.noteOff(noteNumber);
}

RealtimeParamResult Test::applyNumericParam(const std::vector<std::string_view>& parts,
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

RealtimeParamResult Test::applyStringParam(const std::vector<std::string_view>& parts,
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

RealtimeParamResult Test::tryBuildRealtimeNumericCommand(const std::vector<std::string_view>& parts,
                                                         double value,
                                                         RealtimeCommand& command,
                                                         std::string* errorMessage) const {
    if (parts.size() < 3 || parts[0] != "sources" || parts[1] != "test") {
        return RealtimeParamResult::NotHandled;
    }

    if (parts.size() == 3 && parts[2] == "active") {
        command.type = RealtimeCommandType::TestActive;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "midiEnabled") {
        command.type = RealtimeCommandType::TestMidiEnabled;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "frequency") {
        command.type = RealtimeCommandType::TestFrequency;
        command.value = static_cast<float>(std::clamp(value, 20.0, 20000.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 3 && parts[2] == "gain") {
        command.type = RealtimeCommandType::TestGain;
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 4 && parts[2] == "output") {
        std::uint32_t outputIndex = 0;
        const char* begin = parts[3].data();
        const char* end = parts[3].data() + parts[3].size();
        auto parseResult = std::from_chars(begin, end, outputIndex);
        if (parseResult.ec != std::errc{} || parseResult.ptr != end || outputIndex >= state_.outputs.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid test output index.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::TestOutputEnabled;
        command.index = outputIndex;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
        return RealtimeParamResult::Applied;
    }
    if (parts.size() == 4 && parts[2] == "envelope") {
        if (parts[3] == "attackMs") {
            command.type = RealtimeCommandType::TestEnvelopeAttackMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "decayMs") {
            command.type = RealtimeCommandType::TestEnvelopeDecayMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "sustain") {
            command.type = RealtimeCommandType::TestEnvelopeSustain;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
            return RealtimeParamResult::Applied;
        }
        if (parts[3] == "releaseMs") {
            command.type = RealtimeCommandType::TestEnvelopeReleaseMs;
            command.value = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            return RealtimeParamResult::Applied;
        }
    }

    return RealtimeParamResult::NotHandled;
}

RealtimeParamResult Test::tryBuildRealtimeStringCommand(const std::vector<std::string_view>& parts,
                                                        std::string_view value,
                                                        RealtimeCommand& command,
                                                        std::string* errorMessage) const {
    if (parts.size() == 3 && parts[0] == "sources" && parts[1] == "test" && parts[2] == "waveform") {
        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::TestWaveform;
        command.code = static_cast<std::uint32_t>(waveform);
        return RealtimeParamResult::Applied;
    }

    return RealtimeParamResult::NotHandled;
}

bool Test::handlesRealtimeCommand(RealtimeCommandType type) const {
    switch (type) {
        case RealtimeCommandType::TestActive:
        case RealtimeCommandType::TestMidiEnabled:
        case RealtimeCommandType::TestFrequency:
        case RealtimeCommandType::TestGain:
        case RealtimeCommandType::TestEnvelopeAttackMs:
        case RealtimeCommandType::TestEnvelopeDecayMs:
        case RealtimeCommandType::TestEnvelopeSustain:
        case RealtimeCommandType::TestEnvelopeReleaseMs:
        case RealtimeCommandType::TestOutputEnabled:
        case RealtimeCommandType::TestWaveform:
            return true;
        default:
            return false;
    }
}

void Test::applyRealtimeCommand(const RealtimeCommand& command) {
    switch (command.type) {
        case RealtimeCommandType::TestActive:
            state_.active = command.value >= 0.5f;
            sourceNode_.setActive(state_.active);
            break;
        case RealtimeCommandType::TestMidiEnabled:
            state_.midiEnabled = command.value >= 0.5f;
            sourceNode_.setMidiEnabled(state_.midiEnabled);
            break;
        case RealtimeCommandType::TestFrequency:
            state_.frequency = std::clamp(command.value, 20.0f, 20000.0f);
            sourceNode_.setFrequency(state_.frequency);
            break;
        case RealtimeCommandType::TestGain:
            state_.gain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.setGain(state_.gain);
            break;
        case RealtimeCommandType::TestEnvelopeAttackMs:
            state_.envelope.attackMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.setEnvelopeAttackSeconds(state_.envelope.attackMs / 1000.0f);
            break;
        case RealtimeCommandType::TestEnvelopeDecayMs:
            state_.envelope.decayMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.setEnvelopeDecaySeconds(state_.envelope.decayMs / 1000.0f);
            break;
        case RealtimeCommandType::TestEnvelopeSustain:
            state_.envelope.sustain = std::clamp(command.value, 0.0f, 1.0f);
            sourceNode_.setEnvelopeSustainLevel(state_.envelope.sustain);
            break;
        case RealtimeCommandType::TestEnvelopeReleaseMs:
            state_.envelope.releaseMs = std::clamp(command.value, 0.0f, 5000.0f);
            sourceNode_.setEnvelopeReleaseSeconds(state_.envelope.releaseMs / 1000.0f);
            break;
        case RealtimeCommandType::TestOutputEnabled:
            if (command.index >= state_.outputs.size()) {
                return;
            }
            state_.outputs[command.index] = command.value >= 0.5f;
            sourceNode_.setOutputEnabled(command.index, state_.outputs[command.index]);
            break;
        case RealtimeCommandType::TestWaveform:
            state_.waveform = static_cast<dsp::Waveform>(command.code);
            sourceNode_.setWaveform(state_.waveform);
            break;
        default:
            break;
    }
}

void Test::appendStateJson(std::ostringstream& json) const {
    json << "{"
         << "\"implemented\":" << (state_.implemented ? "true" : "false") << ","
         << "\"playable\":" << (state_.playable ? "true" : "false") << ","
         << "\"active\":" << (state_.active ? "true" : "false") << ","
         << "\"midiEnabled\":" << (state_.midiEnabled ? "true" : "false") << ","
         << "\"frequency\":" << state_.frequency << ","
         << "\"gain\":" << state_.gain << ","
         << "\"waveform\":\"" << waveformToString(state_.waveform) << "\","
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

bool Test::implemented() const {
    return state_.implemented;
}

bool Test::playable() const {
    return state_.playable;
}

void Test::resizeOutputs(std::uint32_t outputCount) {
    state_.outputs.resize(std::max<std::uint32_t>(1, outputCount), false);
    if (std::none_of(state_.outputs.begin(), state_.outputs.end(), [](bool enabled) { return enabled; })) {
        assignDefaultOutputs(state_.outputs);
    }
}

std::uint32_t Test::outputCount() const {
    return static_cast<std::uint32_t>(state_.outputs.size());
}

void Test::assignDefaultOutputs(std::vector<bool>& outputs) {
    std::fill(outputs.begin(), outputs.end(), false);
    if (!outputs.empty()) {
        outputs[0] = true;
    }
    if (outputs.size() > 1) {
        outputs[1] = true;
    }
}

const char* Test::waveformToString(dsp::Waveform waveform) {
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

bool Test::tryParseWaveform(std::string_view value, dsp::Waveform& waveform) {
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

float Test::midiNoteToFrequency(int noteNumber) {
    return 440.0f * std::pow(2.0f, static_cast<float>(noteNumber - 69) / 12.0f);
}

void Test::syncNodeState() {
    sourceNode_.setActive(state_.active);
    sourceNode_.setMidiEnabled(state_.midiEnabled);
    sourceNode_.setFrequency(state_.frequency);
    sourceNode_.setGain(state_.gain);
    sourceNode_.setWaveform(state_.waveform);
    sourceNode_.setEnvelopeAttackSeconds(state_.envelope.attackMs / 1000.0f);
    sourceNode_.setEnvelopeDecaySeconds(state_.envelope.decayMs / 1000.0f);
    sourceNode_.setEnvelopeSustainLevel(state_.envelope.sustain);
    sourceNode_.setEnvelopeReleaseSeconds(state_.envelope.releaseMs / 1000.0f);
    for (std::uint32_t outputIndex = 0; outputIndex < state_.outputs.size(); ++outputIndex) {
        sourceNode_.setOutputEnabled(outputIndex, state_.outputs[outputIndex]);
    }
}

}  // namespace synth::app
