// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the partial set as an SDIF file using Loris's built-in writer.
 *
 * Frame type 1TRC: standard sinusoidal track (broad compatibility).
 * Frame type RBEP: Loris reassigned bandwidth-enhanced partial (preserves
 *                  bandwidth field for round-trip with other Loris tools).
 *
 * See docs/DESIGN.md § 8.
 */
class SdifExporter {
public:
    enum class FrameType { Standard1TRC, LorisRBEP };

    struct Options {
        FrameType frameType = FrameType::Standard1TRC;
    };

    [[nodiscard]] static bool exportToFile(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const Options&                               options,
        const juce::File&                            outputFile);
};