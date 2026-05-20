// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AboutComponent.h"

// Update this URL when documentation is published.
static constexpr const char* kHelpUrl = "https://github.com/ideocentric/partialfkr";

AboutComponent::AboutComponent()
{
    const auto version = juce::JUCEApplication::getInstance()->getApplicationVersion();

    auto setup = [this](juce::Label& label, const juce::String& text,
                        float fontSize, bool bold = false)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setFont(bold ? juce::Font{fontSize}.boldened() : juce::Font{fontSize});
        label.setJustificationType(juce::Justification::centred);
    };

    setup(nameLabel,      "PartialFKR",                                          22.0f, true);
    setup(versionLabel,   "Version " + version,                                  13.0f);
    setup(descLabel,      "Interactive sinusoidal analysis, editing,\n"
                          "and resynthesis.",                                     13.0f);
    setup(licenseLabel,   "Distributed under the GNU AGPLv3 License.",           12.0f);
    setup(copyrightLabel, juce::String(juce::CharPointer_UTF8("\xc2\xa9 2025 Matt Comeione")), 12.0f);

    addAndMakeVisible(okButton);
    okButton.onClick = [this] {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };

    setSize(kWidth, kHeight);
}

void AboutComponent::resized()
{
    const int pad = 16;
    auto area = getLocalBounds().reduced(pad);

    nameLabel     .setBounds(area.removeFromTop(30));
    versionLabel  .setBounds(area.removeFromTop(20));
    area.removeFromTop(12);
    descLabel     .setBounds(area.removeFromTop(36));
    area.removeFromTop(12);
    licenseLabel  .setBounds(area.removeFromTop(18));
    copyrightLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(12);

    const int btnW = 80;
    okButton.setBounds(area.removeFromTop(28)
                           .withSizeKeepingCentre(btnW, 28));
}

void AboutComponent::show(juce::Component* centreAround)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(new AboutComponent());
    opts.dialogTitle               = "About PartialFKR";
    opts.componentToCentreAround   = centreAround;
    opts.dialogBackgroundColour    = juce::LookAndFeel::getDefaultLookAndFeel()
                                         .findColour(juce::ResizableWindow::backgroundColourId);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar         = false;
    opts.resizable                 = false;
    opts.launchAsync();
}