add_library(lowpass_module SHARED LowPassModule.cpp)

target_include_directories(lowpass_module
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(lowpass_module PROPERTIES
    OUTPUT_NAME "lowpass_module"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
)
