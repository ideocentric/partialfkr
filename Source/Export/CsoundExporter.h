// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <vector>

/**
 * Exports the partial set as a Csound score + optional orchestra.
 *
 * Mode A: GEN02 f-tables (one per partial, referenced by i-statements).
 * Mode B: Inline linseg p-fields (suitable for sparse breakpoint sets).
 *
 * See docs/DESIGN.md § 7 for full format specification.
 */
class CsoundExporter {
public:
    enum class Mode { TableBased, Inline };

    struct Options {
        Mode   mode           = Mode::TableBased;
        int    autoModeThreshold = 15;  ///< if avg breakpoints/partial > this, use TableBased
        bool   writeOrchestra = true;   ///< emit a template .orc alongside the .sco
        double startTableIdx  = 1001;
    };

    [[nodiscard]] static bool exportToFile(
        const std::vector<std::unique_ptr<Partial>>& partials,
        const Options&                               options,
        const juce::File&                            outputFile);

private:
    static bool exportTableBased(const std::vector<std::unique_ptr<Partial>>& partials,
                                 const Options& options,
                                 juce::OutputStream& out);

    static bool exportInline(const std::vector<std::unique_ptr<Partial>>& partials,
                             const Options& options,
                             juce::OutputStream& out);

    static void writeOrchestra(juce::OutputStream& out);
};