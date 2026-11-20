cmake_minimum_required(VERSION 3.22)

# Universal Binary (Apple Silicon + Intel) auf dem Mac
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(ShakerLab VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokaler Checkout über -DJUCE_DIR=..., sonst automatisch laden
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# Eingebettete Samples
file(GLOB SHAKER_SAMPLE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/samples/*.wav")
juce_add_binary_data(ShakerSamples SOURCES ${SHAKER_SAMPLE_FILES})
set_target_properties(ShakerSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SHAKER_FORMATS VST3)
if(APPLE)
  list(APPEND SHAKER_FORMATS AU)
endif()
list(APPEND SHAKER_FORMATS Standalone)

juce_add_plugin(ShakerLab
  PRODUCT_NAME "Shaker Lab"
  COMPANY_NAME "Don Kirschbaum"
  BUNDLE_ID "com.donkirschbaum.shakerlab"
  PLUGIN_MANUFACTURER_CODE Dkrs
  PLUGIN_CODE Shk4
  FORMATS ${SHAKER_FORMATS}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "4-track shaker sequencer with polyrhythms and randomness")

set(SHAKER_CORE_SOURCES
  src/SampleLibrary.cpp
  src/Engine.cpp
  src/Generator.cpp)

target_sources(ShakerLab PRIVATE
  ${SHAKER_CORE_SOURCES}
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

set(SHAKER_DEFS
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(ShakerLab PUBLIC ${SHAKER_DEFS})

target_link_libraries(ShakerLab
  PRIVATE
    ShakerSamples
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# Offline-Test: rendert die Engine ohne Host in eine WAV-Datei
juce_add_console_app(shaker_render_test PRODUCT_NAME "shaker_render_test")
set_target_properties(shaker_render_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(shaker_render_test PRIVATE tools/render_test.cpp ${SHAKER_CORE_SOURCES})
target_compile_definitions(shaker_render_test PRIVATE ${SHAKER_DEFS} JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(shaker_render_test PRIVATE
  ShakerSamples
  juce::juce_audio_formats
  juce::juce_audio_basics
  juce::juce_recommended_config_flags)
