#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "synth/audio/SampleBuffer.hpp"
#include "synth/audio/SamplePlayerEngine.hpp"

namespace synth::graph {

class PiecesSourceNode {
public:
    void prepare(double sampleRate, std::uint32_t outputChannels);
    void setVoiceCount(std::uint32_t voiceCount);
    void setSampleBuffer(std::shared_ptr<const audio::SampleBuffer> sampleBuffer);
    void setMidiEnabled(bool enabled);
    void setGain(float gain);
    void setRootNote(int rootNote);
    void setTransposeSemitones(float transposeSemitones);
    void setFineTuneCents(float fineTuneCents);
    void setStartPosition(float startPosition);
    void setEndPosition(float endPosition);
    void setLoopEnabled(bool enabled);
    void setPlaybackMode(audio::SamplePlaybackMode mode);
    void setReverse(bool reverse);
    void setEnvelopeAttackSeconds(float attackSeconds);
    void setEnvelopeDecaySeconds(float decaySeconds);
    void setEnvelopeSustainLevel(float sustainLevel);
    void setEnvelopeReleaseSeconds(float releaseSeconds);
    void setOutputEnabled(std::uint32_t outputChannel, bool enabled);
    void clearNotes();
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);
    void process(float* output, std::uint32_t frames, std::uint32_t channels, bool enabled, float level);

private:
    audio::SamplePlayerEngine engine_;
    std::atomic<float> targetLevel_{0.0f};
    float currentLevel_ = 0.0f;
    std::vector<float> renderBuffer_;
};

}  // namespace synth::graph
