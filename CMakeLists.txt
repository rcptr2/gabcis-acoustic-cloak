cmake_minimum_required(VERSION 3.22)

project(AcousticCloak VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(ACOUSTICCLOAK_FORMATS VST3 AU Standalone)
else()
    set(ACOUSTICCLOAK_FORMATS VST3 Standalone)
endif()

juce_add_plugin(AcousticCloakPlugin
    PRODUCT_NAME "Acoustic Cloak"
    COMPANY_NAME "Gabor Tomori"
    BUNDLE_ID "com.gabortomori.acousticcloak"
    PLUGIN_MANUFACTURER_CODE Gtmr
    PLUGIN_CODE Aclk
    FORMATS ${ACOUSTICCLOAK_FORMATS}
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST2_CATEGORY kPlugCategEffect
)

target_sources(AcousticCloakPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/BandIsolator.cpp
        Source/ComplexResonator.cpp
        Source/ResonatorBank.cpp
        Source/AllpassHilbertNetwork.cpp
        Source/PhaseRotator.cpp
        Source/PhaseCorrelationAnalyzer.cpp
        Source/AcousticCloakLookAndFeel.cpp
        Source/PhaseRadarComponent.cpp
        Source/AboutPanel.cpp
)

target_compile_definitions(AcousticCloakPlugin
    PUBLIC
        JUCE_ALSA=1
        JUCE_DIRECTSOUND=1
        JUCE_PLUGINHOST_VST3=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AcousticCloakPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(ACOUSTICCLOAK_BUILD_TESTS "Build the Acoustic Cloak unit tests" ON)

if(ACOUSTICCLOAK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
