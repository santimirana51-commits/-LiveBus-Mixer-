#include "MonitorGraphicEQComponent.h"
#include <cmath>

namespace
{

juce::String getEQPresetCategoryLabel(LiveBus::EQPresetCategory category)
{
    switch (category)
    {
        case LiveBus::EQPresetCategory::Vocal: return "Vocal";
        case LiveBus::EQPresetCategory::Streaming: return "Streaming";
        case LiveBus::EQPresetCategory::Music: return "Music";
        case LiveBus::EQPresetCategory::Monitoring: return "Monitoring";
        case LiveBus::EQPresetCategory::TonalCorrection: return "Tonal Correction";
        case LiveBus::EQPresetCategory::Character: return "Sound Character";
    }
    return "EQ";
}

} // namespace

MonitorGraphicEQComponent::MonitorGraphicEQComponent(
    BusDSPProcessor& target, LiveBus::PresetManager& presets)
    : processor(target), presetManager(presets),
      responseDisplay(std::make_unique<ResponseDisplay>(*this))
{
    enabledToggle.addListener(this);
    addAndMakeVisible(enabledToggle);

    analyzerEnabledToggle.addListener(this);
    addAndMakeVisible(analyzerEnabledToggle);
    preAnalyzerToggle.setToggleState(true, juce::dontSendNotification);
    preAnalyzerToggle.addListener(this);
    addAndMakeVisible(preAnalyzerToggle);
    postAnalyzerToggle.setToggleState(true, juce::dontSendNotification);
    postAnalyzerToggle.addListener(this);
    addAndMakeVisible(postAnalyzerToggle);
    analyzerModeCombo.addItem("Mono", 1);
    analyzerModeCombo.addItem("Stereo", 2);
    analyzerModeCombo.setSelectedId(1, juce::dontSendNotification);
    analyzerModeCombo.setTooltip(
        "Mono averages left and right; Stereo shows each channel separately");
    analyzerModeCombo.addListener(this);
    addAndMakeVisible(analyzerModeCombo);

    presetCombo.setTooltip(
        "Apply a factory or user EQ Catalog preset to the Monitor 31-band EQ");
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    presetCombo.addListener(this);
    addAndMakeVisible(presetCombo);
    refreshPresetCatalog();

    for (auto* button : { &slotAButton, &slotBButton, &copyButton, &pasteButton })
    {
        button->addListener(this);
        addAndMakeVisible(*button);
    }
    slotAButton.setTooltip("Select or store configuration A");
    slotBButton.setTooltip("Select or store configuration B");
    copyButton.setTooltip("Copy the current 31-band Monitor EQ settings");
    pasteButton.setTooltip("Paste the copied Monitor EQ settings");
    pasteButton.setEnabled(false);
    resetButton.addListener(this);
    addAndMakeVisible(resetButton);

    for (int band = 0; band < numMonitorGraphicEQBands; ++band)
    {
        auto& slider = bandSliders[(size_t)band];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
        slider.setRange(-12.0, 12.0, 0.1);
        slider.setTextValueSuffix(" dB");
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff38bdf8));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe2e8f0));
        slider.addListener(this);
        addAndMakeVisible(slider);

        auto& label = bandLabels[(size_t)band];
        const float frequency = monitorGraphicEQFrequencies[(size_t)band];
        const juce::String text = frequency >= 1000.0f
            ? juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k"
            : juce::String(frequency, frequency < 100.0f && std::floor(frequency) != frequency ? 1 : 0);
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions(10.0f));
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
        addAndMakeVisible(label);
    }

    addAndMakeVisible(*responseDisplay);
    updateUIFromProcessor();
    abStates[0] = processor.getMonitorGraphicEQState();
    abStates[1] = abStates[0];
    updateABButtonColours();
    startTimerHz(30);
}

MonitorGraphicEQComponent::~MonitorGraphicEQComponent()
{
    stopTimer();
    processor.setMonitorGraphicEQAnalyzerEnabled(false);
}

void MonitorGraphicEQComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff0b0f17));
    graphics.setColour(juce::Colour(0xff1e293b));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f, 1.0f);
    graphics.setColour(juce::Colour(0xffe2e8f0));
    graphics.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    graphics.drawText("Monitor Output — 31-Band Graphic EQ",
                      18, 12, 420, 26, juce::Justification::left);
    graphics.setColour(juce::Colour(0xff94a3b8));
    graphics.setFont(juce::FontOptions(11.0f));
    graphics.drawText("1/3-octave bands · ±12 dB · Monitor Bus only",
                      18, 38, 420, 18, juce::Justification::left);
}

void MonitorGraphicEQComponent::resized()
{
    const int width = getWidth();
    enabledToggle.setBounds(18, 64, 150, 24);
    presetCombo.setBounds(180, 64, 220, 24);
    resetButton.setBounds(405, 64, 55, 24);
    analyzerEnabledToggle.setBounds(465, 64, 82, 24);
    preAnalyzerToggle.setBounds(550, 64, 42, 24);
    postAnalyzerToggle.setBounds(594, 64, 48, 24);
    analyzerModeCombo.setBounds(645, 64, 75, 24);
    slotAButton.setBounds(724, 64, 30, 24);
    slotBButton.setBounds(758, 64, 30, 24);
    copyButton.setBounds(794, 64, 48, 24);
    pasteButton.setBounds(846, 64, 52, 24);

    const int graphX = 18;
    const int graphY = 98;
    const int graphWidth = width - 36;
    responseDisplay->setBounds(graphX, graphY, graphWidth, 190);

    const int banksY = graphY + 205;
    const int bankHeight = 142;
    const int spacing = 12;
    for (int band = 0; band < numMonitorGraphicEQBands; ++band)
    {
        const int row = band / 16;
        const int column = band % 16;
        const int columnsInRow = row == 0 ? 16 : 15;
        const int bankWidth = (width - 36 - spacing) / 2;
        const int columnWidth = bankWidth / columnsInRow;
        const int x = graphX + (row == 1 ? bankWidth + spacing : 0) +
                      column * columnWidth;
        const int y = banksY;
        bandLabels[(size_t)band].setBounds(x, y, columnWidth, 18);
        bandSliders[(size_t)band].setBounds(x + 2, y + 18,
                                             juce::jmax(18, columnWidth - 4),
                                             bankHeight - 18);
    }
}

void MonitorGraphicEQComponent::updateUIFromProcessor()
{
    updatingUI = true;
    refreshPresetCatalog();
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    enabledToggle.setToggleState(
        processor.params.monitorGraphicEQEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    for (int band = 0; band < numMonitorGraphicEQBands; ++band)
        bandSliders[(size_t)band].setValue(
            processor.params.monitorGraphicEQGainDb[(size_t)band].load(
                std::memory_order_relaxed),
            juce::dontSendNotification);
    abStates[(size_t)activeABSlot] = processor.getMonitorGraphicEQState();
    updatingUI = false;
    updateABButtonColours();
    responseDisplay->repaint();
}

void MonitorGraphicEQComponent::sliderValueChanged(juce::Slider* changedSlider)
{
    if (updatingUI)
        return;
    for (int band = 0; band < numMonitorGraphicEQBands; ++band)
    {
        if (changedSlider != &bandSliders[(size_t)band])
            continue;
        processor.params.monitorGraphicEQGainDb[(size_t)band].store(
            (float)changedSlider->getValue(), std::memory_order_relaxed);
        processor.updateMonitorGraphicEQCoefficients();
        storeActiveABSlot();
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        responseDisplay->repaint();
        return;
    }
}

void MonitorGraphicEQComponent::buttonClicked(juce::Button* button)
{
    if (button == &analyzerEnabledToggle)
    {
        processor.setMonitorGraphicEQAnalyzerEnabled(
            analyzerEnabledToggle.getToggleState());
    }
    else if (button == &preAnalyzerToggle || button == &postAnalyzerToggle)
    {
        responseDisplay->repaint();
    }
    else if (button == &enabledToggle)
    {
        processor.params.monitorGraphicEQEnabled.store(
            enabledToggle.getToggleState(), std::memory_order_relaxed);
        storeActiveABSlot();
        responseDisplay->repaint();
    }
    else if (button == &resetButton)
    {
        MonitorGraphicEQ31State flat;
        flat.enabled = processor.params.monitorGraphicEQEnabled.load(
            std::memory_order_relaxed);
        processor.setMonitorGraphicEQState(flat);
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        storeActiveABSlot();
        updateUIFromProcessor();
    }
    else if (button == &slotAButton)
        selectABSlot(0);
    else if (button == &slotBButton)
        selectABSlot(1);
    else if (button == &copyButton)
    {
        copiedState = processor.getMonitorGraphicEQState();
        hasCopiedState = true;
        pasteButton.setEnabled(true);
    }
    else if (button == &pasteButton && hasCopiedState)
    {
        processor.setMonitorGraphicEQState(copiedState);
        storeActiveABSlot();
        updateUIFromProcessor();
    }
}

MonitorGraphicEQComponent::ABState MonitorGraphicEQComponent::getABStates()
{
    storeActiveABSlot();
    return abStates;
}

void MonitorGraphicEQComponent::setABStates(const ABState& states, int activeSlot)
{
    abStates = states;
    activeABSlot = juce::jlimit(0, 1, activeSlot);
    processor.setMonitorGraphicEQState(abStates[(size_t)activeABSlot]);
    updateUIFromProcessor();
    updateABButtonColours();
}

void MonitorGraphicEQComponent::storeActiveABSlot()
{
    abStates[(size_t)activeABSlot] = processor.getMonitorGraphicEQState();
}

void MonitorGraphicEQComponent::selectABSlot(int slot)
{
    storeActiveABSlot();
    activeABSlot = juce::jlimit(0, 1, slot);
    processor.setMonitorGraphicEQState(abStates[(size_t)activeABSlot]);
    updateUIFromProcessor();
    updateABButtonColours();
}

void MonitorGraphicEQComponent::updateABButtonColours()
{
    slotAButton.setColour(juce::TextButton::buttonColourId,
        activeABSlot == 0 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
    slotBButton.setColour(juce::TextButton::buttonColourId,
        activeABSlot == 1 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
}

void MonitorGraphicEQComponent::timerCallback()
{
    if (!analyzerEnabledToggle.getToggleState() || !isShowing())
        return;

    updateAnalyzerSpectrum(true);
    updateAnalyzerSpectrum(false);
    responseDisplay->repaint();
}

void MonitorGraphicEQComponent::updateAnalyzerSpectrum(bool preEQ)
{
    std::array<float, 2048> leftSamples{};
    std::array<float, 2048> rightSamples{};
    const bool received = preEQ
        ? processor.pullMonitorGraphicEQPreAnalyzerStereo(
            leftSamples.data(), rightSamples.data(), (int)leftSamples.size())
        : processor.pullMonitorGraphicEQPostAnalyzerStereo(
            leftSamples.data(), rightSamples.data(), (int)leftSamples.size());
    if (!received)
        return;

    const bool stereo = analyzerModeCombo.getSelectedId() == 2;
    const auto calculateSpectrum = [&](const std::array<float, 2048>& left,
                                       const std::array<float, 2048>& right,
                                       std::array<float, 1024>& spectrum)
    {
        analyzerFFTData.fill(0.0f);
        for (int sample = 0; sample < (int)left.size(); ++sample)
        {
            const float mono = stereo ? left[(size_t)sample]
                                      : 0.5f * (left[(size_t)sample] +
                                                right[(size_t)sample]);
            const float window = 0.5f - 0.5f * std::cos(
                juce::MathConstants<float>::twoPi * (float)sample /
                (float)(left.size() - 1));
            analyzerFFTData[(size_t)sample] = mono * window;
        }
        analyzerFFT.performFrequencyOnlyForwardTransform(analyzerFFTData.data());
        for (size_t bin = 0; bin < spectrum.size(); ++bin)
            spectrum[bin] = juce::Decibels::gainToDecibels(
                analyzerFFTData[bin] / 1024.0f, -100.0f);
    };

    auto& leftSpectrum = preEQ ? preAnalyzerSpectrum : postAnalyzerSpectrum;
    auto& rightSpectrum = preEQ ? preAnalyzerRightSpectrum : postAnalyzerRightSpectrum;
    calculateSpectrum(leftSamples, rightSamples, leftSpectrum);
    if (stereo)
        calculateSpectrum(rightSamples, rightSamples, rightSpectrum);
}

void MonitorGraphicEQComponent::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &analyzerModeCombo)
    {
        responseDisplay->repaint();
        return;
    }
    if (combo != &presetCombo)
        return;

    const int presetIndex = presetCombo.getSelectedId() - 2;
    if (presetIndex < 0 ||
        presetIndex >= (int)catalogPresetItems.size())
        return;

    const auto& item = catalogPresetItems[(size_t)presetIndex];
    LiveBus::EQPresetDefinition definition;
    juce::String error;
    if (!presetManager.loadEQCatalogPreset(item, definition, error))
    {
        juce::AlertWindow::showMessageBox(
            juce::AlertWindow::WarningIcon, "EQ Preset Error",
            "Could not load EQ preset '" + item.name + "': " + error, "OK");
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    auto state = processor.getMonitorGraphicEQState();
    if (!LiveBus::EQPresetCatalog::mapToMonitor31Band(
            definition, state, error))
    {
        juce::AlertWindow::showMessageBox(
            juce::AlertWindow::WarningIcon, "EQ Preset Error",
            "Could not map EQ preset '" + item.name + "': " + error, "OK");
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    if (!juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon, "Approximate EQ Mapping",
            "This 8-band catalog curve will be approximated on the Monitor "
            "Graphic EQ's 31-band frequency grid. Continue?",
            "Apply", "Cancel"))
    {
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    processor.setMonitorGraphicEQState(state);
    storeActiveABSlot();
    updateUIFromProcessor();
}

void MonitorGraphicEQComponent::refreshPresetCatalog()
{
    presetCombo.clear(juce::dontSendNotification);
    presetCombo.addItem("Select EQ preset...", 1);
    catalogPresetItems = presetManager.getPresetsForCategory(
        LiveBus::PresetCategory::EQCatalog);

    int itemId = 2;
    bool hasFactoryHeading = false;
    bool hasUserHeading = false;
    for (const auto& item : catalogPresetItems)
    {
        if (item.isFactory && !hasFactoryHeading)
        {
            presetCombo.addSectionHeading("Factory EQ presets");
            hasFactoryHeading = true;
        }
        else if (!item.isFactory && !hasUserHeading)
        {
            presetCombo.addSectionHeading("Your EQ presets");
            hasUserHeading = true;
        }

        LiveBus::EQPresetDefinition definition;
        juce::String error;
        juce::String label = item.name;
        if (presetManager.loadEQCatalogPreset(item, definition, error))
            label = getEQPresetCategoryLabel(definition.category) + " - " +
                    item.name;
        presetCombo.addItem(label, itemId++);
    }
}

void MonitorGraphicEQComponent::ResponseDisplay::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();
    graphics.fillAll(juce::Colour(0xff090d14));
    graphics.setColour(juce::Colour(0xff1e293b));
    graphics.drawRect(bounds, 1.0f);

    const auto xForFrequency = [width](float frequency)
    {
        const float normalized = std::log10(frequency / 20.0f) /
                                 std::log10(20000.0f / 20.0f);
        return normalized * width;
    };
    const auto yForGain = [height](float gainDb)
    {
        const float normalized = juce::jlimit(-12.0f, 12.0f, gainDb);
        return (height - 12.0f) -
               ((normalized + 12.0f) / 24.0f) * (height - 24.0f);
    };

    graphics.setFont(juce::FontOptions(9.0f));
    graphics.setColour(juce::Colour(0xff64748b));
    for (const float frequency : { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
    {
        const float x = xForFrequency(frequency);
        graphics.drawVerticalLine((int)x, 0.0f, height - 12.0f);
        const juce::String label = frequency >= 1000.0f
            ? juce::String(frequency / 1000.0f, 0) + "k"
            : juce::String((int)frequency);
        graphics.drawText(label, (int)x - 18, (int)height - 12, 36, 11,
                          juce::Justification::centred);
    }
    for (const float gain : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yForGain(gain);
        graphics.setColour(gain == 0.0f ? juce::Colour(0x6638bdf8)
                                        : juce::Colour(0x2238bdf8));
        graphics.drawHorizontalLine((int)y, 0.0f, width);
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.drawText(juce::String((int)gain), 2, (int)y - 6, 28, 12,
                          juce::Justification::left);
    }

    juce::Path response;
    constexpr int pointCount = 240;
    for (int point = 0; point <= pointCount; ++point)
    {
        const float x = (float)point / (float)pointCount * width;
        const float frequency = 20.0f * std::pow(1000.0f, (float)point / pointCount);
        const float gainDb = owner.enabledToggle.getToggleState()
            ? owner.processor.getMonitorGraphicEQMagnitudeDb(frequency) : 0.0f;
        const float y = yForGain(gainDb);
        if (point == 0)
            response.startNewSubPath(x, y);
        else
            response.lineTo(x, y);
    }
    graphics.setColour(juce::Colour(0xff38bdf8));
    graphics.strokePath(response, juce::PathStrokeType(2.0f));

    const auto drawSpectrum = [&](const std::array<float, 1024>& spectrum,
                                  juce::Colour colour)
    {
        juce::Path path;
        bool started = false;
        for (size_t bin = 1; bin < spectrum.size(); ++bin)
        {
            const float frequency = (float)bin *
                                    (float)owner.processor.getSampleRate() / 2048.0f;
            if (frequency < 20.0f || frequency > 20000.0f)
                continue;
            const float x = xForFrequency(frequency);
            const float level = juce::jlimit(-90.0f, 0.0f, spectrum[bin]);
            const float y = height - 12.0f - ((level + 90.0f) / 90.0f) *
                                              (height - 24.0f);
            if (!started)
            {
                path.startNewSubPath(x, y);
                started = true;
            }
            else
                path.lineTo(x, y);
        }
        graphics.setColour(colour);
        graphics.strokePath(path, juce::PathStrokeType(1.4f));
    };

    if (owner.analyzerEnabledToggle.getToggleState())
    {
        const bool stereo = owner.analyzerModeCombo.getSelectedId() == 2;
        if (owner.preAnalyzerToggle.getToggleState())
        {
            drawSpectrum(owner.preAnalyzerSpectrum, juce::Colour(0xffffb454));
            if (stereo)
                drawSpectrum(owner.preAnalyzerRightSpectrum,
                             juce::Colour(0xfffb7185));
        }
        if (owner.postAnalyzerToggle.getToggleState())
        {
            drawSpectrum(owner.postAnalyzerSpectrum, juce::Colour(0xff4ade80));
            if (stereo)
                drawSpectrum(owner.postAnalyzerRightSpectrum,
                             juce::Colour(0xff38bdf8));
        }
        graphics.setFont(juce::FontOptions(10.0f));
        graphics.setColour(juce::Colour(0xffffb454));
        graphics.drawText(stereo ? "Pre-L" : "Pre", 8, 3, 38, 12,
                          juce::Justification::left);
        graphics.setColour(juce::Colour(0xff4ade80));
        graphics.drawText(stereo ? "Post-L" : "Post", 49, 3, 44, 12,
                          juce::Justification::left);
        if (stereo)
        {
            graphics.setColour(juce::Colour(0xfffb7185));
            graphics.drawText("Pre-R", 96, 3, 38, 12,
                              juce::Justification::left);
            graphics.setColour(juce::Colour(0xff38bdf8));
            graphics.drawText("Post-R", 137, 3, 48, 12,
                              juce::Justification::left);
        }
    }
}
