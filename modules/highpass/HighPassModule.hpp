#pragma once

#include "synth/dsp/IDspModule.hpp"

namespace synth::modules {

class HighPassModule final : public dsp::IDspModule {
public:
    void prepare(double sampleRate) override;
    float processSample(float input) override;
    void setParameter(std::string_view name, float value) override;

private:
    double sampleRate_ = 48000.0;
    float cutoff_ = 250.0f;
    float state_ = 0.0f;
};

}  // namespace synth::modules
