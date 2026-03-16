#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace synth::core {

class CrashDiagnostics {
public:
    explicit CrashDiagnostics(std::filesystem::path logDirectory);
    ~CrashDiagnostics();

    bool initialize();
    void install();
    void breadcrumb(std::string_view message);

    const std::filesystem::path& crashLogPath() const;
    static void noteObjectiveCException(std::string_view name, std::string_view reason);

private:
    static void handleSignal(int signalNumber);
    static void handleTerminate();
    static void writeSignalMessage(int signalNumber) noexcept;
    static void writeTerminateMessage() noexcept;
    static void writeStackTrace() noexcept;
    static void writeRaw(const char* data, std::size_t size) noexcept;
    static void flush() noexcept;
    static std::string timestamp();

    std::filesystem::path logDirectory_;
    std::filesystem::path crashLogPath_;
    int crashFd_ = -1;
    std::mutex mutex_;

    static CrashDiagnostics* activeInstance_;
    static bool handlersInstalled_;
};

}  // namespace synth::core
