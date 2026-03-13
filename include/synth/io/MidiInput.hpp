#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace synth::core {
class Logger;
}

namespace synth::io {

using MidiNoteCallback = std::function<void(int noteNumber, float velocity)>;

class MidiInput {
public:
    explicit MidiInput(core::Logger& logger);
    ~MidiInput();

    bool start(MidiNoteCallback callback);
    void stop();
    bool isRunning() const;
    std::size_t sourceCount() const;
    void handlePacketData(const unsigned char* data, std::size_t length);

private:
    core::Logger& logger_;
    MidiNoteCallback callback_;
    bool running_ = false;
    std::size_t sourceCount_ = 0;

#if defined(SYNTH_PLATFORM_MACOS)
    std::uint32_t client_ = 0;
    std::uint32_t inputPort_ = 0;
#endif
};

}  // namespace synth::io
