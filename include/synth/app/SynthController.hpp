#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "synth/audio/Synth.hpp"
#include "synth/core/Logger.hpp"
#include "synth/interfaces/IAudioDriver.hpp"

namespace synth::io {
class MidiInput;
class OscServer;
}

namespace synth::app {

struct RuntimeConfig {
    double sampleRate = 48000.0;
    std::uint32_t channels = 2;
    std::uint32_t framesPerBuffer = 256;
    std::uint32_t voiceCount = 16;
    std::uint32_t oscillatorsPerVoice = 6;
    float frequency = 400.0f;
    float gain = 0.15f;
    dsp::Waveform waveform = dsp::Waveform::Sine;
    std::filesystem::path logDirectory = "logs";
};

struct OscillatorState {
    bool enabled = false;
    float gain = 1.0f;
    bool relativeToVoice = true;
    float frequencyValue = 1.0f;
    dsp::Waveform waveform = dsp::Waveform::Sine;
};

struct VoiceState {
    bool active = false;
    float frequency = 400.0f;
    float gain = 1.0f;
    std::vector<bool> outputs;
    std::vector<OscillatorState> oscillators;
};

struct LfoState {
    bool enabled = false;
    float depth = 0.5f;
    float phaseSpreadDegrees = 0.0f;
    bool polarityFlip = false;
    bool unlinkedOutputs = false;
    bool clockLinked = false;
    float tempoBpm = 120.0f;
    float rateMultiplier = 1.0f;
    float fixedFrequencyHz = 2.0f;
    dsp::LfoWaveform waveform = dsp::LfoWaveform::Sine;
};

enum class RoutingPreset {
    RoundRobin,
    FirstOutput,
    AllOutputs,
    Custom,
};

class SynthController {
public:
    explicit SynthController(RuntimeConfig config = {});
    ~SynthController();

    bool initialize();
    bool startAudio();
    void stopAudio();
    bool isRunning() const;
    std::string stateJson() const;
    bool setParam(std::string_view path, double value, std::string* errorMessage);
    bool setParam(std::string_view path, std::string_view value, std::string* errorMessage);

    audio::Synth& synth();
    core::Logger& logger();
    const RuntimeConfig& config() const;

private:
    static const char* waveformToString(dsp::Waveform waveform);
    static bool tryParseWaveform(std::string_view value, dsp::Waveform& waveform);
    static const char* lfoWaveformToString(dsp::LfoWaveform waveform);
    static bool tryParseLfoWaveform(std::string_view value, dsp::LfoWaveform& waveform);
    static const char* routingPresetToString(RoutingPreset preset);
    static bool tryParseRoutingPreset(std::string_view value, RoutingPreset& preset);
    static std::string escapeJson(std::string_view value);
    static bool tryParseIndex(std::string_view value, std::uint32_t& index);
    static float midiNoteToFrequency(int noteNumber);

    void buildDefaultStateLocked();
    void syncVoiceStateLocked(std::uint32_t voiceIndex);
    void syncAllVoicesLocked();
    void syncLfoLocked();
    void applyGlobalFrequencyLocked(float frequencyHz);
    void applyGlobalWaveformLocked(dsp::Waveform waveform);
    void applyRoutingPresetLocked(RoutingPreset preset);
    void reconfigureStructureLocked(std::uint32_t voiceCount, std::uint32_t oscillatorsPerVoice);
    void handleNoteOnLocked(int noteNumber, float velocity);
    void handleNoteOffLocked(int noteNumber);

    RuntimeConfig config_;
    core::Logger logger_;
    std::unique_ptr<interfaces::IAudioDriver> driver_;
    std::unique_ptr<io::MidiInput> midiInput_;
    std::unique_ptr<io::OscServer> oscServer_;
    audio::Synth synth_;
    std::vector<VoiceState> voices_;
    LfoState lfoState_;
    RoutingPreset routingPreset_ = RoutingPreset::RoundRobin;
    std::string audioBackendName_ = "Unknown";
    std::string outputDeviceName_ = "Unknown";
    std::uint16_t oscPort_ = 9000;
    bool midiEnabled_ = false;
    bool oscEnabled_ = false;
    int activeMidiNote_ = -1;
    bool autoActivatedVoice0_ = false;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

}  // namespace synth::app
