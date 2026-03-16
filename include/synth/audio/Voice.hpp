#pragma once

#include <cstdint>
#include <vector>

#include "synth/dsp/Envelope.hpp"
#include "synth/dsp/LowPassFilter.hpp"
#include "synth/dsp/Oscillator.hpp"

namespace synth::audio {

struct OscillatorSlot {
    dsp::Oscillator oscillator;
    bool enabled = false;
    float gain = 1.0f;
    bool relativeToVoice = true;
    float frequencyValue = 1.0f;
};

class Voice {
public:
    void configure(std::uint32_t oscillatorCount);
    void setSampleRate(double sampleRate);
    void setFrequency(float frequencyHz);
    void setWaveform(dsp::Waveform waveform);
    void setOscillatorEnabled(std::uint32_t oscillatorIndex, bool enabled);
    void setOscillatorGain(std::uint32_t oscillatorIndex, float gain);
    void setOscillatorFrequency(std::uint32_t oscillatorIndex, float frequencyValue);
    void setOscillatorRelativeToVoice(std::uint32_t oscillatorIndex, bool relativeToVoice);
    void setOscillatorWaveform(std::uint32_t oscillatorIndex, dsp::Waveform waveform);
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
    void setGain(float gain);
    void setOutputChannelCount(std::uint32_t outputChannelCount);
    void setOutputEnabled(std::uint32_t outputChannel, bool enabled);
    void setActive(bool active);
    void noteOn();
    void noteOff();
    void clearNote();

    void renderAdd(float* output,
                   std::uint32_t frames,
                   std::uint32_t channels,
                   float masterGain,
                   const float* outputModulation);

private:
    void updateOscillatorFrequency(OscillatorSlot& slot);

    std::vector<OscillatorSlot> oscillators_;
    std::vector<bool> outputEnabled_;
    dsp::Envelope envelope_;
    dsp::Envelope filterEnvelope_;
    dsp::LowPassFilter filter_;
    double sampleRate_ = 48000.0;
    float frequencyHz_ = 440.0f;
    dsp::Waveform waveform_ = dsp::Waveform::Sine;
    bool active_ = false;
    bool pendingDeactivate_ = false;
    float gain_ = 1.0f;
    float filterCutoffHz_ = 18000.0f;
    float filterResonance_ = 0.707f;
    float filterEnvelopeAmount_ = 0.0f;
};

}  // namespace synth::audio
