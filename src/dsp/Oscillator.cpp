#include "synth/dsp/Oscillator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

namespace synth::dsp {

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPi = 3.14159265358979323846;
constexpr float kMinFrequencyHz = 1.0f;
constexpr float kNyquistSafety = 0.49f;
constexpr std::size_t kWavetableSize = 4096;
constexpr float kTargetWaveformRms = 0.70710678f;
constexpr float kMaxWaveformPeak = 1.25f;
constexpr float kUniformNoiseRms = 0.57735026919f;
constexpr std::array<int, 12> kBandLimitedHarmonics{
    2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1,
};

float nextNoiseSample() {
    thread_local std::minstd_rand generator{std::random_device{}()};
    thread_local std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    return distribution(generator);
}

using Wavetable = std::vector<float>;

Wavetable normalizeTable(Wavetable table) {
    float peak = 0.0f;
    double squaredSum = 0.0;
    for (const float sample : table) {
        peak = std::max(peak, std::abs(sample));
        squaredSum += static_cast<double>(sample) * static_cast<double>(sample);
    }

    const float rms = table.empty()
        ? 0.0f
        : static_cast<float>(std::sqrt(squaredSum / static_cast<double>(table.size())));
    float gain = 1.0f;
    if (rms > 0.0f) {
        gain = kTargetWaveformRms / rms;
    }
    if (peak > 0.0f) {
        gain = std::min(gain, kMaxWaveformPeak / peak);
    }

    if (gain != 1.0f) {
        for (float& sample : table) {
            sample *= gain;
        }
    }

    table.push_back(table.front());
    return table;
}

Wavetable buildSineTable() {
    Wavetable table;
    table.reserve(kWavetableSize + 1);

    for (std::size_t index = 0; index < kWavetableSize; ++index) {
        const double phase = static_cast<double>(index) / static_cast<double>(kWavetableSize);
        table.push_back(static_cast<float>(std::sin(kTwoPi * phase)));
    }

    return normalizeTable(std::move(table));
}

Wavetable buildSawTable(int maxHarmonic) {
    Wavetable table;
    table.reserve(kWavetableSize + 1);

    for (std::size_t index = 0; index < kWavetableSize; ++index) {
        const double phase = static_cast<double>(index) / static_cast<double>(kWavetableSize);
        double sample = 0.0;
        for (int harmonic = 1; harmonic <= maxHarmonic; ++harmonic) {
            sample += std::sin(kTwoPi * static_cast<double>(harmonic) * phase) / static_cast<double>(harmonic);
        }
        table.push_back(static_cast<float>(-2.0 * sample / kPi));
    }

    return normalizeTable(std::move(table));
}

Wavetable buildSquareTable(int maxHarmonic) {
    Wavetable table;
    table.reserve(kWavetableSize + 1);

    for (std::size_t index = 0; index < kWavetableSize; ++index) {
        const double phase = static_cast<double>(index) / static_cast<double>(kWavetableSize);
        double sample = 0.0;
        for (int harmonic = 1; harmonic <= maxHarmonic; harmonic += 2) {
            sample += std::sin(kTwoPi * static_cast<double>(harmonic) * phase) / static_cast<double>(harmonic);
        }
        table.push_back(static_cast<float>(4.0 * sample / kPi));
    }

    return normalizeTable(std::move(table));
}

Wavetable buildTriangleTable(int maxHarmonic) {
    Wavetable table;
    table.reserve(kWavetableSize + 1);

    for (std::size_t index = 0; index < kWavetableSize; ++index) {
        const double phase = static_cast<double>(index) / static_cast<double>(kWavetableSize);
        double sample = 0.0;
        for (int harmonic = 1; harmonic <= maxHarmonic; harmonic += 2) {
            const double harmonicValue = static_cast<double>(harmonic);
            sample += std::cos(kTwoPi * harmonicValue * phase) / (harmonicValue * harmonicValue);
        }
        table.push_back(static_cast<float>(-8.0 * sample / (kPi * kPi)));
    }

    return normalizeTable(std::move(table));
}

struct WavetableBank {
    Wavetable sine;
    std::array<Wavetable, kBandLimitedHarmonics.size()> saw;
    std::array<Wavetable, kBandLimitedHarmonics.size()> square;
    std::array<Wavetable, kBandLimitedHarmonics.size()> triangle;
};

const WavetableBank& wavetableBank() {
    static const WavetableBank bank = [] {
        WavetableBank tables;
        tables.sine = buildSineTable();
        for (std::size_t tableIndex = 0; tableIndex < kBandLimitedHarmonics.size(); ++tableIndex) {
            const int harmonics = kBandLimitedHarmonics[tableIndex];
            tables.saw[tableIndex] = buildSawTable(harmonics);
            tables.square[tableIndex] = buildSquareTable(harmonics);
            tables.triangle[tableIndex] = buildTriangleTable(harmonics);
        }
        return tables;
    }();

    return bank;
}

std::size_t selectBandLimitedTable(float frequencyHz, double sampleRate) {
    const double allowedHarmonics =
        (sampleRate * 0.5) / std::max<double>(static_cast<double>(frequencyHz), static_cast<double>(kMinFrequencyHz));

    for (std::size_t tableIndex = 0; tableIndex < kBandLimitedHarmonics.size(); ++tableIndex) {
        if (allowedHarmonics >= static_cast<double>(kBandLimitedHarmonics[tableIndex])) {
            return tableIndex;
        }
    }

    return kBandLimitedHarmonics.size() - 1;
}

float interpolateWavetable(const Wavetable& table, double phase) {
    const double wrappedPhase = phase - std::floor(phase);
    const double position = wrappedPhase * static_cast<double>(kWavetableSize);
    const std::size_t index = static_cast<std::size_t>(position);
    const double fraction = position - static_cast<double>(index);
    const float sampleA = table[index];
    const float sampleB = table[index + 1];
    return sampleA + static_cast<float>((sampleB - sampleA) * fraction);
}
}

void Oscillator::setSampleRate(double sampleRate) {
    if (sampleRate > 0.0) {
        sampleRate_ = sampleRate;
        frequencyHz_ = std::clamp(frequencyHz_, kMinFrequencyHz, maxStableFrequencyHz());
        updateWavetable();
    }
}

void Oscillator::setFrequency(float frequencyHz) {
    if (frequencyHz > 0.0f) {
        frequencyHz_ = std::clamp(frequencyHz, kMinFrequencyHz, maxStableFrequencyHz());
        updateWavetable();
    }
}

void Oscillator::setWaveform(Waveform waveform) {
    waveform_ = waveform;
    updateWavetable();
}

void Oscillator::updateWavetable() {
    if (waveform_ == Waveform::Noise) {
        wavetableIndex_ = 0;
        outputGain_ = std::min(kTargetWaveformRms / kUniformNoiseRms, kMaxWaveformPeak);
        return;
    }

    outputGain_ = 1.0f;
    if (waveform_ == Waveform::Sine) {
        wavetableIndex_ = 0;
        (void)wavetableBank();
        return;
    }

    (void)wavetableBank();
    wavetableIndex_ = selectBandLimitedTable(frequencyHz_, sampleRate_);
}

float Oscillator::nextSample() {
    const double phaseIncrement = static_cast<double>(frequencyHz_) / sampleRate_;
    phase_ += phaseIncrement;
    while (phase_ >= 1.0) {
        phase_ -= 1.0;
    }
    while (phase_ < 0.0) {
        phase_ += 1.0;
    }

    const auto& tables = wavetableBank();
    switch (waveform_) {
        case Waveform::Sine:
            return interpolateWavetable(tables.sine, phase_);
        case Waveform::Saw:
            return interpolateWavetable(tables.saw[wavetableIndex_], phase_);
        case Waveform::Square:
            return interpolateWavetable(tables.square[wavetableIndex_], phase_);
        case Waveform::Triangle:
            return interpolateWavetable(tables.triangle[wavetableIndex_], phase_);
        case Waveform::Noise:
            return nextNoiseSample() * outputGain_;
        default:
            return 0.0f;
    }
}

float Oscillator::maxStableFrequencyHz() const {
    return std::max(kMinFrequencyHz, static_cast<float>(sampleRate_ * kNyquistSafety));
}

}  // namespace synth::dsp
