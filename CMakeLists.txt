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
if(MSVC)
    # MSVC omits M_PI from <cmath> unless this is defined.
    target_compile_definitions(loris_static PRIVATE _USE_MATH_DEFINES)
endif()
set_target_properties(loris_static PROPERTIES CXX_STANDARD 14)
if(MSVC)
    target_compile_options(loris_static PRIVATE /w)
else()
    target_compile_options(loris_static PRIVATE -w)
endif()

# ── Main application ──────────────────────────────────────────────────────────
# ── Platform-specific application icon ───────────────────────────────────────
if(APPLE)
    # Pass .icns directly — JUCE uses it as the app bundle icon without conversion.
    set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/Resources/dist/macos/PartialFKR.icns")
else()
    # juceaide reads ICON_BIG as a PNG and generates the platform icon (.ico on
    # Windows, unused on Linux). Pass the 512px PNG from the iconsmaker dist tree.
    set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/Resources/dist/linux/hicolor/512x512/apps/com.partialfkr.PartialFKR.png")
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
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/dist/macos/PartialFKR.icns"
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
    Source/UI/SideTabBar.cpp
    Source/UI/ToolsPanel.cpp
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

juce_add_binary_data(PartialFKRAssets
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES
        Resources/icons/ui/select.svg
        Resources/icons/ui/direct.svg
        Resources/icons/ui/bridge.svg
        Resources/icons/ui/xfade.svg
        Resources/icons/ui/stretch.svg
        Resources/icons/ui/scale.svg
        Resources/icons/ui/normalize.svg
        Resources/icons/ui/in.svg
        Resources/icons/ui/out.svg
        Resources/icons/ui/sel.svg
        Resources/icons/ui/fadein.svg
        Resources/icons/ui/fadeout.svg
)

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
        PartialFKRAssets
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

# ── Install rules ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)

if(APPLE)
    install(TARGETS PartialFKR
        BUNDLE DESTINATION .
        COMPONENT Application)
elseif(WIN32)
    install(TARGETS PartialFKR
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Application)
else()
    install(TARGETS PartialFKR
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Application)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Resources/dist/linux/hicolor/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor"
        COMPONENT Application)
    install(FILES
        "${CMAKE_SOURCE_DIR}/Resources/dist/linux/com.partialfkr.PartialFKR.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
        COMPONENT Application)
    install(FILES
        "${CMAKE_SOURCE_DIR}/Resources/dist/linux/com.partialfkr.PartialFKR.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
        COMPONENT Application)
endif()

# ── CPack ─────────────────────────────────────────────────────────────────────
# Only package our Application component. Without this, CPACK defaults to ALL,
# which pulls in JUCE's Unspecified component: juceaide, all module headers,
# and cmake configs — none of which belong in a distributable DMG/DEB/RPM.
set(CPACK_COMPONENTS_ALL Application)
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_CURRENT_BINARY_DIR};${PROJECT_NAME};Application;/")

set(CPACK_PACKAGE_NAME             "PartialFKR")
set(CPACK_PACKAGE_VENDOR           "ideocentric")
set(CPACK_PACKAGE_VERSION          "0.1.0")
set(CPACK_PACKAGE_VERSION_MAJOR    "0")
set(CPACK_PACKAGE_VERSION_MINOR    "1")
set(CPACK_PACKAGE_VERSION_PATCH    "0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Loris-based spectral audio editor and resynthesizer")
set(CPACK_PACKAGE_HOMEPAGE_URL     "https://github.com/ideocentric/partialfkr")
set(CPACK_RESOURCE_FILE_LICENSE    "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PartialFKR")

# ── macOS: custom dmg target ─────────────────────────────────────────────────
# Uses dmgbuild (pip install dmgbuild) instead of CPack's DragNDrop generator.
# dmgbuild writes the .DS_Store programmatically — no AppleScript, no Finder,
# no Applications symlink in the source tree (safe for CLion and CI runners).
# Layout is defined in packaging/macos/dmgbuild-settings.py.
if(APPLE)
    add_custom_target(dmg
        DEPENDS PartialFKR
        COMMAND bash
            "${CMAKE_SOURCE_DIR}/packaging/macos/make-dmg.sh"
            "$<TARGET_BUNDLE_DIR:PartialFKR>"
            "${CMAKE_BINARY_DIR}/PartialFKR-0.1.0-macOS.dmg"
        COMMENT "Creating PartialFKR-0.1.0-macOS.dmg"
        VERBATIM
    )
endif()

# ── Windows: NSIS installer (.exe) ───────────────────────────────────────────
if(WIN32)
    set(CPACK_GENERATOR            "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME    "PartialFKR")
    set(CPACK_NSIS_DISPLAY_NAME    "PartialFKR")
    set(CPACK_NSIS_MUI_ICON
        "${CMAKE_SOURCE_DIR}/Resources/dist/windows/PartialFKR.ico")
    set(CPACK_NSIS_MUI_UNIICON
        "${CMAKE_SOURCE_DIR}/Resources/dist/windows/PartialFKR.ico")
    # CPACK_NSIS_INSTALLED_ICON_NAME: four backslashes → two after CMake parse →
    # CPack writes two (unescaped) → CMake re-reads as one → NSIS gets bin\PartialFKR.exe
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\PartialFKR.exe")
    set(CPACK_NSIS_HELP_LINK       "https://github.com/ideocentric/partialfkr")
    set(CPACK_NSIS_URL_INFO_ABOUT  "https://github.com/ideocentric/partialfkr")
    set(CPACK_NSIS_MODIFY_PATH     OFF)
    # Register .pfkr extension via external .nsh files so NSIS commands with
    # backslashes and embedded quotes never pass through CMake string escaping.
    # CMAKE_SOURCE_DIR is expanded at configure time; forward slashes are fine
    # for NSIS !include on Windows.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "!include \\\"${CMAKE_SOURCE_DIR}/packaging/windows/fileassoc.nsh\\\"")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "!include \\\"${CMAKE_SOURCE_DIR}/packaging/windows/fileassoc_uninstall.nsh\\\"")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(CPACK_PACKAGE_FILE_NAME "PartialFKR-0.1.0-Windows-arm64")
    else()
        set(CPACK_PACKAGE_FILE_NAME "PartialFKR-0.1.0-Windows-x64")
    endif()
endif()

# ── Linux: DEB + RPM ──────────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR            "DEB;RPM")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(_deb_arch "arm64")
        set(_rpm_arch "aarch64")
    else()
        set(_deb_arch "amd64")
        set(_rpm_arch "x86_64")
    endif()
    # DEB
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER
        "Matt Comeione <matt@ideocentric.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION      "sound")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_deb_arch}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libasound2 (>= 1.0), libx11-6, libxinerama1, libxrandr2, libxcursor1, libfreetype6, libfontconfig1, libgtk-3-0, libgl1")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
        "https://github.com/ideocentric/partialfkr")
    set(CPACK_DEBIAN_FILE_NAME
        "PartialFKR-0.1.0-Linux-${_deb_arch}.deb")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/packaging/linux/postinst"
        "${CMAKE_SOURCE_DIR}/packaging/linux/postrm")
    # RPM
    set(CPACK_RPM_PACKAGE_LICENSE         "AGPL-3.0")
    set(CPACK_RPM_PACKAGE_GROUP           "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE    "${_rpm_arch}")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "alsa-lib >= 1.0, gtk3 >= 3.0, mesa-libGL >= 1.0")
    set(CPACK_RPM_PACKAGE_URL
        "https://github.com/ideocentric/partialfkr")
    set(CPACK_RPM_FILE_NAME
        "PartialFKR-0.1.0-Linux-${_rpm_arch}.rpm")
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
        "${CMAKE_SOURCE_DIR}/packaging/linux/postinst")
    set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
        "${CMAKE_SOURCE_DIR}/packaging/linux/postrm")
endif()

include(CPack)