// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the partial set as a MIDI file (MPE or single-channel).
 *
 * See CLAUDE.md § "MIDI Export Specification (Locked)" and
 * docs/DESIGN.md § 6 for full math and semantics.
 */
class MidiExporter {
public:
    struct Options {
        int    bendRangeSemitones   = 24;   ///< ±2, ±12, ±24, or ±48
        bool   useMpe               = true;
        bool   splitOnSaturation    = false;
        int    ppq                  = 960;
        double tempoBpm             = 120.0;
        float  ampFloorLinear       = 1e-4f;
        float  ampCeilLinear        = 1.0f;
    };

    /** Returns true on success. Writes to the given file. */
    [[nodiscard]] static bool exportToFile(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const Options&                               options,
        const juce::File&                            outputFile);

    /**
     * Pre-pass: returns a histogram of per-partial max pitch deviation in cents.
     * Used by the export dialog to help the user pick a bend range.
     */
    [[nodiscard]] static std::vector<float> computeDeviationHistogram(
        const std::vector<std::unique_ptr<Partial>>& partials);

private:
    static int ampToVelocity(float linearAmp, float floor, float ceil) noexcept;
    static int freqToMidiNote(float Hz) noexcept;
    static double deviationCents(float Hz, int nominalMidiNote) noexcept;
};