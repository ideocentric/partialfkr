// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the partial set as a versioned JSON file.
 *
 * See docs/DESIGN.md § 9 for the schema definition.
 */
class JsonExporter {
public:
    struct Options {
        bool includePhase     = false;
        bool includeBandwidth = false;
        juce::String sourceFileName;
        double       sampleRate     = 0.0;
        double       durationSeconds = 0.0;
    };

    [[nodiscard]] static bool exportToFile(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const Options&                               options,
        const juce::File&                            outputFile);
};