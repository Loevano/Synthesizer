#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "synth/audio/SampleBuffer.hpp"
#include "synth/audio/SamplePlaybackMode.hpp"
#include "synth/dsp/Envelope.hpp"

namespace synth::audio {

class SamplePlayerEngine {
public:
    void setSampleRate(double sampleRate);
    void setOutputChannelCount(std::uint32_t outputChannelCount);
    void setVoiceCount(std::uint32_t voiceCount);
    void setSampleBuffer(std::shared_ptr<const SampleBuffer> sampleBuffer);
    void setMidiEnabled(bool enabled);
    void setGain(float gain);
    void setRootNote(int rootNote);
    void setTransposeSemitones(float transposeSemitones);
    void setFineTuneCents(float fineTuneCents);
    void setStartPosition(float startPosition);
    void setEndPosition(float endPosition);
    void setLoopEnabled(bool enabled);
    void setPlaybackMode(SamplePlaybackMode mode);
    void setReverse(bool reverse);
    void setEnvelopeAttackSeconds(float attackSeconds);
    void setEnvelopeDecaySeconds(float decaySeconds);
    void setEnvelopeSustainLevel(float sustainLevel);
    void setEnvelopeReleaseSeconds(float releaseSeconds);
    void setOutputEnabled(std::uint32_t outputChannel, bool enabled);
    void clearNotes();
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    void process(float* output, std::uint32_t frames, std::uint32_t channels, float masterGain);

private:
    struct Voice {
        bool active = false;
        bool gateOpen = false;
        int noteNumber = -1;
        float velocity = 1.0f;
        double position = 0.0;
        double step = 1.0;
        int direction = 1;
        double startFrame = 0.0;
        double endFrame = 0.0;
        dsp::Envelope envelope;
    };

    static double pitchRatioForSemitones(double semitones);
    static float readSampleLinear(const SampleBuffer& sampleBuffer, double position);
    void applyEnvelopeSettings(Voice& voice);
    void applyEnvelopeSettingsToVoices();
    void resetVoice(Voice& voice);
    Voice& allocateVoice();
    std::pair<double, double> playbackWindow() const;
    bool hasEnabledOutput(std::uint32_t channels) const;

    std::shared_ptr<const SampleBuffer> sampleBuffer_;
    std::vector<Voice> voices_;
    std::vector<bool> outputEnabled_;
    double sampleRate_ = 48000.0;
    float gain_ = 0.8f;
    float transposeSemitones_ = 0.0f;
    float fineTuneCents_ = 0.0f;
    float startPosition_ = 0.0f;
    float endPosition_ = 1.0f;
    float attackSeconds_ = 0.01f;
    float decaySeconds_ = 0.08f;
    float sustainLevel_ = 0.8f;
    float releaseSeconds_ = 0.2f;
    std::uint32_t nextVoiceIndex_ = 0;
    int rootNote_ = 60;
    bool midiEnabled_ = true;
    bool reverse_ = false;
    SamplePlaybackMode playbackMode_ = SamplePlaybackMode::Gate;
};

}  // namespace synth::audio
