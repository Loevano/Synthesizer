cmake_minimum_required(VERSION 3.21)
project(Synthesizer VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(synth_core
    src/core/CrashDiagnostics.cpp
    src/core/Logger.cpp
    src/dsp/Envelope.cpp
    src/dsp/ThreeBandEq.cpp
    src/dsp/LowPassFilter.cpp
    src/dsp/Oscillator.cpp
    src/dsp/DelayLine.cpp
    src/dsp/Chorus.cpp
    src/dsp/Lfo.cpp
    src/audio/Voice.cpp
    src/audio/TestSynth.cpp
    src/audio/Synth.cpp
    src/graph/RobinSourceNode.cpp
    src/graph/TestSourceNode.cpp
    src/graph/FxRackNode.cpp
    src/graph/OutputMixerNode.cpp
    src/graph/LiveGraph.cpp
    src/app/SynthController.cpp
    src/io/MidiInput.cpp
    src/io/OscServer.cpp
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
            "-framework CoreFoundation"
            "-framework CoreMIDI"
    )
endif()

add_executable(synth_host src/main.cpp)
target_link_libraries(synth_host PRIVATE synth_core)

set(web_assets
    src/ui_web/index.html
    src/ui_web/styles.css
    src/ui_web/app.js
)

set_source_files_properties(${web_assets}
    PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/web"
)

add_executable(Synthesizer MACOSX_BUNDLE
    src/platform/macos/AppMain.mm
    ${web_assets}
)

target_include_directories(Synthesizer PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Synthesizer PRIVATE synth_core)

if(APPLE)
    target_compile_options(Synthesizer PRIVATE "-fobjc-arc")
    set_target_properties(Synthesizer PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.loevano.synthesizer"
        MACOSX_BUNDLE_BUNDLE_NAME "Synthesizer"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/resources/Info.plist.in
    )
    target_link_libraries(Synthesizer
        PRIVATE
            "-framework Cocoa"
            "-framework WebKit"
    )
endif()
