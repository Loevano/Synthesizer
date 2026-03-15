#pragma once

#include <cstdint>

namespace synth::dsp {

class Envelope {
public:
    void setSampleRate(double sampleRate);
    void setAttackSeconds(float attackSeconds);
    void setDecaySeconds(float decaySeconds);
    void setSustainLevel(float sustainLevel);
    void setReleaseSeconds(float releaseSeconds);

    void noteOn();
    void noteOff();
    void reset();

    float nextValue();
    bool isActive() const;

private:
    enum class Stage : std::uint8_t {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release,
    };

    std::uint32_t secondsToSamples(float seconds) const;
    void startStage(Stage stage, float targetValue, float durationSeconds);
    void completeCurrentStage();

    double sampleRate_ = 48000.0;
    float attackSeconds_ = 0.01f;
    float decaySeconds_ = 0.08f;
    float sustainLevel_ = 0.8f;
    float releaseSeconds_ = 0.2f;
    float value_ = 0.0f;
    float stageIncrement_ = 0.0f;
    float stageTarget_ = 0.0f;
    std::uint32_t samplesRemaining_ = 0;
    Stage stage_ = Stage::Idle;
};

}  // namespace synth::dsp
