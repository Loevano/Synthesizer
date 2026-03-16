#include "synth/dsp/Chorus.hpp"

#include <algorithm>
#include <cmath>

namespace synth::dsp {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi = 3.14159265358979323846f;

float wrapPhase(float phase) {
    while (phase >= kTwoPi) {
        phase -= kTwoPi;
    }
    while (phase < 0.0f) {
        phase += kTwoPi;
    }
    return phase;
}

float shortestPhaseDelta(float fromPhase, float toPhase) {
    float delta = toPhase - fromPhase;
    while (delta > kPi) {
        delta -= kTwoPi;
    }
    while (delta < -kPi) {
        delta += kTwoPi;
    }
    return delta;
}

}  // namespace

void Chorus::prepare(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    smoothingAmount_ =
        1.0f - std::exp(-1.0f / static_cast<float>(sampleRate_ * kParameterSmoothingSeconds));
    const float maxDelayMs = kBaseDelayMs + kMaxDepthMs + 1.0f;
    const std::size_t maxDelaySamples = static_cast<std::size_t>(std::ceil((sampleRate_ * maxDelayMs) / 1000.0));
    buffer_.assign(std::max<std::size_t>(maxDelaySamples + 2, 4), 0.0f);
    writeIndex_ = 0;
    currentDepth_ = targetDepth_;
    currentRateHz_ = targetRateHz_;
    currentPhaseOffset_ = targetPhaseOffset_;
    phaseAccumulator_ = 0.0f;
}

void Chorus::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
    currentDepth_ = targetDepth_;
    currentRateHz_ = targetRateHz_;
    currentPhaseOffset_ = targetPhaseOffset_;
    phaseAccumulator_ = 0.0f;
}

void Chorus::setDepth(float depth) {
    targetDepth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Chorus::setRateHz(float rateHz) {
    targetRateHz_ = std::clamp(rateHz, 0.01f, 20.0f);
}

void Chorus::setPhaseOffsetDegrees(float phaseOffsetDegrees) {
    targetPhaseOffset_ = wrapPhase(std::clamp(phaseOffsetDegrees, 0.0f, 360.0f) * (kTwoPi / 360.0f));
}

float Chorus::processSample(float inputSample) {
    if (buffer_.empty()) {
        return inputSample;
    }

    smoothParameters();

    buffer_[writeIndex_] = inputSample;

    if (currentDepth_ <= 0.0001f && targetDepth_ <= 0.0001f) {
        writeIndex_ = (writeIndex_ + 1) % buffer_.size();
        const float phaseIncrement = (kTwoPi * currentRateHz_) / static_cast<float>(sampleRate_);
        phaseAccumulator_ = wrapPhase(phaseAccumulator_ + phaseIncrement);
        return inputSample;
    }

    const float modulationPhase = wrapPhase(phaseAccumulator_ + currentPhaseOffset_);
    const float modulation = 0.5f * (std::sin(modulationPhase) + 1.0f);
    const float delayMs = kBaseDelayMs + (currentDepth_ * kMaxDepthMs * modulation);
    const float delaySamples = static_cast<float>((sampleRate_ * delayMs) / 1000.0);
    const float delayedSample = readDelayedSample(delaySamples);

    writeIndex_ = (writeIndex_ + 1) % buffer_.size();

    const float phaseIncrement = (kTwoPi * currentRateHz_) / static_cast<float>(sampleRate_);
    phaseAccumulator_ = wrapPhase(phaseAccumulator_ + phaseIncrement);

    return inputSample + (delayedSample * kWetMix);
}

void Chorus::smoothParameters() {
    currentDepth_ += (targetDepth_ - currentDepth_) * smoothingAmount_;
    currentRateHz_ += (targetRateHz_ - currentRateHz_) * smoothingAmount_;
    currentPhaseOffset_ =
        wrapPhase(currentPhaseOffset_ + (shortestPhaseDelta(currentPhaseOffset_, targetPhaseOffset_) * smoothingAmount_));
}

float Chorus::readDelayedSample(float delaySamples) const {
    if (buffer_.empty()) {
        return 0.0f;
    }

    const float wrappedReadIndex = static_cast<float>(writeIndex_) - delaySamples;
    float readIndex = wrappedReadIndex;
    while (readIndex < 0.0f) {
        readIndex += static_cast<float>(buffer_.size());
    }

    const float readIndexFloor = std::floor(readIndex);
    const std::size_t indexA = static_cast<std::size_t>(readIndexFloor) % buffer_.size();
    const std::size_t indexB = (indexA + 1) % buffer_.size();
    const float fraction = readIndex - readIndexFloor;
    return buffer_[indexA] + ((buffer_[indexB] - buffer_[indexA]) * fraction);
}

}  // namespace synth::dsp
