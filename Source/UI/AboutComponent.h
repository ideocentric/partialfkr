// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * About dialog content. Launched via show(); fully cross-platform.
 */
class AboutComponent : public juce::Component {
public:
    AboutComponent();
    void resized() override;

    static constexpr int kWidth  = 380;
    static constexpr int kHeight = 230;

    /** Create and show the about dialog centred on the given component. */
    static void show(juce::Component* centreAround);

private:
    juce::Label nameLabel;
    juce::Label versionLabel;
    juce::Label descLabel;
    juce::Label licenseLabel;
    juce::Label copyrightLabel;

    juce::TextButton okButton{"OK"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutComponent)
};