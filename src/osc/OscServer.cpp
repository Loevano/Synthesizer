#include "synth/osc/OscServer.hpp"

#include "synth/core/Logger.hpp"

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace synth::osc {

namespace {

bool readPaddedString(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::string& out) {
    if (offset >= size) {
        return false;
    }

    const std::size_t start = offset;
    while (offset < size && data[offset] != '\0') {
        ++offset;
    }

    if (offset >= size) {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(data + start), offset - start);

    ++offset;
    while (offset % 4 != 0) {
        ++offset;
    }

    return offset <= size;
}

bool readInt32(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::int32_t& out) {
    if (offset + 4 > size) {
        return false;
    }

    std::uint32_t net = 0;
    std::memcpy(&net, data + offset, sizeof(net));
    offset += 4;

    out = static_cast<std::int32_t>(ntohl(net));
    return true;
}

bool readFloat32(const std::uint8_t* data, std::size_t size, std::size_t& offset, float& out) {
    if (offset + 4 > size) {
        return false;
    }

    std::uint32_t net = 0;
    std::memcpy(&net, data + offset, sizeof(net));
    offset += 4;

    std::uint32_t host = ntohl(net);
    out = std::bit_cast<float>(host);
    return true;
}

bool parseOscPacket(const std::uint8_t* data, std::size_t size, OscMessage& message) {
    std::size_t offset = 0;

    if (!readPaddedString(data, size, offset, message.address)) {
        return false;
    }

    std::string typeTags;
    if (!readPaddedString(data, size, offset, typeTags)) {
        return false;
    }

    if (typeTags.empty() || typeTags[0] != ',') {
        return false;
    }

    for (std::size_t i = 1; i < typeTags.size(); ++i) {
        const char tag = typeTags[i];
        if (tag == 'i') {
            std::int32_t value = 0;
            if (!readInt32(data, size, offset, value)) {
                return false;
            }
            message.ints.push_back(value);
        } else if (tag == 'f') {
            float value = 0.0f;
            if (!readFloat32(data, size, offset, value)) {
                return false;
            }
            message.floats.push_back(value);
        } else {
            return false;
        }
    }

    return true;
}

}  // namespace

struct OscServer::Impl {
    explicit Impl(core::Logger& loggerRef)
        : logger(loggerRef) {}

    bool start(std::uint16_t listenPort, MessageHandler messageHandler) {
        if (running.load()) {
            return true;
        }

        handler = std::move(messageHandler);

#if defined(_WIN32)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            logger.error("OSC: WSAStartup failed.");
            return false;
        }
#endif

        socketFd = static_cast<int>(::socket(AF_INET, SOCK_DGRAM, 0));
        if (socketFd < 0) {
            logger.error("OSC: failed to create UDP socket.");
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(listenPort);

        if (::bind(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            logger.error("OSC: failed to bind UDP socket.");
#if defined(_WIN32)
            closesocket(socketFd);
            WSACleanup();
#else
            close(socketFd);
#endif
            socketFd = -1;
            return false;
        }

        running.store(true);
        worker = std::thread([this, listenPort] { run(listenPort); });
        return true;
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }

#if defined(_WIN32)
        closesocket(socketFd);
#else
        close(socketFd);
#endif
        socketFd = -1;

        if (worker.joinable()) {
            worker.join();
        }

#if defined(_WIN32)
        WSACleanup();
#endif
        handler = nullptr;
    }

    void run(std::uint16_t listenPort) {
        logger.info("OSC: listening on UDP port " + std::to_string(listenPort));

        std::uint8_t buffer[2048] = {0};

        while (running.load()) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(socketFd, &readSet);

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 200000;

            const int ready = select(socketFd + 1, &readSet, nullptr, nullptr, &timeout);
            if (ready <= 0 || !FD_ISSET(socketFd, &readSet)) {
                continue;
            }

            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            const int bytes = recvfrom(
                socketFd,
                reinterpret_cast<char*>(buffer),
                sizeof(buffer),
                0,
                reinterpret_cast<sockaddr*>(&clientAddr),
                &clientLen);

            if (bytes <= 0) {
                continue;
            }

            OscMessage message;
            if (!parseOscPacket(buffer, static_cast<std::size_t>(bytes), message)) {
                logger.warn("OSC: failed to parse packet.");
                continue;
            }

            if (handler) {
                handler(message);
            }
        }
    }

    core::Logger& logger;
    std::atomic<bool> running{false};
    int socketFd = -1;
    std::thread worker;
    MessageHandler handler;
};

OscServer::OscServer(core::Logger& logger)
    : impl_(new Impl(logger)) {}

OscServer::~OscServer() {
    stop();
    delete impl_;
}

bool OscServer::start(std::uint16_t port, MessageHandler handler) {
    return impl_->start(port, std::move(handler));
}

void OscServer::stop() {
    impl_->stop();
}

}  // namespace synth::osc
