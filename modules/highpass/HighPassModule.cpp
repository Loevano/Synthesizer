#include "HighPassModule.hpp"

#include <algorithm>
#include <cmath>

namespace synth::modules {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void HighPassModule::prepare(double sampleRate) {
    if (sampleRate > 0.0) {
        sampleRate_ = sampleRate;
    }
}

float HighPassModule::processSample(float input) {
    const float normalizedCutoff = std::clamp(cutoff_, 20.0f, 20000.0f);
    const float x = std::exp(-2.0f * kPi * normalizedCutoff / static_cast<float>(sampleRate_));
    const float alpha = 1.0f - x;

    state_ = state_ + alpha * (input - state_);
    return input - state_;
}

void HighPassModule::setParameter(std::string_view name, float value) {
    if (name == "cutoff") {
        cutoff_ = std::clamp(value, 20.0f, 20000.0f);
    }
}

}  // namespace synth::modules

extern "C" synth::dsp::IDspModule* createModule() {
    return new synth::modules::HighPassModule();
}

extern "C" void destroyModule(synth::dsp::IDspModule* module) {
    delete module;
}

extern "C" const char* getModuleName() {
    return "HighPass";
}
