#pragma once

#include <vector>

#include "synth/dsp/Oscillator.hpp"

namespace synth::app {

struct EnvelopeState {
    float attackMs = 10.0f;
    float decayMs = 80.0f;
    float sustain = 0.8f;
    float releaseMs = 200.0f;
};

struct TestSourceState {
    bool implemented = true;
    bool playable = true;
    bool active = false;
    bool midiEnabled = true;
    float frequency = 220.0f;
    float gain = 0.4f;
    dsp::Waveform waveform = dsp::Waveform::Sine;
    EnvelopeState envelope;
    std::vector<bool> outputs;
};

}  // namespace synth::app
