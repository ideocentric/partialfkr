// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <functional>

class Project;

/**
 * Renders unmuted partials to an audio file using the same synthesis engine
 * as live playback — mute and solo state are both honoured.
 *
 * Follows the same stateless pattern as MidiExporter / CsoundExporter.
 */
class AudioExporter {
public:
    enum class Format { AIFF, WAV, FLAC, OggVorbis, AAC, ALAC };

    struct Options {
        Format format          = Format::AIFF;
        double sampleRate      = 0.0;   ///< 0 = match project sample rate (fallback 44100)
        int    bitDepth        = 24;    ///< 16 / 24 / 32; 32 = IEEE float for WAV / AIFF
        int    numChannels     = 2;
        float  oggQuality      = 0.5f;  ///< 0.0 (lowest) – 1.0 (highest)
        int    flacCompression = 5;     ///< 1 (fastest) – 8 (smallest)
        int    aacBitrateKbps  = 256;   ///< AAC only: 128 / 192 / 256 / 320
        bool   alac            = false; ///< macOS only: true = ALAC lossless, false = AAC

        /** Optional 0.0–1.0 progress callback called from the render loop. */
        std::function<void(double)> progressCallback;
    };

    /**
     * Render all non-muted (solo-respecting) partials to destFile.
     * Returns true on success.  Called on the message thread; synthesis is
     * fast enough for typical partial counts without blocking the UI.
     */
    [[nodiscard]] static bool exportToFile(const Project& project,
                                           const Options& opts,
                                           const juce::File& destFile);

    /** Default file extension (no dot) for a given format. */
    [[nodiscard]] static juce::String extensionFor(Format f) noexcept;

    /** FileChooser wildcard string (e.g. "*.aiff;*.aif") for a given format. */
    [[nodiscard]] static juce::String wildcardFor(Format f) noexcept;
};