// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <unordered_set>

class Project;

/**
 * Applies reduction filters to the partial set by writing mute flags.
 *
 * Each method re-evaluates all reduction criteria and updates the atomic
 * muted flags on the message thread. The audio thread observes changes at
 * the next block boundary (~5–10 ms).
 *
 * Reduction-mutes and selection-mutes are tracked separately; clearing
 * reductions does not un-mute manually-muted partials.
 */
class ReductionController {
public:
    explicit ReductionController(Project& project);

    // ── Filter operations ─────────────────────────────────────────────────────

    /** Keep only the top-N partials by peak amplitude at each moment in time. */
    void applyAmplitudeRank(int keepCount);

    /** Mute partials shorter than the given duration. */
    void applyDurationThreshold(double minSeconds);

    /** Mute partials whose peak amplitude falls below threshold. */
    void applyAmplitudeThreshold(float minLinearAmplitude);

    /** Mute partials whose median frequency falls outside [lowHz, highHz]. */
    void applyFrequencyBand(float lowHz, float highHz);

    /** Mute partials whose total energy is below threshold. */
    void applyEnergyThreshold(float minEnergy);

    /** Remove all reduction filters; selection-mutes are unaffected. */
    void clearReductions();

private:
    void recomputeMuteMask();

    Project& project;

    // Current filter state
    int    amplitudeRank        = -1;    // -1 = inactive
    double durationThreshold    = -1.0;
    float  amplitudeThreshold   = -1.0f;
    float  frequencyLow         = -1.0f;
    float  frequencyHigh        = -1.0f;
    float  energyThreshold      = -1.0f;

    // IDs that are muted due to reduction filters (not selection)
    std::unordered_set<uint32_t> reductionMuted;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReductionController)
};