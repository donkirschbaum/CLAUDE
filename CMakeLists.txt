cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(PulseForge VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PULSEFORGE_BUILD_TESTS "Build the offline DSP test" OFF)

set(JUCE_PATH "" CACHE PATH "Local JUCE checkout (optional, otherwise downloaded)")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(PF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PF_FORMATS AU)
endif()

juce_add_plugin(PulseForge
    COMPANY_NAME "donkirschbaum"
    COMPANY_WEBSITE "https://github.com/donkirschbaum/CLAUDE/tree/pulseforge"
    DESCRIPTION "Rhythmic step-sequenced multi-effect"
    BUNDLE_ID "com.donkirschbaum.pulseforge"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Dkbm
    PLUGIN_CODE Plfg
    FORMATS ${PF_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Modulation Delay
    PRODUCT_NAME "PulseForge")

target_sources(PulseForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PulseForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PulseForge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- tests
if(PULSEFORGE_BUILD_TESTS)
    juce_add_console_app(pulseforge_test PRODUCT_NAME "PulseForgeTest")
    target_sources(pulseforge_test PRIVATE tests/OfflineTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(pulseforge_test PRIVATE Source)
    target_compile_definitions(pulseforge_test PRIVATE
        JucePlugin_Name="PulseForge" JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(pulseforge_test PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
