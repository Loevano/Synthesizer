#include "synth/dsp/Lfo.hpp"

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

float normalizedTriangle(float phase) {
    const float normalized = phase / kTwoPi;
    return normalized < 0.5f ? (4.0f * normalized - 1.0f) : (3.0f - 4.0f * normalized);
}

}  // namespace

void Lfo::setSampleRate(double sampleRate) {
    if (sampleRate > 0.0) {
        sampleRate_ = sampleRate;
    }
}

void Lfo::setOutputChannelCount(std::uint32_t outputChannelCount) {
    resizeOutputs(outputChannelCount);
}

void Lfo::setEnabled(bool enabled) {
    enabled_ = enabled;
}

void Lfo::setWaveform(LfoWaveform waveform) {
    waveform_ = waveform;
}

void Lfo::setDepth(float depth) {
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void Lfo::setPhaseSpreadDegrees(float phaseSpreadDegrees) {
    phaseSpreadDegrees_ = std::clamp(phaseSpreadDegrees, 0.0f, 360.0f);
    reseedPhases();
}

void Lfo::setPolarityFlip(bool polarityFlip) {
    polarityFlip_ = polarityFlip;
}

void Lfo::setUnlinkedOutputs(bool unlinkedOutputs) {
    unlinkedOutputs_ = unlinkedOutputs;
}

void Lfo::setClockLinked(bool clockLinked) {
    clockLinked_ = clockLinked;
}

void Lfo::setTempoBpm(float tempoBpm) {
    tempoBpm_ = std::clamp(tempoBpm, 20.0f, 300.0f);
}

void Lfo::setRateMultiplier(float rateMultiplier) {
    rateMultiplier_ = std::clamp(rateMultiplier, 0.125f, 8.0f);
}

void Lfo::setFixedFrequencyHz(float frequencyHz) {
    fixedFrequencyHz_ = std::clamp(frequencyHz, 0.01f, 40.0f);
}

void Lfo::renderFrame(float* outputMultipliers, std::uint32_t channels) {
    if (outputMultipliers == nullptr || channels == 0) {
        return;
    }

    resizeOutputs(channels);

    if (!enabled_ || depth_ <= 0.0f) {
        std::fill(outputMultipliers, outputMultipliers + channels, 1.0f);
        return;
    }

    const float baseRateHz = currentRateHz();
    for (std::uint32_t channel = 0; channel < channels; ++channel) {
        float lfoValue = evaluateWaveform(channel);
        if (polarityFlip_ && (channel % 2 == 1)) {
            lfoValue = -lfoValue;
        }

        outputMultipliers[channel] = std::clamp(1.0f + (lfoValue * depth_), 0.0f, 2.0f);

        const float perOutputRate = unlinkedOutputs_
            ? baseRateHz * (1.0f + 0.125f * static_cast<float>(channel))
            : baseRateHz;
        const float phaseIncrement = (kTwoPi * perOutputRate) / static_cast<float>(sampleRate_);

        const float previousPhase = phases_[channel];
        phases_[channel] = wrapPhase(phases_[channel] + phaseIncrement);
        if (waveform_ == LfoWaveform::Random && phases_[channel] < previousPhase) {
            std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
            randomValues_[channel] = distribution(randomGenerator_);
        }
    }
}

void Lfo::resizeOutputs(std::uint32_t outputChannelCount) {
    const std::size_t channelCount = std::max<std::uint32_t>(1, outputChannelCount);
    if (phases_.size() == channelCount) {
        return;
    }

    phases_.resize(channelCount, 0.0f);
    randomValues_.resize(channelCount, 0.0f);

    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    for (auto& value : randomValues_) {
        value = distribution(randomGenerator_);
    }

    reseedPhases();
}

void Lfo::reseedPhases() {
    if (phases_.empty()) {
        return;
    }

    const float basePhase = phases_.front();
    const float spreadRadians = phaseSpreadDegrees_ * (kPi / 180.0f);
    const float denominator = phases_.size() > 1 ? static_cast<float>(phases_.size() - 1) : 1.0f;

    for (std::size_t channel = 0; channel < phases_.size(); ++channel) {
        const float offset = spreadRadians * (static_cast<float>(channel) / denominator);
        phases_[channel] = wrapPhase(basePhase + offset);
    }
}

float Lfo::evaluateWaveform(std::uint32_t channel) {
    const float phase = phases_[channel];
    switch (waveform_) {
        case LfoWaveform::Triangle:
            return normalizedTriangle(phase);
        case LfoWaveform::SawDown:
            return 1.0f - (2.0f * (phase / kTwoPi));
        case LfoWaveform::SawUp:
            return (2.0f * (phase / kTwoPi)) - 1.0f;
        case LfoWaveform::Random:
            return randomValues_[channel];
        case LfoWaveform::Sine:
        default:
            return std::sin(phase);
    }
}

float Lfo::currentRateHz() const {
    if (clockLinked_) {
        return std::clamp((tempoBpm_ / 60.0f) * rateMultiplier_, 0.01f, 40.0f);
    }
    return fixedFrequencyHz_;
}

}  // namespace synth::dsp
