#pragma once

namespace synth::dsp {

class ThreeBandEq {
public:
    struct BiquadSection {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    void prepare(double sampleRate);
    void reset();
    void setLowGainDb(float gainDb);
    void setMidGainDb(float gainDb);
    void setHighGainDb(float gainDb);
    void setGainsDb(float lowGainDb, float midGainDb, float highGainDb);

    float processSample(float inputSample);

private:
    static BiquadSection makeLowShelf(double sampleRate, float frequencyHz, float gainDb);
    static BiquadSection makePeaking(double sampleRate, float frequencyHz, float q, float gainDb);
    static BiquadSection makeHighShelf(double sampleRate, float frequencyHz, float gainDb);
    static float processSection(BiquadSection& section, float inputSample);

    void updateFilters();

    double sampleRate_ = 48000.0;
    float lowGainDb_ = 0.0f;
    float midGainDb_ = 0.0f;
    float highGainDb_ = 0.0f;
    BiquadSection lowSection_;
    BiquadSection midSection_;
    BiquadSection highSection_;
};

}  // namespace synth::dsp
