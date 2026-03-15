#pragma once

#include <cstdint>

#include "synth/audio/TestSynth.hpp"

namespace synth::graph {

class TestSourceNode {
public:
    void prepare(double sampleRate, std::uint32_t outputChannels);
    void setActive(bool active);
    void setMidiEnabled(bool enabled);
    void setFrequency(float frequencyHz);
    void setGain(float gain);
    void setWaveform(dsp::Waveform waveform);
    void setEnvelopeAttackSeconds(float attackSeconds);
    void setEnvelopeDecaySeconds(float decaySeconds);
    void setEnvelopeSustainLevel(float sustainLevel);
    void setEnvelopeReleaseSeconds(float releaseSeconds);
    void setOutputEnabled(std::uint32_t outputChannel, bool enabled);
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);
    void renderAdd(float* output, std::uint32_t frames, std::uint32_t channels, bool enabled, float level);

private:
    audio::TestSynth synth_;
};

}  // namespace synth::graph
