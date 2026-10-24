#pragma once
#include <JuceHeader.h>
#include "SceneManager.h"

namespace LiveBus
{

class CopyToolsComponent : public juce::Component
{
public:
    CopyToolsComponent();
    ~CopyToolsComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setActiveSceneName(const juce::String& name);

    // Callbacks to engine
    std::function<void(int srcTarget, int destTarget)> onCopyEQ;
    std::function<void(int srcTarget, int destTarget)> onCopyVocalDSP;
    std::function<void(int srcCh, int destCh, bool includeSends)> onCopyChannel;
    std::function<void()> onUpdateActiveScene;
    std::function<void(int targetIndex)> onResetTargetDefaults;

private:
    juce::String activeSceneName{ "Default" };

    // Update active scene section
    juce::Label activeSceneLabel;
    juce::TextButton updateSceneBtn{ "Update Active Scene Now" };

    // Copy 8-Band EQ section
    juce::ComboBox eqSourceCombo;
    juce::ComboBox eqDestCombo;
    juce::TextButton copyEQBtn{ "Copy 8-Band EQ" };

    // Copy Vocal DSP section
    juce::ComboBox dspSourceCombo;
    juce::ComboBox dspDestCombo;
    juce::TextButton copyDSPBtn{ "Copy Vocal DSP Rack" };

    // Copy Channel Strip section
    juce::ComboBox chSourceCombo;
    juce::ComboBox chDestCombo;
    juce::ToggleButton chIncludeSendsToggle{ "Include Send Levels" };
    juce::TextButton copyChBtn{ "Copy Channel Settings" };

    // Reset defaults section
    juce::ComboBox resetTargetCombo;
    juce::TextButton resetDefaultsBtn{ "Reset Target To Defaults" };

    juce::Label statusLabel;

    void populateTargetCombo(juce::ComboBox& box);
    void populateChannelCombo(juce::ComboBox& box);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CopyToolsComponent)
};

} // namespace LiveBus
