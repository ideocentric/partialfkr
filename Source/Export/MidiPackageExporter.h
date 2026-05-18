// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the (non-muted) partial set as a folder package containing:
 *   - One MIDI file per partial  (partial_001.mid, partial_002.mid, …)
 *   - bom.csv   — Bill of Materials with per-partial metadata
 *   - readme.txt — Source info, parameter summary, and usage notes
 *
 * Each MIDI file uses channel 1, auto-selects the smallest sufficient
 * standard pitch-bend range (±2 / ±12 / ±24 / ±48 semitones) for that
 * partial, and encodes CC11 (Expression) for the amplitude envelope.
 */
class MidiPackageExporter {
public:
    struct Options {
        int    ppq          = 960;
        double tempoBpm     = 120.0;
        float  ampFloorLinear = 1e-4f;  ///< linear amplitude mapped to velocity 1
        float  ampCeilLinear  = 1.0f;   ///< linear amplitude mapped to velocity 127
    };

    struct Result {
        bool         success      = false;
        int          filesWritten = 0;
        juce::String errorMessage;
    };

    /**
     * Export all partials to outputFolder.
     * sourceAudioFile is used in readme.txt metadata; pass File() if unknown.
     */
    [[nodiscard]] static Result exportPackage(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const juce::File&                            sourceAudioFile,
        const juce::File&                            outputFolder,
        const Options&                               options);

private:
    static int    ampToVelocity    (float linearAmp, float floor, float ceil) noexcept;
    static int    freqToMidiNote   (float Hz) noexcept;
    static double deviationCents   (float Hz, int nominalMidiNote) noexcept;
    static float  maxDeviationCents(const Partial& p, int nominalNote) noexcept;
    static int    chooseBendRange  (float maxDevCents) noexcept;
    static float  linearToDb       (float linear) noexcept;
    static juce::String midiNoteName(int note);
};