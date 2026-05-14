// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Analyzer.h"

// Loris headers
#include <Analyzer.h>
#include <Partial.h>
#include <PartialList.h>

#include <cassert>

// ── Analyzer::Job ─────────────────────────────────────────────────────────────

class Analyzer::Job : public juce::ThreadPoolJob {
public:
    Job(const juce::AudioBuffer<float>& audio,
        double                          sr,
        const Parameters&               params,
        ProgressCallback                onProgress,
        CompletionCallback              onComplete)
        : juce::ThreadPoolJob("LorisAnalysis"),
          audioData(audio),
          sampleRate(sr),
          parameters(params),
          progressCallback(std::move(onProgress)),
          completionCallback(std::move(onComplete))
    {
    }

    JobStatus runJob() override
    {
        if (shouldExit())
            return JobStatus::jobHasFinished;

        try {
            // Wrap in shared_ptr so the lambda is copyable (std::function requirement),
            // then move out of the shared_ptr on the message thread.
            auto result = std::make_shared<std::vector<std::unique_ptr<Partial>>>(runLoris());
            juce::MessageManager::callAsync([cb = completionCallback, result]() mutable {
                cb(std::move(*result));
            });
        } catch (const std::exception& e) {
            juce::Logger::writeToLog("Loris analysis error: " + juce::String(e.what()));
            juce::MessageManager::callAsync([cb = completionCallback]() mutable {
                cb({});
            });
        }

        return JobStatus::jobHasFinished;
    }

private:
    std::vector<std::unique_ptr<Partial>> runLoris()
    {
        Loris::Analyzer loris(parameters.frequencyResolutionHz, parameters.windowWidthHz);

        // Copy mono samples into a std::vector for Loris
        const int numSamples = audioData.getNumSamples();
        const float* src     = audioData.getReadPointer(0);
        std::vector<double> samples(src, src + numSamples);

        if (shouldExit())
            return {};

        loris.analyze(samples, sampleRate);

        if (shouldExit())
            return {};

        // Report 50% progress after analysis completes
        juce::MessageManager::callAsync([cb = progressCallback] { cb(0.5f); });

        // Convert Loris::PartialList to our Partial type
        const auto& lorisPartials = loris.partials();

        // Assign colours evenly across hue wheel
        const int count = static_cast<int>(lorisPartials.size());
        int idx = 0;

        std::vector<std::unique_ptr<Partial>> result;
        result.reserve(static_cast<size_t>(count));

        uint32_t nextId = 0;
        for (const auto& lp : lorisPartials) {
            if (shouldExit())
                return {};

            const float hue   = (count > 1) ? static_cast<float>(idx) / static_cast<float>(count) : 0.5f;
            const auto colour = juce::Colour::fromHSV(hue, 0.8f, 0.9f, 1.0f);

            auto partial = std::make_unique<Partial>(nextId++, colour);

            for (auto it = lp.begin(); it != lp.end(); ++it) {
                Breakpoint bp;
                bp.time      = it.time();
                bp.frequency = static_cast<float>(it->frequency());
                bp.amplitude = static_cast<float>(it->amplitude());
                bp.phase     = static_cast<float>(it->phase());
                bp.bandwidth = static_cast<float>(it->bandwidth());
                partial->breakpoints.push_back(bp);
            }

            if (partial->breakpoints.size() >= 2)
                result.push_back(std::move(partial));

            ++idx;

            const float progress = 0.5f + 0.5f * (static_cast<float>(idx) / static_cast<float>(count));
            juce::MessageManager::callAsync([cb = progressCallback, progress] { cb(progress); });
        }

        return result;
    }

    juce::AudioBuffer<float> audioData;
    double                   sampleRate;
    Parameters               parameters;
    ProgressCallback         progressCallback;
    CompletionCallback       completionCallback;
};

// ── Analyzer ─────────────────────────────────────────────────────────────────

Analyzer::Analyzer()  = default;
Analyzer::~Analyzer() = default;

void Analyzer::setParameters(const Parameters& p) { parameters = p; }
Analyzer::Parameters Analyzer::getParameters() const { return parameters; }

void Analyzer::analyzeAsync(const juce::AudioBuffer<float>& monoAudio,
                            double                          sampleRate,
                            ProgressCallback                progressCallback,
                            CompletionCallback              completionCallback)
{
    cancel();

    auto job = std::make_unique<Job>(monoAudio, sampleRate, parameters,
                                     std::move(progressCallback),
                                     std::move(completionCallback));
    currentJob = std::move(job);
    threadPool.addJob(currentJob.get(), false);
}

void Analyzer::waitForCompletion()
{
    threadPool.waitForJobToFinish(currentJob.get(), -1);
}

void Analyzer::cancel()
{
    if (currentJob)
        threadPool.removeJob(currentJob.get(), true, 5000);
}