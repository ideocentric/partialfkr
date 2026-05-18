// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace CommandIDs {
    // File
    static constexpr int fileNew          = 0x2001;
    static constexpr int fileOpenProject  = 0x2002;  // open .pfkr
    static constexpr int fileAnalyzeAudio = 0x2003;  // load audio → analyze (disabled if project has audio)
    static constexpr int fileSave         = 0x2004;
    static constexpr int fileSaveAs       = 0x2005;
    static constexpr int fileClose        = 0x2006;  // close current window (with dirty check)

    // Application
    static constexpr int appAbout = 0x2070;
    static constexpr int appHelp  = 0x2071;

    // Window
    static constexpr int windowMinimize       = 0x2060;
    static constexpr int windowZoom           = 0x2061;
    static constexpr int windowBringAllToFront = 0x2062;

    // View
    static constexpr int viewZoomIn     = 0x2050;
    static constexpr int viewZoomOut    = 0x2051;
    static constexpr int viewZoomFit    = 0x2052;
    static constexpr int viewTogglePanel = 0x2053;

    // Transport
    static constexpr int transportPlayPause = 0x2040;
    static constexpr int transportStop      = 0x2041;
    static constexpr int transportLoop      = 0x2042;

    // Join operations
    static constexpr int editBridgePartials   = 0x2080;
    static constexpr int editCrossfadeOverlap = 0x2081;
    static constexpr int editStretch          = 0x2082;

    // Selection
    static constexpr int invertSelection = 0x2030;

    // Amplitude
    static constexpr int normalize      = 0x2020;
    static constexpr int scaleAmplitude = 0x2021;

    // Export
    static constexpr int exportMidi             = 0x2010;
    static constexpr int exportMidiPackage      = 0x2015;
    static constexpr int exportCsound           = 0x2011;
    static constexpr int exportSuperCollider    = 0x2016;
    static constexpr int exportSdif             = 0x2012;
    static constexpr int exportJson             = 0x2013;
}
// Standard edit commands use juce::StandardApplicationCommandIDs:
//   undo=0x1008, redo=0x1009, cut, copy, paste, del, selectAll, deselectAll