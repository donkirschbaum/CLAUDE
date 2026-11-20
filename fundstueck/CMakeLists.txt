cmake_minimum_required(VERSION 3.22)
project(Fundstueck VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(FS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FS_FORMATS AU)
endif()

juce_add_plugin(Fundstueck
    COMPANY_NAME "Fundstueck"
    BUNDLE_ID "com.donkirschbaum.fundstueck"
    PRODUCT_NAME "Fundstueck"
    PLUGIN_NAME "Fundstueck"
    DESCRIPTION "Alltagsgeraeusche zu Beats"
    PLUGIN_MANUFACTURER_CODE Fdsk
    PLUGIN_CODE Fst1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Fundstueck nimmt Geraeusche ueber das Mikrofon auf."
    FORMATS ${FS_FORMATS})

target_sources(Fundstueck PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Fundstueck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Fundstueck
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(FS_TESTS "Build offline render test" OFF)
if(FS_TESTS)
    add_executable(fs_render_test tests/render_test.cpp)
    target_link_libraries(fs_render_test PRIVATE Fundstueck)
    target_include_directories(fs_render_test PRIVATE $<TARGET_PROPERTY:Fundstueck,INCLUDE_DIRECTORIES>)
    target_compile_definitions(fs_render_test PRIVATE $<TARGET_PROPERTY:Fundstueck,COMPILE_DEFINITIONS>)
endif()
