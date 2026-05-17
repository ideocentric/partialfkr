// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ReductionController.h"

#include "Project.h"

#include <algorithm>
#include <vector>

ReductionController::ReductionController(Project& p) : project(p) {}

void ReductionController::applyAmplitudeRank(int keepCount)
{
    amplitudeRank = keepCount;
    recomputeMuteMask();
}

void ReductionController::applyDurationThreshold(double minSeconds)
{
    durationThreshold = minSeconds;
    recomputeMuteMask();
}

void ReductionController::applyAmplitudeThreshold(float minLinearAmplitude)
{
    amplitudeThreshold = minLinearAmplitude;
    recomputeMuteMask();
}

void ReductionController::applyFrequencyBand(float lowHz, float highHz)
{
    frequencyLow  = lowHz;
    frequencyHigh = highHz;
    recomputeMuteMask();
}

void ReductionController::applyEnergyThreshold(float minEnergy)
{
    energyThreshold = minEnergy;
    recomputeMuteMask();
}

void ReductionController::clearReductions()
{
    amplitudeRank      = -1;
    durationThreshold  = -1.0;
    amplitudeThreshold = -1.0f;
    frequencyLow       = -1.0f;
    frequencyHigh      = -1.0f;
    energyThreshold    = -1.0f;
    recomputeMuteMask();
}

void ReductionController::recomputeMuteMask()
{
    const auto& partials = project.getPartials();

    // Determine which partials should be reduction-muted
    std::unordered_set<uint32_t> newMuted;

    for (const auto& p : partials) {
        bool mute = false;

        if (durationThreshold > 0.0 && p->duration() < durationThreshold)
            mute = true;
        if (amplitudeThreshold >= 0.0f && p->peakAmplitude() < amplitudeThreshold)
            mute = true;
        if (frequencyLow >= 0.0f && (p->medianFrequency() < frequencyLow || p->medianFrequency() > frequencyHigh))
            mute = true;
        if (energyThreshold >= 0.0f && p->totalEnergy() < energyThreshold)
            mute = true;

        if (mute)
            newMuted.insert(p->getId());
    }

    // Amplitude rank: keep top-N by peak amplitude
    if (amplitudeRank > 0) {
        std::vector<std::pair<float, uint32_t>> ranked;
        ranked.reserve(partials.size());
        for (const auto& p : partials) {
            if (!newMuted.count(p->getId()))
                ranked.emplace_back(p->peakAmplitude(), p->getId());
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;  // descending amplitude
        });
        for (size_t i = static_cast<size_t>(amplitudeRank); i < ranked.size(); ++i)
            newMuted.insert(ranked[i].second);
    }

    // Un-mute any partials previously reduction-muted that no longer qualify
    for (auto id : reductionMuted)
        if (!newMuted.count(id))
            if (auto* p = project.findPartialById(id))
                p->muted.store(false, std::memory_order_relaxed);

    // Apply new reduction mutes
    for (auto id : newMuted)
        if (auto* p = project.findPartialById(id))
            p->muted.store(true, std::memory_order_relaxed);

    reductionMuted = std::move(newMuted);

    // Notify listeners so PartialView repaints immediately to reflect the new mute state.
    project.notifyPartialsChanged();
}