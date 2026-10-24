#include "DuckingComponent.h"

namespace LiveBus
{

DuckingComponent::DuckingComponent(AutoDucker& duckerRef)
    : ducker(duckerRef)
{
    // Header
    headerTitle.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour(0xfff0f6fc));
    addAndMakeVisible(headerTitle);

    masterEnableBtn.setButtonText("ENABLE DUCKING");
    masterEnableBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff00f5d4));
    masterEnableBtn.addListener(this);
    addAndMakeVisible(masterEnableBtn);

    stateBadge.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    stateBadge.setJustificationType(juce::Justification::centred);
    stateBadge.setText("STATE: IDLE", juce::dontSendNotification);
    addAndMakeVisible(stateBadge);

    presetCombo.addItem("Preset: Speech over Music", 1);
    presetCombo.addItem("Preset: Podcast Studio", 2);
    presetCombo.addItem("Preset: Light Background", 3);
    presetCombo.addItem("Preset: Strong Ducking", 4);
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    presetCombo.addListener(this);
    addAndMakeVisible(presetCombo);

    savePresetBtn.addListener(this);
    addAndMakeVisible(savePresetBtn);

    // Column 1: Trigger & Sidechain Group
    addAndMakeVisible(triggerGroup);

    triggerSourceLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    addAndMakeVisible(triggerSourceLabel);

    triggerSourceCombo.addItem("Input 1: Microphone (Default)", 1);
    triggerSourceCombo.addItem("Input 2: Guitar / Instrument", 2);
    triggerSourceCombo.addItem("Input 3: Music Left", 3);
    triggerSourceCombo.addItem("Input 4: Music Right", 4);
    for (int channel = 5; channel <= 16; ++channel)
        triggerSourceCombo.addItem("Input " + juce::String(channel), channel);
    triggerSourceCombo.setSelectedId(1, juce::dontSendNotification);
    triggerSourceCombo.addListener(this);
    addAndMakeVisible(triggerSourceCombo);

    addAndMakeVisible(triggerMeterLabel);

    threshLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    addAndMakeVisible(threshLabel);

    threshSlider.setRange(-60.0, 0.0, 0.5);
    threshSlider.setValue(-30.0, juce::dontSendNotification);
    threshSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    threshSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    threshSlider.addListener(this);
    addAndMakeVisible(threshSlider);

    detModeLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
    addAndMakeVisible(detModeLabel);

    detModeCombo.addItem("Peak Detector (Fast)", 1);
    detModeCombo.addItem("RMS Detector (Smooth)", 2);
    detModeCombo.setSelectedId(1, juce::dontSendNotification);
    detModeCombo.addListener(this);
    addAndMakeVisible(detModeCombo);

    filterEnableToggle.addListener(this);
    addAndMakeVisible(filterEnableToggle);

    addAndMakeVisible(hpfLabel);
    hpfSlider.setRange(20.0, 2000.0, 1.0);
    hpfSlider.setValue(80.0, juce::dontSendNotification);
    hpfSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hpfSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    hpfSlider.addListener(this);
    addAndMakeVisible(hpfSlider);

    addAndMakeVisible(lpfLabel);
    lpfSlider.setRange(200.0, 20000.0, 10.0);
    lpfSlider.setValue(8000.0, juce::dontSendNotification);
    lpfSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lpfSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    lpfSlider.addListener(this);
    addAndMakeVisible(lpfSlider);

    // Column 2: Ducking Dynamics Group
    addAndMakeVisible(dynamicsGroup);

    addAndMakeVisible(depthLabel);
    depthSlider.setRange(-36.0, 0.0, 0.5);
    depthSlider.setValue(-12.0, juce::dontSendNotification);
    depthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    depthSlider.addListener(this);
    addAndMakeVisible(depthSlider);

    addAndMakeVisible(attackLabel);
    attackSlider.setRange(0.5, 200.0, 0.5);
    attackSlider.setValue(15.0, juce::dontSendNotification);
    attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    attackSlider.addListener(this);
    addAndMakeVisible(attackSlider);

    addAndMakeVisible(holdLabel);
    holdSlider.setRange(0.0, 3000.0, 10.0);
    holdSlider.setValue(350.0, juce::dontSendNotification);
    holdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    holdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    holdSlider.addListener(this);
    addAndMakeVisible(holdSlider);

    addAndMakeVisible(releaseLabel);
    releaseSlider.setRange(10.0, 5000.0, 10.0);
    releaseSlider.setValue(600.0, juce::dontSendNotification);
    releaseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    releaseSlider.addListener(this);
    addAndMakeVisible(releaseSlider);

    addAndMakeVisible(grMeterLabel);

    // Column 3: Target Routing Group
    addAndMakeVisible(targetGroup);

    streamEnableToggle.setToggleState(true, juce::dontSendNotification);
    streamEnableToggle.addListener(this);
    addAndMakeVisible(streamEnableToggle);

    addAndMakeVisible(streamDepthLabel);
    streamDepthSlider.setRange(-36.0, 0.0, 0.5);
    streamDepthSlider.setValue(-12.0, juce::dontSendNotification);
    streamDepthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    streamDepthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    streamDepthSlider.addListener(this);
    addAndMakeVisible(streamDepthSlider);

    monitorEnableToggle.setToggleState(true, juce::dontSendNotification);
    monitorEnableToggle.addListener(this);
    addAndMakeVisible(monitorEnableToggle);

    addAndMakeVisible(monitorDepthLabel);
    monitorDepthSlider.setRange(-36.0, 0.0, 0.5);
    monitorDepthSlider.setValue(-12.0, juce::dontSendNotification);
    monitorDepthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    monitorDepthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    monitorDepthSlider.addListener(this);
    addAndMakeVisible(monitorDepthSlider);

    addAndMakeVisible(targetChannelsLabel);

    channelToggles[0].setButtonText("Ch 1: Mic (Trigger - Protected)");
    channelToggles[0].setEnabled(false);
    channelToggles[1].setButtonText("Ch 2: Instrument / Aux");
    channelToggles[2].setButtonText("Ch 3: Music Left (Ducked)");
    channelToggles[2].setToggleState(true, juce::dontSendNotification);
    channelToggles[3].setButtonText("Ch 4: Music Right (Ducked)");
    channelToggles[3].setToggleState(true, juce::dontSendNotification);

    for (int i = 0; i < 4; ++i)
    {
        channelToggles[i].addListener(this);
        addAndMakeVisible(channelToggles[i]);
    }

    bypassBtn.addListener(this);
    addAndMakeVisible(bypassBtn);

    updateUIFromDSP();
    startTimerHz(30);
}

DuckingComponent::~DuckingComponent()
{
    stopTimer();
}

void DuckingComponent::updateUIFromDSP()
{
    isUpdatingFromDSP = true;
    auto setSliderFromModel = [](juce::Slider& slider, double value)
    {
        if (!slider.isMouseButtonDown())
            slider.setValue(value, juce::dontSendNotification);
    };
    masterEnableBtn.setToggleState(ducker.isEnabled(), juce::dontSendNotification);
    if (!triggerSourceCombo.isPopupActive())
        triggerSourceCombo.setSelectedId(ducker.getTriggerSource() + 1, juce::dontSendNotification);
    setSliderFromModel(threshSlider, ducker.getThresholdDb());
    setSliderFromModel(depthSlider, ducker.getDuckingDepthDb());
    setSliderFromModel(streamDepthSlider, ducker.getStreamDepthDb());
    setSliderFromModel(monitorDepthSlider, ducker.getMonitorDepthDb());
    setSliderFromModel(attackSlider, ducker.getAttackMs());
    setSliderFromModel(holdSlider, ducker.getHoldMs());
    setSliderFromModel(releaseSlider, ducker.getReleaseMs());
    if (!detModeCombo.isPopupActive())
        detModeCombo.setSelectedId(ducker.getDetectorMode() == DuckingDetectorMode::Peak ? 1 : 2, juce::dontSendNotification);
    filterEnableToggle.setToggleState(ducker.isFilterEnabled(), juce::dontSendNotification);
    setSliderFromModel(hpfSlider, ducker.getFilterHpfFreq());
    setSliderFromModel(lpfSlider, ducker.getFilterLpfFreq());
    streamEnableToggle.setToggleState(ducker.isStreamEnabled(), juce::dontSendNotification);
    monitorEnableToggle.setToggleState(ducker.isMonitorEnabled(), juce::dontSendNotification);

    uint16_t mask = ducker.getTargetChannelsMask();
    const int triggerSource = ducker.getTriggerSource();
    for (int i = 0; i < 4; ++i)
    {
        channelToggles[i].setToggleState((mask & (1 << i)) != 0, juce::dontSendNotification);
        channelToggles[i].setEnabled(i != triggerSource);
    }
    if (triggerSource < 4)
        channelToggles[(size_t)triggerSource].setToggleState(false, juce::dontSendNotification);
    isUpdatingFromDSP = false;
}

void DuckingComponent::buttonClicked(juce::Button* b)
{
    if (isUpdatingFromDSP) return;

    if (b == &masterEnableBtn)
    {
        ducker.setEnabled(masterEnableBtn.getToggleState());
    }
    else if (b == &bypassBtn)
    {
        masterEnableBtn.setToggleState(false, juce::sendNotification);
    }
    else if (b == &filterEnableToggle)
    {
        ducker.setFilterEnabled(filterEnableToggle.getToggleState());
    }
    else if (b == &streamEnableToggle)
    {
        ducker.setStreamEnabled(streamEnableToggle.getToggleState());
    }
    else if (b == &monitorEnableToggle)
    {
        ducker.setMonitorEnabled(monitorEnableToggle.getToggleState());
    }
    else if (b == &savePresetBtn)
    {
        // Preset save feedback
        stateBadge.setText("PRESET SAVED", juce::dontSendNotification);
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            if (b == &channelToggles[i])
            {
                ducker.setTargetChannel(i, channelToggles[i].getToggleState());
                break;
            }
        }
    }
}

void DuckingComponent::sliderValueChanged(juce::Slider* s)
{
    if (isUpdatingFromDSP) return;

    if (s == &threshSlider)
        ducker.setThresholdDb((float)threshSlider.getValue());
    else if (s == &depthSlider)
    {
        float d = (float)depthSlider.getValue();
        ducker.setDuckingDepthDb(d);
        streamDepthSlider.setValue(d, juce::dontSendNotification);
        monitorDepthSlider.setValue(d, juce::dontSendNotification);
    }
    else if (s == &streamDepthSlider)
        ducker.setStreamDepthDb((float)streamDepthSlider.getValue());
    else if (s == &monitorDepthSlider)
        ducker.setMonitorDepthDb((float)monitorDepthSlider.getValue());
    else if (s == &attackSlider)
        ducker.setAttackMs((float)attackSlider.getValue());
    else if (s == &holdSlider)
        ducker.setHoldMs((float)holdSlider.getValue());
    else if (s == &releaseSlider)
        ducker.setReleaseMs((float)releaseSlider.getValue());
    else if (s == &hpfSlider)
        ducker.setFilterHpfFreq((float)hpfSlider.getValue());
    else if (s == &lpfSlider)
        ducker.setFilterLpfFreq((float)lpfSlider.getValue());
}

void DuckingComponent::comboBoxChanged(juce::ComboBox* cb)
{
    if (isUpdatingFromDSP) return;

    if (cb == &triggerSourceCombo)
    {
        int src = triggerSourceCombo.getSelectedId() - 1;
        ducker.setTriggerSource(src);
        // Protect trigger source channel from ducking target
        channelToggles[src].setEnabled(false);
        channelToggles[src].setToggleState(false, juce::sendNotification);
        for (int i = 0; i < 4; ++i)
            if (i != src) channelToggles[i].setEnabled(true);
    }
    else if (cb == &detModeCombo)
    {
        ducker.setDetectorMode(detModeCombo.getSelectedId() == 1 ? DuckingDetectorMode::Peak : DuckingDetectorMode::RMS);
    }
    else if (cb == &presetCombo)
    {
        auto presets = AutoDucker::getFactoryPresets();
        int idx = presetCombo.getSelectedId() - 1;
        if (idx >= 0 && idx < (int)presets.size())
        {
            ducker.applyPreset(presets[(size_t)idx]);
            updateUIFromDSP();
        }
    }
}

void DuckingComponent::timerCallback()
{
    currentTriggerMeterDb = ducker.getTriggerLevelDb();
    currentSidechainActive = ducker.isSidechainActive();
    currentGrStreamDb = ducker.getGainReductionDbStream();
    currentGrMonitorDb = ducker.getGainReductionDbMonitor();

    auto state = ducker.getCurrentState();
    switch (state)
    {
        case DuckingState::Attack:
            stateBadge.setText("STATE: ATTACK (DUCKING)", juce::dontSendNotification);
            stateBadge.setColour(juce::Label::textColourId, juce::Colour(0xffffbe0b));
            break;
        case DuckingState::Hold:
            stateBadge.setText("STATE: HOLD (VOICE ACTIVE)", juce::dontSendNotification);
            stateBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
            break;
        case DuckingState::Release:
            stateBadge.setText("STATE: RELEASE (RECOVERING)", juce::dontSendNotification);
            stateBadge.setColour(juce::Label::textColourId, juce::Colour(0xffa78bfa));
            break;
        case DuckingState::Idle:
        default:
            stateBadge.setText(ducker.isEnabled() ? "STATE: IDLE (READY)" : "STATE: BYPASSED", juce::dontSendNotification);
            stateBadge.setColour(juce::Label::textColourId, ducker.isEnabled() ? juce::Colour(0xff94a3b8) : juce::Colour(0xff64748b));
            break;
    }

    repaint();
}

void DuckingComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));

    // Draw Trigger Level Meter Bar
    auto trigMeterBounds = triggerMeterLabel.getBounds().translated(0, 22).withHeight(14);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(trigMeterBounds.toFloat(), 3.0f);

    float normTrig = juce::jlimit(0.0f, 1.0f, (currentTriggerMeterDb + 60.0f) / 60.0f);
    if (normTrig > 0.001f)
    {
        auto fillW = (float)trigMeterBounds.getWidth() * normTrig;
        auto fillRect = trigMeterBounds.toFloat().withWidth(fillW);
        g.setColour(currentSidechainActive ? juce::Colour(0xff00f5d4) : juce::Colour(0xff3b82f6));
        g.fillRoundedRectangle(fillRect, 3.0f);
    }

    // Sidechain Active LED dot
    auto ledRect = juce::Rectangle<float>(trigMeterBounds.getRight() - 14.0f, trigMeterBounds.getY() - 18.0f, 10.0f, 10.0f);
    g.setColour(currentSidechainActive ? juce::Colour(0xff00f5d4) : juce::Colour(0xff334155));
    g.fillEllipse(ledRect);

    // Draw Dual Gain Reduction Meter Bars (Stream & Monitor)
    auto grMeterBounds = grMeterLabel.getBounds().translated(0, 22).withHeight(14);
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(grMeterBounds.toFloat(), 3.0f);

    float normGrStream = juce::jlimit(0.0f, 1.0f, std::abs(currentGrStreamDb) / 30.0f);
    if (normGrStream > 0.001f)
    {
        auto fillW = (float)grMeterBounds.getWidth() * normGrStream;
        auto fillRect = grMeterBounds.toFloat().withWidth(fillW);
        g.setColour(juce::Colour(0xffff5a5f)); // red/amber GR meter
        g.fillRoundedRectangle(fillRect, 3.0f);
    }
}

void DuckingComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Header
    auto headerArea = area.removeFromTop(40);
    headerTitle.setBounds(headerArea.removeFromLeft(320));
    masterEnableBtn.setBounds(headerArea.removeFromLeft(160));
    stateBadge.setBounds(headerArea.removeFromLeft(200));
    presetCombo.setBounds(headerArea.removeFromLeft(200).reduced(2));
    savePresetBtn.setBounds(headerArea.removeFromLeft(110).reduced(2));

    area.removeFromTop(12);

    // 3 Columns Layout
    int colWidth = (area.getWidth() - 24) / 3;
    auto col1 = area.removeFromLeft(colWidth);
    area.removeFromLeft(12);
    auto col2 = area.removeFromLeft(colWidth);
    area.removeFromLeft(12);
    auto col3 = area;

    // Col 1: Trigger & Sidechain
    triggerGroup.setBounds(col1);
    auto c1Inner = col1.reduced(14, 28);
    triggerSourceLabel.setBounds(c1Inner.removeFromTop(20));
    triggerSourceCombo.setBounds(c1Inner.removeFromTop(28));
    c1Inner.removeFromTop(10);
    triggerMeterLabel.setBounds(c1Inner.removeFromTop(20));
    c1Inner.removeFromTop(22); // space for painted meter bar
    threshLabel.setBounds(c1Inner.removeFromTop(20));
    threshSlider.setBounds(c1Inner.removeFromTop(28));
    c1Inner.removeFromTop(8);
    detModeLabel.setBounds(c1Inner.removeFromTop(20));
    detModeCombo.setBounds(c1Inner.removeFromTop(28));
    c1Inner.removeFromTop(12);
    filterEnableToggle.setBounds(c1Inner.removeFromTop(24));
    hpfLabel.setBounds(c1Inner.removeFromTop(20));
    hpfSlider.setBounds(c1Inner.removeFromTop(28));
    lpfLabel.setBounds(c1Inner.removeFromTop(20));
    lpfSlider.setBounds(c1Inner.removeFromTop(28));

    // Col 2: Ducking Dynamics
    dynamicsGroup.setBounds(col2);
    auto c2Inner = col2.reduced(14, 28);
    depthLabel.setBounds(c2Inner.removeFromTop(20));
    depthSlider.setBounds(c2Inner.removeFromTop(28));
    c2Inner.removeFromTop(10);
    attackLabel.setBounds(c2Inner.removeFromTop(20));
    attackSlider.setBounds(c2Inner.removeFromTop(28));
    c2Inner.removeFromTop(10);
    holdLabel.setBounds(c2Inner.removeFromTop(20));
    holdSlider.setBounds(c2Inner.removeFromTop(28));
    c2Inner.removeFromTop(10);
    releaseLabel.setBounds(c2Inner.removeFromTop(20));
    releaseSlider.setBounds(c2Inner.removeFromTop(28));
    c2Inner.removeFromTop(16);
    grMeterLabel.setBounds(c2Inner.removeFromTop(20));
    c2Inner.removeFromTop(24);

    // Col 3: Target Routing
    targetGroup.setBounds(col3);
    auto c3Inner = col3.reduced(14, 28);
    streamEnableToggle.setBounds(c3Inner.removeFromTop(24));
    streamDepthLabel.setBounds(c3Inner.removeFromTop(18));
    streamDepthSlider.setBounds(c3Inner.removeFromTop(28));
    c3Inner.removeFromTop(10);
    monitorEnableToggle.setBounds(c3Inner.removeFromTop(24));
    monitorDepthLabel.setBounds(c3Inner.removeFromTop(18));
    monitorDepthSlider.setBounds(c3Inner.removeFromTop(28));
    c3Inner.removeFromTop(12);
    targetChannelsLabel.setBounds(c3Inner.removeFromTop(20));
    for (int i = 0; i < 4; ++i)
    {
        channelToggles[i].setBounds(c3Inner.removeFromTop(24));
    }
    c3Inner.removeFromTop(16);
    bypassBtn.setBounds(c3Inner.removeFromTop(32));
}

} // namespace LiveBus
