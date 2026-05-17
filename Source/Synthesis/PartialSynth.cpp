// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PartialSynth.h"

#include "Model/Project.h"

#include <cmath>

static constexpr double kTwoPi = 6.283185307179586;

PartialSynth::PartialSynth() = default;

void PartialSynth::prepareToPlay(double sr, int /*blockSize*/)
{
    sampleRate = sr;
    resetStates();
}

void PartialSynth::releaseResources()
{
    states.clear();
}

void PartialSynth::setProject(Project* p)
{
    project = p;
    resetStates();
}

void PartialSynth::setPlayheadTime(double seconds)
{
    playheadSeconds = seconds;
    resetStates();
}

void PartialSynth::resetStates()
{
    if (project == nullptr) {
        states.clear();
        return;
    }

    const auto& partials = project->getPartials();
    states.resize(partials.size());

    for (size_t i = 0; i < partials.size(); ++i) {
        const auto& bps = partials[i]->breakpoints;

        // Pre-seek bpIndex to the correct breakpoint pair for playheadSeconds
        // so the per-sample loop doesn't have to walk from zero.
        size_t idx = 0;
        while (idx + 1 < bps.size() - 1 && bps[idx + 1].time <= playheadSeconds)
            ++idx;

        states[i].bpIndex = idx;
        states[i].phase   = 0.0;   // phase starts at zero — Loris phase not used
    }
}

void PartialSynth::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                   int startSample,
                                   int numSamples)
{
    if (project == nullptr || sampleRate <= 0.0)
        return;

    const bool scrubbing = isScrubbing.load(std::memory_order_acquire);

    if (!isPlaying.load(std::memory_order_relaxed) && !scrubbing)
        return;

    // In scrub mode, pin the playhead to the cursor and re-seek bpIndex
    // without touching phases — lets the oscillators morph smoothly as
    // the user drags rather than clicking from phase resets.
    if (scrubbing)
    {
        const double scrubT = scrubTargetSeconds.load(std::memory_order_relaxed);
        playheadSeconds = scrubT;
        const auto& ps = project->getPartials();
        if (states.size() != ps.size())
            resetStates();
        for (size_t i = 0; i < ps.size(); ++i)
        {
            const auto& bps = ps[i]->breakpoints;
            size_t idx = 0;
            while (idx + 1 < bps.size() - 1 && bps[idx + 1].time <= scrubT)
                ++idx;
            states[i].bpIndex = idx;
            // state.phase intentionally NOT reset — smooth morph on drag
        }
    }

    const double blockDuration = static_cast<double>(numSamples) / sampleRate;
    const double tStart        = playheadSeconds;
    const double tEnd          = tStart + blockDuration;

    const bool soloActive = hasSolo.load(std::memory_order_relaxed);

    float* outL = outputBuffer.getWritePointer(0, startSample);
    float* outR = outputBuffer.getNumChannels() > 1
                      ? outputBuffer.getWritePointer(1, startSample)
                      : nullptr;

    const auto& partials = project->getPartials();
    if (!scrubbing && states.size() != partials.size())
        resetStates();

    for (size_t i = 0; i < partials.size(); ++i) {
        const Partial& p = *partials[i];

        // audio thread — relaxed loads are sufficient
        if (p.muted.load(std::memory_order_relaxed))
            continue;
        if (soloActive && !p.soloed.load(std::memory_order_relaxed))
            continue;
        if (p.endTime() < tStart || p.startTime() > tEnd)
            continue;

        synthesizePartial(p, states[i], outL, numSamples);
    }

    // Master gain then copy L→R
    const float gain = outputGain.load(std::memory_order_relaxed);
    juce::FloatVectorOperations::multiply(outL, gain, numSamples);
    if (outR != nullptr)
        juce::FloatVectorOperations::copy(outR, outL, numSamples);

    // Don't advance playhead while scrubbing — position is owned by the cursor
    if (!scrubbing)
        playheadSeconds = tEnd;
}

void PartialSynth::synthesizePartial(const Partial& p,
                                     OscState&      state,
                                     float*         out,
                                     int            numSamples) noexcept
{
    const auto& bps = p.breakpoints;
    if (bps.size() < 2)
        return;

    const double dt = 1.0 / sampleRate;

    for (int n = 0; n < numSamples; ++n) {
        const double t = playheadSeconds + static_cast<double>(n) * dt;

        // Advance breakpoint cursor: find the pair [idx, idx+1] bracketing t.
        // Loop stops one before the last pair so i+1 is always valid.
        while (state.bpIndex + 1 < bps.size() - 1 && bps[state.bpIndex + 1].time <= t)
            ++state.bpIndex;

        const size_t i = state.bpIndex;
        if (i + 1 >= bps.size())
            break;

        const Breakpoint& b0 = bps[i];
        const Breakpoint& b1 = bps[i + 1];

        // Alpha: position within the current breakpoint segment [0, 1]
        const double span  = b1.time - b0.time;
        const double alpha = (span > 0.0) ? juce::jlimit(0.0, 1.0, (t - b0.time) / span) : 0.0;

        // Amplitude — linear interpolation (same as csound linseg on amplitude)
        const float amp = static_cast<float>(b0.amplitude + alpha * (b1.amplitude - b0.amplitude));

        // Frequency — linear interpolation in Hz (csound linseg on frequency).
        // Treating breakpoints as control points: the oscillator tracks the
        // linearly-interpolated value at each sample, exactly like k-rate linseg→oscili.
        const float freq = static_cast<float>(b0.frequency + alpha * (b1.frequency - b0.frequency));

        if (freq <= 0.0f)
            continue;

        // Accumulate phase; single-step wrap is sufficient since the per-sample
        // increment (2π × freq / sr) is always < 2π for audible frequencies.
        state.phase += kTwoPi * static_cast<double>(freq) * dt;
        if (state.phase >= kTwoPi) state.phase -= kTwoPi;

        out[n] += amp * static_cast<float>(std::sin(state.phase));
    }
}