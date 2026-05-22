// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AmplitudeOps.h"
#include "Project.h"

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <vector>

// ── Undoable action ───────────────────────────────────────────────────────────

struct ScaleAmplitudesAction : public juce::UndoableAction {
    Project& project;
    // before[i] = { partialId, amplitude snapshot at time of action creation }
    std::vector<std::pair<uint32_t, std::vector<float>>> before;
    float factor;

    ScaleAmplitudesAction(Project& p,
                          std::vector<std::pair<uint32_t, std::vector<float>>> b,
                          float f)
        : project(p), before(std::move(b)), factor(f) {}


    bool perform() override
    {
        for (auto& [id, _] : before)
            if (auto* p = project.findPartialById(id))
                for (auto& bp : p->breakpoints)
                    bp.amplitude *= factor;
        project.notifyPartialsChanged();
        return true;
    }

    bool undo() override
    {
        for (auto& [id, amps] : before)
            if (auto* p = project.findPartialById(id))
                for (size_t i = 0; i < amps.size() && i < p->breakpoints.size(); ++i)
                    p->breakpoints[i].amplitude = amps[i];
        project.notifyPartialsChanged();
        return true;
    }

    int getSizeInUnits() override
    {
        int total = 0;
        for (const auto& [id, amps] : before)
            total += static_cast<int>(amps.size());
        return std::max(1, total);
    }
};

// ── computePeak ───────────────────────────────────────────────────────────────

float AmplitudeOps::computePeak(const std::vector<SynthData>& partials)
{
    if (partials.empty())
        return 0.0f;

    static constexpr double kOfflineSr = 8820.0;   // 1/5 of 44100; captures inter-partial beats
    static constexpr double kTwoPi     = 6.283185307179586;
    static constexpr int    kBlockSize = 2048;

    // Find total duration
    double totalDuration = 0.0;
    for (const auto& pd : partials)
        if (!pd.breakpoints.empty())
            totalDuration = std::max(totalDuration, pd.breakpoints.back().time);

    if (totalDuration <= 0.0)
        return 0.0f;

    const int    totalSamples = static_cast<int>(totalDuration * kOfflineSr) + 1;
    const double dt           = 1.0 / kOfflineSr;

    // Per-partial oscillator state maintained across blocks
    struct State { size_t bpIdx = 0; double phase = 0.0; };
    std::vector<State> states(partials.size());

    std::vector<float> mix(kBlockSize, 0.0f);
    float globalPeak = 0.0f;

    for (int blockStart = 0; blockStart < totalSamples; blockStart += kBlockSize)
    {
        const int n = std::min(kBlockSize, totalSamples - blockStart);
        std::fill(mix.begin(), mix.begin() + n, 0.0f);

        for (size_t pi = 0; pi < partials.size(); ++pi)
        {
            const auto& bps = partials[pi].breakpoints;
            if (bps.size() < 2) continue;

            auto& st = states[pi];

            for (size_t i = 0; i < static_cast<size_t>(n); ++i)
            {
                const double t = (blockStart + static_cast<int>(i)) * dt;

                // Advance breakpoint cursor
                while (st.bpIdx + 1 < bps.size() - 1 && bps[st.bpIdx + 1].time <= t)
                    ++st.bpIdx;

                if (st.bpIdx + 1 >= bps.size())
                    break;

                const Breakpoint& b0 = bps[st.bpIdx];
                const Breakpoint& b1 = bps[st.bpIdx + 1];

                const double span  = b1.time - b0.time;
                const double alpha = (span > 0.0)
                                     ? juce::jlimit(0.0, 1.0, (t - b0.time) / span)
                                     : 0.0;

                const float amp  = static_cast<float>(b0.amplitude + alpha * (b1.amplitude - b0.amplitude));
                const float freq = static_cast<float>(b0.frequency  + alpha * (b1.frequency  - b0.frequency));

                if (freq <= 0.0f) continue;

                st.phase += kTwoPi * static_cast<double>(freq) * dt;
                if (st.phase >= kTwoPi) st.phase -= kTwoPi;

                mix[i] += amp * static_cast<float>(std::sin(st.phase));
            }
        }

        for (size_t i = 0; i < static_cast<size_t>(n); ++i)
            globalPeak = std::max(globalPeak, std::abs(mix[i]));
    }

    return globalPeak;
}

// ── scale ─────────────────────────────────────────────────────────────────────

void AmplitudeOps::scale(Project&                       project,
                          const std::vector<uint32_t>&  partialIds,
                          float                         factor)
{
    if (factor <= 0.0f || partialIds.empty())
        return;

    // Snapshot current amplitudes for undo
    std::vector<std::pair<uint32_t, std::vector<float>>> before;
    before.reserve(partialIds.size());

    for (uint32_t id : partialIds)
    {
        if (auto* p = project.findPartialById(id))
        {
            std::vector<float> amps;
            amps.reserve(p->breakpoints.size());
            for (const auto& bp : p->breakpoints)
                amps.push_back(bp.amplitude);
            before.emplace_back(id, std::move(amps));
        }
    }

    if (before.empty())
        return;

    auto* action = new ScaleAmplitudesAction{project, std::move(before), factor};
    project.getEditHistory().perform(action, "Scale Amplitude");
}