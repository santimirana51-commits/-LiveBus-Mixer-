#include "OBSMixerComponent.h"

namespace LiveBus
{

// ==============================================================================
// OBS Fader LookAndFeel
// ==============================================================================
OBSFaderLookAndFeel::OBSFaderLookAndFeel()
{
}

void OBSFaderLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float,
                                           const juce::Slider::SliderStyle, juce::Slider&)
{
    float trackWidth = 4.0f;
    float trackX = (float)x + ((float)width - trackWidth) * 0.5f;

    // Track Background (Dark Slate)
    g.setColour(juce::Colour(0xff2d3748));
    g.fillRoundedRectangle(trackX, (float)y, trackWidth, (float)height, 2.0f);

    // Active Track (OBS Vibrant Blue from bottom up to sliderPos)
    float activeH = (float)(y + height) - sliderPos;
    if (activeH > 0.0f)
    {
        g.setColour(juce::Colour(0xff3b82f6)); // OBS Blue
        g.fillRoundedRectangle(trackX, sliderPos, trackWidth, activeH, 2.0f);
    }

    // Fader Cap (OBS White Capsule Pill with subtle shadow)
    float thumbW = 20.0f;
    float thumbH = 34.0f;
    float thumbX = (float)x + ((float)width - thumbW) * 0.5f;
    float thumbY = sliderPos - thumbH * 0.5f;

    // Subtle thumb shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(thumbX - 1.0f, thumbY + 1.0f, thumbW + 2.0f, thumbH + 2.0f, 6.0f);

    // Thumb Body
    g.setColour(juce::Colour(0xfff8fafc));
    g.fillRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 5.0f);

    // Center grip line
    g.setColour(juce::Colour(0xff94a3b8));
    g.fillRect(thumbX + 3.0f, thumbY + thumbH * 0.5f - 1.0f, thumbW - 6.0f, 2.0f);
}

// ==============================================================================
// OBS Channel Strip
// ==============================================================================
OBSChannelStrip::OBSChannelStrip(const juce::String& name, const juce::String& badgeText, juce::Colour badgeCol)
    : channelName(name), badge(badgeText), badgeColour(badgeCol)
{
    nameLabel.setText(name + " ▼", juce::dontSendNotification);
    nameLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    nameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameLabel);

    dbReadoutLabel.setText("0.0 dB", juce::dontSendNotification);
    dbReadoutLabel.setFont(juce::FontOptions(11.0f));
    dbReadoutLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    dbReadoutLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dbReadoutLabel);

    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange(-60.0, 6.0, 0.1);
    fader.setValue(0.0);
    fader.onValueChange = [this]() {
        float val = (float)fader.getValue();
        if (val <= -59.5f)
            dbReadoutLabel.setText("-inf dB", juce::dontSendNotification);
        else
            dbReadoutLabel.setText(juce::String(val, 1) + " dB", juce::dontSendNotification);

        float linearGain = (val <= -59.5f) ? 0.0f : juce::Decibels::decibelsToGain(val);
        if (onGainChanged) onGainChanged(linearGain);
    };
    addAndMakeVisible(fader);

    muteBtn.setButtonText("MUTE");
    muteBtn.setClickingTogglesState(true);
    muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    muteBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444)); // Red when muted
    muteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff1f5f9));
    muteBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    muteBtn.onClick = [this]() {
        bool isMuted = muteBtn.getToggleState();
        if (onMuteToggled) onMuteToggled(isMuted);
    };
    addAndMakeVisible(muteBtn);

    monitorBtn.setButtonText("MON");
    monitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    monitorBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(monitorBtn);

    gearBtn.setButtonText("OPT");
    gearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    gearBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(gearBtn);
}

void OBSChannelStrip::setLevelMeter(float peakL, float peakR, float, float)
{
    currentPeakL = peakL;
    currentPeakR = peakR;

    if (peakL > peakHoldL)
    {
        peakHoldL = peakL;
        peakHoldFrames = 30; // ~1 second hold
    }
    else if (--peakHoldFrames <= 0)
    {
        peakHoldL *= 0.92f;
    }

    if (peakR > peakHoldR)
        peakHoldR = peakR;
    else
        peakHoldR *= 0.92f;

    repaint();
}

void OBSChannelStrip::setGainDb(float gainDb)
{
    fader.setValue(gainDb, juce::dontSendNotification);
    if (gainDb <= -59.5f)
        dbReadoutLabel.setText("-inf dB", juce::dontSendNotification);
    else
        dbReadoutLabel.setText(juce::String(gainDb, 1) + " dB", juce::dontSendNotification);
}

float OBSChannelStrip::getGainDb() const
{
    return (float)fader.getValue();
}

void OBSChannelStrip::drawOBSMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float peakVal, float peakHoldVal)
{
    // Meter track background
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRect(bounds);

    float peakDb = (peakVal > 0.00001f) ? juce::Decibels::gainToDecibels(peakVal) : -100.0f;
    float norm = juce::jlimit(0.0f, 1.0f, (peakDb + 60.0f) / 60.0f); // 0.0 at -60dB, 1.0 at 0dB

    int barH = (int)((float)bounds.getHeight() * norm);
    int barY = bounds.getBottom() - barH;

    // Segmented Colors:
    // Green: <= -20dB (lower 66%)
    // Yellow/Amber: -20dB to -9dB (next 18%)
    // Red: > -9dB (top 16%)
    int y0 = bounds.getY();
    int hTotal = bounds.getHeight();
    int yMinus9  = y0 + (int)((float)hTotal * (9.0f / 60.0f));
    int yMinus20 = y0 + (int)((float)hTotal * (20.0f / 60.0f));

    // Green section
    int greenTop = juce::jmax(barY, yMinus20);
    if (bounds.getBottom() > greenTop)
    {
        g.setColour(juce::Colour(0xff22c55e)); // Green
        g.fillRect(bounds.getX(), greenTop, bounds.getWidth(), bounds.getBottom() - greenTop);
    }

    // Yellow section
    if (barY < yMinus20)
    {
        int yellowTop = juce::jmax(barY, yMinus9);
        g.setColour(juce::Colour(0xffeab308)); // Yellow/Amber
        g.fillRect(bounds.getX(), yellowTop, bounds.getWidth(), yMinus20 - yellowTop);
    }

    // Red section
    if (barY < yMinus9)
    {
        g.setColour(juce::Colour(0xffef4444)); // Red
        g.fillRect(bounds.getX(), barY, bounds.getWidth(), yMinus9 - barY);
    }

    // Peak hold line
    float holdDb = (peakHoldVal > 0.00001f) ? juce::Decibels::gainToDecibels(peakHoldVal) : -100.0f;
    float holdNorm = juce::jlimit(0.0f, 1.0f, (holdDb + 60.0f) / 60.0f);
    int holdY = bounds.getBottom() - (int)((float)bounds.getHeight() * holdNorm);
    g.setColour(juce::Colours::white);
    g.drawHorizontalLine(holdY, (float)bounds.getX(), (float)bounds.getRight());
}

void OBSChannelStrip::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();

    // Dark Panel Background
    g.setColour(juce::Colour(0xff181f2a));
    g.fillRoundedRectangle(b.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff2a3447));
    g.drawRoundedRectangle(b.toFloat(), 6.0f, 1.0f);

    // Top Header Badge
    juce::Rectangle<int> badgeRect(b.getX() + 8, b.getY() + 8, b.getWidth() - 16, 20);
    g.setColour(badgeColour.withAlpha(0.25f));
    g.fillRoundedRectangle(badgeRect.toFloat(), 4.0f);
    g.setColour(badgeColour);
    g.drawRoundedRectangle(badgeRect.toFloat(), 4.0f, 1.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(badge, badgeRect, juce::Justification::centred);

    // Fader & Meter Area
    int faderW = 30;
    int meterW = 6;
    int meterGap = 3;
    int faderX = b.getCentreX() - (faderW / 2) - 8;
    int faderY = b.getY() + 74;
    int faderH = b.getHeight() - 130;

    int meterLX = faderX + faderW + meterGap;
    int meterRX = meterLX + meterW + 2;

    // Draw Stereo Meter Bars
    drawOBSMeter(g, {meterLX, faderY, meterW, faderH}, currentPeakL, peakHoldL);
    drawOBSMeter(g, {meterRX, faderY, meterW, faderH}, currentPeakR, peakHoldR);

    // Scale ticks and numbers on right
    int scaleX = meterRX + meterW + 4;
    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::FontOptions(9.0f));

    const int dBTicks[] = { 0, -6, -12, -18, -24, -30, -36, -42, -48, -54, -60 };
    for (int db : dBTicks)
    {
        float norm = (float)(db + 60) / 60.0f;
        int tickY = (faderY + faderH) - (int)((float)faderH * norm);

        g.drawHorizontalLine(tickY, (float)scaleX, (float)(scaleX + 3));
        g.drawText(juce::String(db), scaleX + 5, tickY - 6, 24, 12, juce::Justification::left);
    }
}

void OBSChannelStrip::resized()
{
    auto b = getLocalBounds();
    nameLabel.setBounds(b.getX() + 6, b.getY() + 32, b.getWidth() - 12, 20);
    dbReadoutLabel.setBounds(b.getX() + 6, b.getY() + 50, b.getWidth() - 12, 18);

    int faderW = 34;
    int faderX = b.getCentreX() - (faderW / 2) - 8;
    int faderY = b.getY() + 74;
    int faderH = b.getHeight() - 130;
    fader.setBounds(faderX, faderY, faderW, faderH);

    // Bottom action row: 3 evenly spaced buttons
    int botY = b.getBottom() - 44;
    int btnW = 52;
    int gap = 6;
    int totalBtnW = (btnW * 3) + (gap * 2);
    int startX = b.getCentreX() - (totalBtnW / 2);
    muteBtn.setBounds(startX, botY, btnW, 28);
    monitorBtn.setBounds(startX + btnW + gap, botY, btnW, 28);
    gearBtn.setBounds(startX + (btnW + gap) * 2, botY, btnW, 28);
}

// ==============================================================================
// OBS Mixer Component (Main Dock View)
// ==============================================================================
OBSMixerComponent::OBSMixerComponent(OBSIntegrationService& service,
                                     RoutingMatrix& routing,
                                     LevelMeterProcessor* chMeters,
                                     LevelMeterProcessor* busMeters)
    : obsService(service),
      routingMatrix(routing),
      channelLevelMeters(chMeters),
      busLevelMeters(busMeters)
{
    // Apply custom LookAndFeel to sliders
    obsLookAndFeel.setColour(juce::Slider::thumbColourId, juce::Colours::white);

    // Title & Status
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(titleLabel);

    statusPill.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    statusPill.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    statusPill.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    statusPill.setText("● VIRTUAL CABLE ROUTING: READY", juce::dontSendNotification);
    addAndMakeVisible(statusPill);

    // Stream Output Device Combo
    deviceLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    deviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(deviceLabel);

    streamDeviceCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff12161f));
    streamDeviceCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a3447));
    streamDeviceCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff1f5f9));
    streamDeviceCombo.onChange = [this]() {
        juce::String selectedDev = streamDeviceCombo.getText();
        if (selectedDev.isNotEmpty())
        {
            obsService.openSecondaryVirtualDevice(selectedDev);
            updateUIFromState();
        }
    };
    addAndMakeVisible(streamDeviceCombo);

    // Routing Mode Combo
    routingModeLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    routingModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(routingModeLabel);

    routingModeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff12161f));
    routingModeCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a3447));
    routingModeCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff1f5f9));
    routingModeCombo.addItem("Primary Multi-Channel (Ch 1-2 Stream, Ch 3-4 Monitor)", 1);
    routingModeCombo.addItem("Dedicated Virtual Device (Stream to Virtual Cable, Monitor on Headphones)", 2);
    routingModeCombo.addItem("Primary Stream Only (Entire device to OBS)", 3);
    routingModeCombo.addItem("Combined Audition (Stream + Monitor mix)", 4);
    routingModeCombo.setSelectedId(2, juce::dontSendNotification);
    routingModeCombo.onChange = [this]() {
        int id = routingModeCombo.getSelectedId();
        obsService.setRoutingMode(static_cast<OBSRoutingMode>(id - 1));
        updateUIFromState();
    };
    addAndMakeVisible(routingModeCombo);

    // Audio Specs Info Pill
    audioSpecsPill.setFont(juce::FontOptions(11.0f));
    audioSpecsPill.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
    addAndMakeVisible(audioSpecsPill);

    // Action Buttons
    testSignalBtn.setClickingTogglesState(true);
    testSignalBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    testSignalBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706)); // Amber when test signal active
    testSignalBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    testSignalBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    testSignalBtn.onClick = [this]() {
        bool on = testSignalBtn.getToggleState();
        obsService.setTestSignalEnabled(on);
        testSignalBtn.setButtonText(on ? "Stop Test Tone" : "Send Test Tone to OBS");
    };
    addAndMakeVisible(testSignalBtn);

    openGuideBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    openGuideBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openGuideBtn.onClick = [this]() {
        showGuideOverlay = true;
        if (!guideComponent)
        {
            guideComponent = std::make_unique<OBSGuideComponent>(obsService);
            guideComponent->onClose = [this]() {
                showGuideOverlay = false;
                guideComponent->setVisible(false);
                repaint();
            };
            addAndMakeVisible(guideComponent.get());
        }
        guideComponent->setVisible(true);
        resized();
        repaint();
    };
    addAndMakeVisible(openGuideBtn);

    webSocketBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    webSocketBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    webSocketBtn.onClick = [this]() {
        showWebSocketOverlay = !showWebSocketOverlay;
        resized();
        repaint();
    };
    addAndMakeVisible(webSocketBtn);

    // 5 OBS-style vertical channel strips
    streamStrip = std::make_unique<OBSChannelStrip>("Stream Bus (OBS)", "Global", juce::Colour(0xff2563eb));
    streamStrip->fader.setLookAndFeel(&obsLookAndFeel);
    streamStrip->onGainChanged = [this](float g) {
        if (onBusGainChanged) onBusGainChanged(0, g);
    };
    streamStrip->onMuteToggled = [this](bool m) {
        if (onBusMuteChanged) onBusMuteChanged(0, m);
    };
    addAndMakeVisible(streamStrip.get());

    micStrip = std::make_unique<OBSChannelStrip>("Mic / Aux (Ch 1)", "Active", juce::Colour(0xff0284c7));
    micStrip->fader.setLookAndFeel(&obsLookAndFeel);
    micStrip->onGainChanged = [this](float g) {
        if (onChannelGainChanged) onChannelGainChanged(0, g);
    };
    micStrip->onMuteToggled = [this](bool m) {
        if (onChannelMuteChanged) onChannelMuteChanged(0, m);
    };
    addAndMakeVisible(micStrip.get());

    guitarStrip = std::make_unique<OBSChannelStrip>("Guitar / Inst (Ch 2)", "Active", juce::Colour(0xff0284c7));
    guitarStrip->fader.setLookAndFeel(&obsLookAndFeel);
    guitarStrip->onGainChanged = [this](float g) {
        if (onChannelGainChanged) onChannelGainChanged(1, g);
    };
    guitarStrip->onMuteToggled = [this](bool m) {
        if (onChannelMuteChanged) onChannelMuteChanged(1, m);
    };
    addAndMakeVisible(guitarStrip.get());

    musicStrip = std::make_unique<OBSChannelStrip>("Music L/R (Ch 3-4)", "Active", juce::Colour(0xff0284c7));
    musicStrip->fader.setLookAndFeel(&obsLookAndFeel);
    musicStrip->onGainChanged = [this](float g) {
        if (onChannelGainChanged) {
            onChannelGainChanged(2, g);
            onChannelGainChanged(3, g);
        }
    };
    musicStrip->onMuteToggled = [this](bool m) {
        if (onChannelMuteChanged) {
            onChannelMuteChanged(2, m);
            onChannelMuteChanged(3, m);
        }
    };
    addAndMakeVisible(musicStrip.get());

    monitorStrip = std::make_unique<OBSChannelStrip>("Monitor Bus (Phones)", "Inactive", juce::Colour(0xff64748b));
    monitorStrip->fader.setLookAndFeel(&obsLookAndFeel);
    monitorStrip->onGainChanged = [this](float g) {
        if (onBusGainChanged) onBusGainChanged(1, g);
    };
    monitorStrip->onMuteToggled = [this](bool m) {
        if (onBusMuteChanged) onBusMuteChanged(1, m);
    };
    addAndMakeVisible(monitorStrip.get());

    // WebSocket Dialog Controls
    wsHostEditor.setText("127.0.0.1");
    wsHostEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12161f));
    addChildComponent(wsHostEditor);

    wsPortEditor.setText("4455");
    wsPortEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12161f));
    addChildComponent(wsPortEditor);

    wsPassEditor.setText("");
    wsPassEditor.setPasswordCharacter('*');
    wsPassEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12161f));
    addChildComponent(wsPassEditor);

    wsStatusLabel.setText("OBS WebSocket v5 (Optional Control Connection)", juce::dontSendNotification);
    wsStatusLabel.setFont(juce::FontOptions(12.0f));
    wsStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addChildComponent(wsStatusLabel);

    wsConnectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
    wsConnectBtn.onClick = [this]() {
        webSocketClient.configure(wsHostEditor.getText(), wsPortEditor.getText().getIntValue(), wsPassEditor.getText());
        webSocketClient.connectAsync();
    };
    addChildComponent(wsConnectBtn);

    wsCloseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
    wsCloseBtn.onClick = [this]() {
        showWebSocketOverlay = false;
        resized();
        repaint();
    };
    addChildComponent(wsCloseBtn);

    webSocketClient.onStateChanged = [this](OBSWebSocketState state, const juce::String& msg) {
        wsStatusLabel.setText(msg, juce::dontSendNotification);
        if (state == OBSWebSocketState::Connected)
        {
            webSocketBtn.setButtonText("OBS WebSocket (Connected)");
            webSocketBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff065f46));
        }
        else
        {
            webSocketBtn.setButtonText("OBS WebSocket");
            webSocketBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        }
    };

    obsService.onStatusChanged = [this]() {
        updateUIFromState();
    };

    refreshDevices();
    startTimerHz(30); // 30 Hz UI refresh for meters
}

OBSMixerComponent::~OBSMixerComponent()
{
    stopTimer();
    streamStrip->fader.setLookAndFeel(nullptr);
    micStrip->fader.setLookAndFeel(nullptr);
    guitarStrip->fader.setLookAndFeel(nullptr);
    musicStrip->fader.setLookAndFeel(nullptr);
    monitorStrip->fader.setLookAndFeel(nullptr);
}

void OBSMixerComponent::refreshDevices()
{
    streamDeviceCombo.clear(juce::dontSendNotification);

    auto allDevices = obsService.scanAvailableOutputDevices();
    int id = 1;
    int virtualIdToSelect = 0;

    for (const auto& dev : allDevices)
    {
        juce::String label = dev.name;
        if (dev.isVirtual)
        {
            label += " [" + dev.virtualDriverType + "]";
            if (virtualIdToSelect == 0)
                virtualIdToSelect = id;
        }
        streamDeviceCombo.addItem(label, id++);
    }

    if (virtualIdToSelect != 0)
    {
        streamDeviceCombo.setSelectedId(virtualIdToSelect, juce::sendNotification);
    }
    else if (streamDeviceCombo.getNumItems() > 0)
    {
        streamDeviceCombo.setSelectedId(1, juce::sendNotification);
    }

    updateUIFromState();
}

void OBSMixerComponent::updateUIFromState()
{
    if (obsService.isDeviceError())
    {
        statusPill.setText("[!] " + obsService.getErrorMessage(), juce::dontSendNotification);
        statusPill.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
        statusPill.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
    }
    else if (obsService.isSecondaryDeviceActive())
    {
        statusPill.setText("[STREAMING] " + obsService.getSecondaryDeviceName(), juce::dontSendNotification);
        statusPill.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
        statusPill.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    }
    else
    {
        statusPill.setText("[ROUTING] " + obsService.getStatusDescription(), juce::dontSendNotification);
        statusPill.setColour(juce::Label::backgroundColourId, juce::Colour(0x220284c7));
        statusPill.setColour(juce::Label::textColourId, juce::Colour(0xff0284c7));
    }

    updateAudioSpecsLabel();
}

void OBSMixerComponent::updateAudioSpecsLabel()
{
    double sr = obsService.getSecondarySampleRate();
    int bs = obsService.getSecondaryBufferSize();
    float durMs = OBSIntegrationService::isVirtualAudioDevice(obsService.getSecondaryDeviceName())
                      ? AudioDeviceService::calculateBufferDurationMs(bs, sr)
                      : 0.0f;

    audioSpecsPill.setText("Stream Clock: " + juce::String(sr * 0.001, 1) + " kHz | Buffer: " +
                           juce::String(bs) + " samples (~" + juce::String(durMs, 1) + " ms single buffer)",
                           juce::dontSendNotification);
}

void OBSMixerComponent::timerCallback()
{
    // Feed meters from LiveBus LevelMeterProcessor and OBSIntegrationService
    if (busLevelMeters != nullptr)
    {
        auto sSnap = busLevelMeters[0].getSnapshot();
        streamStrip->setLevelMeter(sSnap.peakL, sSnap.peakR, sSnap.rmsL, sSnap.rmsR);

        auto mSnap = busLevelMeters[1].getSnapshot();
        monitorStrip->setLevelMeter(mSnap.peakL, mSnap.peakR, mSnap.rmsL, mSnap.rmsR);
    }
    else
    {
        streamStrip->setLevelMeter(obsService.getStreamPeakL(), obsService.getStreamPeakR(),
                                   obsService.getStreamRmsL(), obsService.getStreamRmsR());
    }

    if (channelLevelMeters != nullptr)
    {
        auto ch0Snap = channelLevelMeters[0].getSnapshot();
        micStrip->setLevelMeter(ch0Snap.peakL, ch0Snap.peakR, ch0Snap.rmsL, ch0Snap.rmsR);

        auto ch1Snap = channelLevelMeters[1].getSnapshot();
        guitarStrip->setLevelMeter(ch1Snap.peakL, ch1Snap.peakR, ch1Snap.rmsL, ch1Snap.rmsR);

        auto ch2Snap = channelLevelMeters[2].getSnapshot();
        auto ch3Snap = channelLevelMeters[3].getSnapshot();
        musicStrip->setLevelMeter(ch2Snap.peakL, ch3Snap.peakR,
                                 ch2Snap.rmsL, ch3Snap.rmsR);
    }
}

void OBSMixerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12161f));

    // Top Card Panel
    g.setColour(juce::Colour(0xff181f2a));
    g.fillRoundedRectangle(12.0f, 8.0f, (float)getWidth() - 24.0f, 96.0f, 6.0f);
    g.setColour(juce::Colour(0xff2a3447));
    g.drawRoundedRectangle(12.0f, 8.0f, (float)getWidth() - 24.0f, 96.0f, 6.0f, 1.0f);

    // Audio Mixer Header Card
    g.setColour(juce::Colour(0xff161b26));
    g.fillRoundedRectangle(12.0f, 112.0f, (float)getWidth() - 24.0f, (float)getHeight() - 124.0f, 6.0f);
    g.setColour(juce::Colour(0xff222a3a));
    g.drawRoundedRectangle(12.0f, 112.0f, (float)getWidth() - 24.0f, (float)getHeight() - 124.0f, 6.0f, 1.0f);

    // Audio Mixer Title
    g.setColour(juce::Colour(0xfff1f5f9));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("Audio Mixer", 24, 118, 200, 24, juce::Justification::left);

    // WebSocket Overlay Background if active
    if (showWebSocketOverlay)
    {
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRect(getLocalBounds());

        juce::Rectangle<float> modalRect(getWidth() * 0.5f - 240.0f, getHeight() * 0.5f - 140.0f, 480.0f, 280.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRoundedRectangle(modalRect, 8.0f);
        g.setColour(juce::Colour(0xff3b82f6));
        g.drawRoundedRectangle(modalRect, 8.0f, 1.5f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText("OBS WebSocket v5 Connection (Optional)", (int)modalRect.getX() + 20, (int)modalRect.getY() + 16, 400, 24, juce::Justification::left);

        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Host IP:", (int)modalRect.getX() + 20, (int)modalRect.getY() + 50, 100, 20, juce::Justification::left);
        g.drawText("Port:", (int)modalRect.getX() + 260, (int)modalRect.getY() + 50, 60, 20, juce::Justification::left);
        g.drawText("Password (if enabled in OBS):", (int)modalRect.getX() + 20, (int)modalRect.getY() + 110, 250, 20, juce::Justification::left);
    }
}

void OBSMixerComponent::resized()
{
    // 1. Top Controls Layout
    titleLabel.setBounds(20, 14, 220, 22);
    statusPill.setBounds(250, 14, 460, 22);

    int row2Y = 44;
    deviceLabel.setBounds(20, row2Y, 130, 24);
    streamDeviceCombo.setBounds(155, row2Y, 260, 24);

    routingModeLabel.setBounds(430, row2Y, 95, 24);
    routingModeCombo.setBounds(530, row2Y, 320, 24);

    audioSpecsPill.setBounds(20, 74, 520, 20);

    testSignalBtn.setBounds(getWidth() - 470, row2Y, 160, 28);
    openGuideBtn.setBounds(getWidth() - 300, row2Y, 160, 28);
    webSocketBtn.setBounds(getWidth() - 130, row2Y, 110, 28);

    // 2. 5 Vertical OBS Strips Layout
    int stripAreaY = 148;
    int stripAreaH = getHeight() - 164;
    int totalStrips = 5;
    int stripGap = 12;
    int startX = 24;
    int stripW = ((getWidth() - 48) - (stripGap * (totalStrips - 1))) / totalStrips;

    if (streamStrip) streamStrip->setBounds(startX + 0 * (stripW + stripGap), stripAreaY, stripW, stripAreaH);
    if (micStrip) micStrip->setBounds(startX + 1 * (stripW + stripGap), stripAreaY, stripW, stripAreaH);
    if (guitarStrip) guitarStrip->setBounds(startX + 2 * (stripW + stripGap), stripAreaY, stripW, stripAreaH);
    if (musicStrip) musicStrip->setBounds(startX + 3 * (stripW + stripGap), stripAreaY, stripW, stripAreaH);
    if (monitorStrip) monitorStrip->setBounds(startX + 4 * (stripW + stripGap), stripAreaY, stripW, stripAreaH);

    // 3. WebSocket Overlay Positioning
    if (showWebSocketOverlay)
    {
        int modalX = getWidth() / 2 - 240;
        int modalY = getHeight() / 2 - 140;
        wsHostEditor.setBounds(modalX + 20, modalY + 74, 220, 26);
        wsPortEditor.setBounds(modalX + 260, modalY + 74, 180, 26);
        wsPassEditor.setBounds(modalX + 20, modalY + 134, 420, 26);
        wsStatusLabel.setBounds(modalX + 20, modalY + 170, 420, 24);
        wsConnectBtn.setBounds(modalX + 20, modalY + 214, 200, 32);
        wsCloseBtn.setBounds(modalX + 240, modalY + 214, 200, 32);

        wsHostEditor.setVisible(true);
        wsPortEditor.setVisible(true);
        wsPassEditor.setVisible(true);
        wsStatusLabel.setVisible(true);
        wsConnectBtn.setVisible(true);
        wsCloseBtn.setVisible(true);
    }
    else
    {
        wsHostEditor.setVisible(false);
        wsPortEditor.setVisible(false);
        wsPassEditor.setVisible(false);
        wsStatusLabel.setVisible(false);
        wsConnectBtn.setVisible(false);
        wsCloseBtn.setVisible(false);
    }

    // 4. Guide Overlay Positioning
    if (showGuideOverlay && guideComponent)
    {
        guideComponent->setBounds(getLocalBounds().reduced(40));
    }
}

} // namespace LiveBus
