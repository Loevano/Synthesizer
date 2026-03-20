#pragma once

#include <cstddef>
#include <vector>

#include "synth/dsp/Effects.hpp"

namespace synth::dsp {

class Chorus final : public Effects {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setDepth(float depth);
    void setRateHz(float rateHz);
    void setPhaseOffsetDegrees(float phaseOffsetDegrees);
    float processSample(float inputSample) override;

private:
    float readDelayedSample(float delaySamples) const;
    void smoothParameters();

    static constexpr float kBaseDelayMs = 12.0f;
    static constexpr float kMaxDepthMs = 8.0f;
    static constexpr float kWetMix = 0.35f;
    static constexpr float kWetFadeDepth = 0.05f;
    static constexpr float kParameterSmoothingSeconds = 0.02f;

    std::vector<float> buffer_;
    std::size_t writeIndex_ = 0;
    double sampleRate_ = 48000.0;
    float smoothingAmount_ = 1.0f;
    float targetDepth_ = 0.0f;
    float currentDepth_ = 0.0f;
    float targetRateHz_ = 0.25f;
    float currentRateHz_ = 0.25f;
    float phaseAccumulator_ = 0.0f;
    float targetPhaseOffset_ = 0.0f;
    float currentPhaseOffset_ = 0.0f;
};

}  // namespace synth::dsp
