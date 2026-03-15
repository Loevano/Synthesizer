#include "synth/dsp/Chorus.hpp"

#include <algorithm>
#include <cmath>

namespace synth::dsp {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

float wrapPhase(float phase) {
    while (phase >= kTwoPi) {
        phase -= kTwoPi;
    }
    while (phase < 0.0f) {
        phase += kTwoPi;
    }
    return phase;
}

}  // namespace

void Chorus::prepare(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    const float maxDelayMs = kBaseDelayMs + kMaxDepthMs + 1.0f;
    const std::size_t maxDelaySamples = static_cast<std::size_t>(std::ceil((sampleRate_ * maxDelayMs) / 1000.0));
    buffer_.assign(std::max<std::size_t>(maxDelaySamples + 2, 4), 0.0f);
    writeIndex_ = 0;
}

void Chorus::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
}

void Chorus::setDepth(float depth) {
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Chorus::setRateHz(float rateHz) {
    rateHz_ = std::clamp(rateHz, 0.01f, 20.0f);
}

void Chorus::setPhaseOffsetDegrees(float phaseOffsetDegrees) {
    phase_ = wrapPhase(std::clamp(phaseOffsetDegrees, 0.0f, 360.0f) * (kTwoPi / 360.0f));
}

float Chorus::processSample(float inputSample) {
    if (buffer_.empty()) {
        return inputSample;
    }

    if (depth_ <= 0.0f) {
        return inputSample;
    }

    buffer_[writeIndex_] = inputSample;

    const float modulation = 0.5f * (std::sin(phase_) + 1.0f);
    const float delayMs = kBaseDelayMs + (depth_ * kMaxDepthMs * modulation);
    const float delaySamples = static_cast<float>((sampleRate_ * delayMs) / 1000.0);
    const float delayedSample = readDelayedSample(delaySamples);

    writeIndex_ = (writeIndex_ + 1) % buffer_.size();

    const float phaseIncrement = (kTwoPi * rateHz_) / static_cast<float>(sampleRate_);
    phase_ = wrapPhase(phase_ + phaseIncrement);

    return (inputSample * kDryMix) + (delayedSample * kWetMix);
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
