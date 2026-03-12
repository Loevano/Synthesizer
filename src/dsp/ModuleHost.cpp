#include "synth/dsp/ModuleHost.hpp"

#include "synth/core/Logger.hpp"

#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace synth::dsp {

namespace {

void* openLibrary(const std::filesystem::path& path) {
#if defined(_WIN32)
    return static_cast<void*>(LoadLibraryA(path.string().c_str()));
#else
    return dlopen(path.string().c_str(), RTLD_NOW);
#endif
}

void closeLibrary(void* handle) {
    if (handle == nullptr) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* resolveSymbol(void* handle, const char* symbol) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol));
#else
    return dlsym(handle, symbol);
#endif
}

std::string libraryError() {
#if defined(_WIN32)
    return "failed to load symbol/library";
#else
    const char* error = dlerror();
    return error == nullptr ? "unknown error" : std::string(error);
#endif
}

}  // namespace

ModuleHost::ModuleHost(core::Logger& logger) : logger_(logger) {}

ModuleHost::~ModuleHost() {
    unload();
}

bool ModuleHost::load(const std::filesystem::path& modulePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadUnlocked(modulePath);
}

bool ModuleHost::reloadIfChanged() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (modulePath_.empty() || !std::filesystem::exists(modulePath_)) {
        return false;
    }

    const auto latestWriteTime = std::filesystem::last_write_time(modulePath_);
    if (latestWriteTime == moduleWriteTime_) {
        return false;
    }

    logger_.info("Module change detected, reloading: " + modulePath_.string());
    return loadUnlocked(modulePath_);
}

void ModuleHost::unload() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (module_ != nullptr && destroyFn_ != nullptr) {
        destroyFn_(module_);
    }

    module_ = nullptr;
    destroyFn_ = nullptr;
    moduleName_ = "none";

    closeLibrary(libraryHandle_);
    libraryHandle_ = nullptr;
}

void ModuleHost::prepare(double sampleRate) {
    std::lock_guard<std::mutex> lock(mutex_);
    sampleRate_ = sampleRate;
    if (module_ != nullptr) {
        module_->prepare(sampleRate_);
    }
}

float ModuleHost::processSample(float input) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (module_ == nullptr) {
        return input;
    }
    return module_->processSample(input);
}

void ModuleHost::setParameter(const std::string& parameter, float value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (module_ != nullptr) {
        module_->setParameter(parameter, value);
    }
}

std::string ModuleHost::activeModuleName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return moduleName_;
}

bool ModuleHost::loadUnlocked(const std::filesystem::path& modulePath) {
    if (!std::filesystem::exists(modulePath)) {
        logger_.error("Module file not found: " + modulePath.string());
        return false;
    }

    if (module_ != nullptr && destroyFn_ != nullptr) {
        destroyFn_(module_);
        module_ = nullptr;
    }
    destroyFn_ = nullptr;

    closeLibrary(libraryHandle_);
    libraryHandle_ = nullptr;

    libraryHandle_ = openLibrary(modulePath);
    if (libraryHandle_ == nullptr) {
        logger_.error("Failed to load module library: " + modulePath.string() + " (" + libraryError() + ")");
        return false;
    }

    auto* createFn = reinterpret_cast<CreateModuleFn>(resolveSymbol(libraryHandle_, kCreateModuleSymbol));
    auto* destroyFn = reinterpret_cast<DestroyModuleFn>(resolveSymbol(libraryHandle_, kDestroyModuleSymbol));
    auto* getNameFn = reinterpret_cast<GetModuleNameFn>(resolveSymbol(libraryHandle_, kGetModuleNameSymbol));

    if (createFn == nullptr || destroyFn == nullptr || getNameFn == nullptr) {
        logger_.error("Module is missing required symbols: " + modulePath.string());
        closeLibrary(libraryHandle_);
        libraryHandle_ = nullptr;
        return false;
    }

    module_ = createFn();
    if (module_ == nullptr) {
        logger_.error("Failed to create module instance: " + modulePath.string());
        closeLibrary(libraryHandle_);
        libraryHandle_ = nullptr;
        return false;
    }

    destroyFn_ = destroyFn;
    modulePath_ = modulePath;
    moduleWriteTime_ = std::filesystem::last_write_time(modulePath);
    moduleName_ = getNameFn();

    module_->prepare(sampleRate_);
    logger_.info("Loaded DSP module: " + moduleName_ + " from " + modulePath.string());
    return true;
}

}  // namespace synth::dsp
