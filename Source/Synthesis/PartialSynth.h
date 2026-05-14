// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"

#include <JuceHeader.h>

#include <atomic>
#include <vector>

class Project;

/**
 * Real-time oscillator bank that synthesizes the active (unmuted) partials.
 *
 * All methods except setProject/setPlayheadTime are called from the audio thread.
 * setProject is called while the audio thread is not running (device stopped
 * or under CriticalSection).
 */
class PartialSynth {
public:
    PartialSynth();

    void prepareToPlay(double sampleRate, int blockSize);
    void releaseResources();

    /** Called between audio blocks to swap in a new project. */
    void setProject(Project* project);

    /** Called by the transport to seek; resets all oscillator phases. */
    void setPlayheadTime(double seconds);

    void setPlaying(bool shouldPlay) noexcept { isPlaying.store(shouldPlay); }
    [[nodiscard]] bool getIsPlaying() const noexcept { return isPlaying.load(); }

    /** Inform the synth whether any partial is currently soloed. */
    void setSolo(bool anySoloed) noexcept { hasSolo.store(anySoloed); }

    /** Master output gain (linear). Default -20 dB (0.1) prevents clipping on
     *  dense analyses where hundreds of partials sum. Adjust per source material. */
    void  setOutputGain(float gain) noexcept { outputGain.store(gain); }
    [[nodiscard]] float getOutputGain() const noexcept { return outputGain.load(); }

    /** Read back the current playhead position (for display). */
    [[nodiscard]] double getPlayheadSeconds() const noexcept { return playheadSeconds; }

    // audio thread
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample,
                         int numSamples);

private:
    struct OscState {
        size_t bpIndex = 0;   ///< current breakpoint pair index
        double phase   = 0.0; ///< radians, running accumulator
    };

    // audio thread
    void synthesizePartial(const Partial& p,
                           OscState&      state,
                           float*         out,
                           int            numSamples) noexcept;

    // audio thread
    void resetStates();

    Project* project    = nullptr;
    double   sampleRate = 44100.0;
    double   playheadSeconds = 0.0;

    std::atomic<bool>  isPlaying{false};
    std::atomic<bool>  hasSolo{false};
    std::atomic<float> outputGain{0.1f};  // -20 dB default
    std::vector<OscState> states;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PartialSynth)
};