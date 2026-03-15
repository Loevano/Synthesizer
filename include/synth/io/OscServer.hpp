#pragma once

#include <cstdint>
#include <atomic>
#include <functional>
#include <string_view>
#include <thread>

namespace synth::core {
class Logger;
}

namespace synth::io {

struct OscCallbacks {
    std::function<void(std::string_view path, double value)> onNumericParam;
    std::function<void(std::string_view path, std::string_view value)> onStringParam;
    std::function<void(int noteNumber, float velocity)> onNoteOn;
    std::function<void(int noteNumber)> onNoteOff;
};

class OscServer {
public:
    explicit OscServer(core::Logger& logger);
    ~OscServer();

    bool start(std::uint16_t port, OscCallbacks callbacks);
    void stop();
    bool isRunning() const;
    std::uint16_t port() const;

private:
    core::Logger& logger_;
    OscCallbacks callbacks_;
    int socketFd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace synth::io
