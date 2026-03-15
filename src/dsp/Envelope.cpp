#include "synth/dsp/Envelope.hpp"

#include <algorithm>

namespace synth::dsp {

void Envelope::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
}

void Envelope::setAttackSeconds(float attackSeconds) {
    attackSeconds_ = std::max(0.0f, attackSeconds);
}

void Envelope::setDecaySeconds(float decaySeconds) {
    decaySeconds_ = std::max(0.0f, decaySeconds);
}

void Envelope::setSustainLevel(float sustainLevel) {
    sustainLevel_ = std::clamp(sustainLevel, 0.0f, 1.0f);
}

void Envelope::setReleaseSeconds(float releaseSeconds) {
    releaseSeconds_ = std::max(0.0f, releaseSeconds);
}

void Envelope::noteOn() {
    startStage(Stage::Attack, 1.0f, attackSeconds_);
}

void Envelope::noteOff() {
    if (stage_ == Stage::Idle) {
        return;
    }

    startStage(Stage::Release, 0.0f, releaseSeconds_);
}

void Envelope::reset() {
    value_ = 0.0f;
    stageIncrement_ = 0.0f;
    stageTarget_ = 0.0f;
    samplesRemaining_ = 0;
    stage_ = Stage::Idle;
}

float Envelope::nextValue() {
    switch (stage_) {
        case Stage::Idle:
            value_ = 0.0f;
            break;
        case Stage::Attack:
        case Stage::Decay:
        case Stage::Release:
            if (samplesRemaining_ == 0) {
                completeCurrentStage();
                break;
            }

            value_ += stageIncrement_;
            --samplesRemaining_;
            if (samplesRemaining_ == 0) {
                completeCurrentStage();
            }
            break;
        case Stage::Sustain:
            value_ = sustainLevel_;
            break;
    }

    return value_;
}

bool Envelope::isActive() const {
    return stage_ != Stage::Idle;
}

std::uint32_t Envelope::secondsToSamples(float seconds) const {
    if (seconds <= 0.0f || sampleRate_ <= 0.0) {
        return 0;
    }

    return static_cast<std::uint32_t>(std::max(1.0f, seconds * static_cast<float>(sampleRate_)));
}

void Envelope::startStage(Stage stage, float targetValue, float durationSeconds) {
    const std::uint32_t sampleCount = secondsToSamples(durationSeconds);
    if (sampleCount == 0) {
        value_ = targetValue;
        stageTarget_ = targetValue;
        stageIncrement_ = 0.0f;
        samplesRemaining_ = 0;
        stage_ = stage;
        completeCurrentStage();
        return;
    }

    stage_ = stage;
    stageTarget_ = std::clamp(targetValue, 0.0f, 1.0f);
    samplesRemaining_ = sampleCount;
    stageIncrement_ = (stageTarget_ - value_) / static_cast<float>(sampleCount);
}

void Envelope::completeCurrentStage() {
    value_ = stageTarget_;

    switch (stage_) {
        case Stage::Attack:
            startStage(Stage::Decay, sustainLevel_, decaySeconds_);
            break;
        case Stage::Decay:
            stage_ = Stage::Sustain;
            stageIncrement_ = 0.0f;
            samplesRemaining_ = 0;
            value_ = sustainLevel_;
            break;
        case Stage::Release:
            stage_ = Stage::Idle;
            stageIncrement_ = 0.0f;
            samplesRemaining_ = 0;
            value_ = 0.0f;
            break;
        case Stage::Sustain:
            value_ = sustainLevel_;
            break;
        case Stage::Idle:
            value_ = 0.0f;
            break;
    }
}

}  // namespace synth::dsp
