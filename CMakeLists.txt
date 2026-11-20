cmake_minimum_required(VERSION 3.22)

# Universal Binary (Apple Silicon + Intel), ab macOS 11
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimale macOS-Version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Zielarchitekturen")

project(DroneAcht VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokaler Pfad über -DJUCE_DIR=..., sonst automatisch von GitHub laden
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
    set(DRONE_FORMATS AU VST3 Standalone CACHE STRING "Plugin-Formate")
else()
    set(DRONE_FORMATS VST3 Standalone CACHE STRING "Plugin-Formate")
endif()

juce_add_plugin(DroneAcht
    PRODUCT_NAME                "Drone Acht"
    COMPANY_NAME                "Drone Acht"
    BUNDLE_ID                   com.droneacht.synth
    PLUGIN_MANUFACTURER_CODE    Dach
    PLUGIN_CODE                 Drn8
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    FORMATS                     ${DRONE_FORMATS})

target_sources(DroneAcht PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DroneAcht PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DroneAcht
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
