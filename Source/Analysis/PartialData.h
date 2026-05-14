// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cstdint>
#include <vector>

/** A single time-stamped point on a partial track. */
struct Breakpoint {
    double time;       ///< seconds, source timebase
    float  frequency;  ///< Hz
    float  amplitude;  ///< linear, 0.0–1.0+ (no enforced ceiling)
    float  phase;      ///< radians; 0 if phase tracking was off
    float  bandwidth;  ///< Loris bandwidth [0,1]; stored but ignored at synthesis
};

/**
 * A single sinusoidal component with its time-varying envelope.
 *
 * Partials are always heap-allocated and owned by Project via unique_ptr.
 * The audio thread reads muted/soloed via relaxed atomics; no lock required
 * for flag reads. List-altering edits take a CriticalSection between blocks.
 */
class Partial {
public:
    explicit Partial(uint32_t id, juce::Colour colour);

    // Non-copyable: atomic members and unique ownership.
    Partial(const Partial&)            = delete;
    Partial& operator=(const Partial&) = delete;

    [[nodiscard]] uint32_t      getId() const noexcept { return id; }
    [[nodiscard]] juce::Colour  getColour() const noexcept { return displayColour; }

    [[nodiscard]] double startTime() const { return breakpoints.front().time; }
    [[nodiscard]] double endTime() const { return breakpoints.back().time; }
    [[nodiscard]] double duration() const { return endTime() - startTime(); }

    [[nodiscard]] bool containsTime(double t) const noexcept
    {
        return t >= startTime() && t <= endTime();
    }

    /** Peak amplitude across all breakpoints. */
    [[nodiscard]] float peakAmplitude() const noexcept;

    /** Energy: sum of amplitude * inter-breakpoint duration. */
    [[nodiscard]] float totalEnergy() const noexcept;

    /** Median frequency across breakpoints. */
    [[nodiscard]] float medianFrequency() const;

    // ── Mute / solo — audio thread reads these without locks ─────────────────
    // audio thread
    std::atomic<bool> muted{false};
    // audio thread
    std::atomic<bool> soloed{false};

    std::vector<Breakpoint> breakpoints;

private:
    uint32_t    id;
    juce::Colour displayColour;
};