#pragma once

namespace synth::dsp {

class LowPassFilter {
public:
    void prepare(double sampleRate);
    void reset();
    void setCutoffHz(float cutoffHz);
    void setResonance(float resonance);

    float processSample(float inputSample);

private:
    void updateCoefficients();

    double sampleRate_ = 48000.0;
    float cutoffHz_ = 18000.0f;
    float resonance_ = 0.707f;
    float b0_ = 1.0f;
    float b1_ = 0.0f;
    float b2_ = 0.0f;
    float a1_ = 0.0f;
    float a2_ = 0.0f;
    float z1_ = 0.0f;
    float z2_ = 0.0f;
};

}  // namespace synth::dsp
