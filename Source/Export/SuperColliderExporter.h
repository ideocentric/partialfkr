// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the partial set as a self-contained SuperCollider .scd file.
 *
 * The generated file boots the default server, defines a single reusable
 * \pfkrPartial SynthDef (BufRd + Line + SinOsc + Env.linen), discretizes
 * each partial's breakpoint envelope into a Buffer via Env.asSignal at the
 * given kInterval rate, then schedules one Synth per partial with SystemClock.
 *
 * Evaluate the file in the SuperCollider IDE: place the cursor inside the
 * outer parens and press Cmd+Return (macOS) / Ctrl+Return (Win/Linux).
 */
class SuperColliderExporter {
public:
    struct Options {
        int    sampleRate = 48000;
        double kInterval  = 0.005;  ///< control period in seconds (5 ms = Loris hop)
    };

    /** Write a .scd file. Returns true on success. */
    [[nodiscard]] static bool exportToFile(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const Options&                               options,
        const juce::File&                            outputFile);
};