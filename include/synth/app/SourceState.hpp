#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "synth/audio/SampleBuffer.hpp"
#include "synth/audio/SamplePlaybackMode.hpp"
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

struct PiecesSourceState {
    bool implemented = true;
    bool playable = true;
    bool midiEnabled = true;
    bool loopEnabled = false;
    bool reverse = false;
    bool sampleLoaded = false;
    audio::SamplePlaybackMode playbackMode = audio::SamplePlaybackMode::Gate;
    std::uint32_t voiceCount = 8;
    int rootNote = 60;
    float gain = 0.8f;
    float transposeSemitones = 0.0f;
    float fineTuneCents = 0.0f;
    float start = 0.0f;
    float end = 1.0f;
    double sampleRate = 0.0;
    std::uint64_t sampleFrames = 0;
    std::string samplePath;
    std::string sampleName;
    std::vector<float> samplePeaks;
    EnvelopeState envelope;
    std::vector<bool> outputs;
    std::shared_ptr<const audio::SampleBuffer> sampleBuffer;
};

}  // namespace synth::app
