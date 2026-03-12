#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace synth::core {
class Logger;
}

namespace synth::midi {

struct MidiMessage {
    std::array<std::uint8_t, 3> bytes{};
    std::size_t size = 0;
};

class MidiInput {
public:
    using MessageHandler = std::function<void(const MidiMessage&)>;

    explicit MidiInput(core::Logger& logger);
    ~MidiInput();

    bool start(MessageHandler handler);
    void stop();

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace synth::midi
