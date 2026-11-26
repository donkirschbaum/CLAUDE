cmake_minimum_required(VERSION 3.22)

project(PolyRhythm32 VERSION 1.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: either a local checkout (-DJUCE_DIR=/pfad/zu/JUCE) or fetched automatically
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(PR_FORMATS AU VST3 Standalone)
else()
    set(PR_FORMATS VST3 Standalone)
endif()

juce_add_plugin(PolyRhythm32
    COMPANY_NAME               "donkirschbaum"
    BUNDLE_ID                  "com.donkirschbaum.polyrhythm32"
    PLUGIN_MANUFACTURER_CODE   Dkrb
    PLUGIN_CODE                Pr32
    FORMATS                    ${PR_FORMATS}
    PRODUCT_NAME               "PolyRhythm32"
    DESCRIPTION                "Polyrhythmischer 6-Spur MIDI-Sequencer"
    IS_SYNTH                   TRUE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          TRUE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            Instrument Generator
    AU_MAIN_TYPE               kAudioUnitType_MusicDevice)

target_sources(PolyRhythm32 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PolyRhythm32 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PolyRhythm32
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
