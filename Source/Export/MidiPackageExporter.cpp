// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiPackageExporter.h"

#include <algorithm>
#include <cmath>

// ── Private helpers ───────────────────────────────────────────────────────────

int MidiPackageExporter::ampToVelocity(float linear, float floor, float ceil) noexcept
{
    if (linear <= floor)
        return 1;
    const float db      = 20.0f * std::log10(linear / ceil);
    const float dbFloor = 20.0f * std::log10(floor  / ceil);
    const float norm    = (db - dbFloor) / (-dbFloor);
    return juce::jlimit(1, 127, static_cast<int>(std::round(norm * 126.0f)) + 1);
}

int MidiPackageExporter::freqToMidiNote(float Hz) noexcept
{
    return static_cast<int>(std::round(69.0f + 12.0f * std::log2(Hz / 440.0f)));
}

double MidiPackageExporter::deviationCents(float Hz, int nominalMidiNote) noexcept
{
    const double nomHz = 440.0 * std::exp2((nominalMidiNote - 69) / 12.0);
    return 1200.0 * std::log2(static_cast<double>(Hz) / nomHz);
}

float MidiPackageExporter::maxDeviationCents(const Partial& p, int nominalNote) noexcept
{
    float maxDev = 0.0f;
    for (const auto& bp : p.breakpoints)
        maxDev = std::max(maxDev,
                          static_cast<float>(std::abs(deviationCents(bp.frequency, nominalNote))));
    return maxDev;
}

int MidiPackageExporter::chooseBendRange(float maxDevCents) noexcept
{
    const float devSemitones = maxDevCents / 100.0f;
    if (devSemitones <= 2.0f)  return 2;
    if (devSemitones <= 12.0f) return 12;
    if (devSemitones <= 24.0f) return 24;
    return 48;
}

float MidiPackageExporter::linearToDb(float linear) noexcept
{
    if (linear <= 0.0f) return -80.0f;
    return std::max(-80.0f, 20.0f * std::log10(linear));
}

juce::String MidiPackageExporter::midiNoteName(int note)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    const int octave = (note / 12) - 1;
    return juce::String(names[note % 12]) + juce::String(octave);
}

// ── Export ────────────────────────────────────────────────────────────────────

MidiPackageExporter::Result MidiPackageExporter::exportPackage(
    const std::vector<std::unique_ptr<Partial>>& partials,
    const juce::File&                            sourceAudioFile,
    const juce::File&                            outputFolder,
    const Options&                               opts)
{
    Result result;

    if (!outputFolder.createDirectory())
    {
        result.errorMessage = "Could not create output folder: " + outputFolder.getFullPathName();
        return result;
    }

    // Count valid partials (>= 2 breakpoints) for zero-padding width
    int numValid = 0;
    for (const auto& p : partials)
        if (p->breakpoints.size() >= 2) ++numValid;

    const int padWidth = numValid <= 9    ? 1
                       : numValid <= 99   ? 2
                       : numValid <= 999  ? 3
                       : numValid <= 9999 ? 4 : 5;

    const double secondsPerTick = 60.0 / (opts.tempoBpm * opts.ppq);
    auto timeToTick = [&](double t) -> int {
        return static_cast<int>(std::round(t / secondsPerTick));
    };

    // Per-row data collected for BOM and README
    struct BomRow {
        juce::String filename;
        int          noteNumber;
        juce::String noteName;
        double       startS, endS, durationS;
        float        freqMinHz, freqMaxHz;
        float        ampMinDb,  ampMaxDb;
        int          peakVelocity;
        int          bendRangeSemitones;
        float        maxDevCents;
        float        energy;
    };
    std::vector<BomRow> bomRows;
    bomRows.reserve(static_cast<size_t>(numValid));

    int fileIndex = 1;

    for (const auto& p : partials)
    {
        const auto& bps = p->breakpoints;
        if (bps.size() < 2) continue;

        // ── Per-partial analysis ──────────────────────────────────────────────

        const float nomFreq    = p->medianFrequency();
        const int   nomNote    = freqToMidiNote(nomFreq);
        const float maxDevCts  = maxDeviationCents(*p, nomNote);
        const int   bendRange  = chooseBendRange(maxDevCts);

        float freqMin = bps[0].frequency, freqMax = bps[0].frequency;
        float ampMin  = bps[0].amplitude, ampMax  = bps[0].amplitude;
        for (const auto& bp : bps)
        {
            freqMin = std::min(freqMin, bp.frequency);
            freqMax = std::max(freqMax, bp.frequency);
            ampMin  = std::min(ampMin,  bp.amplitude);
            ampMax  = std::max(ampMax,  bp.amplitude);
        }

        const int peakVel = ampToVelocity(p->peakAmplitude(),
                                           opts.ampFloorLinear,
                                           opts.ampCeilLinear);

        // ── MIDI file ─────────────────────────────────────────────────────────

        const juce::String filename = "partial_"
            + juce::String(fileIndex).paddedLeft('0', padWidth) + ".mid";

        juce::MidiFile midi;
        midi.setTicksPerQuarterNote(opts.ppq);

        juce::MidiMessageSequence seq;
        seq.addEvent(juce::MidiMessage::tempoMetaEvent(
            static_cast<int>(60'000'000.0 / opts.tempoBpm)), 0);

        constexpr int ch = 1;
        const int bendTick       = timeToTick(bps.front().time);
        const double bendRangeCents = bendRange * 100.0;

        // RPN: set pitch bend sensitivity for this channel
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 0),        bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 0),        bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 6, bendRange),  bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 38, 0),         bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 127),      bendTick);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 127),      bendTick);

        const int velocity = ampToVelocity(bps.front().amplitude,
                                            opts.ampFloorLinear, opts.ampCeilLinear);
        seq.addEvent(juce::MidiMessage::noteOn(ch, nomNote,
                                                static_cast<uint8_t>(velocity)),
                     timeToTick(bps.front().time));

        for (const auto& bp : bps)
        {
            const int    tick     = timeToTick(bp.time);
            const double devCents = deviationCents(bp.frequency, nomNote);
            const double norm     = juce::jlimit(-1.0, 1.0, devCents / bendRangeCents);
            const int    bend14   = static_cast<int>(std::round(norm * 8191.0)) + 8192;
            seq.addEvent(juce::MidiMessage::pitchWheel(ch, bend14), tick);

            const int cc11 = ampToVelocity(bp.amplitude,
                                            opts.ampFloorLinear, opts.ampCeilLinear);
            seq.addEvent(juce::MidiMessage::controllerEvent(ch, 11, cc11), tick);
        }

        seq.addEvent(juce::MidiMessage::noteOff(ch, nomNote),
                     timeToTick(bps.back().time));

        seq.sort();
        midi.addTrack(seq);

        auto midiStream = outputFolder.getChildFile(filename).createOutputStream();
        if (midiStream == nullptr || !midi.writeTo(*midiStream))
        {
            result.errorMessage = "Failed to write: " + filename;
            return result;
        }

        // ── Collect BOM row ───────────────────────────────────────────────────

        bomRows.push_back({ filename, nomNote, midiNoteName(nomNote),
                            bps.front().time, bps.back().time,
                            bps.back().time - bps.front().time,
                            freqMin, freqMax,
                            linearToDb(ampMin), linearToDb(ampMax),
                            peakVel, bendRange, maxDevCts, p->totalEnergy() });

        ++fileIndex;
        ++result.filesWritten;
    }

    // ── BOM CSV ───────────────────────────────────────────────────────────────

    {
        juce::String csv =
            "filename,note_number,note_name,start_s,end_s,duration_s,"
            "freq_min_hz,freq_max_hz,amp_min_db,amp_max_db,"
            "peak_velocity,bend_range_semitones,max_deviation_cents,energy\n";

        for (const auto& r : bomRows)
        {
            csv += r.filename + ","
                + juce::String(r.noteNumber) + ","
                + r.noteName + ","
                + juce::String(r.startS, 4) + ","
                + juce::String(r.endS, 4) + ","
                + juce::String(r.durationS, 4) + ","
                + juce::String(r.freqMinHz, 2) + ","
                + juce::String(r.freqMaxHz, 2) + ","
                + juce::String(r.ampMinDb, 1) + ","
                + juce::String(r.ampMaxDb, 1) + ","
                + juce::String(r.peakVelocity) + ","
                + juce::String(r.bendRangeSemitones) + ","
                + juce::String(r.maxDevCents, 1) + ","
                + juce::String(r.energy, 5) + "\n";
        }

        auto stream = outputFolder.getChildFile("bom.csv").createOutputStream();
        if (stream == nullptr || !stream->writeText(csv, false, false, nullptr))
        {
            result.errorMessage = "Failed to write bom.csv";
            return result;
        }
    }

    // ── README ────────────────────────────────────────────────────────────────

    {
        int dist2 = 0, dist12 = 0, dist24 = 0, dist48 = 0;
        for (const auto& r : bomRows)
        {
            if      (r.bendRangeSemitones == 2)  ++dist2;
            else if (r.bendRangeSemitones == 12) ++dist12;
            else if (r.bendRangeSemitones == 24) ++dist24;
            else                                  ++dist48;
        }

        const juce::String dateStr =
            juce::Time::getCurrentTime().toString(true, true, false, true);

        juce::String readme;
        readme += "PartialFKR MIDI Package Export\n";
        readme += "================================\n\n";
        readme += "Source File:              " + sourceAudioFile.getFileName() + "\n";
        readme += "Export Date:              " + dateStr + "\n";
        readme += "Partials Exported:        " + juce::String(result.filesWritten) + "\n\n";
        readme += "Playback Parameters:\n";
        readme += "  Tempo:                  " + juce::String(opts.tempoBpm, 1) + " BPM\n";
        readme += "  Ticks Per Quarter Note: " + juce::String(opts.ppq) + "\n\n";
        readme += "Pitch Bend Range Distribution:\n";
        readme += "  +/- 2  semitones:       " + juce::String(dist2)  + " partials\n";
        readme += "  +/- 12 semitones:       " + juce::String(dist12) + " partials\n";
        readme += "  +/- 24 semitones:       " + juce::String(dist24) + " partials\n";
        readme += "  +/- 48 semitones:       " + juce::String(dist48) + " partials\n\n";
        readme += "Files:\n";
        readme += "  bom.csv              Bill of materials listing metadata for each partial\n";
        readme += "  partial_NNN.mid      One MIDI file per partial\n\n";
        readme += "Usage:\n";
        readme += "  Import each MIDI file onto a separate track in your DAW.\n";
        readme += "  Set each instrument's pitch bend range to the value in the\n";
        readme += "  bend_range_semitones column of bom.csv.\n";
        readme += "  CC11 (Expression) carries the amplitude envelope.\n";
        readme += "  Velocity reflects the initial amplitude of each partial.\n";
        readme += "  Amplitude mapping: -80 dBFS -> velocity 1,  0 dBFS -> velocity 127.\n";

        auto stream = outputFolder.getChildFile("readme.txt").createOutputStream();
        if (stream == nullptr || !stream->writeText(readme, false, false, nullptr))
        {
            result.errorMessage = "Failed to write readme.txt";
            return result;
        }
    }

    result.success = true;
    return result;
}