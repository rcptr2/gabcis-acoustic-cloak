/*
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by
    Gabor Tomori and Gemini, under Gabor Tomori's direction and with Gemini's
    review, in 2026.
    Tested on Intel Mac OS 15.7, FL Studio 2026
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Full-editor-size overlay shown/hidden by the "About" button. Carries the
// required plugin signature (see PluginSignature.h) plus a short factual
// feature summary. Click anywhere, or the Close button, to dismiss.
class AboutPanel final : public juce::Component
{
public:
    AboutPanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutPanel)
};
