#pragma once

#include <cstdint>

namespace synth::audio {

enum class SamplePlaybackMode : std::uint8_t {
    Gate,
    OneShot,
    Loop,
};

}  // namespace synth::audio
