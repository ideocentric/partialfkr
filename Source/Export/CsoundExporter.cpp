// SPDX-License-Identifier: AGPL-3.0-or-later
#include "CsoundExporter.h"

#include <algorithm>
#include <cmath>

bool CsoundExporter::exportToFile(
    const std::vector<std::unique_ptr<Partial>>& partials,
    const Options&                               opts,
    const juce::File&                            outputFile)
{
    if (partials.empty())
        return false;

    auto stream = outputFile.createOutputStream();
    if (stream == nullptr)
        return false;

    const int sr    = opts.sampleRate;
    const int ksmps = (opts.ksmps > 0)
                        ? opts.ksmps
                        : std::max(1, static_cast<int>(std::round(sr * opts.kInterval)));

    // ── Orchestra ─────────────────────────────────────────────────────────────

    juce::String orc;
    orc += "sr     = " + juce::String(sr) + "\n";
    orc += "ksmps  = " + juce::String(ksmps)
        +  "  ; " + juce::String(static_cast<double>(ksmps) / sr * 1000.0, 2)
        +  " ms per k-cycle\n";
    orc += "nchnls = 2\n";
    orc += "0dbfs  = 1\n\n";

    orc += "; Sinusoidal partial resynthesis from GEN07 breakpoint tables.\n";
    orc += "; p4 = frequency envelope table, p5 = amplitude envelope table\n";
    orc += "instr 1\n";
    orc += "  idur     =  p3\n";
    orc += "  ifreqtab =  p4\n";
    orc += "  iamptab  =  p5\n";
    orc += "  isz      =  ftlen(ifreqtab)  ; actual data points (not power-of-2 padded)\n\n";
    orc += "  ; Linear ramp 0->isz in raw index units — avoids xmode=1 power-of-2 issues\n";
    orc += "  aindex   line     0, idur, isz\n\n";
    orc += "  ; Read Hz and linear amplitude from GEN07 envelope tables (raw index mode)\n";
    orc += "  afreq    tablei   aindex, ifreqtab\n";
    orc += "  aamp     tablei   aindex, iamptab\n\n";
    orc += "  ; 10 ms linear ramp at onset and offset to prevent clicks\n";
    orc += "  aamp     linen    aamp, 0.01, idur, 0.01\n\n";
    orc += "  ; Sinusoidal oscillator — phase accumulator handles varying frequency\n";
    orc += "  asig     poscil   aamp, afreq, 1\n\n";
    orc += "           outs     asig, asig\n";
    orc += "endin\n";

    // ── Score ─────────────────────────────────────────────────────────────────

    juce::String sco;
    sco += "; Table 1: sine wave\n";
    sco += "f 1  0  4096  10  1\n\n";

    struct IStatement {
        double onset, duration;
        int    freqTab, ampTab;
    };
    std::vector<IStatement> iStatements;
    iStatements.reserve(partials.size());

    int tableNum = 2;

    for (const auto& p : partials)
    {
        const auto& bps = p->breakpoints;
        if (bps.size() < 2)
            continue;

        const int B = static_cast<int>(bps.size());

        // Compute per-segment frame counts (proportional to time intervals)
        std::vector<int> frames;
        frames.reserve(static_cast<size_t>(B - 1));
        int totalFrames = 0;
        for (int i = 0; i < B - 1; ++i)
        {
            const double dt = bps[static_cast<size_t>(i + 1)].time
                            - bps[static_cast<size_t>(i)].time;
            const int f = std::max(1, static_cast<int>(std::round(dt / opts.kInterval)));
            frames.push_back(f);
            totalFrames += f;
        }

        // Frequency table (GEN07 — linear interpolation between breakpoints)
        juce::String freqLine = "f " + juce::String(tableNum)
            + "  0  -" + juce::String(totalFrames) + "  7";
        for (int i = 0; i < B; ++i)
        {
            freqLine += "  " + juce::String(bps[static_cast<size_t>(i)].frequency, 3);
            if (i < B - 1)
                freqLine += "  " + juce::String(frames[static_cast<size_t>(i)]);
        }
        sco += freqLine + "\n";

        // Amplitude table (GEN07)
        juce::String ampLine = "f " + juce::String(tableNum + 1)
            + "  0  -" + juce::String(totalFrames) + "  7";
        for (int i = 0; i < B; ++i)
        {
            ampLine += "  " + juce::String(bps[static_cast<size_t>(i)].amplitude, 6);
            if (i < B - 1)
                ampLine += "  " + juce::String(frames[static_cast<size_t>(i)]);
        }
        sco += ampLine + "\n";

        iStatements.push_back({ p->startTime(), p->duration(), tableNum, tableNum + 1 });
        tableNum += 2;
    }

    sco += "\n";
    sco += "; ── Instrument events ────────────────────────────────────────────────────────\n";
    sco += "; instr   onset(s)      duration(s)   freq_table   amp_table\n";
    for (const auto& s : iStatements)
    {
        sco += "i 1  "
            + juce::String(s.onset,    6) + "  "
            + juce::String(s.duration, 6) + "  "
            + juce::String(s.freqTab)     + "  "
            + juce::String(s.ampTab)      + "\n";
    }
    sco += "\ne\n";

    // ── CSD wrapper ───────────────────────────────────────────────────────────

    juce::String csd;
    csd += "<CsoundSynthesizer>\n\n";
    csd += "<CsOptions>\n";
    csd += "-o dac\n";
    csd += "</CsOptions>\n\n";
    csd += "<CsInstruments>\n";
    csd += orc;
    csd += "</CsInstruments>\n\n";
    csd += "<CsScore>\n";
    csd += sco;
    csd += "</CsScore>\n\n";
    csd += "</CsoundSynthesizer>\n";

    return stream->writeText(csd, false, false, nullptr);
}