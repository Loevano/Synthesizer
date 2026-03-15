#include "synth/dsp/DelayLine.hpp"

#include <algorithm>
#include <cmath>

namespace synth::dsp {

void DelayLine::prepare(double sampleRate, float maxDelayMs) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    maxDelaySamples_ = static_cast<std::size_t>(std::ceil(sampleRate_ * std::max(0.0f, maxDelayMs) / 1000.0f));
    buffer_.assign(std::max<std::size_t>(maxDelaySamples_ + 1, 2), 0.0f);
    writeIndex_ = 0;
    delaySamples_ = std::min(delaySamples_, maxDelaySamples_);
}

void DelayLine::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
}

void DelayLine::setDelayMs(float delayMs) {
    if (sampleRate_ <= 0.0) {
        return;
    }

    const auto requestedSamples = static_cast<std::size_t>(
        std::llround(sampleRate_ * std::clamp(delayMs, 0.0f, static_cast<float>(maxDelaySamples_ * 1000.0 / sampleRate_)) / 1000.0));
    delaySamples_ = std::min(requestedSamples, maxDelaySamples_);
}

float DelayLine::processSample(float inputSample) {
    if (buffer_.empty()) {
        return inputSample;
    }

    float outputSample = inputSample;
    if (delaySamples_ > 0) {
        const std::size_t readIndex =
            (writeIndex_ + buffer_.size() - delaySamples_) % buffer_.size();
        outputSample = buffer_[readIndex];
    }

    buffer_[writeIndex_] = inputSample;
    writeIndex_ = (writeIndex_ + 1) % buffer_.size();
    return outputSample;
}

}  // namespace synth::dsp
