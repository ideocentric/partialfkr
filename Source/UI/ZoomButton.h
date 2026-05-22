// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * Small vector-drawn zoom button showing either a + or − symbol.
 * Renders at any DPI without external assets.
 * States: normal, hover, pressed, disabled — all drawn from paths.
 */
class ZoomButton : public juce::Button {
public:
    enum class Sign { Plus, Minus };

    explicit ZoomButton(Sign s) : juce::Button({}), sign(s) {}

protected:
    void paintButton(juce::Graphics& g,
                     bool isMouseOverButton,
                     bool isButtonDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const float cx    = bounds.getCentreX();
        const float cy    = bounds.getCentreY();
        const float r     = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
        const float arm   = r * 0.55f;
        const float thick = r * 0.18f;

        // Background rounded square
        const juce::Colour bg = isButtonDown       ? juce::Colour(0xff4a4a4a)
                               : isMouseOverButton  ? juce::Colour(0xff3a3a3a)
                               : isEnabled()        ? juce::Colour(0xff2a2a2a)
                                                    : juce::Colour(0xff1e1e1e);
        constexpr float cornerR = 3.0f;
        g.setColour(bg);
        g.fillRoundedRectangle(bounds, cornerR);

        // Border
        g.setColour(juce::Colour(0xff606060));
        g.drawRoundedRectangle(bounds, cornerR, 0.8f);

        // Symbol
        const juce::Colour sym = isEnabled() ? juce::Colours::lightgrey
                                              : juce::Colour(0xff444444);
        g.setColour(sym);

        // Horizontal bar (both + and −)
        g.fillRoundedRectangle(cx - arm, cy - thick * 0.5f, arm * 2.0f, thick, thick * 0.4f);

        // Vertical bar (+ only)
        if (sign == Sign::Plus)
            g.fillRoundedRectangle(cx - thick * 0.5f, cy - arm, thick, arm * 2.0f, thick * 0.4f);
    }

private:
    Sign sign;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoomButton)
};