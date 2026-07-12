#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "synth/app/Synth.hpp"
#include "synth/app/SourceState.hpp"
#include "synth/audio/SampleBuffer.hpp"
#include "synth/graph/PiecesSourceNode.hpp"

namespace synth::app {

class PiecesSynth final : public Synth {
public:
    void prepare(double sampleRate, std::uint32_t outputChannels) override;
    void process(float* output,
                 std::uint32_t frames,
                 std::uint32_t channels,
                 bool enabled,
                 float level) override;
    void clearAllNotes();
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

    void resizeOutputs(std::uint32_t outputCount);
    void setVoiceCount(std::uint32_t voiceCount);
    std::uint32_t outputCount() const;
    const PiecesSourceState& state() const;
    void applyState(const PiecesSourceState& state);

private:
    static void assignDefaultOutputs(std::vector<bool>& outputs);
    static std::string escapeJson(std::string_view value);

    void syncNodeState();

    graph::PiecesSourceNode sourceNode_;
    PiecesSourceState state_;
};

}  // namespace synth::app
