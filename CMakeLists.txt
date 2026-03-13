cmake_minimum_required(VERSION 3.21)
project(Synthesizer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(synth_core
    src/core/Logger.cpp
    src/dsp/Oscillator.cpp
    src/audio/Voice.cpp
    src/audio/Synth.cpp
    src/interfaces/AudioDriverCoreAudio.cpp
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
    )
endif()

add_executable(synth_host src/main.cpp)
target_link_libraries(synth_host PRIVATE synth_core)
