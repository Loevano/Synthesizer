#include "synth/audio/PolySynth.hpp"

#include <algorithm>

namespace synth::audio {

PolySynth::PolySynth() {
    configure({});
}

void PolySynth::configure(const PolySynthConfig& config) {
    const std::uint32_t voiceCount = std::max<std::uint32_t>(1, config.voiceCount);
    const std::uint32_t oscillatorsPerVoice = std::max<std::uint32_t>(1, config.oscillatorsPerVoice);
    oscillatorsPerVoice_ = oscillatorsPerVoice;

    voices_.clear();
    voices_.reserve(voiceCount);

    for (std::uint32_t i = 0; i < voiceCount; ++i) {
        voices_.emplace_back();
        auto& voice = voices_.back();
        voice.configure(oscillatorsPerVoice);
        voice.setSampleRate(sampleRate_);
        voice.setFrequency(frequencyHz_.load());
        voice.setWaveform(waveform_);
        voice.setGain(1.0f);
        voice.setEnvelopeAttackSeconds(attackSeconds_);
        voice.setEnvelopeDecaySeconds(decaySeconds_);
        voice.setEnvelopeSustainLevel(sustainLevel_);
        voice.setEnvelopeReleaseSeconds(releaseSeconds_);
        voice.setOutputChannelCount(outputChannelCount_);
        for (std::uint32_t outputIndex = 0; outputIndex < outputChannelCount_; ++outputIndex) {
            voice.setOutputEnabled(outputIndex, outputIndex == (i % outputChannelCount_));
        }
        voice.setActive(true);
    }

    lfo_.setSampleRate(sampleRate_);
    lfo_.setOutputChannelCount(outputChannelCount_);
}

void PolySynth::setSampleRate(double sampleRate) {
    if (sampleRate <= 0.0) {
        return;
    }

    sampleRate_ = sampleRate;
    lfo_.setSampleRate(sampleRate_);
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate_);
    }
}

void PolySynth::setFrequency(float frequencyHz) {
    const float clampedFrequency = std::max(1.0f, frequencyHz);
    frequencyHz_.store(clampedFrequency);
    for (auto& voice : voices_) {
        voice.setFrequency(clampedFrequency);
    }
}

void PolySynth::setGain(float gain) {
    gain_.store(std::clamp(gain, 0.0f, 1.0f));
}

void PolySynth::setWaveform(dsp::Waveform waveform) {
    waveform_ = waveform;
    for (auto& voice : voices_) {
        voice.setWaveform(waveform_);
    }
}

void PolySynth::setVoiceActive(std::uint32_t voiceIndex, bool active) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setActive(active);
}

void PolySynth::setVoiceFrequency(std::uint32_t voiceIndex, float frequencyHz) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFrequency(frequencyHz);
}

void PolySynth::setVoiceGain(std::uint32_t voiceIndex, float gain) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setGain(gain);
}

void PolySynth::setVoiceEnvelopeAttackSeconds(std::uint32_t voiceIndex, float attackSeconds) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setEnvelopeAttackSeconds(attackSeconds);
}

void PolySynth::setVoiceEnvelopeDecaySeconds(std::uint32_t voiceIndex, float decaySeconds) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setEnvelopeDecaySeconds(decaySeconds);
}

void PolySynth::setVoiceEnvelopeSustainLevel(std::uint32_t voiceIndex, float sustainLevel) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setEnvelopeSustainLevel(sustainLevel);
}

void PolySynth::setVoiceEnvelopeReleaseSeconds(std::uint32_t voiceIndex, float releaseSeconds) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setEnvelopeReleaseSeconds(releaseSeconds);
}

void PolySynth::setVoiceFilterCutoffHz(std::uint32_t voiceIndex, float cutoffHz) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterCutoffHz(cutoffHz);
}

void PolySynth::setVoiceFilterResonance(std::uint32_t voiceIndex, float resonance) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterResonance(resonance);
}

void PolySynth::setVoiceFilterEnvelopeAttackSeconds(std::uint32_t voiceIndex, float attackSeconds) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterEnvelopeAttackSeconds(attackSeconds);
}

void PolySynth::setVoiceFilterEnvelopeDecaySeconds(std::uint32_t voiceIndex, float decaySeconds) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterEnvelopeDecaySeconds(decaySeconds);
}

void PolySynth::setVoiceFilterEnvelopeSustainLevel(std::uint32_t voiceIndex, float sustainLevel) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterEnvelopeSustainLevel(sustainLevel);
}

void PolySynth::setVoiceFilterEnvelopeReleaseSeconds(std::uint32_t voiceIndex, float releaseSeconds) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterEnvelopeReleaseSeconds(releaseSeconds);
}

void PolySynth::setVoiceFilterEnvelopeAmount(std::uint32_t voiceIndex, float amount) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setFilterEnvelopeAmount(amount);
}

void PolySynth::setOutputChannelCount(std::uint32_t outputChannelCount) {
    outputChannelCount_ = std::max<std::uint32_t>(1, outputChannelCount);
    lfo_.setOutputChannelCount(outputChannelCount_);
    for (auto& voice : voices_) {
        voice.setOutputChannelCount(outputChannelCount_);
    }
}

void PolySynth::setVoiceOutputEnabled(std::uint32_t voiceIndex, std::uint32_t outputChannel, bool enabled) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOutputEnabled(outputChannel, enabled);
}

void PolySynth::setOscillatorEnabled(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, bool enabled) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorEnabled(oscillatorIndex, enabled);
}

void PolySynth::setOscillatorGain(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, float gain) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorGain(oscillatorIndex, gain);
}

void PolySynth::setOscillatorFrequency(std::uint32_t voiceIndex, std::uint32_t oscillatorIndex, float frequencyValue) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorFrequency(oscillatorIndex, frequencyValue);
}

void PolySynth::setOscillatorRelativeToVoice(std::uint32_t voiceIndex,
                                         std::uint32_t oscillatorIndex,
                                         bool relativeToVoice) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorRelativeToVoice(oscillatorIndex, relativeToVoice);
}

void PolySynth::setOscillatorWaveform(std::uint32_t voiceIndex,
                                  std::uint32_t oscillatorIndex,
                                  dsp::Waveform waveform) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].setOscillatorWaveform(oscillatorIndex, waveform);
}

void PolySynth::setEnvelopeAttackSeconds(float attackSeconds) {
    attackSeconds_ = std::max(0.0f, attackSeconds);
    for (auto& voice : voices_) {
        voice.setEnvelopeAttackSeconds(attackSeconds_);
    }
}

void PolySynth::setEnvelopeDecaySeconds(float decaySeconds) {
    decaySeconds_ = std::max(0.0f, decaySeconds);
    for (auto& voice : voices_) {
        voice.setEnvelopeDecaySeconds(decaySeconds_);
    }
}

void PolySynth::setEnvelopeSustainLevel(float sustainLevel) {
    sustainLevel_ = std::clamp(sustainLevel, 0.0f, 1.0f);
    for (auto& voice : voices_) {
        voice.setEnvelopeSustainLevel(sustainLevel_);
    }
}

void PolySynth::setEnvelopeReleaseSeconds(float releaseSeconds) {
    releaseSeconds_ = std::max(0.0f, releaseSeconds);
    for (auto& voice : voices_) {
        voice.setEnvelopeReleaseSeconds(releaseSeconds_);
    }
}

void PolySynth::setFilterCutoffHz(float cutoffHz) {
    for (auto& voice : voices_) {
        voice.setFilterCutoffHz(cutoffHz);
    }
}

void PolySynth::setFilterResonance(float resonance) {
    for (auto& voice : voices_) {
        voice.setFilterResonance(resonance);
    }
}

void PolySynth::setFilterEnvelopeAttackSeconds(float attackSeconds) {
    for (auto& voice : voices_) {
        voice.setFilterEnvelopeAttackSeconds(attackSeconds);
    }
}

void PolySynth::setFilterEnvelopeDecaySeconds(float decaySeconds) {
    for (auto& voice : voices_) {
        voice.setFilterEnvelopeDecaySeconds(decaySeconds);
    }
}

void PolySynth::setFilterEnvelopeSustainLevel(float sustainLevel) {
    for (auto& voice : voices_) {
        voice.setFilterEnvelopeSustainLevel(sustainLevel);
    }
}

void PolySynth::setFilterEnvelopeReleaseSeconds(float releaseSeconds) {
    for (auto& voice : voices_) {
        voice.setFilterEnvelopeReleaseSeconds(releaseSeconds);
    }
}

void PolySynth::setFilterEnvelopeAmount(float amount) {
    for (auto& voice : voices_) {
        voice.setFilterEnvelopeAmount(amount);
    }
}

void PolySynth::setLfoEnabled(bool enabled) {
    lfo_.setEnabled(enabled);
}

void PolySynth::setLfoWaveform(dsp::LfoWaveform waveform) {
    lfo_.setWaveform(waveform);
}

void PolySynth::setLfoDepth(float depth) {
    lfo_.setDepth(depth);
}

void PolySynth::setLfoPhaseSpreadDegrees(float phaseSpreadDegrees) {
    lfo_.setPhaseSpreadDegrees(phaseSpreadDegrees);
}

void PolySynth::setLfoPolarityFlip(bool polarityFlip) {
    lfo_.setPolarityFlip(polarityFlip);
}

void PolySynth::setLfoUnlinkedOutputs(bool unlinkedOutputs) {
    lfo_.setUnlinkedOutputs(unlinkedOutputs);
}

void PolySynth::setLfoClockLinked(bool clockLinked) {
    lfo_.setClockLinked(clockLinked);
}

void PolySynth::setLfoTempoBpm(float tempoBpm) {
    lfo_.setTempoBpm(tempoBpm);
}

void PolySynth::setLfoRateMultiplier(float rateMultiplier) {
    lfo_.setRateMultiplier(rateMultiplier);
}

void PolySynth::setLfoFixedFrequencyHz(float frequencyHz) {
    lfo_.setFixedFrequencyHz(frequencyHz);
}

void PolySynth::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    std::fill(output, output + sampleCount, 0.0f);

    process(output, frames, channels);
}

void PolySynth::process(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    const float gain = gain_.load();
    const std::size_t sampleCount = static_cast<std::size_t>(frames) * channels;
    if (lfoModulationBuffer_.size() < sampleCount) {
        lfoModulationBuffer_.resize(sampleCount);
    }
    std::fill(lfoModulationBuffer_.begin(), lfoModulationBuffer_.begin() + sampleCount, 1.0f);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        lfo_.renderFrame(lfoModulationBuffer_.data() + (frame * channels), channels);
    }

    for (auto& voice : voices_) {
        voice.process(output, frames, channels, gain, lfoModulationBuffer_.data());
    }
}

void PolySynth::noteOn(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].noteOn();
}

void PolySynth::noteOff(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].noteOff();
}

void PolySynth::clearVoice(std::uint32_t voiceIndex) {
    if (voiceIndex >= voices_.size()) {
        return;
    }

    voices_[voiceIndex].clearNote();
}

void PolySynth::clearNotes() {
    for (auto& voice : voices_) {
        voice.clearNote();
    }
}

}  // namespace synth::audio
