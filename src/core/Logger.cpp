#include "synth/core/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace synth::core {

Logger::Logger(std::filesystem::path logDirectory)
    : logDirectory_(std::move(logDirectory)) {}

bool Logger::initialize() {
    std::error_code ec;
    std::filesystem::create_directories(logDirectory_, ec);
    if (ec) {
        std::cerr << "[ERROR] Failed to create log directory: " << ec.message() << '\n';
        return false;
    }

    const auto filename = logDirectory_ / ("synth_" + timestamp() + ".log");
    file_.open(filename, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "[ERROR] Failed to open log file: " << filename << '\n';
        return false;
    }

    info("Logger initialized. Writing to " + filename.string());
    return true;
}

void Logger::log(LogLevel level, std::string_view message) {
    const std::string line = "[" + timestamp() + "] [" + levelToString(level) + "] " + std::string(message);

    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << line << '\n';
    if (file_.is_open()) {
        file_ << line << '\n';
        file_.flush();
    }
}

void Logger::debug(std::string_view message) { log(LogLevel::Debug, message); }
void Logger::info(std::string_view message) { log(LogLevel::Info, message); }
void Logger::warn(std::string_view message) { log(LogLevel::Warn, message); }
void Logger::error(std::string_view message) { log(LogLevel::Error, message); }

std::string Logger::timestamp() {
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

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

}  // namespace synth::core
