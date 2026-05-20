cmake_minimum_required(VERSION 3.22)
project(PartialFKR VERSION 0.1.0)

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
# ── Platform-specific application icon ───────────────────────────────────────
if(APPLE)
    set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/PartialFKR.icns")
elseif(WIN32)
    set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/PartialFKR.ico")
else()
    set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/PartialFKR_512.png")
endif()

juce_add_gui_app(PartialFKR
    PRODUCT_NAME  "PartialFKR"
    VERSION       "0.1.0"
    BUNDLE_ID     "com.ideocentric.PartialFKR"
    COMPANY_NAME  "ideocentric"
    ICON_BIG      "${_app_icon}"
)

# Generates JuceHeader.h in the artefacts dir — must be called explicitly in JUCE 8
juce_generate_juce_header(PartialFKR)

# ── macOS: .pfkr file association (post-build plist additions) ────────────────
if(APPLE)
    set(_plist "$<TARGET_BUNDLE_DIR:PartialFKR>/Contents/Info.plist")
    add_custom_command(TARGET PartialFKR POST_BUILD
        COMMENT "Adding .pfkr file association to Info.plist"
        # Remove any existing entries from a previous build to avoid duplicates
        COMMAND /usr/libexec/PlistBuddy -c "Delete :CFBundleDocumentTypes"   "${_plist}" 2>/dev/null || true
        COMMAND /usr/libexec/PlistBuddy -c "Delete :UTExportedTypeDeclarations" "${_plist}" 2>/dev/null || true
        # CFBundleDocumentTypes — registers .pfkr with Finder / Open With
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes array"                                              "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0 dict"                                             "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeName string 'PartialFKR Project'"     "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeRole string Editor"                  "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:LSHandlerRank string Owner"                      "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:LSItemContentTypes array"                        "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:LSItemContentTypes:0 string com.ideocentric.pfkr" "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions array"                    "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions:0 string pfkr"            "${_plist}"
        # UTExportedTypeDeclarations — defines the com.ideocentric.pfkr UTI
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations array"                                        "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0 dict"                                       "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeIdentifier string com.ideocentric.pfkr" "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeDescription string 'PartialFKR Project'" "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeConformsTo array"                     "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeConformsTo:0 string public.data"      "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeConformsTo:1 string public.json"      "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeTagSpecification dict"                "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeTagSpecification:public.filename-extension array" "${_plist}"
        COMMAND /usr/libexec/PlistBuddy -c "Add :UTExportedTypeDeclarations:0:UTTypeTagSpecification:public.filename-extension:0 string pfkr" "${_plist}"
        # juceaide creates a stub Icon.icns when given a pre-built .icns — overwrite it with the real file
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/PartialFKR.icns"
            "$<TARGET_BUNDLE_DIR:PartialFKR>/Contents/Resources/Icon.icns"
    )

    # ── Code signing (opt-in: cmake -DPFKR_CODESIGN=ON) ──────────────────────
    # Runs after plist and icon steps so the signature covers the final bundle.
    option(PFKR_CODESIGN "Sign the app bundle with Developer ID (requires certificate in keychain)" OFF)
    if(PFKR_CODESIGN)
        set(PFKR_SIGN_IDENTITY
            "Developer ID Application: Matthew Comeione (3722A5T4AG)"
            CACHE STRING "Codesigning identity as shown in Keychain Access")
        set(PFKR_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/MacOS/PartialFKR.entitlements")

        add_custom_command(TARGET PartialFKR POST_BUILD
            COMMENT "Code-signing PartialFKR.app"
            COMMAND codesign
                --force
                --deep
                --verify
                --timestamp
                --options=runtime
                --entitlements "${PFKR_ENTITLEMENTS}"
                --sign "${PFKR_SIGN_IDENTITY}"
                "$<TARGET_BUNDLE_DIR:PartialFKR>"
            VERBATIM
        )
    endif()
endif()

target_sources(PartialFKR PRIVATE
    Source/Main.cpp
    Source/Analysis/PartialData.cpp
    Source/Analysis/Analyzer.cpp
    Source/Synthesis/PartialSynth.cpp
    Source/Model/Project.cpp
    Source/Model/Selection.cpp
    Source/Model/ReductionController.cpp
    Source/Model/AmplitudeOps.cpp
    Source/Model/EditHistory.cpp
    Source/Model/PfkrFormat.cpp
    Source/UI/Juce2DRenderer.cpp
    Source/UI/LevelMeter.cpp
    Source/UI/AboutComponent.cpp
    Source/UI/MacProxyIcon.cpp
    Source/UI/MacWindowHelpers.cpp
    Source/UI/MainComponent.cpp
    Source/UI/PartialView.cpp
    Source/UI/ReductionPanel.cpp
    Source/UI/TransportBar.cpp
    Source/UI/InspectorPanel.cpp
    Source/Export/CsoundExporter.cpp
    Source/Export/SuperColliderExporter.cpp
    Source/Export/MidiExporter.cpp
    Source/Export/MidiPackageExporter.cpp
    Source/Export/SdifExporter.cpp
    Source/Export/JsonExporter.cpp
)

if(APPLE)
    target_sources(PartialFKR PRIVATE
        Source/UI/MacProxyIcon.mm
        Source/UI/MacWindowHelpers.mm
    )
endif()

target_include_directories(PartialFKR PRIVATE Source)

target_compile_definitions(PartialFKR
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PartialFKR,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PartialFKR,JUCE_VERSION>"
)

target_link_libraries(PartialFKR
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
juce_add_console_app(PartialFKRTests)
juce_generate_juce_header(PartialFKRTests)

target_sources(PartialFKRTests PRIVATE
    Tests/PartialDataTests.cpp
    Tests/PfkrFormatTests.cpp
    Tests/ProjectTests.cpp
    Tests/SelectionTests.cpp
    Tests/EditHistoryTests.cpp
    Source/Analysis/PartialData.cpp
    Source/Model/EditHistory.cpp
    Source/Model/PfkrFormat.cpp
    Source/Model/Project.cpp
    Source/Model/Selection.cpp
)

target_include_directories(PartialFKRTests PRIVATE Source)

target_compile_definitions(PartialFKRTests
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(PartialFKRTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_data_structures
        juce::juce_graphics
)

include(CTest)
include(Catch)
catch_discover_tests(PartialFKRTests)