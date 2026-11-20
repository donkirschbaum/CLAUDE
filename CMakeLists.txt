cmake_minimum_required(VERSION 3.22)
project(Grids8 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokal per -DJUCE_DIR=/pfad/zu/JUCE oder automatisch von GitHub
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(G8_FORMATS AU VST3 Standalone)
else()
    set(G8_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Grids8
    COMPANY_NAME "donkirschbaum"
    BUNDLE_ID "com.donkirschbaum.grids8"
    PLUGIN_MANUFACTURER_CODE Dkrb
    PLUGIN_CODE Gr8d
    FORMATS ${G8_FORMATS}
    PRODUCT_NAME "Grids 8"
    DESCRIPTION "Topografischer 8-Spur Drum-Sequencer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(Grids8 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

juce_add_binary_data(Grids8Samples SOURCES
    Resources/samples/1kick.wav
    Resources/samples/2clap.wav
    Resources/samples/3hat.wav
    Resources/samples/4hat.wav
    Resources/samples/5snare.wav
    Resources/samples/6tom1.wav
    Resources/samples/7tom2.wav
    Resources/samples/8conga.wav)

target_compile_definitions(Grids8 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Grids8
    PRIVATE
        Grids8Samples
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(GRIDS8_TESTS "Rauchtest bauen" OFF)
if(GRIDS8_TESTS)
    add_executable(Grids8Smoke tests/Smoke.cpp)
    target_include_directories(Grids8Smoke PRIVATE $<TARGET_PROPERTY:Grids8,INCLUDE_DIRECTORIES>)
    target_compile_definitions(Grids8Smoke PRIVATE $<TARGET_PROPERTY:Grids8,COMPILE_DEFINITIONS> JUCE_STANDALONE_APPLICATION=0)
    target_link_libraries(Grids8Smoke PRIVATE Grids8)
endif()
