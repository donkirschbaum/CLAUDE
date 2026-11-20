cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(MIKROKORN VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokal per -DJUCE_DIR=... oder automatisch von GitHub
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(Mikrokorn
    COMPANY_NAME "donkirschbaum"
    BUNDLE_ID com.donkirschbaum.mikrokorn
    PRODUCT_NAME "MIKROKORN"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Dkbm
    PLUGIN_CODE Mkrn
    FORMATS AU VST3 Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Sampler)

# Werks-Samples + Presets (werden vorher von tools/gen_assets.js erzeugt)
file(GLOB MIKROKORN_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/assets/samples/*.wav")
if(NOT MIKROKORN_SAMPLES)
    message(FATAL_ERROR "Keine Samples gefunden. Bitte zuerst 'node tools/gen_assets.js' ausfuehren.")
endif()
juce_add_binary_data(MikrokornAssets
    SOURCES ${MIKROKORN_SAMPLES} "${CMAKE_CURRENT_SOURCE_DIR}/assets/presets.json")

target_sources(Mikrokorn PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SampleLibrary.cpp)

target_compile_definitions(Mikrokorn PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(Mikrokorn
    PRIVATE
        MikrokornAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(MIKROKORN_TESTS "Smoke-Test bauen" OFF)
if(MIKROKORN_TESTS)
    add_executable(mikrokorn_smoke tests/smoke_test.cpp)
    target_link_libraries(mikrokorn_smoke PRIVATE Mikrokorn)
    target_include_directories(mikrokorn_smoke PRIVATE $<TARGET_PROPERTY:Mikrokorn,INCLUDE_DIRECTORIES>)
    target_compile_definitions(mikrokorn_smoke PRIVATE $<TARGET_PROPERTY:Mikrokorn,COMPILE_DEFINITIONS>)
endif()
