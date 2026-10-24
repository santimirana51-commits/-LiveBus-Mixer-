#include "AudioSafetyComponent.h"

namespace LiveBus
{

AudioSafetyComponent::AudioSafetyComponent(AudioSafetyProcessor& safetyRef)
    : safety(safetyRef)
{
    // Header
    headerTitle.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour(0xfff0f6fc));
    addAndMakeVisible(headerTitle);

    masterStatusPill.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    masterStatusPill.setJustificationType(juce::Justification::centred);
    masterStatusPill.setText("PROTECTION: ACTIVE", juce::dontSendNotification);
    masterStatusPill.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(masterStatusPill);

    // Column 1: Limiters Group
    addAndMakeVisible(limiterGroup);

    streamLimiterToggle.setToggleState(true, juce::dontSendNotification);
    streamLimiterToggle.addListener(this);
    addAndMakeVisible(streamLimiterToggle);

    addAndMakeVisible(streamCeilingLabel);
    streamCeilingSlider.setRange(-24.0, 0.0, 0.1);
    streamCeilingSlider.setValue(-0.5, juce::dontSendNotification);
    streamCeilingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    streamCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    streamCeilingSlider.addListener(this);
    addAndMakeVisible(streamCeilingSlider);

    addAndMakeVisible(streamReleaseLabel);
    streamReleaseSlider.setRange(5.0, 1000.0, 5.0);
    streamReleaseSlider.setValue(50.0, juce::dontSendNotification);
    streamReleaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    streamReleaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    streamReleaseSlider.addListener(this);
    addAndMakeVisible(streamReleaseSlider);

    monitorLimiterToggle.setToggleState(true, juce::dontSendNotification);
    monitorLimiterToggle.addListener(this);
    addAndMakeVisible(monitorLimiterToggle);

    addAndMakeVisible(monitorCeilingLabel);
    monitorCeilingSlider.setRange(-24.0, 0.0, 0.1);
    monitorCeilingSlider.setValue(-0.5, juce::dontSendNotification);
    monitorCeilingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    monitorCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    monitorCeilingSlider.addListener(this);
    addAndMakeVisible(monitorCeilingSlider);

    addAndMakeVisible(monitorReleaseLabel);
    monitorReleaseSlider.setRange(5.0, 1000.0, 5.0);
    monitorReleaseSlider.setValue(50.0, juce::dontSendNotification);
    monitorReleaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    monitorReleaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    monitorReleaseSlider.addListener(this);
    addAndMakeVisible(monitorReleaseSlider);

    // Column 2: Clipping & Anti-Feedback Group
    addAndMakeVisible(riskGroup);

    clipStatusBadge.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    clipStatusBadge.setJustificationType(juce::Justification::centred);
    clipStatusBadge.setText("CLIPPING: NONE (CLEAN)", juce::dontSendNotification);
    clipStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(clipStatusBadge);

    addAndMakeVisible(clipCountLabel);

    resetPeakBtn.addListener(this);
    addAndMakeVisible(resetPeakBtn);

    addAndMakeVisible(feedbackHeaderLabel);

    feedbackStatusBadge.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    feedbackStatusBadge.setJustificationType(juce::Justification::centred);
    feedbackStatusBadge.setText("ROUTING: NORMAL (SAFE)", juce::dontSendNotification);
    feedbackStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(feedbackStatusBadge);

    feedbackDescriptionBox.setMultiLine(true);
    feedbackDescriptionBox.setReadOnly(true);
    feedbackDescriptionBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff161b22));
    feedbackDescriptionBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
    feedbackDescriptionBox.setText(safety.getRiskDescription());
    addAndMakeVisible(feedbackDescriptionBox);

    // Column 3: Emergency Mute Group
    addAndMakeVisible(emergencyGroup);

    emergencyMuteAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626)); // Red panic
    emergencyMuteAllBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    emergencyMuteAllBtn.addListener(this);
    addAndMakeVisible(emergencyMuteAllBtn);

    emergencyMuteStreamBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb91c1c));
    emergencyMuteStreamBtn.addListener(this);
    addAndMakeVisible(emergencyMuteStreamBtn);

    emergencyMuteMonitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb91c1c));
    emergencyMuteMonitorBtn.addListener(this);
    addAndMakeVisible(emergencyMuteMonitorBtn);

    unmuteAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff059669)); // Green restore
    unmuteAllBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    unmuteAllBtn.addListener(this);
    addAndMakeVisible(unmuteAllBtn);

    emergencyStatusLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    emergencyStatusLabel.setJustificationType(juce::Justification::centred);
    emergencyStatusLabel.setText("AUDIO OUTPUT: LIVE (UNMUTED)", juce::dontSendNotification);
    emergencyStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(emergencyStatusLabel);

    updateUIFromDSP();
    startTimerHz(30);
}

AudioSafetyComponent::~AudioSafetyComponent()
{
    stopTimer();
}

void AudioSafetyComponent::updateUIFromDSP()
{
    isUpdatingFromDSP = true;
    auto setSliderFromModel = [](juce::Slider& slider, double value)
    {
        if (!slider.isMouseButtonDown())
            slider.setValue(value, juce::dontSendNotification);
    };
    streamLimiterToggle.setToggleState(safety.isStreamLimiterEnabled(), juce::dontSendNotification);
    setSliderFromModel(streamCeilingSlider, safety.getStreamCeilingDb());
    setSliderFromModel(streamReleaseSlider, safety.getStreamReleaseMs());

    monitorLimiterToggle.setToggleState(safety.isMonitorLimiterEnabled(), juce::dontSendNotification);
    setSliderFromModel(monitorCeilingSlider, safety.getMonitorCeilingDb());
    setSliderFromModel(monitorReleaseSlider, safety.getMonitorReleaseMs());
    isUpdatingFromDSP = false;
}

void AudioSafetyComponent::buttonClicked(juce::Button* b)
{
    if (isUpdatingFromDSP) return;

    if (b == &streamLimiterToggle)
        safety.setStreamLimiterEnabled(streamLimiterToggle.getToggleState());
    else if (b == &monitorLimiterToggle)
        safety.setMonitorLimiterEnabled(monitorLimiterToggle.getToggleState());
    else if (b == &resetPeakBtn)
    {
        safety.resetClipWarnings();
        clipStatusBadge.setText("CLIPPING: CLEARED", juce::dontSendNotification);
        clipStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    }
    else if (b == &emergencyMuteAllBtn)
    {
        safety.triggerEmergencyMuteAll();
    }
    else if (b == &emergencyMuteStreamBtn)
    {
        safety.setEmergencyMute(EmergencyMuteMode::StreamOnly);
    }
    else if (b == &emergencyMuteMonitorBtn)
    {
        safety.setEmergencyMute(EmergencyMuteMode::MonitorOnly);
    }
    else if (b == &unmuteAllBtn)
    {
        safety.unmuteAll();
    }
}

void AudioSafetyComponent::sliderValueChanged(juce::Slider* s)
{
    if (isUpdatingFromDSP) return;

    if (s == &streamCeilingSlider)
        safety.setStreamCeilingDb((float)streamCeilingSlider.getValue());
    else if (s == &streamReleaseSlider)
        safety.setStreamReleaseMs((float)streamReleaseSlider.getValue());
    else if (s == &monitorCeilingSlider)
        safety.setMonitorCeilingDb((float)monitorCeilingSlider.getValue());
    else if (s == &monitorReleaseSlider)
        safety.setMonitorReleaseMs((float)monitorReleaseSlider.getValue());
}

void AudioSafetyComponent::timerCallback()
{
    isFlashing = !isFlashing;
    currentStreamPeakDb = safety.getStreamPeakDb();
    currentMonitorPeakDb = safety.getMonitorPeakDb();
    currentStreamGrDb = safety.getStreamGainReductionDb();
    currentMonitorGrDb = safety.getMonitorGainReductionDb();

    // Emergency Mute state
    auto emMode = safety.getEmergencyMuteMode();
    if (emMode != EmergencyMuteMode::None)
    {
        juce::String txt = "EMERGENCY MUTE ACTIVE: ";
        if (emMode == EmergencyMuteMode::Both) txt += "ALL OUTPUTS MUTED";
        else if (emMode == EmergencyMuteMode::StreamOnly) txt += "STREAM BUS MUTED";
        else txt += "MONITOR BUS MUTED";

        emergencyStatusLabel.setText(txt, juce::dontSendNotification);
        emergencyStatusLabel.setColour(juce::Label::textColourId, isFlashing ? juce::Colour(0xffff0055) : juce::Colour(0xffffa0b0));
    }
    else
    {
        emergencyStatusLabel.setText("AUDIO OUTPUT: LIVE (UNMUTED)", juce::dontSendNotification);
        emergencyStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    }

    // Clipping state
    bool isClip = safety.isStreamClipping() || safety.isMonitorClipping();
    int sCount = safety.getStreamClipCount();
    int mCount = safety.getMonitorClipCount();
    clipCountLabel.setText("Clips detected: Stream " + juce::String(sCount) + " | Monitor " + juce::String(mCount), juce::dontSendNotification);

    if (isClip)
    {
        clipStatusBadge.setText("⚠️ CLIPPING DETECTED (LATCHED)", juce::dontSendNotification);
        clipStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffff5a5f));
    }
    else
    {
        clipStatusBadge.setText("CLIPPING: NONE (CLEAN)", juce::dontSendNotification);
        clipStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    }

    // Feedback risk state
    auto risk = safety.evaluateRiskLevel();
    switch (risk)
    {
        case SafetyRiskLevel::CriticalFeedbackRisk:
            feedbackStatusBadge.setText("🚨 CRITICAL FEEDBACK RISK!", juce::dontSendNotification);
            feedbackStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffff0055));
            break;
        case SafetyRiskLevel::Caution:
            feedbackStatusBadge.setText("⚠️ CAUTION: LOOPBACK ACTIVE", juce::dontSendNotification);
            feedbackStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffffbe0b));
            break;
        case SafetyRiskLevel::Normal:
        default:
            feedbackStatusBadge.setText("ROUTING: NORMAL (SAFE)", juce::dontSendNotification);
            feedbackStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
            break;
    }
    feedbackDescriptionBox.setText(safety.getRiskDescription());

    repaint();
}

void AudioSafetyComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));

    // Draw Limiter Peak & Gain Reduction Meters in Col 1
    auto c1 = limiterGroup.getBounds().reduced(14, 28);
    // Stream Bus Meter Area
    auto sMeterRect = juce::Rectangle<float>((float)c1.getX(), (float)streamReleaseSlider.getBottom() + 6.0f, (float)c1.getWidth(), 12.0f);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(sMeterRect, 3.0f);

    float sPeakNorm = juce::jlimit(0.0f, 1.0f, (currentStreamPeakDb + 60.0f) / 60.0f);
    if (sPeakNorm > 0.001f)
    {
        g.setColour(currentStreamPeakDb >= -0.5f ? juce::Colour(0xffff5a5f) : juce::Colour(0xff00f5d4));
        g.fillRoundedRectangle(sMeterRect.withWidth(sMeterRect.getWidth() * sPeakNorm), 3.0f);
    }

    // Monitor Bus Meter Area
    auto mMeterRect = juce::Rectangle<float>((float)c1.getX(), (float)monitorReleaseSlider.getBottom() + 6.0f, (float)c1.getWidth(), 12.0f);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(mMeterRect, 3.0f);

    float mPeakNorm = juce::jlimit(0.0f, 1.0f, (currentMonitorPeakDb + 60.0f) / 60.0f);
    if (mPeakNorm > 0.001f)
    {
        g.setColour(currentMonitorPeakDb >= -0.5f ? juce::Colour(0xffff5a5f) : juce::Colour(0xff3b82f6));
        g.fillRoundedRectangle(mMeterRect.withWidth(mMeterRect.getWidth() * mPeakNorm), 3.0f);
    }
}

void AudioSafetyComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Header
    auto headerArea = area.removeFromTop(40);
    headerTitle.setBounds(headerArea.removeFromLeft(480));
    masterStatusPill.setBounds(headerArea.removeFromLeft(220));

    area.removeFromTop(12);

    // 3 Columns Layout
    int colWidth = (area.getWidth() - 24) / 3;
    auto col1 = area.removeFromLeft(colWidth);
    area.removeFromLeft(12);
    auto col2 = area.removeFromLeft(colWidth);
    area.removeFromLeft(12);
    auto col3 = area;

    // Col 1: Limiters
    limiterGroup.setBounds(col1);
    auto c1Inner = col1.reduced(14, 28);
    streamLimiterToggle.setBounds(c1Inner.removeFromTop(24));
    streamCeilingLabel.setBounds(c1Inner.removeFromTop(18));
    streamCeilingSlider.setBounds(c1Inner.removeFromTop(26));
    streamReleaseLabel.setBounds(c1Inner.removeFromTop(18));
    streamReleaseSlider.setBounds(c1Inner.removeFromTop(26));
    c1Inner.removeFromTop(20); // space for painted meter

    monitorLimiterToggle.setBounds(c1Inner.removeFromTop(24));
    monitorCeilingLabel.setBounds(c1Inner.removeFromTop(18));
    monitorCeilingSlider.setBounds(c1Inner.removeFromTop(26));
    monitorReleaseLabel.setBounds(c1Inner.removeFromTop(18));
    monitorReleaseSlider.setBounds(c1Inner.removeFromTop(26));
    c1Inner.removeFromTop(20);

    // Col 2: Clipping & Anti-Feedback
    riskGroup.setBounds(col2);
    auto c2Inner = col2.reduced(14, 28);
    clipStatusBadge.setBounds(c2Inner.removeFromTop(28));
    clipCountLabel.setBounds(c2Inner.removeFromTop(20));
    c2Inner.removeFromTop(6);
    resetPeakBtn.setBounds(c2Inner.removeFromTop(32));
    c2Inner.removeFromTop(18);
    feedbackHeaderLabel.setBounds(c2Inner.removeFromTop(20));
    feedbackStatusBadge.setBounds(c2Inner.removeFromTop(28));
    c2Inner.removeFromTop(6);
    feedbackDescriptionBox.setBounds(c2Inner.removeFromTop(90));

    // Col 3: Emergency Mute
    emergencyGroup.setBounds(col3);
    auto c3Inner = col3.reduced(14, 28);
    emergencyMuteAllBtn.setBounds(c3Inner.removeFromTop(48));
    c3Inner.removeFromTop(10);
    emergencyMuteStreamBtn.setBounds(c3Inner.removeFromTop(32));
    c3Inner.removeFromTop(8);
    emergencyMuteMonitorBtn.setBounds(c3Inner.removeFromTop(32));
    c3Inner.removeFromTop(16);
    unmuteAllBtn.setBounds(c3Inner.removeFromTop(44));
    c3Inner.removeFromTop(16);
    emergencyStatusLabel.setBounds(c3Inner.removeFromTop(28));
}

} // namespace LiveBus
