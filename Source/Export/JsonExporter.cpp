// SPDX-License-Identifier: AGPL-3.0-or-later
#include "JsonExporter.h"

bool JsonExporter::exportToFile(
    const std::vector<std::unique_ptr<Partial>>& partials,
    const Options&                               opts,
    const juce::File&                            outputFile)
{
    juce::DynamicObject* root = new juce::DynamicObject();

    root->setProperty("schema_version",    "1.0");
    root->setProperty("source_file",       opts.sourceFileName);
    root->setProperty("sample_rate",       opts.sampleRate);
    root->setProperty("duration_seconds",  opts.durationSeconds);
    root->setProperty("partial_count",     static_cast<int>(partials.size()));

    juce::Array<juce::var> partialsArray;
    for (const auto& p : partials) {
        juce::DynamicObject* partialObj = new juce::DynamicObject();
        partialObj->setProperty("id", static_cast<int>(p->getId()));

        juce::Array<juce::var> bpsArray;
        for (const auto& bp : p->breakpoints) {
            juce::DynamicObject* bpObj = new juce::DynamicObject();
            bpObj->setProperty("t", bp.time);
            bpObj->setProperty("f", static_cast<double>(bp.frequency));
            bpObj->setProperty("a", static_cast<double>(bp.amplitude));
            if (opts.includePhase)
                bpObj->setProperty("phase", static_cast<double>(bp.phase));
            if (opts.includeBandwidth)
                bpObj->setProperty("bw", static_cast<double>(bp.bandwidth));
            bpsArray.add(juce::var(bpObj));
        }

        partialObj->setProperty("breakpoints", bpsArray);
        partialsArray.add(juce::var(partialObj));
    }

    root->setProperty("partials", partialsArray);

    const juce::String json = juce::JSON::toString(juce::var(root), true);

    auto stream = outputFile.createOutputStream();
    if (stream == nullptr)
        return false;

    stream->writeText(json, false, false, "\n");
    return true;
}