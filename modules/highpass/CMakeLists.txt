add_library(highpass_module SHARED HighPassModule.cpp)

target_include_directories(highpass_module
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(highpass_module PROPERTIES
    OUTPUT_NAME "highpass_module"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
)
