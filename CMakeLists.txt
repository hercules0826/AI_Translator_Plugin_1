cmake_minimum_required(VERSION 3.22)
project(WhisperFreeWin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Point this to your JUCE root (or set env var JUCE_DIR)
if(NOT DEFINED JUCE_DIR)
  set(JUCE_DIR "$ENV{JUCE_DIR}")
endif()
if(NOT JUCE_DIR)
  message(FATAL_ERROR "Please set JUCE_DIR to your JUCE folder")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

# ===== whisper.cpp =====
# Clone to third_party/whisper.cpp (v1.7.1)
set(WHISPER_DIR ${CMAKE_SOURCE_DIR}/Source/third_party/whisper.cpp)
if(NOT EXISTS "${WHISPER_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "whisper.cpp not found at ${WHISPER_DIR}. Clone it there.")
endif()

# Turn off examples/tests to speed up build
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${WHISPER_DIR} whisper_build)  # defines target: whisper

target_link_libraries(WhisperFreeWin PRIVATE whispercpp)

# On Windows/MSVC you may want to disable AVX if your target machines are old:
# add_definitions(-DGGML_NO_AVX)

juce_add_plugin(WhisperFreeWin
    COMPANY_NAME "Hercules"
    BUNDLE_ID "com.hercules.whisperfreewin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE HCLE
    PLUGIN_CODE WFWN
    FORMATS VST3
    PRODUCT_NAME "WhisperFreeWin"
    VST3_CATEGORIES         "Fx"
    VST3_SUBCATEGORY        "Fx"
)

target_sources(WhisperFreeWin PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/WhisperEngine.h
    Source/WhisperEngine.cpp
    Source/WhisperThread.h
)

target_compile_definitions(WhisperFreeWin PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0
    JUCE_DISABLE_ASSERTIONS=1  # calmer debugging in hosts
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(WhisperFreeWin PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_gui_extra
    whisper          # target from whisper.cpp's CMake
)

# Ensure VST3 is 64-bit
set_target_properties(WhisperFreeWin PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)

add_definitions(-DGGML_NO_AVX -DGGML_NO_AVX2)

set_property(GLOBAL PROPERTY USE_FOLDERS YES)
