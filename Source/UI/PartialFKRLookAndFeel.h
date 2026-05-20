// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * Application-wide LookAndFeel.
 * Installed once as the default in Main.cpp; provides dark-theme color
 * overrides for sliders and generic TextButtons without touching any
 * component that overrides paint() directly (TransportButton, ZoomButton).
 *
 * Components that call setColour() on themselves (Select / Direct Select
 * buttons) are unaffected — per-component colors always take precedence.
 */
class PartialFKRLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PartialFKRLookAndFeel()
    {
        // ── Sliders ───────────────────────────────────────────────────────────
        setColour(juce::Slider::thumbColourId,             juce::Colour(0xffffffff));
        setColour(juce::Slider::trackColourId,             juce::Colour(0xff505050));
        setColour(juce::Slider::backgroundColourId,        juce::Colour(0xff252525));
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffcccccc));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xff000000));
        setColour(juce::Slider::textBoxHighlightColourId,  juce::Colour(0xff4488ff));

        // ── TextButtons (Reset; Select/Direct Select override these locally) ─
        setColour(juce::TextButton::buttonColourId,        juce::Colour(0xff2a2a2a));
        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff505050));
        setColour(juce::TextButton::textColourOffId,       juce::Colour(0xffaaaaaa));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffffffff));

        // ── ComboBox (if used in future) ──────────────────────────────────────
        setColour(juce::ComboBox::backgroundColourId,      juce::Colour(0xff2a2a2a));
        setColour(juce::ComboBox::textColourId,            juce::Colour(0xffcccccc));
        setColour(juce::ComboBox::outlineColourId,         juce::Colour(0xff444444));
        setColour(juce::ComboBox::arrowColourId,           juce::Colour(0xff888888));

        // ── Labels ────────────────────────────────────────────────────────────
        // Only affects labels that do NOT call setFont() themselves.
        // All current panel-title and data labels set their font explicitly,
        // so this acts as a safe fallback for any future unlabelled labels.
        setColour(juce::Label::textColourId,               juce::Colour(0xffcccccc));
        setColour(juce::Label::backgroundColourId,         juce::Colour(0x00000000));

        // ── ScrollBar ─────────────────────────────────────────────────────────
        setColour(juce::ScrollBar::backgroundColourId,     juce::Colour(0xff0d0d0d));
        setColour(juce::ScrollBar::thumbColourId,          juce::Colour(0xff3a3a3a));
    }
};