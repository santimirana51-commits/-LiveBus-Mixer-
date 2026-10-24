#pragma once
#include <JuceHeader.h>
#include "AudioSafetyProcessor.h"

namespace LiveBus
{

class AudioSafetyComponent : public juce::Component,
                             private juce::Timer,
                             private juce::Button::Listener,
                             private juce::Slider::Listener
{
public:
    explicit AudioSafetyComponent(AudioSafetyProcessor& safetyRef);
    ~AudioSafetyComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateUIFromDSP();

private:
    void timerCallback() override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    AudioSafetyProcessor& safety;
    bool isUpdatingFromDSP = false;

    // Header
    juce::Label headerTitle{"Audio Safety, Anti-Feedback & Output Protection"};
    juce::Label masterStatusPill;

    // Column 1: Output Protection Limiters
    juce::GroupComponent limiterGroup{"limiterGroup", "1. OUTPUT BUS BRICKWALL LIMITERS"};
    juce::ToggleButton streamLimiterToggle{"Stream Bus Limiter (OBS Protection)"};
    juce::Label streamCeilingLabel{"Stream Ceiling (dBFS):"};
    juce::Slider streamCeilingSlider;
    juce::Label streamReleaseLabel{"Stream Release (ms):"};
    juce::Slider streamReleaseSlider;

    juce::ToggleButton monitorLimiterToggle{"Monitor Bus Limiter (Headphone Protection)"};
    juce::Label monitorCeilingLabel{"Monitor Ceiling (dBFS):"};
    juce::Slider monitorCeilingSlider;
    juce::Label monitorReleaseLabel{"Monitor Release (ms):"};
    juce::Slider monitorReleaseSlider;

    // Column 2: Clipping & Anti-Feedback Risk
    juce::GroupComponent riskGroup{"riskGroup", "2. CLIPPING MONITOR & ANTI-FEEDBACK"};
    juce::Label clipStatusBadge;
    juce::Label clipCountLabel{"Clips detected: Stream 0 | Monitor 0"};
    juce::TextButton resetPeakBtn{"Clear Clip Warnings / Reset Peak"};

    juce::Label feedbackHeaderLabel{"Routing Risk Assessment:"};
    juce::Label feedbackStatusBadge;
    juce::TextEditor feedbackDescriptionBox;

    // Column 3: Emergency Mute Panel
    juce::GroupComponent emergencyGroup{"emergencyGroup", "3. EMERGENCY MUTE CONTROLS"};
    juce::TextButton emergencyMuteAllBtn{"EMERGENCY MUTE ALL"};
    juce::TextButton emergencyMuteStreamBtn{"Mute Stream Bus Only"};
    juce::TextButton emergencyMuteMonitorBtn{"Mute Monitor Bus Only"};
    juce::TextButton unmuteAllBtn{"RESTORE / UNMUTE ALL AUDIO"};
    juce::Label emergencyStatusLabel;

    // Meter values for paint
    float currentStreamPeakDb = -100.0f;
    float currentMonitorPeakDb = -100.0f;
    float currentStreamGrDb = 0.0f;
    float currentMonitorGrDb = 0.0f;
    bool isFlashing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSafetyComponent)
};

} // namespace LiveBus
