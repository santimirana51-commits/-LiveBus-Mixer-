#include "AudioSettingsComponent.h"

namespace LiveBus
{

// =============================================================================
// SegmentedInputMeterComponent
// =============================================================================
SegmentedInputMeterComponent::SegmentedInputMeterComponent()
{
}

void SegmentedInputMeterComponent::setLevelDb(float peakDb, float rmsDb)
{
    currentPeakDb = peakDb;
    currentRmsDb = rmsDb;
    repaint();
}

void SegmentedInputMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Dark recessed background
    g.setColour(juce::Colour(0xff141820));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2d3648));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    auto meterArea = bounds.reduced(3.0f);
    const float segGap = 2.0f;
    const float totalGaps = (numSegments - 1) * segGap;
    const float segWidth = (meterArea.getWidth() - totalGaps) / (float)numSegments;

    // Thresholds for 8 segments: -42, -30, -24, -18, -12, -6, -3, 0 dBFS
    const float thresholds[numSegments] = { -42.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, -3.0f, 0.0f };

    for (int i = 0; i < numSegments; ++i)
    {
        const float x = meterArea.getX() + i * (segWidth + segGap);
        auto segRect = juce::Rectangle<float>(x, meterArea.getY(), segWidth, meterArea.getHeight());

        const bool isLit = (currentPeakDb >= thresholds[i]);

        juce::Colour litColour;
        juce::Colour unlitColour;

        if (i < 4) // Green / Cyan (-42 .. -18 dB)
        {
            litColour = juce::Colour(0xff00f5d4);
            unlitColour = juce::Colour(0x2200f5d4);
        }
        else if (i < 6) // Yellow / Amber (-12 .. -6 dB)
        {
            litColour = juce::Colour(0xfff59e0b);
            unlitColour = juce::Colour(0x22f59e0b);
        }
        else // Orange to Red (-3 .. 0 dB CLIP)
        {
            litColour = juce::Colour(0xffef4444);
            unlitColour = juce::Colour(0x22ef4444);
        }

        if (isLit)
        {
            g.setColour(litColour);
            g.fillRoundedRectangle(segRect, 1.5f);
        }
        else
        {
            g.setColour(unlitColour);
            g.fillRoundedRectangle(segRect, 1.5f);
        }
    }
}

// =============================================================================
// ChannelListBoxComponent
// =============================================================================
ChannelListBoxComponent::ChannelListBoxComponent(bool isOutput, AudioDeviceService& devService)
    : isOutputList(isOutput), service(devService)
{
    viewport.setViewedComponent(&contentContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    refreshChannels();
}

void ChannelListBoxComponent::refreshChannels()
{
    channelCheckboxes.clear();

    const auto names = isOutputList ? service.getAvailableOutputChannelNames()
                                    : service.getAvailableInputChannelNames();
    const auto activeBits = isOutputList ? service.getActiveOutputChannels()
                                         : service.getActiveInputChannels();

    const int rowHeight = 26;
    int y = 4;

    for (int i = 0; i < names.size(); ++i)
    {
        auto* toggle = channelCheckboxes.add(new juce::ToggleButton(names[i]));
        toggle->setToggleState(activeBits[i], juce::dontSendNotification);
        toggle->setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff0f6fc));
        toggle->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));

        const int chIndex = i;
        toggle->onClick = [this, chIndex, toggle]() {
            if (isOutputList)
                service.setOutputChannelActive(chIndex, toggle->getToggleState());
            else
                service.setInputChannelActive(chIndex, toggle->getToggleState());
        };

        contentContainer.addAndMakeVisible(toggle);
        toggle->setBounds(8, y, 360, rowHeight);
        y += rowHeight + 2;
    }

    contentContainer.setSize(getWidth() > 0 ? getWidth() - 16 : 380, juce::jmax(y + 4, getHeight()));
    resized();
}

void ChannelListBoxComponent::resized()
{
    viewport.setBounds(getLocalBounds().reduced(2));
    contentContainer.setSize(juce::jmax(getWidth() - 16, 200), contentContainer.getHeight());
}

void ChannelListBoxComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff12161f));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour(0xff2b3444));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
}

// =============================================================================
// AudioSettingsComponent
// =============================================================================
AudioSettingsComponent::AudioSettingsComponent(AudioDeviceService& devService)
    : service(devService)
{
    service.setListener(this);

    // Title & Close Button
    titleLabel.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    closeBtn.onClick = [this]() {
        if (onCloseClicked)
            onCloseClicked();
    };
    addAndMakeVisible(closeBtn);

    // Setup Row Labels
    auto setupLabel = [this](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        lbl.setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);
    };

    setupLabel(deviceTypeLabel, "Audio device type:");
    setupLabel(outputDeviceLabel, "Audio Output Device:");
    setupLabel(inputDeviceLabel, "Audio Input Device:");
    setupLabel(outputChannelsLabel, "Audio Output Channels:");
    setupLabel(inputChannelsLabel, "Audio Input Channels:");
    setupLabel(sampleRateLabel, "Sample Rate:");
    setupLabel(bufferSizeLabel, "Audio Buffer Size:");

    // Setup Combo Boxes
    auto styleCombo = [](juce::ComboBox& cb) {
        cb.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1c212b));
        cb.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333e52));
        cb.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0f6fc));
        cb.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff00f5d4));
    };

    styleCombo(deviceTypeCombo);
    deviceTypeCombo.onChange = [this]() {
        if (isUpdatingUI) return;
        const auto selectedType = deviceTypeCombo.getText();
        service.setBackendType(selectedType);
        updateUIFromDevice();
    };
    addAndMakeVisible(deviceTypeCombo);

    styleCombo(outputDeviceCombo);
    outputDeviceCombo.onChange = [this]() {
        if (isUpdatingUI) return;
        const auto selectedDev = outputDeviceCombo.getText();
        service.setOutputDevice(selectedDev);
        updateUIFromDevice();
    };
    addAndMakeVisible(outputDeviceCombo);

    testToneBtn.setClickingTogglesState(false);
    testToneBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff232b38));
    testToneBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    testToneBtn.onClick = [this]() {
        const bool active = !service.isTestToneEnabled();
        service.setTestToneEnabled(active);
        testToneBtn.setButtonText(active ? "Stop" : "Test");
        testToneBtn.setColour(juce::TextButton::buttonColourId, active ? juce::Colour(0xff10b981) : juce::Colour(0xff232b38));
    };
    addAndMakeVisible(testToneBtn);

    styleCombo(inputDeviceCombo);
    inputDeviceCombo.onChange = [this]() {
        if (isUpdatingUI) return;
        const auto selectedDev = inputDeviceCombo.getText();
        service.setInputDevice(selectedDev);
        updateUIFromDevice();
    };
    addAndMakeVisible(inputDeviceCombo);
    addAndMakeVisible(inputMeter);

    // Channel Boxes
    outputChannelsBox = std::make_unique<ChannelListBoxComponent>(true, service);
    addAndMakeVisible(outputChannelsBox.get());

    inputChannelsBox = std::make_unique<ChannelListBoxComponent>(false, service);
    addAndMakeVisible(inputChannelsBox.get());

    styleCombo(sampleRateCombo);
    sampleRateCombo.onChange = [this]() {
        if (isUpdatingUI) return;
        const double rate = sampleRateCombo.getText().upToFirstOccurrenceOf(" ", false, false).getDoubleValue();
        if (rate > 0.0)
        {
            service.setSampleRate(rate);
            updateUIFromDevice();
        }
    };
    addAndMakeVisible(sampleRateCombo);

    styleCombo(bufferSizeCombo);
    bufferSizeCombo.onChange = [this]() {
        if (isUpdatingUI) return;
        const int size = bufferSizeCombo.getText().upToFirstOccurrenceOf(" ", false, false).getIntValue();
        if (size > 0)
        {
            service.setBufferSize(size);
            updateUIFromDevice();
        }
    };
    addAndMakeVisible(bufferSizeCombo);

    latencyNoteLabel.setText("Note: Buffer duration is single-buffer processing time, not total round-trip latency.", juce::dontSendNotification);
    latencyNoteLabel.setFont(juce::FontOptions(11.0f));
    latencyNoteLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8896aa));
    latencyNoteLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(latencyNoteLabel);

    statusBadgeLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    statusBadgeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusBadgeLabel);

    statusDetailsLabel.setFont(juce::FontOptions(11.0f));
    statusDetailsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    statusDetailsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusDetailsLabel);

    retryBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2b3342));
    retryBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    retryBtn.onClick = [this]() {
        service.manualRetry();
        updateUIFromDevice();
    };
    addAndMakeVisible(retryBtn);

    asioControlBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    asioControlBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00f5d4));
    asioControlBtn.onClick = [this]() {
        service.openControlPanel();
    };
    addAndMakeVisible(asioControlBtn);

    updateUIFromDevice();
    startTimerHz(30);
}

AudioSettingsComponent::~AudioSettingsComponent()
{
    stopTimer();
    service.setListener(nullptr);
}

void AudioSettingsComponent::setCloseButtonVisible(bool visible)
{
    closeBtn.setVisible(visible);
}

void AudioSettingsComponent::updateUIFromDevice()
{
    isUpdatingUI = true;

    // 1. Backend types
    deviceTypeCombo.clear(juce::dontSendNotification);
    const auto backends = service.getAvailableBackendTypes();
    const auto currentBackend = service.getCurrentBackendType();
    int selectedBackendId = 1;
    for (int i = 0; i < backends.size(); ++i)
    {
        deviceTypeCombo.addItem(backends[i], i + 1);
        if (backends[i] == currentBackend)
            selectedBackendId = i + 1;
    }
    deviceTypeCombo.setSelectedId(selectedBackendId, juce::dontSendNotification);

    // 2. Output devices
    outputDeviceCombo.clear(juce::dontSendNotification);
    const auto outDevs = service.getAvailableOutputDevices();
    const auto currentOut = service.getCurrentOutputDevice();
    int selectedOutId = 1;
    for (int i = 0; i < outDevs.size(); ++i)
    {
        outputDeviceCombo.addItem(outDevs[i], i + 1);
        if (outDevs[i] == currentOut)
            selectedOutId = i + 1;
    }
    if (outDevs.isEmpty())
        outputDeviceCombo.addItem("<< No output device >>", 1);
    outputDeviceCombo.setSelectedId(selectedOutId, juce::dontSendNotification);

    // 3. Input devices
    inputDeviceCombo.clear(juce::dontSendNotification);
    const auto inDevs = service.getAvailableInputDevices();
    const auto currentIn = service.getCurrentInputDevice();
    int selectedInId = 1;
    for (int i = 0; i < inDevs.size(); ++i)
    {
        inputDeviceCombo.addItem(inDevs[i], i + 1);
        if (inDevs[i] == currentIn)
            selectedInId = i + 1;
    }
    if (inDevs.isEmpty())
        inputDeviceCombo.addItem("<< No input device >>", 1);
    inputDeviceCombo.setSelectedId(selectedInId, juce::dontSendNotification);

    // 4. Channels
    const auto outputChannelNames = service.getAvailableOutputChannelNames();
    const auto inputChannelNames = service.getAvailableInputChannelNames();
    outputChannelsLabel.setText("Output Channels (" + juce::String(outputChannelNames.size()) + ")",
                                juce::dontSendNotification);
    inputChannelsLabel.setText("Input Channels (" + juce::String(inputChannelNames.size()) + ")",
                               juce::dontSendNotification);
    if (outputChannelsBox) outputChannelsBox->refreshChannels();
    if (inputChannelsBox) inputChannelsBox->refreshChannels();

    // 5. Sample Rates
    sampleRateCombo.clear(juce::dontSendNotification);
    const auto sampleRates = service.getAvailableSampleRates();
    const double currentRate = service.getCurrentSampleRate();
    int selectedRateId = 1;
    for (int i = 0; i < sampleRates.size(); ++i)
    {
        const juce::String text = juce::String((int)sampleRates[i]) + " Hz";
        sampleRateCombo.addItem(text, i + 1);
        if (std::abs(sampleRates[i] - currentRate) < 1.0)
            selectedRateId = i + 1;
    }
    sampleRateCombo.setSelectedId(selectedRateId, juce::dontSendNotification);

    // 6. Buffer sizes
    bufferSizeCombo.clear(juce::dontSendNotification);
    const auto bufferSizes = service.getAvailableBufferSizes();
    const int currentSize = service.getCurrentBufferSize();
    int selectedSizeId = 1;
    for (int i = 0; i < bufferSizes.size(); ++i)
    {
        const float ms = AudioDeviceService::calculateBufferDurationMs(bufferSizes[i], currentRate);
        const juce::String text = juce::String(bufferSizes[i]) + " samples (" + juce::String(ms, 1) + " ms)";
        bufferSizeCombo.addItem(text, i + 1);
        if (bufferSizes[i] == currentSize)
            selectedSizeId = i + 1;
    }
    bufferSizeCombo.setSelectedId(selectedSizeId, juce::dontSendNotification);

    // 7. ASIO Control Panel
    asioControlBtn.setEnabled(service.hasControlPanel());
    asioControlBtn.setVisible(service.hasControlPanel());

    // 8. Test Tone Button state
    const bool isTone = service.isTestToneEnabled();
    testToneBtn.setButtonText(isTone ? "Stop" : "Test");
    testToneBtn.setColour(juce::TextButton::buttonColourId, isTone ? juce::Colour(0xff10b981) : juce::Colour(0xff232b38));

    // 9. Status badge & diagnostics
    const auto state = service.getDeviceState();
    switch (state)
    {
        case DeviceState::Active:
            statusBadgeLabel.setText(juce::CharPointer_UTF8("\xe2\x97\x8f ACTIVE"), juce::dontSendNotification);
            statusBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
            statusBadgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
            retryBtn.setEnabled(false);
            break;
        case DeviceState::Reconnecting:
            statusBadgeLabel.setText(juce::CharPointer_UTF8("\xe2\x97\x8f RECONNECTING"), juce::dontSendNotification);
            statusBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
            statusBadgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x22f59e0b));
            retryBtn.setEnabled(true);
            break;
        case DeviceState::Disconnected:
            statusBadgeLabel.setText(juce::CharPointer_UTF8("\xe2\x97\x8f DISCONNECTED"), juce::dontSendNotification);
            statusBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
            statusBadgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
            retryBtn.setEnabled(true);
            break;
        case DeviceState::Failed:
            statusBadgeLabel.setText(juce::CharPointer_UTF8("\xe2\x97\x8f FAILED"), juce::dontSendNotification);
            statusBadgeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdc2626));
            statusBadgeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x22dc2626));
            retryBtn.setEnabled(true);
            break;
    }

    statusDetailsLabel.setText(service.getStatusMessage(), juce::dontSendNotification);

    isUpdatingUI = false;
    repaint();
}

void AudioSettingsComponent::timerCallback()
{
    // Update live input meter
    inputMeter.setLevelDb(service.getInputPeakDb(), service.getInputRmsDb());

    // Keep test tone button sync
    const bool isTone = service.isTestToneEnabled();
    if (isTone != (testToneBtn.getButtonText() == "Stop"))
    {
        testToneBtn.setButtonText(isTone ? "Stop" : "Test");
        testToneBtn.setColour(juce::TextButton::buttonColourId, isTone ? juce::Colour(0xff10b981) : juce::Colour(0xff232b38));
    }

    // Refresh status if supervisor changed
    const auto state = service.getDeviceState();
    juce::String currentBadge = statusBadgeLabel.getText();
    if ((state == DeviceState::Active && !currentBadge.contains("ACTIVE")) ||
        (state == DeviceState::Reconnecting && !currentBadge.contains("RECONNECTING")) ||
        (state == DeviceState::Disconnected && !currentBadge.contains("DISCONNECTED")) ||
        (state == DeviceState::Failed && !currentBadge.contains("FAILED")))
    {
        updateUIFromDevice();
    }
}

void AudioSettingsComponent::onDeviceStateChanged(DeviceState, const juce::String& message)
{
    juce::MessageManager::callAsync([this, message]() {
        statusDetailsLabel.setText(message, juce::dontSendNotification);
        updateUIFromDevice();
    });
}

void AudioSettingsComponent::onDeviceSetupChanged()
{
    juce::MessageManager::callAsync([this]() {
        updateUIFromDevice();
    });
}

void AudioSettingsComponent::paint(juce::Graphics& g)
{
    // Background card
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0d1017));

    // Centered card container styling
    const int cardW = juce::jmin(getWidth() - 32, 620);
    const int cardH = juce::jmin(getHeight() - 32, 740);
    auto cardArea = getLocalBounds().withSizeKeepingCentre(cardW, cardH).toFloat();

    g.setColour(juce::Colour(0xff141720));
    g.fillRoundedRectangle(cardArea, 10.0f);
    g.setColour(juce::Colour(0xff252c3c));
    g.drawRoundedRectangle(cardArea, 10.0f, 1.2f);
}

void AudioSettingsComponent::resized()
{
    const int cardW = juce::jmin(getWidth() - 32, 620);
    const int cardH = juce::jmin(getHeight() - 32, 740);
    auto card = getLocalBounds().withSizeKeepingCentre(cardW, cardH).reduced(18);

    // Header
    auto header = card.removeFromTop(32);
    titleLabel.setBounds(header);
    closeBtn.setBounds(header.getRight() - 28, header.getY(), 26, 26);

    card.removeFromTop(12);

    const int labelW = 180;
    const int rowH = 30;
    const int gap = 12;

    // 1. Audio device type
    auto row1 = card.removeFromTop(rowH);
    deviceTypeLabel.setBounds(row1.removeFromLeft(labelW));
    deviceTypeCombo.setBounds(row1);
    card.removeFromTop(gap);

    // 2. Audio Output Device
    auto row2 = card.removeFromTop(rowH);
    outputDeviceLabel.setBounds(row2.removeFromLeft(labelW));
    testToneBtn.setBounds(row2.removeFromRight(74));
    row2.removeFromRight(8);
    outputDeviceCombo.setBounds(row2);
    card.removeFromTop(gap);

    // 3. Audio Input Device
    auto row3 = card.removeFromTop(rowH);
    inputDeviceLabel.setBounds(row3.removeFromLeft(labelW));
    inputMeter.setBounds(row3.removeFromRight(74));
    row3.removeFromRight(8);
    inputDeviceCombo.setBounds(row3);
    card.removeFromTop(gap);

    // 4. Audio Output Channels
    const int chBoxH = 130;
    auto row4 = card.removeFromTop(chBoxH);
    outputChannelsLabel.setBounds(row4.removeFromLeft(labelW).removeFromTop(rowH));
    if (outputChannelsBox) outputChannelsBox->setBounds(row4);
    card.removeFromTop(gap);

    // 5. Audio Input Channels
    auto row5 = card.removeFromTop(chBoxH);
    inputChannelsLabel.setBounds(row5.removeFromLeft(labelW).removeFromTop(rowH));
    if (inputChannelsBox) inputChannelsBox->setBounds(row5);
    card.removeFromTop(gap);

    // 6. Sample Rate
    auto row6 = card.removeFromTop(rowH);
    sampleRateLabel.setBounds(row6.removeFromLeft(labelW));
    sampleRateCombo.setBounds(row6);
    card.removeFromTop(gap);

    // 7. Audio Buffer Size
    auto row7 = card.removeFromTop(rowH);
    bufferSizeLabel.setBounds(row7.removeFromLeft(labelW));
    bufferSizeCombo.setBounds(row7);
    card.removeFromTop(6);

    // Latency note
    latencyNoteLabel.setBounds(card.removeFromTop(20).withTrimmedLeft(labelW));
    card.removeFromTop(10);

    // Status Bar & Controls
    auto statusRow = card.removeFromTop(28);
    statusBadgeLabel.setBounds(statusRow.removeFromLeft(120));
    statusRow.removeFromLeft(10);
    statusDetailsLabel.setBounds(statusRow);

    card.removeFromTop(8);
    auto btnRow = card.removeFromTop(28);
    if (service.hasControlPanel())
    {
        asioControlBtn.setBounds(btnRow.removeFromLeft(190));
        btnRow.removeFromLeft(10);
    }
    retryBtn.setBounds(btnRow.removeFromLeft(180));
}

// =============================================================================
// AudioSettingsDialog
// =============================================================================
AudioSettingsDialog::AudioSettingsDialog(AudioDeviceService& devService)
    : DialogWindow("Audio Settings", juce::Colour(0xff0d1017), true, false)
{
    auto* settingsComp = new AudioSettingsComponent(devService);
    settingsComp->setCloseButtonVisible(false); // window provides its own close button
    setContentOwned(settingsComp, true);
    centreWithSize(640, 720);
    setVisible(true);
}

void AudioSettingsDialog::closeButtonPressed()
{
    setVisible(false);
}

} // namespace LiveBus
