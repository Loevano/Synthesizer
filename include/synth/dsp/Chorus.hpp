#pragma once

#include <cstddef>
#include <vector>

namespace synth::dsp {

class Chorus {
public:
    void prepare(double sampleRate);
    void reset();
    void setDepth(float depth);
    void setRateHz(float rateHz);
    void setPhaseOffsetDegrees(float phaseOffsetDegrees);
    float processSample(float inputSample);

private:
    float readDelayedSample(float delaySamples) const;

    static constexpr float kBaseDelayMs = 12.0f;
    static constexpr float kMaxDepthMs = 8.0f;
    static constexpr float kDryMix = 0.6f;
    static constexpr float kWetMix = 0.4f;

    std::vector<float> buffer_;
    std::size_t writeIndex_ = 0;
    double sampleRate_ = 48000.0;
    float depth_ = 0.0f;
    float rateHz_ = 0.25f;
    float phase_ = 0.0f;
};

}  // namespace synth::dsp
