cmake_minimum_required(VERSION 3.22)
project(PartialEditor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

# ── JUCE ─────────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Catch2 ───────────────────────────────────────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── Loris static library ──────────────────────────────────────────────────────
set(LORIS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/loris/src")

file(GLOB LORIS_SOURCES "${LORIS_SRC}/*.C" "${LORIS_SRC}/*.cpp")
list(FILTER LORIS_SOURCES EXCLUDE REGEX ".*_pi\\.C$")  # Python interface shims

set(LORIS_C_SOURCES "${LORIS_SRC}/fftsg.c")  # plain C file; keep as C

# Force CMake to compile uppercase .C files as C++
set_source_files_properties(${LORIS_SOURCES} PROPERTIES LANGUAGE CXX)

add_library(loris_static STATIC ${LORIS_SOURCES} ${LORIS_C_SOURCES})
target_include_directories(loris_static PUBLIC "${LORIS_SRC}")
target_compile_definitions(loris_static PUBLIC
    HAVE_FFTW3_H=0
    HAVE_FFTW_H=0
    HAVE_M_PI=1
    HAVE_ISFINITE=1
)
set_target_properties(loris_static PROPERTIES CXX_STANDARD 14)
target_compile_options(loris_static PRIVATE -w)  # suppress vendored-code warnings

# ── Main application ──────────────────────────────────────────────────────────
juce_add_gui_app(PartialEditor
    PRODUCT_NAME  "PartialEditor"
    VERSION       "0.1.0"
    BUNDLE_ID     "com.ideocentric.PartialEditor"
    COMPANY_NAME  "Ideocentric"
)

# Generates JuceHeader.h in the artefacts dir — must be called explicitly in JUCE 8
juce_generate_juce_header(PartialEditor)

target_sources(PartialEditor PRIVATE
    Source/Main.cpp
    Source/Analysis/PartialData.cpp
    Source/Analysis/Analyzer.cpp
    Source/Synthesis/PartialSynth.cpp
    Source/Model/Project.cpp
    Source/Model/Selection.cpp
    Source/Model/ReductionController.cpp
    Source/Model/EditHistory.cpp
    Source/UI/MainComponent.cpp
    Source/UI/PartialView.cpp
    Source/UI/ReductionPanel.cpp
    Source/UI/TransportBar.cpp
    Source/UI/InspectorPanel.cpp
    Source/Export/CsoundExporter.cpp
    Source/Export/MidiExporter.cpp
    Source/Export/SdifExporter.cpp
    Source/Export/JsonExporter.cpp
)

target_include_directories(PartialEditor PRIVATE Source)

target_compile_definitions(PartialEditor
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PartialEditor,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PartialEditor,JUCE_VERSION>"
)

target_link_libraries(PartialEditor
    PRIVATE
        loris_static
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ── Tests ─────────────────────────────────────────────────────────────────────
juce_add_console_app(PartialEditorTests)
juce_generate_juce_header(PartialEditorTests)

target_sources(PartialEditorTests PRIVATE
    Tests/PartialDataTests.cpp
    Source/Analysis/PartialData.cpp
)

target_include_directories(PartialEditorTests PRIVATE Source)

target_compile_definitions(PartialEditorTests
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(PartialEditorTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_data_structures
        juce::juce_graphics
)

include(CTest)
include(Catch)
catch_discover_tests(PartialEditorTests)