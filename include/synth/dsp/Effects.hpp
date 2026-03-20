#pragma once

namespace synth::dsp {

// Shared lifecycle for output effects that process one sample stream at a time.
class Effects {
public:
    virtual ~Effects() = default;

    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual float processSample(float inputSample) = 0;
};

}  // namespace synth::dsp
