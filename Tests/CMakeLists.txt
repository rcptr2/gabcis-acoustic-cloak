FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

juce_add_console_app(AcousticCloakTests
    PRODUCT_NAME "AcousticCloakTests"
)

target_sources(AcousticCloakTests
    PRIVATE
        ComplexResonatorTests.cpp
        BandIsolatorTests.cpp
        AllpassHilbertNetworkTests.cpp
        PhaseRotatorTests.cpp
        PhaseCorrelationAnalyzerTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/ComplexResonator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/ResonatorBank.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/BandIsolator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/AllpassHilbertNetwork.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/PhaseRotator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/PhaseCorrelationAnalyzer.cpp
)

target_include_directories(AcousticCloakTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source
)

target_compile_definitions(AcousticCloakTests
    PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

target_link_libraries(AcousticCloakTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_dsp
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

include(CTest)
include(Catch)
catch_discover_tests(AcousticCloakTests)

# Standalone load generator for real CPU profiling (Phase 5) -- not a
# Catch2 test, no discovery/registration needed. Build Release for
# meaningful numbers; see LoadTest.cpp's own header comment.
juce_add_console_app(AcousticCloakLoadTest
    PRODUCT_NAME "AcousticCloakLoadTest"
)

target_sources(AcousticCloakLoadTest
    PRIVATE
        LoadTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/BandIsolator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/ComplexResonator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/ResonatorBank.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/AllpassHilbertNetwork.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/PhaseRotator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/PhaseCorrelationAnalyzer.cpp
)

target_include_directories(AcousticCloakLoadTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source
)

target_compile_definitions(AcousticCloakLoadTest
    PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

target_link_libraries(AcousticCloakLoadTest
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
