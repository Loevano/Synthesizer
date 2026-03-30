#include "synth/dsp/LowPassFilter.hpp"

#include <algorithm>
#include <cmath>

namespace synth::dsp {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr float kMinCutoffHz = 20.0f;
constexpr float kMinResonance = 0.1f;
constexpr float kMaxResonance = 10.0f;
constexpr float kNyquistSafety = 0.49f;

}

void LowPassFilter::prepare(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    updateCoefficients();
    reset();
}

void LowPassFilter::reset() {
    z1_ = 0.0f;
    z2_ = 0.0f;
}

void LowPassFilter::setParameters(float cutoffHz, float resonance) {
    const float maxCutoffHz = std::max(kMinCutoffHz, static_cast<float>(sampleRate_ * kNyquistSafety));
    cutoffHz_ = std::clamp(cutoffHz, kMinCutoffHz, maxCutoffHz);
    resonance_ = std::clamp(resonance, kMinResonance, kMaxResonance);
    updateCoefficients();
}

void LowPassFilter::setCutoffHz(float cutoffHz) {
    setParameters(cutoffHz, resonance_);
}

void LowPassFilter::setResonance(float resonance) {
    setParameters(cutoffHz_, resonance);
}

float LowPassFilter::processSample(float inputSample) {
    const float outputSample = b0_ * inputSample + z1_;
    z1_ = b1_ * inputSample - a1_ * outputSample + z2_;
    z2_ = b2_ * inputSample - a2_ * outputSample;
    return outputSample;
}

void LowPassFilter::updateCoefficients() {
    if (sampleRate_ <= 0.0) {
        return;
    }

    const float maxCutoffHz = std::max(kMinCutoffHz, static_cast<float>(sampleRate_ * kNyquistSafety));
    const double cutoffHz = std::clamp(static_cast<double>(cutoffHz_), static_cast<double>(kMinCutoffHz), static_cast<double>(maxCutoffHz));
    const double q = std::clamp(static_cast<double>(resonance_), static_cast<double>(kMinResonance), static_cast<double>(kMaxResonance));
    const double w0 = kTwoPi * cutoffHz / sampleRate_;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * q);

    const double rawB0 = (1.0 - cosW0) * 0.5;
    const double rawB1 = 1.0 - cosW0;
    const double rawB2 = (1.0 - cosW0) * 0.5;
    const double rawA0 = 1.0 + alpha;
    const double rawA1 = -2.0 * cosW0;
    const double rawA2 = 1.0 - alpha;

    if (std::abs(rawA0) < 1.0e-12) {
        return;
    }

    b0_ = static_cast<float>(rawB0 / rawA0);
    b1_ = static_cast<float>(rawB1 / rawA0);
    b2_ = static_cast<float>(rawB2 / rawA0);
    a1_ = static_cast<float>(rawA1 / rawA0);
    a2_ = static_cast<float>(rawA2 / rawA0);
}

}  // namespace synth::dsp
