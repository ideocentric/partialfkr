// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioExporter.h"

#include "Model/Project.h"
#include "Synthesis/PartialSynth.h"

static constexpr int kBlockSize = 4096;

juce::String AudioExporter::extensionFor(Format f) noexcept
{
    switch (f)
    {
        case Format::AIFF:      return "aiff";
        case Format::WAV:       return "wav";
        case Format::FLAC:      return "flac";
        case Format::OggVorbis: return "ogg";
        case Format::AAC:       return "aac";
        case Format::ALAC:      return "m4a";
    }
    return "aiff";
}

juce::String AudioExporter::wildcardFor(Format f) noexcept
{
    switch (f)
    {
        case Format::AIFF:      return "*.aiff;*.aif";
        case Format::WAV:       return "*.wav";
        case Format::FLAC:      return "*.flac";
        case Format::OggVorbis: return "*.ogg";
        case Format::AAC:       return "*.aac;*.m4a";
        case Format::ALAC:      return "*.m4a;*.alac";
    }
    return "*.aiff;*.aif";
}

bool AudioExporter::exportToFile(const Project& project,
                                  const Options& opts,
                                  const juce::File& destFile)
{
    const double sr = (opts.sampleRate > 0.0)     ? opts.sampleRate
                    : (project.getSampleRate() > 0.0) ? project.getSampleRate()
                    : 44100.0;

    // Determine total render duration from all partials (synth skips muted ones)
    double totalDuration = 0.0;
    for (const auto& p : project.getPartials())
        totalDuration = std::max(totalDuration, p->endTime());
    if (totalDuration <= 0.0)
        return false;

    // ── Select format and build writer ───────────────────────────────────────

    std::unique_ptr<juce::AudioFormat> fmt;
    int qualityIndex = 0;

    switch (opts.format)
    {
        case Format::AIFF:
            fmt = std::make_unique<juce::AiffAudioFormat>();
            break;

        case Format::WAV:
            fmt = std::make_unique<juce::WavAudioFormat>();
            break;

        case Format::FLAC:
        {
            fmt = std::make_unique<juce::FlacAudioFormat>();
            const auto qs = fmt->getQualityOptions();
            // FLAC quality options are ordered low→high compression.
            // opts.flacCompression is 1–8; map to the nearest option index.
            if (!qs.isEmpty())
                qualityIndex = juce::jlimit(0, qs.size() - 1, opts.flacCompression - 1);
            break;
        }

        case Format::OggVorbis:
        {
            fmt = std::make_unique<juce::OggVorbisAudioFormat>();
            const auto qs = fmt->getQualityOptions();
            if (!qs.isEmpty())
                qualityIndex = juce::jlimit(0, qs.size() - 1,
                    juce::roundToInt(opts.oggQuality * (float)(qs.size() - 1)));
            break;
        }

#if JUCE_MAC
        case Format::AAC:
        case Format::ALAC:
            fmt = std::make_unique<juce::CoreAudioFormat>();
            if (opts.format == Format::AAC)
            {
                // CoreAudioFormat quality options for AAC map to bitrate steps.
                // Map [128, 320] kbps → quality option index proportionally.
                const auto qs = fmt->getQualityOptions();
                if (!qs.isEmpty())
                {
                    const float frac = (float)(opts.aacBitrateKbps - 128) / (320.0f - 128.0f);
                    qualityIndex = juce::jlimit(0, qs.size() - 1,
                        juce::roundToInt(frac * (float)(qs.size() - 1)));
                }
            }
            break;
#else
        case Format::AAC:
        case Format::ALAC:
            // CoreAudio formats are macOS-only; fall back to AIFF on other platforms.
            fmt = std::make_unique<juce::AiffAudioFormat>();
            break;
#endif
    }

    if (fmt == nullptr)
        return false;

    auto outputStream = destFile.createOutputStream();
    if (outputStream == nullptr)
        return false;

    juce::StringPairArray metadata;
    auto* rawStream = outputStream.get();
    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt->createWriterFor(rawStream,
                             sr,
                             (unsigned int)opts.numChannels,
                             opts.bitDepth,
                             metadata,
                             qualityIndex));

    if (writer == nullptr)
        return false;

    // Writer now owns the stream
    outputStream.release();  // NOLINT(bugprone-unused-return-value)

    // ── Offline render via a fresh PartialSynth ───────────────────────────────
    // A fresh synth is used so the live playback state is never disturbed.

    PartialSynth renderSynth;
    renderSynth.setProject(&project);
    renderSynth.prepareToPlay(sr, kBlockSize);
    renderSynth.setOutputGain(1.0f);   // export at full amplitude, no monitor attenuation
    renderSynth.setSolo(project.hasAnySolo());
    renderSynth.setPlaying(true);
    renderSynth.setPlayheadTime(0.0);

    const auto totalSamples = static_cast<int64_t>(std::ceil(totalDuration * sr));
    int64_t    written      = 0;

    juce::AudioBuffer<float> buffer(opts.numChannels, kBlockSize);

    while (written < totalSamples)
    {
        const int n = (int)std::min((int64_t)kBlockSize, totalSamples - written);
        buffer.clear();
        renderSynth.renderNextBlock(buffer, 0, n);

        if (!writer->writeFromAudioSampleBuffer(buffer, 0, n))
            return false;

        written += n;

        if (opts.progressCallback)
            opts.progressCallback((double)written / (double)totalSamples);
    }

    return true;
}