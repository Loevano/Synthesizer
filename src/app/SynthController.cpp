#include "synth/app/SynthController.hpp"

#include "synth/io/MidiInput.hpp"
#include "synth/io/OscServer.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
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

SynthController::SynthController(RuntimeConfig config)
    : config_(std::move(config)),
      logger_(config_.logDirectory) {}

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

    audioBackendName_ = "CoreAudio";
#if defined(SYNTH_PLATFORM_MACOS)
    outputDeviceName_ = queryDefaultOutputDeviceName();
#else
    outputDeviceName_ = "Unavailable";
#endif

    driver_ = interfaces::createAudioDriver(logger_);
    if (!driver_) {
        logger_.error("Could not create audio driver.");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);

        audio::SynthConfig synthConfig;
        synthConfig.voiceCount = config_.voiceCount;
        synthConfig.oscillatorsPerVoice = config_.oscillatorsPerVoice;
        synth_.configure(synthConfig);
        synth_.setSampleRate(config_.sampleRate);
        synth_.setOutputChannelCount(config_.channels);
        synth_.setGain(config_.gain);

        buildDefaultStateLocked();
        applyGlobalFrequencyLocked(config_.frequency);
        applyGlobalWaveformLocked(config_.waveform);
        applyRoutingPresetLocked(routingPreset_);
        syncLfoLocked();
        syncAllVoicesLocked();
    }

    initialized_ = true;
    return true;
}

bool SynthController::startAudio() {
    if (!initialize()) {
        return false;
    }

    if (driver_->isRunning()) {
        return true;
    }

    interfaces::AudioConfig audioConfig;
    audioConfig.sampleRate = config_.sampleRate;
    audioConfig.channels = config_.channels;
    audioConfig.framesPerBuffer = config_.framesPerBuffer;

    logger_.info("Starting audio...");
    const bool started = driver_->start(
        audioConfig, [&](float* output, std::uint32_t frames, std::uint32_t channels) {
            std::lock_guard<std::mutex> lock(mutex_);
            synth_.render(output, frames, channels);
        });

    if (!started) {
        logger_.error("Audio failed to start.");
        midiEnabled_ = false;
        oscEnabled_ = false;
        return false;
    }

    if (midiInput_ == nullptr) {
        midiInput_ = std::make_unique<io::MidiInput>(logger_);
    }
    if (!midiInput_->isRunning()) {
        midiEnabled_ = midiInput_->start([this](int noteNumber, float velocity) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (velocity > 0.0f) {
                handleNoteOnLocked(noteNumber, velocity);
            } else {
                handleNoteOffLocked(noteNumber);
            }
        });
    } else {
        midiEnabled_ = true;
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

    return true;
}

void SynthController::stopAudio() {
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
}

bool SynthController::isRunning() const {
    return driver_ != nullptr && driver_->isRunning();
}

std::string SynthController::stateJson() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream json;
    json << "{"
         << "\"running\":" << (isRunning() ? "true" : "false") << ","
         << "\"audioBackend\":\"" << escapeJson(audioBackendName_) << "\","
         << "\"outputDeviceName\":\"" << escapeJson(outputDeviceName_) << "\","
         << "\"midiEnabled\":" << (midiEnabled_ ? "true" : "false") << ","
         << "\"midiSourceCount\":" << (midiInput_ != nullptr ? midiInput_->sourceCount() : 0) << ","
         << "\"oscEnabled\":" << (oscEnabled_ ? "true" : "false") << ","
         << "\"oscPort\":" << oscPort_ << ","
         << "\"activeMidiNote\":" << activeMidiNote_ << ","
         << "\"sampleRate\":" << config_.sampleRate << ","
         << "\"channels\":" << config_.channels << ","
         << "\"framesPerBuffer\":" << config_.framesPerBuffer << ","
         << "\"voiceCount\":" << config_.voiceCount << ","
         << "\"oscillatorsPerVoice\":" << config_.oscillatorsPerVoice << ","
         << "\"frequency\":" << config_.frequency << ","
         << "\"gain\":" << config_.gain << ","
         << "\"waveform\":\"" << escapeJson(waveformToString(config_.waveform)) << "\","
         << "\"routingPreset\":\"" << escapeJson(routingPresetToString(routingPreset_)) << "\","
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
         << "\"voices\":[";

    for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        if (voiceIndex > 0) {
            json << ",";
        }

        const auto& voice = voices_[voiceIndex];
        json << "{"
             << "\"index\":" << voiceIndex << ","
             << "\"active\":" << (voice.active ? "true" : "false") << ","
             << "\"frequency\":" << voice.frequency << ","
             << "\"gain\":" << voice.gain << ","
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

    json << "]}";
    return json.str();
}

bool SynthController::setParam(std::string_view path, double value, std::string* errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (path == "voiceCount") {
        reconfigureStructureLocked(static_cast<std::uint32_t>(std::clamp(value, 1.0, 32.0)),
                                   config_.oscillatorsPerVoice);
        return true;
    }

    if (path == "oscillatorsPerVoice") {
        reconfigureStructureLocked(config_.voiceCount,
                                   static_cast<std::uint32_t>(std::clamp(value, 1.0, 8.0)));
        return true;
    }

    if (path == "frequency") {
        applyGlobalFrequencyLocked(static_cast<float>(std::clamp(value, 20.0, 20000.0)));
        return true;
    }

    if (path == "gain") {
        config_.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
        synth_.setGain(config_.gain);
        return true;
    }

    if (path == "lfo.enabled") {
        lfoState_.enabled = value >= 0.5;
        synth_.setLfoEnabled(lfoState_.enabled);
        return true;
    }

    if (path == "lfo.depth") {
        lfoState_.depth = static_cast<float>(std::clamp(value, 0.0, 1.0));
        synth_.setLfoDepth(lfoState_.depth);
        return true;
    }

    if (path == "lfo.phaseSpreadDegrees") {
        lfoState_.phaseSpreadDegrees = static_cast<float>(std::clamp(value, 0.0, 360.0));
        synth_.setLfoPhaseSpreadDegrees(lfoState_.phaseSpreadDegrees);
        return true;
    }

    if (path == "lfo.polarityFlip") {
        lfoState_.polarityFlip = value >= 0.5;
        synth_.setLfoPolarityFlip(lfoState_.polarityFlip);
        return true;
    }

    if (path == "lfo.unlinkedOutputs") {
        lfoState_.unlinkedOutputs = value >= 0.5;
        synth_.setLfoUnlinkedOutputs(lfoState_.unlinkedOutputs);
        return true;
    }

    if (path == "lfo.clockLinked") {
        lfoState_.clockLinked = value >= 0.5;
        synth_.setLfoClockLinked(lfoState_.clockLinked);
        return true;
    }

    if (path == "lfo.tempoBpm") {
        lfoState_.tempoBpm = static_cast<float>(std::clamp(value, 20.0, 300.0));
        synth_.setLfoTempoBpm(lfoState_.tempoBpm);
        return true;
    }

    if (path == "lfo.rateMultiplier") {
        lfoState_.rateMultiplier = static_cast<float>(std::clamp(value, 0.125, 8.0));
        synth_.setLfoRateMultiplier(lfoState_.rateMultiplier);
        return true;
    }

    if (path == "lfo.fixedFrequencyHz") {
        lfoState_.fixedFrequencyHz = static_cast<float>(std::clamp(value, 0.01, 40.0));
        synth_.setLfoFixedFrequencyHz(lfoState_.fixedFrequencyHz);
        return true;
    }

    const auto parts = splitPath(path);
    if (parts.size() < 3 || parts[0] != "voice") {
        if (errorMessage != nullptr) {
            *errorMessage = "Unsupported numeric parameter path.";
        }
        return false;
    }

    std::uint32_t voiceIndex = 0;
    if (!tryParseIndex(parts[1], voiceIndex) || voiceIndex >= voices_.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid voice index.";
        }
        return false;
    }

    auto& voice = voices_[voiceIndex];

    if (parts.size() == 3 && parts[2] == "active") {
        voice.active = value >= 0.5;
        synth_.setVoiceActive(voiceIndex, voice.active);
        return true;
    }

    if (parts.size() == 3 && parts[2] == "frequency") {
        voice.frequency = static_cast<float>(std::clamp(value, 20.0, 20000.0));
        synth_.setVoiceFrequency(voiceIndex, voice.frequency);
        return true;
    }

    if (parts.size() == 3 && parts[2] == "gain") {
        voice.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
        synth_.setVoiceGain(voiceIndex, voice.gain);
        return true;
    }

    if (parts.size() == 4 && parts[2] == "output") {
        std::uint32_t outputIndex = 0;
        if (!tryParseIndex(parts[3], outputIndex) || outputIndex >= voice.outputs.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid output index.";
            }
            return false;
        }

        voice.outputs[outputIndex] = value >= 0.5;
        routingPreset_ = RoutingPreset::Custom;
        synth_.setVoiceOutputEnabled(voiceIndex, outputIndex, voice.outputs[outputIndex]);
        return true;
    }

    if (parts.size() == 5 && parts[2] == "oscillator") {
        std::uint32_t oscillatorIndex = 0;
        if (!tryParseIndex(parts[3], oscillatorIndex) || oscillatorIndex >= voice.oscillators.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid oscillator index.";
            }
            return false;
        }

        auto& oscillator = voice.oscillators[oscillatorIndex];
        if (parts[4] == "enabled") {
            oscillator.enabled = value >= 0.5;
            synth_.setOscillatorEnabled(voiceIndex, oscillatorIndex, oscillator.enabled);
            return true;
        }

        if (parts[4] == "gain") {
            oscillator.gain = static_cast<float>(std::clamp(value, 0.0, 1.0));
            synth_.setOscillatorGain(voiceIndex, oscillatorIndex, oscillator.gain);
            return true;
        }

        if (parts[4] == "relative") {
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
            synth_.setOscillatorRelativeToVoice(voiceIndex, oscillatorIndex, oscillator.relativeToVoice);
            synth_.setOscillatorFrequency(voiceIndex, oscillatorIndex, oscillator.frequencyValue);
            return true;
        }

        if (parts[4] == "frequency") {
            oscillator.frequencyValue = oscillator.relativeToVoice
                ? static_cast<float>(std::clamp(value, 0.01, 8.0))
                : static_cast<float>(std::clamp(value, 1.0, 20000.0));
            synth_.setOscillatorFrequency(voiceIndex, oscillatorIndex, oscillator.frequencyValue);
            return true;
        }
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unsupported numeric parameter path.";
    }
    return false;
}

bool SynthController::setParam(std::string_view path, std::string_view value, std::string* errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (path == "routingPreset") {
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

    if (path == "waveform") {
        dsp::Waveform waveform;
        if (!tryParseWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid waveform value.";
            }
            return false;
        }

        applyGlobalWaveformLocked(waveform);
        return true;
    }

    if (path == "lfo.waveform") {
        dsp::LfoWaveform waveform;
        if (!tryParseLfoWaveform(value, waveform)) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid LFO waveform value.";
            }
            return false;
        }

        lfoState_.waveform = waveform;
        synth_.setLfoWaveform(lfoState_.waveform);
        return true;
    }

    const auto parts = splitPath(path);
    if (parts.size() == 5 && parts[0] == "voice" && parts[2] == "oscillator" && parts[4] == "waveform") {
        std::uint32_t voiceIndex = 0;
        std::uint32_t oscillatorIndex = 0;
        if (!tryParseIndex(parts[1], voiceIndex) || voiceIndex >= voices_.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "Invalid voice index.";
            }
            return false;
        }
        if (!tryParseIndex(parts[3], oscillatorIndex) || oscillatorIndex >= voices_[voiceIndex].oscillators.size()) {
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

        voices_[voiceIndex].oscillators[oscillatorIndex].waveform = waveform;
        synth_.setOscillatorWaveform(voiceIndex, oscillatorIndex, waveform);
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "Unsupported string parameter path.";
    }
    return false;
}

audio::Synth& SynthController::synth() {
    return synth_;
}

core::Logger& SynthController::logger() {
    return logger_;
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

const char* SynthController::routingPresetToString(RoutingPreset preset) {
    switch (preset) {
        case RoutingPreset::FirstOutput:
            return "first-output";
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
    if (value == "round-robin") {
        preset = RoutingPreset::RoundRobin;
        return true;
    }
    if (value == "first-output") {
        preset = RoutingPreset::FirstOutput;
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

void SynthController::buildDefaultStateLocked() {
    voices_.clear();
    voices_.resize(config_.voiceCount);

    for (std::uint32_t voiceIndex = 0; voiceIndex < config_.voiceCount; ++voiceIndex) {
        auto& voice = voices_[voiceIndex];
        voice.active = (voiceIndex == 0);
        voice.frequency = config_.frequency;
        voice.gain = 1.0f;
        voice.outputs.assign(config_.channels, false);
        if (!voice.outputs.empty()) {
            voice.outputs[voiceIndex % voice.outputs.size()] = true;
        }

        voice.oscillators.clear();
        voice.oscillators.resize(config_.oscillatorsPerVoice);
        for (std::uint32_t oscillatorIndex = 0; oscillatorIndex < config_.oscillatorsPerVoice; ++oscillatorIndex) {
            auto& oscillator = voice.oscillators[oscillatorIndex];
            oscillator.enabled = (oscillatorIndex == 0);
            oscillator.gain = 1.0f;
            oscillator.relativeToVoice = true;
            oscillator.frequencyValue = 1.0f;
            oscillator.waveform = config_.waveform;
        }
    }
}

void SynthController::syncVoiceStateLocked(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    const auto& voice = voices_[voiceIndex];
    synth_.setVoiceActive(voiceIndex, voice.active);
    synth_.setVoiceFrequency(voiceIndex, voice.frequency);
    synth_.setVoiceGain(voiceIndex, voice.gain);

    for (std::uint32_t outputIndex = 0; outputIndex < voice.outputs.size(); ++outputIndex) {
        synth_.setVoiceOutputEnabled(voiceIndex, outputIndex, voice.outputs[outputIndex]);
    }

    for (std::uint32_t oscillatorIndex = 0; oscillatorIndex < voice.oscillators.size(); ++oscillatorIndex) {
        synth_.setOscillatorEnabled(voiceIndex, oscillatorIndex, voice.oscillators[oscillatorIndex].enabled);
        synth_.setOscillatorGain(voiceIndex, oscillatorIndex, voice.oscillators[oscillatorIndex].gain);
        synth_.setOscillatorRelativeToVoice(
            voiceIndex,
            oscillatorIndex,
            voice.oscillators[oscillatorIndex].relativeToVoice);
        synth_.setOscillatorFrequency(
            voiceIndex,
            oscillatorIndex,
            voice.oscillators[oscillatorIndex].frequencyValue);
        synth_.setOscillatorWaveform(voiceIndex, oscillatorIndex, voice.oscillators[oscillatorIndex].waveform);
    }
}

void SynthController::syncAllVoicesLocked() {
    for (std::uint32_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        syncVoiceStateLocked(voiceIndex);
    }
}

void SynthController::syncLfoLocked() {
    synth_.setLfoEnabled(lfoState_.enabled);
    synth_.setLfoWaveform(lfoState_.waveform);
    synth_.setLfoDepth(lfoState_.depth);
    synth_.setLfoPhaseSpreadDegrees(lfoState_.phaseSpreadDegrees);
    synth_.setLfoPolarityFlip(lfoState_.polarityFlip);
    synth_.setLfoUnlinkedOutputs(lfoState_.unlinkedOutputs);
    synth_.setLfoClockLinked(lfoState_.clockLinked);
    synth_.setLfoTempoBpm(lfoState_.tempoBpm);
    synth_.setLfoRateMultiplier(lfoState_.rateMultiplier);
    synth_.setLfoFixedFrequencyHz(lfoState_.fixedFrequencyHz);
}

void SynthController::applyGlobalFrequencyLocked(float frequencyHz) {
    config_.frequency = std::clamp(frequencyHz, 20.0f, 20000.0f);
    for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        voices_[voiceIndex].frequency = config_.frequency;
        synth_.setVoiceFrequency(static_cast<std::uint32_t>(voiceIndex), config_.frequency);
    }
}

void SynthController::applyGlobalWaveformLocked(dsp::Waveform waveform) {
    config_.waveform = waveform;
    for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        for (std::size_t oscillatorIndex = 0; oscillatorIndex < voices_[voiceIndex].oscillators.size(); ++oscillatorIndex) {
            voices_[voiceIndex].oscillators[oscillatorIndex].waveform = config_.waveform;
            synth_.setOscillatorWaveform(
                static_cast<std::uint32_t>(voiceIndex),
                static_cast<std::uint32_t>(oscillatorIndex),
                config_.waveform);
        }
    }
}

void SynthController::applyRoutingPresetLocked(RoutingPreset preset) {
    routingPreset_ = preset;
    if (preset == RoutingPreset::Custom) {
        return;
    }

    for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
        auto& outputs = voices_[voiceIndex].outputs;
        outputs.assign(config_.channels, false);
        if (outputs.empty()) {
            continue;
        }

        switch (preset) {
            case RoutingPreset::FirstOutput:
                outputs[0] = true;
                break;
            case RoutingPreset::AllOutputs:
                std::fill(outputs.begin(), outputs.end(), true);
                break;
            case RoutingPreset::RoundRobin:
                outputs[voiceIndex % outputs.size()] = true;
                break;
            case RoutingPreset::Custom:
            default:
                break;
        }
    }
}

void SynthController::reconfigureStructureLocked(std::uint32_t voiceCount, std::uint32_t oscillatorsPerVoice) {
    const auto previousVoices = voices_;

    config_.voiceCount = std::max<std::uint32_t>(1, voiceCount);
    config_.oscillatorsPerVoice = std::max<std::uint32_t>(1, oscillatorsPerVoice);

    audio::SynthConfig synthConfig;
    synthConfig.voiceCount = config_.voiceCount;
    synthConfig.oscillatorsPerVoice = config_.oscillatorsPerVoice;
    synth_.configure(synthConfig);
    synth_.setSampleRate(config_.sampleRate);
    synth_.setOutputChannelCount(config_.channels);
    synth_.setGain(config_.gain);

    buildDefaultStateLocked();

    const std::size_t voiceCountToCopy = std::min(previousVoices.size(), voices_.size());
    for (std::size_t voiceIndex = 0; voiceIndex < voiceCountToCopy; ++voiceIndex) {
        const auto& previousVoice = previousVoices[voiceIndex];
        auto& voice = voices_[voiceIndex];

        voice.active = previousVoice.active;
        voice.frequency = previousVoice.frequency;
        voice.gain = previousVoice.gain;

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

    syncAllVoicesLocked();
}

void SynthController::handleNoteOnLocked(int noteNumber, float /*velocity*/) {
    activeMidiNote_ = noteNumber;

    const bool hasActiveVoice = std::any_of(
        voices_.begin(),
        voices_.end(),
        [](const VoiceState& voice) { return voice.active; });

    if (!hasActiveVoice && !voices_.empty()) {
        voices_[0].active = true;
        synth_.setVoiceActive(0, true);
        autoActivatedVoice0_ = true;
    } else {
        autoActivatedVoice0_ = false;
    }

    applyGlobalFrequencyLocked(midiNoteToFrequency(noteNumber));
}

void SynthController::handleNoteOffLocked(int noteNumber) {
    if (noteNumber != activeMidiNote_) {
        return;
    }

    activeMidiNote_ = -1;
    if (autoActivatedVoice0_ && !voices_.empty()) {
        voices_[0].active = false;
        synth_.setVoiceActive(0, false);
        autoActivatedVoice0_ = false;
    }
}

}  // namespace synth::app
