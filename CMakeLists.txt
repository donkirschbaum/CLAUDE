cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(LoopTape VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LOOPTAPE_BUILD_TESTS "Build the offline DSP test" OFF)

# ---------------------------------------------------------------- JUCE
set(JUCE_PATH "" CACHE PATH "Local JUCE checkout (optional, otherwise downloaded)")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- plugin
set(LOOPTAPE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LOOPTAPE_FORMATS AU)
endif()

juce_add_plugin(LoopTape
    COMPANY_NAME "donkirschbaum"
    COMPANY_WEBSITE "https://github.com/donkirschbaum/CLAUDE/tree/looptape"
    BUNDLE_ID "com.donkirschbaum.looptape"
    PLUGIN_MANUFACTURER_CODE Dkbm
    PLUGIN_CODE Ltp1
    FORMATS ${LOOPTAPE_FORMATS}
    PRODUCT_NAME "LoopTape"
    DESCRIPTION "Tape echo with multi-layer looper"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LoopTape PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(LoopTape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LoopTape
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- tests
if(LOOPTAPE_BUILD_TESTS)
    add_executable(looptape_test tests/OfflineTest.cpp)
    target_include_directories(looptape_test PRIVATE Source)

    # the real processor driven by a simulated host transport
    juce_add_console_app(looptape_hosttest PRODUCT_NAME "LoopTapeHostTest")
    target_sources(looptape_hosttest PRIVATE tests/HostSyncTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(looptape_hosttest PRIVATE
        JucePlugin_Name="LoopTape" JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(looptape_hosttest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
