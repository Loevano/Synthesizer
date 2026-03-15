#pragma once

#include <cstddef>
#include <vector>

namespace synth::dsp {

class DelayLine {
public:
    void prepare(double sampleRate, float maxDelayMs);
    void reset();
    void setDelayMs(float delayMs);
    float processSample(float inputSample);

private:
    std::vector<float> buffer_;
    std::size_t writeIndex_ = 0;
    std::size_t delaySamples_ = 0;
    std::size_t maxDelaySamples_ = 0;
    double sampleRate_ = 48000.0;
};

}  // namespace synth::dsp
