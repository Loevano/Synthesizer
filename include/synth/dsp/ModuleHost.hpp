#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "synth/dsp/IDspModule.hpp"

namespace synth::core {
class Logger;
}

namespace synth::dsp {

class ModuleHost {
public:
    explicit ModuleHost(core::Logger& logger);
    ~ModuleHost();

    bool load(const std::filesystem::path& modulePath);
    bool reloadIfChanged();
    void unload();

    void prepare(double sampleRate);
    float processSample(float input);
    void setParameter(const std::string& parameter, float value);

    std::string activeModuleName() const;

private:
    bool loadUnlocked(const std::filesystem::path& modulePath);

    core::Logger& logger_;

    mutable std::mutex mutex_;
    void* libraryHandle_ = nullptr;
    IDspModule* module_ = nullptr;
    DestroyModuleFn destroyFn_ = nullptr;

    std::filesystem::path modulePath_;
    std::filesystem::file_time_type moduleWriteTime_{};
    std::string moduleName_ = "none";
    double sampleRate_ = 44100.0;
};

}  // namespace synth::dsp
