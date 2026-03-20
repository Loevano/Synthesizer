#pragma once

#include <cstdint>
#include <vector>

#include "synth/dsp/Envelope.hpp"
#include "synth/dsp/Oscillator.hpp"

namespace synth::audio {

class TestEngine {
public:
    void setSampleRate(double sampleRate);
    void setOutputChannelCount(std::uint32_t outputChannelCount);
    void setFrequency(float frequencyHz);
    void setGain(float gain);
    void setWaveform(dsp::Waveform waveform);
    void setOutputEnabled(std::uint32_t outputChannel, bool enabled);
    void setActive(bool active);
    void setMidiEnabled(bool enabled);
    void setEnvelopeAttackSeconds(float attackSeconds);
    void setEnvelopeDecaySeconds(float decaySeconds);
    void setEnvelopeSustainLevel(float sustainLevel);
    void setEnvelopeReleaseSeconds(float releaseSeconds);
    void clearNotes();
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    void renderAdd(float* output, std::uint32_t frames, std::uint32_t channels, float masterGain);

private:
    static float midiNoteToFrequency(int noteNumber);
    void syncGate();
    bool shouldGateBeOpen() const;

    dsp::Oscillator oscillator_;
    dsp::Envelope envelope_;
    std::vector<bool> outputEnabled_;
    std::vector<int> heldNotes_;
    double sampleRate_ = 48000.0;
    float frequencyHz_ = 220.0f;
    float gain_ = 0.4f;
    float velocityGain_ = 1.0f;
    bool active_ = false;
    bool midiEnabled_ = true;
    bool gateOpen_ = false;
};

}  // namespace synth::audio
