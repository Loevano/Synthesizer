#include "synth/core/CrashDiagnostics.hpp"

#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <iomanip>
#include <sstream>

#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

namespace synth::core {

CrashDiagnostics* CrashDiagnostics::activeInstance_ = nullptr;
bool CrashDiagnostics::handlersInstalled_ = false;

namespace {

void appendInteger(char* buffer, std::size_t bufferSize, int value) noexcept {
    if (bufferSize == 0) {
        return;
    }

    std::size_t cursor = 0;
    int remaining = value;
    if (remaining == 0) {
        buffer[cursor++] = '0';
    } else {
        if (remaining < 0 && cursor < bufferSize) {
            buffer[cursor++] = '-';
            remaining = -remaining;
        }

        char digits[16];
        std::size_t digitCount = 0;
        while (remaining > 0 && digitCount < sizeof(digits)) {
            digits[digitCount++] = static_cast<char>('0' + (remaining % 10));
            remaining /= 10;
        }

        while (digitCount > 0 && cursor < bufferSize) {
            buffer[cursor++] = digits[--digitCount];
        }
    }

    if (cursor < bufferSize) {
        buffer[cursor] = '\0';
    } else {
        buffer[bufferSize - 1] = '\0';
    }
}

}  // namespace

CrashDiagnostics::CrashDiagnostics(std::filesystem::path logDirectory)
    : logDirectory_(std::move(logDirectory)) {}

CrashDiagnostics::~CrashDiagnostics() {
    if (crashFd_ >= 0) {
        ::close(crashFd_);
        crashFd_ = -1;
    }
}

bool CrashDiagnostics::initialize() {
    std::error_code ec;
    std::filesystem::create_directories(logDirectory_, ec);
    if (ec) {
        return false;
    }

    crashLogPath_ = logDirectory_ / ("crash_" + timestamp() + ".log");
    crashFd_ = ::open(crashLogPath_.string().c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (crashFd_ < 0) {
        return false;
    }

    breadcrumb("Crash diagnostics initialized.");
    return true;
}

void CrashDiagnostics::install() {
    activeInstance_ = this;
    if (handlersInstalled_) {
        return;
    }

    std::set_terminate(&CrashDiagnostics::handleTerminate);
    std::signal(SIGABRT, &CrashDiagnostics::handleSignal);
    std::signal(SIGBUS, &CrashDiagnostics::handleSignal);
    std::signal(SIGFPE, &CrashDiagnostics::handleSignal);
    std::signal(SIGILL, &CrashDiagnostics::handleSignal);
    std::signal(SIGSEGV, &CrashDiagnostics::handleSignal);
    handlersInstalled_ = true;
}

void CrashDiagnostics::breadcrumb(std::string_view message) {
    if (crashFd_ < 0) {
        return;
    }

    const std::string line = "[" + timestamp() + "] " + std::string(message) + "\n";
    std::lock_guard<std::mutex> lock(mutex_);
    writeRaw(line.c_str(), line.size());
}

const std::filesystem::path& CrashDiagnostics::crashLogPath() const {
    return crashLogPath_;
}

void CrashDiagnostics::noteObjectiveCException(std::string_view name, std::string_view reason) {
    if (activeInstance_ == nullptr) {
        return;
    }

    activeInstance_->breadcrumb(
        "Uncaught Objective-C exception: " + std::string(name) + " reason=" + std::string(reason));
    writeStackTrace();
    flush();
}

void CrashDiagnostics::handleSignal(int signalNumber) {
    writeSignalMessage(signalNumber);
    writeStackTrace();
    flush();

    std::signal(signalNumber, SIG_DFL);
    std::raise(signalNumber);
}

void CrashDiagnostics::handleTerminate() {
    writeTerminateMessage();
    writeStackTrace();
    flush();
    std::abort();
}

void CrashDiagnostics::writeSignalMessage(int signalNumber) noexcept {
    writeRaw("\n=== Fatal signal ", 18);
    char signalBuffer[16];
    appendInteger(signalBuffer, sizeof(signalBuffer), signalNumber);
    writeRaw(signalBuffer, std::strlen(signalBuffer));
    writeRaw(" ===\n", 5);
}

void CrashDiagnostics::writeTerminateMessage() noexcept {
    writeRaw("\n=== std::terminate ===\n", 23);
    try {
        const auto exception = std::current_exception();
        if (exception == nullptr) {
            writeRaw("No active exception.\n", 21);
            return;
        }

        std::rethrow_exception(exception);
    } catch (const std::exception& error) {
        writeRaw("Exception: ", 11);
        writeRaw(error.what(), std::strlen(error.what()));
        writeRaw("\n", 1);
    } catch (...) {
        writeRaw("Exception: unknown\n", 19);
    }
}

void CrashDiagnostics::writeStackTrace() noexcept {
    if (activeInstance_ == nullptr || activeInstance_->crashFd_ < 0) {
        return;
    }

    writeRaw("Stack trace:\n", 13);
    std::array<void*, 64> frames{};
    const int frameCount = ::backtrace(frames.data(), static_cast<int>(frames.size()));
    if (frameCount > 0) {
        ::backtrace_symbols_fd(frames.data(), frameCount, activeInstance_->crashFd_);
    }
    writeRaw("\n", 1);
}

void CrashDiagnostics::writeRaw(const char* data, std::size_t size) noexcept {
    if (activeInstance_ == nullptr || activeInstance_->crashFd_ < 0 || data == nullptr || size == 0) {
        return;
    }

    while (size > 0) {
        const ssize_t written = ::write(activeInstance_->crashFd_, data, size);
        if (written <= 0) {
            return;
        }
        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

void CrashDiagnostics::flush() noexcept {
    if (activeInstance_ == nullptr || activeInstance_->crashFd_ < 0) {
        return;
    }

    ::fsync(activeInstance_->crashFd_);
}

std::string CrashDiagnostics::timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

}  // namespace synth::core
