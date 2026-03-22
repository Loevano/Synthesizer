#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "synth/app/SourceState.hpp"
#include "synth/app/Synth.hpp"
#include "synth/audio/PiecesEngine.hpp"

namespace synth::app {

class Pieces final : public Synth {
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

    const PiecesSourceState& state() const;
    void applyState(const PiecesSourceState& state);

private:
    static const char* outputAlgorithmToString(PiecesOutputAlgorithm outputAlgorithm);
    static bool tryParseOutputAlgorithm(std::string_view value, PiecesOutputAlgorithm& outputAlgorithm);

    void syncEngineState();

    audio::PiecesEngine engine_;
    PiecesSourceState state_;
};

}  // namespace synth::app
