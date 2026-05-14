// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PartialData.h"

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

/**
 * Wraps the Loris reassigned bandwidth-enhanced analyzer.
 *
 * Runs on a background thread (juce::ThreadPool); progress is reported via
 * the progressCallback and completion via completionCallback, both called on
 * the message thread through juce::MessageManager::callAsync.
 */
class Analyzer {
public:
    struct Parameters {
        // frequencyResolutionHz: minimum instantaneous frequency difference between
        // adjacent partials. Smaller = more partials tracked, slower analysis.
        // windowWidthHz: analysis window main-lobe width. Smaller window = longer
        // in time = better low-frequency resolution, worse onset precision.
        // Rule of thumb: set windowWidthHz to ~2× the lowest frequency you care about.
        // At 50 Hz floor → 100 Hz window (~20 ms). Default 200 Hz misses content below ~100 Hz.
        double frequencyResolutionHz = 50.0;
        double windowWidthHz         = 100.0;
        float  amplitudeFloor        = 1e-4f;  ///< linear amplitude floor; quieter breakpoints pruned
        double hopTimeSeconds        = 0.005;  ///< not directly set in Loris — informational
    };

    using ProgressCallback   = std::function<void(float)>;    ///< 0.0–1.0
    using CompletionCallback = std::function<void(std::vector<std::unique_ptr<Partial>>)>;

    Analyzer();
    ~Analyzer();

    void setParameters(const Parameters& params);
    [[nodiscard]] Parameters getParameters() const;

    /**
     * Begin analysis of the supplied mono audio buffer asynchronously.
     * Calls progressCallback periodically and completionCallback when done.
     * Both callbacks are delivered on the message thread.
     */
    void analyzeAsync(const juce::AudioBuffer<float>& monoAudio,
                      double                          sampleRate,
                      ProgressCallback                progressCallback,
                      CompletionCallback              completionCallback);

    /** Block until any in-progress analysis completes. */
    void waitForCompletion();

    /** Cancel any in-progress analysis. */
    void cancel();

private:
    class Job;

    Parameters              parameters;
    std::unique_ptr<Job>    currentJob;
    juce::ThreadPool        threadPool{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Analyzer)
};