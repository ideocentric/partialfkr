// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PartialData.h"

#include <algorithm>
#include <numeric>

Partial::Partial(uint32_t partialId, juce::Colour colour)
    : id(partialId), displayColour(colour)
{
}

float Partial::peakAmplitude() const noexcept
{
    float peak = 0.0f;
    for (const auto& bp : breakpoints)
        peak = std::max(peak, bp.amplitude);
    return peak;
}

float Partial::totalEnergy() const noexcept
{
    if (breakpoints.size() < 2)
        return 0.0f;

    float energy = 0.0f;
    for (size_t i = 0; i + 1 < breakpoints.size(); ++i) {
        const float dt  = static_cast<float>(breakpoints[i + 1].time - breakpoints[i].time);
        const float amp = (breakpoints[i].amplitude + breakpoints[i + 1].amplitude) * 0.5f;
        energy += amp * dt;
    }
    return energy;
}

float Partial::medianFrequency() const
{
    if (breakpoints.empty())
        return 0.0f;

    std::vector<float> freqs;
    freqs.reserve(breakpoints.size());
    for (const auto& bp : breakpoints)
        freqs.push_back(bp.frequency);

    auto mid = freqs.begin() + static_cast<ptrdiff_t>(freqs.size() / 2);
    std::nth_element(freqs.begin(), mid, freqs.end());
    return *mid;
}