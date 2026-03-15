#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace synth::core {
class Logger;
}

namespace synth::io {

using MidiNoteCallback = std::function<void(std::uint32_t sourceIndex, int noteNumber, float velocity)>;

struct MidiSourceInfo {
    std::uint32_t index = 0;
    std::string name;
    bool connected = false;
};

class MidiInput {
public:
    explicit MidiInput(core::Logger& logger);
    ~MidiInput();

    bool start(MidiNoteCallback callback, bool connectAllSources = true);
    void stop();
    bool isRunning() const;
    std::size_t sourceCount() const;
    std::size_t connectedSourceCount() const;
    std::vector<MidiSourceInfo> sources() const;
    bool setSourceConnected(std::uint32_t sourceIndex, bool connected);
    void handlePacketData(std::uint32_t sourceIndex, const unsigned char* data, std::size_t length);

private:
    struct SourceState {
        MidiSourceInfo info;
#if defined(SYNTH_PLATFORM_MACOS)
        std::uint32_t endpoint = 0;
#endif
    };

    core::Logger& logger_;
    MidiNoteCallback callback_;
    bool running_ = false;
    std::vector<SourceState> sources_;

    SourceState* findSourceState(std::uint32_t sourceIndex);
    const SourceState* findSourceState(std::uint32_t sourceIndex) const;

#if defined(SYNTH_PLATFORM_MACOS)
    bool populateSources(bool connectAllSources);
    std::uint32_t client_ = 0;
    std::uint32_t inputPort_ = 0;
#endif
};

}  // namespace synth::io
