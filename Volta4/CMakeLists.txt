cmake_minimum_required(VERSION 3.22)
project(Volta4 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Lokale JUCE-Kopie optional über -DJUCE_DIR=/pfad/zu/JUCE, sonst automatisch laden
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

if(NOT DEFINED VOLTA_FORMATS)
  set(VOLTA_FORMATS AU VST3 Standalone)
endif()

juce_add_plugin(Volta4
  COMPANY_NAME "donkirschbaum"
  BUNDLE_ID com.donkirschbaum.volta4
  PLUGIN_MANUFACTURER_CODE Dnkb
  PLUGIN_CODE Vlt4
  FORMATS ${VOLTA_FORMATS}
  PRODUCT_NAME "VOLTA-4"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_WEB_BROWSER TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Drum)

juce_add_binary_data(Volta4Data SOURCES Resources/index.html)

target_sources(Volta4 PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/VoltaEngine.cpp)

target_include_directories(Volta4 PRIVATE Source)

target_compile_definitions(Volta4 PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Volta4
  PRIVATE Volta4Data juce::juce_audio_utils juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
