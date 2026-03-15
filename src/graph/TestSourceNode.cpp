#include "synth/graph/TestSourceNode.hpp"

#include <algorithm>

namespace synth::graph {

void TestSourceNode::prepare(double sampleRate, std::uint32_t outputChannels) {
    synth_.setSampleRate(sampleRate);
    synth_.setOutputChannelCount(outputChannels);
}

void TestSourceNode::setActive(bool active) {
    synth_.setActive(active);
}

void TestSourceNode::setMidiEnabled(bool enabled) {
    synth_.setMidiEnabled(enabled);
}

void TestSourceNode::setFrequency(float frequencyHz) {
    synth_.setFrequency(frequencyHz);
}

void TestSourceNode::setGain(float gain) {
    synth_.setGain(gain);
}

void TestSourceNode::setWaveform(dsp::Waveform waveform) {
    synth_.setWaveform(waveform);
}

void TestSourceNode::setEnvelopeAttackSeconds(float attackSeconds) {
    synth_.setEnvelopeAttackSeconds(attackSeconds);
}

void TestSourceNode::setEnvelopeDecaySeconds(float decaySeconds) {
    synth_.setEnvelopeDecaySeconds(decaySeconds);
}

void TestSourceNode::setEnvelopeSustainLevel(float sustainLevel) {
    synth_.setEnvelopeSustainLevel(sustainLevel);
}

void TestSourceNode::setEnvelopeReleaseSeconds(float releaseSeconds) {
    synth_.setEnvelopeReleaseSeconds(releaseSeconds);
}

void TestSourceNode::setOutputEnabled(std::uint32_t outputChannel, bool enabled) {
    synth_.setOutputEnabled(outputChannel, enabled);
}

void TestSourceNode::noteOn(int noteNumber, float velocity) {
    synth_.noteOn(noteNumber, velocity);
}

void TestSourceNode::noteOff(int noteNumber) {
    synth_.noteOff(noteNumber);
}

void TestSourceNode::renderAdd(float* output,
                               std::uint32_t frames,
                               std::uint32_t channels,
                               bool enabled,
                               float level) {
    if (!enabled) {
        return;
    }

    synth_.renderAdd(output, frames, channels, std::clamp(level, 0.0f, 1.0f));
}

}  // namespace synth::graph
