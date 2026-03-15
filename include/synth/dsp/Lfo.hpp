#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace synth::dsp {

enum class LfoWaveform {
    Sine,
    Triangle,
    SawDown,
    SawUp,
    Random,
};

class Lfo {
public:
    void setSampleRate(double sampleRate);
    void setOutputChannelCount(std::uint32_t outputChannelCount);
    void setEnabled(bool enabled);
    void setWaveform(LfoWaveform waveform);
    void setDepth(float depth);
    void setPhaseSpreadDegrees(float phaseSpreadDegrees);
    void setPolarityFlip(bool polarityFlip);
    void setUnlinkedOutputs(bool unlinkedOutputs);
    void setClockLinked(bool clockLinked);
    void setTempoBpm(float tempoBpm);
    void setRateMultiplier(float rateMultiplier);
    void setFixedFrequencyHz(float frequencyHz);

    void renderFrame(float* outputMultipliers, std::uint32_t channels);

private:
    void resizeOutputs(std::uint32_t outputChannelCount);
    void reseedPhases();
    float evaluateWaveform(std::uint32_t channel);
    float currentRateHz() const;

    double sampleRate_ = 48000.0;
    bool enabled_ = false;
    LfoWaveform waveform_ = LfoWaveform::Sine;
    float depth_ = 0.0f;
    float phaseSpreadDegrees_ = 0.0f;
    bool polarityFlip_ = false;
    bool unlinkedOutputs_ = false;
    bool clockLinked_ = false;
    float tempoBpm_ = 120.0f;
    float rateMultiplier_ = 1.0f;
    float fixedFrequencyHz_ = 2.0f;
    std::vector<float> phases_;
    std::vector<float> randomValues_;
    std::minstd_rand randomGenerator_{std::random_device{}()};
};

}  // namespace synth::dsp
