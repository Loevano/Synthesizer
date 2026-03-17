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

void assignDefaultTestOutputs(std::vector<bool>& outputs) {
    std::fill(outputs.begin(), outputs.end(), false);
    if (!outputs.empty()) {
        outputs[0] = true;
    }
    if (outputs.size() > 1) {
        outputs[1] = true;
    }
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
      debugCrashBreadcrumbs_(envFlagEnabled("SYNTH_DEBUG_CRASH")) {}

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

        audio::SynthConfig synthConfig;
        synthConfig.voiceCount = config_.voiceCount;
        synthConfig.oscillatorsPerVoice = config_.oscillatorsPerVoice;
        robinSource_.configure(synthConfig);

        buildDefaultStateLocked();
        resizeScaffoldStateLocked();
        syncTestSourceLocked();
        syncOutputProcessorNodesLocked();
        applyRobinLevelLocked(robinMixerState_.level);
        applyTestLevelLocked(testMixerState_.level);
        applyGlobalFrequencyLocked(config_.frequency);
        applyGlobalWaveformLocked(config_.waveform);
        applyRoutingPresetLocked(routingPreset_);
        syncRobinEnvelopeLocked();
        syncLfoLocked();
        syncAllVoicesLocked();
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
        midiEnabled_ = midiInput_->start([this](std::uint32_t sourceIndex, int noteNumber, float velocity) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (velocity > 0.0f) {
                handleMidiNoteOnLocked(sourceIndex, noteNumber, velocity);
            } else {
                handleMidiNoteOffLocked(sourceIndex, noteNumber);
            }
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
            std::lock_guard<std::mutex> lock(mutex_);
            handleNoteOnLocked(noteNumber, velocity);
        };
        callbacks.onNoteOff = [this](int noteNumber) {
            std::lock_guard<std::mutex> lock(mutex_);
            handleNoteOffLocked(noteNumber);
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
         << "\"test\":{"
         << "\"implemented\":" << (testState_.implemented ? "true" : "false") << ","
         << "\"playable\":" << (testState_.playable ? "true" : "false") << ","
         << "\"active\":" << (testState_.active ? "true" : "false") << ","
         << "\"midiEnabled\":" << (testState_.midiEnabled ? "true" : "false") << ","
         << "\"frequency\":" << testState_.frequency << ","
         << "\"gain\":" << testState_.gain << ","
         << "\"waveform\":\"" << escapeJson(waveformToString(testState_.waveform)) << "\","
         << "\"envelope\":{"
         << "\"attackMs\":" << testState_.envelope.attackMs << ","
         << "\"decayMs\":" << testState_.envelope.decayMs << ","
         << "\"sustain\":" << testState_.envelope.sustain << ","
         << "\"releaseMs\":" << testState_.envelope.releaseMs
         << "},"
         << "\"outputs\":[";

    for (std::size_t outputIndex = 0; outputIndex < testState_.outputs.size(); ++outputIndex) {
        if (outputIndex > 0) {
            json << ",";
        }
        json << (testState_.outputs[outputIndex] ? "true" : "false");
    }

    json << "]"
         << "},"
         << "\"robin\":{"
         << "\"implemented\":true,"
         << "\"playable\":true,"
         << "\"voiceCount\":" << config_.voiceCount << ","
         << "\"oscillatorsPerVoice\":" << config_.oscillatorsPerVoice << ","
         << "\"frequency\":" << config_.frequency << ","
         << "\"gain\":" << robinMasterVoiceGain_ << ","
         << "\"transposeSemitones\":" << robinPitchState_.transposeSemitones << ","
         << "\"fineTuneCents\":" << robinPitchState_.fineTuneCents << ","
         << "\"routingPreset\":\"" << escapeJson(routingPresetToString(routingPreset_)) << "\","
         << "\"vcf\":{"
         << "\"cutoffHz\":" << robinVcfState_.cutoffHz << ","
         << "\"resonance\":" << robinVcfState_.resonance
         << "},"
         << "\"envVcf\":{"
         << "\"attackMs\":" << robinEnvVcfState_.attackMs << ","
         << "\"decayMs\":" << robinEnvVcfState_.decayMs << ","
         << "\"sustain\":" << robinEnvVcfState_.sustain << ","
         << "\"releaseMs\":" << robinEnvVcfState_.releaseMs << ","
         << "\"amount\":" << robinEnvVcfState_.amount
         << "},"
         << "\"envelope\":{"
         << "\"attackMs\":" << robinEnvelopeState_.attackMs << ","
         << "\"decayMs\":" << robinEnvelopeState_.decayMs << ","
         << "\"sustain\":" << robinEnvelopeState_.sustain << ","
         << "\"releaseMs\":" << robinEnvelopeState_.releaseMs
         << "},"
         << "\"lfo\":{"
         << "\"enabled\":" << (lfoState_.enabled ? "true" : "false") << ","
         << "\"depth\":" << lfoState_.depth << ","
         << "\"phaseSpreadDegrees\":" << lfoState_.phaseSpreadDegrees << ","
         << "\"polarityFlip\":" << (lfoState_.polarityFlip ? "true" : "false") << ","
         << "\"unlinkedOutputs\":" << (lfoState_.unlinkedOutputs ? "true" : "false") << ","
         << "\"clockLinked\":" << (lfoState_.clockLinked ? "true" : "false") << ","
         << "\"tempoBpm\":" << lfoState_.tempoBpm << ","
         << "\"rateMultiplier\":" << lfoState_.rateMultiplier << ","
         << "\"fixedFrequencyHz\":" << lfoState_.fixedFrequencyHz << ","
         << "\"waveform\":\"" << escapeJson(lfoWaveformToString(lfoState_.waveform)) << "\""
         << "},"
         << "\"masterOscillators\":[";

    for (std::size_t oscillatorIndex = 0; oscillatorIndex < masterOscillators_.size(); ++oscillatorIndex) {
        if (oscillatorIndex > 0) {
            json << ",";
        }

        const auto& oscillator = masterOscillators_[oscillatorIndex];
        json << "{"
             << "\"index\":" << oscillatorIndex << ","
             << "\"enabled\":" << (oscillator.enabled ? "true" : "false") << ","
             << "\"gain\":" << oscillator.gain << ","
             << "\"relativeToVoice\":" << (oscillator.relativeToVoice ? "true" : "false") << ","
             << "\"frequencyValue\":" << oscillator.frequencyValue << ","
             << "\"waveform\":\"" << escapeJson(waveformToString(oscillator.waveform)) << "\""
             << "}";
    }

    json << "],"
         << "\"voices\":[";

    for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (voiceIndex > 0) {
            json << ",";
        }

        const auto& voice = voices_[voiceIndex];
        json << "{"
             << "\"index\":" << voiceIndex << ","
             << "\"active\":" << (voice.active ? "true" : "false") << ","
             << "\"linkedToMaster\":" << (voice.linkedToMaster ? "true" : "false") << ","
             << "\"frequency\":" << voice.frequency << ","
             << "\"gain\":" << voice.gain << ","
             << "\"vcf\":{"
             << "\"cutoffHz\":" << voice.vcf.cutoffHz << ","
             << "\"resonance\":" << voice.vcf.resonance
             << "},"
             << "\"envVcf\":{"
             << "\"attackMs\":" << voice.envVcf.attackMs << ","
             << "\"decayMs\":" << voice.envVcf.decayMs << ","
             << "\"sustain\":" << voice.envVcf.sustain << ","
             << "\"releaseMs\":" << voice.envVcf.releaseMs << ","
             << "\"amount\":" << voice.envVcf.amount
             << "},"
             << "\"envelope\":{"
             << "\"attackMs\":" << voice.envelope.attackMs << ","
             << "\"decayMs\":" << voice.envelope.decayMs << ","
             << "\"sustain\":" << voice.envelope.sustain << ","
             << "\"releaseMs\":" << voice.envelope.releaseMs
             << "},"
             << "\"outputs\":[";

        for (std::size_t outputIndex = 0; outputIndex < voice.outputs.size(); ++outputIndex) {
            if (outputIndex > 0) {
                json << ",";
            }
            json << (voice.outputs[outputIndex] ? "true" : "false");
        }

        json << "],\"oscillators\":[";
        for (std::size_t oscillatorIndex = 0; oscillatorIndex < voice.oscillators.size(); ++oscillatorIndex) {
            if (oscillatorIndex > 0) {
                json << ",";
            }

            const auto& oscillator = voice.oscillators[oscillatorIndex];
            json << "{"
                 << "\"index\":" << oscillatorIndex << ","
                 << "\"enabled\":" << (oscillator.enabled ? "true" : "false") << ","
                 << "\"gain\":" << oscillator.gain << ","
                 << "\"relativeToVoice\":" << (oscillator.relativeToVoice ? "true" : "false") << ","
                 << "\"frequencyValue\":" << oscillator.frequencyValue << ","
                 << "\"waveform\":\"" << escapeJson(waveformToString(oscillator.waveform)) << "\""
                 << "}";
        }

        json << "]}";
    }

    json << "]},"
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

    if (path == "frequency" || path == "sources.robin.frequency") {
        applyGlobalFrequencyLocked(static_cast<float>(std::clamp(value, 20.0, 20000.0)));
        return true;
    }

    if (path == "sources.robin.gain") {
        robinMasterVoiceGain_ = static_cast<float>(std::clamp(value, 0.0, 1.0));
        syncLinkedRobinVoicesLocked();
        return true;
    }

    if (path == "sources.robin.transposeSemitones") {
        robinPitchState_.transposeSemitones = static_cast<float>(std::clamp(value, -12.0, 12.0));
        syncAssignedRobinVoiceFrequenciesLocked();
        return true;
    }

    if (path == "sources.robin.fineTuneCents") {
        robinPitchState_.fineTuneCents = static_cast<float>(std::clamp(value, -100.0, 100.0));
        syncAssignedRobinVoiceFrequenciesLocked();
        return true;
    }

    if (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "vcf") {
        if (parts[3] == "cutoffHz") {
            robinVcfState_.cutoffHz = static_cast<float>(std::clamp(value, 20.0, 20000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "resonance") {
            robinVcfState_.resonance = static_cast<float>(std::clamp(value, 0.1, 10.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
    }

    if (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "envVcf") {
        if (parts[3] == "attackMs") {
            robinEnvVcfState_.attackMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "decayMs") {
            robinEnvVcfState_.decayMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "sustain") {
            robinEnvVcfState_.sustain = static_cast<float>(std::clamp(value, 0.0, 1.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "releaseMs") {
            robinEnvVcfState_.releaseMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "amount") {
            robinEnvVcfState_.amount = static_cast<float>(std::clamp(value, 0.0, 1.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
    }

    if (parts.size() == 4 && parts[0] == "sources" && parts[1] == "robin" && parts[2] == "envelope") {
        if (parts[3] == "attackMs") {
            robinEnvelopeState_.attackMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "decayMs") {
            robinEnvelopeState_.decayMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "sustain") {
            robinEnvelopeState_.sustain = static_cast<float>(std::clamp(value, 0.0, 1.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
        if (parts[3] == "releaseMs") {
            robinEnvelopeState_.releaseMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
            syncLinkedRobinVoicesLocked();
            return true;
        }
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
                    robinSource_.synth().clearNotes();
                    robinVoiceAssignments_.clear();
                    robinNextVoiceCursor_ = 0;
                    autoActivatedVoice0_ = false;
                    resetRobinRoutingStateLocked();
                }
                applyRobinLevelLocked(sourceState->level);
            }
            return true;
        }
    }

    if (parts.size() >= 3 && parts[0] == "sources" && parts[1] == "test") {
        if (parts.size() == 3 && parts[2] == "active") {
            testState_.active = value >= 0.5;
            testSource_.setActive(testState_.active);
            return true;
        }

        if (parts.size() == 3 && parts[2] == "midiEnabled") {
            testState_.midiEnabled = value >= 0.5;
            testSource_.setMidiEnabled(testState_.midiEnabled);
            return true;
        }

        if (parts.size() == 3 && parts[2] == "frequency") {
            testState_.frequency = static_cast<float>(std::clamp(value, 20.0, 20000.0));
            testSource_.setFrequency(testState_.frequency);
            return true;
        }

        if (parts.size() == 3 && parts[2] == "gain") {
            testState_.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
            testSource_.setGain(testState_.gain);
            return true;
        }

        if (parts.size() == 4 && parts[2] == "output") {
            std::uint32_t outputIndex = 0;
            if (!tryParseIndex(parts[3], outputIndex) || outputIndex >= testState_.outputs.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid test output index.";
                }
                return false;
            }

            testState_.outputs[outputIndex] = value >= 0.5;
            testSource_.setOutputEnabled(outputIndex, testState_.outputs[outputIndex]);
            return true;
        }

        if (parts.size() == 4 && parts[2] == "envelope") {
            if (parts[3] == "attackMs") {
                testState_.envelope.attackMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                testSource_.setEnvelopeAttackSeconds(testState_.envelope.attackMs / 1000.0f);
                return true;
            }
            if (parts[3] == "decayMs") {
                testState_.envelope.decayMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                testSource_.setEnvelopeDecaySeconds(testState_.envelope.decayMs / 1000.0f);
                return true;
            }
            if (parts[3] == "sustain") {
                testState_.envelope.sustain = static_cast<float>(std::clamp(value, 0.0, 1.0));
                testSource_.setEnvelopeSustainLevel(testState_.envelope.sustain);
                return true;
            }
            if (parts[3] == "releaseMs") {
                testState_.envelope.releaseMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                testSource_.setEnvelopeReleaseSeconds(testState_.envelope.releaseMs / 1000.0f);
                return true;
            }
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

    if (path == "lfo.enabled" || path == "sources.robin.lfo.enabled") {
        lfoState_.enabled = value >= 0.5;
        robinSource_.synth().setLfoEnabled(lfoState_.enabled);
        return true;
    }

    if (path == "lfo.depth" || path == "sources.robin.lfo.depth") {
        lfoState_.depth = static_cast<float>(std::clamp(value, 0.0, 1.0));
        robinSource_.synth().setLfoDepth(lfoState_.depth);
        return true;
    }

    if (path == "lfo.phaseSpreadDegrees" || path == "sources.robin.lfo.phaseSpreadDegrees") {
        lfoState_.phaseSpreadDegrees = static_cast<float>(std::clamp(value, 0.0, 360.0));
        robinSource_.synth().setLfoPhaseSpreadDegrees(lfoState_.phaseSpreadDegrees);
        return true;
    }

    if (path == "lfo.polarityFlip" || path == "sources.robin.lfo.polarityFlip") {
        lfoState_.polarityFlip = value >= 0.5;
        robinSource_.synth().setLfoPolarityFlip(lfoState_.polarityFlip);
        return true;
    }

    if (path == "lfo.unlinkedOutputs" || path == "sources.robin.lfo.unlinkedOutputs") {
        lfoState_.unlinkedOutputs = value >= 0.5;
        robinSource_.synth().setLfoUnlinkedOutputs(lfoState_.unlinkedOutputs);
        return true;
    }

    if (path == "lfo.clockLinked" || path == "sources.robin.lfo.clockLinked") {
        lfoState_.clockLinked = value >= 0.5;
        robinSource_.synth().setLfoClockLinked(lfoState_.clockLinked);
        return true;
    }

    if (path == "lfo.tempoBpm" || path == "sources.robin.lfo.tempoBpm") {
        lfoState_.tempoBpm = static_cast<float>(std::clamp(value, 20.0, 300.0));
        robinSource_.synth().setLfoTempoBpm(lfoState_.tempoBpm);
        return true;
    }

    if (path == "lfo.rateMultiplier" || path == "sources.robin.lfo.rateMultiplier") {
        lfoState_.rateMultiplier = static_cast<float>(std::clamp(value, 0.125, 8.0));
        robinSource_.synth().setLfoRateMultiplier(lfoState_.rateMultiplier);
        return true;
    }

    if (path == "lfo.fixedFrequencyHz" || path == "sources.robin.lfo.fixedFrequencyHz") {
        lfoState_.fixedFrequencyHz = static_cast<float>(std::clamp(value, 0.01, 40.0));
        robinSource_.synth().setLfoFixedFrequencyHz(lfoState_.fixedFrequencyHz);
        return true;
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

    std::size_t rootOffset = 0;
    if (parts.size() >= 3 && parts[0] == "sources" && parts[1] == "robin") {
        rootOffset = 2;
    }

    if (rootOffset == 2 && parts.size() == 5 && parts[rootOffset] == "oscillator") {
        std::uint32_t oscillatorIndex = 0;
        if (!tryParseIndex(parts[rootOffset + 1], oscillatorIndex)
            || oscillatorIndex >= masterOscillators_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return false;
        }

        auto& oscillator = masterOscillators_[oscillatorIndex];

        if (parts[rootOffset + 2] == "enabled") {
            oscillator.enabled = value >= 0.5;
        } else if (parts[rootOffset + 2] == "gain") {
            oscillator.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
        } else if (parts[rootOffset + 2] == "relative") {
            const bool nextRelative = value >= 0.5;
            if (oscillator.relativeToVoice != nextRelative) {
                if (nextRelative) {
                    oscillator.frequencyValue = static_cast<float>(std::clamp(
                        oscillator.frequencyValue / std::max(1.0f, config_.frequency),
                        0.01f,
                        8.0f));
                } else {
                    oscillator.frequencyValue = static_cast<float>(std::clamp(
                        oscillator.frequencyValue * std::max(1.0f, config_.frequency),
                        1.0f,
                        20000.0f));
                }
            }

            oscillator.relativeToVoice = nextRelative;
        } else if (parts[rootOffset + 2] == "frequency") {
            oscillator.frequencyValue = oscillator.relativeToVoice
                ? static_cast<float>(std::clamp(value, 0.01, 8.0))
                : static_cast<float>(std::clamp(value, 1.0, 20000.0));
        } else {
            if (errorMessage != nullptr) {
                *errorMessage = "Unsupported master oscillator parameter.";
            }
            return false;
        }

        for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
            if (!voices_[voiceIndex].linkedToMaster) {
                continue;
            }

            robinSource_.synth().setOscillatorEnabled(voiceIndex, oscillatorIndex, oscillator.enabled);
            robinSource_.synth().setOscillatorGain(voiceIndex, oscillatorIndex, oscillator.gain);
            robinSource_.synth().setOscillatorRelativeToVoice(voiceIndex, oscillatorIndex, oscillator.relativeToVoice);
            robinSource_.synth().setOscillatorFrequency(voiceIndex, oscillatorIndex, oscillator.frequencyValue);
        }

        if (debugRobinOscillatorParams_) {
            std::ostringstream valueDescription;
            valueDescription << value;
            logRobinMasterOscillatorUpdateLocked(path, valueDescription.str());
        }
        return true;
    }

    if (parts.size() >= rootOffset + 3 && parts[rootOffset] == "voice") {
        std::uint32_t voiceIndex = 0;
        if (!tryParseIndex(parts[rootOffset + 1], voiceIndex) || voiceIndex >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return false;
        }

        auto& voice = voices_[voiceIndex];

        if (parts.size() == rootOffset + 3 && parts[rootOffset + 2] == "active") {
            voice.active = value >= 0.5;
            robinSource_.synth().setVoiceActive(voiceIndex, voice.active);
            return true;
        }

        if (parts.size() == rootOffset + 3 && parts[rootOffset + 2] == "linkedToMaster") {
            const bool nextLinkedToMaster = value >= 0.5;
            voice.linkedToMaster = nextLinkedToMaster;
            syncVoiceStateLocked(voiceIndex);
            return true;
        }

        if (parts.size() == rootOffset + 3 && parts[rootOffset + 2] == "resetToMasterState") {
            copyMasterStateToVoiceLocked(voice);
            if (!voice.linkedToMaster) {
                syncVoiceStateLocked(voiceIndex);
            }
            return true;
        }

        if (parts.size() == rootOffset + 3 && parts[rootOffset + 2] == "frequency") {
            if (voice.linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return false;
            }
            voice.frequency = static_cast<float>(std::clamp(value, 20.0, 20000.0));
            syncRobinVoiceFrequencyLocked(voiceIndex);
            return true;
        }

        if (parts.size() == rootOffset + 3 && parts[rootOffset + 2] == "gain") {
            if (voice.linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return false;
            }
            voice.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
            robinSource_.synth().setVoiceGain(voiceIndex, voice.gain);
            return true;
        }

        if (parts.size() == rootOffset + 4 && parts[rootOffset + 2] == "output") {
            std::uint32_t outputIndex = 0;
            if (!tryParseIndex(parts[rootOffset + 3], outputIndex) || outputIndex >= voice.outputs.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid output index.";
                }
                return false;
            }

            voice.outputs[outputIndex] = value >= 0.5;
            routingPreset_ = RoutingPreset::Custom;
            resetRobinRoutingStateLocked();
            robinSource_.synth().setVoiceOutputEnabled(voiceIndex, outputIndex, voice.outputs[outputIndex]);
            return true;
        }

        if (parts.size() == rootOffset + 4 && parts[rootOffset + 2] == "vcf") {
            if (voice.linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return false;
            }

            if (parts[rootOffset + 3] == "cutoffHz") {
                voice.vcf.cutoffHz = static_cast<float>(std::clamp(value, 20.0, 20000.0));
                robinSource_.synth().setVoiceFilterCutoffHz(voiceIndex, voice.vcf.cutoffHz);
                return true;
            }
            if (parts[rootOffset + 3] == "resonance") {
                voice.vcf.resonance = static_cast<float>(std::clamp(value, 0.1, 10.0));
                robinSource_.synth().setVoiceFilterResonance(voiceIndex, voice.vcf.resonance);
                return true;
            }
        }

        if (parts.size() == rootOffset + 4 && parts[rootOffset + 2] == "envVcf") {
            if (voice.linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return false;
            }

            if (parts[rootOffset + 3] == "attackMs") {
                voice.envVcf.attackMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                robinSource_.synth().setVoiceFilterEnvelopeAttackSeconds(voiceIndex, voice.envVcf.attackMs / 1000.0f);
                return true;
            }
            if (parts[rootOffset + 3] == "decayMs") {
                voice.envVcf.decayMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                robinSource_.synth().setVoiceFilterEnvelopeDecaySeconds(voiceIndex, voice.envVcf.decayMs / 1000.0f);
                return true;
            }
            if (parts[rootOffset + 3] == "sustain") {
                voice.envVcf.sustain = static_cast<float>(std::clamp(value, 0.0, 1.0));
                robinSource_.synth().setVoiceFilterEnvelopeSustainLevel(voiceIndex, voice.envVcf.sustain);
                return true;
            }
            if (parts[rootOffset + 3] == "releaseMs") {
                voice.envVcf.releaseMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                robinSource_.synth().setVoiceFilterEnvelopeReleaseSeconds(voiceIndex, voice.envVcf.releaseMs / 1000.0f);
                return true;
            }
            if (parts[rootOffset + 3] == "amount") {
                voice.envVcf.amount = static_cast<float>(std::clamp(value, 0.0, 1.0));
                robinSource_.synth().setVoiceFilterEnvelopeAmount(voiceIndex, voice.envVcf.amount);
                return true;
            }
        }

        if (parts.size() == rootOffset + 4 && parts[rootOffset + 2] == "envelope") {
            if (voice.linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return false;
            }

            if (parts[rootOffset + 3] == "attackMs") {
                voice.envelope.attackMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                robinSource_.synth().setVoiceEnvelopeAttackSeconds(voiceIndex, voice.envelope.attackMs / 1000.0f);
                return true;
            }
            if (parts[rootOffset + 3] == "decayMs") {
                voice.envelope.decayMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                robinSource_.synth().setVoiceEnvelopeDecaySeconds(voiceIndex, voice.envelope.decayMs / 1000.0f);
                return true;
            }
            if (parts[rootOffset + 3] == "sustain") {
                voice.envelope.sustain = static_cast<float>(std::clamp(value, 0.0, 1.0));
                robinSource_.synth().setVoiceEnvelopeSustainLevel(voiceIndex, voice.envelope.sustain);
                return true;
            }
            if (parts[rootOffset + 3] == "releaseMs") {
                voice.envelope.releaseMs = static_cast<float>(std::clamp(value, 0.0, 5000.0));
                robinSource_.synth().setVoiceEnvelopeReleaseSeconds(voiceIndex, voice.envelope.releaseMs / 1000.0f);
                return true;
            }
        }

        if (parts.size() == rootOffset + 5 && parts[rootOffset + 2] == "oscillator") {
            if (voice.linkedToMaster) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Voice is linked to the master template.";
                }
                return false;
            }

            std::uint32_t oscillatorIndex = 0;
            if (!tryParseIndex(parts[rootOffset + 3], oscillatorIndex)
                || oscillatorIndex >= voice.oscillators.size()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Invalid oscillator index.";
                }
                return false;
            }

            auto& oscillator = voice.oscillators[oscillatorIndex];
            if (parts[rootOffset + 4] == "enabled") {
                oscillator.enabled = value >= 0.5;
                robinSource_.synth().setOscillatorEnabled(voiceIndex, oscillatorIndex, oscillator.enabled);
                return true;
            }

            if (parts[rootOffset + 4] == "gain") {
                oscillator.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
                robinSource_.synth().setOscillatorGain(voiceIndex, oscillatorIndex, oscillator.gain);
                return true;
            }

            if (parts[rootOffset + 4] == "relative") {
                const bool nextRelative = value >= 0.5;
                if (oscillator.relativeToVoice != nextRelative) {
                    if (nextRelative) {
                        oscillator.frequencyValue =
                            static_cast<float>(std::clamp(
                                oscillator.frequencyValue / std::max(1.0f, voice.frequency),
                                0.01f,
                                8.0f));
                    } else {
                        oscillator.frequencyValue =
                            static_cast<float>(std::clamp(
                                oscillator.frequencyValue * std::max(1.0f, voice.frequency),
                                1.0f,
                                20000.0f));
                    }
                }

                oscillator.relativeToVoice = nextRelative;
                robinSource_.synth().setOscillatorRelativeToVoice(
                    voiceIndex,
                    oscillatorIndex,
                    oscillator.relativeToVoice);
                robinSource_.synth().setOscillatorFrequency(voiceIndex, oscillatorIndex, oscillator.frequencyValue);
                return true;
            }

            if (parts[rootOffset + 4] == "frequency") {
                oscillator.frequencyValue = oscillator.relativeToVoice
                    ? static_cast<float>(std::clamp(value, 0.01, 8.0))
                    : static_cast<float>(std::clamp(value, 1.0, 20000.0));
                robinSource_.synth().setOscillatorFrequency(voiceIndex, oscillatorIndex, oscillator.frequencyValue);
                return true;
            }
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

    markStateSnapshotDirty();
    std::unique_lock<std::mutex> lock(mutex_);
    const auto parts = splitPath(path);

    if (path == "engine.outputDeviceId") {
        lock.unlock();
        return applyOutputEngineConfig(std::string(value), std::nullopt, errorMessage);
    }

    if (path == "routingPreset" || path == "sources.robin.routingPreset") {
        RoutingPreset preset;
        if (!tryParseRoutingPreset(value, preset)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid routing preset.";
            }
            return false;
        }

        applyRoutingPresetLocked(preset);
        syncAllVoicesLocked();
        return true;
    }

    if (path == "lfo.waveform" || path == "sources.robin.lfo.waveform") {
        dsp::LfoWaveform waveform;
        if (!tryParseLfoWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid LFO waveform value.";
            }
            return false;
        }

        lfoState_.waveform = waveform;
        robinSource_.synth().setLfoWaveform(lfoState_.waveform);
        return true;
    }

    if (path == "sources.test.waveform") {
        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return false;
        }

        testState_.waveform = waveform;
        testSource_.setWaveform(testState_.waveform);
        return true;
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

    std::size_t rootOffset = 0;
    if (parts.size() >= 3 && parts[0] == "sources" && parts[1] == "robin") {
        rootOffset = 2;
    }

    if (rootOffset == 2
        && parts.size() == 5
        && parts[rootOffset] == "oscillator"
        && parts[rootOffset + 2] == "waveform") {
        std::uint32_t oscillatorIndex = 0;
        if (!tryParseIndex(parts[rootOffset + 1], oscillatorIndex)
            || oscillatorIndex >= masterOscillators_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return false;
        }

        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return false;
        }

        masterOscillators_[oscillatorIndex].waveform = waveform;
        for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
            if (!voices_[voiceIndex].linkedToMaster) {
                continue;
            }
            robinSource_.synth().setOscillatorWaveform(voiceIndex, oscillatorIndex, waveform);
        }
        if (debugRobinOscillatorParams_) {
            logRobinMasterOscillatorUpdateLocked(path, value);
        }
        return true;
    }

    if (parts.size() == rootOffset + 5
        && parts[rootOffset] == "voice"
        && parts[rootOffset + 2] == "oscillator"
        && parts[rootOffset + 4] == "waveform") {
        std::uint32_t voiceIndex = 0;
        std::uint32_t oscillatorIndex = 0;
        if (!tryParseIndex(parts[rootOffset + 1], voiceIndex) || voiceIndex >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return false;
        }
        if (!tryParseIndex(parts[rootOffset + 3], oscillatorIndex)
            || oscillatorIndex >= voices_[voiceIndex].oscillators.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return false;
        }

        if (voices_[voiceIndex].linkedToMaster) {
            if (errorMessage != nullptr) {
                *errorMessage = "Voice is linked to the master template.";
            }
            return false;
        }

        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return false;
        }

        voices_[voiceIndex].oscillators[oscillatorIndex].waveform = waveform;
        robinSource_.synth().setOscillatorWaveform(voiceIndex, oscillatorIndex, waveform);
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unsupported string parameter path.";
    }
    return false;
}

audio::Synth& SynthController::synth() {
    return robinSource_.synth();
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

float SynthController::tunedRobinFrequencyLocked(float baseFrequencyHz) const {
    const float clampedBaseFrequency = std::clamp(baseFrequencyHz, 20.0f, 20000.0f);
    const float semitoneOffset = robinPitchState_.transposeSemitones + (robinPitchState_.fineTuneCents / 100.0f);
    const float tunedFrequency = clampedBaseFrequency * std::pow(2.0f, semitoneOffset / 12.0f);
    return std::clamp(tunedFrequency, 20.0f, 20000.0f);
}

void SynthController::copyMasterStateToVoiceLocked(VoiceState& voice) const {
    voice.frequency = config_.frequency;
    voice.gain = robinMasterVoiceGain_;
    voice.vcf = robinVcfState_;
    voice.envVcf = robinEnvVcfState_;
    voice.envelope = robinEnvelopeState_;
    voice.oscillators = masterOscillators_;
}

void SynthController::syncLinkedRobinVoicesLocked(bool syncFrequency) {
    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (!voices_[voiceIndex].linkedToMaster) {
            continue;
        }
        syncVoiceStateLocked(voiceIndex, syncFrequency);
    }
}

void SynthController::syncRobinVoiceFrequencyLocked(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    const auto& voice = voices_[voiceIndex];
    float baseFrequency = voice.linkedToMaster ? config_.frequency : voice.frequency;

    const auto assignment = std::find_if(
        robinVoiceAssignments_.begin(),
        robinVoiceAssignments_.end(),
        [voiceIndex](const RobinVoiceAssignment& currentAssignment) {
            return currentAssignment.voiceIndex == voiceIndex;
        });
    if (assignment != robinVoiceAssignments_.end()) {
        baseFrequency = midiNoteToFrequency(assignment->noteNumber);
    }

    robinSource_.synth().setVoiceFrequency(voiceIndex, tunedRobinFrequencyLocked(baseFrequency));
}

void SynthController::syncAllRobinVoiceFrequenciesLocked() {
    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        syncRobinVoiceFrequencyLocked(voiceIndex);
    }
}

void SynthController::syncAssignedRobinVoiceFrequenciesLocked() {
    for (const auto& assignment : robinVoiceAssignments_) {
        syncRobinVoiceFrequencyLocked(assignment.voiceIndex);
    }
}

void SynthController::logRobinMasterOscillatorUpdateLocked(std::string_view path,
                                                           std::string_view valueDescription) {
    if (!debugRobinOscillatorParams_) {
        return;
    }

    logger_.debug(
        "Robin debug: "
        + std::string(path)
        + "=" + std::string(valueDescription)
        + " activeMidiNote=" + std::to_string(activeMidiNote_)
        + " heldNotes=" + std::to_string(heldMidiNotes_.size())
        + " activeAssignments=" + std::to_string(robinVoiceAssignments_.size()));
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

    if (parts.size() == 3 && parts[0] == "sourceMixer" && parts[2] == "level") {
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
    } else {
        return RealtimeParamResult::NotHandled;
    }

    markStateSnapshotDirty();
    if (driver_ != nullptr && driver_->isRunning()) {
        enqueueRealtimeCommand(command);
        return RealtimeParamResult::Applied;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    applyRealtimeCommandLocked(command);
    return RealtimeParamResult::Applied;
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
    switch (command.type) {
        case RealtimeCommandType::SourceLevelRobin:
            applyRobinLevelLocked(command.value);
            break;
        case RealtimeCommandType::SourceLevelTest:
            applyTestLevelLocked(command.value);
            break;
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
    }
}

void SynthController::buildLiveGraphLocked() {
    liveGraph_.clear();

    liveGraph_.addSourceNode({
        "robin",
        [this](double sampleRate, std::uint32_t outputChannels) {
            robinSource_.prepare(sampleRate, outputChannels);
        },
        [this]() { return robinMixerState_.implemented; },
        [this]() { return robinMixerState_.enabled; },
        [this]() {
            return robinMixerState_.routeTarget == SourceMixerSlotState::RouteTarget::Fx
                ? graph::LiveGraph::SourceRenderTarget::FxBus
                : graph::LiveGraph::SourceRenderTarget::Dry;
        },
        [this](float* output, std::uint32_t frames, std::uint32_t channels) {
            robinSource_.renderAdd(output, frames, channels);
        },
        [this](int noteNumber, float velocity) { handleRobinNoteOnLocked(noteNumber, velocity); },
        [this](int noteNumber) { handleRobinNoteOffLocked(noteNumber); },
    });

    liveGraph_.addSourceNode({
        "test",
        [this](double sampleRate, std::uint32_t outputChannels) {
            testSource_.prepare(sampleRate, outputChannels);
        },
        [this]() { return testMixerState_.implemented; },
        [this]() { return testMixerState_.enabled; },
        [this]() {
            return testMixerState_.routeTarget == SourceMixerSlotState::RouteTarget::Fx
                ? graph::LiveGraph::SourceRenderTarget::FxBus
                : graph::LiveGraph::SourceRenderTarget::Dry;
        },
        [this](float* output, std::uint32_t frames, std::uint32_t channels) {
            testSource_.renderAdd(output, frames, channels, testMixerState_.enabled, testMixerState_.level);
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
    masterOscillators_.clear();
    masterOscillators_.resize(config_.oscillatorsPerVoice);
    for (std::uint32_t oscillatorIndex = 0; oscillatorIndex < config_.oscillatorsPerVoice; ++oscillatorIndex) {
        auto& oscillator = masterOscillators_[oscillatorIndex];
        oscillator.enabled = (oscillatorIndex == 0);
        oscillator.gain = 1.0f;
        oscillator.relativeToVoice = true;
        oscillator.frequencyValue = 1.0f;
        oscillator.waveform = config_.waveform;
    }

    voices_.clear();
    voices_.resize(config_.voiceCount);
    robinVoiceReleaseUntil_.assign(
        config_.voiceCount,
        std::chrono::steady_clock::time_point::min());

    for (std::uint32_t voiceIndex = 0; voiceIndex < config_.voiceCount; ++voiceIndex) {
        auto& voice = voices_[voiceIndex];
        voice.active = true;
        voice.linkedToMaster = true;
        voice.outputs.assign(config_.channels, false);
        if (!voice.outputs.empty()) {
            voice.outputs[voiceIndex % voice.outputs.size()] = true;
        }
        copyMasterStateToVoiceLocked(voice);
    }

    testState_.implemented = true;
    testState_.playable = true;
    testState_.outputs.assign(std::max<std::uint32_t>(1, config_.channels), false);
    assignDefaultTestOutputs(testState_.outputs);

    robinVoiceAssignments_.clear();
    robinNextVoiceCursor_ = 0;
    autoActivatedVoice0_ = false;
    resetRobinRoutingStateLocked();
}

void SynthController::syncVoiceStateLocked(std::uint32_t voiceIndex, bool syncFrequency) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    const auto& voice = voices_[voiceIndex];
    const auto& envelope = voice.linkedToMaster ? robinEnvelopeState_ : voice.envelope;
    const auto& vcf = voice.linkedToMaster ? robinVcfState_ : voice.vcf;
    const auto& envVcf = voice.linkedToMaster ? robinEnvVcfState_ : voice.envVcf;
    const auto& oscillators = voice.linkedToMaster ? masterOscillators_ : voice.oscillators;

    robinSource_.synth().setVoiceActive(voiceIndex, voice.active);
    if (syncFrequency) {
        syncRobinVoiceFrequencyLocked(voiceIndex);
    }
    robinSource_.synth().setVoiceGain(voiceIndex, voice.linkedToMaster ? robinMasterVoiceGain_ : voice.gain);
    robinSource_.synth().setVoiceEnvelopeAttackSeconds(voiceIndex, envelope.attackMs / 1000.0f);
    robinSource_.synth().setVoiceEnvelopeDecaySeconds(voiceIndex, envelope.decayMs / 1000.0f);
    robinSource_.synth().setVoiceEnvelopeSustainLevel(voiceIndex, envelope.sustain);
    robinSource_.synth().setVoiceEnvelopeReleaseSeconds(voiceIndex, envelope.releaseMs / 1000.0f);
    robinSource_.synth().setVoiceFilterCutoffHz(voiceIndex, vcf.cutoffHz);
    robinSource_.synth().setVoiceFilterResonance(voiceIndex, vcf.resonance);
    robinSource_.synth().setVoiceFilterEnvelopeAttackSeconds(voiceIndex, envVcf.attackMs / 1000.0f);
    robinSource_.synth().setVoiceFilterEnvelopeDecaySeconds(voiceIndex, envVcf.decayMs / 1000.0f);
    robinSource_.synth().setVoiceFilterEnvelopeSustainLevel(voiceIndex, envVcf.sustain);
    robinSource_.synth().setVoiceFilterEnvelopeReleaseSeconds(voiceIndex, envVcf.releaseMs / 1000.0f);
    robinSource_.synth().setVoiceFilterEnvelopeAmount(voiceIndex, envVcf.amount);

    for (std::uint32_t outputIndex = 0; outputIndex < voice.outputs.size(); ++outputIndex) {
        robinSource_.synth().setVoiceOutputEnabled(voiceIndex, outputIndex, voice.outputs[outputIndex]);
    }

    const std::uint32_t oscillatorCount = static_cast<std::uint32_t>(oscillators.size());
    for (std::uint32_t oscillatorIndex = 0; oscillatorIndex < oscillatorCount; ++oscillatorIndex) {
        robinSource_.synth().setOscillatorEnabled(
            voiceIndex,
            oscillatorIndex,
            oscillators[oscillatorIndex].enabled);
        robinSource_.synth().setOscillatorGain(
            voiceIndex,
            oscillatorIndex,
            oscillators[oscillatorIndex].gain);
        robinSource_.synth().setOscillatorRelativeToVoice(
            voiceIndex,
            oscillatorIndex,
            oscillators[oscillatorIndex].relativeToVoice);
        robinSource_.synth().setOscillatorFrequency(
            voiceIndex,
            oscillatorIndex,
            oscillators[oscillatorIndex].frequencyValue);
        robinSource_.synth().setOscillatorWaveform(
            voiceIndex,
            oscillatorIndex,
            oscillators[oscillatorIndex].waveform);
    }
}

void SynthController::syncAllVoicesLocked() {
    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        syncVoiceStateLocked(voiceIndex, true);
    }
}

void SynthController::syncLfoLocked() {
    robinSource_.synth().setLfoEnabled(lfoState_.enabled);
    robinSource_.synth().setLfoWaveform(lfoState_.waveform);
    robinSource_.synth().setLfoDepth(lfoState_.depth);
    robinSource_.synth().setLfoPhaseSpreadDegrees(lfoState_.phaseSpreadDegrees);
    robinSource_.synth().setLfoPolarityFlip(lfoState_.polarityFlip);
    robinSource_.synth().setLfoUnlinkedOutputs(lfoState_.unlinkedOutputs);
    robinSource_.synth().setLfoClockLinked(lfoState_.clockLinked);
    robinSource_.synth().setLfoTempoBpm(lfoState_.tempoBpm);
    robinSource_.synth().setLfoRateMultiplier(lfoState_.rateMultiplier);
    robinSource_.synth().setLfoFixedFrequencyHz(lfoState_.fixedFrequencyHz);
}

void SynthController::syncRobinEnvelopeLocked() {
    syncLinkedRobinVoicesLocked();
}

void SynthController::resizeScaffoldStateLocked() {
    robinMixerState_.available = true;
    robinMixerState_.implemented = true;
    testMixerState_.available = true;
    testMixerState_.implemented = true;
    decorMixerState_.available = true;
    piecesMixerState_.available = true;

    const std::uint32_t outputCount = std::max<std::uint32_t>(1, config_.channels);
    testState_.outputs.resize(outputCount, false);
    if (std::none_of(testState_.outputs.begin(), testState_.outputs.end(), [](bool enabled) { return enabled; })) {
        assignDefaultTestOutputs(testState_.outputs);
    }

    outputMixerChannels_.resize(outputCount);
    outputMixerNode_.resize(outputCount);
    decorState_.voiceCount = outputCount;
    piecesState_.voiceCount = std::max<std::uint32_t>(1, config_.voiceCount);
}

void SynthController::syncTestSourceLocked() {
    testSource_.prepare(config_.sampleRate, config_.channels);
    testSource_.setActive(testState_.active);
    testSource_.setMidiEnabled(testState_.midiEnabled);
    testSource_.setFrequency(testState_.frequency);
    testSource_.setGain(testState_.gain);
    testSource_.setWaveform(testState_.waveform);
    testSource_.setEnvelopeAttackSeconds(testState_.envelope.attackMs / 1000.0f);
    testSource_.setEnvelopeDecaySeconds(testState_.envelope.decayMs / 1000.0f);
    testSource_.setEnvelopeSustainLevel(testState_.envelope.sustain);
    testSource_.setEnvelopeReleaseSeconds(testState_.envelope.releaseMs / 1000.0f);

    for (std::uint32_t outputIndex = 0; outputIndex < testState_.outputs.size(); ++outputIndex) {
        testSource_.setOutputEnabled(outputIndex, testState_.outputs[outputIndex]);
    }
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
    robinSource_.setLevel(robinMixerState_.enabled, robinMixerState_.level);
}

void SynthController::applyTestLevelLocked(float level) {
    testMixerState_.level = std::clamp(level, 0.0f, 1.0f);
}

void SynthController::applyGlobalFrequencyLocked(float frequencyHz) {
    config_.frequency = std::clamp(frequencyHz, 20.0f, 20000.0f);
    syncLinkedRobinVoicesLocked(true);
}

void SynthController::applyGlobalWaveformLocked(dsp::Waveform waveform) {
    config_.waveform = waveform;
    for (auto& oscillator : masterOscillators_) {
        oscillator.waveform = config_.waveform;
    }
    syncLinkedRobinVoicesLocked();
}

void SynthController::applyRoutingPresetLocked(RoutingPreset preset) {
    routingPreset_ = preset;
    resetRobinRoutingStateLocked();
}

void SynthController::resetRobinRoutingStateLocked() {
    const std::uint32_t outputCount = std::max<std::uint32_t>(1, config_.channels);
    robinForwardOutputCursor_ = 0;
    robinBackwardOutputCursor_ = outputCount - 1;
    robinRoundRobinPool_.clear();
    robinNextTriggerOutputIndex_.reset();
}

std::uint32_t SynthController::computeNextRobinTriggerOutputLocked() {
    const std::uint32_t outputCount = std::max<std::uint32_t>(1, config_.channels);
    std::uint32_t outputIndex = 0;

    switch (routingPreset_) {
        case RoutingPreset::Forward:
            outputIndex = robinForwardOutputCursor_ % outputCount;
            robinForwardOutputCursor_ = (outputIndex + 1) % outputCount;
            break;
        case RoutingPreset::Backward:
            outputIndex = robinBackwardOutputCursor_ % outputCount;
            robinBackwardOutputCursor_ = outputIndex == 0 ? (outputCount - 1) : (outputIndex - 1);
            break;
        case RoutingPreset::Random: {
            std::uniform_int_distribution<std::uint32_t> distribution(0, outputCount - 1);
            outputIndex = distribution(robinRoutingRandom_);
            break;
        }
        case RoutingPreset::RoundRobin: {
            if (robinRoundRobinPool_.empty()) {
                robinRoundRobinPool_.reserve(outputCount);
                for (std::uint32_t index = 0; index < outputCount; ++index) {
                    robinRoundRobinPool_.push_back(index);
                }
            }

            std::uniform_int_distribution<std::size_t> distribution(0, robinRoundRobinPool_.size() - 1);
            const std::size_t poolIndex = distribution(robinRoutingRandom_);
            outputIndex = robinRoundRobinPool_[poolIndex];
            robinRoundRobinPool_.erase(robinRoundRobinPool_.begin() + static_cast<std::ptrdiff_t>(poolIndex));
            break;
        }
        case RoutingPreset::AllOutputs:
        case RoutingPreset::Custom:
        default:
            outputIndex = 0;
            break;
    }

    robinNextTriggerOutputIndex_ = outputIndex;
    return outputIndex;
}

void SynthController::routeRobinVoiceToOutputLocked(std::uint32_t voiceIndex, std::uint32_t outputIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    auto& outputs = voices_[voiceIndex].outputs;
    if (outputs.empty()) {
        return;
    }

    outputs.assign(outputs.size(), false);
    outputs[outputIndex % outputs.size()] = true;

    for (std::uint32_t channelIndex = 0; channelIndex < outputs.size(); ++channelIndex) {
        robinSource_.synth().setVoiceOutputEnabled(voiceIndex, channelIndex, outputs[channelIndex]);
    }
}

void SynthController::routeRobinVoiceToAllOutputsLocked(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    auto& outputs = voices_[voiceIndex].outputs;
    if (outputs.empty()) {
        return;
    }

    outputs.assign(outputs.size(), true);

    for (std::uint32_t channelIndex = 0; channelIndex < outputs.size(); ++channelIndex) {
        robinSource_.synth().setVoiceOutputEnabled(voiceIndex, channelIndex, true);
    }
}

std::uint32_t SynthController::allocateRobinVoiceLocked() {
    if (voices_.empty()) {
        return 0;
    }

    std::vector<std::uint32_t> activeVoiceIndices;
    activeVoiceIndices.reserve(voices_.size());
    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (voices_[voiceIndex].active) {
            activeVoiceIndices.push_back(voiceIndex);
        }
    }

    if (activeVoiceIndices.empty()) {
        voices_[0].active = true;
        robinSource_.synth().setVoiceActive(0, true);
        autoActivatedVoice0_ = true;
        activeVoiceIndices.push_back(0);
    }

    const std::uint32_t activeVoiceCount = static_cast<std::uint32_t>(activeVoiceIndices.size());
    const std::uint32_t cursorOffset = robinNextVoiceCursor_ % activeVoiceCount;
    const auto now = std::chrono::steady_clock::now();

    auto isAssigned = [this](std::uint32_t voiceIndex) {
        return std::find_if(
                   robinVoiceAssignments_.begin(),
                   robinVoiceAssignments_.end(),
                   [voiceIndex](const RobinVoiceAssignment& assignment) {
                       return assignment.voiceIndex == voiceIndex;
                   })
            != robinVoiceAssignments_.end();
    };

    auto isReleaseBusy = [this, now](std::uint32_t voiceIndex) {
        return voiceIndex < robinVoiceReleaseUntil_.size()
            && now < robinVoiceReleaseUntil_[voiceIndex];
    };

    auto findCandidateOffset = [&](bool requireUnassigned, bool requireReleaseFree) -> std::optional<std::uint32_t> {
        for (std::uint32_t step = 0; step < activeVoiceCount; ++step) {
            const std::uint32_t candidateOffset = (cursorOffset + step) % activeVoiceCount;
            const std::uint32_t candidateVoiceIndex = activeVoiceIndices[candidateOffset];
            if (requireUnassigned && isAssigned(candidateVoiceIndex)) {
                continue;
            }
            if (requireReleaseFree && isReleaseBusy(candidateVoiceIndex)) {
                continue;
            }
            return candidateOffset;
        }
        return std::nullopt;
    };

    std::optional<std::uint32_t> freeVoiceOffset = findCandidateOffset(true, true);
    if (!freeVoiceOffset.has_value()) {
        freeVoiceOffset = findCandidateOffset(true, false);
    }

    const std::uint32_t selectedOffset = freeVoiceOffset.value_or(cursorOffset);
    const std::uint32_t voiceIndex = activeVoiceIndices[selectedOffset];
    robinNextVoiceCursor_ = (selectedOffset + 1) % activeVoiceCount;

    auto voiceAssignment = std::find_if(
        robinVoiceAssignments_.begin(),
        robinVoiceAssignments_.end(),
        [voiceIndex](const RobinVoiceAssignment& assignment) {
            return assignment.voiceIndex == voiceIndex;
        });
    if (voiceAssignment != robinVoiceAssignments_.end()) {
        robinSource_.synth().noteOff(voiceAssignment->voiceIndex);
        robinVoiceAssignments_.erase(voiceAssignment);
    }

    return voiceIndex;
}

void SynthController::reconfigureStructureLocked(std::uint32_t voiceCount, std::uint32_t oscillatorsPerVoice) {
    const auto previousVoices = voices_;
    const auto previousMasterOscillators = masterOscillators_;

    // Structural changes must drop live Robin note allocation state before the
    // voice/oscillator layout is rebuilt.
    robinSource_.synth().clearNotes();
    robinVoiceAssignments_.clear();
    robinVoiceReleaseUntil_.clear();
    robinNextVoiceCursor_ = 0;
    autoActivatedVoice0_ = false;
    resetRobinRoutingStateLocked();

    config_.voiceCount = std::max<std::uint32_t>(1, voiceCount);
    config_.oscillatorsPerVoice = std::max<std::uint32_t>(1, oscillatorsPerVoice);

    audio::SynthConfig synthConfig;
    synthConfig.voiceCount = config_.voiceCount;
    synthConfig.oscillatorsPerVoice = config_.oscillatorsPerVoice;
    robinSource_.configure(synthConfig);

    buildDefaultStateLocked();
    resizeScaffoldStateLocked();

    const std::size_t masterOscillatorCountToCopy = std::min(previousMasterOscillators.size(), masterOscillators_.size());
    for (std::size_t oscillatorIndex = 0; oscillatorIndex < masterOscillatorCountToCopy; ++oscillatorIndex) {
        masterOscillators_[oscillatorIndex] = previousMasterOscillators[oscillatorIndex];
    }

    const std::size_t voiceCountToCopy = std::min(previousVoices.size(), voices_.size());
    for (std::size_t voiceIndex = 0; voiceIndex < voiceCountToCopy; ++voiceIndex) {
        const auto& previousVoice = previousVoices[voiceIndex];
        auto& voice = voices_[voiceIndex];

        voice.active = previousVoice.active;
        voice.linkedToMaster = previousVoice.linkedToMaster;
        voice.frequency = previousVoice.frequency;
        voice.gain = previousVoice.gain;
        voice.vcf = previousVoice.vcf;
        voice.envVcf = previousVoice.envVcf;
        voice.envelope = previousVoice.envelope;

        const std::size_t outputCountToCopy = std::min(previousVoice.outputs.size(), voice.outputs.size());
        for (std::size_t outputIndex = 0; outputIndex < outputCountToCopy; ++outputIndex) {
            voice.outputs[outputIndex] = previousVoice.outputs[outputIndex];
        }

        const std::size_t oscillatorCountToCopy =
            std::min(previousVoice.oscillators.size(), voice.oscillators.size());
        for (std::size_t oscillatorIndex = 0; oscillatorIndex < oscillatorCountToCopy; ++oscillatorIndex) {
            voice.oscillators[oscillatorIndex] = previousVoice.oscillators[oscillatorIndex];
        }
    }

    if (routingPreset_ != RoutingPreset::Custom) {
        applyRoutingPresetLocked(routingPreset_);
    }

    syncTestSourceLocked();
    syncOutputProcessorNodesLocked();
    applyRobinLevelLocked(robinMixerState_.level);
    applyTestLevelLocked(testMixerState_.level);
    syncRobinEnvelopeLocked();
    syncAllVoicesLocked();
    liveGraph_.prepare(config_.sampleRate, config_.channels);
}

void SynthController::handleNoteOnLocked(int noteNumber, float velocity) {
    heldMidiNotes_.erase(std::remove(heldMidiNotes_.begin(), heldMidiNotes_.end(), noteNumber), heldMidiNotes_.end());
    heldMidiNotes_.push_back(noteNumber);
    activeMidiNote_ = noteNumber;
    liveGraph_.noteOn(noteNumber, velocity);
    markStateSnapshotDirty();
}

void SynthController::handleNoteOffLocked(int noteNumber) {
    const auto newEnd = std::remove(heldMidiNotes_.begin(), heldMidiNotes_.end(), noteNumber);
    if (newEnd == heldMidiNotes_.end()) {
        return;
    }

    heldMidiNotes_.erase(newEnd, heldMidiNotes_.end());
    activeMidiNote_ = heldMidiNotes_.empty() ? -1 : heldMidiNotes_.back();
    liveGraph_.noteOff(noteNumber);
    markStateSnapshotDirty();
}

void SynthController::handleMidiNoteOnLocked(std::uint32_t sourceIndex, int noteNumber, float velocity) {
    heldMidiNotes_.erase(std::remove(heldMidiNotes_.begin(), heldMidiNotes_.end(), noteNumber), heldMidiNotes_.end());
    heldMidiNotes_.push_back(noteNumber);
    activeMidiNote_ = noteNumber;
    markStateSnapshotDirty();

    const MidiSourceRouteState* routeState = findMidiRouteLocked(sourceIndex);
    if (routeState == nullptr) {
        return;
    }

    if (routeState->robin && robinMixerState_.enabled) {
        handleRobinNoteOnLocked(noteNumber, velocity);
    }
    if (routeState->test && testMixerState_.enabled) {
        handleTestNoteOnLocked(noteNumber, velocity);
    }
}

void SynthController::handleMidiNoteOffLocked(std::uint32_t sourceIndex, int noteNumber) {
    const auto newEnd = std::remove(heldMidiNotes_.begin(), heldMidiNotes_.end(), noteNumber);
    if (newEnd == heldMidiNotes_.end()) {
        return;
    }

    heldMidiNotes_.erase(newEnd, heldMidiNotes_.end());
    activeMidiNote_ = heldMidiNotes_.empty() ? -1 : heldMidiNotes_.back();
    markStateSnapshotDirty();

    const MidiSourceRouteState* routeState = findMidiRouteLocked(sourceIndex);
    if (routeState == nullptr) {
        return;
    }

    if (routeState->robin && robinMixerState_.enabled) {
        handleRobinNoteOffLocked(noteNumber);
    }
    if (routeState->test && testMixerState_.enabled) {
        handleTestNoteOffLocked(noteNumber);
    }
}

void SynthController::handleRobinNoteOnLocked(int noteNumber, float /*velocity*/) {
    if (voices_.empty()) {
        return;
    }

    auto existingAssignment = std::find_if(
        robinVoiceAssignments_.begin(),
        robinVoiceAssignments_.end(),
        [noteNumber](const RobinVoiceAssignment& assignment) {
            return assignment.noteNumber == noteNumber;
        });
    if (existingAssignment != robinVoiceAssignments_.end()) {
        robinSource_.synth().noteOff(existingAssignment->voiceIndex);
        if (existingAssignment->voiceIndex < robinVoiceReleaseUntil_.size()) {
            const auto& voice = voices_[existingAssignment->voiceIndex];
            const auto releaseMs = voice.linkedToMaster ? robinEnvelopeState_.releaseMs : voice.envelope.releaseMs;
            robinVoiceReleaseUntil_[existingAssignment->voiceIndex] =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(std::max(0.0f, releaseMs)));
        }
        robinVoiceAssignments_.erase(existingAssignment);
    }

    const std::uint32_t voiceIndex = allocateRobinVoiceLocked();
    if (voiceIndex < robinVoiceReleaseUntil_.size()) {
        robinVoiceReleaseUntil_[voiceIndex] = std::chrono::steady_clock::time_point::min();
    }
    robinVoiceAssignments_.push_back({noteNumber, voiceIndex});
    syncRobinVoiceFrequencyLocked(voiceIndex);

    if (routingPreset_ == RoutingPreset::AllOutputs) {
        routeRobinVoiceToAllOutputsLocked(voiceIndex);
    } else if (routingPreset_ != RoutingPreset::Custom) {
        routeRobinVoiceToOutputLocked(voiceIndex, computeNextRobinTriggerOutputLocked());
    }

    robinSource_.synth().noteOn(voiceIndex);
}

void SynthController::handleRobinNoteOffLocked(int noteNumber) {
    auto assignment = std::find_if(
        robinVoiceAssignments_.begin(),
        robinVoiceAssignments_.end(),
        [noteNumber](const RobinVoiceAssignment& currentAssignment) {
            return currentAssignment.noteNumber == noteNumber;
        });
    if (assignment == robinVoiceAssignments_.end()) {
        return;
    }

    const std::uint32_t voiceIndex = assignment->voiceIndex;
    robinSource_.synth().noteOff(assignment->voiceIndex);
    if (voiceIndex < robinVoiceReleaseUntil_.size()) {
        const auto& voice = voices_[voiceIndex];
        const auto releaseMs = voice.linkedToMaster ? robinEnvelopeState_.releaseMs : voice.envelope.releaseMs;
        robinVoiceReleaseUntil_[voiceIndex] =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(std::max(0.0f, releaseMs)));
    }
    robinVoiceAssignments_.erase(assignment);
    // Keep the released note's pitch during the release tail instead of snapping
    // the voice back to its root frequency while the envelope is still active.
    (void)voiceIndex;

    if (autoActivatedVoice0_ && robinVoiceAssignments_.empty() && !voices_.empty()) {
        voices_[0].active = false;
        robinSource_.synth().setVoiceActive(0, false);
        autoActivatedVoice0_ = false;
        robinNextVoiceCursor_ = 0;
    }
}

void SynthController::handleTestNoteOnLocked(int noteNumber, float velocity) {
    if (!testState_.midiEnabled) {
        return;
    }

    testState_.frequency = midiNoteToFrequency(noteNumber);
    testSource_.noteOn(noteNumber, velocity);
}

void SynthController::handleTestNoteOffLocked(int noteNumber) {
    if (!testState_.midiEnabled) {
        return;
    }

    testSource_.noteOff(noteNumber);
}

}  // namespace synth::app
