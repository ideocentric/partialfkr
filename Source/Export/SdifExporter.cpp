// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SdifExporter.h"

// Loris SDIF support
#include <SdifFile.h>

bool SdifExporter::exportToFile(
    const std::vector<std::unique_ptr<Partial>>& partials,
    const Options&                               opts,
    const juce::File&                            outputFile)
{
    // Build a Loris::PartialList from our partials, then delegate to Loris::SdifFile.
    // This preserves the bandwidth field for RBEP round-trips.
    try {
        Loris::PartialList lorisPartials;

        for (const auto& p : partials) {
            Loris::Partial lp;
            for (const auto& bp : p->breakpoints) {
                lp.insert(bp.time,
                          Loris::Breakpoint(bp.frequency, bp.amplitude,
                                           bp.bandwidth, bp.phase));
            }
            lorisPartials.push_back(lp);
        }

        const std::string path = outputFile.getFullPathName().toStdString();
        const bool useRbep     = (opts.frameType == FrameType::LorisRBEP);

        Loris::SdifFile sdif;
        sdif.addPartials(lorisPartials.begin(), lorisPartials.end());
        if (useRbep)
            sdif.write(path);       // RBEP format (preserves bandwidth)
        else
            sdif.write1TRC(path);   // 1TRC format (resampled, standard)

        return true;
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("SDIF export error: " + juce::String(e.what()));
        return false;
    }
}