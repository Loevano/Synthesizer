#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace synth::core {
class Logger;
}

namespace synth::osc {

struct OscMessage {
    std::string address;
    std::vector<std::int32_t> ints;
    std::vector<float> floats;
};

class OscServer {
public:
    using MessageHandler = std::function<void(const OscMessage&)>;

    explicit OscServer(core::Logger& logger);
    ~OscServer();

    bool start(std::uint16_t port, MessageHandler handler);
    void stop();

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace synth::osc
