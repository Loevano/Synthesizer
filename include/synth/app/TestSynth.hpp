#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "synth/app/Instrument.hpp"
#include "synth/app/InstrumentState.hpp"
#include "synth/graph/TestSourceNode.hpp"

namespace synth::app {

class TestSynth final : public Instrument {
public:
    void prepare(double sampleRate, std::uint32_t outputChannels) override;
    void renderAdd(float* output,
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
    std::uint32_t outputCount() const;

private:
    static void assignDefaultOutputs(std::vector<bool>& outputs);
    static const char* waveformToString(dsp::Waveform waveform);
    static bool tryParseWaveform(std::string_view value, dsp::Waveform& waveform);
    static float midiNoteToFrequency(int noteNumber);

    void syncNodeState();

    graph::TestSourceNode sourceNode_;
    TestSourceState state_;
};

}  // namespace synth::app
