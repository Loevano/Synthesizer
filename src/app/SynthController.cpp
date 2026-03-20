#include "synth/app/SynthController.hpp"

#include "synth/io/MidiInput.hpp"
#include "synth/io/OscServer.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

#if defined(SYNTH_PLATFORM_MACOS)
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace synth::app {

namespace {

std::vector<std::string_view> splitPath(std::string_view path) {
    std::vector<std::string_view> parts;

    while (!path.empty()) {
        const std::size_t separator = path.find('.');
        if (separator == std::string_view::npos) {
            parts.push_back(path);
            break;
        }

        parts.push_back(path.substr(0, separator));
        path.remove_prefix(separator + 1);
    }

    return parts;
}

bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    return std::search(
               haystack.begin(),
               haystack.end(),
               needle.begin(),
               needle.end(),
               [](char left, char right) {
                   return std::tolower(static_cast<unsigned char>(left)) ==
                          std::tolower(static_cast<unsigned char>(right));
               }) != haystack.end();
}

bool isPreferredMidiSource(std::string_view sourceName) {
    return containsIgnoreCase(sourceName, "behringer swing");
}

bool envFlagEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return false;
    }

    const std::string_view flag{value};
    return flag != "0" && flag != "false" && flag != "FALSE" && flag != "off" && flag != "OFF";
}

std::filesystem::path defaultLogDirectory() {
#if defined(SYNTH_PLATFORM_MACOS)
    if (const char* overridePath = std::getenv("SYNTH_LOG_DIR"); overridePath != nullptr && *overridePath != '\0') {
        return std::filesystem::path(overridePath);
    }

    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / "Library" / "Logs" / "Synthesizer";
    }
#endif

    return std::filesystem::path("logs");
}

RuntimeConfig resolveRuntimeConfig(RuntimeConfig config) {
    if (config.logDirectory.empty() || config.logDirectory.is_relative()) {
        config.logDirectory = defaultLogDirectory();
    }
    return config;
}

std::optional<std::uint32_t> sourceMixerSlotCode(std::string_view key) {
    if (key == "robin") {
        return 0;
    }
    if (key == "test") {
        return 1;
    }
    if (key == "decor") {
        return 2;
    }
    if (key == "pieces") {
        return 3;
    }
    return std::nullopt;
}

#if defined(SYNTH_PLATFORM_MACOS)
std::string copyCfString(CFStringRef source) {
    if (source == nullptr) {
        return {};
    }

    const CFIndex length = CFStringGetLength(source);
    const CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<std::size_t>(maxSize), '\0');
    if (!CFStringGetCString(source, result.data(), maxSize, kCFStringEncodingUTF8)) {
        return {};
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::string queryDefaultOutputDeviceName() {
    AudioDeviceID deviceId = kAudioObjectUnknown;
    UInt32 size = sizeof(deviceId);
    AudioObjectPropertyAddress defaultDeviceAddress{
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    const OSStatus deviceStatus = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &defaultDeviceAddress,
        0,
        nullptr,
        &size,
        &deviceId);
    if (deviceStatus != noErr || deviceId == kAudioObjectUnknown) {
        return "System Default Output";
    }

    CFStringRef deviceName = nullptr;
    size = sizeof(deviceName);
    AudioObjectPropertyAddress nameAddress{
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    const OSStatus nameStatus = AudioObjectGetPropertyData(
        deviceId,
        &nameAddress,
        0,
        nullptr,
        &size,
        &deviceName);
    if (nameStatus != noErr || deviceName == nullptr) {
        return "System Default Output";
    }

    std::string name = copyCfString(deviceName);
    CFRelease(deviceName);
    return name.empty() ? "System Default Output" : name;
}
#endif

}  // namespace

SynthController::SynthController(
    RuntimeConfig config,
    std::unique_ptr<interfaces::IAudioDriver> driver)
    : config_(resolveRuntimeConfig(std::move(config))),
      logger_(config_.logDirectory),
      crashDiagnostics_(config_.logDirectory),
      driver_(std::move(driver)),
      debugRobinOscillatorParams_(envFlagEnabled("SYNTH_DEBUG_ROBIN")),
      debugCrashBreadcrumbs_(envFlagEnabled("SYNTH_DEBUG_CRASH")) {
    robin_.setLogger(&logger_);
    robin_.setDebugOscillatorParams(debugRobinOscillatorParams_);
}

SynthController::~SynthController() {
    stopAudio();
}

bool SynthController::initialize() {
    if (initialized_) {
        return true;
    }

    if (!logger_.initialize()) {
        return false;
    }

    if (crashDiagnostics_.initialize()) {
        crashDiagnostics_.install();
        logger_.info("Crash diagnostics initialized. Writing to " + crashDiagnostics_.crashLogPath().string());
    } else {
        logger_.warn("Crash diagnostics failed to initialize.");
    }

    audioBackendName_ = "CoreAudio";
#if defined(SYNTH_PLATFORM_MACOS)
    outputDeviceName_ = queryDefaultOutputDeviceName();
#else
    outputDeviceName_ = "Unavailable";
#endif

    if (!driver_) {
        driver_ = interfaces::createAudioDriver(logger_);
    }
    if (!driver_) {
        logger_.error("Could not create audio driver.");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        buildLiveGraphLocked();
        syncOutputDeviceSelectionLocked(driver_->availableOutputDevices());

        buildDefaultStateLocked();
        resizeScaffoldStateLocked();
        syncTestSourceLocked();
        syncOutputProcessorNodesLocked();
        applyRobinLevelLocked(robinMixerState_.level);
        applyTestLevelLocked(testMixerState_.level);
        robin_.setBaseFrequency(config_.frequency);
        liveGraph_.prepare(config_.sampleRate, config_.channels);
    }

    initialized_ = true;
    markStateSnapshotDirty();
    return true;
}

bool SynthController::startAudio() {
    if (!initialize()) {
        return false;
    }

    crashDiagnostics_.breadcrumb("startAudio requested.");

    if (driver_->isRunning()) {
        return true;
    }

    interfaces::AudioConfig audioConfig;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::uint32_t previousChannels = config_.channels;
        syncOutputDeviceSelectionLocked(driver_->availableOutputDevices());
        if (config_.channels != previousChannels) {
            reconfigureStructureLocked(config_.voiceCount, config_.oscillatorsPerVoice);
        }

        audioConfig.sampleRate = config_.sampleRate;
        audioConfig.channels = config_.channels;
        audioConfig.framesPerBuffer = config_.framesPerBuffer;
        audioConfig.outputDeviceId = config_.outputDeviceId;
    }

    logger_.info("Starting audio...");
    const bool started = driver_->start(
        audioConfig, [&](float* output, std::uint32_t frames, std::uint32_t channels) {
            std::lock_guard<std::mutex> lock(mutex_);
            renderAudioLocked(output, frames, channels);
        });

    if (!started) {
        logger_.error("Audio failed to start.");
        midiEnabled_ = false;
        oscEnabled_ = false;
        markStateSnapshotDirty();
        return false;
    }

    if (midiInput_ == nullptr) {
        midiInput_ = std::make_unique<io::MidiInput>(logger_);
    }
    if (!midiInput_->isRunning()) {
        midiEnabled_ = midiInput_->start([this](const io::MidiMessage& message) {
            RealtimeCommand command;
            command.index = message.sourceIndex;
            command.noteNumber = message.noteNumber;
            command.value = message.velocity;

            switch (message.type) {
                case io::MidiMessageType::NoteOn:
                    command.type = RealtimeCommandType::MidiNoteOn;
                    break;
                case io::MidiMessageType::NoteOff:
                    command.type = RealtimeCommandType::MidiNoteOff;
                    break;
                case io::MidiMessageType::AllNotesOff:
                    command.type = RealtimeCommandType::MidiAllNotesOff;
                    break;
            }
            submitRealtimeCommandOrApply(command);
        }, false);
        if (midiEnabled_) {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto midiSources = midiInput_->sources();
            for (const auto& source : midiSources) {
                (void)midiInput_->setSourceConnected(source.index, isPreferredMidiSource(source.name));
            }
            syncMidiRoutesLocked();
        }
    } else {
        midiEnabled_ = true;
        std::lock_guard<std::mutex> lock(mutex_);
        syncMidiRoutesLocked();
    }

    if (oscServer_ == nullptr) {
        oscServer_ = std::make_unique<io::OscServer>(logger_);
    }
    if (!oscServer_->isRunning()) {
        io::OscCallbacks callbacks;
        callbacks.onNumericParam = [this](std::string_view path, double value) {
            std::string errorMessage;
            if (!setParam(path, value, &errorMessage) && !errorMessage.empty()) {
                logger_.error("OSC: " + errorMessage);
            }
        };
        callbacks.onStringParam = [this](std::string_view path, std::string_view value) {
            std::string errorMessage;
            if (!setParam(path, value, &errorMessage) && !errorMessage.empty()) {
                logger_.error("OSC: " + errorMessage);
            }
        };
        callbacks.onNoteOn = [this](int noteNumber, float velocity) {
            submitRealtimeCommandOrApply({
                RealtimeCommandType::GlobalNoteOn,
                0,
                noteNumber,
                velocity,
            });
        };
        callbacks.onNoteOff = [this](int noteNumber) {
            submitRealtimeCommandOrApply({
                RealtimeCommandType::GlobalNoteOff,
                0,
                noteNumber,
                0.0f,
            });
        };
        oscEnabled_ = oscServer_->start(oscPort_, std::move(callbacks));
    } else {
        oscEnabled_ = true;
    }

    markStateSnapshotDirty();
    return true;
}

void SynthController::stopAudio() {
    crashDiagnostics_.breadcrumb("stopAudio requested.");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!heldMidiNotes_.empty()) {
            robin_.clearAllNotes();
            test_.clearAllNotes();
            (void)releaseAllHeldMidiNotesLocked();
        }
    }

    if (oscServer_ != nullptr && oscServer_->isRunning()) {
        oscServer_->stop();
    }
    if (midiInput_ != nullptr && midiInput_->isRunning()) {
        midiInput_->stop();
    }
    oscEnabled_ = false;
    midiEnabled_ = false;

    if (driver_ != nullptr && driver_->isRunning()) {
        driver_->stop();
        logger_.info("Stopped.");
    }

    markStateSnapshotDirty();
}

bool SynthController::isRunning() const {
    return driver_ != nullptr && driver_->isRunning();
}

std::string SynthController::stateJson() const {
    if (!stateSnapshotDirty_.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> cacheLock(stateSnapshotMutex_);
        return stateJsonCache_;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    if (!stateSnapshotDirty_.load(std::memory_order_relaxed)) {
        lock.unlock();
        std::lock_guard<std::mutex> cacheLock(stateSnapshotMutex_);
        return stateJsonCache_;
    }

    const_cast<SynthController*>(this)->drainRealtimeCommandsLocked();
    std::string nextSnapshot = buildStateJsonLocked();
    {
        std::lock_guard<std::mutex> cacheLock(stateSnapshotMutex_);
        stateJsonCache_ = nextSnapshot;
    }
    stateSnapshotDirty_.store(false, std::memory_order_release);
    return nextSnapshot;
}

void SynthController::markStateSnapshotDirty() const {
    stateSnapshotDirty_.store(true, std::memory_order_release);
}

std::string SynthController::buildStateJsonLocked() const {
    std::ostringstream json;
    const auto sourceOrder = liveGraph_.sourceOrder();
    const auto activeSourceNames = liveGraph_.activeSourceNames();
    const auto outputProcessorOrder = liveGraph_.outputProcessorOrder();
    const auto outputDevices = driver_ != nullptr ? driver_->availableOutputDevices() : std::vector<interfaces::OutputDeviceInfo>{};
    const interfaces::OutputDeviceInfo* selectedOutputDevice = findOutputDevice(outputDevices, config_.outputDeviceId);
    const auto midiSources = midiInput_ != nullptr ? midiInput_->sources() : std::vector<io::MidiSourceInfo>{};
    const auto midiConnectedSourceCount = midiInput_ != nullptr ? midiInput_->connectedSourceCount() : 0;

    json << "{"
         << "\"schemaVersion\":3,"
         << "\"engine\":{"
         << "\"running\":" << (isRunning() ? "true" : "false") << ","
         << "\"audioBackend\":\"" << escapeJson(audioBackendName_) << "\","
         << "\"outputDeviceName\":\"" << escapeJson(outputDeviceName_) << "\","
         << "\"selectedOutputDeviceId\":\"" << escapeJson(config_.outputDeviceId) << "\","
         << "\"maxOutputChannels\":"
         << (selectedOutputDevice != nullptr ? selectedOutputDevice->outputChannels : config_.channels) << ","
         << "\"sampleRate\":" << config_.sampleRate << ","
         << "\"outputChannels\":" << config_.channels << ","
         << "\"framesPerBuffer\":" << config_.framesPerBuffer << ","
         << "\"midiEnabled\":" << (midiEnabled_ ? "true" : "false") << ","
         << "\"midiSourceCount\":" << (midiInput_ != nullptr ? midiInput_->sourceCount() : 0) << ","
         << "\"midiConnectedSourceCount\":" << midiConnectedSourceCount << ","
         << "\"oscEnabled\":" << (oscEnabled_ ? "true" : "false") << ","
         << "\"oscPort\":" << oscPort_ << ","
         << "\"activeMidiNote\":" << activeMidiNote_ << ","
         << "\"midi\":{"
         << "\"running\":" << (midiEnabled_ ? "true" : "false") << ","
         << "\"connectedSourceCount\":" << midiConnectedSourceCount << ","
         << "\"sources\":[";

    for (std::size_t index = 0; index < midiSources.size(); ++index) {
        if (index > 0) {
            json << ",";
        }

        json << "{"
             << "\"index\":" << midiSources[index].index << ","
             << "\"name\":\"" << escapeJson(midiSources[index].name) << "\","
             << "\"connected\":" << (midiSources[index].connected ? "true" : "false") << ",";

        const MidiSourceRouteState* routeState = findMidiRouteLocked(midiSources[index].index);
        json << "\"routes\":{"
             << "\"robin\":" << ((routeState != nullptr && routeState->robin) ? "true" : "false") << ","
             << "\"test\":" << ((routeState != nullptr && routeState->test) ? "true" : "false") << ","
             << "\"decor\":" << ((routeState != nullptr && routeState->decor) ? "true" : "false") << ","
             << "\"pieces\":" << ((routeState != nullptr && routeState->pieces) ? "true" : "false")
             << "}"
             << "}";
    }

    json << "],"
         << "\"availableOutputDevices\":[";

    for (std::size_t index = 0; index < outputDevices.size(); ++index) {
        if (index > 0) {
            json << ",";
        }

        json << "{"
             << "\"id\":\"" << escapeJson(outputDevices[index].id) << "\","
             << "\"name\":\"" << escapeJson(outputDevices[index].name) << "\","
             << "\"outputChannels\":" << outputDevices[index].outputChannels << ","
             << "\"isDefault\":" << (outputDevices[index].isDefault ? "true" : "false")
             << "}";
    }

    json << "]"
         << "}"
         << "},"
         << "\"graph\":{"
         << "\"sourceStageOrder\":[";

    for (std::size_t index = 0; index < sourceOrder.size(); ++index) {
        if (index > 0) {
            json << ",";
        }

        json << "\"" << escapeJson(sourceOrder[index]) << "\"";
    }

    json << "],"
         << "\"activeSourceNodes\":[";

    for (std::size_t index = 0; index < activeSourceNames.size(); ++index) {
        if (index > 0) {
            json << ",";
        }

        json << "\"" << escapeJson(activeSourceNames[index]) << "\"";
    }

    json << "],"
         << "\"outputProcessorOrder\":[";

    for (std::size_t index = 0; index < outputProcessorOrder.size(); ++index) {
        if (index > 0) {
            json << ",";
        }

        json << "\"" << escapeJson(outputProcessorOrder[index]) << "\"";
    }

    json << "]"
         << "},"
         << "\"sourceMixer\":{"
         << "\"robin\":{"
         << "\"available\":" << (robinMixerState_.available ? "true" : "false") << ","
         << "\"implemented\":" << (robinMixerState_.implemented ? "true" : "false") << ","
         << "\"enabled\":" << (robinMixerState_.enabled ? "true" : "false") << ","
         << "\"level\":" << robinMixerState_.level << ","
         << "\"routeTarget\":\"" << escapeJson(sourceRouteTargetToString(robinMixerState_.routeTarget)) << "\""
         << "},"
         << "\"test\":{"
         << "\"available\":" << (testMixerState_.available ? "true" : "false") << ","
         << "\"implemented\":" << (testMixerState_.implemented ? "true" : "false") << ","
         << "\"enabled\":" << (testMixerState_.enabled ? "true" : "false") << ","
         << "\"level\":" << testMixerState_.level << ","
         << "\"routeTarget\":\"" << escapeJson(sourceRouteTargetToString(testMixerState_.routeTarget)) << "\""
         << "},"
         << "\"decor\":{"
         << "\"available\":" << (decorMixerState_.available ? "true" : "false") << ","
         << "\"implemented\":" << (decorMixerState_.implemented ? "true" : "false") << ","
         << "\"enabled\":" << (decorMixerState_.enabled ? "true" : "false") << ","
         << "\"level\":" << decorMixerState_.level << ","
         << "\"routeTarget\":\"" << escapeJson(sourceRouteTargetToString(decorMixerState_.routeTarget)) << "\""
         << "},"
         << "\"pieces\":{"
         << "\"available\":" << (piecesMixerState_.available ? "true" : "false") << ","
         << "\"implemented\":" << (piecesMixerState_.implemented ? "true" : "false") << ","
         << "\"enabled\":" << (piecesMixerState_.enabled ? "true" : "false") << ","
         << "\"level\":" << piecesMixerState_.level << ","
         << "\"routeTarget\":\"" << escapeJson(sourceRouteTargetToString(piecesMixerState_.routeTarget)) << "\""
         << "}"
         << "},"
         << "\"outputMixer\":{"
         << "\"outputs\":[";

    for (std::size_t outputIndex = 0; outputIndex < outputMixerChannels_.size(); ++outputIndex) {
        if (outputIndex > 0) {
            json << ",";
        }

        const auto& output = outputMixerChannels_[outputIndex];
        json << "{"
             << "\"index\":" << outputIndex << ","
             << "\"level\":" << output.level << ","
             << "\"delayMs\":" << output.delayMs << ","
             << "\"eq\":{"
             << "\"lowDb\":" << output.eq.lowDb << ","
             << "\"midDb\":" << output.eq.midDb << ","
             << "\"highDb\":" << output.eq.highDb
             << "}"
             << "}";
    }

    json << "]},"
         << "\"sources\":{"
         << "\"test\":";
    test_.appendStateJson(json);
    json << ",\"robin\":";
    robin_.appendStateJson(json);
    json << ","
         << "\"decor\":{"
         << "\"implemented\":" << (decorState_.implemented ? "true" : "false") << ","
         << "\"playable\":" << (decorState_.playable ? "true" : "false") << ","
         << "\"voiceCount\":" << decorState_.voiceCount << ","
         << "\"routingMode\":\"voice-per-output\""
         << "},"
         << "\"pieces\":{"
         << "\"implemented\":" << (piecesState_.implemented ? "true" : "false") << ","
         << "\"playable\":" << (piecesState_.playable ? "true" : "false") << ","
         << "\"voiceCount\":" << piecesState_.voiceCount << ","
         << "\"routingMode\":\"algorithmic-granular\""
         << "}"
         << "},"
         << "\"processors\":{"
         << "\"fx\":{"
         << "\"implemented\":false,"
         << "\"parallelSupported\":false,"
         << "\"saturator\":{"
         << "\"enabled\":" << (saturatorState_.enabled ? "true" : "false") << ","
         << "\"inputLevel\":" << saturatorState_.inputLevel << ","
         << "\"outputLevel\":" << saturatorState_.outputLevel
         << "},"
         << "\"chorus\":{"
         << "\"enabled\":" << (chorusState_.enabled ? "true" : "false") << ","
         << "\"depth\":" << chorusState_.depth << ","
         << "\"speedHz\":" << chorusState_.speedHz << ","
         << "\"phaseSpreadDegrees\":" << chorusState_.phaseSpreadDegrees
         << "},"
         << "\"sidechain\":{"
         << "\"enabled\":" << (sidechainState_.enabled ? "true" : "false")
         << "},"
         << "\"outputs\":[";

    for (std::size_t outputIndex = 0; outputIndex < outputMixerChannels_.size(); ++outputIndex) {
        if (outputIndex > 0) {
            json << ",";
        }

        json << "{"
             << "\"index\":" << outputIndex << ","
             << "\"hasDryPath\":true,"
             << "\"hasFxPath\":true"
             << "}";
    }

    json << "]}}}";
    return json.str();
}

bool SynthController::setParam(std::string_view path, double value, std::string* errorMessage) {
    if (debugCrashBreadcrumbs_) {
        std::ostringstream breadcrumb;
        breadcrumb << "setParam number path=" << path << " value=" << value;
        crashDiagnostics_.breadcrumb(breadcrumb.str());
    }

    if (const auto realtimeResult = tryEnqueueRealtimeNumericParam(path, value, errorMessage);
        realtimeResult != RealtimeParamResult::NotHandled) {
        return realtimeResult == RealtimeParamResult::Applied;
    }

    markStateSnapshotDirty();
    std::unique_lock<std::mutex> lock(mutex_);
    const auto parts = splitPath(path);

    if (const auto robinResult = robin_.applyNumericParam(parts, value, errorMessage);
        robinResult != RealtimeParamResult::NotHandled) {
        if ((path == "frequency" || path == "sources.robin.frequency") && robinResult == RealtimeParamResult::Applied) {
            config_.frequency = robin_.baseFrequencyHz();
        }
        return robinResult == RealtimeParamResult::Applied;
    }

    if (const auto testResult = test_.applyNumericParam(parts, value, errorMessage);
        testResult != RealtimeParamResult::NotHandled) {
        return testResult == RealtimeParamResult::Applied;
    }

    if (path == "engine.outputChannels") {
        const std::uint32_t requestedChannels = static_cast<std::uint32_t>(std::clamp(value, 1.0, 64.0));
        lock.unlock();
        return applyOutputEngineConfig(std::nullopt, requestedChannels, errorMessage);
    }

    if (path == "voiceCount" || path == "sources.robin.voiceCount") {
        reconfigureStructureLocked(static_cast<std::uint32_t>(std::clamp(value, 1.0, 32.0)),
                                   config_.oscillatorsPerVoice);
        return true;
    }

    if (path == "oscillatorsPerVoice" || path == "sources.robin.oscillatorsPerVoice") {
        reconfigureStructureLocked(config_.voiceCount,
                                   static_cast<std::uint32_t>(std::clamp(value, 1.0, 8.0)));
        return true;
    }

    if (path == "gain") {
        applyRobinLevelLocked(static_cast<float>(std::clamp(value, 0.0, 1.0)));
        return true;
    }

    if (parts.size() == 5 && parts[0] == "engine" && parts[1] == "midi" && parts[2] == "source" && parts[4] == "connected") {
        std::uint32_t sourceIndex = 0;
        if (!tryParseIndex(parts[3], sourceIndex) || midiInput_ == nullptr || !midiInput_->setSourceConnected(sourceIndex, value >= 0.5)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid MIDI source index.";
            }
            return false;
        }

        if (value < 0.5) {
            handleMidiAllNotesOffLocked(sourceIndex);
        }
        return true;
    }

    if (parts.size() == 6 && parts[0] == "engine" && parts[1] == "midi" && parts[2] == "source" && parts[4] == "route") {
        std::uint32_t sourceIndex = 0;
        if (!tryParseIndex(parts[3], sourceIndex)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid MIDI source index.";
            }
            return false;
        }

        MidiSourceRouteState* routeState = findMidiRouteLocked(sourceIndex);
        if (routeState == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "Unknown MIDI source route.";
            }
            return false;
        }

        const bool enabled = value >= 0.5;
        if (parts[5] == "robin") {
            routeState->robin = enabled;
            return true;
        }
        if (parts[5] == "test") {
            routeState->test = enabled;
            return true;
        }
        if (parts[5] == "decor") {
            routeState->decor = enabled;
            return true;
        }
        if (parts[5] == "pieces") {
            routeState->pieces = enabled;
            return true;
        }
    }

    if (parts.size() == 3 && parts[0] == "sourceMixer") {
        SourceMixerSlotState* sourceState = nullptr;
        if (parts[1] == "robin") {
            sourceState = &robinMixerState_;
        } else if (parts[1] == "test") {
            sourceState = &testMixerState_;
        } else if (parts[1] == "decor") {
            sourceState = &decorMixerState_;
        } else if (parts[1] == "pieces") {
            sourceState = &piecesMixerState_;
        }

        if (sourceState == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "Unsupported source mixer target.";
            }
            return false;
        }

        if (parts[2] == "level") {
            sourceState->level = static_cast<float>(std::clamp(value, 0.0, 1.0));
            if (parts[1] == "robin") {
                applyRobinLevelLocked(sourceState->level);
            } else if (parts[1] == "test") {
                applyTestLevelLocked(sourceState->level);
            }
            return true;
        }

        if (parts[2] == "enabled") {
            sourceState->enabled = value >= 0.5;
            if (parts[1] == "robin") {
                if (!sourceState->enabled) {
                    robin_.clearAllNotes();
                }
                applyRobinLevelLocked(sourceState->level);
            }
            return true;
        }
    }

    if (parts.size() == 4 && parts[0] == "outputMixer" && parts[1] == "output") {
        std::uint32_t outputIndex = 0;
        if (!tryParseIndex(parts[2], outputIndex) || outputIndex >= outputMixerChannels_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output mixer index.";
            }
            return false;
        }

        auto& output = outputMixerChannels_[outputIndex];
        if (parts[3] == "level") {
            output.level = static_cast<float>(std::clamp(value, 0.0, 1.0));
            outputMixerNode_.setLevel(outputIndex, output.level);
            return true;
        }

        if (parts[3] == "delayMs") {
            output.delayMs = static_cast<float>(std::clamp(value, 0.0, 250.0));
            outputMixerNode_.setDelayMs(outputIndex, output.delayMs);
            return true;
        }
    }

    if (parts.size() == 5 && parts[0] == "outputMixer" && parts[1] == "output" && parts[3] == "eq") {
        std::uint32_t outputIndex = 0;
        if (!tryParseIndex(parts[2], outputIndex) || outputIndex >= outputMixerChannels_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output mixer index.";
            }
            return false;
        }

        auto& eq = outputMixerChannels_[outputIndex].eq;
        const float nextValue = static_cast<float>(std::clamp(value, -24.0, 24.0));
        if (parts[4] == "lowDb") {
            eq.lowDb = nextValue;
            outputMixerNode_.setEqLow(outputIndex, eq.lowDb);
            return true;
        }
        if (parts[4] == "midDb") {
            eq.midDb = nextValue;
            outputMixerNode_.setEqMid(outputIndex, eq.midDb);
            return true;
        }
        if (parts[4] == "highDb") {
            eq.highDb = nextValue;
            outputMixerNode_.setEqHigh(outputIndex, eq.highDb);
            return true;
        }
    }

    if (parts.size() == 4 && parts[0] == "processors" && parts[1] == "fx" && parts[2] == "saturator") {
        if (parts[3] == "enabled") {
            saturatorState_.enabled = value >= 0.5;
            return true;
        }
        if (parts[3] == "inputLevel") {
            saturatorState_.inputLevel = static_cast<float>(std::clamp(value, 0.0, 2.0));
            return true;
        }
        if (parts[3] == "outputLevel") {
            saturatorState_.outputLevel = static_cast<float>(std::clamp(value, 0.0, 2.0));
            return true;
        }
    }

    if (parts.size() == 4 && parts[0] == "processors" && parts[1] == "fx" && parts[2] == "chorus") {
        if (parts[3] == "enabled") {
            chorusState_.enabled = value >= 0.5;
            fxRackNode_.setChorusEnabled(chorusState_.enabled);
            return true;
        }
        if (parts[3] == "depth") {
            chorusState_.depth = static_cast<float>(std::clamp(value, 0.0, 1.0));
            fxRackNode_.setChorusDepth(chorusState_.depth);
            return true;
        }
        if (parts[3] == "speedHz") {
            chorusState_.speedHz = static_cast<float>(std::clamp(value, 0.01, 20.0));
            fxRackNode_.setChorusSpeedHz(chorusState_.speedHz);
            return true;
        }
        if (parts[3] == "phaseSpreadDegrees") {
            chorusState_.phaseSpreadDegrees = static_cast<float>(std::clamp(value, 0.0, 360.0));
            fxRackNode_.setChorusPhaseSpreadDegrees(chorusState_.phaseSpreadDegrees);
            return true;
        }
    }

    if (parts.size() == 4 && parts[0] == "processors" && parts[1] == "fx" && parts[2] == "sidechain") {
        if (parts[3] == "enabled") {
            sidechainState_.enabled = value >= 0.5;
            return true;
        }
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unsupported numeric parameter path.";
    }
    return false;
}

bool SynthController::setParam(std::string_view path, std::string_view value, std::string* errorMessage) {
    if (debugCrashBreadcrumbs_) {
        crashDiagnostics_.breadcrumb(
            "setParam string path=" + std::string(path) + " value=" + std::string(value));
    }

    if (const auto realtimeResult = tryEnqueueRealtimeStringParam(path, value, errorMessage);
        realtimeResult != RealtimeParamResult::NotHandled) {
        return realtimeResult == RealtimeParamResult::Applied;
    }

    markStateSnapshotDirty();
    std::unique_lock<std::mutex> lock(mutex_);
    const auto parts = splitPath(path);

    if (const auto robinResult = robin_.applyStringParam(parts, value, errorMessage);
        robinResult != RealtimeParamResult::NotHandled) {
        return robinResult == RealtimeParamResult::Applied;
    }

    if (const auto testResult = test_.applyStringParam(parts, value, errorMessage);
        testResult != RealtimeParamResult::NotHandled) {
        return testResult == RealtimeParamResult::Applied;
    }

    if (path == "engine.outputDeviceId") {
        lock.unlock();
        return applyOutputEngineConfig(std::string(value), std::nullopt, errorMessage);
    }

    if (parts.size() == 3 && parts[0] == "sourceMixer" && parts[2] == "routeTarget") {
        SourceMixerSlotState* sourceState = nullptr;
        if (parts[1] == "robin") {
            sourceState = &robinMixerState_;
        } else if (parts[1] == "test") {
            sourceState = &testMixerState_;
        } else if (parts[1] == "decor") {
            sourceState = &decorMixerState_;
        } else if (parts[1] == "pieces") {
            sourceState = &piecesMixerState_;
        }

        if (sourceState == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "Unsupported source mixer target.";
            }
            return false;
        }

        SourceMixerSlotState::RouteTarget routeTarget;
        if (!tryParseSourceRouteTarget(value, routeTarget)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid source route target.";
            }
            return false;
        }

        sourceState->routeTarget = routeTarget;
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unsupported string parameter path.";
    }
    return false;
}

audio::Synth& SynthController::synth() {
    return robin_.synth();
}

core::Logger& SynthController::logger() {
    return logger_;
}

core::CrashDiagnostics& SynthController::crashDiagnostics() {
    return crashDiagnostics_;
}

const RuntimeConfig& SynthController::config() const {
    return config_;
}

const char* SynthController::waveformToString(dsp::Waveform waveform) {
    switch (waveform) {
        case dsp::Waveform::Square:
            return "square";
        case dsp::Waveform::Triangle:
            return "triangle";
        case dsp::Waveform::Saw:
            return "saw";
        case dsp::Waveform::Noise:
            return "noise";
        case dsp::Waveform::Sine:
        default:
            return "sine";
    }
}

bool SynthController::tryParseWaveform(std::string_view value, dsp::Waveform& waveform) {
    if (value == "sine") {
        waveform = dsp::Waveform::Sine;
        return true;
    }
    if (value == "square") {
        waveform = dsp::Waveform::Square;
        return true;
    }
    if (value == "triangle") {
        waveform = dsp::Waveform::Triangle;
        return true;
    }
    if (value == "saw") {
        waveform = dsp::Waveform::Saw;
        return true;
    }
    if (value == "noise") {
        waveform = dsp::Waveform::Noise;
        return true;
    }
    return false;
}

const char* SynthController::lfoWaveformToString(dsp::LfoWaveform waveform) {
    switch (waveform) {
        case dsp::LfoWaveform::Triangle:
            return "triangle";
        case dsp::LfoWaveform::SawDown:
            return "saw-down";
        case dsp::LfoWaveform::SawUp:
            return "saw-up";
        case dsp::LfoWaveform::Random:
            return "random";
        case dsp::LfoWaveform::Sine:
        default:
            return "sine";
    }
}

bool SynthController::tryParseLfoWaveform(std::string_view value, dsp::LfoWaveform& waveform) {
    if (value == "sine") {
        waveform = dsp::LfoWaveform::Sine;
        return true;
    }
    if (value == "triangle") {
        waveform = dsp::LfoWaveform::Triangle;
        return true;
    }
    if (value == "saw-down") {
        waveform = dsp::LfoWaveform::SawDown;
        return true;
    }
    if (value == "saw-up") {
        waveform = dsp::LfoWaveform::SawUp;
        return true;
    }
    if (value == "random") {
        waveform = dsp::LfoWaveform::Random;
        return true;
    }
    return false;
}

const char* SynthController::sourceRouteTargetToString(SourceMixerSlotState::RouteTarget routeTarget) {
    switch (routeTarget) {
        case SourceMixerSlotState::RouteTarget::Fx:
            return "fx";
        case SourceMixerSlotState::RouteTarget::Dry:
        default:
            return "dry";
    }
}

bool SynthController::tryParseSourceRouteTarget(std::string_view value, SourceMixerSlotState::RouteTarget& routeTarget) {
    if (value == "dry") {
        routeTarget = SourceMixerSlotState::RouteTarget::Dry;
        return true;
    }
    if (value == "fx") {
        routeTarget = SourceMixerSlotState::RouteTarget::Fx;
        return true;
    }
    return false;
}

const char* SynthController::routingPresetToString(RoutingPreset preset) {
    switch (preset) {
        case RoutingPreset::Forward:
            return "forward";
        case RoutingPreset::Backward:
            return "backward";
        case RoutingPreset::Random:
            return "random";
        case RoutingPreset::AllOutputs:
            return "all-outputs";
        case RoutingPreset::Custom:
            return "custom";
        case RoutingPreset::RoundRobin:
        default:
            return "round-robin";
    }
}

bool SynthController::tryParseRoutingPreset(std::string_view value, RoutingPreset& preset) {
    if (value == "forward") {
        preset = RoutingPreset::Forward;
        return true;
    }
    if (value == "backward") {
        preset = RoutingPreset::Backward;
        return true;
    }
    if (value == "random") {
        preset = RoutingPreset::Random;
        return true;
    }
    if (value == "round-robin") {
        preset = RoutingPreset::RoundRobin;
        return true;
    }
    if (value == "all-outputs") {
        preset = RoutingPreset::AllOutputs;
        return true;
    }
    if (value == "custom") {
        preset = RoutingPreset::Custom;
        return true;
    }
    return false;
}

const interfaces::OutputDeviceInfo* SynthController::findOutputDevice(
    const std::vector<interfaces::OutputDeviceInfo>& outputDevices,
    std::string_view outputDeviceId) {
    for (const auto& outputDevice : outputDevices) {
        if (outputDevice.id == outputDeviceId) {
            return &outputDevice;
        }
    }

    return nullptr;
}

std::string SynthController::escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

bool SynthController::tryParseIndex(std::string_view value, std::uint32_t& index) {
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    auto result = std::from_chars(begin, end, index);
    return result.ec == std::errc{} && result.ptr == end;
}

float SynthController::midiNoteToFrequency(int noteNumber) {
    return 440.0f * std::pow(2.0f, static_cast<float>(noteNumber - 69) / 12.0f);
}

RealtimeParamResult SynthController::tryEnqueueRealtimeNumericParam(std::string_view path,
                                                                    double value,
                                                                    std::string* errorMessage) {
    const auto parts = splitPath(path);
    RealtimeCommand command;

    if (const auto robinResult = robin_.tryBuildRealtimeNumericCommand(parts, value, command, errorMessage);
        robinResult != RealtimeParamResult::NotHandled) {
        if (robinResult == RealtimeParamResult::Applied) {
            submitRealtimeCommandOrApply(command);
        }
        return robinResult;
    }

    if (const auto testResult = test_.tryBuildRealtimeNumericCommand(parts, value, command, errorMessage);
        testResult != RealtimeParamResult::NotHandled) {
        if (testResult == RealtimeParamResult::Applied) {
            submitRealtimeCommandOrApply(command);
        }
        return testResult;
    }

    if (parts.size() == 3 && parts[0] == "sourceMixer" && parts[2] == "enabled") {
        const auto sourceCode = sourceMixerSlotCode(parts[1]);
        if (!sourceCode.has_value()) {
            return RealtimeParamResult::NotHandled;
        }
        command.type = RealtimeCommandType::SourceMixerEnabled;
        command.index = *sourceCode;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
    } else if (parts.size() == 3 && parts[0] == "sourceMixer" && parts[2] == "level") {
        if (parts[1] == "robin") {
            command.type = RealtimeCommandType::SourceLevelRobin;
        } else if (parts[1] == "test") {
            command.type = RealtimeCommandType::SourceLevelTest;
        } else {
            return RealtimeParamResult::NotHandled;
        }
        command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
    } else if (parts.size() == 4 && parts[0] == "outputMixer" && parts[1] == "output") {
        if (!tryParseIndex(parts[2], command.index)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output mixer index.";
            }
            return RealtimeParamResult::Failed;
        }

        if (parts[3] == "level") {
            command.type = RealtimeCommandType::OutputLevel;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[3] == "delayMs") {
            command.type = RealtimeCommandType::OutputDelay;
            command.value = static_cast<float>(std::clamp(value, 0.0, 250.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
    } else if (parts.size() == 5
               && parts[0] == "outputMixer"
               && parts[1] == "output"
               && parts[3] == "eq") {
        if (!tryParseIndex(parts[2], command.index)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output mixer index.";
            }
            return RealtimeParamResult::Failed;
        }

        command.value = static_cast<float>(std::clamp(value, -24.0, 24.0));
        if (parts[4] == "lowDb") {
            command.type = RealtimeCommandType::OutputEqLow;
        } else if (parts[4] == "midDb") {
            command.type = RealtimeCommandType::OutputEqMid;
        } else if (parts[4] == "highDb") {
            command.type = RealtimeCommandType::OutputEqHigh;
        } else {
            return RealtimeParamResult::NotHandled;
        }
    } else if (parts.size() == 4
               && parts[0] == "processors"
               && parts[1] == "fx"
               && parts[2] == "saturator") {
        if (parts[3] == "enabled") {
            command.type = RealtimeCommandType::SaturatorEnabled;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[3] == "inputLevel") {
            command.type = RealtimeCommandType::SaturatorInputLevel;
            command.value = static_cast<float>(std::clamp(value, 0.0, 2.0));
        } else if (parts[3] == "outputLevel") {
            command.type = RealtimeCommandType::SaturatorOutputLevel;
            command.value = static_cast<float>(std::clamp(value, 0.0, 2.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
    } else if (parts.size() == 4
               && parts[0] == "processors"
               && parts[1] == "fx"
               && parts[2] == "chorus") {
        if (parts[3] == "enabled") {
            command.type = RealtimeCommandType::ChorusEnabled;
            command.value = value >= 0.5 ? 1.0f : 0.0f;
        } else if (parts[3] == "depth") {
            command.type = RealtimeCommandType::ChorusDepth;
            command.value = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[3] == "speedHz") {
            command.type = RealtimeCommandType::ChorusSpeedHz;
            command.value = static_cast<float>(std::clamp(value, 0.01, 20.0));
        } else if (parts[3] == "phaseSpreadDegrees") {
            command.type = RealtimeCommandType::ChorusPhaseSpreadDegrees;
            command.value = static_cast<float>(std::clamp(value, 0.0, 360.0));
        } else {
            return RealtimeParamResult::NotHandled;
        }
    } else if (parts.size() == 4
               && parts[0] == "processors"
               && parts[1] == "fx"
               && parts[2] == "sidechain"
               && parts[3] == "enabled") {
        command.type = RealtimeCommandType::SidechainEnabled;
        command.value = value >= 0.5 ? 1.0f : 0.0f;
    } else {
        return RealtimeParamResult::NotHandled;
    }

    submitRealtimeCommandOrApply(command);
    return RealtimeParamResult::Applied;
}

RealtimeParamResult SynthController::tryEnqueueRealtimeStringParam(std::string_view path,
                                                                   std::string_view value,
                                                                   std::string* errorMessage) {
    const auto parts = splitPath(path);
    RealtimeCommand command;

    if (const auto robinResult = robin_.tryBuildRealtimeStringCommand(parts, value, command, errorMessage);
        robinResult != RealtimeParamResult::NotHandled) {
        if (robinResult == RealtimeParamResult::Applied) {
            submitRealtimeCommandOrApply(command);
        }
        return robinResult;
    }

    if (const auto testResult = test_.tryBuildRealtimeStringCommand(parts, value, command, errorMessage);
        testResult != RealtimeParamResult::NotHandled) {
        if (testResult == RealtimeParamResult::Applied) {
            submitRealtimeCommandOrApply(command);
        }
        return testResult;
    }

    if (parts.size() == 3 && parts[0] == "sourceMixer" && parts[2] == "routeTarget") {
        const auto sourceCode = sourceMixerSlotCode(parts[1]);
        if (!sourceCode.has_value()) {
            return RealtimeParamResult::NotHandled;
        }

        SourceMixerSlotState::RouteTarget routeTarget;
        if (!tryParseSourceRouteTarget(value, routeTarget)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid source route target.";
            }
            return RealtimeParamResult::Failed;
        }

        command.type = RealtimeCommandType::SourceMixerRouteTarget;
        command.index = *sourceCode;
        command.code = static_cast<std::uint32_t>(routeTarget);
    } else {
        return RealtimeParamResult::NotHandled;
    }

    submitRealtimeCommandOrApply(command);
    return RealtimeParamResult::Applied;
}

void SynthController::submitRealtimeCommandOrApply(RealtimeCommand command) {
    markStateSnapshotDirty();
    if (driver_ != nullptr && driver_->isRunning()) {
        enqueueRealtimeCommand(std::move(command));
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    applyRealtimeCommandLocked(command);
}

void SynthController::enqueueRealtimeCommand(RealtimeCommand command) {
    std::lock_guard<std::mutex> lock(realtimeCommandMutex_);
    pendingRealtimeCommands_.push_back(command);
}

void SynthController::drainRealtimeCommandsLocked() {
    std::deque<RealtimeCommand> commands;
    {
        std::lock_guard<std::mutex> lock(realtimeCommandMutex_);
        if (pendingRealtimeCommands_.empty()) {
            return;
        }
        commands.swap(pendingRealtimeCommands_);
    }

    for (const auto& command : commands) {
        applyRealtimeCommandLocked(command);
    }
}

void SynthController::applyRealtimeCommandLocked(const RealtimeCommand& command) {
    const auto sourceMixerStateForCode = [this](std::uint32_t code) -> SourceMixerSlotState* {
        switch (code) {
            case 0:
                return &robinMixerState_;
            case 1:
                return &testMixerState_;
            case 2:
                return &decorMixerState_;
            case 3:
                return &piecesMixerState_;
            default:
                return nullptr;
        }
    };

    if (test_.handlesRealtimeCommand(command.type)) {
        test_.applyRealtimeCommand(command);
        return;
    }

    if (robin_.handlesRealtimeCommand(command.type)) {
        robin_.applyRealtimeCommand(command);
        return;
    }

    switch (command.type) {
        case RealtimeCommandType::SourceLevelRobin:
            applyRobinLevelLocked(command.value);
            break;
        case RealtimeCommandType::SourceLevelTest:
            applyTestLevelLocked(command.value);
            break;
        case RealtimeCommandType::SourceMixerEnabled: {
            SourceMixerSlotState* sourceState = sourceMixerStateForCode(command.index);
            if (sourceState == nullptr) {
                return;
            }

            sourceState->enabled = command.value >= 0.5f;
            if (command.index == 0) {
                if (!sourceState->enabled) {
                    robin_.clearAllNotes();
                }
                applyRobinLevelLocked(sourceState->level);
            } else if (command.index == 1) {
                if (!sourceState->enabled) {
                    test_.clearAllNotes();
                }
                applyTestLevelLocked(sourceState->level);
            }
            break;
        }
        case RealtimeCommandType::SourceMixerRouteTarget: {
            SourceMixerSlotState* sourceState = sourceMixerStateForCode(command.index);
            if (sourceState == nullptr) {
                return;
            }
            sourceState->routeTarget = static_cast<SourceMixerSlotState::RouteTarget>(command.code);
            break;
        }
        case RealtimeCommandType::OutputLevel:
            if (command.index >= outputMixerChannels_.size()) {
                return;
            }
            outputMixerChannels_[command.index].level = std::clamp(command.value, 0.0f, 1.0f);
            outputMixerNode_.setLevel(command.index, outputMixerChannels_[command.index].level);
            break;
        case RealtimeCommandType::OutputDelay:
            if (command.index >= outputMixerChannels_.size()) {
                return;
            }
            outputMixerChannels_[command.index].delayMs = std::clamp(command.value, 0.0f, 250.0f);
            outputMixerNode_.setDelayMs(command.index, outputMixerChannels_[command.index].delayMs);
            break;
        case RealtimeCommandType::OutputEqLow:
            if (command.index >= outputMixerChannels_.size()) {
                return;
            }
            outputMixerChannels_[command.index].eq.lowDb = std::clamp(command.value, -24.0f, 24.0f);
            outputMixerNode_.setEqLow(command.index, outputMixerChannels_[command.index].eq.lowDb);
            break;
        case RealtimeCommandType::OutputEqMid:
            if (command.index >= outputMixerChannels_.size()) {
                return;
            }
            outputMixerChannels_[command.index].eq.midDb = std::clamp(command.value, -24.0f, 24.0f);
            outputMixerNode_.setEqMid(command.index, outputMixerChannels_[command.index].eq.midDb);
            break;
        case RealtimeCommandType::OutputEqHigh:
            if (command.index >= outputMixerChannels_.size()) {
                return;
            }
            outputMixerChannels_[command.index].eq.highDb = std::clamp(command.value, -24.0f, 24.0f);
            outputMixerNode_.setEqHigh(command.index, outputMixerChannels_[command.index].eq.highDb);
            break;
        case RealtimeCommandType::SaturatorEnabled:
            saturatorState_.enabled = command.value >= 0.5f;
            break;
        case RealtimeCommandType::SaturatorInputLevel:
            saturatorState_.inputLevel = std::clamp(command.value, 0.0f, 2.0f);
            break;
        case RealtimeCommandType::SaturatorOutputLevel:
            saturatorState_.outputLevel = std::clamp(command.value, 0.0f, 2.0f);
            break;
        case RealtimeCommandType::ChorusEnabled:
            chorusState_.enabled = command.value >= 0.5f;
            fxRackNode_.setChorusEnabled(chorusState_.enabled);
            break;
        case RealtimeCommandType::ChorusDepth:
            chorusState_.depth = std::clamp(command.value, 0.0f, 1.0f);
            fxRackNode_.setChorusDepth(chorusState_.depth);
            break;
        case RealtimeCommandType::ChorusSpeedHz:
            chorusState_.speedHz = std::clamp(command.value, 0.01f, 20.0f);
            fxRackNode_.setChorusSpeedHz(chorusState_.speedHz);
            break;
        case RealtimeCommandType::ChorusPhaseSpreadDegrees:
            chorusState_.phaseSpreadDegrees = std::clamp(command.value, 0.0f, 360.0f);
            fxRackNode_.setChorusPhaseSpreadDegrees(chorusState_.phaseSpreadDegrees);
            break;
        case RealtimeCommandType::SidechainEnabled:
            sidechainState_.enabled = command.value >= 0.5f;
            break;
        case RealtimeCommandType::RobinMasterFrequency:
            robin_.setBaseFrequency(command.value);
            config_.frequency = robin_.baseFrequencyHz();
            break;
        case RealtimeCommandType::TestActive:
        case RealtimeCommandType::TestMidiEnabled:
        case RealtimeCommandType::TestFrequency:
        case RealtimeCommandType::TestGain:
        case RealtimeCommandType::TestEnvelopeAttackMs:
        case RealtimeCommandType::TestEnvelopeDecayMs:
        case RealtimeCommandType::TestEnvelopeSustain:
        case RealtimeCommandType::TestEnvelopeReleaseMs:
        case RealtimeCommandType::TestOutputEnabled:
        case RealtimeCommandType::TestWaveform:
            return;
        case RealtimeCommandType::GlobalNoteOn:
            handleNoteOnLocked(command.noteNumber, command.value);
            break;
        case RealtimeCommandType::GlobalNoteOff:
            handleNoteOffLocked(command.noteNumber);
            break;
        case RealtimeCommandType::MidiNoteOn:
            handleMidiNoteOnLocked(command.index, command.noteNumber, command.value);
            break;
        case RealtimeCommandType::MidiNoteOff:
            handleMidiNoteOffLocked(command.index, command.noteNumber);
            break;
        case RealtimeCommandType::MidiAllNotesOff:
            handleMidiAllNotesOffLocked(command.index);
            break;
        default:
            break;
    }
}

void SynthController::buildLiveGraphLocked() {
    liveGraph_.clear();

    liveGraph_.addSourceNode({
        "robin",
        [this](double sampleRate, std::uint32_t outputChannels) {
            robin_.prepare(sampleRate, outputChannels);
        },
        [this]() { return robin_.implemented(); },
        [this]() { return robinMixerState_.enabled; },
        [this]() {
            return robinMixerState_.routeTarget == SourceMixerSlotState::RouteTarget::Fx
                ? graph::LiveGraph::SourceRenderTarget::FxBus
                : graph::LiveGraph::SourceRenderTarget::Dry;
        },
        [this](float* output, std::uint32_t frames, std::uint32_t channels) {
            robin_.renderAdd(output, frames, channels, robinMixerState_.enabled, robinMixerState_.level);
        },
        [this](int noteNumber, float velocity) { robin_.noteOn(noteNumber, velocity); },
        [this](int noteNumber) { robin_.noteOff(noteNumber); },
    });

    liveGraph_.addSourceNode({
        "test",
        [this](double sampleRate, std::uint32_t outputChannels) {
            test_.prepare(sampleRate, outputChannels);
        },
        [this]() { return test_.implemented(); },
        [this]() { return testMixerState_.enabled; },
        [this]() {
            return testMixerState_.routeTarget == SourceMixerSlotState::RouteTarget::Fx
                ? graph::LiveGraph::SourceRenderTarget::FxBus
                : graph::LiveGraph::SourceRenderTarget::Dry;
        },
        [this](float* output, std::uint32_t frames, std::uint32_t channels) {
            test_.renderAdd(output, frames, channels, testMixerState_.enabled, testMixerState_.level);
        },
        [this](int noteNumber, float velocity) { handleTestNoteOnLocked(noteNumber, velocity); },
        [this](int noteNumber) { handleTestNoteOffLocked(noteNumber); },
    });

    liveGraph_.addSourceNode({
        "decor",
        nullptr,
        [this]() { return decorMixerState_.implemented; },
        [this]() { return decorMixerState_.enabled; },
        [this]() {
            return decorMixerState_.routeTarget == SourceMixerSlotState::RouteTarget::Fx
                ? graph::LiveGraph::SourceRenderTarget::FxBus
                : graph::LiveGraph::SourceRenderTarget::Dry;
        },
        nullptr,
        nullptr,
        nullptr,
    });

    liveGraph_.addSourceNode({
        "pieces",
        nullptr,
        [this]() { return piecesMixerState_.implemented; },
        [this]() { return piecesMixerState_.enabled; },
        [this]() {
            return piecesMixerState_.routeTarget == SourceMixerSlotState::RouteTarget::Fx
                ? graph::LiveGraph::SourceRenderTarget::FxBus
                : graph::LiveGraph::SourceRenderTarget::Dry;
        },
        nullptr,
        nullptr,
        nullptr,
    });

    liveGraph_.addOutputProcessorNode({
        "fx-rack",
        graph::LiveGraph::OutputProcessTarget::FxBus,
        [this](double sampleRate, std::uint32_t outputChannels) {
            fxRackNode_.resize(outputChannels);
            fxRackNode_.prepare(sampleRate);
        },
        [this](float* output, std::uint32_t frames, std::uint32_t channels) {
            fxRackNode_.process(output, frames, channels);
        },
    });

    liveGraph_.addOutputProcessorNode({
        "output-mixer",
        graph::LiveGraph::OutputProcessTarget::Main,
        [this](double sampleRate, std::uint32_t outputChannels) {
            outputMixerNode_.resize(outputChannels);
            outputMixerNode_.prepare(sampleRate);
        },
        [this](float* output, std::uint32_t frames, std::uint32_t channels) {
            outputMixerNode_.process(output, frames, channels);
        },
    });
}

void SynthController::syncOutputDeviceSelectionLocked(
    const std::vector<interfaces::OutputDeviceInfo>& outputDevices) {
    if (outputDevices.empty()) {
        config_.outputDeviceId.clear();
        outputDeviceName_ = "Unavailable";
        config_.channels = std::max<std::uint32_t>(1, config_.channels);
        return;
    }

    const interfaces::OutputDeviceInfo* selectedDevice = findOutputDevice(outputDevices, config_.outputDeviceId);
    if (selectedDevice == nullptr) {
        for (const auto& outputDevice : outputDevices) {
            if (outputDevice.isDefault) {
                selectedDevice = &outputDevice;
                break;
            }
        }
    }
    if (selectedDevice == nullptr) {
        selectedDevice = &outputDevices.front();
    }

    config_.outputDeviceId = selectedDevice->id;
    outputDeviceName_ = selectedDevice->name;
    config_.channels = std::clamp(config_.channels, 1u, std::max<std::uint32_t>(1, selectedDevice->outputChannels));
}

void SynthController::buildDefaultStateLocked() {
    robin_.configureStructure(config_.voiceCount, config_.oscillatorsPerVoice, config_.channels);
    robin_.setBaseFrequency(config_.frequency);
    test_.resizeOutputs(std::max<std::uint32_t>(1, config_.channels));
}

void SynthController::resizeScaffoldStateLocked() {
    robinMixerState_.available = true;
    robinMixerState_.implemented = robin_.implemented();
    testMixerState_.available = true;
    testMixerState_.implemented = test_.implemented();
    decorMixerState_.available = true;
    piecesMixerState_.available = true;

    const std::uint32_t outputCount = std::max<std::uint32_t>(1, config_.channels);
    test_.resizeOutputs(outputCount);

    outputMixerChannels_.resize(outputCount);
    outputMixerNode_.resize(outputCount);
    decorState_.voiceCount = outputCount;
    piecesState_.voiceCount = std::max<std::uint32_t>(1, config_.voiceCount);
}

void SynthController::syncTestSourceLocked() {
    test_.prepare(config_.sampleRate, config_.channels);
}

void SynthController::syncMidiRoutesLocked() {
    midiSourceRoutes_.clear();

    if (midiInput_ == nullptr) {
        return;
    }

    const auto midiSources = midiInput_->sources();
    midiSourceRoutes_.reserve(midiSources.size());

    for (const auto& source : midiSources) {
        MidiSourceRouteState routeState;
        routeState.index = source.index;
        routeState.robin = true;
        routeState.test = true;
        midiSourceRoutes_.push_back(routeState);
    }
}

MidiSourceRouteState* SynthController::findMidiRouteLocked(std::uint32_t sourceIndex) {
    for (auto& routeState : midiSourceRoutes_) {
        if (routeState.index == sourceIndex) {
            return &routeState;
        }
    }

    return nullptr;
}

const MidiSourceRouteState* SynthController::findMidiRouteLocked(std::uint32_t sourceIndex) const {
    for (const auto& routeState : midiSourceRoutes_) {
        if (routeState.index == sourceIndex) {
            return &routeState;
        }
    }

    return nullptr;
}

void SynthController::syncOutputProcessorNodesLocked() {
    fxRackNode_.resize(static_cast<std::uint32_t>(outputMixerChannels_.size()));
    fxRackNode_.prepare(config_.sampleRate);
    fxRackNode_.setChorusEnabled(chorusState_.enabled);
    fxRackNode_.setChorusDepth(chorusState_.depth);
    fxRackNode_.setChorusSpeedHz(chorusState_.speedHz);
    fxRackNode_.setChorusPhaseSpreadDegrees(chorusState_.phaseSpreadDegrees);

    outputMixerNode_.resize(static_cast<std::uint32_t>(outputMixerChannels_.size()));
    outputMixerNode_.prepare(config_.sampleRate);
    for (std::size_t outputIndex = 0; outputIndex < outputMixerChannels_.size(); ++outputIndex) {
        outputMixerNode_.setLevel(static_cast<std::uint32_t>(outputIndex), outputMixerChannels_[outputIndex].level);
        outputMixerNode_.setDelayMs(static_cast<std::uint32_t>(outputIndex), outputMixerChannels_[outputIndex].delayMs);
        outputMixerNode_.setEq(static_cast<std::uint32_t>(outputIndex),
                               outputMixerChannels_[outputIndex].eq.lowDb,
                               outputMixerChannels_[outputIndex].eq.midDb,
                               outputMixerChannels_[outputIndex].eq.highDb);
    }
}

bool SynthController::applyOutputEngineConfig(std::optional<std::string> outputDeviceId,
                                              std::optional<std::uint32_t> outputChannels,
                                              std::string* errorMessage) {
    RuntimeConfig previousConfig;
    bool shouldRestartAudio = false;
    bool channelLayoutChanged = false;
    bool wasRunning = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (driver_ == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "Audio driver is not available.";
            }
            return false;
        }

        const auto outputDevices = driver_->availableOutputDevices();
        if (outputDevices.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "No output devices are available.";
            }
            return false;
        }

        previousConfig = config_;
        syncOutputDeviceSelectionLocked(outputDevices);

        const std::string previousOutputDeviceId = config_.outputDeviceId;
        if (outputDeviceId.has_value()) {
            const interfaces::OutputDeviceInfo* requestedDevice = findOutputDevice(outputDevices, *outputDeviceId);
            if (requestedDevice == nullptr) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Unknown output device.";
                }
                return false;
            }

            config_.outputDeviceId = requestedDevice->id;
            outputDeviceName_ = requestedDevice->name;
        }

        const interfaces::OutputDeviceInfo* selectedDevice = findOutputDevice(outputDevices, config_.outputDeviceId);
        if (selectedDevice == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = "No valid output device is selected.";
            }
            return false;
        }

        const std::uint32_t previousChannels = config_.channels;
        if (outputChannels.has_value()) {
            config_.channels = std::clamp(
                *outputChannels,
                1u,
                std::max<std::uint32_t>(1, selectedDevice->outputChannels));
        } else {
            config_.channels = std::clamp(
                config_.channels,
                1u,
                std::max<std::uint32_t>(1, selectedDevice->outputChannels));
        }

        channelLayoutChanged = config_.channels != previousChannels;
        wasRunning = driver_->isRunning();
        if (channelLayoutChanged && !wasRunning) {
            reconfigureStructureLocked(config_.voiceCount, config_.oscillatorsPerVoice);
        }

        shouldRestartAudio =
            wasRunning
            && (channelLayoutChanged || config_.outputDeviceId != previousOutputDeviceId);
    }

    if (!shouldRestartAudio) {
        return true;
    }

    stopAudio();
    if (channelLayoutChanged) {
        std::lock_guard<std::mutex> lock(mutex_);
        reconfigureStructureLocked(config_.voiceCount, config_.oscillatorsPerVoice);
    }
    if (startAudio()) {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = previousConfig;
        if (driver_ != nullptr) {
            syncOutputDeviceSelectionLocked(driver_->availableOutputDevices());
        }
        reconfigureStructureLocked(config_.voiceCount, config_.oscillatorsPerVoice);
    }

    if (!startAudio()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Audio restart failed and the previous output configuration could not be restored.";
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Failed to restart audio with the selected output configuration.";
    }
    return false;
}

void SynthController::renderAudioLocked(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    drainRealtimeCommandsLocked();
    liveGraph_.render(output, frames, channels);
}

void SynthController::applyRobinLevelLocked(float level) {
    robinMixerState_.level = std::clamp(level, 0.0f, 1.0f);
    config_.gain = robinMixerState_.level;
}

void SynthController::applyTestLevelLocked(float level) {
    testMixerState_.level = std::clamp(level, 0.0f, 1.0f);
}

void SynthController::reconfigureStructureLocked(std::uint32_t voiceCount, std::uint32_t oscillatorsPerVoice) {
    config_.voiceCount = std::max<std::uint32_t>(1, voiceCount);
    config_.oscillatorsPerVoice = std::max<std::uint32_t>(1, oscillatorsPerVoice);
    robin_.configureStructure(config_.voiceCount, config_.oscillatorsPerVoice, config_.channels);
    robin_.setBaseFrequency(config_.frequency);
    resizeScaffoldStateLocked();
    syncTestSourceLocked();
    syncOutputProcessorNodesLocked();
    applyRobinLevelLocked(robinMixerState_.level);
    applyTestLevelLocked(testMixerState_.level);
    liveGraph_.prepare(config_.sampleRate, config_.channels);
}

void SynthController::trackHeldMidiNoteLocked(int noteNumber, bool fromMidiSource, std::uint32_t sourceIndex) {
    heldMidiNotes_.push_back({fromMidiSource, sourceIndex, noteNumber});
    syncActiveMidiNoteLocked();
}

bool SynthController::releaseHeldMidiNoteLocked(int noteNumber, bool fromMidiSource, std::uint32_t sourceIndex) {
    const auto heldNote = std::find_if(
        heldMidiNotes_.begin(),
        heldMidiNotes_.end(),
        [noteNumber, fromMidiSource, sourceIndex](const HeldMidiNote& currentNote) {
            return currentNote.noteNumber == noteNumber
                && currentNote.fromMidiSource == fromMidiSource
                && (!fromMidiSource || currentNote.sourceIndex == sourceIndex);
        });
    if (heldNote == heldMidiNotes_.end()) {
        return false;
    }

    heldMidiNotes_.erase(heldNote);
    syncActiveMidiNoteLocked();
    return true;
}

std::vector<int> SynthController::releaseHeldMidiSourceNotesLocked(std::uint32_t sourceIndex) {
    std::vector<int> releasedNotes;

    auto nextHeldNote = std::find_if(
        heldMidiNotes_.begin(),
        heldMidiNotes_.end(),
        [sourceIndex](const HeldMidiNote& heldNote) {
            return heldNote.fromMidiSource && heldNote.sourceIndex == sourceIndex;
        });

    while (nextHeldNote != heldMidiNotes_.end()) {
        releasedNotes.push_back(nextHeldNote->noteNumber);
        nextHeldNote = heldMidiNotes_.erase(nextHeldNote);
        nextHeldNote = std::find_if(
            nextHeldNote,
            heldMidiNotes_.end(),
            [sourceIndex](const HeldMidiNote& heldNote) {
                return heldNote.fromMidiSource && heldNote.sourceIndex == sourceIndex;
            });
    }

    syncActiveMidiNoteLocked();
    return releasedNotes;
}

std::vector<SynthController::HeldMidiNote> SynthController::releaseAllHeldMidiNotesLocked() {
    std::vector<HeldMidiNote> releasedNotes = heldMidiNotes_;
    heldMidiNotes_.clear();
    syncActiveMidiNoteLocked();
    return releasedNotes;
}

void SynthController::syncActiveMidiNoteLocked() {
    activeMidiNote_ = heldMidiNotes_.empty() ? -1 : heldMidiNotes_.back().noteNumber;
}

void SynthController::handleNoteOnLocked(int noteNumber, float velocity) {
    trackHeldMidiNoteLocked(noteNumber, false);
    liveGraph_.noteOn(noteNumber, velocity);
    markStateSnapshotDirty();
}

void SynthController::handleNoteOffLocked(int noteNumber) {
    if (!releaseHeldMidiNoteLocked(noteNumber, false)) {
        return;
    }

    liveGraph_.noteOff(noteNumber);
    markStateSnapshotDirty();
}

void SynthController::handleMidiNoteOnLocked(std::uint32_t sourceIndex, int noteNumber, float velocity) {
    trackHeldMidiNoteLocked(noteNumber, true, sourceIndex);
    markStateSnapshotDirty();

    const MidiSourceRouteState* routeState = findMidiRouteLocked(sourceIndex);
    if (routeState == nullptr) {
        return;
    }

    if (routeState->robin && robinMixerState_.enabled) {
        robin_.noteOn(noteNumber, velocity);
    }
    if (routeState->test && testMixerState_.enabled) {
        handleTestNoteOnLocked(noteNumber, velocity);
    }
}

void SynthController::handleMidiNoteOffLocked(std::uint32_t sourceIndex, int noteNumber) {
    if (!releaseHeldMidiNoteLocked(noteNumber, true, sourceIndex)) {
        return;
    }

    markStateSnapshotDirty();

    const MidiSourceRouteState* routeState = findMidiRouteLocked(sourceIndex);
    if (routeState == nullptr) {
        return;
    }

    if (routeState->robin && robinMixerState_.enabled) {
        robin_.noteOff(noteNumber);
    }
    if (routeState->test && testMixerState_.enabled) {
        handleTestNoteOffLocked(noteNumber);
    }
}

void SynthController::handleMidiAllNotesOffLocked(std::uint32_t sourceIndex) {
    const std::vector<int> releasedNotes = releaseHeldMidiSourceNotesLocked(sourceIndex);
    if (releasedNotes.empty()) {
        return;
    }

    markStateSnapshotDirty();

    const MidiSourceRouteState* routeState = findMidiRouteLocked(sourceIndex);
    if (routeState == nullptr) {
        return;
    }

    for (const int noteNumber : releasedNotes) {
        if (routeState->robin && robinMixerState_.enabled) {
            robin_.noteOff(noteNumber);
        }
        if (routeState->test && testMixerState_.enabled) {
            handleTestNoteOffLocked(noteNumber);
        }
    }
}

void SynthController::handleTestNoteOnLocked(int noteNumber, float velocity) {
    test_.noteOn(noteNumber, velocity);
}

void SynthController::handleTestNoteOffLocked(int noteNumber) {
    test_.noteOff(noteNumber);
}

}  // namespace synth::app
