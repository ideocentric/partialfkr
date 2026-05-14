// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiExporter.h"

#include <algorithm>
#include <cmath>

// ── Helpers ───────────────────────────────────────────────────────────────────

int MidiExporter::ampToVelocity(float linear, float floor, float ceil) noexcept
{
    if (linear <= floor)
        return 1;
    const float db      = 20.0f * std::log10(linear / ceil);
    const float dbFloor = 20.0f * std::log10(floor / ceil);
    const float norm    = (db - dbFloor) / (-dbFloor);
    return juce::jlimit(1, 127, static_cast<int>(std::round(norm * 126.0f)) + 1);
}

int MidiExporter::freqToMidiNote(float Hz) noexcept
{
    return static_cast<int>(std::round(69.0f + 12.0f * std::log2(Hz / 440.0f)));
}

double MidiExporter::deviationCents(float Hz, int nominalMidiNote) noexcept
{
    const double nominalHz = 440.0 * std::exp2((nominalMidiNote - 69) / 12.0);
    return 1200.0 * std::log2(static_cast<double>(Hz) / nominalHz);
}

// ── Public API ────────────────────────────────────────────────────────────────

std::vector<float> MidiExporter::computeDeviationHistogram(
    const std::vector<std::unique_ptr<Partial>>& partials)
{
    std::vector<float> maxDeviations;
    maxDeviations.reserve(partials.size());

    for (const auto& p : partials) {
        float nomFreq = p->medianFrequency();
        int   note    = freqToMidiNote(nomFreq);
        float maxDev  = 0.0f;
        for (const auto& bp : p->breakpoints)
            maxDev = std::max(maxDev, static_cast<float>(std::abs(deviationCents(bp.frequency, note))));
        maxDeviations.push_back(maxDev);
    }
    return maxDeviations;
}

bool MidiExporter::exportToFile(
    const std::vector<std::unique_ptr<Partial>>& partials,
    const Options&                               opts,
    const juce::File&                            outputFile)
{
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(opts.ppq);

    const double secondsPerTick = 60.0 / (opts.tempoBpm * opts.ppq);
    auto timeToTick = [&](double t) -> int {
        return static_cast<int>(std::round(t / secondsPerTick));
    };

    juce::MidiMessageSequence seq;

    // Add tempo event
    seq.addEvent(juce::MidiMessage::tempoMetaEvent(
        static_cast<int>(60'000'000.0 / opts.tempoBpm)), 0);

    int nextChannel = 2;  // channels 2–16 in MPE mode
    const double bendRangeCents = opts.bendRangeSemitones * 100.0;

    for (const auto& p : partials) {
        const auto& bps = p->breakpoints;
        if (bps.size() < 2)
            continue;

        const int ch = opts.useMpe ? nextChannel : 1;
        if (opts.useMpe) {
            nextChannel = (nextChannel >= 16) ? 2 : nextChannel + 1;
        }

        const float nomFreq  = p->medianFrequency();
        const int   nomNote  = freqToMidiNote(nomFreq);
        const int   velocity = ampToVelocity(bps.front().amplitude,
                                             opts.ampFloorLinear,
                                             opts.ampCeilLinear);

        // RPN: set pitch bend sensitivity
        const int bendTick = timeToTick(bps.front().time);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 0),  bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 0),  bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 6, opts.bendRangeSemitones), bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 38, 0),   bendTick);
        // RPN reset
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 127), bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 127), bendTick);

        // Note on
        seq.addEvent(juce::MidiMessage::noteOn(ch, nomNote, static_cast<uint8_t>(velocity)),
                     timeToTick(bps.front().time));

        for (const auto& bp : bps) {
            const int tick = timeToTick(bp.time);

            // Pitch bend
            const double devCents = deviationCents(bp.frequency, nomNote);
            const double norm     = juce::jlimit(-1.0, 1.0, devCents / bendRangeCents);
            const int    bend14   = static_cast<int>(std::round(norm * 8191.0)) + 8192;
            seq.addEvent(juce::MidiMessage::pitchWheel(ch, bend14), tick);

            // CC11 expression for amplitude
            const int cc11 = ampToVelocity(bp.amplitude, opts.ampFloorLinear, opts.ampCeilLinear);
            seq.addEvent(juce::MidiMessage::controllerEvent(ch, 11, cc11), tick);
        }

        // Note off
        seq.addEvent(juce::MidiMessage::noteOff(ch, nomNote),
                     timeToTick(bps.back().time));
    }

    seq.sort();
    midi.addTrack(seq);

    auto stream = outputFile.createOutputStream();
    if (stream == nullptr)
        return false;

    return midi.writeTo(*stream);
}