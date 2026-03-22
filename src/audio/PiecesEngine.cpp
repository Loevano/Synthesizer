#include "synth/audio/PiecesEngine.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace synth::audio {

namespace {

constexpr float kMinGrainSizeMs = 15.0f;
constexpr float kMaxGrainSizeMs = 1000.0f;
constexpr float kMinGrainDensityHz = 0.25f;
constexpr float kMaxGrainDensityHz = 40.0f;
constexpr std::uint32_t kMinActiveGrains = 1;
constexpr std::uint32_t kMaxActiveGrains = 32;
constexpr float kMinPitchSemitones = -24.0f;
constexpr float kMaxPitchSemitones = 24.0f;
constexpr float kMinPitchJitterCents = 0.0f;
constexpr float kMaxPitchJitterCents = 120.0f;
constexpr float kMinFeedbackAmount = 0.0f;
constexpr float kMaxFeedbackAmount = 0.97f;
constexpr float kMinHighPassHz = 20.0f;
constexpr float kMaxHighPassHz = 12000.0f;
constexpr float kTwoPi = 6.28318530717958647692f;

float clampUnit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

}  // namespace

void PiecesEngine::prepare(double sampleRate, std::uint32_t outputChannels) {
    if (sampleRate > 0.0) {
        sampleRate_ = sampleRate;
    }

    outputChannels_ = std::max<std::uint32_t>(1, outputChannels);
    feedbackLowPass_.prepare(sampleRate_);
    feedbackLowPass_.setResonance(0.707f);
    setFeedbackLowPassHz(feedbackLowPassHz_);
    updateHighPassCoefficients();
    rebuildSourceBuffer();
    feedbackBuffer_.assign(sourceBuffer_.size(), 0.0f);
    grains_.assign(static_cast<std::size_t>(activeGrains_), {});
    syncCenterOutOrder();
    reset();
}

void PiecesEngine::reset() {
    std::fill(feedbackBuffer_.begin(), feedbackBuffer_.end(), 0.0f);
    for (auto& grain : grains_) {
        grain = {};
    }
    feedbackWriteIndex_ = 0;
    samplesUntilNextGrain_ = 0.0;
    nextRoundRobinOutput_ = 0;
    pingPongOutput_ = 0;
    pingPongDirection_ = 1;
    sprayCenter_ = outputChannels_ > 1 ? static_cast<double>(outputChannels_ - 1) * 0.5 : 0.0;
    nextCenterOutIndex_ = 0;
    highPassPrevInput_ = 0.0f;
    highPassPrevOutput_ = 0.0f;
    feedbackLowPass_.reset();
}

void PiecesEngine::clearNotes() {
    heldNotes_.clear();
    noteVelocity_ = 1.0f;
    reset();
}

void PiecesEngine::noteOn(int noteNumber, float velocity) {
    heldNotes_.push_back(noteNumber);
    noteVelocity_ = std::clamp(velocity, 0.0f, 1.0f);
    samplesUntilNextGrain_ = 0.0;
}

void PiecesEngine::noteOff(int noteNumber) {
    const auto heldNote = std::find(heldNotes_.begin(), heldNotes_.end(), noteNumber);
    if (heldNote == heldNotes_.end()) {
        return;
    }

    heldNotes_.erase(heldNote);
    if (heldNotes_.empty()) {
        noteVelocity_ = 1.0f;
    }
}

void PiecesEngine::setGrainSizeMs(float grainSizeMs) {
    grainSizeMs_ = std::clamp(grainSizeMs, kMinGrainSizeMs, kMaxGrainSizeMs);
}

void PiecesEngine::setGrainDensityHz(float grainDensityHz) {
    grainDensityHz_ = std::clamp(grainDensityHz, kMinGrainDensityHz, kMaxGrainDensityHz);
}

void PiecesEngine::setActiveGrains(std::uint32_t activeGrains) {
    activeGrains_ = std::clamp(activeGrains, kMinActiveGrains, kMaxActiveGrains);
    grains_.resize(activeGrains_);
}

void PiecesEngine::setPosition(float position) {
    position_ = clampUnit(position);
}

void PiecesEngine::setPositionJitter(float positionJitter) {
    positionJitter_ = clampUnit(positionJitter);
}

void PiecesEngine::setPitchSemitones(float pitchSemitones) {
    pitchSemitones_ = std::clamp(pitchSemitones, kMinPitchSemitones, kMaxPitchSemitones);
}

void PiecesEngine::setPitchJitterCents(float pitchJitterCents) {
    pitchJitterCents_ = std::clamp(pitchJitterCents, kMinPitchJitterCents, kMaxPitchJitterCents);
}

void PiecesEngine::setOutputAlgorithm(app::PiecesOutputAlgorithm outputAlgorithm) {
    outputAlgorithm_ = outputAlgorithm;
}

void PiecesEngine::setOutputSpread(float outputSpread) {
    outputSpread_ = clampUnit(outputSpread);
}

void PiecesEngine::setFeedbackAmount(float feedbackAmount) {
    feedbackAmount_ = std::clamp(feedbackAmount, kMinFeedbackAmount, kMaxFeedbackAmount);
}

void PiecesEngine::setFeedbackHighPassHz(float cutoffHz) {
    feedbackHighPassHz_ = std::clamp(cutoffHz, kMinHighPassHz, kMaxHighPassHz);
    updateHighPassCoefficients();
}

void PiecesEngine::setFeedbackLowPassHz(float cutoffHz) {
    feedbackLowPassHz_ = std::clamp(cutoffHz, kMinHighPassHz, static_cast<float>(sampleRate_ * 0.45));
    feedbackLowPass_.setCutoffHz(feedbackLowPassHz_);
}

void PiecesEngine::process(float* output, std::uint32_t frames, std::uint32_t channels, float masterLevel) {
    if (output == nullptr || channels == 0 || sourceBuffer_.empty() || grains_.empty() || masterLevel <= 0.0f) {
        return;
    }

    const double spawnInterval =
        sampleRate_ / std::max(static_cast<double>(kMinGrainDensityHz), static_cast<double>(grainDensityHz_));
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        if (hasHeldNotes()) {
            samplesUntilNextGrain_ -= 1.0;
            while (samplesUntilNextGrain_ <= 0.0) {
                spawnGrain();
                samplesUntilNextGrain_ += spawnInterval;
            }
        }

        float monoSum = 0.0f;
        const std::size_t frameOffset = static_cast<std::size_t>(frame) * channels;
        for (auto& grain : grains_) {
            if (!grain.active || grain.durationSamples == 0) {
                continue;
            }

            if (grain.ageSamples >= grain.durationSamples) {
                grain.active = false;
                continue;
            }

            const float grainWindow = hannWindow(grain.ageSamples, grain.durationSamples);
            const float sourceSample = readSourceSample(grain.readPosition);
            const float sample = sourceSample * grainWindow * grain.amplitude * masterLevel;
            monoSum += sample;

            if (grain.outputIndex < channels) {
                output[frameOffset + grain.outputIndex] += sample;
            }

            grain.readPosition = wrapPosition(grain.readPosition + grain.readStep, sourceBuffer_.size());
            ++grain.ageSamples;
        }

        writeFeedbackSample(monoSum);
    }
}

float PiecesEngine::midiNoteToPlaybackRatio(int noteNumber, float pitchSemitones) {
    constexpr float kReferenceNote = 60.0f;
    return std::exp2((static_cast<float>(noteNumber) - kReferenceNote + pitchSemitones) / 12.0f);
}

float PiecesEngine::wrapUnit(float value) {
    const float wrapped = std::fmod(value, 1.0f);
    return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
}

double PiecesEngine::wrapPosition(double position, std::size_t size) {
    if (size == 0) {
        return 0.0;
    }

    const double sampleCount = static_cast<double>(size);
    position = std::fmod(position, sampleCount);
    return position < 0.0 ? position + sampleCount : position;
}

float PiecesEngine::hannWindow(std::uint32_t ageSamples, std::uint32_t durationSamples) {
    if (durationSamples <= 1) {
        return 1.0f;
    }

    const float phase = static_cast<float>(ageSamples) / static_cast<float>(durationSamples - 1);
    return 0.5f - (0.5f * std::cos(kTwoPi * phase));
}

void PiecesEngine::rebuildSourceBuffer() {
    const std::size_t sampleCount = static_cast<std::size_t>(std::max(16384.0, std::round(sampleRate_ * 2.0)));
    sourceBuffer_.assign(sampleCount, 0.0f);

    std::mt19937 generator(0x50494543u);
    std::uniform_real_distribution<float> noiseDistribution(-1.0f, 1.0f);
    for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleRate_);
        const float phase = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
        const float macro = 0.62f + (0.38f * std::sin((kTwoPi * 0.19f * t) + 0.4f));
        const float toneA = std::sin(kTwoPi * 220.0f * t);
        const float toneB = std::sin((kTwoPi * 330.0f * t) + (3.7f * std::sin(kTwoPi * 0.31f * t)));
        const float toneC = std::sin((kTwoPi * 110.0f * t) + (2.1f * std::sin(kTwoPi * 0.07f * t)));
        const float texture = noiseDistribution(generator) * (0.25f + (0.15f * std::sin(kTwoPi * 0.11f * t)));
        const float window = 0.55f + (0.45f * std::sin((kTwoPi * phase) - std::numbers::pi_v<float> * 0.5f));
        const float sample = ((0.52f * toneA) + (0.27f * toneB) + (0.17f * toneC) + (0.22f * texture * macro)) * window;
        sourceBuffer_[sampleIndex] = std::clamp(sample, -1.0f, 1.0f);
    }
}

void PiecesEngine::syncCenterOutOrder() {
    centerOutOrder_.clear();
    centerOutOrder_.reserve(outputChannels_);
    if (outputChannels_ == 0) {
        return;
    }

    const double center = outputChannels_ > 1 ? static_cast<double>(outputChannels_ - 1) * 0.5 : 0.0;
    std::vector<std::pair<double, std::uint32_t>> distances;
    distances.reserve(outputChannels_);
    for (std::uint32_t outputIndex = 0; outputIndex < outputChannels_; ++outputIndex) {
        distances.push_back({std::abs(static_cast<double>(outputIndex) - center), outputIndex});
    }

    std::stable_sort(
        distances.begin(),
        distances.end(),
        [](const auto& left, const auto& right) {
            if (left.first == right.first) {
                return left.second < right.second;
            }
            return left.first < right.first;
        });

    for (const auto& entry : distances) {
        centerOutOrder_.push_back(entry.second);
    }
}

void PiecesEngine::updateHighPassCoefficients() {
    const float clampedCutoff = std::clamp(feedbackHighPassHz_, kMinHighPassHz, static_cast<float>(sampleRate_ * 0.45));
    const float rc = 1.0f / (kTwoPi * clampedCutoff);
    const float dt = 1.0f / static_cast<float>(sampleRate_);
    highPassAlpha_ = rc / (rc + dt);
}

float PiecesEngine::processHighPassSample(float inputSample) {
    const float outputSample = highPassAlpha_ * (highPassPrevOutput_ + inputSample - highPassPrevInput_);
    highPassPrevInput_ = inputSample;
    highPassPrevOutput_ = outputSample;
    return outputSample;
}

float PiecesEngine::readSourceSample(double readPosition) const {
    if (sourceBuffer_.empty()) {
        return 0.0f;
    }

    const double wrappedPosition = wrapPosition(readPosition, sourceBuffer_.size());
    const std::size_t indexA = static_cast<std::size_t>(wrappedPosition) % sourceBuffer_.size();
    const std::size_t indexB = (indexA + 1) % sourceBuffer_.size();
    const float fraction = static_cast<float>(wrappedPosition - static_cast<double>(indexA));
    const float baseSample = sourceBuffer_[indexA] + ((sourceBuffer_[indexB] - sourceBuffer_[indexA]) * fraction);

    if (feedbackBuffer_.empty()) {
        return baseSample;
    }

    const float feedbackA = feedbackBuffer_[indexA];
    const float feedbackB = feedbackBuffer_[indexB];
    const float feedbackSample = feedbackA + ((feedbackB - feedbackA) * fraction);
    return std::clamp(baseSample + (feedbackSample * feedbackAmount_), -1.0f, 1.0f);
}

void PiecesEngine::writeFeedbackSample(float inputSample) {
    if (feedbackBuffer_.empty()) {
        return;
    }

    const float highPassed = processHighPassSample(inputSample);
    const float filtered = feedbackLowPass_.processSample(highPassed);
    const float feedbackWet = std::clamp(feedbackAmount_, kMinFeedbackAmount, kMaxFeedbackAmount);
    feedbackBuffer_[feedbackWriteIndex_] =
        std::clamp((feedbackBuffer_[feedbackWriteIndex_] * feedbackWet) + filtered, -1.0f, 1.0f);
    feedbackWriteIndex_ = (feedbackWriteIndex_ + 1) % feedbackBuffer_.size();
}

std::uint32_t PiecesEngine::chooseOutputIndex() {
    if (outputChannels_ <= 1) {
        return 0;
    }

    switch (outputAlgorithm_) {
        case app::PiecesOutputAlgorithm::Random: {
            std::uniform_int_distribution<std::uint32_t> distribution(0, outputChannels_ - 1);
            return distribution(rng_);
        }
        case app::PiecesOutputAlgorithm::PingPong: {
            const std::uint32_t selected = pingPongOutput_;
            if (outputChannels_ > 1) {
                if (pingPongDirection_ > 0) {
                    if (pingPongOutput_ + 1 >= outputChannels_) {
                        pingPongDirection_ = -1;
                        if (pingPongOutput_ > 0) {
                            --pingPongOutput_;
                        }
                    } else {
                        ++pingPongOutput_;
                    }
                } else if (pingPongOutput_ == 0) {
                    pingPongDirection_ = 1;
                    if (outputChannels_ > 1) {
                        ++pingPongOutput_;
                    }
                } else {
                    --pingPongOutput_;
                }
            }
            return selected;
        }
        case app::PiecesOutputAlgorithm::CenterOut: {
            if (centerOutOrder_.empty()) {
                return 0;
            }
            const std::uint32_t selected = centerOutOrder_[nextCenterOutIndex_ % centerOutOrder_.size()];
            nextCenterOutIndex_ = (nextCenterOutIndex_ + 1) % centerOutOrder_.size();
            return selected;
        }
        case app::PiecesOutputAlgorithm::Spray: {
            std::uniform_real_distribution<double> driftDistribution(-0.75, 0.75);
            const double maxIndex = static_cast<double>(outputChannels_ - 1);
            sprayCenter_ = std::clamp(
                sprayCenter_ + (driftDistribution(rng_) * std::max(0.25f, outputSpread_)),
                0.0,
                maxIndex);

            const double radius = std::max(0.0, static_cast<double>(outputSpread_) * maxIndex * 0.5);
            std::uniform_real_distribution<double> offsetDistribution(-radius, radius);
            const double selected = std::clamp(sprayCenter_ + offsetDistribution(rng_), 0.0, maxIndex);
            return static_cast<std::uint32_t>(std::llround(selected));
        }
        case app::PiecesOutputAlgorithm::RoundRobin:
        default: {
            const std::uint32_t selected = nextRoundRobinOutput_ % outputChannels_;
            nextRoundRobinOutput_ = (nextRoundRobinOutput_ + 1) % outputChannels_;
            return selected;
        }
    }
}

PiecesEngine::Grain* PiecesEngine::acquireGrainSlot() {
    auto inactiveGrain = std::find_if(grains_.begin(), grains_.end(), [](const Grain& grain) {
        return !grain.active;
    });
    if (inactiveGrain != grains_.end()) {
        return &(*inactiveGrain);
    }

    auto oldestGrain = std::max_element(grains_.begin(), grains_.end(), [](const Grain& left, const Grain& right) {
        return left.ageSamples < right.ageSamples;
    });
    return oldestGrain != grains_.end() ? &(*oldestGrain) : nullptr;
}

void PiecesEngine::spawnGrain() {
    Grain* grain = acquireGrainSlot();
    if (grain == nullptr || sourceBuffer_.empty()) {
        return;
    }

    std::uniform_real_distribution<float> positionDistribution(-positionJitter_, positionJitter_);
    std::uniform_real_distribution<float> pitchDistribution(-pitchJitterCents_, pitchJitterCents_);
    const float grainPosition = wrapUnit(position_ + positionDistribution(rng_));
    const float jitterSemitones = pitchDistribution(rng_) / 100.0f;
    const float playbackRatio =
        midiNoteToPlaybackRatio(currentNoteNumber(), pitchSemitones_ + jitterSemitones);
    const std::uint32_t grainDuration = std::max<std::uint32_t>(
        16,
        static_cast<std::uint32_t>(std::round(sampleRate_ * (grainSizeMs_ / 1000.0f))));

    grain->active = true;
    grain->readPosition = grainPosition * static_cast<float>(sourceBuffer_.size() - 1);
    grain->readStep = std::clamp(static_cast<double>(playbackRatio), 0.125, 8.0);
    grain->ageSamples = 0;
    grain->durationSamples = grainDuration;
    grain->amplitude = std::clamp(0.42f * noteVelocity_, 0.0f, 1.0f);
    grain->outputIndex = chooseOutputIndex();
}

bool PiecesEngine::hasHeldNotes() const {
    return !heldNotes_.empty();
}

int PiecesEngine::currentNoteNumber() const {
    return heldNotes_.empty() ? 60 : heldNotes_.back();
}

}  // namespace synth::audio
