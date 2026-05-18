// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SuperColliderExporter.h"

#include <cmath>

bool SuperColliderExporter::exportToFile(
    const std::vector<std::unique_ptr<Partial>>& partials,
    const Options&                               opts,
    const juce::File&                            outputFile)
{
    if (partials.empty())
        return false;

    auto stream = outputFile.createOutputStream();
    if (stream == nullptr)
        return false;

    // Format a double array. Arrays longer than wrapAt values wrap onto
    // successive lines with 16-space indentation inside the Event literal.
    auto formatArray = [](const std::vector<double>& vals,
                          int precision,
                          int wrapAt = 8) -> juce::String
    {
        if (vals.size() <= static_cast<size_t>(wrapAt))
        {
            juce::String s = "[";
            for (size_t i = 0; i < vals.size(); ++i)
            {
                if (i > 0) s += ", ";
                s += juce::String(vals[i], precision);
            }
            return s + "]";
        }

        juce::String s = "[\n";
        for (size_t i = 0; i < vals.size(); ++i)
        {
            if (i % static_cast<size_t>(wrapAt) == 0)
                s += "                ";
            s += juce::String(vals[i], precision);
            if (i + 1 < vals.size())
            {
                s += ",";
                s += (i % static_cast<size_t>(wrapAt) == static_cast<size_t>(wrapAt) - 1)
                     ? "\n" : " ";
            }
        }
        return s + "\n            ]";
    };

    size_t count = 0;
    for (const auto& p : partials)
        if (p->breakpoints.size() >= 2) ++count;

    juce::String scd;

    // ── Header ────────────────────────────────────────────────────────────────

    scd += "// PartialFKR SuperCollider Export\n";
    scd += "// " + juce::String((int)count) + " partial" + (count == 1 ? "" : "s")
        +  " \xe2\x80\x94 additive sinusoidal resynthesis\n";
    scd += "// Open in the SuperCollider IDE and evaluate this block:\n";
    scd += "// place cursor inside the outer parens, press Cmd+Return (macOS) / Ctrl+Return (Win/Linux)\n\n";

    scd += "(\n";
    scd += "s.waitForBoot {\n\n";

    // ── Breakpoint data (var declarations must precede all statements) ─────────

    scd += "    var partials = [\n";

    bool firstPartial = true;
    for (const auto& p : partials)
    {
        const auto& bps = p->breakpoints;
        if (bps.size() < 2) continue;

        const int B = static_cast<int>(bps.size());

        std::vector<double> freqLevels, ampLevels, segTimes;
        freqLevels.reserve(static_cast<size_t>(B));
        ampLevels.reserve(static_cast<size_t>(B));
        segTimes.reserve(static_cast<size_t>(B - 1));

        for (int i = 0; i < B; ++i)
        {
            freqLevels.push_back(bps[static_cast<size_t>(i)].frequency);
            ampLevels.push_back(bps[static_cast<size_t>(i)].amplitude);
        }
        for (int i = 0; i < B - 1; ++i)
        {
            segTimes.push_back(bps[static_cast<size_t>(i + 1)].time
                             - bps[static_cast<size_t>(i)].time);
        }

        if (!firstPartial) scd += ",\n";
        firstPartial = false;

        scd += "        // onset: " + juce::String(p->startTime(), 6)
            +  "  duration: "       + juce::String(p->duration(),  6) + "\n";
        scd += "        (\n";
        scd += "            freqLevels: " + formatArray(freqLevels, 3) + ",\n";
        scd += "            freqTimes:  " + formatArray(segTimes, 6)   + ",\n";
        scd += "            ampLevels:  " + formatArray(ampLevels, 6)  + ",\n";
        scd += "            onset: " + juce::String(p->startTime(), 6) + ",\n";
        scd += "            dur:   " + juce::String(p->duration(),  6) + "\n";
        scd += "        )";
    }

    scd += "\n    ];\n";
    scd += "    var buffers;\n\n";

    // ── SynthDef ─────────────────────────────────────────────────────────────

    scd += "    // ── SynthDef \xe2\x80\x94 one definition shared by all partials ───────────────────\n";
    scd += "    SynthDef(\\pfkrPartial, { |freqBuf = 0, ampBuf = 0, dur = 1, out = 0|\n";
    scd += "        var nFrames = BufFrames.kr(freqBuf) - 1;\n";
    scd += "        var phase   = Line.ar(0, nFrames, dur, doneAction: Done.freeSelf);\n";
    scd += "        var freq    = BufRd.ar(1, freqBuf, phase, loop: 0, interpolation: 2);\n";
    scd += "        var amp     = BufRd.ar(1, ampBuf,  phase, loop: 0, interpolation: 2);\n";
    scd += "        var env     = EnvGen.kr(Env.linen(0.01, (dur - 0.02).max(0.001), 0.01));\n";
    scd += "        Out.ar(out, SinOsc.ar(freq, 0, amp * env) ! 2);\n";
    scd += "    }).add;\n\n";

    scd += "    s.sync;\n\n";

    // ── Buffer loading ────────────────────────────────────────────────────────

    const double msPerFrame = opts.kInterval * 1000.0;
    scd += "    // ── Discretize envelopes at " + juce::String(msPerFrame, 1)
        +  " ms and load into server buffers ─────────────\n";
    scd += "    buffers = partials.collect { |p|\n";
    scd += "        var n = (p[\\dur] / "
        +  juce::String(opts.kInterval, 4) + ").ceil.asInteger + 1;\n";
    scd += "        [\n";
    scd += "            Buffer.loadCollection(s,"
           " Env(p[\\freqLevels], p[\\freqTimes], \\lin).asSignal(n).asArray),\n";
    scd += "            Buffer.loadCollection(s,"
           " Env(p[\\ampLevels],  p[\\freqTimes], \\lin).asSignal(n).asArray),\n";
    scd += "        ]\n";
    scd += "    };\n\n";

    scd += "    s.sync;\n\n";

    // ── Scheduling ────────────────────────────────────────────────────────────

    scd += "    // ── Schedule one Synth per partial at its analysis onset time ──────────\n";
    scd += "    partials.do { |p, i|\n";
    scd += "        SystemClock.sched(p[\\onset], {\n";
    scd += "            Synth(\\pfkrPartial, [\n";
    scd += "                \\freqBuf, buffers[i][0],\n";
    scd += "                \\ampBuf,  buffers[i][1],\n";
    scd += "                \\dur,     p[\\dur],\n";
    scd += "            ]);\n";
    scd += "            nil\n";
    scd += "        });\n";
    scd += "    };\n\n";

    scd += "    // To free buffers after playback: buffers.do { |pair| pair.do(_.free) };\n\n";

    scd += "};\n";
    scd += ")\n";

    return stream->writeText(scd, false, false, nullptr);
}