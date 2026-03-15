#include "synth/dsp/Oscillator.hpp"

#include <cmath>
#include <random>

namespace synth::dsp {

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
constexpr float kMinFrequencyHz = 1.0f;
constexpr float kNyquistSafety = 0.49f;

float nextNoiseSample() {
    thread_local std::minstd_rand generator{std::random_device{}()};
    thread_local std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    return distribution(generator);
}
}

void Oscillator::setSampleRate(double sampleRate) {
    if (sampleRate > 0.0) {
        sampleRate_ = sampleRate;
        frequencyHz_ = std::clamp(frequencyHz_, kMinFrequencyHz, maxStableFrequencyHz());
    }
}

void Oscillator::setFrequency(float frequencyHz) {
    if (frequencyHz > 0.0f) {
        frequencyHz_ = std::clamp(frequencyHz, kMinFrequencyHz, maxStableFrequencyHz());
    }
}

void Oscillator::setWaveform(Waveform waveform) {
    waveform_ = waveform;
}

float Oscillator::nextSample() {
    const double phaseIncrement = kTwoPi * static_cast<double>(frequencyHz_) / sampleRate_;
    phase_ += phaseIncrement;
    while (phase_ >= kTwoPi) {
        phase_ -= kTwoPi;
    }
    while (phase_ < 0.0) {
        phase_ += kTwoPi;
    }

    switch (waveform_) {
        case Waveform::Sine:
            return static_cast<float>(std::sin(phase_));
        case Waveform::Saw:
            return static_cast<float>((2.0 * (phase_ / kTwoPi)) - 1.0);
        case Waveform::Square:
            return phase_ < 3.14159265358979323846 ? 1.0f : -1.0f;
        case Waveform::Triangle: {
            const double normalized = phase_ / kTwoPi;
            const double triangle = normalized < 0.5 ? (4.0 * normalized - 1.0) : (3.0 - 4.0 * normalized);
            return static_cast<float>(triangle);
        }
        case Waveform::Noise:
            return nextNoiseSample();
        default:
            return 0.0f;
    }
}

float Oscillator::maxStableFrequencyHz() const {
    return std::max(kMinFrequencyHz, static_cast<float>(sampleRate_ * kNyquistSafety));
}

}  // namespace synth::dsp
