#pragma once
#include <JuceHeader.h>
#include "AutoDucker.h"

namespace LiveBus
{

class DuckingComponent : public juce::Component,
                         private juce::Timer,
                         private juce::Button::Listener,
                         private juce::Slider::Listener,
                         private juce::ComboBox::Listener
{
public:
    explicit DuckingComponent(AutoDucker& duckerRef);
    ~DuckingComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateUIFromDSP();

private:
    void timerCallback() override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void comboBoxChanged(juce::ComboBox* cb) override;

    AutoDucker& ducker;
    bool isUpdatingFromDSP = false;

    // Header Controls
    juce::Label headerTitle{"Auto Ducking & Talkover System"};
    juce::ToggleButton masterEnableBtn{"ENABLE DUCKING"};
    juce::Label stateBadge;
    juce::ComboBox presetCombo;
    juce::TextButton savePresetBtn{"Save Preset"};

    // Column 1: Trigger & Sidechain
    juce::GroupComponent triggerGroup{"triggerGroup", "1. SIDECHAIN TRIGGER & DETECTOR"};
    juce::Label triggerSourceLabel{"Trigger Source:"};
    juce::ComboBox triggerSourceCombo;
    juce::Label triggerMeterLabel{"Trigger Input Level:"};
    float currentTriggerMeterDb = -100.0f;
    bool currentSidechainActive = false;

    juce::Label threshLabel{"Threshold:"};
    juce::Slider threshSlider;
    juce::Label threshValueLabel;

    juce::Label detModeLabel{"Detector Mode:"};
    juce::ComboBox detModeCombo;

    juce::ToggleButton filterEnableToggle{"Sidechain Key Filter"};
    juce::Label hpfLabel{"HPF Freq:"};
    juce::Slider hpfSlider;
    juce::Label lpfLabel{"LPF Freq:"};
    juce::Slider lpfSlider;

    // Column 2: Ducking Dynamics
    juce::GroupComponent dynamicsGroup{"dynamicsGroup", "2. DUCKING DYNAMICS & GAIN REDUCTION"};
    juce::Label depthLabel{"Ducking Depth / Attenuation:"};
    juce::Slider depthSlider;
    juce::Label depthValueLabel;

    juce::Label attackLabel{"Attack (Speed):"};
    juce::Slider attackSlider;
    juce::Label attackValueLabel;

    juce::Label holdLabel{"Hold Time:"};
    juce::Slider holdSlider;
    juce::Label holdValueLabel;

    juce::Label releaseLabel{"Release (Recovery):"};
    juce::Slider releaseSlider;
    juce::Label releaseValueLabel;

    juce::Label grMeterLabel{"Gain Reduction (Stream / Monitor):"};
    float currentGrStreamDb = 0.0f;
    float currentGrMonitorDb = 0.0f;

    // Column 3: Target Routing
    juce::GroupComponent targetGroup{"targetGroup", "3. TARGET BUSES & CHANNELS"};
    juce::ToggleButton streamEnableToggle{"Enable Ducking on Stream Bus (OBS)"};
    juce::Label streamDepthLabel{"Stream Depth Offset:"};
    juce::Slider streamDepthSlider;

    juce::ToggleButton monitorEnableToggle{"Enable Ducking on Monitor Bus (Phones)"};
    juce::Label monitorDepthLabel{"Monitor Depth Offset:"};
    juce::Slider monitorDepthSlider;

    juce::Label targetChannelsLabel{"Ducked Target Channels (Music / Aux):"};
    std::array<juce::ToggleButton, 4> channelToggles;

    juce::TextButton bypassBtn{"Bypass All Ducking"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckingComponent)
};

} // namespace LiveBus
