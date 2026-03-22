#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "synth/app/SourceState.hpp"
#include "synth/dsp/LowPassFilter.hpp"

namespace synth::audio {

class PiecesEngine {
public:
    void prepare(double sampleRate, std::uint32_t outputChannels);
    void reset();
    void clearNotes();
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    void setGrainSizeMs(float grainSizeMs);
    void setGrainDensityHz(float grainDensityHz);
    void setActiveGrains(std::uint32_t activeGrains);
    void setPosition(float position);
    void setPositionJitter(float positionJitter);
    void setPitchSemitones(float pitchSemitones);
    void setPitchJitterCents(float pitchJitterCents);
    void setOutputAlgorithm(app::PiecesOutputAlgorithm outputAlgorithm);
    void setOutputSpread(float outputSpread);
    void setFeedbackAmount(float feedbackAmount);
    void setFeedbackHighPassHz(float cutoffHz);
    void setFeedbackLowPassHz(float cutoffHz);

    void process(float* output, std::uint32_t frames, std::uint32_t channels, float masterLevel);

private:
    struct Grain {
        bool active = false;
        double readPosition = 0.0;
        double readStep = 1.0;
        std::uint32_t ageSamples = 0;
        std::uint32_t durationSamples = 1;
        float amplitude = 0.0f;
        std::uint32_t outputIndex = 0;
    };

    static float midiNoteToPlaybackRatio(int noteNumber, float pitchSemitones);
    static float wrapUnit(float value);
    static double wrapPosition(double position, std::size_t size);
    static float hannWindow(std::uint32_t ageSamples, std::uint32_t durationSamples);

    void rebuildSourceBuffer();
    void syncCenterOutOrder();
    void updateHighPassCoefficients();
    float processHighPassSample(float inputSample);
    float readSourceSample(double readPosition) const;
    void writeFeedbackSample(float inputSample);
    std::uint32_t chooseOutputIndex();
    Grain* acquireGrainSlot();
    void spawnGrain();
    bool hasHeldNotes() const;
    int currentNoteNumber() const;

    double sampleRate_ = 48000.0;
    std::uint32_t outputChannels_ = 2;
    std::vector<float> sourceBuffer_;
    std::vector<float> feedbackBuffer_;
    std::vector<int> heldNotes_;
    std::vector<Grain> grains_;
    std::vector<std::uint32_t> centerOutOrder_;
    std::mt19937 rng_{0x50494543u};
    std::uint32_t nextRoundRobinOutput_ = 0;
    std::uint32_t pingPongOutput_ = 0;
    int pingPongDirection_ = 1;
    double sprayCenter_ = 0.0;
    std::uint32_t nextCenterOutIndex_ = 0;
    std::size_t feedbackWriteIndex_ = 0;
    double samplesUntilNextGrain_ = 0.0;
    float noteVelocity_ = 1.0f;
    float grainSizeMs_ = 140.0f;
    float grainDensityHz_ = 6.0f;
    std::uint32_t activeGrains_ = 8;
    float position_ = 0.22f;
    float positionJitter_ = 0.14f;
    float pitchSemitones_ = 0.0f;
    float pitchJitterCents_ = 10.0f;
    app::PiecesOutputAlgorithm outputAlgorithm_ = app::PiecesOutputAlgorithm::RoundRobin;
    float outputSpread_ = 0.35f;
    float feedbackAmount_ = 0.18f;
    float feedbackHighPassHz_ = 120.0f;
    float feedbackLowPassHz_ = 7500.0f;
    float highPassAlpha_ = 0.0f;
    float highPassPrevInput_ = 0.0f;
    float highPassPrevOutput_ = 0.0f;
    dsp::LowPassFilter feedbackLowPass_;
};

}  // namespace synth::audio
