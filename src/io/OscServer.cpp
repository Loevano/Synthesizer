#include "synth/io/OscServer.hpp"

#include "synth/core/Logger.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace synth::io {

namespace {

bool readOscString(const std::vector<char>& buffer, std::size_t& offset, std::string& value) {
    if (offset >= buffer.size()) {
        return false;
    }

    const std::size_t start = offset;
    while (offset < buffer.size() && buffer[offset] != '\0') {
        ++offset;
    }
    if (offset >= buffer.size()) {
        return false;
    }

    value.assign(buffer.data() + start, offset - start);
    ++offset;
    while (offset % 4 != 0 && offset < buffer.size()) {
        ++offset;
    }
    return true;
}

bool readInt32(const std::vector<char>& buffer, std::size_t& offset, std::int32_t& value) {
    if (offset + 4 > buffer.size()) {
        return false;
    }

    std::uint32_t networkValue = 0;
    std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
    value = static_cast<std::int32_t>(ntohl(networkValue));
    offset += 4;
    return true;
}

bool readFloat32(const std::vector<char>& buffer, std::size_t& offset, float& value) {
    std::int32_t raw = 0;
    if (!readInt32(buffer, offset, raw)) {
        return false;
    }

    static_assert(sizeof(float) == sizeof(std::int32_t));
    std::memcpy(&value, &raw, sizeof(value));
    return true;
}

std::string oscAddressToParamPath(std::string_view address) {
    if (!address.starts_with("/param/")) {
        return {};
    }

    std::string path;
    path.reserve(address.size());
    for (std::size_t index = 7; index < address.size(); ++index) {
        path.push_back(address[index] == '/' ? '.' : address[index]);
    }
    return path;
}

void handleOscPacket(core::Logger& logger, const OscCallbacks& callbacks, const std::vector<char>& buffer) {
    std::size_t offset = 0;
    std::string address;
    std::string typeTags;

    if (!readOscString(buffer, offset, address) || !readOscString(buffer, offset, typeTags)) {
        return;
    }

    if (address == "/noteOn") {
        std::int32_t noteNumber = 0;
        float velocity = 1.0f;
        if (typeTags == ",i") {
            if (readInt32(buffer, offset, noteNumber) && callbacks.onNoteOn) {
                callbacks.onNoteOn(static_cast<int>(noteNumber), velocity);
            }
            return;
        }
        if (typeTags == ",if") {
            if (readInt32(buffer, offset, noteNumber) && readFloat32(buffer, offset, velocity) && callbacks.onNoteOn) {
                callbacks.onNoteOn(static_cast<int>(noteNumber), velocity);
            }
            return;
        }
    }

    if (address == "/noteOff") {
        std::int32_t noteNumber = 0;
        if (typeTags == ",i" && readInt32(buffer, offset, noteNumber) && callbacks.onNoteOff) {
            callbacks.onNoteOff(static_cast<int>(noteNumber));
        }
        return;
    }

    const std::string paramPath = oscAddressToParamPath(address);
    if (paramPath.empty()) {
        return;
    }

    if (typeTags == ",f") {
        float value = 0.0f;
        if (readFloat32(buffer, offset, value) && callbacks.onNumericParam) {
            callbacks.onNumericParam(paramPath, value);
        }
        return;
    }

    if (typeTags == ",i") {
        std::int32_t value = 0;
        if (readInt32(buffer, offset, value) && callbacks.onNumericParam) {
            callbacks.onNumericParam(paramPath, static_cast<double>(value));
        }
        return;
    }

    if (typeTags == ",s") {
        std::string value;
        if (readOscString(buffer, offset, value) && callbacks.onStringParam) {
            callbacks.onStringParam(paramPath, value);
        }
        return;
    }

    logger.info("OSC: ignored unsupported packet.");
}

}  // namespace

OscServer::OscServer(core::Logger& logger)
    : logger_(logger) {}

OscServer::~OscServer() {
    stop();
}

bool OscServer::start(std::uint16_t port, OscCallbacks callbacks) {
    if (running_) {
        return true;
    }

    callbacks_ = std::move(callbacks);
    port_ = port;

    socketFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd_ < 0) {
        logger_.error("OSC: failed to create UDP socket.");
        return false;
    }

    int reuseAddress = 1;
    setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    if (bind(socketFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        logger_.error("OSC: failed to bind UDP socket.");
        ::close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    running_ = true;
    thread_ = std::thread([this]() {
        std::array<char, 2048> rawBuffer{};
        while (running_) {
            const ssize_t receivedSize = recvfrom(socketFd_, rawBuffer.data(), rawBuffer.size(), 0, nullptr, nullptr);
            if (receivedSize <= 0) {
                if (!running_) {
                    break;
                }
                continue;
            }

            std::vector<char> packet(rawBuffer.begin(), rawBuffer.begin() + receivedSize);
            handleOscPacket(logger_, callbacks_, packet);
        }
    });

    logger_.info("OSC: listening on UDP port " + std::to_string(port_) + ".");
    return true;
}

void OscServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (socketFd_ >= 0) {
        shutdown(socketFd_, SHUT_RDWR);
        ::close(socketFd_);
        socketFd_ = -1;
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

bool OscServer::isRunning() const {
    return running_;
}

std::uint16_t OscServer::port() const {
    return port_;
}

}  // namespace synth::io
