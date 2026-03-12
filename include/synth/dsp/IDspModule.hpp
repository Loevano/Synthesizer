#pragma once

#include <string_view>

namespace synth::dsp {

class IDspModule {
public:
    virtual ~IDspModule() = default;

    virtual void prepare(double sampleRate) = 0;
    virtual float processSample(float input) = 0;
    virtual void setParameter(std::string_view name, float value) = 0;
};

using CreateModuleFn = IDspModule* (*)();
using DestroyModuleFn = void (*)(IDspModule*);
using GetModuleNameFn = const char* (*)();

inline constexpr const char* kCreateModuleSymbol = "createModule";
inline constexpr const char* kDestroyModuleSymbol = "destroyModule";
inline constexpr const char* kGetModuleNameSymbol = "getModuleName";

}  // namespace synth::dsp
