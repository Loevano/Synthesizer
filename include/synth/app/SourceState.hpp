#pragma once

#include <cstdint>
#include <vector>

#include "synth/dsp/Oscillator.hpp"

namespace synth::app {

// Shared controller-side source structs that are reused across multiple synths.
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

enum class PiecesOutputAlgorithm {
    RoundRobin,
    Random,
    PingPong,
    CenterOut,
    Spray,
};

struct PiecesFeedbackState {
    float amount = 0.18f;
    float highPassHz = 120.0f;
    float lowPassHz = 7500.0f;
};

struct PiecesSourceState {
    bool implemented = true;
    bool playable = true;
    bool liveInputSupported = false;
    float grainSizeMs = 140.0f;
    float grainDensityHz = 6.0f;
    std::uint32_t activeGrains = 8;
    float position = 0.22f;
    float positionJitter = 0.14f;
    float pitchSemitones = 0.0f;
    float pitchJitterCents = 10.0f;
    PiecesOutputAlgorithm outputAlgorithm = PiecesOutputAlgorithm::RoundRobin;
    float outputSpread = 0.35f;
    PiecesFeedbackState feedback;
};

}  // namespace synth::app
