// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the partial set as a self-contained Csound CSD file.
 *
 * The CSD uses one GEN07 function table pair (frequency + amplitude) per
 * partial, read with a phasor that ramps 0→1 over the note duration.
 * This exactly reproduces the linear-interpolation-between-breakpoints model
 * used by PartialFKR's synthesizer.
 *
 * Orchestra: poscil driven by tablei/phasor, stereo (-o dac by default).
 * Score:     one i-statement per partial at its analysis onset time.
 */
class CsoundExporter {
public:
    struct Options {
        int    sampleRate = 48000;
        int    ksmps      = -1;      ///< -1 = auto: round(sampleRate * kInterval)
        double kInterval  = 0.005;   ///< control period in seconds (5 ms = Loris hop)
    };

    /** Write a .csd file. Returns true on success. */
    [[nodiscard]] static bool exportToFile(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const Options&                               options,
        const juce::File&                            outputFile);
};