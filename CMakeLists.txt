cmake_minimum_required(VERSION 3.22)
project(ShruthiXT VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ShruthiXT
  COMPANY_NAME "Shruthi Emulation"
  BUNDLE_ID com.shruthiemu.shruthixt
  PLUGIN_MANUFACTURER_CODE Shem
  PLUGIN_CODE Shxt
  FORMATS AU VST3 Standalone
  PRODUCT_NAME "Shruthi XT"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_WEB_BROWSER TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Synth)

juce_add_binary_data(ShruthiXTData SOURCES Resources/index.html)

target_sources(ShruthiXT PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/ShruthiEngine.cpp
  Source/ShruthiBank.cpp)

target_include_directories(ShruthiXT PRIVATE Source)

target_compile_definitions(ShruthiXT PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ShruthiXT
  PRIVATE ShruthiXTData juce::juce_audio_utils juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
