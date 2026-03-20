#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "synth/app/RealtimeCommands.hpp"

namespace synth::app {

// Shared app-layer synth/source interface. Concrete synths own their own state
// and realtime command logic; the host only depends on this lifecycle surface.
class Synth {
public:
    virtual ~Synth() = default;

    virtual void prepare(double sampleRate, std::uint32_t outputChannels) = 0;
    virtual void process(float* output,
                         std::uint32_t frames,
                         std::uint32_t channels,
                         bool enabled,
                         float level) = 0;
    virtual void noteOn(int noteNumber, float velocity) = 0;
    virtual void noteOff(int noteNumber) = 0;

    virtual RealtimeParamResult applyNumericParam(const std::vector<std::string_view>& parts,
                                                  double value,
                                                  std::string* errorMessage) = 0;
    virtual RealtimeParamResult applyStringParam(const std::vector<std::string_view>& parts,
                                                 std::string_view value,
                                                 std::string* errorMessage) = 0;
    virtual RealtimeParamResult tryBuildRealtimeNumericCommand(const std::vector<std::string_view>& parts,
                                                               double value,
                                                               RealtimeCommand& command,
                                                               std::string* errorMessage) const = 0;
    virtual RealtimeParamResult tryBuildRealtimeStringCommand(const std::vector<std::string_view>& parts,
                                                              std::string_view value,
                                                              RealtimeCommand& command,
                                                              std::string* errorMessage) const = 0;
    virtual bool handlesRealtimeCommand(RealtimeCommandType type) const = 0;
    virtual void applyRealtimeCommand(const RealtimeCommand& command) = 0;

    virtual void appendStateJson(std::ostringstream& json) const = 0;
    virtual bool implemented() const = 0;
    virtual bool playable() const = 0;
};

}  // namespace synth::app
