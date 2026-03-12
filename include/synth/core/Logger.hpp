#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace synth::core {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    explicit Logger(std::filesystem::path logDirectory);

    bool initialize();
    void log(LogLevel level, std::string_view message);

    void debug(std::string_view message);
    void info(std::string_view message);
    void warn(std::string_view message);
    void error(std::string_view message);

private:
    static std::string timestamp();
    static const char* levelToString(LogLevel level);

    std::filesystem::path logDirectory_;
    std::ofstream file_;
    std::mutex mutex_;
};

}  // namespace synth::core
