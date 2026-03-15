#include "synth/dsp/ThreeBandEq.hpp"

#include <algorithm>
#include <cmath>

namespace synth::dsp {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr float kLowShelfFrequencyHz = 180.0f;
constexpr float kMidPeakFrequencyHz = 1200.0f;
constexpr float kHighShelfFrequencyHz = 4500.0f;
constexpr float kMidPeakQ = 0.707f;

ThreeBandEq::BiquadSection normalizeSection(double b0,
                                            double b1,
                                            double b2,
                                            double a0,
                                            double a1,
                                            double a2) {
    ThreeBandEq::BiquadSection section;
    if (std::abs(a0) < 1.0e-12) {
        return section;
    }

    section.b0 = static_cast<float>(b0 / a0);
    section.b1 = static_cast<float>(b1 / a0);
    section.b2 = static_cast<float>(b2 / a0);
    section.a1 = static_cast<float>(a1 / a0);
    section.a2 = static_cast<float>(a2 / a0);
    return section;
}

}  // namespace

void ThreeBandEq::prepare(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    updateFilters();
    reset();
}

void ThreeBandEq::reset() {
    lowSection_.z1 = 0.0f;
    lowSection_.z2 = 0.0f;
    midSection_.z1 = 0.0f;
    midSection_.z2 = 0.0f;
    highSection_.z1 = 0.0f;
    highSection_.z2 = 0.0f;
}

void ThreeBandEq::setLowGainDb(float gainDb) {
    lowGainDb_ = std::clamp(gainDb, -24.0f, 24.0f);
    updateFilters();
}

void ThreeBandEq::setMidGainDb(float gainDb) {
    midGainDb_ = std::clamp(gainDb, -24.0f, 24.0f);
    updateFilters();
}

void ThreeBandEq::setHighGainDb(float gainDb) {
    highGainDb_ = std::clamp(gainDb, -24.0f, 24.0f);
    updateFilters();
}

void ThreeBandEq::setGainsDb(float lowGainDb, float midGainDb, float highGainDb) {
    lowGainDb_ = std::clamp(lowGainDb, -24.0f, 24.0f);
    midGainDb_ = std::clamp(midGainDb, -24.0f, 24.0f);
    highGainDb_ = std::clamp(highGainDb, -24.0f, 24.0f);
    updateFilters();
}

float ThreeBandEq::processSample(float inputSample) {
    float sample = processSection(lowSection_, inputSample);
    sample = processSection(midSection_, sample);
    sample = processSection(highSection_, sample);
    return sample;
}

ThreeBandEq::BiquadSection ThreeBandEq::makeLowShelf(double sampleRate, float frequencyHz, float gainDb) {
    if (std::abs(gainDb) < 0.001f || sampleRate <= 0.0) {
        return {};
    }

    const double a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const double w0 = kTwoPi * static_cast<double>(frequencyHz) / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / std::sqrt(2.0);
    const double beta = 2.0 * std::sqrt(a) * alpha;

    const double b0 = a * ((a + 1.0) - (a - 1.0) * cosW0 + beta);
    const double b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosW0);
    const double b2 = a * ((a + 1.0) - (a - 1.0) * cosW0 - beta);
    const double a0 = (a + 1.0) + (a - 1.0) * cosW0 + beta;
    const double a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cosW0);
    const double a2 = (a + 1.0) + (a - 1.0) * cosW0 - beta;
    return normalizeSection(b0, b1, b2, a0, a1, a2);
}

ThreeBandEq::BiquadSection ThreeBandEq::makePeaking(double sampleRate,
                                                    float frequencyHz,
                                                    float q,
                                                    float gainDb) {
    if (std::abs(gainDb) < 0.001f || sampleRate <= 0.0) {
        return {};
    }

    const double a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const double w0 = kTwoPi * static_cast<double>(frequencyHz) / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * std::max(0.001f, q));

    const double b0 = 1.0 + alpha * a;
    const double b1 = -2.0 * cosW0;
    const double b2 = 1.0 - alpha * a;
    const double a0 = 1.0 + alpha / a;
    const double a1 = -2.0 * cosW0;
    const double a2 = 1.0 - alpha / a;
    return normalizeSection(b0, b1, b2, a0, a1, a2);
}

ThreeBandEq::BiquadSection ThreeBandEq::makeHighShelf(double sampleRate, float frequencyHz, float gainDb) {
    if (std::abs(gainDb) < 0.001f || sampleRate <= 0.0) {
        return {};
    }

    const double a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const double w0 = kTwoPi * static_cast<double>(frequencyHz) / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / std::sqrt(2.0);
    const double beta = 2.0 * std::sqrt(a) * alpha;

    const double b0 = a * ((a + 1.0) + (a - 1.0) * cosW0 + beta);
    const double b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW0);
    const double b2 = a * ((a + 1.0) + (a - 1.0) * cosW0 - beta);
    const double a0 = (a + 1.0) - (a - 1.0) * cosW0 + beta;
    const double a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cosW0);
    const double a2 = (a + 1.0) - (a - 1.0) * cosW0 - beta;
    return normalizeSection(b0, b1, b2, a0, a1, a2);
}

float ThreeBandEq::processSection(BiquadSection& section, float inputSample) {
    const float outputSample = section.b0 * inputSample + section.z1;
    section.z1 = section.b1 * inputSample - section.a1 * outputSample + section.z2;
    section.z2 = section.b2 * inputSample - section.a2 * outputSample;
    return outputSample;
}

void ThreeBandEq::updateFilters() {
    lowSection_ = makeLowShelf(sampleRate_, kLowShelfFrequencyHz, lowGainDb_);
    midSection_ = makePeaking(sampleRate_, kMidPeakFrequencyHz, kMidPeakQ, midGainDb_);
    highSection_ = makeHighShelf(sampleRate_, kHighShelfFrequencyHz, highGainDb_);
}

}  // namespace synth::dsp
