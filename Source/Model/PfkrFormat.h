// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Project.h"
#include <JuceHeader.h>

/**
 * Serialises and deserialises the .pfkr project file format (JSON via juce::var).
 *
 * File sections:
 *   analysisData  — full original Loris output (t, f, a, bw, phase per breakpoint)
 *   partials      — current edited state (t, f, a + mute/solo/colour per partial)
 *   markers       — in/out points
 *   view          — canvas zoom/scroll state
 *   window        — window bounds
 */
namespace PfkrFormat
{
    struct SaveData
    {
        double              inPoint        = 0.0;
        double              outPoint       = -1.0;
        double              viewTimeStart  = 0.0;
        double              viewTimeEnd    = 10.0;
        float               viewFreqLow    = 0.0f;
        float               viewFreqHigh   = 8000.0f;
        bool                rightPanelVisible = true;
        bool                loopEnabled    = false;
        juce::Rectangle<int> windowBounds;
    };

    /** Serialise project + UI state to a JSON string ready to write to disk. */
    juce::String serialize(const Project& project, const SaveData& data);

    struct LoadResult
    {
        bool         success = false;
        juce::String errorMessage;
        SaveData     data;
    };

    /**
     * Parse a .pfkr JSON string into project.
     * On success, result.data carries the UI state to restore.
     * The caller is responsible for applying result.data to the UI.
     */
    LoadResult deserialize(const juce::String& json, Project& project);
}