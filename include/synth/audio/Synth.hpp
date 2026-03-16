#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "synth/dsp/Lfo.hpp"
#include "synth/audio/Voice.hpp"

namespace synth::audio {

struct SynthConfig {
    std::uint32_t voiceCount = 8;
    std::uint32_t oscillatorsPerVoice = 6;
};

// Synth is the top-level instrument object. It owns a pool of voices, and each
// voice owns a pool of oscillator slots.
class Synth {
public:
    Synth();

    void configure(const SynthConfig& config);
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setGain(float gain);
    void setWaveform(dsp::Waveform waveform);
    void setVoiceActive(std::uint32_t voiceIndex, bool active);
    void setVoiceFrequency(std::uint32_t voiceIndex, float frequencyHz);
    void setVoiceGain(std::uint32_t voiceIndex, float gain);
    void setVoiceEnvelopeAttackSeconds(std::uint32_t voiceIndex, float attackSeconds);
    void setVoiceEnvelopeDecaySeconds(std::uint32_t voiceIndex, float decaySeconds);
    void setVoiceEnvelopeSustainLevel(std::uint32_t voiceIndex, float sustainLevel);
    void setVoiceEnvelopeReleaseSeconds(std::uint32_t voiceIndex, float releaseSeconds);
    void setVoiceFilterCutoffHz(std::uint32_t voiceIndex, float cutoffHz);
    void setVoiceFilterResonance(std::uint32_t voiceIndex, float resonance);
    void setVoiceFilterEnvelopeAttackSeconds(std::uint32_t voiceIndex, float attackSeconds);
    void setVoiceFilterEnvelopeDecaySeconds(std::uint32_t voiceIndex, float decaySeconds);
    void setVoiceFilterEnvelopeSustainLevel(std::uint32_t voiceIndex, float sustainLevel);
    void setVoiceFilterEnvelopeReleaseSeconds(std::uint32_t voiceIndex, float releaseSeconds);
    void setVoiceFilterEnvelopeAmount(std::uint32_t voiceIndex, float amount);
    void setOutputChannelCount(std::uint32_t outputChannelCount);
    void setVoiceOutputEnabled(std::uint32_t voiceIndex, std::uint32_t outputChannel, bool enabled);
    void setOscillatorEnabled(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, bool enabled);
    void setOscillatorGain(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, float gain);
    void setOscillatorFrequency(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, float frequencyValue);
    void setOscillatorRelativeToVoice(std::uint32_t voiceIndex,
                                      std::uint32_t oscillatorIndex,
                                      bool relativeToVoice);
    void setOscillatorWaveform(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, dsp::Waveform waveform);
    void setEnvelopeAttackSeconds(float attackSeconds);
    void setEnvelopeDecaySeconds(float decaySeconds);
    void setEnvelopeSustainLevel(float sustainLevel);
    void setEnvelopeReleaseSeconds(float releaseSeconds);
    void setFilterCutoffHz(float cutoffHz);
    void setFilterResonance(float resonance);
    void setFilterEnvelopeAttackSeconds(float attackSeconds);
    void setFilterEnvelopeDecaySeconds(float decaySeconds);
    void setFilterEnvelopeSustainLevel(float sustainLevel);
    void setFilterEnvelopeReleaseSeconds(float releaseSeconds);
    void setFilterEnvelopeAmount(float amount);
    void setLfoEnabled(bool enabled);
    void setLfoWaveform(dsp::LfoWaveform waveform);
    void setLfoDepth(float depth);
    void setLfoPhaseSpreadDegrees(float phaseSpreadDegrees);
    void setLfoPolarityFlip(bool polarityFlip);
    void setLfoUnlinkedOutputs(bool unlinkedOutputs);
    void setLfoClockLinked(bool clockLinked);
    void setLfoTempoBpm(float tempoBpm);
    void setLfoRateMultiplier(float rateMultiplier);
    void setLfoFixedFrequencyHz(float frequencyHz);

    void render(float* output, std::uint32_t frames, std::uint32_t channels);
    void renderAdd(float* output, std::uint32_t frames, std::uint32_t channels);
    void noteOn(std::uint32_t voiceIndex);
    void noteOff(std::uint32_t voiceIndex);
    void clearNotes();

private:
    std::vector<Voice> voices_;

    std::atomic<float> frequencyHz_{440.0f};
    std::atomic<float> gain_{0.2f};
    double sampleRate_ = 48000.0;
    std::uint32_t outputChannelCount_ = 2;
    dsp::Waveform waveform_ = dsp::Waveform::Sine;
    std::uint32_t oscillatorsPerVoice_ = 6;
    float attackSeconds_ = 0.01f;
    float decaySeconds_ = 0.08f;
    float sustainLevel_ = 0.8f;
    float releaseSeconds_ = 0.2f;
    dsp::Lfo lfo_;
    std::vector<float> lfoModulationBuffer_;
};

}  // namespace synth::audio
