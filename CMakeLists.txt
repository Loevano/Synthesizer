cmake_minimum_required(VERSION 3.21)
project(Synthesizer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYNTH_BUILD_MODULES "Build swappable DSP modules" ON)

add_library(synth_core
    src/core/Logger.cpp
    src/dsp/Oscillator.cpp
    src/dsp/ModuleHost.cpp
    src/audio/SynthEngine.cpp
    src/audio/AudioEngine.cpp
    src/interfaces/AudioDriverCoreAudio.cpp
    src/midi/MidiInput.cpp
    src/osc/OscServer.cpp
)

target_include_directories(synth_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if(APPLE)
    target_compile_definitions(synth_core PRIVATE SYNTH_PLATFORM_MACOS=1)
    target_link_libraries(synth_core
        PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework CoreMIDI"
            "-framework CoreFoundation"
    )
endif()

add_executable(synth_host src/main.cpp)
target_link_libraries(synth_host PRIVATE synth_core)

if(SYNTH_BUILD_MODULES)
    add_subdirectory(modules/lowpass)
    add_subdirectory(modules/highpass)
endif()

include(CTest)
