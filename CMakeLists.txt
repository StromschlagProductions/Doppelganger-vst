cmake_minimum_required(VERSION 3.22)
# Versionsnummer steht in der Datei VERSION (wird bei jeder Aenderung hochgezaehlt)
file(STRINGS VERSION DOP_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
project(Doppelganger VERSION ${DOP_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Laufzeitbibliothek fest einbinden: das Plugin braucht dann kein VC++-Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(DOPPELGANGER_JUCE_PATH "" CACHE PATH "Lokaler JUCE-Ordner (leer = automatisch laden)")
set(DOPPELGANGER_FORMATS "VST3" CACHE STRING "Plugin-Formate")
option(DOPPELGANGER_TESTS "DSP/Transport-Tests ohne JUCE bauen" OFF)
option(DOPPELGANGER_PLUGIN_TESTS "Prozessor-/State-Tests mit JUCE bauen" OFF)
set(DOPPELGANGER_SANITIZER "" CACHE STRING "address,undefined oder thread (GCC/Clang)")
if(DOPPELGANGER_SANITIZER AND NOT MSVC)
    add_compile_options(-fsanitize=${DOPPELGANGER_SANITIZER} -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${DOPPELGANGER_SANITIZER})
endif()

if(DOPPELGANGER_TESTS OR DOPPELGANGER_PLUGIN_TESTS)
    enable_testing()
    add_executable(benchmark_engine tests/benchmark_engine.cpp)
    add_executable(loudness_reference tests/loudness_reference.cpp)
    find_package(Threads REQUIRED)
    foreach(name engine learn regression transport fixture calibration)
        add_executable(test_${name} tests/test_${name}.cpp)
        target_link_libraries(test_${name} PRIVATE Threads::Threads)
    endforeach()
    add_test(NAME dsp_regression COMMAND test_regression)
    add_test(NAME calibration_capture COMMAND test_calibration)
    add_test(NAME concurrent_transport COMMAND test_transport)
    add_test(NAME generate_fixture COMMAND test_fixture ${CMAKE_CURRENT_BINARY_DIR}/voice.f32)
    set_tests_properties(generate_fixture PROPERTIES FIXTURES_SETUP voice)
    add_test(NAME engine_cli COMMAND test_engine ${CMAKE_CURRENT_BINARY_DIR}/voice.f32 ${CMAKE_CURRENT_BINARY_DIR}/processed.f32 48000)
    add_test(NAME learn_cli COMMAND test_learn ${CMAKE_CURRENT_BINARY_DIR}/voice.f32 48000)
    set_tests_properties(engine_cli learn_cli PROPERTIES FIXTURES_REQUIRED voice)
    add_test(NAME engine_missing_input COMMAND test_engine missing.f32 unused.f32 48000)
    add_test(NAME learn_missing_arguments COMMAND test_learn)
    set_tests_properties(engine_missing_input learn_missing_arguments PROPERTIES WILL_FAIL TRUE)
endif()
if(DOPPELGANGER_TESTS AND NOT DOPPELGANGER_PLUGIN_TESTS)
    return()
endif()

if(DOPPELGANGER_JUCE_PATH)
    add_subdirectory(${DOPPELGANGER_JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(DoppelgangerAssets SOURCES Assets/logo.png)

juce_add_plugin(Doppelganger
    COMPANY_NAME "Stromschlag Audio"
    PLUGIN_MANUFACTURER_CODE Strm
    PLUGIN_CODE Dpgg
    FORMATS ${DOPPELGANGER_FORMATS}
    PRODUCT_NAME "Doppelganger"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Doppelganger PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Doppelganger PUBLIC
    PLUGIN_VERSION_TEXT="${DOP_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(Doppelganger PRIVATE /utf-8)
endif()

target_link_libraries(Doppelganger
    PRIVATE
        DoppelgangerAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(DOPPELGANGER_PLUGIN_TESTS)
    juce_add_console_app(test_processor PRODUCT_NAME "Doppelganger processor tests")
    target_sources(test_processor PRIVATE tests/test_processor.cpp)
    target_link_libraries(test_processor PRIVATE Doppelganger)
    target_include_directories(test_processor PRIVATE Source $<TARGET_PROPERTY:Doppelganger,INCLUDE_DIRECTORIES>)
    target_compile_definitions(test_processor PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    add_test(NAME processor_integration COMMAND test_processor)
    add_test(NAME processor_concurrency COMMAND test_processor --concurrency)
    set_tests_properties(processor_integration PROPERTIES TIMEOUT 120)
    set_tests_properties(processor_concurrency PROPERTIES TIMEOUT 120)
endif()

# Real editor snapshots, excluded from normal plugin/release builds.
juce_add_console_app(DoppelgangerScreenshot PRODUCT_NAME "Doppelganger screenshot")
target_sources(DoppelgangerScreenshot PRIVATE tools/screenshot.cpp)
target_include_directories(DoppelgangerScreenshot PRIVATE Source $<TARGET_PROPERTY:Doppelganger,INCLUDE_DIRECTORIES>)
target_compile_definitions(DoppelgangerScreenshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PLUGIN_VERSION_TEXT="${DOP_VERSION}")
target_link_libraries(DoppelgangerScreenshot PRIVATE Doppelganger)
set_target_properties(DoppelgangerScreenshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
