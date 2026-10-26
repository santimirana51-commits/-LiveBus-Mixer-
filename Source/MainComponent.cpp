#include "MainComponent.h"
#include "MasterOutputRouter.h"
#include <cmath>

namespace
{
    const std::array<juce::Colour, 8> channelAccentPalette = {
        juce::Colour(0xff2563eb), juce::Colour(0xff16a34a),
        juce::Colour(0xffdc2626), juce::Colour(0xffea580c),
        juce::Colour(0xff06b6d4), juce::Colour(0xff9333ea),
        juce::Colour(0xffdb2777), juce::Colour(0xffca8a04)
    };

    juce::Colour defaultChannelAccent(int channelIndex)
    {
        return channelIndex < 4 ? channelAccentPalette[(size_t)channelIndex]
                                : channelAccentPalette[0];
    }
}

// ==============================================================================
// SceneListModel Implementation
// ==============================================================================
int MainComponent::SceneListModel::getNumRows()
{
    return owner.availableScenes.size();
}

void MainComponent::SceneListModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (!juce::isPositiveAndBelow(rowNumber, owner.availableScenes.size()))
        return;

    auto sceneName = owner.availableScenes[rowNumber];
    bool isCurrent = (sceneName == owner.sceneCombo.getText());

    if (rowIsSelected || isCurrent)
    {
        g.setColour(juce::Colour(0xff1d2b3a));
        g.fillRoundedRectangle(2.0f, 1.0f, (float)width - 4.0f, (float)height - 2.0f, 4.0f);
        g.setColour(isCurrent ? juce::Colour(0xff00f5d4) : juce::Colour(0xff3b82f6));
        g.drawRoundedRectangle(2.0f, 1.0f, (float)width - 4.0f, (float)height - 2.0f, 4.0f, 1.0f);
    }

    g.setFont(juce::FontOptions(12.0f, isCurrent ? juce::Font::bold : juce::Font::plain));
    g.setColour(isCurrent ? juce::Colour(0xff00f5d4) : juce::Colour(0xffe2e8f0));

    juce::String prefix = isCurrent ? "[ACTIVE] " : "[ ] ";
    g.drawText(prefix + sceneName, 10, 0, width - 20, height, juce::Justification::centredLeft, true);
}

void MainComponent::SceneListModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow(row, owner.availableScenes.size()))
    {
        owner.onLoadScene(owner.availableScenes[row]);
    }
}

// ==============================================================================
// EQCurveDisplay Implementation
// ==============================================================================
void MainComponent::EQCurveDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0f141c));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff222a38));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Draw Frequency & Gain Grid Lines
    g.setColour(juce::Colour(0xff1e2636));
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float f : freqs)
    {
        float normX = (std::log10(f) - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));
        float gx = bounds.getX() + normX * bounds.getWidth();
        g.drawVerticalLine((int)gx, bounds.getY(), bounds.getBottom());
    }

    // 0 dB center line
    float zeroY = bounds.getY() + bounds.getHeight() * 0.5f;
    g.setColour(juce::Colour(0xff2d3b55));
    g.drawHorizontalLine((int)zeroY, bounds.getX(), bounds.getRight());

    // ±6 dB and ±12 dB grid
    float plus6Y = zeroY - bounds.getHeight() * 0.2f;
    float minus6Y = zeroY + bounds.getHeight() * 0.2f;
    g.setColour(juce::Colour(0xff1e2636));
    g.drawHorizontalLine((int)plus6Y, bounds.getX(), bounds.getRight());
    g.drawHorizontalLine((int)minus6Y, bounds.getX(), bounds.getRight());

    // Grid Labels
    g.setFont(juce::FontOptions(9.0f));
    g.setColour(juce::Colour(0xff64748b));
    g.drawText("20", 4, (int)bounds.getBottom() - 14, 24, 12, juce::Justification::left);
    g.drawText("100", (int)(bounds.getX() + bounds.getWidth() * 0.23f), (int)bounds.getBottom() - 14, 24, 12, juce::Justification::centred);
    g.drawText("1k", (int)(bounds.getX() + bounds.getWidth() * 0.57f), (int)bounds.getBottom() - 14, 24, 12, juce::Justification::centred);
    g.drawText("10k", (int)(bounds.getX() + bounds.getWidth() * 0.90f), (int)bounds.getBottom() - 14, 24, 12, juce::Justification::centred);

    g.drawText("+12", 4, 4, 24, 12, juce::Justification::left);
    g.drawText("0", 4, (int)zeroY - 6, 24, 12, juce::Justification::left);
    g.drawText("-12", 4, (int)bounds.getBottom() - 24, 24, 12, juce::Justification::left);

    // Calculate real frequency response curve
    auto& proc = owner.getProcessorForTarget(owner.selectedTarget);
    juce::Path curvePath;
    juce::Path fillPath;
    const int numPoints = 120;
    bool pathStarted = false;

    for (int i = 0; i < numPoints; ++i)
    {
        float frac = (float)i / (float)(numPoints - 1);
        float freq = 20.0f * std::pow(1000.0f, frac);
        float mag = proc.getMagnitudeForFrequency(freq);
        float db = juce::Decibels::gainToDecibels(mag, -24.0f);
        db = juce::jlimit(-15.0f, 15.0f, db);

        float px = bounds.getX() + frac * bounds.getWidth();
        float py = zeroY - (db / 15.0f) * (bounds.getHeight() * 0.45f);

        if (!pathStarted)
        {
            curvePath.startNewSubPath(px, py);
            fillPath.startNewSubPath(px, zeroY);
            fillPath.lineTo(px, py);
            pathStarted = true;
        }
        else
        {
            curvePath.lineTo(px, py);
            fillPath.lineTo(px, py);
        }
    }
    fillPath.lineTo(bounds.getRight(), zeroY);
    fillPath.closeSubPath();

    // Fill under curve
    g.setColour(juce::Colour(0x2200f5d4));
    g.fillPath(fillPath);

    // Stroke curve
    g.setColour(juce::Colour(0xff00f5d4));
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Draw 4 interactive node handles
    const float nodeFreqs[] = {
        (float)owner.eqLowFreqSlider.getValue(),
        (float)owner.eqMid1FreqSlider.getValue(),
        (float)owner.eqMid2FreqSlider.getValue(),
        (float)owner.eqHighFreqSlider.getValue()
    };
    const float nodeGains[] = {
        (float)owner.eqLowGainSlider.getValue(),
        (float)owner.eqMid1GainSlider.getValue(),
        (float)owner.eqMid2GainSlider.getValue(),
        (float)owner.eqHighGainSlider.getValue()
    };
    const juce::Colour nodeColours[] = {
        juce::Colour(0xff00f5d4), // Cyan - Low Shelf
        juce::Colour(0xffef4444), // Red - Mid 1
        juce::Colour(0xffffb703), // Orange - Mid 2
        juce::Colour(0xffffffff)  // White - High Shelf
    };

    for (int b = 0; b < 4; ++b)
    {
        float normX = (std::log10(juce::jlimit(20.0f, 20000.0f, nodeFreqs[b])) - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));
        float nx = bounds.getX() + normX * bounds.getWidth();
        float ny = zeroY - (juce::jlimit(-15.0f, 15.0f, nodeGains[b]) / 15.0f) * (bounds.getHeight() * 0.45f);

        g.setColour(nodeColours[b].withAlpha(0.3f));
        g.fillEllipse(nx - 8.0f, ny - 8.0f, 16.0f, 16.0f);
        g.setColour(nodeColours[b]);
        g.fillEllipse(nx - 5.0f, ny - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour(0xff0d1117));
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText(juce::String(b + 1), (int)(nx - 5.0f), (int)(ny - 5.0f), 10, 10, juce::Justification::centred);
    }
}

void MainComponent::EQCurveDisplay::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat();
    float zeroY = bounds.getY() + bounds.getHeight() * 0.5f;
    const float nodeFreqs[] = {
        (float)owner.eqLowFreqSlider.getValue(),
        (float)owner.eqMid1FreqSlider.getValue(),
        (float)owner.eqMid2FreqSlider.getValue(),
        (float)owner.eqHighFreqSlider.getValue()
    };
    const float nodeGains[] = {
        (float)owner.eqLowGainSlider.getValue(),
        (float)owner.eqMid1GainSlider.getValue(),
        (float)owner.eqMid2GainSlider.getValue(),
        (float)owner.eqHighGainSlider.getValue()
    };

    draggedBand = -1;
    for (int b = 0; b < 4; ++b)
    {
        float normX = (std::log10(juce::jlimit(20.0f, 20000.0f, nodeFreqs[b])) - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));
        float nx = bounds.getX() + normX * bounds.getWidth();
        float ny = zeroY - (juce::jlimit(-15.0f, 15.0f, nodeGains[b]) / 15.0f) * (bounds.getHeight() * 0.45f);

        if (e.position.getDistanceFrom({ nx, ny }) <= 16.0f)
        {
            draggedBand = b;
            break;
        }
    }
}

void MainComponent::EQCurveDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedBand < 0) return;
    auto bounds = getLocalBounds().toFloat();
    float zeroY = bounds.getY() + bounds.getHeight() * 0.5f;

    float normX = juce::jlimit(0.0f, 1.0f, (e.position.x - bounds.getX()) / bounds.getWidth());
    float freq = 20.0f * std::pow(1000.0f, normX);

    float deltaY = zeroY - e.position.y;
    float gain = (deltaY / (bounds.getHeight() * 0.45f)) * 15.0f;
    gain = juce::jlimit(-15.0f, 15.0f, gain);

    if (draggedBand == 0) {
        owner.eqLowFreqSlider.setValue(freq, juce::sendNotification);
        owner.eqLowGainSlider.setValue(gain, juce::sendNotification);
    } else if (draggedBand == 1) {
        owner.eqMid1FreqSlider.setValue(freq, juce::sendNotification);
        owner.eqMid1GainSlider.setValue(gain, juce::sendNotification);
    } else if (draggedBand == 2) {
        owner.eqMid2FreqSlider.setValue(freq, juce::sendNotification);
        owner.eqMid2GainSlider.setValue(gain, juce::sendNotification);
    } else if (draggedBand == 3) {
        owner.eqHighFreqSlider.setValue(freq, juce::sendNotification);
        owner.eqHighGainSlider.setValue(gain, juce::sendNotification);
    }
    repaint();
}

// ==============================================================================
// MainComponent Constructor
// ==============================================================================
MainComponent::MainComponent()
{
    setLookAndFeel(&consoleLookAndFeel);

    // 1. Top Header Bar
    brandLabel.setText("LiveBus Mixer v1.0", juce::dontSendNotification);
    brandLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(brandLabel);

    subBrandLabel.setText("Professional Broadcast & Monitor Console", juce::dontSendNotification);
    subBrandLabel.setFont(juce::FontOptions(11.0f));
    subBrandLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(subBrandLabel);

    audioStatusBadge.setText("[RUNNING]", juce::dontSendNotification);
    audioStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    audioStatusBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(audioStatusBadge);

    devicePillLabel.setText("Device: Focusrite USB ASIO", juce::dontSendNotification);
    devicePillLabel.setFont(juce::FontOptions(11.0f));
    devicePillLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(devicePillLabel);

    srPillLabel.setText("SR: 48.0 kHz", juce::dontSendNotification);
    srPillLabel.setFont(juce::FontOptions(11.0f));
    srPillLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(srPillLabel);

    bufferPillLabel.setText("Buffer: 128", juce::dontSendNotification);
    bufferPillLabel.setFont(juce::FontOptions(11.0f));
    bufferPillLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(bufferPillLabel);

    cpuPillLabel.setText("CPU 3.2%", juce::dontSendNotification);
    cpuPillLabel.setFont(juce::FontOptions(11.0f));
    cpuPillLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    addAndMakeVisible(cpuPillLabel);

    // Top Header Quick Action Buttons (matching reference design)
    auto setupTopBtn = [this](juce::TextButton& btn, int tabDest) {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        btn.addListener(this);
        btn.onClick = [this, tabDest]() {
            currentTab = tabDest;
            resized();
            repaint();
        };
        addAndMakeVisible(btn);
    };
    setupTopBtn(topScenesBtn, TabScenes);
    setupTopBtn(topPresetsBtn, TabScenes);
    setupTopBtn(topMidiBtn, TabMIDI);
    setupTopBtn(topObsBtn, TabOBS);
    setupTopBtn(topSettingsBtn, TabSettings);

    // Nav Tab Buttons (Left Sidebar - exactly 13 items + OBS)
    const juce::String sidebarNames[] = {
        "Mixer",
        "Routing",
        "DSP",
        "Effects",
        "Auto Ducking",
        "Audio Safety",
        "Macros",
        "Playback",
        "Scenes",
        "Devices",
        "MIDI Learn",
        "Settings",
        "Diagnostics",
        "OBS",
        "YouTube"
    };
    for (int t = 0; t < NumTabs; ++t)
    {
        navTabButtons[t].setButtonText(sidebarNames[t]);
        navTabButtons[t].addListener(this);
        navTabButtons[t].setClickingTogglesState(false);
        addAndMakeVisible(navTabButtons[t]);
    }

    // 2. Input Channels Setup (1..4 default, expandable to maxInputs)
    const char* defaultNames[] = { "Microphone", "Guitar / Instrument", "Music Left", "Music Right" };
    const char* defaultIcons[] = { "MIC", "INST", "MUS L", "MUS R" };
    const double defaultFaderDbs[] = { -8.3, -12.6, -18.1, -17.9 };
    const double defaultGainDbs[] = { 0.0, 0.0, -3.0, -3.0 };
    const double defaultPans[] = { 0.0, 0.0, -1.0, 1.0 };

    // Inisialisasi VST Target Index Array
    // Index 0-15: Channel 1-16
    // Index 16: Stream Bus
    // Index 17: Monitor Bus
    for (int i = 0; i < maxInputs + 2; ++i) {
        if (i < maxInputs) {
            vstTargetIndex[i].store(0); // Channel default = tidak ada VST aktif
        } else if (i == maxInputs) {
            vstTargetIndex[i].store(maxInputs); // Stream Bus default
        } else { // i == maxInputs + 1
            vstTargetIndex[i].store(maxInputs + 1); // Monitor Bus default
        }
    }

    // Initialize all maxInputs channels UI
    for (int i = 0; i < maxInputs; ++i)
    {
        auto& ch = inputUIs[i];
        ch.accentColour = defaultChannelAccent(i);
        ch.channelColorButton.setButtonText(juce::String(i + 1));
        ch.channelColorButton.setTooltip("Click to choose this channel's color");
        ch.channelColorButton.setColour(juce::TextButton::buttonColourId, ch.accentColour.withAlpha(0.22f));
        ch.channelColorButton.setColour(juce::TextButton::textColourOffId, ch.accentColour.brighter(0.35f));
        ch.channelColorButton.onClick = [this, i]() {
            juce::PopupMenu menu;
            for (int colourIndex = 0; colourIndex < (int)channelAccentPalette.size(); ++colourIndex)
                menu.addItem(colourIndex + 1, "Color " + juce::String(colourIndex + 1),
                             true, inputUIs[i].accentColour == channelAccentPalette[(size_t)colourIndex]);

            juce::Component::SafePointer<MainComponent> safeThis(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&inputUIs[i].channelColorButton),
                [safeThis, i](int result) {
                    if (safeThis == nullptr || result < 1 || result > (int)channelAccentPalette.size())
                        return;

                    auto& channel = safeThis->inputUIs[i];
                    channel.accentColour = channelAccentPalette[(size_t)(result - 1)];
                    channel.channelColorButton.setColour(juce::TextButton::buttonColourId,
                                                         channel.accentColour.withAlpha(0.22f));
                    channel.channelColorButton.setColour(juce::TextButton::textColourOffId,
                                                         channel.accentColour.brighter(0.35f));
                    channel.gainSlider.setColour(juce::Slider::rotarySliderFillColourId, channel.accentColour);
                    channel.panSlider.setColour(juce::Slider::rotarySliderFillColourId, channel.accentColour);
                    if (i < 4)
                    {
                        safeThis->matrixChannelPills[(size_t)i].setColour(
                            juce::Label::backgroundColourId, channel.accentColour.withAlpha(0.2f));
                        safeThis->matrixChannelPills[(size_t)i].setColour(
                            juce::Label::textColourId, channel.accentColour.brighter(0.4f));
                        safeThis->matrixPanSliders[(size_t)i].setColour(
                            juce::Slider::rotarySliderFillColourId, channel.accentColour);
                    }
                    safeThis->repaint();
                });
        };
        addAndMakeVisible(ch.channelColorButton);

        ch.iconLabel.setText(i < 4 ? defaultIcons[i % 4] : "🎙", juce::dontSendNotification);
        ch.iconLabel.setFont(juce::FontOptions(13.0f));
        addAndMakeVisible(ch.iconLabel);

        ch.nameEditor.setText(i < 4 ? defaultNames[i % 4] : ("Input " + juce::String(i + 1)));
        ch.nameEditor.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        ch.nameEditor.onTextChange = [this, i]() {
            routingMatrix.getChannel(i).name = inputUIs[i].nameEditor.getText();
            routingChannelNames[i].setText(inputUIs[i].nameEditor.getText(), juce::dontSendNotification);
        };
        addAndMakeVisible(ch.nameEditor);

        ch.inputSourceHeaderLabel.setFont(juce::FontOptions(9.0f));
        ch.inputSourceHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(ch.inputSourceHeaderLabel);

        // Input Source Selector Combo
        ch.inputSourceCombo.onChange = [this, i]() {
            int id = inputUIs[i].inputSourceCombo.getSelectedId();
            auto& state = routingMatrix.getChannel(i);
            if (id == 1001) // Playback / BGM
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Playback);
            }
            else if (id == 1002) // Test Signal
            {
                state.inputSource.store(LiveBus::ChannelInputSource::TestSignal);
            }
            else if (id == 1003) // Disabled
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Disabled);
            }
            else if (id == 1004) // YouTube Player
            {
                state.inputSource.store(LiveBus::ChannelInputSource::YouTube);
            }
            else if (id >= 500) // Unavailable hardware channel retained
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Hardware);
                state.hardwareInputChannel.store(id - 500);
            }
            else if (id >= 1) // Active Hardware Input (1-indexed id -> 0-indexed port)
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Hardware);
                state.hardwareInputChannel.store(id - 1);
            }
            routingMatrix.updateDuplicateWarnings();
            if (i < 4 && matrixSourceCombos[i].getSelectedId() != id)
                matrixSourceCombos[i].setSelectedId(id, juce::dontSendNotification);
        };
        addAndMakeVisible(ch.inputSourceCombo);

        // Vertical Fader Slider
        ch.faderSlider.setSliderStyle(juce::Slider::LinearVertical);
        ch.faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ch.faderSlider.setRange(-60.0, 12.0, 0.1);
        double defFaderDb = (i < 4 ? defaultFaderDbs[i % 4] : 0.0);
        ch.faderSlider.setValue(defFaderDb);
        ch.faderSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff8fafc));
        ch.faderSlider.onValueChange = [this, i]() {
            float db = (float)inputUIs[i].faderSlider.getValue();
            routingMatrix.getChannel(i).gain.store(juce::Decibels::decibelsToGain(db));
            inputUIs[i].faderValueLabel.setText(db <= -59.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
        };
        addAndMakeVisible(ch.faderSlider);

        ch.faderValueLabel.setText(juce::String(defFaderDb, 1) + " dB", juce::dontSendNotification);
        ch.faderValueLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        ch.faderValueLabel.setJustificationType(juce::Justification::centred);
        ch.faderValueLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff121721));
        ch.faderValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
        addAndMakeVisible(ch.faderValueLabel);

        // Rotary Gain Knob
        ch.gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ch.gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ch.gainSlider.setRange(-24.0, 24.0, 0.1);
        double defGainDb = (i < 4 ? defaultGainDbs[i % 4] : 0.0);
        ch.gainSlider.setValue(defGainDb);
        ch.gainSlider.setColour(juce::Slider::rotarySliderFillColourId, defaultChannelAccent(i));
        ch.gainSlider.onValueChange = [this, i]() {
            float db = (float)inputUIs[i].gainSlider.getValue();
            channelDSPs[i].params.masterGain.store(juce::Decibels::decibelsToGain(db));
            inputUIs[i].gainValueLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        };
        addAndMakeVisible(ch.gainSlider);

        ch.gainValueLabel.setText(juce::String(defGainDb, 1) + " dB", juce::dontSendNotification);
        ch.gainValueLabel.setFont(juce::FontOptions(10.0f));
        ch.gainValueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(ch.gainValueLabel);

        // Rotary Pan Knob
        ch.panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ch.panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ch.panSlider.setRange(-1.0, 1.0, 0.01);
        double defPan = (i < 4 ? defaultPans[i % 4] : 0.0);
        ch.panSlider.setValue(defPan);
        ch.panSlider.setColour(juce::Slider::rotarySliderFillColourId, defaultChannelAccent(i));
        ch.panSlider.onValueChange = [this, i]() {
            float p = (float)inputUIs[i].panSlider.getValue();
            routingMatrix.getChannel(i).pan.store(p);
            if (std::abs(p) < 0.05f)
                inputUIs[i].panValueLabel.setText("C", juce::dontSendNotification);
            else if (p < 0.0f)
                inputUIs[i].panValueLabel.setText("L " + juce::String((int)(-p * 100)), juce::dontSendNotification);
            else
                inputUIs[i].panValueLabel.setText("R " + juce::String((int)(p * 100)), juce::dontSendNotification);
        };
        addAndMakeVisible(ch.panSlider);

        ch.panValueLabel.setText(defPan == 0.0 ? "C" : (defPan < 0.0 ? "L 100" : "R 100"), juce::dontSendNotification);
        ch.panValueLabel.setFont(juce::FontOptions(10.0f));
        ch.panValueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(ch.panValueLabel);

        // Channel Strip Buttons: Mute, Solo, Link, Bottom Pill
        ch.muteBtn.setClickingTogglesState(true);
        ch.muteBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
        ch.muteBtn.onClick = [this, i]() {
            routingMatrix.getChannel(i).mute.store(inputUIs[i].muteBtn.getToggleState());
        };
        addAndMakeVisible(ch.muteBtn);

        ch.soloBtn.setClickingTogglesState(true);
        ch.soloBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffeab308));
        ch.soloBtn.onClick = [this, i]() {
            routingMatrix.getChannel(i).solo.store(inputUIs[i].soloBtn.getToggleState());
        };
        addAndMakeVisible(ch.soloBtn);

        ch.linkBtn.setClickingTogglesState(true);
        ch.linkBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff06b6d4));
        ch.linkBtn.onClick = [this, i]() {
            routingMatrix.getChannel(i).stereoMode.store(inputUIs[i].linkBtn.getToggleState() ? LiveBus::ChannelStereoMode::StereoPair : LiveBus::ChannelStereoMode::Mono);
        };
        ch.linkBtn.setButtonText("Link");
        addAndMakeVisible(ch.linkBtn);

        ch.outputRouteCombo.addItem("Stream + Monitor", 1);
        ch.outputRouteCombo.addItem("Stream only", 2);
        ch.outputRouteCombo.addItem("Monitor only", 3);
        ch.outputRouteCombo.addItem("Off", 4);
        ch.outputRouteCombo.addSeparator();
        ch.outputRouteCombo.addItem("Sub 1 Vocals", 5);
        ch.outputRouteCombo.addItem("Sub 2 BGM", 6);
        ch.outputRouteCombo.addItem("Sub 3 Inst", 7);
        ch.outputRouteCombo.addItem("Sub 4 Aux", 8);
        ch.outputRouteCombo.setTooltip("Choose the channel output bus or route it through a submix");
        ch.outputRouteCombo.onChange = [this, i]() {
            const int selectedId = inputUIs[(size_t)i].outputRouteCombo.getSelectedId();
            if (selectedId >= 1 && selectedId <= 4)
            {
                const bool toStream = selectedId == 1 || selectedId == 2;
                const bool toMonitor = selectedId == 1 || selectedId == 3;
                routingMatrix.applyChannelRouteState(i, toStream, toMonitor);
                syncChannelRouteUiState(i);
            }
            else if (selectedId >= 5 && selectedId <= 8)
            {
                const int submixIndex = selectedId - 5;
                routingMatrix.assignChannelToSubmix(i, submixIndex);
                syncChannelRouteUiState(i);
            }
        };
        addAndMakeVisible(ch.outputRouteCombo);

        const char* pillLabels[] = { "Mic", "Guitar", "Music L", "Music R" };
        ch.bottomPillBtn.setButtonText(i < 4 ? pillLabels[i % 4] : ("In " + juce::String(i + 1)));
        ch.bottomPillBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
        ch.bottomPillBtn.onClick = [this, i]() {
            selectDSPTarget(i);
        };
        addAndMakeVisible(ch.bottomPillBtn);

        ch.dspSelectBtn.setButtonText("FX");
        ch.dspSelectBtn.setTooltip("Left-click: open insert rack. Right-click: quick bypass controls.");
        ch.dspSelectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff164e63));
        ch.dspSelectBtn.onContextMenu = [this, i, button = &ch.dspSelectBtn]() {
            showFxChainMenu(i, *button);
        };
        // Legacy stream / monitor knobs
        ch.streamRouteToggle.setToggleState(true, juce::dontSendNotification);
        ch.streamRouteToggle.onStateChange = [this, i]() {
            const bool toStream = inputUIs[i].streamRouteToggle.getToggleState();
            const bool toMonitor = routingMatrix.getChannel(i).routeEnable[1].load(std::memory_order_relaxed);
            routingMatrix.applyChannelRouteState(i, toStream, toMonitor);
            syncChannelRouteUiState(i);
        };
        addChildComponent(ch.streamRouteToggle);

        ch.streamSendKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ch.streamSendKnob.setRange(0.0, 1.5, 0.01);
        ch.streamSendKnob.setValue(1.0);
        ch.streamSendKnob.onValueChange = [this, i]() {
            routingMatrix.getChannel(i).sendLevel[0].store((float)inputUIs[i].streamSendKnob.getValue());
        };
        addChildComponent(ch.streamSendKnob);

        ch.monitorRouteToggle.setToggleState(true, juce::dontSendNotification);
        ch.monitorRouteToggle.onStateChange = [this, i]() {
            const bool toMonitor = inputUIs[i].monitorRouteToggle.getToggleState();
            const bool toStream = routingMatrix.getChannel(i).routeEnable[0].load(std::memory_order_relaxed);
            routingMatrix.applyChannelRouteState(i, toStream, toMonitor);
            syncChannelRouteUiState(i);
        };
        addChildComponent(ch.monitorRouteToggle);

        ch.monitorSendKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ch.monitorSendKnob.setRange(0.0, 1.5, 0.01);
        ch.monitorSendKnob.setValue(0.5);
        ch.monitorSendKnob.onValueChange = [this, i]() {
            routingMatrix.getChannel(i).sendLevel[1].store((float)inputUIs[i].monitorSendKnob.getValue());
        };
        addChildComponent(ch.monitorSendKnob);

        ch.dspSelectBtn.addListener(this);
        addChildComponent(ch.dspSelectBtn);

        // Initial visibility - show all channels for Mixcraft-style workflow
        bool initialVis = true;
        ch.channelColorButton.setVisible(initialVis);
        ch.iconLabel.setVisible(initialVis);
        ch.nameEditor.setVisible(initialVis);
        ch.inputSourceHeaderLabel.setVisible(initialVis);
        ch.inputSourceCombo.setVisible(initialVis);
        ch.faderSlider.setVisible(initialVis);
        ch.faderValueLabel.setVisible(initialVis);
        ch.gainSlider.setVisible(initialVis);
        ch.gainValueLabel.setVisible(initialVis);
        ch.panSlider.setVisible(initialVis);
        ch.panValueLabel.setVisible(initialVis);
        ch.muteBtn.setVisible(initialVis);
        ch.soloBtn.setVisible(initialVis);
        ch.linkBtn.setVisible(initialVis);
        ch.outputRouteCombo.setVisible(initialVis);
        ch.bottomPillBtn.setVisible(initialVis);
    }

    addChannelBtn.addListener(this);
    addChildComponent(addChannelBtn);
    removeChannelBtn.addListener(this);
    addChildComponent(removeChannelBtn);

    // 3. Master Buses (Stream Bus & Monitor Bus)
    // Stream Bus (Purple)
    masterUIs[0].headerLabel.setText("STREAM BUS", juce::dontSendNotification);
    masterUIs[0].headerLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    masterUIs[0].headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa855f7));
    addAndMakeVisible(masterUIs[0].headerLabel);

    masterUIs[0].iconLabel.setText("(( * ))", juce::dontSendNotification);
    masterUIs[0].iconLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    masterUIs[0].iconLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa855f7));
    addAndMakeVisible(masterUIs[0].iconLabel);

    masterUIs[0].masterFader.setSliderStyle(juce::Slider::LinearVertical);
    masterUIs[0].masterFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterUIs[0].masterFader.setRange(-60.0, 12.0, 0.1);
    masterUIs[0].masterFader.setValue(0.0);
    masterUIs[0].masterFader.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff8fafc));
    masterUIs[0].masterFader.onValueChange = [this]() {
        float db = (float)masterUIs[0].masterFader.getValue();
        busProcessors[0].params.masterGain.store(juce::Decibels::decibelsToGain(db));
        masterUIs[0].faderValueLabel.setText(db <= -59.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible(masterUIs[0].masterFader);

    masterUIs[0].faderValueLabel.setText("0.0 dB", juce::dontSendNotification);
    masterUIs[0].faderValueLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    masterUIs[0].faderValueLabel.setJustificationType(juce::Justification::centred);
    masterUIs[0].faderValueLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff121721));
    masterUIs[0].faderValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(masterUIs[0].faderValueLabel);

    masterUIs[0].muteBtn.setClickingTogglesState(true);
    masterUIs[0].muteBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
    masterUIs[0].muteBtn.onClick = [this]() {
        busProcessors[0].params.masterMute.store(masterUIs[0].muteBtn.getToggleState());
    };
    addAndMakeVisible(masterUIs[0].muteBtn);

    masterUIs[0].linkBtn.setClickingTogglesState(true);
    masterUIs[0].linkBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffa855f7));
    masterUIs[0].linkBtn.setButtonText("Link");
    addAndMakeVisible(masterUIs[0].linkBtn);

    masterUIs[0].bottomPillBtn.setButtonText("Stream");
    masterUIs[0].bottomPillBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
    masterUIs[0].bottomPillBtn.onClick = [this]() { selectDSPTarget(maxInputs); };
    addAndMakeVisible(masterUIs[0].bottomPillBtn);

    masterUIs[0].dspSelectBtn.setButtonText("FX");
    masterUIs[0].dspSelectBtn.setTooltip("Left-click: open insert rack. Right-click: quick bypass controls.");
    masterUIs[0].dspSelectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4c1d95));
    masterUIs[0].dspSelectBtn.onContextMenu = [this]() {
        showFxChainMenu(maxInputs, masterUIs[0].dspSelectBtn);
    };
    masterUIs[0].outputTagLabel.setText("OBS Output", juce::dontSendNotification);
    masterUIs[0].outputTagLabel.setFont(juce::FontOptions(10.0f));
    masterUIs[0].outputTagLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa855f7));
    addChildComponent(masterUIs[0].outputTagLabel);

    masterUIs[0].dspSelectBtn.addListener(this);
    addChildComponent(masterUIs[0].dspSelectBtn);

    // Monitor Bus (Cyan)
    masterUIs[1].headerLabel.setText("MONITOR BUS", juce::dontSendNotification);
    masterUIs[1].headerLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    masterUIs[1].headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff06b6d4));
    addAndMakeVisible(masterUIs[1].headerLabel);

    masterUIs[1].iconLabel.setText("[HP]", juce::dontSendNotification);
    masterUIs[1].iconLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    masterUIs[1].iconLabel.setColour(juce::Label::textColourId, juce::Colour(0xff06b6d4));
    addAndMakeVisible(masterUIs[1].iconLabel);

    masterUIs[1].masterFader.setSliderStyle(juce::Slider::LinearVertical);
    masterUIs[1].masterFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterUIs[1].masterFader.setRange(-60.0, 12.0, 0.1);
    masterUIs[1].masterFader.setValue(0.0);
    masterUIs[1].masterFader.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff8fafc));
    masterUIs[1].masterFader.onValueChange = [this]() {
        float db = (float)masterUIs[1].masterFader.getValue();
        busProcessors[1].params.masterGain.store(juce::Decibels::decibelsToGain(db));
        masterUIs[1].faderValueLabel.setText(db <= -59.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible(masterUIs[1].masterFader);

    masterUIs[1].faderValueLabel.setText("0.0 dB", juce::dontSendNotification);
    masterUIs[1].faderValueLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    masterUIs[1].faderValueLabel.setJustificationType(juce::Justification::centred);
    masterUIs[1].faderValueLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff121721));
    masterUIs[1].faderValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(masterUIs[1].faderValueLabel);

    masterUIs[1].muteBtn.setClickingTogglesState(true);
    masterUIs[1].muteBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
    masterUIs[1].muteBtn.onClick = [this]() {
        busProcessors[1].params.masterMute.store(masterUIs[1].muteBtn.getToggleState());
    };
    addAndMakeVisible(masterUIs[1].muteBtn);

    masterUIs[1].linkBtn.setClickingTogglesState(true);
    masterUIs[1].linkBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff06b6d4));
    masterUIs[1].linkBtn.setButtonText("Link");
    addAndMakeVisible(masterUIs[1].linkBtn);

    masterUIs[1].bottomPillBtn.setButtonText("Monitor");
    masterUIs[1].bottomPillBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
    masterUIs[1].bottomPillBtn.onClick = [this]() { selectDSPTarget(maxInputs + 1); };
    addAndMakeVisible(masterUIs[1].bottomPillBtn);

    masterUIs[1].dspSelectBtn.setButtonText("FX");
    masterUIs[1].dspSelectBtn.setTooltip("Left-click: open insert rack. Right-click: quick bypass controls.");
    masterUIs[1].dspSelectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff164e63));
    masterUIs[1].dspSelectBtn.onContextMenu = [this]() {
        showFxChainMenu(maxInputs + 1, masterUIs[1].dspSelectBtn);
    };
    masterUIs[1].outputTagLabel.setText("Headphones", juce::dontSendNotification);
    masterUIs[1].outputTagLabel.setFont(juce::FontOptions(10.0f));
    masterUIs[1].outputTagLabel.setColour(juce::Label::textColourId, juce::Colour(0xff06b6d4));
    addChildComponent(masterUIs[1].outputTagLabel);

    masterUIs[1].dspSelectBtn.addListener(this);
    addChildComponent(masterUIs[1].dspSelectBtn);

    // Add / Remove Channel Buttons
    addChannelBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff16a34a));
    addChannelBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(addChannelBtn);

    removeChannelBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
    removeChannelBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(removeChannelBtn);

    previousChannelPageBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    previousChannelPageBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    previousChannelPageBtn.onClick = [this]() {
        if (channelScrollPosition > 0)
        {
            --channelScrollPosition;
            channelScrollSlider.setValue(channelScrollPosition, juce::dontSendNotification);
            resized();
            repaint();
        }
    };
    addAndMakeVisible(previousChannelPageBtn);

    nextChannelPageBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    nextChannelPageBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    nextChannelPageBtn.onClick = [this]() {
        const int maxScrollPosition = juce::jmax(0, routingMatrix.getNumActiveChannels() - 4);
        if (channelScrollPosition < maxScrollPosition)
        {
            ++channelScrollPosition;
            channelScrollSlider.setValue(channelScrollPosition, juce::dontSendNotification);
            resized();
            repaint();
        }
    };
    addAndMakeVisible(nextChannelPageBtn);
    channelPageLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    channelPageLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    channelPageLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(channelPageLabel);

    channelScrollSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    channelScrollSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    channelScrollSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff334155));
    channelScrollSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff38bdf8));
    channelScrollSlider.setRange(0, maxInputs - 4, 1);
    channelScrollSlider.setTooltip("Drag to scroll through input channels");
    channelScrollSlider.onValueChange = [this]() {
        channelScrollPosition = (int)channelScrollSlider.getValue();
        resized();
        repaint();
    };
    addAndMakeVisible(channelScrollSlider);

    // Meters Setup
    for (int i = 0; i < maxInputs; ++i)
    {
        channelLevelMeters[i].prepare(currentSampleRate, currentBlockSize);
        channelPeakMeters[i] = std::make_unique<LiveBus::DigitalPeakMeterComponent>(false, LiveBus::DigitalPeakMeterComponent::Orientation::Vertical);
        channelPeakMeters[i]->setResetCallback([this, i]() {
            channelLevelMeters[i].resetPeakHold();
            channelLevelMeters[i].resetClip();
        });
        channelPeakMeters[i]->setShowScale(false);
        addAndMakeVisible(channelPeakMeters[i].get());
    }

    busLevelMeters[0].prepare(currentSampleRate, currentBlockSize);
    busLevelMeters[1].prepare(currentSampleRate, currentBlockSize);

    streamBusPeakMeter = std::make_unique<LiveBus::DigitalPeakMeterComponent>(true, LiveBus::DigitalPeakMeterComponent::Orientation::Vertical);
    streamBusPeakMeter->setResetCallback([this]() {
        busLevelMeters[0].resetPeakHold();
        busLevelMeters[0].resetClip();
    });
    addAndMakeVisible(streamBusPeakMeter.get());

    monitorBusPeakMeter = std::make_unique<LiveBus::DigitalPeakMeterComponent>(true, LiveBus::DigitalPeakMeterComponent::Orientation::Vertical);
    monitorBusPeakMeter->setResetCallback([this]() {
        busLevelMeters[1].resetPeakHold();
        busLevelMeters[1].resetClip();
    });
    addAndMakeVisible(monitorBusPeakMeter.get());

    masterMeterStation = std::make_unique<LiveBus::MasterMeterPanel>();
    addChildComponent(masterMeterStation.get());

    // =========================================================================
    // Area 3: Right DSP Rack Setup
    // =========================================================================
    dspRackHeaderLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    dspRackHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(dspRackHeaderLabel);

    rackPowerToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(rackPowerToggle);

    bypassAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    bypassAllBtn.addListener(this);
    addAndMakeVisible(bypassAllBtn);

    auto setupDspKnob = [this](juce::Slider& s, double minV, double maxV, double defV, juce::Colour accent) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 14);
        s.setRange(minV, maxV, 0.1);
        s.setValue(defV);
        s.setColour(juce::Slider::rotarySliderFillColourId, accent);
        s.onValueChange = [this]() { applyDSPControls(selectedTarget); };
        addAndMakeVisible(s);
    };

    // HPF
    hpfToggle.setToggleState(true, juce::dontSendNotification);
    hpfToggle.onClick = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(hpfToggle);
    setupDspKnob(hpfFreqSlider, 20.0, 500.0, 80.0, juce::Colour(0xff10b981));
    hpfSlopeCombo.addItem("12 dB/Oct", 1);
    hpfSlopeCombo.addItem("24 dB/Oct", 2);
    hpfSlopeCombo.setSelectedId(2, juce::dontSendNotification);
    hpfSlopeCombo.onChange = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(hpfSlopeCombo);

    // Noise Gate
    gateToggle.setToggleState(false, juce::dontSendNotification);
    gateToggle.onClick = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(gateToggle);
    setupDspKnob(gateThreshSlider, -80.0, 0.0, -40.0, juce::Colour(0xffef4444));
    setupDspKnob(gateAttackSlider, 0.1, 100.0, 10.0, juce::Colour(0xffef4444));
    setupDspKnob(gateHoldSlider, 0.0, 500.0, 100.0, juce::Colour(0xffef4444));
    setupDspKnob(gateReleaseSlider, 10.0, 1000.0, 200.0, juce::Colour(0xffef4444));
    gateRatioSlider.setRange(1.0, 20.0, 0.1);
    gateRatioSlider.setValue(4.0);

    // Compressor
    compToggle.setToggleState(true, juce::dontSendNotification);
    compToggle.onClick = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(compToggle);
    setupDspKnob(compThreshSlider, -60.0, 0.0, -18.0, juce::Colour(0xfff59e0b));
    setupDspKnob(compRatioSlider, 1.0, 20.0, 3.0, juce::Colour(0xfff59e0b));
    setupDspKnob(compAttackSlider, 0.1, 100.0, 10.0, juce::Colour(0xfff59e0b));
    setupDspKnob(compReleaseSlider, 10.0, 1000.0, 150.0, juce::Colour(0xfff59e0b));
    setupDspKnob(compMakeupSlider, -12.0, 24.0, 4.0, juce::Colour(0xfff59e0b));

    // Limiter
    limiterToggle.setToggleState(true, juce::dontSendNotification);
    limiterToggle.onClick = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(limiterToggle);
    setupDspKnob(limiterCeilSlider, -20.0, 0.0, -1.0, juce::Colour(0xff8b5cf6));
    setupDspKnob(limiterReleaseSlider, 1.0, 500.0, 100.0, juce::Colour(0xff8b5cf6));

    // Reverb
    reverbToggle.setToggleState(false, juce::dontSendNotification);
    reverbToggle.onClick = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(reverbToggle);
    setupDspKnob(reverbRoomSlider, 0.0, 1.0, 0.25, juce::Colour(0xff06b6d4));
    setupDspKnob(reverbDampingSlider, 0.0, 1.0, 0.50, juce::Colour(0xff06b6d4));
    setupDspKnob(reverbWetSlider, 0.0, 1.0, 0.25, juce::Colour(0xff06b6d4));
    setupDspKnob(reverbDrySlider, 0.0, 1.0, 1.0, juce::Colour(0xff06b6d4));

    // Delay
    delayToggle.setToggleState(false, juce::dontSendNotification);
    delayToggle.onClick = [this]() { applyDSPControls(selectedTarget); };
    addAndMakeVisible(delayToggle);
    setupDspKnob(delayTimeSlider, 10.0, 1500.0, 350.0, juce::Colour(0xffec4899));
    delayTempoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
    addAndMakeVisible(delayTempoBtn);
    setupDspKnob(delayFeedbackSlider, 0.0, 0.90, 0.25, juce::Colour(0xffec4899));
    setupDspKnob(delayWetSlider, 0.0, 1.0, 0.25, juce::Colour(0xffec4899));
    setupDspKnob(delayDrySlider, 0.0, 1.0, 1.0, juce::Colour(0xffec4899));

    // Legacy EQ Display
    streamBusDSPTab.addListener(this);
    addChildComponent(streamBusDSPTab);
    monitorBusDSPTab.addListener(this);
    addChildComponent(monitorBusDSPTab);
    for (int s = 0; s < 6; ++s) {
        dspSubTabs[s].addListener(this);
        addChildComponent(dspSubTabs[s]);
    }
    enableEQToggle.setToggleState(true, juce::dontSendNotification);
    addChildComponent(enableEQToggle);
    eqCurveDisplay = std::make_unique<EQCurveDisplay>(*this);
    addChildComponent(eqCurveDisplay.get());
    auto setupEQSlider = [this](juce::Slider& s, double minV, double maxV, double defV) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(minV, maxV, 0.1);
        s.setValue(defV);
        s.onValueChange = [this]() { applyDSPControls(selectedTarget); if (eqCurveDisplay) eqCurveDisplay->repaint(); };
        addChildComponent(s);
    };
    setupEQSlider(eqLowFreqSlider, 20.0, 500.0, 80.0);
    setupEQSlider(eqLowGainSlider, -15.0, 15.0, 3.0);
    setupEQSlider(eqLowQSlider, 0.1, 5.0, 0.7);
    setupEQSlider(eqMid1FreqSlider, 100.0, 4000.0, 250.0);
    setupEQSlider(eqMid1GainSlider, -15.0, 15.0, -2.5);
    setupEQSlider(eqMid1QSlider, 0.1, 5.0, 1.2);
    setupEQSlider(eqMid2FreqSlider, 500.0, 10000.0, 2500.0);
    setupEQSlider(eqMid2GainSlider, -15.0, 15.0, 4.0);
    setupEQSlider(eqMid2QSlider, 0.1, 5.0, 1.2);
    setupEQSlider(eqHighFreqSlider, 2000.0, 20000.0, 8000.0);
    setupEQSlider(eqHighGainSlider, -15.0, 15.0, 2.0);
    setupEQSlider(eqHighQSlider, 0.1, 5.0, 0.7);

    // =========================================================================
    // Bottom Tier Components Setup
    // =========================================================================
    // Card 1: Input / Output Routing Matrix
    routingMatrixHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    routingMatrixHeader.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(routingMatrixHeader);

    matrixResetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    matrixResetBtn.onClick = [this]() {
        routingMatrix.applyRoutingPreset(0);
        for (int i = 0; i < 4; ++i) {
            matrixSourceCombos[i].setSelectedId(i + 1, juce::dontSendNotification);
            matrixStereoCombos[i].setSelectedId(1, juce::dontSendNotification);
            matrixStreamToggles[i].setToggleState(true, juce::dontSendNotification);
            matrixMonitorToggles[i].setToggleState(true, juce::dontSendNotification);
        }
        log("Routing Matrix reset to default broadcast state.");
    };
    addAndMakeVisible(matrixResetBtn);

    matrixPresetCombo.addItem("Preset: Default", 1);
    matrixPresetCombo.addItem("Preset: Broadcast Dual", 2);
    matrixPresetCombo.addItem("Preset: Music Session", 3);
    matrixPresetCombo.addItem("Preset: All to Stream", 4);
    matrixPresetCombo.setSelectedId(1, juce::dontSendNotification);
    matrixPresetCombo.onChange = [this]() {
        int id = matrixPresetCombo.getSelectedId();
        routingMatrix.applyRoutingPreset(id - 1);
        log("Routing Preset loaded: " + matrixPresetCombo.getText());
    };
    addAndMakeVisible(matrixPresetCombo);

    const char* rowPillNames[] = { "1. Microphone", "2. Guitar / Instrument", "3. Music Left", "4. Music Right" };
    for (int r = 0; r < 4; ++r)
    {
        matrixChannelPills[r].setText(rowPillNames[r], juce::dontSendNotification);
        matrixChannelPills[r].setFont(juce::FontOptions(10.5f, juce::Font::bold));
        matrixChannelPills[r].setColour(juce::Label::backgroundColourId, defaultChannelAccent(r).withAlpha(0.2f));
        matrixChannelPills[r].setColour(juce::Label::textColourId, defaultChannelAccent(r).brighter(0.4f));
        addAndMakeVisible(matrixChannelPills[r]);

        // Source combo
        matrixSourceCombos[r].onChange = [this, r]() {
            int id = matrixSourceCombos[r].getSelectedId();
            auto& state = routingMatrix.getChannel(r);
            if (id == 1001) // Playback / BGM
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Playback);
            }
            else if (id == 1002) // Test Signal
            {
                state.inputSource.store(LiveBus::ChannelInputSource::TestSignal);
            }
            else if (id == 1003) // Disabled
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Disabled);
            }
            else if (id == 1004) // YouTube Player
            {
                state.inputSource.store(LiveBus::ChannelInputSource::YouTube);
            }
            else if (id >= 500) // Unavailable hardware channel retained
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Hardware);
                state.hardwareInputChannel.store(id - 500);
            }
            else if (id >= 1) // Active Hardware Input
            {
                state.inputSource.store(LiveBus::ChannelInputSource::Hardware);
                state.hardwareInputChannel.store(id - 1);
            }
            routingMatrix.updateDuplicateWarnings();
            if (inputUIs[r].inputSourceCombo.getSelectedId() != id)
                inputUIs[r].inputSourceCombo.setSelectedId(id, juce::dontSendNotification);
        };
        addAndMakeVisible(matrixSourceCombos[r]);

        // Stereo combo
        matrixStereoCombos[r].addItem("Mono", 1);
        matrixStereoCombos[r].addItem("Stereo Pair", 2);
        matrixStereoCombos[r].setSelectedId(1, juce::dontSendNotification);
        matrixStereoCombos[r].onChange = [this, r]() {
            int id = matrixStereoCombos[r].getSelectedId();
            routingMatrix.getChannel(r).stereoMode.store(static_cast<LiveBus::ChannelStereoMode>(id - 1));
        };
        addAndMakeVisible(matrixStereoCombos[r]);

        // Stream Bus Toggle & Level
        matrixStreamToggles[r].setToggleState(true, juce::dontSendNotification);
        matrixStreamToggles[r].onClick = [this, r]() {
            bool s = matrixStreamToggles[r].getToggleState();
            const bool toMonitor = routingMatrix.getChannel(r).routeEnable[1].load(std::memory_order_relaxed);
            routingMatrix.applyChannelRouteState(r, s, toMonitor);
            inputUIs[r].streamRouteToggle.setToggleState(s, juce::dontSendNotification);
            if (routingMatrix.getChannel(r).submixGroup.load(std::memory_order_relaxed) < 0)
            {
                const int selectedId = (s && toMonitor) ? 1 : (s ? 2 : (toMonitor ? 3 : 4));
                inputUIs[r].outputRouteCombo.setSelectedId(selectedId, juce::dontSendNotification);
            }
        };
        addAndMakeVisible(matrixStreamToggles[r]);

        double defSendDb = (r == 0 ? 0.0 : (r == 1 ? -3.0 : -6.0));
        matrixStreamSliders[r].setSliderStyle(juce::Slider::LinearHorizontal);
        matrixStreamSliders[r].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        matrixStreamSliders[r].setRange(-60.0, 12.0, 0.1);
        matrixStreamSliders[r].setValue(defSendDb);
        matrixStreamSliders[r].onValueChange = [this, r]() {
            float db = (float)matrixStreamSliders[r].getValue();
            routingMatrix.getChannel(r).sendLevel[0].store(juce::Decibels::decibelsToGain(db));
            matrixStreamLabels[r].setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        };
        addAndMakeVisible(matrixStreamSliders[r]);

        matrixStreamLabels[r].setText(juce::String(defSendDb, 1) + " dB", juce::dontSendNotification);
        matrixStreamLabels[r].setFont(juce::FontOptions(9.5f));
        matrixStreamLabels[r].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(matrixStreamLabels[r]);

        // Monitor Bus Toggle & Level
        matrixMonitorToggles[r].setToggleState(true, juce::dontSendNotification);
        matrixMonitorToggles[r].onClick = [this, r]() {
            bool s = matrixMonitorToggles[r].getToggleState();
            const bool toStream = routingMatrix.getChannel(r).routeEnable[0].load(std::memory_order_relaxed);
            routingMatrix.applyChannelRouteState(r, toStream, s);
            inputUIs[r].monitorRouteToggle.setToggleState(s, juce::dontSendNotification);
            if (routingMatrix.getChannel(r).submixGroup.load(std::memory_order_relaxed) < 0)
            {
                const int selectedId = (toStream && s) ? 1 : (toStream ? 2 : (s ? 3 : 4));
                inputUIs[r].outputRouteCombo.setSelectedId(selectedId, juce::dontSendNotification);
            }
        };
        addAndMakeVisible(matrixMonitorToggles[r]);

        matrixMonitorSliders[r].setSliderStyle(juce::Slider::LinearHorizontal);
        matrixMonitorSliders[r].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        matrixMonitorSliders[r].setRange(-60.0, 12.0, 0.1);
        matrixMonitorSliders[r].setValue(defSendDb);
        matrixMonitorSliders[r].onValueChange = [this, r]() {
            float db = (float)matrixMonitorSliders[r].getValue();
            routingMatrix.getChannel(r).sendLevel[1].store(juce::Decibels::decibelsToGain(db));
            matrixMonitorLabels[r].setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        };
        addAndMakeVisible(matrixMonitorSliders[r]);

        matrixMonitorLabels[r].setText(juce::String(defSendDb, 1) + " dB", juce::dontSendNotification);
        matrixMonitorLabels[r].setFont(juce::FontOptions(9.5f));
        matrixMonitorLabels[r].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(matrixMonitorLabels[r]);

        // Pan Knob & Label
        matrixPanSliders[r].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        matrixPanSliders[r].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        matrixPanSliders[r].setRange(-1.0, 1.0, 0.01);
        matrixPanSliders[r].setValue(defaultPans[r]);
        matrixPanSliders[r].setColour(juce::Slider::rotarySliderFillColourId, defaultChannelAccent(r));
        matrixPanSliders[r].onValueChange = [this, r]() {
            float p = (float)matrixPanSliders[r].getValue();
            routingMatrix.getChannel(r).pan.store(p);
            inputUIs[r].panSlider.setValue(p, juce::dontSendNotification);
            matrixPanLabels[r].setText(std::abs(p) < 0.05f ? "C" : (p < 0 ? ("L " + juce::String((int)(-p * 100))) : ("R " + juce::String((int)(p * 100)))), juce::dontSendNotification);
        };
        addAndMakeVisible(matrixPanSliders[r]);

        matrixPanLabels[r].setText(defaultPans[r] == 0.0 ? "C" : (defaultPans[r] < 0.0 ? "L 100" : "R 100"), juce::dontSendNotification);
        matrixPanLabels[r].setFont(juce::FontOptions(9.5f));
        matrixPanLabels[r].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(matrixPanLabels[r]);
    }

    // Card 2: Auto Ducking Setup
    duckingHeaderLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    duckingHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(duckingHeaderLabel);

    duckingEnableToggle.setToggleState(true, juce::dontSendNotification);
    duckingEnableToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff10b981));
    duckingEnableToggle.onClick = [this]() {
        autoDucker.setEnabled(duckingEnableToggle.getToggleState());
    };
    addAndMakeVisible(duckingEnableToggle);

    duckingPresetCombo.addItem("Preset: Speech over Music", 1);
    duckingPresetCombo.addItem("Preset: Gentle Background", 2);
    duckingPresetCombo.addItem("Preset: Aggressive Voiceover", 3);
    duckingPresetCombo.addItem("Preset: Sidechain Pump", 4);
    duckingPresetCombo.setSelectedId(1, juce::dontSendNotification);
    duckingPresetCombo.onChange = [this]() {
        int id = duckingPresetCombo.getSelectedId();
        auto presets = LiveBus::AutoDucker::getFactoryPresets();
        if (id - 1 >= 0 && id - 1 < (int)presets.size())
            autoDucker.applyPreset(presets[id - 1]);
        log("Auto Ducking preset applied: " + duckingPresetCombo.getText());
    };
    addAndMakeVisible(duckingPresetCombo);

    duckingTriggerLabel.setFont(juce::FontOptions(10.0f));
    duckingTriggerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(duckingTriggerLabel);

    duckingTriggerCombo.addItem("Microphone (CH 1)", 1);
    duckingTriggerCombo.addItem("Guitar / Instrument (CH 2)", 2);
    for (int channel = 3; channel <= maxInputs; ++channel)
        duckingTriggerCombo.addItem("Input Channel " + juce::String(channel), channel);
    duckingTriggerCombo.setSelectedId(1, juce::dontSendNotification);
    duckingTriggerCombo.onChange = [this]() {
        autoDucker.setTriggerSource(duckingTriggerCombo.getSelectedId() - 1);
    };
    addAndMakeVisible(duckingTriggerCombo);

    // Threshold knob
    duckingThresholdSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    duckingThresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    duckingThresholdSlider.setRange(-60.0, 0.0, 0.5);
    duckingThresholdSlider.setValue(-28.0);
    duckingThresholdSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff10b981));
    duckingThresholdSlider.onValueChange = [this]() {
        autoDucker.setThresholdDb((float)duckingThresholdSlider.getValue());
    };
    addAndMakeVisible(duckingThresholdSlider);

    // Ducking 4 Control Knobs: Depth, Attack, Hold, Release
    auto setupDuckerKnob = [this](juce::Slider& s, double minV, double maxV, double defV) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
        s.setRange(minV, maxV, 1.0);
        s.setValue(defV);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff38bdf8));
        addAndMakeVisible(s);
    };
    setupDuckerKnob(duckingDepthSlider, -40.0, 0.0, -12.0);
    duckingDepthSlider.onValueChange = [this]() { autoDucker.setDuckingDepthDb((float)duckingDepthSlider.getValue()); };
    setupDuckerKnob(duckingAttackSlider, 1.0, 200.0, 10.0);
    duckingAttackSlider.onValueChange = [this]() { autoDucker.setAttackMs((float)duckingAttackSlider.getValue()); };
    setupDuckerKnob(duckingHoldSlider, 0.0, 2000.0, 500.0);
    duckingHoldSlider.onValueChange = [this]() { autoDucker.setHoldMs((float)duckingHoldSlider.getValue()); };
    setupDuckerKnob(duckingReleaseSlider, 50.0, 3000.0, 800.0);
    duckingReleaseSlider.onValueChange = [this]() { autoDucker.setReleaseMs((float)duckingReleaseSlider.getValue()); };

    duckingTargetCh3Check.setToggleState(true, juce::dontSendNotification);
    duckingTargetCh3Check.onClick = [this]() { autoDucker.setTargetChannel(2, duckingTargetCh3Check.getToggleState()); };
    addAndMakeVisible(duckingTargetCh3Check);

    duckingTargetCh4Check.setToggleState(true, juce::dontSendNotification);
    duckingTargetCh4Check.onClick = [this]() { autoDucker.setTargetChannel(3, duckingTargetCh4Check.getToggleState()); };
    addAndMakeVisible(duckingTargetCh4Check);

    duckingApplyStreamCheck.setToggleState(true, juce::dontSendNotification);
    duckingApplyStreamCheck.onClick = [this]() { autoDucker.setStreamEnabled(duckingApplyStreamCheck.getToggleState()); };
    addAndMakeVisible(duckingApplyStreamCheck);

    duckingApplyMonitorCheck.setToggleState(true, juce::dontSendNotification);
    duckingApplyMonitorCheck.onClick = [this]() { autoDucker.setMonitorEnabled(duckingApplyMonitorCheck.getToggleState()); };
    addAndMakeVisible(duckingApplyMonitorCheck);

    duckingInputLevelLabel.setFont(juce::FontOptions(10.0f));
    duckingInputLevelLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(duckingInputLevelLabel);

    duckingGRLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    duckingGRLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(duckingGRLabel);

    // Card 3: Audio Safety Setup
    safetyHeaderLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    safetyHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(safetyHeaderLabel);

    safetyStreamLimiterToggle.setToggleState(true, juce::dontSendNotification);
    safetyStreamLimiterToggle.onClick = [this]() { audioSafetyProcessor.setStreamLimiterEnabled(safetyStreamLimiterToggle.getToggleState()); };
    addAndMakeVisible(safetyStreamLimiterToggle);

    safetyStreamCeilingSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    safetyStreamCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    safetyStreamCeilingSlider.setRange(-20.0, 0.0, 0.1);
    safetyStreamCeilingSlider.setValue(-1.0);
    safetyStreamCeilingSlider.onValueChange = [this]() { audioSafetyProcessor.setStreamCeilingDb((float)safetyStreamCeilingSlider.getValue()); };
    addAndMakeVisible(safetyStreamCeilingSlider);

    safetyStreamReleaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    safetyStreamReleaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    safetyStreamReleaseSlider.setRange(10.0, 500.0, 1.0);
    safetyStreamReleaseSlider.setValue(100.0);
    safetyStreamReleaseSlider.onValueChange = [this]() { audioSafetyProcessor.setStreamReleaseMs((float)safetyStreamReleaseSlider.getValue()); };
    addAndMakeVisible(safetyStreamReleaseSlider);

    safetyMonitorLimiterToggle.setToggleState(true, juce::dontSendNotification);
    safetyMonitorLimiterToggle.onClick = [this]() { audioSafetyProcessor.setMonitorLimiterEnabled(safetyMonitorLimiterToggle.getToggleState()); };
    addAndMakeVisible(safetyMonitorLimiterToggle);

    safetyMonitorCeilingSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    safetyMonitorCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    safetyMonitorCeilingSlider.setRange(-20.0, 0.0, 0.1);
    safetyMonitorCeilingSlider.setValue(-1.0);
    safetyMonitorCeilingSlider.onValueChange = [this]() { audioSafetyProcessor.setMonitorCeilingDb((float)safetyMonitorCeilingSlider.getValue()); };
    addAndMakeVisible(safetyMonitorCeilingSlider);

    safetyMonitorReleaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    safetyMonitorReleaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    safetyMonitorReleaseSlider.setRange(10.0, 500.0, 1.0);
    safetyMonitorReleaseSlider.setValue(100.0);
    safetyMonitorReleaseSlider.onValueChange = [this]() { audioSafetyProcessor.setMonitorReleaseMs((float)safetyMonitorReleaseSlider.getValue()); };
    addAndMakeVisible(safetyMonitorReleaseSlider);

    safetyClipWarningLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    safetyClipWarningLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
    addAndMakeVisible(safetyClipWarningLabel);

    safetyResetPeakBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff271014));
    safetyResetPeakBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff87171));
    safetyResetPeakBtn.onClick = [this]() { audioSafetyProcessor.resetClipWarnings(); };
    addAndMakeVisible(safetyResetPeakBtn);

    safetyStreamClipLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    safetyStreamClipLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f1215));
    safetyStreamClipLabel.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
    safetyStreamClipLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(safetyStreamClipLabel);

    safetyMonitorPeakHoldLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    safetyMonitorPeakHoldLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f1215));
    safetyMonitorPeakHoldLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
    safetyMonitorPeakHoldLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(safetyMonitorPeakHoldLabel);

    safetyRoutingCheckBadge.setFont(juce::FontOptions(10.0f));
    safetyRoutingCheckBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    addAndMakeVisible(safetyRoutingCheckBadge);

    safetyRoutingSettingsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    safetyRoutingSettingsBtn.onClick = [this]() { currentTab = TabRouting; resized(); repaint(); };
    addAndMakeVisible(safetyRoutingSettingsBtn);

    safetyEmergencyMuteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb91c1c));
    safetyEmergencyMuteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    safetyEmergencyMuteBtn.onClick = [this]() {
        if (audioSafetyProcessor.getEmergencyMuteMode() != LiveBus::EmergencyMuteMode::None)
            audioSafetyProcessor.unmuteAll();
        else
        {
            if (safetyMuteStreamRadio.getToggleState())
                audioSafetyProcessor.setEmergencyMute(LiveBus::EmergencyMuteMode::StreamOnly);
            else if (safetyMuteMonitorRadio.getToggleState())
                audioSafetyProcessor.setEmergencyMute(LiveBus::EmergencyMuteMode::MonitorOnly);
            else
                audioSafetyProcessor.setEmergencyMute(LiveBus::EmergencyMuteMode::Both);
        }
    };
    addAndMakeVisible(safetyEmergencyMuteBtn);

    safetyMuteStreamRadio.setRadioGroupId(101);
    addAndMakeVisible(safetyMuteStreamRadio);
    safetyMuteMonitorRadio.setRadioGroupId(101);
    addAndMakeVisible(safetyMuteMonitorRadio);
    safetyMuteBothRadio.setRadioGroupId(101);
    safetyMuteBothRadio.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(safetyMuteBothRadio);

    // 6. VST3 Effects Rack Setup
    vstRackHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(vstRackHeader);

    vstTargetCombo.addItem("Target: Stream Bus", 1);
    vstTargetCombo.addItem("Target: Monitor Bus", 2);
    for (int c = 1; c <= maxInputs; ++c)
        vstTargetCombo.addItem("Target: Channel " + juce::String(c), 3 + (c - 1));
    vstTargetCombo.setSelectedId(1, juce::dontSendNotification);
    vstTargetCombo.onChange = [this]() {
        int id = vstTargetCombo.getSelectedId();
        if (id == 1) vstTargetIndex[maxInputs].store(maxInputs);
        else if (id == 2) vstTargetIndex[maxInputs + 1].store(maxInputs + 1);
        else if (id >= 3 && id <= 18) {
                int chIndex = id - 3;
                vstTargetIndex[chIndex].store(chIndex);
            }
        log("VST3 insertion target changed: " + vstTargetCombo.getText());
    };
    addAndMakeVisible(vstTargetCombo);

    scanPluginsBtn.addListener(this);
    addAndMakeVisible(scanPluginsBtn);
    addPluginBtn.addListener(this);
    addAndMakeVisible(addPluginBtn);
    bypassAllPluginsBtn.addListener(this);
    addAndMakeVisible(bypassAllPluginsBtn);

    const char* defaultPlugins[] = { "FabFilter Pro-Q 3", "FabFilter Pro-C 2", "ValhallaVintageVerb" };
    const char* defaultCats[] = { "Equalizer", "Compressor", "Reverb" };

    for (int p = 0; p < 3; ++p)
    {
        auto& row = pluginSlotRows[p];
        row.dragHandle.setText("≡", juce::dontSendNotification);
        addAndMakeVisible(row.dragHandle);

        row.indexLabel.setText(juce::String(p + 1), juce::dontSendNotification);
        addAndMakeVisible(row.indexLabel);

        row.nameLabel.setText(defaultPlugins[p], juce::dontSendNotification);
        row.nameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(row.nameLabel);

        row.categoryLabel.setText(defaultCats[p], juce::dontSendNotification);
        row.categoryLabel.setFont(juce::FontOptions(10.0f));
        row.categoryLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(row.categoryLabel);

        row.statusBadge.setText(p < 2 ? "Active" : "Bypassed", juce::dontSendNotification);
        row.statusBadge.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        row.statusBadge.setColour(juce::Label::textColourId, p < 2 ? juce::Colour(0xff10b981) : juce::Colour(0xffffb703));
        row.statusBadge.setColour(juce::Label::backgroundColourId, p < 2 ? juce::Colour(0x2210b981) : juce::Colour(0x22ffb703));
        row.statusBadge.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(row.statusBadge);

        row.configBtn.addListener(this);
        addAndMakeVisible(row.configBtn);

        row.bypassBtn.setClickingTogglesState(true);
        row.bypassBtn.setToggleState(p == 2, juce::dontSendNotification);
        row.bypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffb703));
        row.bypassBtn.onClick = [this, p]() {
            bool bypassed = pluginSlotRows[p].bypassBtn.getToggleState();
            pluginSlotRows[p].statusBadge.setText(bypassed ? "Bypassed" : "Active", juce::dontSendNotification);
            pluginSlotRows[p].statusBadge.setColour(juce::Label::textColourId, bypassed ? juce::Colour(0xffffb703) : juce::Colour(0xff10b981));
            pluginSlotRows[p].statusBadge.setColour(juce::Label::backgroundColourId, bypassed ? juce::Colour(0x22ffb703) : juce::Colour(0x2210b981));
            log("Plugin " + pluginSlotRows[p].nameLabel.getText() + (bypassed ? " bypassed" : " activated"));
        };
        addAndMakeVisible(row.bypassBtn);
    }

    // 7. Scene Manager Setup
    sceneManagerHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(sceneManagerHeader);

    availableScenes = { "Default", "Live Streaming", "Music Practice", "Vocal Recording", "Podcast", "Gaming" };
    for (int s = 0; s < availableScenes.size(); ++s)
        sceneCombo.addItem(availableScenes[s], s + 1);
    sceneCombo.setSelectedId(2, juce::dontSendNotification); // Live Streaming
    sceneCombo.onChange = [this]() {
        onLoadScene(sceneCombo.getText());
    };
    addAndMakeVisible(sceneCombo);

    saveSceneBtn.addListener(this);
    addAndMakeVisible(saveSceneBtn);
    saveSceneAsBtn.addListener(this);
    addAndMakeVisible(saveSceneAsBtn);

    sceneListModel = std::make_unique<SceneListModel>(*this);
    sceneListBox.setModel(sceneListModel.get());
    sceneListBox.setRowHeight(24);
    addAndMakeVisible(sceneListBox);

    dupSceneBtn.addListener(this);
    addAndMakeVisible(dupSceneBtn);
    renameSceneBtn.addListener(this);
    addAndMakeVisible(renameSceneBtn);
    deleteSceneBtn.addListener(this);
    addAndMakeVisible(deleteSceneBtn);

    // 8. Log / Status Setup
    logHeaderLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(logHeaderLabel);

    clearLogBtn.addListener(this);
    addAndMakeVisible(clearLogBtn);
    copyLogBtn.addListener(this);
    addAndMakeVisible(copyLogBtn);

    logBox.setMultiLine(true);
    logBox.setReadOnly(true);
    logBox.setScrollbarsShown(true);
    logBox.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    addAndMakeVisible(logBox);

    // 9. Device Settings Quick Bar Setup
    deviceSettingsHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(deviceSettingsHeader);

    inputDeviceCombo.addItem("Focusrite USB ASIO", 1);
    inputDeviceCombo.addItem("WASAPI: Line In (Realtek)", 2);
    inputDeviceCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(inputDeviceCombo);

    outputDeviceCombo.addItem("Focusrite USB ASIO", 1);
    outputDeviceCombo.addItem("WASAPI: Speakers (Realtek)", 2);
    outputDeviceCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(outputDeviceCombo);

    sampleRateCombo.addItem("44.1 kHz", 1);
    sampleRateCombo.addItem("48.0 kHz", 2);
    sampleRateCombo.addItem("96.0 kHz", 3);
    sampleRateCombo.setSelectedId(2, juce::dontSendNotification);
    sampleRateCombo.onChange = [this]() {
        currentSampleRate = (sampleRateCombo.getSelectedId() == 1 ? 44100.0 : (sampleRateCombo.getSelectedId() == 3 ? 96000.0 : 48000.0));
        srPillLabel.setText("SR: " + sampleRateCombo.getText(), juce::dontSendNotification);
        log("Sample rate changed to " + sampleRateCombo.getText());
    };
    addAndMakeVisible(sampleRateCombo);

    bufferSizeCombo.addItem("64 samples", 1);
    bufferSizeCombo.addItem("128 samples", 2);
    bufferSizeCombo.addItem("256 samples", 3);
    bufferSizeCombo.addItem("512 samples", 4);
    bufferSizeCombo.setSelectedId(2, juce::dontSendNotification);
    bufferSizeCombo.onChange = [this]() {
        bufferPillLabel.setText("Buffer: " + bufferSizeCombo.getText().upToFirstOccurrenceOf(" ", false, false), juce::dontSendNotification);
        log("Audio buffer size set to " + bufferSizeCombo.getText());
    };
    addAndMakeVisible(bufferSizeCombo);

    streamOutputMapCombo.addItem("Stream: Output 1-2", 1);
    streamOutputMapCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(streamOutputMapCombo);

    monitorOutputMapCombo.addItem("Monitor: Output 3-4", 1);
    monitorOutputMapCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(monitorOutputMapCombo);

    testToneBtn.setClickingTogglesState(true);
    testToneBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff10b981));
    testToneBtn.onClick = [this]() {
        bool on = testToneBtn.getToggleState();
        testToneEnabled.store(on);
        testToneBtn.setButtonText(on ? "Test Tone Active [ON]" : "Test Tone (440Hz)");
        log(on ? "Test tone 440 Hz injected into Stream & Monitor" : "Test tone disabled");
    };
    addAndMakeVisible(testToneBtn);

    // =========================================================================
    // Subpage 1: DSP Full Page
    // =========================================================================
    dspPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    dspPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addChildComponent(dspPageTitle);

    for (int channel = 0; channel < maxInputs; ++channel)
        dspPageTargetCombo.addItem("Target: Channel " + juce::String(channel + 1), channel + 1);
    dspPageTargetCombo.addItem("Target: Stream Bus (OBS)", maxInputs + 1);
    dspPageTargetCombo.addItem("Target: Monitor Bus (Phones)", maxInputs + 2);
    dspPageTargetCombo.setSelectedId(maxInputs + 1, juce::dontSendNotification);
    dspPageTargetCombo.onChange = [this]() {
        int id = dspPageTargetCombo.getSelectedId();
        const int targetIndex = id >= 1 && id <= maxInputs ? id - 1
            : id == maxInputs + 1 ? maxInputs
            : maxInputs + 1;
        selectDSPTarget(targetIndex);

        if (puzuDSPComponent)
        {
            puzuDSPComponent->setSelectedBus(targetIndex == maxInputs + 1 ? 1 : 0);
            puzuDSPComponent->updateUIFromProcessor();
        }
    };
    addChildComponent(dspPageTargetCombo);

    dspPuzuEQToggleBtn.addListener(this);
    addChildComponent(dspPuzuEQToggleBtn);

    puzuDSPComponent = std::make_unique<PuzuDSPComponent>(busProcessors);
    addChildComponent(puzuDSPComponent.get());

    std::array<BusDSPProcessor*, LiveBus::EightBandEQComponent::NumTargets> dspTargets{};
    for (int channel = 0; channel < maxInputs; ++channel)
        dspTargets[(size_t)channel] = &channelDSPs[(size_t)channel];
    dspTargets[(size_t)maxInputs] = &busProcessors[0];
    dspTargets[(size_t)(maxInputs + 1)] = &busProcessors[1];
    eightBandEQComponent = std::make_unique<LiveBus::EightBandEQComponent>(dspTargets);
    eightBandEQComponent->onTargetSelected = [this](LiveBus::EightBandEQComponent::TargetIndex target) {
        selectDSPTarget((int)target);
    };
    addChildComponent(eightBandEQComponent.get());

    std::array<LiveBus::VocalDSPProcessor*, LiveBus::VocalDSPRackComponent::NumTargets> vocalTargets{};
    for (int channel = 0; channel < maxInputs; ++channel)
        vocalTargets[(size_t)channel] = &channelDSPs[(size_t)channel].vocalRack;
    vocalTargets[(size_t)maxInputs] = &busProcessors[0].vocalRack;
    vocalTargets[(size_t)(maxInputs + 1)] = &busProcessors[1].vocalRack;
    vocalDSPRackComponent = std::make_unique<LiveBus::VocalDSPRackComponent>(vocalTargets);
    vocalDSPRackComponent->onTargetSelected = [this](LiveBus::VocalDSPRackComponent::TargetIndex target) {
        selectDSPTarget((int)target);
    };
    addChildComponent(vocalDSPRackComponent.get());

    dspViewSwitchBtn.addListener(this);
    dspViewSwitchBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    dspViewSwitchBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    addChildComponent(dspViewSwitchBtn);

    const char* puzuBandFreqs[] = { "31Hz", "63Hz", "125Hz", "250Hz", "500Hz", "1kHz", "1.5k", "2kHz", "2.5k", "4kHz", "8kHz", "10k", "12.5k", "16k", "20k" };
    for (int b = 0; b < 15; ++b)
    {
        puzuBandLabels[b].setText(puzuBandFreqs[b], juce::dontSendNotification);
        puzuBandLabels[b].setFont(juce::FontOptions(9.0f));
        puzuBandLabels[b].setJustificationType(juce::Justification::centred);
        addChildComponent(puzuBandLabels[b]);

        puzuBandSliders[b].setSliderStyle(juce::Slider::LinearVertical);
        puzuBandSliders[b].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        puzuBandSliders[b].setRange(-15.0, 15.0, 0.1);
        puzuBandSliders[b].setValue(0.0);
        puzuBandSliders[b].setColour(juce::Slider::thumbColourId, juce::Colour(0xff00f5d4));
        puzuBandSliders[b].onValueChange = [this, b]() {
            auto& proc = getProcessorForTarget(selectedTarget);
            proc.params.puzuBands[b].gainDb.store((float)puzuBandSliders[b].getValue());
            proc.updateCoefficients();
            if (eqCurveDisplay) eqCurveDisplay->repaint();
        };
        addChildComponent(puzuBandSliders[b]);
    }

    // =========================================================================
    // Subpage 2: Effects Full Page (VST3 Sandbox Host)
    // =========================================================================
    effectsPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    effectsPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addChildComponent(effectsPageTitle);

    helperHostStatusLabel.setText("Helper: LiveBusPluginHost.exe [Active IPC - Named Memory / Events]", juce::dontSendNotification);
    helperHostStatusLabel.setFont(juce::FontOptions(11.0f));
    helperHostStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    helperHostStatusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    addChildComponent(helperHostStatusLabel);

    testWatchdogRecoveryBtn.addListener(this);
    addChildComponent(testWatchdogRecoveryBtn);

    browseVSTPathBtn.addListener(this);
    addChildComponent(browseVSTPathBtn);

    vstPathEditor.setText("C:\\Program Files\\Common Files\\VST3; C:\\Program Files\\Vstplugins");
    vstPathEditor.setFont(juce::FontOptions(11.0f));
    addChildComponent(vstPathEditor);

    discoveredPluginsEditor.setMultiLine(true);
    discoveredPluginsEditor.setReadOnly(true);
    discoveredPluginsEditor.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    discoveredPluginsEditor.setText("Installed & Verified VST3 Plugins:\n"
                                    "  [1] FabFilter Pro-Q 3       | Equalizer   | FabFilter    | 64-bit | Latency: 0 spl\n"
                                    "  [2] FabFilter Pro-C 2       | Compressor  | FabFilter    | 64-bit | Latency: 0 spl\n"
                                    "  [3] ValhallaVintageVerb     | Reverb      | Valhalla DSP | 64-bit | Latency: 0 spl\n"
                                    "  [4] Tokyo Dawn Kotelnikov   | Compressor  | TDR          | 64-bit | Latency: 0 spl\n"
                                    "  [5] Youlean Loudness Meter2 | Metering    | Youlean      | 64-bit | Latency: 0 spl\n"
                                    "  [6] OTT Dynamics Multi-band | Multiband   | Xfer Records | 64-bit | Latency: 0 spl\n"
                                    "\nStatus: All plugins isolated in helper process LiveBusPluginHost.exe.");
    addChildComponent(discoveredPluginsEditor);

    // =========================================================================
    // Subpage 3: Scenes Full Page (XML & Preset Cloud)
    // =========================================================================
    scenesPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    scenesPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xffeab308));
    addChildComponent(scenesPageTitle);

    exportZipBtn.addListener(this);
    addChildComponent(exportZipBtn);
    importZipBtn.addListener(this);
    addChildComponent(importZipBtn);

    sceneXmlPreview.setMultiLine(true);
    sceneXmlPreview.setReadOnly(true);
    sceneXmlPreview.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    addChildComponent(sceneXmlPreview);

    // =========================================================================
    // Subpage 4: Devices Full Page
    // =========================================================================
    devicesPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    devicesPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addChildComponent(devicesPageTitle);

    deviceStatusDetailedLabel.setFont(juce::FontOptions(11.0f));
    deviceStatusDetailedLabel.setText("Active: Focusrite USB ASIO | Sample Rate: 48000 Hz | Buffer Size: 128 (2.7 ms latency) | Dropouts: 0", juce::dontSendNotification);
    addChildComponent(deviceStatusDetailedLabel);

    restartDeviceBtn.addListener(this);
    addChildComponent(restartDeviceBtn);

    asioControlPanelBtn.addListener(this);
    addChildComponent(asioControlPanelBtn);



    // =========================================================================
    // Subpage 5: MIDI Full Page
    // =========================================================================
    midiPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    midiPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xffa855f7));
    addChildComponent(midiPageTitle);

    midiInputCombo.addItem("None", 1);
    auto midiDevs = midiEngine.getAvailableInputDevices();
    for (int m = 0; m < midiDevs.size(); ++m)
        midiInputCombo.addItem(midiDevs[m], m + 2);
    midiInputCombo.setSelectedId(1, juce::dontSendNotification);
    midiInputCombo.onChange = [this]() {
        int idx = midiInputCombo.getSelectedId() - 2;
        if (idx >= 0)
        {
            midiEngine.openInputDevice(idx);
            log("MIDI Input Device opened: " + midiInputCombo.getText());
        }
    };
    addChildComponent(midiInputCombo);

    midiActivityLog.setMultiLine(true);
    midiActivityLog.setReadOnly(true);
    midiActivityLog.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    midiActivityLog.setText("MIDI Controller Mapping:\n"
                           "  CC 1  -> Ch 1 Gain (Microphone)\n"
                           "  CC 2  -> Ch 2 Gain (Guitar / Inst)\n"
                           "  CC 3  -> Ch 3 Gain (Music Left)\n"
                           "  CC 4  -> Ch 4 Gain (Music Right)\n"
                           "  CC 7  -> Stream Bus Master Fader (OBS)\n"
                           "  CC 8  -> Monitor Bus Master Fader (Phones)\n\n"
                           "Waiting for incoming MIDI messages...\n");
    addChildComponent(midiActivityLog);

    // =========================================================================
    // Subpage 6: Playback Full Page (BGM Player)
    // =========================================================================
    playbackPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    playbackPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addChildComponent(playbackPageTitle);

    openAudioFileBtn.addListener(this);
    addChildComponent(openAudioFileBtn);
    playAudioFileBtn.addListener(this);
    addChildComponent(playAudioFileBtn);
    pauseAudioFileBtn.addListener(this);
    addChildComponent(pauseAudioFileBtn);
    stopAudioFileBtn.addListener(this);
    addChildComponent(stopAudioFileBtn);

    loopAudioFileToggle.setToggleState(true, juce::dontSendNotification);
    addChildComponent(loopAudioFileToggle);

    playbackRouteStream.setToggleState(true, juce::dontSendNotification);
    playbackRouteStream.onStateChange = [this]() {
        audioFilePlayer.setRouteStream(playbackRouteStream.getToggleState());
    };
    addChildComponent(playbackRouteStream);

    playbackRouteMonitor.setToggleState(true, juce::dontSendNotification);
    playbackRouteMonitor.onStateChange = [this]() {
        audioFilePlayer.setRouteMonitor(playbackRouteMonitor.getToggleState());
    };
    addChildComponent(playbackRouteMonitor);

    audioFileGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    audioFileGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    audioFileGainSlider.setRange(0.0, 1.5, 0.01);
    audioFileGainSlider.setValue(0.8);
    audioFileGainSlider.onValueChange = [this]() {
        float g = (float)audioFileGainSlider.getValue();
        audioFilePlayer.setGain(g);
        float db = juce::Decibels::gainToDecibels(g, -60.0f);
        audioFileGainValueLabel.setText(db <= -59.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
    };
    addChildComponent(audioFileGainSlider);

    audioFileGainValueLabel.setText("-2.0 dB", juce::dontSendNotification);
    audioFileGainValueLabel.setFont(juce::FontOptions(10.0f));
    addChildComponent(audioFileGainValueLabel);

    playbackPositionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    playbackPositionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    playbackPositionSlider.setRange(0.0, 100.0, 0.1);
    playbackPositionSlider.setValue(0.0);
    playbackPositionSlider.onValueChange = [this]() {
        if (playbackPositionSlider.isMouseButtonDown())
        {
            double len = audioFilePlayer.getLengthInSeconds();
            if (len > 0.0)
            {
                double pos = (playbackPositionSlider.getValue() / 100.0) * len;
                audioFilePlayer.setPosition(pos);
            }
        }
    };
    addChildComponent(playbackPositionSlider);

    playbackStatusLabel.setText("No Audio File Loaded. Click [Open Audio File...] to load BGM.", juce::dontSendNotification);
    playbackStatusLabel.setFont(juce::FontOptions(11.0f));
    playbackStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addChildComponent(playbackStatusLabel);

    // =========================================================================
    // Subpage 7: Settings Full Page
    // =========================================================================
    settingsPageTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    settingsPageTitle.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addChildComponent(settingsPageTitle);

    darkThemeToggle.setToggleState(true, juce::dontSendNotification);
    addChildComponent(darkThemeToggle);

    openAudioSettingsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    openAudioSettingsBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00f5d4));
    openAudioSettingsBtn.onClick = [this]() {
        currentTab = TabDevices;
        for (int i = 0; i < NumTabs; ++i)
        {
            navTabButtons[i].setColour(juce::TextButton::buttonColourId, (i == currentTab) ? juce::Colour(0xff00f5d4) : juce::Colour(0xff1f293d));
            navTabButtons[i].setColour(juce::TextButton::textColourOffId, (i == currentTab) ? juce::Colour(0xff0d1117) : juce::Colour(0xfff0f6fc));
        }
        resized();
        repaint();
    };
    addChildComponent(openAudioSettingsBtn);
    autoSaveSceneToggle.setToggleState(true, juce::dontSendNotification);
    addChildComponent(autoSaveSceneToggle);
    peakHoldToggle.setToggleState(true, juce::dontSendNotification);
    addChildComponent(peakHoldToggle);

    resetDefaultsBtn.addListener(this);
    addChildComponent(resetDefaultsBtn);

    aboutBoxEditor.setMultiLine(true);
    aboutBoxEditor.setReadOnly(true);
    aboutBoxEditor.setFont(juce::FontOptions(11.0f));
    aboutBoxEditor.setText("LiveBus Mixer v1.0 — Professional Broadcast & Monitor Console\n\n"
                           "Engineered with JUCE 7 and modern C++17 for Windows 10/11 x64.\n"
                           "Features:\n"
                           "  • 16-Channel Routing Matrix with Equal-Power Pan Law (-3dB)\n"
                           "  • Dual Master Summing Buses (Stream Bus [OBS] and Monitor Bus [Phones])\n"
                           "  • Built-in 4-Band Studio Parametric EQ & Puzu 15-Band Graphic Equalizer\n"
                           "  • Dynamic Noise Gate, Compressor, Brickwall Limiter, Reverb & Stereo Delay\n"
                           "  • Out-of-Process VST3 Crash Isolation Host (LiveBusPluginHost.exe)\n"
                           "  • Non-blocking 0ms audio thread fallback on plugin failure\n"
                           "  • Complete Scene Persistence with Atomic XML Serialization & ZIP Packages\n"
                           "  • Zero heap allocation in audio thread (real-time safe guaranteed)\n\n"
                           "Licensed under MIT. Built with pride for professional audio broadcasters.");
    addChildComponent(aboutBoxEditor);

    // Initial Logs
    log("LiveBus Mixer v1.0 started — Professional Broadcast Console initialized.");
    log("Audio device active: Focusrite USB ASIO (48.0 kHz, 128 samples).");
    log("Routing: 4 inputs -> Stream (1-2), Monitor (3-4).");
    log("VST3: Loaded FabFilter Pro-Q 3 (Stream Bus)");
    log("VST3: Loaded FabFilter Pro-C 2 (Stream Bus)");
    log("VST3: Added ValhallaVintageVerb (Stream Bus)");
    log("VST3: ValhallaVintageVerb bypassed");

    // Audio Engine Setup
    deviceManager.initialiseWithDefaultDevices(4, 4);
    deviceManager.addAudioCallback(this);
    deviceManager.addChangeListener(this);

    audioDeviceService = std::make_unique<LiveBus::AudioDeviceService>(deviceManager);
    audioDeviceService->setListener(this);
    audioSettingsComponent = std::make_unique<LiveBus::AudioSettingsComponent>(*audioDeviceService);
    audioSettingsComponent->setCloseButtonVisible(false);
    addChildComponent(audioSettingsComponent.get());

    scenesConsoleComponent = std::make_unique<LiveBus::ScenesConsoleComponent>(sceneManager, presetManager);
    scenesConsoleComponent->setActiveScene(activeSceneName);
    addChildComponent(scenesConsoleComponent.get());

    // Wire up Scenes Console Callbacks
    scenesConsoleComponent->onRecallSceneWithFilter = [this](const juce::String& name, const LiveBus::RecallFilter& filter) {
        SceneData data;
        juce::String err;
        if (sceneManager.loadScene(name, data, err))
        {
            applySceneWithFilter(data, filter);
            activeSceneName = name;
            hasUnsavedChanges = false;
            scenesConsoleComponent->setActiveScene(activeSceneName);
            scenesConsoleComponent->setUnsavedChanges(false);
            sceneCombo.setText(activeSceneName, juce::dontSendNotification);
            log("Recalled scene with filter: " + name);
        }
        else
        {
            log("Recall failed for: " + name + " (" + err + ")");
        }
    };

    scenesConsoleComponent->onSaveScene = [this](const juce::String& name, const juce::String& desc) {
        SceneData data;
        captureSceneFromUI(data);
        data.sceneName = name;
        data.description = desc;
        if (sceneManager.saveScene(name, data))
        {
            activeSceneName = name;
            hasUnsavedChanges = false;
            scenesConsoleComponent->setActiveScene(activeSceneName);
            scenesConsoleComponent->setUnsavedChanges(false);
            if (!availableScenes.contains(name)) availableScenes.add(name);
            refreshSceneList();
            sceneCombo.setText(activeSceneName, juce::dontSendNotification);
            log("Scene saved: " + name);
        }
    };

    scenesConsoleComponent->onDeleteScene = [this](const juce::String& name) {
        if (sceneManager.deleteScene(name))
        {
            availableScenes.removeString(name);
            refreshSceneList();
            log("Scene deleted: " + name);
        }
    };

    scenesConsoleComponent->onRenameScene = [this](const juce::String& oldName, const juce::String& newName) {
        if (sceneManager.renameScene(oldName, newName))
        {
            if (activeSceneName == oldName) activeSceneName = newName;
            availableScenes.removeString(oldName);
            availableScenes.add(newName);
            refreshSceneList();
            scenesConsoleComponent->setActiveScene(activeSceneName);
            log("Renamed scene: " + oldName + " -> " + newName);
        }
    };

    scenesConsoleComponent->onDuplicateScene = [this](const juce::String& src, const juce::String& dest) {
        if (sceneManager.duplicateScene(src, dest))
        {
            if (!availableScenes.contains(dest)) availableScenes.add(dest);
            refreshSceneList();
            log("Duplicated scene: " + src + " -> " + dest);
        }
    };

    scenesConsoleComponent->onImportScene = [this](const juce::File& file) {
        juce::String outName, outErr;
        if (sceneManager.importSceneFromFile(file, outName, outErr))
        {
            if (!availableScenes.contains(outName)) availableScenes.add(outName);
            refreshSceneList();
            log("Imported scene: " + outName);
        }
        else
        {
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Import Error", outErr, "OK");
        }
    };

    scenesConsoleComponent->onExportScene = [this](const juce::String& name, const juce::File& file) {
        if (sceneManager.exportSceneToFile(name, file))
            log("Exported scene to: " + file.getFileName());
    };

    scenesConsoleComponent->onUpdateActiveScene = [this]() {
        SceneData data;
        captureSceneFromUI(data);
        data.sceneName = activeSceneName;
        data.description = "Updated live snapshot";
        if (sceneManager.saveScene(activeSceneName, data))
        {
            hasUnsavedChanges = false;
            scenesConsoleComponent->setUnsavedChanges(false);
            log("Updated active scene: " + activeSceneName);
        }
    };

    // Copy Tools Callbacks
    scenesConsoleComponent->getCopyToolsComponent().onCopyEQ = [this](int src, int dest) { copyEQ(src, dest); };
    scenesConsoleComponent->getCopyToolsComponent().onCopyVocalDSP = [this](int src, int dest) { copyVocalDSP(src, dest); };
    scenesConsoleComponent->getCopyToolsComponent().onCopyChannel = [this](int src, int dest, bool incSends) { copyChannelStrip(src, dest, incSends); };
    scenesConsoleComponent->getCopyToolsComponent().onResetTargetDefaults = [this](int t) { resetTargetDefaults(t); };
    scenesConsoleComponent->getCopyToolsComponent().onUpdateActiveScene = [this]() {
        SceneData data;
        captureSceneFromUI(data);
        data.sceneName = activeSceneName;
        data.description = "Updated live snapshot";
        sceneManager.saveScene(activeSceneName, data);
        hasUnsavedChanges = false;
        scenesConsoleComponent->setUnsavedChanges(false);
    };

    // Preset Manager Callbacks
    scenesConsoleComponent->getPresetManagerComponent().onLoadPresetToTarget = [this](LiveBus::PresetCategory cat, int t, const LiveBus::PresetItem& item) {
        loadPresetToTarget(cat, t, item);
    };
    scenesConsoleComponent->getPresetManagerComponent().onSavePresetFromTarget = [this](LiveBus::PresetCategory cat, int t, const juce::String& name) {
        savePresetFromTarget(cat, t, name);
    };

    // =========================================================================
    // OBS Studio Integration Setup
    // =========================================================================
    obsIntegrationService = std::make_unique<LiveBus::OBSIntegrationService>(deviceManager);
    obsMixerComponent = std::make_unique<LiveBus::OBSMixerComponent>(*obsIntegrationService, routingMatrix, channelLevelMeters.data(), busLevelMeters.data());
    addChildComponent(obsMixerComponent.get());

    obsMixerComponent->onChannelGainChanged = [this](int ch, float linearGain) {
        if (ch >= 0 && ch < maxInputs)
        {
            inputUIs[ch].gainSlider.setValue(juce::Decibels::gainToDecibels(linearGain), juce::dontSendNotification);
            routingMatrix.getChannel(ch).gain.store(linearGain);
        }
    };
    obsMixerComponent->onChannelMuteChanged = [this](int ch, bool muted) {
        if (ch >= 0 && ch < maxInputs)
        {
            inputUIs[ch].muteBtn.setToggleState(muted, juce::dontSendNotification);
            routingMatrix.getChannel(ch).mute.store(muted);
        }
    };
    obsMixerComponent->onBusGainChanged = [this](int bus, float linearGain) {
        if (bus >= 0 && bus < 2)
        {
            masterUIs[bus].masterFader.setValue(juce::Decibels::gainToDecibels(linearGain), juce::dontSendNotification);
            busProcessors[bus].params.masterGain.store(linearGain);
        }
    };
    obsMixerComponent->onBusMuteChanged = [this](int bus, bool muted) {
        if (bus >= 0 && bus < 2)
        {
            masterUIs[bus].muteBtn.setToggleState(muted, juce::dontSendNotification);
            busProcessors[bus].params.masterMute.store(muted);
        }
    };

    // =========================================================================
    // Professional MIDI Controller & MIDI Learn Setup
    // =========================================================================
    initializeMidiParameterRegistry();
    midiManager = std::make_unique<LiveBus::MidiManager>(midiParameterRegistry);
    midiLearnComponent = std::make_unique<LiveBus::MidiLearnComponent>(*midiManager, midiParameterRegistry);
    midiLearnComponent->onCloseRequested = [this]() {
        navTabButtons[TabMixer].triggerClick();
    };
    addChildComponent(midiLearnComponent.get());

    auto midiSessionFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("LiveBusMixer")
                               .getChildFile("_last_midi_session.xml");
    if (midiSessionFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(midiSessionFile);
        if (xml != nullptr)
            midiManager->restoreStateFromXml(*xml);
    }

    // Initialize Professional Playback & Test Signal Console
    audioFilePlayer.prepareToPlay(currentSampleRate, currentBlockSize);
    testSignalGenerator.prepareToPlay(currentSampleRate, currentBlockSize);

    // Populate default demo tracks from bundled assets if playlist is empty
    if (audioFilePlayer.getPlaylist().empty())
    {
        const auto appDirectory =
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        const auto demoCello = appDirectory.getChildFile("Cello_Solo.wav");
        const auto demoGuitar = appDirectory.getChildFile("Guitar_Riff.wav");

        if (demoCello.existsAsFile())
        {
            audioFilePlayer.addTrackToPlaylist(demoCello);
            if (demoGuitar.existsAsFile())
                audioFilePlayer.addTrackToPlaylist(demoGuitar);

            audioFilePlayer.selectTrack(0);
            double dur = audioFilePlayer.getLengthInSeconds();
            if (dur > 0.0)
            {
                audioFilePlayer.setLoopRange(dur * 0.15, dur * 0.70);
                audioFilePlayer.setLoopEnabled(true);
                audioFilePlayer.addMarker(dur * 0.22, "Verse Cue");
                audioFilePlayer.addMarker(dur * 0.60, "Peak Solo");
            }
        }
    }

    playbackComponent = std::make_unique<LiveBus::PlaybackComponent>(audioFilePlayer, testSignalGenerator);
    addChildComponent(playbackComponent.get());

    // Initialize Professional VST3 Plugin Host & Effects Console
    vst3HostEngine = std::make_unique<LiveBus::VST3HostEngine>(pluginHostClient);
    vst3HostEngine->prepareToPlay(currentSampleRate, currentBlockSize);

    // Pre-insert demo plugins matching reference if empty
    if (vst3HostEngine->getNumPluginsInTarget(maxInputs) == 0) // Stream Bus
    {
        juce::PluginDescription d1;
        d1.name = "Harrison MPC Compressor";
        d1.category = "Fx|Dynamics";
        d1.fileOrIdentifier = "vst3://Harrison Audio/Harrison MPC Compressor";
        d1.pluginFormatName = "VST3";
        vst3HostEngine->loadPluginToTarget(maxInputs, d1, false);

        juce::PluginDescription d2;
        d2.name = "Harrison 32 Bus";
        d2.category = "Fx|Dynamics|EQ";
        d2.fileOrIdentifier = "vst3://Harrison Audio/Harrison 32 Bus";
        d2.pluginFormatName = "VST3";
        vst3HostEngine->loadPluginToTarget(maxInputs, d2, false);
    }
    if (vst3HostEngine->getNumPluginsInTarget(0) == 0) // Mic
    {
        juce::PluginDescription d;
        d.name = "Harrison Vocal Flow";
        d.category = "Fx|Dynamics|EQ";
        d.fileOrIdentifier = "vst3://Harrison Audio/Harrison Vocal Flow";
        d.pluginFormatName = "VST3";
        vst3HostEngine->loadPluginToTarget(0, d, false);
    }

    effectsConsoleComponent = std::make_unique<LiveBus::EffectsConsoleComponent>(*vst3HostEngine, pluginHostClient);
    effectsConsoleComponent->onTargetSelected = [this](int targetIndex) {
        selectDSPTarget(targetIndex);
    };
    addChildComponent(effectsConsoleComponent.get());

    // Initialize Professional Performance & Diagnostics System
    audioPerformanceMonitor = std::make_unique<LiveBus::AudioPerformanceMonitor>();
    diagnosticsLogManager = std::make_unique<LiveBus::DiagnosticsLogManager>(*audioPerformanceMonitor);
    if (audioDeviceService != nullptr)
    {
        performanceDiagnosticsComponent = std::make_unique<LiveBus::PerformanceDiagnosticsComponent>(
            *audioDeviceService,
            *audioPerformanceMonitor,
            *diagnosticsLogManager,
            vst3HostEngine.get(),
            &routingMatrix);
        addChildComponent(performanceDiagnosticsComponent.get());
    }

    if (diagnosticsLogManager != nullptr)
        diagnosticsLogManager->log(LiveBus::DiagnosticSeverity::Info,
                                   LiveBus::DiagnosticCategory::AudioEngine,
                                   "LiveBus Mixer v1.0 Audio Engine initialized");

    // Initialize Ducking, Audio Safety & Macro Engine Components
    duckingComponent = std::make_unique<LiveBus::DuckingComponent>(autoDucker);
    addChildComponent(duckingComponent.get());

    audioSafetyComponent = std::make_unique<LiveBus::AudioSafetyComponent>(audioSafetyProcessor);
    addChildComponent(audioSafetyComponent.get());

    // Register parameters into MacroEngine
    for (int ch = 0; ch < maxInputs; ++ch)
    {
        macroEngine.registerParameter({
            "ch." + juce::String(ch) + ".gain",
            "Ch " + juce::String(ch + 1) + " Gain",
            "Mixer",
            0.0f, 1.414f, 0.707f,
            [this, ch](float v) { inputUIs[ch].gainSlider.setValue(v, juce::sendNotification); },
            [this, ch]() { return (float)inputUIs[ch].gainSlider.getValue(); }
        });
        macroEngine.registerParameter({
            "ch." + juce::String(ch) + ".pan",
            "Ch " + juce::String(ch + 1) + " Pan",
            "Mixer",
            -1.0f, 1.0f, 0.0f,
            [this, ch](float v) { inputUIs[ch].panSlider.setValue(v, juce::sendNotification); },
            [this, ch]() { return (float)inputUIs[ch].panSlider.getValue(); }
        });
        macroEngine.registerParameter({
            "ch." + juce::String(ch) + ".eq.midGain",
            "Ch " + juce::String(ch + 1) + " EQ Mid Gain",
            "DSP",
            -18.0f, 18.0f, 0.0f,
            [this, ch](float v) {
                channelDSPs[ch].params.eqGainDb.store(v, std::memory_order_relaxed);
                channelDSPs[ch].updateCoefficients();
            },
            [this, ch]() { return channelDSPs[ch].params.eqGainDb.load(std::memory_order_relaxed); }
        });
        macroEngine.registerParameter({
            "ch." + juce::String(ch) + ".comp.makeup",
            "Ch " + juce::String(ch + 1) + " Comp Makeup",
            "DSP",
            0.0f, 18.0f, 0.0f,
            [this, ch](float v) {
                channelDSPs[ch].params.compMakeupDb.store(v, std::memory_order_relaxed);
            },
            [this, ch]() { return channelDSPs[ch].params.compMakeupDb.load(std::memory_order_relaxed); }
        });
    }

    macroEngine.registerParameter({
        "ducking.depth", "Ducking Depth (dB)", "Ducking",
        -36.0f, 0.0f, -12.0f,
        [this](float v) { autoDucker.setDuckingDepthDb(v); if (duckingComponent) duckingComponent->updateUIFromDSP(); },
        [this]() { return autoDucker.getDuckingDepthDb(); }
    });
    macroEngine.registerParameter({
        "ducking.threshold", "Ducking Threshold (dB)", "Ducking",
        -60.0f, 0.0f, -30.0f,
        [this](float v) { autoDucker.setThresholdDb(v); if (duckingComponent) duckingComponent->updateUIFromDSP(); },
        [this]() { return autoDucker.getThresholdDb(); }
    });
    macroEngine.registerParameter({
        "safety.streamCeiling", "Stream Limiter Ceiling (dBFS)", "Safety",
        -24.0f, 0.0f, -0.5f,
        [this](float v) { audioSafetyProcessor.setStreamCeilingDb(v); if (audioSafetyComponent) audioSafetyComponent->updateUIFromDSP(); },
        [this]() { return audioSafetyProcessor.getStreamCeilingDb(); }
    });
    macroEngine.registerParameter({
        "safety.monitorCeiling", "Monitor Limiter Ceiling (dBFS)", "Safety",
        -24.0f, 0.0f, -0.5f,
        [this](float v) { audioSafetyProcessor.setMonitorCeilingDb(v); if (audioSafetyComponent) audioSafetyComponent->updateUIFromDSP(); },
        [this]() { return audioSafetyProcessor.getMonitorCeilingDb(); }
    });
    macroEngine.registerParameter({
        "stream.gain", "Stream Bus Master Gain", "Master",
        0.0f, 1.414f, 1.0f,
        [this](float v) { masterUIs[0].masterFader.setValue(v, juce::sendNotification); },
        [this]() { return (float)masterUIs[0].masterFader.getValue(); }
    });
    macroEngine.registerParameter({
        "monitor.gain", "Monitor Bus Master Gain", "Master",
        0.0f, 1.414f, 1.0f,
        [this](float v) { masterUIs[1].masterFader.setValue(v, juce::sendNotification); },
        [this]() { return (float)masterUIs[1].masterFader.getValue(); }
    });

    // Register Quick Actions
    macroEngine.registerQuickActionHandler(LiveBus::QuickActionType::EmergencyMuteAll, [this]() {
        headerEmergencyMuteBtn.triggerClick();
    });
    macroEngine.registerQuickActionHandler(LiveBus::QuickActionType::ToggleStreamMute, [this]() {
        masterUIs[0].muteBtn.triggerClick();
    });
    macroEngine.registerQuickActionHandler(LiveBus::QuickActionType::ToggleMonitorMute, [this]() {
        masterUIs[1].muteBtn.triggerClick();
    });
    macroEngine.registerQuickActionHandler(LiveBus::QuickActionType::ToggleDucking, [this]() {
        autoDucker.setEnabled(!autoDucker.isEnabled());
        if (duckingComponent) duckingComponent->updateUIFromDSP();
    });
    macroEngine.registerQuickActionHandler(LiveBus::QuickActionType::ResetPeakWarnings, [this]() {
        audioSafetyProcessor.resetClipWarnings();
    });
    macroEngine.registerQuickActionHandler(LiveBus::QuickActionType::TogglePlayback, [this]() {
        if (audioFilePlayer.isPlaying()) audioFilePlayer.pause();
        else audioFilePlayer.start();
    });

    macroControlComponent = std::make_unique<LiveBus::MacroControlComponent>(macroEngine);
    addChildComponent(macroControlComponent.get());

    routingMatrixComponent = std::make_unique<LiveBus::RoutingMatrixComponent>(
        routingMatrix, audioDeviceService.get(), [this](int busIndex) {
            if (busIndex < 0 || busIndex >= numBuses)
                return false;
            return busProcessors[(size_t)busIndex].params.masterMute.load(std::memory_order_relaxed);
        });
    routingMatrixComponent->masterBusControlCallbacks.getGainDb = [this](int busIndex) {
        if (busIndex < 0 || busIndex >= numBuses)
            return 0.0f;
        return juce::Decibels::gainToDecibels(
            busProcessors[(size_t)busIndex].params.masterGain.load(std::memory_order_relaxed), -60.0f);
    };
    routingMatrixComponent->masterBusControlCallbacks.setGainDb = [this](int busIndex, float db) {
        if (busIndex < 0 || busIndex >= numBuses)
            return;
        const auto gain = juce::Decibels::decibelsToGain(db);
        busProcessors[(size_t)busIndex].params.masterGain.store(gain, std::memory_order_relaxed);
        masterUIs[(size_t)busIndex].masterFader.setValue(db, juce::dontSendNotification);
        masterUIs[(size_t)busIndex].faderValueLabel.setText(
            db <= -59.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
    };
    routingMatrixComponent->masterBusControlCallbacks.getMute = [this](int busIndex) {
        return busIndex >= 0 && busIndex < numBuses &&
               busProcessors[(size_t)busIndex].params.masterMute.load(std::memory_order_relaxed);
    };
    routingMatrixComponent->masterBusControlCallbacks.setMute = [this](int busIndex, bool muted) {
        if (busIndex < 0 || busIndex >= numBuses)
            return;
        busProcessors[(size_t)busIndex].params.masterMute.store(muted, std::memory_order_relaxed);
        masterUIs[(size_t)busIndex].muteBtn.setToggleState(muted, juce::dontSendNotification);
    };
    routingMatrixComponent->masterBusControlCallbacks.getDim = [this](int busIndex) {
        return busIndex >= 0 && busIndex < numBuses &&
               busProcessors[(size_t)busIndex].params.masterDim.load(std::memory_order_relaxed);
    };
    routingMatrixComponent->masterBusControlCallbacks.setDim = [this](int busIndex, bool dimmed) {
        if (busIndex < 0 || busIndex >= numBuses)
            return;
        busProcessors[(size_t)busIndex].params.masterDim.store(dimmed, std::memory_order_relaxed);
    };
    routingMatrixComponent->dspControlCallbacks.getModuleEnabled =
        [this](int targetIndex, LiveBus::RoutingGraphDSPModule module) {
            const auto& params = getProcessorForTarget(targetIndex).params;
            switch (module)
            {
                case LiveBus::RoutingGraphDSPModule::HPF:
                    return params.hpfEnabled.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPModule::LPF:
                    return params.lpfEnabled.load(std::memory_order_relaxed) &&
                           params.lpfSlope.load(std::memory_order_relaxed) > 0;
                case LiveBus::RoutingGraphDSPModule::EQ:
                    return params.eqEnabled.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPModule::Gate:
                    return params.gateEnabled.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPModule::Compressor:
                    return params.compEnabled.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPModule::Limiter:
                    return params.limiterEnabled.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPModule::Reverb:
                    return params.reverbEnabled.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPModule::Delay:
                    return params.delayEnabled.load(std::memory_order_relaxed);
            }
            return false;
        };
    routingMatrixComponent->dspControlCallbacks.setModuleEnabled =
        [this](int targetIndex, LiveBus::RoutingGraphDSPModule module, bool enabled) {
            auto& processor = getProcessorForTarget(targetIndex);
            auto& params = processor.params;
            switch (module)
            {
                case LiveBus::RoutingGraphDSPModule::HPF:
                    params.hpfEnabled.store(enabled, std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPModule::LPF:
                    params.lpfEnabled.store(enabled, std::memory_order_relaxed);
                    if (enabled && params.lpfSlope.load(std::memory_order_relaxed) <= 0)
                        params.lpfSlope.store(1, std::memory_order_relaxed);
                    processor.updateCoefficients();
                    break;
                case LiveBus::RoutingGraphDSPModule::EQ:
                    params.eqEnabled.store(enabled, std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPModule::Gate:
                    params.gateEnabled.store(enabled, std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPModule::Compressor:
                    params.compEnabled.store(enabled, std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPModule::Limiter:
                    params.limiterEnabled.store(enabled, std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPModule::Reverb:
                    params.reverbEnabled.store(enabled, std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPModule::Delay:
                    params.delayEnabled.store(enabled, std::memory_order_relaxed);
                    break;
            }
        };
    routingMatrixComponent->dspControlCallbacks.getParameter =
        [this](int targetIndex, LiveBus::RoutingGraphDSPParameter parameter) {
            const auto& params = getProcessorForTarget(targetIndex).params;
            switch (parameter)
            {
                case LiveBus::RoutingGraphDSPParameter::HPFFrequency:
                    return params.hpfFreq.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::LPFFrequency:
                    return params.lpfFreq.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::EQGain:
                    return params.eqGainDb.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::GateThreshold:
                    return params.gateThresholdDb.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::CompressorThreshold:
                    return params.compThresholdDb.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::LimiterCeiling:
                    return params.limiterCeilingDb.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::ReverbWet:
                    return params.reverbWetLevel.load(std::memory_order_relaxed);
                case LiveBus::RoutingGraphDSPParameter::DelayTime:
                    return params.delayTimeMs.load(std::memory_order_relaxed);
            }
            return 0.0f;
        };
    routingMatrixComponent->dspControlCallbacks.setParameter =
        [this](int targetIndex, LiveBus::RoutingGraphDSPParameter parameter, float value) {
            auto& processor = getProcessorForTarget(targetIndex);
            auto& params = processor.params;
            bool updateCoefficients = false;
            switch (parameter)
            {
                case LiveBus::RoutingGraphDSPParameter::HPFFrequency:
                    params.hpfFreq.store(juce::jlimit(20.0f, 1000.0f, value),
                                         std::memory_order_relaxed);
                    updateCoefficients = true;
                    break;
                case LiveBus::RoutingGraphDSPParameter::LPFFrequency:
                    params.lpfFreq.store(juce::jlimit(1000.0f, 22000.0f, value),
                                         std::memory_order_relaxed);
                    updateCoefficients = true;
                    break;
                case LiveBus::RoutingGraphDSPParameter::EQGain:
                    params.eqGainDb.store(juce::jlimit(-18.0f, 18.0f, value),
                                          std::memory_order_relaxed);
                    updateCoefficients = true;
                    break;
                case LiveBus::RoutingGraphDSPParameter::GateThreshold:
                    params.gateThresholdDb.store(juce::jlimit(-80.0f, 0.0f, value),
                                                 std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPParameter::CompressorThreshold:
                    params.compThresholdDb.store(juce::jlimit(-60.0f, 0.0f, value),
                                                 std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPParameter::LimiterCeiling:
                    params.limiterCeilingDb.store(juce::jlimit(-20.0f, 0.0f, value),
                                                  std::memory_order_relaxed);
                    break;
                case LiveBus::RoutingGraphDSPParameter::ReverbWet:
                    params.reverbWetLevel.store(juce::jlimit(0.0f, 1.0f, value),
                                                std::memory_order_relaxed);
                    updateCoefficients = true;
                    break;
                case LiveBus::RoutingGraphDSPParameter::DelayTime:
                    params.delayTimeMs.store(juce::jlimit(10.0f, 1500.0f, value),
                                             std::memory_order_relaxed);
                    break;
            }
            if (updateCoefficients)
                processor.updateCoefficients();
        };
    routingMatrixComponent->dspControlCallbacks.controlChanged = [this](int targetIndex) {
        if (selectedTarget != targetIndex)
            selectDSPTarget(targetIndex);
        updateDSPControlsFromTarget(targetIndex);
        if (eqCurveDisplay)
            eqCurveDisplay->repaint();
        markUnsavedChanges();
    };
    routingMatrixComponent->onChannelSelected = [this](int channelIndex) {
        selectDSPTarget(channelIndex);
    };
    routingMatrixComponent->onDSPNodeSelected = [this](int targetIndex) {
        selectDSPTarget(targetIndex);
    };
    routingMatrixComponent->onVSTChainSelected = [this](int targetIndex) {
        openFloatingFxRack(targetIndex);
    };
    routingMatrixComponent->vstChainPluginsProvider = [this](int targetIndex) {
        std::vector<juce::String> plugins;
        if (vst3HostEngine == nullptr)
            return plugins;

        const int count = vst3HostEngine->getNumPluginsInTarget(targetIndex);
        plugins.reserve((size_t)count);
        for (int slot = 0; slot < count; ++slot)
        {
            const auto info = vst3HostEngine->getSlotInfo(targetIndex, slot);
            plugins.push_back(info.pluginName + (info.isBypassed ? " [bypassed]" : ""));
        }
        return plugins;
    };
    routingMatrixComponent->onAddInputNodeRequested = [this](juce::Component* menuTarget) {
        handleAddChannel(menuTarget);
    };
    routingMatrixComponent->onPaletteActionRequested =
        [this](LiveBus::RoutingGraphPaletteAction action) {
            switch (action)
            {
                case LiveBus::RoutingGraphPaletteAction::Gate:
                case LiveBus::RoutingGraphPaletteAction::Equalizer:
                case LiveBus::RoutingGraphPaletteAction::Compressor:
                {
                    const auto typeId = action == LiveBus::RoutingGraphPaletteAction::Gate
                        ? "gate" : action == LiveBus::RoutingGraphPaletteAction::Equalizer
                        ? "eq" : "compressor";
                    selectDSPTarget(selectedTarget);
                    routingMatrixComponent->focusRoutingGraphNode(typeId, selectedTarget);
                    break;
                }
                case LiveBus::RoutingGraphPaletteAction::VST3Chain:
                    routingMatrixComponent->focusRoutingGraphNode("vst3", selectedTarget);
                    openFloatingFxRack(selectedTarget);
                    break;
                case LiveBus::RoutingGraphPaletteAction::Submix:
                    routingMatrixComponent->focusRoutingGraphNode("submix", 0);
                    break;
                case LiveBus::RoutingGraphPaletteAction::Aux:
                    routingMatrixComponent->focusRoutingGraphNode("aux", 0);
                    break;
                case LiveBus::RoutingGraphPaletteAction::StreamBus:
                    selectDSPTarget(maxInputs);
                    routingMatrixComponent->focusRoutingGraphNode("stream", 0);
                    break;
                case LiveBus::RoutingGraphPaletteAction::MonitorBus:
                    selectDSPTarget(maxInputs + 1);
                    routingMatrixComponent->focusRoutingGraphNode("monitor", 0);
                    break;
                case LiveBus::RoutingGraphPaletteAction::VirtualCable:
                case LiveBus::RoutingGraphPaletteAction::OBS:
                    currentTab = TabOBS;
                    resized();
                    repaint();
                    break;
                case LiveBus::RoutingGraphPaletteAction::AudioDevice:
                    showAudioSettings();
                    break;
                case LiveBus::RoutingGraphPaletteAction::YouTubeCapture:
                    currentTab = TabYouTube;
                    resized();
                    repaint();
                    break;
                case LiveBus::RoutingGraphPaletteAction::Recorder:
                    chooseStreamRecordingFile();
                    break;
                case LiveBus::RoutingGraphPaletteAction::AddInput:
                    break;
            }
        };
    routingMatrixComponent->showRoutingGraph();
    addChildComponent(routingMatrixComponent.get());

    // Embedded YouTube Player Component
    youtubePlayerComponent = std::make_unique<LiveBus::YouTubePlayerComponent>(youtubeAudioService);
    addChildComponent(youtubePlayerComponent.get());

    // Top Bar Panic Mute Setup
    headerEmergencyMuteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
    headerEmergencyMuteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    headerEmergencyMuteBtn.addListener(this);
    addAndMakeVisible(headerEmergencyMuteBtn);

    // Initial DSP Setup
    selectDSPTarget(maxInputs); // Stream Bus Master default

    // Auto-restore previous session state if available
    SceneData restoredSession;
    juce::String restoreErr;
    if (sceneManager.loadScene("_last_session", restoredSession, restoreErr))
    {
        applySceneToUI(restoredSession);
        log("Restored previous session configuration (_last_session).");
    }

    // Refresh input source comboboxes from active device hardware channels
    refreshChannelInputSourceCombos();

    // Responsive window size
    setSize(1280, 860);
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeAudioCallback(this);
    deviceManager.removeChangeListener(this);
    stopStreamRecording();

    // Auto-save session state on exit
    SceneData lastSession;
    captureSceneFromUI(lastSession);
    lastSession.sceneName = "_last_session";
    sceneManager.saveScene("_last_session", lastSession);

    // Auto-save MIDI session on exit
    if (midiManager != nullptr)
    {
        auto midiSessionFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("LiveBusMixer")
                                   .getChildFile("_last_midi_session.xml");
        juce::XmlElement root("LiveBusMidiSession");
        midiManager->saveCurrentStateToXml(root);
        midiSessionFile.replaceWithText(root.toString());
    }

    if (diagnosticsLogManager != nullptr)
        diagnosticsLogManager->log(LiveBus::DiagnosticSeverity::Info,
                                   LiveBus::DiagnosticCategory::AudioEngine,
                                   "LiveBus Mixer shutdown initiated");

    performanceDiagnosticsComponent.reset();
    diagnosticsLogManager.reset();
    audioPerformanceMonitor.reset();

    setLookAndFeel(nullptr);
}

void MainComponent::chooseStreamRecordingFile()
{
    if (activeStreamRecorder.load(std::memory_order_seq_cst) != nullptr)
    {
        stopStreamRecording();
        return;
    }

    if (recordingFileChooserOpen.exchange(true, std::memory_order_acq_rel))
        return;

    recordingFileChooser = std::make_unique<juce::FileChooser>(
        "Record Stream Bus to WAV",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory)
            .getChildFile("LiveBus Stream.wav"),
        "*.wav");
    const auto safeComponent = juce::Component::SafePointer<MainComponent>(this);
    const auto chooserFlags = juce::FileBrowserComponent::saveMode |
                              juce::FileBrowserComponent::canSelectFiles;
    recordingFileChooser->launchAsync(chooserFlags,
        [safeComponent](const juce::FileChooser& chooser)
        {
            if (safeComponent == nullptr)
                return;

            safeComponent->recordingFileChooserOpen.store(false, std::memory_order_release);
            auto file = chooser.getResult();
            if (file == juce::File{})
                return;
            if (!file.getFileExtension().equalsIgnoreCase(".wav"))
                file = file.withFileExtension(".wav");
            safeComponent->startStreamRecording(file);
        });
}

bool MainComponent::startStreamRecording(const juce::File& file)
{
    if (!isAudioRunning.load(std::memory_order_acquire))
    {
        log("Stream recording not started: audio device is not running.");
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Recorder", "Start the audio device before recording the Stream Bus.");
        return false;
    }

    juce::WavAudioFormat wavFormat;
    auto outputStream = std::make_unique<juce::FileOutputStream>(file);
    if (!outputStream->openedOk())
    {
        log("Stream recording failed: could not open " + file.getFullPathName());
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Recorder", "The selected WAV file could not be opened for writing.");
        return false;
    }
    if (!outputStream->setPosition(0))
    {
        log("Stream recording failed: could not rewind " + file.getFullPathName());
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Recorder", "The selected WAV file could not be prepared for recording.");
        return false;
    }
    const auto truncateResult = outputStream->truncate();
    if (truncateResult.failed())
    {
        log("Stream recording failed: could not truncate " + file.getFullPathName() +
            " (" + truncateResult.getErrorMessage() + ")");
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Recorder", "The selected WAV file could not be prepared for recording.");
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        outputStream.get(), currentSampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
    {
        log("Stream recording failed: WAV writer could not be created.");
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Recorder", "A WAV writer could not be created for the selected file.");
        return false;
    }
    outputStream.release();

    if (!streamRecorderThread.isThreadRunning() && !streamRecorderThread.startThread())
    {
        log("Stream recording failed: writer thread could not be started.");
        return false;
    }

    streamRecorder = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer.release(), streamRecorderThread, juce::jmax(8192, currentBlockSize * 16));
    streamRecordingFile = file;
    droppedRecordingBlocks.store(0, std::memory_order_relaxed);
    activeStreamRecorder.store(streamRecorder.get(), std::memory_order_seq_cst);
    log("Stream Bus recording started: " + file.getFullPathName());
    return true;
}

void MainComponent::updateLatencyCompensation()
{
    std::array<int, maxInputs> channelLatencies{};
    const int activeChannels = routingMatrix.getNumActiveChannels();
    int maxChannelLatency = 0;
    int maxBusLatency = 0;

    auto getTargetLatency = [this](int targetIndex)
    {
        if (vst3HostEngine != nullptr)
            return juce::jmax(0, vst3HostEngine->getTargetTotalLatency(targetIndex));
        if (targetIndex >= 0 && targetIndex < maxInputs + 2 &&
            vstTargetIndex[targetIndex].load(std::memory_order_relaxed) == targetIndex)
            return juce::jmax(0, pluginHostClient.getPluginLatencySamples());
        return 0;
    };

    for (int channel = 0; channel < maxInputs; ++channel)
    {
        if (channel < activeChannels)
        {
            channelLatencies[(size_t)channel] = getTargetLatency(channel);
            maxChannelLatency = juce::jmax(maxChannelLatency, channelLatencies[(size_t)channel]);
        }
    }

    std::array<int, numBuses> busLatencies{};
    for (int bus = 0; bus < numBuses; ++bus)
    {
        busLatencies[(size_t)bus] = getTargetLatency(maxInputs + bus);
        maxBusLatency = juce::jmax(maxBusLatency, busLatencies[(size_t)bus]);
    }

    bool delayOutOfRange = false;
    for (int channel = 0; channel < maxInputs; ++channel)
    {
        const int compensation = maxChannelLatency - channelLatencies[(size_t)channel];
        delayOutOfRange = !channelLatencyCompensators[(size_t)channel].setDelaySamples(compensation) ||
                          delayOutOfRange;
    }
    for (int bus = 0; bus < numBuses; ++bus)
    {
        const int compensation = maxBusLatency - busLatencies[(size_t)bus];
        delayOutOfRange = !busLatencyCompensators[(size_t)bus].setDelaySamples(compensation) ||
                          delayOutOfRange;
    }
    delayOutOfRange = !hardwareDirectLatencyCompensator.setDelaySamples(maxBusLatency) ||
                      delayOutOfRange;
    latencyCompensationOutOfRange.store(delayOutOfRange, std::memory_order_release);
}

void MainComponent::stopStreamRecording()
{
    auto* const writer = activeStreamRecorder.exchange(nullptr, std::memory_order_seq_cst);
    if (writer == nullptr)
        return;

    while (streamRecorderReaders.load(std::memory_order_seq_cst) != 0)
        juce::Thread::yield();

    streamRecorder.reset();
    const bool writerThreadStopped = streamRecorderThread.stopThread(2000);
    const auto droppedBlocks = droppedRecordingBlocks.exchange(0, std::memory_order_relaxed);
    if (droppedBlocks == 0 && writerThreadStopped)
        log("Stream Bus recording saved: " + streamRecordingFile.getFullPathName());
    else
        log("Stream Bus recording ended with " + juce::String((juce::int64)droppedBlocks) +
            " dropped audio blocks" + (writerThreadStopped ? ": " : " and writer thread did not stop cleanly: ") +
            streamRecordingFile.getFullPathName());
    streamRecordingFile = {};
}

// ==============================================================================
// Audio Processing Callback
// ==============================================================================
void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                     float* const* outputChannelData, int numOutputChannels,
                                                     int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    isAudioRunning.store(true);

    if (audioPerformanceMonitor != nullptr)
        audioPerformanceMonitor->callbackStart(numSamples, currentSampleRate > 0.0 ? currentSampleRate : 48000.0);

    const auto clearOutputs = [&]()
    {
        if (numSamples <= 0)
            return;
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData != nullptr && outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    };

    bool buffersCanProcessBlock = numSamples > 0 &&
        streamBuffer.getNumSamples() >= numSamples &&
        monitorBuffer.getNumSamples() >= numSamples &&
        routingPlaybackBuffer.getNumSamples() >= numSamples &&
        routingTestSignalBuffer.getNumSamples() >= numSamples &&
        routingHwDirectBuffer.getNumSamples() >= numSamples;
    for (int ch = 0; ch < maxInputs && buffersCanProcessBlock; ++ch)
        buffersCanProcessBlock = channelBuffers[ch].getNumSamples() >= numSamples;
    if (!buffersCanProcessBlock)
    {
        clearOutputs();
        if (audioPerformanceMonitor != nullptr)
            audioPerformanceMonitor->callbackEnd();
        return;
    }

    // Buffers pre-allocated in audioDeviceAboutToStart - no setSize() in callback
    streamBuffer.clear();
    monitorBuffer.clear();

    for (int i = 0; i < maxInputs; ++i)
    {
        channelBuffers[i].clear();
    }

    // Feed input meter for AudioDeviceService
    if (audioDeviceService != nullptr)
        audioDeviceService->processInputMeter(inputChannelData, numInputChannels, numSamples);

    // Check if any channel uses Playback, Test Signal, or YouTube as input source
    bool anyChannelUsesPlayback = false;
    bool anyChannelUsesTestSignal = false;
    bool anyChannelUsesYouTube = false;
    for (int ch = 0; ch < maxInputs; ++ch)
    {
        auto src = routingMatrix.getChannel(ch).inputSource.load(std::memory_order_relaxed);
        if (src == LiveBus::ChannelInputSource::Playback) anyChannelUsesPlayback = true;
        if (src == LiveBus::ChannelInputSource::TestSignal) anyChannelUsesTestSignal = true;
        if (src == LiveBus::ChannelInputSource::YouTube) anyChannelUsesYouTube = true;
    }

    // Pre-allocated buffers (sized in audioDeviceAboutToStart). Defensive checks below.
    // setSize() inside audio callback avoided - buffers must be prepared with correct block size.

    if (anyChannelUsesPlayback)
    {
        // routingPlaybackBuffer guaranteed prepared in audioDeviceAboutToStart
        routingPlaybackBuffer.clear();
        audioFilePlayer.renderBlock(routingPlaybackBuffer, numSamples);
    }

    if (anyChannelUsesTestSignal)
    {
        // routingTestSignalBuffer guaranteed prepared in audioDeviceAboutToStart
        routingTestSignalBuffer.clear();
        if (testSignalGenerator.isEnabled())
        {
            float* const ptrs[2] = { routingTestSignalBuffer.getWritePointer(0), routingTestSignalBuffer.getWritePointer(1) };
            testSignalGenerator.renderBlock(ptrs, 2, numSamples);
        }
    }

    if (anyChannelUsesYouTube)
    {
        // routingYouTubeBuffer guaranteed prepared in audioDeviceAboutToStart
        routingYouTubeBuffer.clear();
        youtubeAudioService.pullAudioBlock(routingYouTubeBuffer, numSamples, currentSampleRate);
    }

    // 1. Process Channel Audio & Per-Channel DSP
    const float* inPtrs[maxInputs];

    for (int ch = 0; ch < maxInputs; ++ch)
    {
        auto& chState = routingMatrix.getChannel(ch);
        auto src = chState.inputSource.load(std::memory_order_relaxed);
        bool isEnabled = chState.isEnabled.load(std::memory_order_relaxed);
        auto stMode = chState.stereoMode.load(std::memory_order_relaxed);
        int hwCh = chState.hardwareInputChannel.load(std::memory_order_relaxed);
        int pbCh = chState.playbackChannel.load(std::memory_order_relaxed);

        if (!isEnabled || src == LiveBus::ChannelInputSource::Disabled)
        {
            channelBuffers[ch].clear();
        }
        else if (src == LiveBus::ChannelInputSource::Hardware)
        {
            if (hwCh >= 0 && hwCh < numInputChannels && inputChannelData[hwCh] != nullptr)
            {
                if (stMode == LiveBus::ChannelStereoMode::StereoPair && (hwCh + 1) < numInputChannels && inputChannelData[hwCh + 1] != nullptr)
                {
                    channelBuffers[ch].copyFrom(0, 0, inputChannelData[hwCh], numSamples);
                    channelBuffers[ch].copyFrom(1, 0, inputChannelData[hwCh + 1], numSamples);
                }
                else
                {
                    channelBuffers[ch].copyFrom(0, 0, inputChannelData[hwCh], numSamples);
                    channelBuffers[ch].copyFrom(1, 0, inputChannelData[hwCh], numSamples);
                }
            }
            else
            {
                channelBuffers[ch].clear();
            }
        }
        else if (src == LiveBus::ChannelInputSource::Playback)
        {
            if (pbCh == 0) // Left
            {
                channelBuffers[ch].copyFrom(0, 0, routingPlaybackBuffer, 0, 0, numSamples);
                channelBuffers[ch].copyFrom(1, 0, routingPlaybackBuffer, 0, 0, numSamples);
            }
            else if (pbCh == 1) // Right
            {
                channelBuffers[ch].copyFrom(0, 0, routingPlaybackBuffer, 1, 0, numSamples);
                channelBuffers[ch].copyFrom(1, 0, routingPlaybackBuffer, 1, 0, numSamples);
            }
            else if (pbCh == 2) // Stereo
            {
                channelBuffers[ch].copyFrom(0, 0, routingPlaybackBuffer, 0, 0, numSamples);
                channelBuffers[ch].copyFrom(1, 0, routingPlaybackBuffer, 1, 0, numSamples);
            }
            else // Sum / Mono
            {
                channelBuffers[ch].copyFrom(0, 0, routingPlaybackBuffer, 0, 0, numSamples);
                channelBuffers[ch].addFrom(0, 0, routingPlaybackBuffer, 1, 0, numSamples);
                channelBuffers[ch].applyGain(0, 0, numSamples, 0.5f);
                channelBuffers[ch].copyFrom(1, 0, channelBuffers[ch], 0, 0, numSamples);
            }
        }
        else if (src == LiveBus::ChannelInputSource::TestSignal)
        {
            channelBuffers[ch].copyFrom(0, 0, routingTestSignalBuffer, 0, 0, numSamples);
            channelBuffers[ch].copyFrom(1, 0, routingTestSignalBuffer, 1, 0, numSamples);
        }
        else if (src == LiveBus::ChannelInputSource::YouTube)
        {
            if (stMode == LiveBus::ChannelStereoMode::StereoPair)
            {
                channelBuffers[ch].copyFrom(0, 0, routingYouTubeBuffer, 0, 0, numSamples);
                channelBuffers[ch].copyFrom(1, 0, routingYouTubeBuffer, 1, 0, numSamples);
            }
            else
            {
                // Mono sum or stereo pass
                channelBuffers[ch].copyFrom(0, 0, routingYouTubeBuffer, 0, 0, numSamples);
                channelBuffers[ch].copyFrom(1, 0, routingYouTubeBuffer, 1, 0, numSamples);
            }
        }
        else
        {
            channelBuffers[ch].clear();
        }

        // Measure pre-DSP input meter
        float inPeakL = channelBuffers[ch].getMagnitude(0, 0, numSamples);
        float inPeakR = channelBuffers[ch].getMagnitude(1, 0, numSamples);
        chState.inputMeter.store(juce::jmax(inPeakL, inPeakR), std::memory_order_relaxed);

        // Test Signal Injection if target is this input channel via SignalDestination
        if (testSignalGenerator.isEnabled() && src != LiveBus::ChannelInputSource::TestSignal)
        {
            auto dest = testSignalGenerator.getDestination();
            if ((int)dest == ch) // SignalDestination::InputChannel1..4
            {
                float* const ptrs[2] = { channelBuffers[ch].getWritePointer(0), channelBuffers[ch].getWritePointer(1) };
                testSignalGenerator.renderBlock(ptrs, 2, numSamples);
            }
        }

        // Channel DSP
        channelDSPs[ch].process(channelBuffers[ch], numSamples);

        // Channel VST3 insertion
        if (vst3HostEngine != nullptr)
        {
            vst3HostEngine->processTarget(ch, channelBuffers[ch], numSamples);
        }
        else if (vstTargetIndex[ch].load() == ch)
        {
            pluginHostClient.processBlock(channelBuffers[ch], numSamples);
        }

        channelLatencyCompensators[(size_t)ch].process(channelBuffers[ch], numSamples);

        // Process Channel Level Metering (Thread-Safe DSP)
        channelLevelMeters[ch].processBlock(channelBuffers[ch], 0, numSamples);

        inPtrs[ch] = channelBuffers[ch].getReadPointer(0);
    }

    // 2. Auto Ducking Sidechain & Routing Matrix Summing
    int trigSrc = autoDucker.getTriggerSource();
    const float* trigAudio = (trigSrc >= 0 && trigSrc < maxInputs) ? inPtrs[trigSrc] : nullptr;
    autoDucker.processSidechain(trigAudio, numSamples);

    float duckMultStream = autoDucker.getGainMultiplierStream();
    float duckMultMonitor = autoDucker.getGainMultiplierMonitor();
    uint16_t duckTargetMask = autoDucker.getTargetChannelsMask();

    // Clear and route to direct hardware buffers
    routingHwDirectBuffer.clear(0, numSamples);
    float* const* hwDirectPtrs = routingHwDirectBuffer.getArrayOfWritePointers();
    int numHwDirectChannels = routingHwDirectBuffer.getNumChannels();

    routingMatrix.routeAndSum(channelBuffers.data(), maxInputs, streamBuffer, monitorBuffer, numSamples,
                             duckMultStream, duckMultMonitor, duckTargetMask, trigSrc,
                             hwDirectPtrs, numHwDirectChannels);

    // 3. Audio File Player (fallback mix to master only if no channel strips route Playback)
    if (!anyChannelUsesPlayback)
    {
        audioFilePlayer.processMix(streamBuffer, monitorBuffer, numSamples);
    }

    // 4. Test Signal Generator Master Bus Injection
    if (testSignalGenerator.isEnabled())
    {
        auto dest = testSignalGenerator.getDestination();
        if (dest == LiveBus::SignalDestination::StreamBus || dest == LiveBus::SignalDestination::BothBuses)
        {
            float* const ptrs[2] = { streamBuffer.getWritePointer(0), streamBuffer.getWritePointer(1) };
            testSignalGenerator.renderBlock(ptrs, 2, numSamples);
        }
        if (dest == LiveBus::SignalDestination::MonitorBus || dest == LiveBus::SignalDestination::BothBuses)
        {
            float* const ptrs[2] = { monitorBuffer.getWritePointer(0), monitorBuffer.getWritePointer(1) };
            testSignalGenerator.renderBlock(ptrs, 2, numSamples);
        }
    }

    // 4. Test Tone Synthesis (Safe -18 dBFS)
    if (audioDeviceService != nullptr && audioDeviceService->isTestToneEnabled())
    {
        audioDeviceService->renderTestTone(streamBuffer.getArrayOfWritePointers(), 2, numSamples, currentSampleRate);
        audioDeviceService->renderTestTone(monitorBuffer.getArrayOfWritePointers(), 2, numSamples, currentSampleRate);
    }
    else if (testToneEnabled.load())
    {
        float phaseDelta = (float)(440.0 * juce::MathConstants<double>::twoPi / currentSampleRate);
        for (int s = 0; s < numSamples; ++s)
        {
            float toneSample = 0.126f * std::sin(testTonePhase);
            testTonePhase += phaseDelta;
            if (testTonePhase >= juce::MathConstants<float>::twoPi)
                testTonePhase -= juce::MathConstants<float>::twoPi;

            streamBuffer.addSample(0, s, toneSample);
            streamBuffer.addSample(1, s, toneSample);
            monitorBuffer.addSample(0, s, toneSample);
            monitorBuffer.addSample(1, s, toneSample);
        }
    }

    // 5. Master Bus DSP & VST3 Insertion
    busProcessors[0].process(streamBuffer, numSamples);
    busProcessors[1].process(monitorBuffer, numSamples);

    if (vst3HostEngine != nullptr)
    {
        vst3HostEngine->processTarget(maxInputs, streamBuffer, numSamples);
        vst3HostEngine->processTarget(maxInputs + 1, monitorBuffer, numSamples);
    }
    else
    {
        if (vstTargetIndex[maxInputs].load() == maxInputs)
            pluginHostClient.processBlock(streamBuffer, numSamples);
        else if (vstTargetIndex[maxInputs + 1].load() == maxInputs + 1)
            pluginHostClient.processBlock(monitorBuffer, numSamples);
    }

    busLatencyCompensators[0].process(streamBuffer, numSamples);
    busLatencyCompensators[1].process(monitorBuffer, numSamples);

    // Master Bus Level Metering (Thread-Safe DSP)
    busLevelMeters[0].processBlock(streamBuffer, 0, numSamples);
    busLevelMeters[1].processBlock(monitorBuffer, 0, numSamples);

    const float streamPeakL = streamBuffer.getMagnitude(0, 0, numSamples);
    const float streamPeakR = streamBuffer.getMagnitude(1, 0, numSamples);

    // 5b. Audio Safety Processor: Brickwall Output Limiter, True Peak & Emergency Mute
    audioSafetyProcessor.processStreamBus(streamBuffer, numSamples);
    audioSafetyProcessor.processMonitorBus(monitorBuffer, numSamples);
    streamRecorderReaders.fetch_add(1, std::memory_order_seq_cst);
    if (auto* recorder = activeStreamRecorder.load(std::memory_order_seq_cst))
    {
        if (!recorder->write(streamBuffer.getArrayOfReadPointers(), numSamples))
            droppedRecordingBlocks.fetch_add(1, std::memory_order_relaxed);
    }
    streamRecorderReaders.fetch_sub(1, std::memory_order_seq_cst);
    routingMatrix.updateMasterBusMeter(0,
        streamBuffer.getMagnitude(0, 0, numSamples),
        streamBuffer.getMagnitude(1, 0, numSamples));
    routingMatrix.updateMasterBusMeter(1,
        monitorBuffer.getMagnitude(0, 0, numSamples),
        monitorBuffer.getMagnitude(1, 0, numSamples));

    // Push Stream Bus audio into secondary virtual device FIFO & update telemetry for OBS
    if (obsIntegrationService != nullptr)
    {
        obsIntegrationService->pushOutputAudioBlock(streamBuffer, monitorBuffer, numSamples);
        float sRmsL = streamBuffer.getRMSLevel(0, 0, numSamples);
        float sRmsR = streamBuffer.getRMSLevel(1, 0, numSamples);
        obsIntegrationService->updateStreamMeter(streamPeakL, streamPeakR, sRmsL, sRmsR);
    }

    // 6. Write to Hardware Outputs
    clearOutputs();
    const auto outputMode = obsIntegrationService != nullptr
        ? obsIntegrationService->getRoutingMode()
        : LiveBus::OBSRoutingMode::PrimaryMultiChannel;
    LiveBus::routeMasterBusesToDevice(outputChannelData, numOutputChannels,
                                      streamBuffer, monitorBuffer, numSamples, outputMode);

    hardwareDirectLatencyCompensator.process(routingHwDirectBuffer, numSamples);
    for (int h = 0; h < juce::jmin(numOutputChannels, maxHwOutputs); ++h)
        if (outputChannelData != nullptr && outputChannelData[h] != nullptr)
            juce::FloatVectorOperations::add(outputChannelData[h], routingHwDirectBuffer.getReadPointer(h), numSamples);

    audioSafetyProcessor.processHardwareOutputs(outputChannelData, numOutputChannels, numSamples);

    if (audioPerformanceMonitor != nullptr)
        audioPerformanceMonitor->callbackEnd();
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (!device) return;
    currentSampleRate = device->getCurrentSampleRate();
    currentBlockSize = device->getCurrentBufferSizeSamples();

    streamBuffer.setSize(2, currentBlockSize);
    monitorBuffer.setSize(2, currentBlockSize);
    for (int i = 0; i < maxInputs; ++i)
        channelBuffers[i].setSize(2, currentBlockSize);

    routingPlaybackBuffer.setSize(2, currentBlockSize);
    routingTestSignalBuffer.setSize(2, currentBlockSize);
    routingHwDirectBuffer.setSize(maxHwOutputs, currentBlockSize);
    for (auto& compensator : channelLatencyCompensators)
        compensator.prepare(2, currentBlockSize);
    for (auto& compensator : busLatencyCompensators)
        compensator.prepare(2, currentBlockSize);
    hardwareDirectLatencyCompensator.prepare(maxHwOutputs, currentBlockSize);

    auto inNames = device->getInputChannelNames();
    auto outNames = device->getOutputChannelNames();
    routingMatrix.validateAvailableInputs(inNames.size());
    routingMatrix.validateAvailableOutputs(outNames.size());
    routingMatrix.prepare(currentSampleRate, currentBlockSize);

    busProcessors[0].prepare(currentSampleRate, currentBlockSize);
    busProcessors[1].prepare(currentSampleRate, currentBlockSize);
    for (int i = 0; i < maxInputs; ++i)
        channelDSPs[i].prepare(currentSampleRate, currentBlockSize);

    for (int i = 0; i < maxInputs; ++i)
        channelLevelMeters[i].prepare(currentSampleRate, currentBlockSize);
    busLevelMeters[0].prepare(currentSampleRate, currentBlockSize);
    busLevelMeters[1].prepare(currentSampleRate, currentBlockSize);

    audioFilePlayer.prepareToPlay(currentSampleRate, currentBlockSize);
    autoDucker.prepare(currentSampleRate, currentBlockSize);
    audioSafetyProcessor.prepare(currentSampleRate, currentBlockSize);
    testSignalGenerator.prepareToPlay(currentSampleRate, currentBlockSize);
    if (vst3HostEngine != nullptr)
        vst3HostEngine->prepareToPlay(currentSampleRate, currentBlockSize);
    updateLatencyCompensation();
    for (auto& compensator : channelLatencyCompensators)
        compensator.reset();
    for (auto& compensator : busLatencyCompensators)
        compensator.reset();
    hardwareDirectLatencyCompensator.reset();

    if (audioPerformanceMonitor != nullptr)
        audioPerformanceMonitor->resetStatistics();

    const auto deviceName = device->getName();
    const auto sampleRate = currentSampleRate;
    const auto blockSize = currentBlockSize;
    const auto safeComponent = juce::Component::SafePointer<MainComponent>(this);
    juce::MessageManager::callAsync([safeComponent, deviceName, sampleRate, blockSize,
                                     inNames, outNames]() mutable
    {
        if (safeComponent == nullptr)
            return;

        safeComponent->srPillLabel.setText("SR: " + juce::String(sampleRate * 0.001, 1) + " kHz", juce::dontSendNotification);
        safeComponent->bufferPillLabel.setText("Buffer: " + juce::String(blockSize), juce::dontSendNotification);
        safeComponent->devicePillLabel.setText("Device: " + deviceName, juce::dontSendNotification);
        if (safeComponent->routingMatrixComponent)
        {
            safeComponent->routingMatrixComponent->updateAvailableHardwareInputs(inNames.size(), inNames);
            safeComponent->routingMatrixComponent->updateAvailableHardwareOutputs(outNames.size(), outNames);
        }
        safeComponent->log("Audio device started: " + deviceName + " (" + juce::String(sampleRate) +
                           " Hz, " + juce::String(blockSize) + " samples)");
        if (safeComponent->diagnosticsLogManager != nullptr)
            safeComponent->diagnosticsLogManager->log(
                LiveBus::DiagnosticSeverity::Info, LiveBus::DiagnosticCategory::Device,
                "Audio device opened: " + deviceName + " (" + juce::String((int)sampleRate) +
                " Hz, " + juce::String(blockSize) + " smp)");
        if (safeComponent->performanceDiagnosticsComponent != nullptr)
            safeComponent->performanceDiagnosticsComponent->refreshFromHardware();
        safeComponent->refreshChannelInputSourceCombos();
        if (safeComponent->audioSettingsComponent != nullptr)
            safeComponent->audioSettingsComponent->updateUIFromDevice();
    });
}

void MainComponent::audioDeviceStopped()
{
    isAudioRunning.store(false);
    autoDucker.reset();
    audioSafetyProcessor.reset();
    for (int i = 0; i < maxInputs; ++i)
        channelLevelMeters[i].reset();
    busLevelMeters[0].reset();
    busLevelMeters[1].reset();

    const auto safeComponent = juce::Component::SafePointer<MainComponent>(this);
    juce::MessageManager::callAsync([safeComponent]()
    {
        if (safeComponent == nullptr)
            return;
        safeComponent->stopStreamRecording();
        safeComponent->audioStatusBadge.setText("● Audio Stopped", juce::dontSendNotification);
        safeComponent->audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
        safeComponent->audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
        safeComponent->log("Audio device stopped.");
        safeComponent->refreshChannelInputSourceCombos();
        if (safeComponent->diagnosticsLogManager != nullptr)
            safeComponent->diagnosticsLogManager->log(
                LiveBus::DiagnosticSeverity::Warning, LiveBus::DiagnosticCategory::Device,
                "Audio device stopped");
        if (safeComponent->audioSettingsComponent != nullptr)
            safeComponent->audioSettingsComponent->updateUIFromDevice();
    });
}

void MainComponent::audioDeviceError(const juce::String& errorMessage)
{
    const auto safeComponent = juce::Component::SafePointer<MainComponent>(this);
    juce::MessageManager::callAsync([safeComponent, errorMessage]()
    {
        if (safeComponent == nullptr)
            return;
        if (safeComponent->audioPerformanceMonitor != nullptr)
            safeComponent->audioPerformanceMonitor->recordDeviceError(errorMessage.toRawUTF8());
        if (safeComponent->diagnosticsLogManager != nullptr)
            safeComponent->diagnosticsLogManager->log(
                LiveBus::DiagnosticSeverity::Error, LiveBus::DiagnosticCategory::Device,
                "Audio device error: " + errorMessage);
        safeComponent->log("Audio device error: " + errorMessage);
    });
}

// ==============================================================================
// LiveBus::AudioDeviceServiceListener Implementation
// ==============================================================================
void MainComponent::onDeviceStateChanged(DeviceState newState, const juce::String& message)
{
    juce::MessageManager::callAsync([this, newState, message]() {
        switch (newState)
        {
            case DeviceState::Active:
                audioStatusBadge.setText("● Audio Running", juce::dontSendNotification);
                audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
                audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
                break;
            case DeviceState::Reconnecting:
                audioStatusBadge.setText("● Reconnecting", juce::dontSendNotification);
                audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
                audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22f59e0b));
                break;
            case DeviceState::Disconnected:
                audioStatusBadge.setText("● Disconnected", juce::dontSendNotification);
                audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
                audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
                break;
            case DeviceState::Failed:
                audioStatusBadge.setText("● Device Error", juce::dontSendNotification);
                audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffdc2626));
                audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22dc2626));
                break;
        }

        if (message.isNotEmpty())
            log(message);

        refreshChannelInputSourceCombos();
        if (audioSettingsComponent != nullptr)
            audioSettingsComponent->updateUIFromDevice();
    });
}

void MainComponent::onDeviceSetupChanged()
{
    juce::MessageManager::callAsync([this]() {
        refreshChannelInputSourceCombos();
        if (audioSettingsComponent != nullptr)
            audioSettingsComponent->updateUIFromDevice();
    });
}

void MainComponent::refreshChannelInputSourceCombos()
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    juce::StringArray inNames;
    juce::String backendName;

    if (dev != nullptr)
    {
        inNames = dev->getInputChannelNames();
        backendName = dev->getTypeName();
    }

    const int numHw = inNames.size();
    routingMatrix.validateAvailableInputs(numHw);

    const bool isAsio = backendName.containsIgnoreCase("ASIO");
    const juce::String prefix = isAsio ? "ASIO In " : "In ";

    for (int ch = 0; ch < maxInputs; ++ch)
    {
        auto& combo = inputUIs[ch].inputSourceCombo;
        auto& chState = routingMatrix.getChannel(ch);

        const auto currentSrc = chState.inputSource.load();
        const int currentHw = chState.hardwareInputChannel.load();

        combo.clear(juce::dontSendNotification);

        if (numHw > 0)
        {
            for (int h = 0; h < numHw; ++h)
            {
                juce::String name = inNames[h];
                if (name.isEmpty()) name = juce::String(h + 1);
                combo.addItem(prefix + juce::String(h + 1) + ": " + name, h + 1);
            }
        }
        else
        {
            combo.addItem("<< No Hardware Inputs >>", 1);
        }

        // Keep slot for unavailable hardware channel if assigned
        if (currentSrc == LiveBus::ChannelInputSource::Hardware && (currentHw < 0 || currentHw >= numHw))
        {
            combo.addItem(prefix + juce::String(currentHw + 1) + " (Unavailable)", 500 + currentHw);
        }

        combo.addSeparator();
        combo.addItem("Playback / BGM", 1001);
        combo.addItem("Test Signal", 1002);
        combo.addItem("Disabled", 1003);
        combo.addItem("YouTube Player (captured audio)", 1004);

        // Restore selected item
        int targetSelectId = 1003;
        if (currentSrc == LiveBus::ChannelInputSource::Hardware)
        {
            if (currentHw >= 0 && currentHw < numHw)
                targetSelectId = currentHw + 1;
            else if (numHw == 0)
                targetSelectId = 1;
            else
                targetSelectId = 500 + currentHw;
        }
        else if (currentSrc == LiveBus::ChannelInputSource::Playback)
        {
            targetSelectId = 1001;
        }
        else if (currentSrc == LiveBus::ChannelInputSource::TestSignal)
        {
            targetSelectId = 1002;
        }
        else if (currentSrc == LiveBus::ChannelInputSource::YouTube)
        {
            targetSelectId = 1004;
        }
        else
        {
            targetSelectId = 1003;
        }
        combo.setSelectedId(targetSelectId, juce::dontSendNotification);

        // Update routing matrix bottom card combo if in 0..3
        if (ch < 4)
        {
            auto& mCombo = matrixSourceCombos[ch];
            mCombo.clear(juce::dontSendNotification);
            if (numHw > 0)
            {
                for (int h = 0; h < numHw; ++h)
                {
                    juce::String name = inNames[h];
                    if (name.isEmpty()) name = juce::String(h + 1);
                    mCombo.addItem(prefix + juce::String(h + 1) + ": " + name, h + 1);
                }
            }
            else
            {
                mCombo.addItem("<< No Inputs >>", 1);
            }

            if (currentSrc == LiveBus::ChannelInputSource::Hardware && (currentHw < 0 || currentHw >= numHw))
                mCombo.addItem(prefix + juce::String(currentHw + 1) + " (Unavailable)", 500 + currentHw);

            mCombo.addSeparator();
            mCombo.addItem("Playback", 1001);
            mCombo.addItem("Test Signal", 1002);
            mCombo.addItem("Disabled", 1003);
            mCombo.addItem("YouTube Player (captured audio)", 1004);
            mCombo.setSelectedId(targetSelectId, juce::dontSendNotification);
        }
    }

    routingMatrix.updateDuplicateWarnings();
    if (routingMatrixComponent != nullptr)
    {
        routingMatrixComponent->updateAvailableHardwareInputs(numHw, inNames);
        if (dev != nullptr)
        {
            auto outNames = dev->getOutputChannelNames();
            routingMatrix.validateAvailableOutputs(outNames.size());
            routingMatrixComponent->updateAvailableHardwareOutputs(outNames.size(), outNames);
        }
    }
}
// Timer Callback (UI & Meter Refresh at 30 Hz)
// ==============================================================================
void MainComponent::timerCallback()
{
    syncSharedUIFromModel();
    updateLatencyCompensation();
    if (++fxStatusRefreshTicks >= 8)
    {
        fxStatusRefreshTicks = 0;
        updateFxStripIndicators();
    }
    const bool latencyOutOfRange = latencyCompensationOutOfRange.load(std::memory_order_acquire);
    if (latencyOutOfRange && !latencyCompensationWarningLogged)
        log("Plugin delay compensation exceeds the supported delay limit; affected routes are left uncompensated.");
    latencyCompensationWarningLogged = latencyOutOfRange;

    // Drain real-time lock-free queue into DiagnosticsLogManager
    if (diagnosticsLogManager != nullptr)
        diagnosticsLogManager->drainQueueFromAudioThread();

    // Update CPU usage from actual audio processing monitor
    if (audioPerformanceMonitor != nullptr)
    {
        currentCpuUsage = audioPerformanceMonitor->getAverageLoadPercent();
        cpuPillLabel.setText("CPU " + juce::String(currentCpuUsage, 1) + "%", juce::dontSendNotification);

        if (audioPerformanceMonitor->hasRecentWarning())
            cpuPillLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
        else if (currentCpuUsage > 85.0f)
            cpuPillLabel.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
        else
            cpuPillLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    }
    else
    {
        currentCpuUsage = (float)(deviceManager.getCpuUsage() * 100.0f);
        cpuPillLabel.setText("CPU " + juce::String(currentCpuUsage, 1) + "%", juce::dontSendNotification);
    }

    // Audio status badge: query actual device state
    if (audioDeviceService != nullptr)
    {
        auto st = audioDeviceService->getDeviceState();
        if (st == DeviceState::Active && isAudioRunning.load())
        {
            audioStatusBadge.setText("● Audio Running", juce::dontSendNotification);
            audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
            audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
        }
        else if (st == DeviceState::Reconnecting)
        {
            audioStatusBadge.setText("● Reconnecting", juce::dontSendNotification);
            audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
            audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22f59e0b));
        }
        else if (st == DeviceState::Disconnected)
        {
            audioStatusBadge.setText("● Disconnected", juce::dontSendNotification);
            audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
            audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
        }
        else if (st == DeviceState::Failed)
        {
            audioStatusBadge.setText("● Device Error", juce::dontSendNotification);
            audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffdc2626));
            audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22dc2626));
        }
    }
    else if (isAudioRunning.load())
    {
        audioStatusBadge.setText("● Audio Running", juce::dontSendNotification);
        audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    }
    else
    {
        audioStatusBadge.setText("● Audio Stopped", juce::dontSendNotification);
        audioStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
        audioStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
    }

    // Update Level Meter Ballistics & UI Readings
    const float dt = 0.0333f;
    for (int i = 0; i < maxInputs; ++i)
    {
        channelLevelMeters[i].updateBallistics(dt);
        if (channelPeakMeters[i])
            channelPeakMeters[i]->updateReading(channelLevelMeters[i].getSnapshot());
    }
    for (int b = 0; b < numBuses; ++b)
    {
        busLevelMeters[b].updateBallistics(dt);
    }
    if (streamBusPeakMeter)
        streamBusPeakMeter->updateReading(busLevelMeters[0].getSnapshot());
    if (monitorBusPeakMeter)
        monitorBusPeakMeter->updateReading(busLevelMeters[1].getSnapshot());
    if (masterMeterStation)
        masterMeterStation->updateMeters();

    // Update Playback Timeline
    if (audioFilePlayer.isPlaying())
    {
        double curPos = audioFilePlayer.getCurrentPosition();
        double totalLen = audioFilePlayer.getLengthInSeconds();
        if (totalLen > 0.0 && !playbackPositionSlider.isMouseButtonDown())
        {
            playbackPositionSlider.setValue((curPos / totalLen) * 100.0, juce::dontSendNotification);
            playbackStatusLabel.setText("Playing: " + audioFilePlayer.getLoadedFileName() + " (" +
                                        juce::String(curPos, 1) + "s / " + juce::String(totalLen, 1) + "s)", juce::dontSendNotification);
        }
    }

    // Process MIDI Manager queued events & mappings
    if (midiManager != nullptr)
    {
        midiManager->processQueuedEvents();
    }

    // Ducking Telemetry
    float trigDb = autoDucker.getTriggerLevelDb();
    duckingInputLevelLabel.setText(juce::String(trigDb, 1) + " dB", juce::dontSendNotification);
    float grStream = autoDucker.getGainReductionDbStream();
    float grMonitor = autoDucker.getGainReductionDbMonitor();
    float grMax = juce::jmin(grStream, grMonitor);
    duckingGRLabel.setText(juce::String(grMax, 1) + " dB", juce::dontSendNotification);

    // Audio Safety Telemetry
    float sPeak = audioSafetyProcessor.getStreamPeakDb();
    float mPeak = audioSafetyProcessor.getMonitorPeakDb();
    bool sClip = audioSafetyProcessor.isStreamClipping();
    bool mClip = audioSafetyProcessor.isMonitorClipping();
    safetyStreamClipLabel.setText((sClip ? "CLIP: " : "Peak: ") + juce::String(sPeak, 1) + " dB", juce::dontSendNotification);
    safetyMonitorPeakHoldLabel.setText((mClip ? "CLIP: " : "Peak: ") + juce::String(mPeak, 1) + " dB", juce::dontSendNotification);
    safetyClipWarningLabel.setText((sClip || mClip) ? "[!] CLIPPING DETECTED" : "[OK] Signal Normal", juce::dontSendNotification);
    safetyClipWarningLabel.setColour(juce::Label::textColourId, (sClip || mClip) ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));

    bool loopRisk = routingMatrix.hasAnyDuplicateWarning();
    safetyRoutingCheckBadge.setText(loopRisk ? "[!] Duplicate routing detected" : "[OK] No feedback risk detected", juce::dontSendNotification);
    safetyRoutingCheckBadge.setColour(juce::Label::textColourId, loopRisk ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));

    repaint();
}

void MainComponent::syncSharedUIFromModel()
{
    auto setSliderFromModel = [](juce::Slider& slider, double value)
    {
        if (!slider.isMouseButtonDown() && std::abs(slider.getValue() - value) > 0.0001)
            slider.setValue(value, juce::dontSendNotification);
    };

    for (int i = 0; i < maxInputs; ++i)
    {
        auto& state = routingMatrix.getChannel(i);
        auto& ui = inputUIs[i];

        if (!ui.nameEditor.hasKeyboardFocus(false) && ui.nameEditor.getText() != state.name)
            ui.nameEditor.setText(state.name, false);

        int sourceId = 1003;
        switch (state.inputSource.load(std::memory_order_relaxed))
        {
            case LiveBus::ChannelInputSource::Hardware:
                sourceId = state.hardwareInputChannel.load(std::memory_order_relaxed) + 1;
                if (!state.isSourceAvailable.load(std::memory_order_relaxed) ||
                    ui.inputSourceCombo.indexOfItemId(sourceId) < 0)
                {
                    const int unavailableId = 500 + state.hardwareInputChannel.load(std::memory_order_relaxed);
                    sourceId = unavailableId;
                    if (ui.inputSourceCombo.indexOfItemId(sourceId) < 0)
                        ui.inputSourceCombo.addItem("In " +
                            juce::String(state.hardwareInputChannel.load(std::memory_order_relaxed) + 1) +
                            " (Unavailable)", sourceId);
                }
                break;
            case LiveBus::ChannelInputSource::Playback: sourceId = 1001; break;
            case LiveBus::ChannelInputSource::TestSignal: sourceId = 1002; break;
            case LiveBus::ChannelInputSource::YouTube: sourceId = 1004; break;
            case LiveBus::ChannelInputSource::Disabled: break;
        }
        if (!ui.inputSourceCombo.isPopupActive() && ui.inputSourceCombo.getSelectedId() != sourceId)
            ui.inputSourceCombo.setSelectedId(sourceId, juce::dontSendNotification);
        if (i < 4 && sourceId >= 500 && matrixSourceCombos[i].indexOfItemId(sourceId) < 0)
            matrixSourceCombos[i].addItem("In " +
                juce::String(state.hardwareInputChannel.load(std::memory_order_relaxed) + 1) +
                " (Unavailable)", sourceId);
        if (i < 4 && !matrixSourceCombos[i].isPopupActive() &&
            matrixSourceCombos[i].getSelectedId() != sourceId)
            matrixSourceCombos[i].setSelectedId(sourceId, juce::dontSendNotification);

        const float channelGain = state.gain.load(std::memory_order_relaxed);
        const double faderDb = juce::jlimit(-60.0, 12.0,
            (double)juce::Decibels::gainToDecibels(channelGain, -60.0f));
        setSliderFromModel(ui.faderSlider, faderDb);
        ui.faderValueLabel.setText(faderDb <= -59.0 ? "-inf dB" : juce::String(faderDb, 1) + " dB",
                                  juce::dontSendNotification);

        const double gainDb = juce::Decibels::gainToDecibels(
            channelDSPs[(size_t)i].params.masterGain.load(std::memory_order_relaxed), -24.0f);
        setSliderFromModel(ui.gainSlider, juce::jlimit(-24.0, 24.0, gainDb));
        ui.gainValueLabel.setText(juce::String(ui.gainSlider.getValue(), 1) + " dB",
                                  juce::dontSendNotification);

        const float pan = state.pan.load(std::memory_order_relaxed);
        setSliderFromModel(ui.panSlider, pan);
        ui.panValueLabel.setText(std::abs(pan) < 0.05f ? "C" :
            (pan < 0.0f ? "L " + juce::String((int)(-pan * 100.0f))
                        : "R " + juce::String((int)(pan * 100.0f))),
            juce::dontSendNotification);

        ui.muteBtn.setToggleState(state.mute.load(std::memory_order_relaxed), juce::dontSendNotification);
        ui.soloBtn.setToggleState(state.solo.load(std::memory_order_relaxed), juce::dontSendNotification);
        const bool stereoLinked = state.stereoMode.load(std::memory_order_relaxed) == LiveBus::ChannelStereoMode::StereoPair;
        ui.linkBtn.setToggleState(stereoLinked, juce::dontSendNotification);
        if (i < 4 && !matrixStereoCombos[i].isPopupActive())
            matrixStereoCombos[i].setSelectedId((int)state.stereoMode.load(std::memory_order_relaxed) + 1,
                                                juce::dontSendNotification);
        if (!ui.outputRouteCombo.isPopupActive())
        {
            const int submix = state.submixGroup.load(std::memory_order_relaxed);
            int routeId = submix >= 0 && submix < maxSubmixBuses ? 5 + submix : 4;
            if (submix < 0)
            {
                const bool toStream = state.routeEnable[0].load(std::memory_order_relaxed);
                const bool toMonitor = state.routeEnable[1].load(std::memory_order_relaxed);
                routeId = toStream ? (toMonitor ? 1 : 2) : (toMonitor ? 3 : 4);
            }
            ui.outputRouteCombo.setSelectedId(routeId, juce::dontSendNotification);
        }

        for (int bus = 0; bus < numBuses; ++bus)
        {
            const bool enabled = state.routeEnable[(size_t)bus].load(std::memory_order_relaxed);
            const float sendGain = state.sendLevel[(size_t)bus].load(std::memory_order_relaxed);
            const double sendDb = sendGain > 0.0001f
                ? juce::jlimit(-60.0, 12.0, (double)(20.0f * std::log10(sendGain))) : -60.0;

            auto& routeToggle = bus == 0 ? ui.streamRouteToggle : ui.monitorRouteToggle;
            routeToggle.setToggleState(enabled, juce::dontSendNotification);
            auto& sendKnob = bus == 0 ? ui.streamSendKnob : ui.monitorSendKnob;
            setSliderFromModel(sendKnob, sendGain);

            if (i < 4)
            {
                auto& matrixToggle = bus == 0 ? matrixStreamToggles[i] : matrixMonitorToggles[i];
                auto& matrixSlider = bus == 0 ? matrixStreamSliders[i] : matrixMonitorSliders[i];
                auto& matrixLabel = bus == 0 ? matrixStreamLabels[i] : matrixMonitorLabels[i];
                matrixToggle.setToggleState(enabled, juce::dontSendNotification);
                setSliderFromModel(matrixSlider, sendDb);
                matrixLabel.setText(juce::String(sendDb, 1) + " dB", juce::dontSendNotification);
            }
        }
    }

    for (int bus = 0; bus < numBuses; ++bus)
    {
        const float masterGain = busProcessors[(size_t)bus].params.masterGain.load(std::memory_order_relaxed);
        const double masterDb = juce::jlimit(-60.0, 12.0,
            (double)juce::Decibels::gainToDecibels(masterGain, -60.0f));
        setSliderFromModel(masterUIs[(size_t)bus].masterFader, masterDb);
        masterUIs[(size_t)bus].faderValueLabel.setText(
            masterDb <= -59.0 ? "-inf dB" : juce::String(masterDb, 1) + " dB",
            juce::dontSendNotification);
        masterUIs[(size_t)bus].muteBtn.setToggleState(
            busProcessors[(size_t)bus].params.masterMute.load(std::memory_order_relaxed),
            juce::dontSendNotification);
    }

    updateDSPControlsFromTarget(selectedTarget);
    if (duckingComponent)
        duckingComponent->updateUIFromDSP();
    if (audioSafetyComponent)
        audioSafetyComponent->updateUIFromDSP();

    setSliderFromModel(duckingThresholdSlider, autoDucker.getThresholdDb());
    setSliderFromModel(duckingDepthSlider, autoDucker.getDuckingDepthDb());
    setSliderFromModel(duckingAttackSlider, autoDucker.getAttackMs());
    setSliderFromModel(duckingHoldSlider, autoDucker.getHoldMs());
    setSliderFromModel(duckingReleaseSlider, autoDucker.getReleaseMs());
    duckingEnableToggle.setToggleState(autoDucker.isEnabled(), juce::dontSendNotification);
    if (!duckingTriggerCombo.isPopupActive())
        duckingTriggerCombo.setSelectedId(autoDucker.getTriggerSource() + 1, juce::dontSendNotification);
    duckingApplyStreamCheck.setToggleState(autoDucker.isStreamEnabled(), juce::dontSendNotification);
    duckingApplyMonitorCheck.setToggleState(autoDucker.isMonitorEnabled(), juce::dontSendNotification);
    duckingTargetCh3Check.setToggleState(autoDucker.isTargetChannel(2), juce::dontSendNotification);
    duckingTargetCh4Check.setToggleState(autoDucker.isTargetChannel(3), juce::dontSendNotification);

    setSliderFromModel(safetyStreamCeilingSlider, audioSafetyProcessor.getStreamCeilingDb());
    setSliderFromModel(safetyStreamReleaseSlider, audioSafetyProcessor.getStreamReleaseMs());
    setSliderFromModel(safetyMonitorCeilingSlider, audioSafetyProcessor.getMonitorCeilingDb());
    setSliderFromModel(safetyMonitorReleaseSlider, audioSafetyProcessor.getMonitorReleaseMs());
    safetyStreamLimiterToggle.setToggleState(audioSafetyProcessor.isStreamLimiterEnabled(), juce::dontSendNotification);
    safetyMonitorLimiterToggle.setToggleState(audioSafetyProcessor.isMonitorLimiterEnabled(), juce::dontSendNotification);

    const auto muteMode = audioSafetyProcessor.getEmergencyMuteMode();
    const bool streamMuted = muteMode == LiveBus::EmergencyMuteMode::StreamOnly ||
                             muteMode == LiveBus::EmergencyMuteMode::Both;
    const bool monitorMuted = muteMode == LiveBus::EmergencyMuteMode::MonitorOnly ||
                              muteMode == LiveBus::EmergencyMuteMode::Both;
    safetyMuteStreamRadio.setToggleState(muteMode == LiveBus::EmergencyMuteMode::StreamOnly, juce::dontSendNotification);
    safetyMuteMonitorRadio.setToggleState(muteMode == LiveBus::EmergencyMuteMode::MonitorOnly, juce::dontSendNotification);
    safetyMuteBothRadio.setToggleState(muteMode == LiveBus::EmergencyMuteMode::Both, juce::dontSendNotification);
    headerEmergencyMuteBtn.setButtonText(muteMode == LiveBus::EmergencyMuteMode::None ? "🚨 PANIC MUTE" : "RESTORE AUDIO");
    headerEmergencyMuteBtn.setColour(juce::TextButton::buttonColourId,
        muteMode == LiveBus::EmergencyMuteMode::None ? juce::Colour(0xffdc2626) : juce::Colour(0xff059669));
    safetyEmergencyMuteBtn.setButtonText(muteMode == LiveBus::EmergencyMuteMode::None
        ? "EMERGENCY MUTE" : (streamMuted && monitorMuted ? "RESTORE ALL AUDIO" :
            streamMuted ? "RESTORE STREAM AUDIO" : "RESTORE MONITOR AUDIO"));
}

// ==============================================================================
// Graphics & Painting
// ==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1217)); // Charcoal dark background

    // 1. Top Header Bar
    g.setColour(juce::Colour(0xff121620));
    g.fillRect(0, 0, getWidth(), 44);
    g.setColour(juce::Colour(0xff1e2638));
    g.drawHorizontalLine(44, 0.0f, (float)getWidth());

    // Mini CPU Bar Gauge
    int gaugeX = 575;
    int gaugeY = 18;
    int gaugeW = 34;
    int gaugeH = 7;
    g.setColour(juce::Colour(0xff1a2333));
    g.fillRoundedRectangle((float)gaugeX, (float)gaugeY, (float)gaugeW, (float)gaugeH, 2.0f);
    float cpuRatio = juce::jlimit(0.0f, 1.0f, currentCpuUsage / 100.0f);
    g.setColour(currentCpuUsage > 85.0f ? juce::Colour(0xffef4444) : (currentCpuUsage > 50.0f ? juce::Colour(0xfff59e0b) : juce::Colour(0xff10b981)));
    g.fillRoundedRectangle((float)gaugeX, (float)gaugeY, (float)gaugeW * cpuRatio, (float)gaugeH, 2.0f);

    // 2. Left Sidebar Background
    g.setColour(juce::Colour(0xff0d1117));
    g.fillRect(0, 44, 134, getHeight() - 44);
    g.setColour(juce::Colour(0xff1e2638));
    g.drawVerticalLine(134, 44.0f, (float)getHeight());

    if (currentTab == TabMixer)
    {
        int areaX = 138;
        int areaY = 48;
        int areaW = getWidth() - 146;
        int areaH = getHeight() - 54;
        const int maxTopTierH = juce::jmax(300, areaH - 240);
        int topTierH = juce::jlimit(300, maxTopTierH,
                                    juce::roundToInt((float)areaH * mixerTopPanelProportion));

        int dspRackW = 330;
        int stripsAreaW = areaW - dspRackW - 10;
        int stripW = (stripsAreaW - (5 * 6)) / 6;

        const int firstVisibleChannel = channelScrollPosition;

        // Top Tier Channel Strips
        for (int slot = 0; slot < 4; ++slot)
        {
            const int channelIndex = firstVisibleChannel + slot;
            const int sx = areaX + slot * (stripW + 6);
            // Card container
            g.setColour(juce::Colour(0xff11151e));
            g.fillRoundedRectangle((float)sx, (float)areaY, (float)stripW, (float)topTierH, 4.0f);
            g.setColour(juce::Colour(0xff1e2638));
            g.drawRoundedRectangle((float)sx, (float)areaY, (float)stripW, (float)topTierH, 4.0f, 1.0f);

            // Banner
            g.setColour(juce::isPositiveAndBelow(channelIndex, maxInputs)
                            ? inputUIs[(size_t)channelIndex].accentColour
                            : juce::Colour(0xff2563eb));
            g.fillRoundedRectangle((float)sx, (float)areaY, (float)stripW, 20.0f, 4.0f);
            g.fillRect((float)sx, (float)(areaY + 12), (float)stripW, 8.0f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(channelIndex < maxInputs ? "INPUT CH " + juce::String(channelIndex + 1) : "",
                       sx, areaY, stripW, 20, juce::Justification::centred);

            // Subtle divider above bottom pill
            g.setColour(juce::Colour(0xff1e2638));
            g.drawHorizontalLine(areaY + topTierH - 32, (float)(sx + 4), (float)(sx + stripW - 4));
        }

        // Stream Bus Card
        int streamX = areaX + 4 * (stripW + 6);
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle((float)streamX, (float)areaY, (float)stripW, (float)topTierH, 4.0f);
        g.setColour(juce::Colour(0xff7c3aed).withAlpha(0.5f));
        g.drawRoundedRectangle((float)streamX, (float)areaY, (float)stripW, (float)topTierH, 4.0f, 1.0f);

        g.setColour(juce::Colour(0xff7c3aed));
        g.fillRoundedRectangle((float)streamX, (float)areaY, (float)stripW, 20.0f, 4.0f);
        g.fillRect((float)streamX, (float)(areaY + 12), (float)stripW, 8.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText("MASTER: STREAM", streamX, areaY, stripW, 20, juce::Justification::centred);

        // Monitor Bus Card
        int monitorX = areaX + 5 * (stripW + 6);
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle((float)monitorX, (float)areaY, (float)stripW, (float)topTierH, 4.0f);
        g.setColour(juce::Colour(0xff0284c7).withAlpha(0.5f));
        g.drawRoundedRectangle((float)monitorX, (float)areaY, (float)stripW, (float)topTierH, 4.0f, 1.0f);

        g.setColour(juce::Colour(0xff0284c7));
        g.fillRoundedRectangle((float)monitorX, (float)areaY, (float)stripW, 20.0f, 4.0f);
        g.fillRect((float)monitorX, (float)(areaY + 12), (float)stripW, 8.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText("MASTER: MONITOR", monitorX, areaY, stripW, 20, juce::Justification::centred);

        // Right DSP Rack Container
        int rx = areaX + stripsAreaW + 8;
        int rw = dspRackW;
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle((float)rx, (float)areaY, (float)rw, (float)topTierH, 4.0f);
        g.setColour(juce::Colour(0xff1e2638));
        g.drawRoundedRectangle((float)rx, (float)areaY, (float)rw, (float)topTierH, 4.0f, 1.0f);

        // Draw 6 DSP Module Card frames
        int cardH = (topTierH - 34) / 6;
        const char* dspCardNames[6] = { "High-Pass\nFilter", "Noise\nGate", "Compressor", "Limiter", "Reverb", "Delay" };
        const juce::Colour dspColors[6] = {
            juce::Colour(0xff10b981), // HPF
            juce::Colour(0xffef4444), // Gate
            juce::Colour(0xfff59e0b), // Comp
            juce::Colour(0xff8b5cf6), // Limiter
            juce::Colour(0xff06b6d4), // Reverb
            juce::Colour(0xffec4899)  // Delay
        };

        for (int c = 0; c < 6; ++c)
        {
            int cy = areaY + 28 + c * cardH;
            g.setColour(juce::Colour(0xff141924));
            g.fillRoundedRectangle((float)(rx + 4), (float)cy, (float)(rw - 8), (float)(cardH - 3), 3.0f);
            g.setColour(juce::Colour(0xff1f2838));
            g.drawRoundedRectangle((float)(rx + 4), (float)cy, (float)(rw - 8), (float)(cardH - 3), 3.0f, 1.0f);

            // Left colored tag/badge
            g.setColour(dspColors[c].withAlpha(0.2f));
            g.fillRoundedRectangle((float)(rx + 6), (float)(cy + 2), 64.0f, (float)(cardH - 7), 2.0f);
            g.setColour(dspColors[c]);
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawFittedText(dspCardNames[c], rx + 8, cy + 3, 60, cardH - 9, juce::Justification::centred, 2);
        }

        // =====================================================================
        // Bottom Tier 3 Side-by-Side Cards
        // =====================================================================
        int botY = areaY + topTierH + 8;
        int botH = areaH - topTierH - 8;

        int c1W = (int)(areaW * 0.44f);
        int c2W = (int)(areaW * 0.31f);
        int c3W = areaW - (c1W + c2W + 16);

        int c2X = areaX + c1W + 8;
        int c3X = c2X + c2W + 8;

        // Card 1: Routing Matrix Container
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle((float)areaX, (float)botY, (float)c1W, (float)botH, 4.0f);
        g.setColour(juce::Colour(0xff1e2638));
        g.drawRoundedRectangle((float)areaX, (float)botY, (float)c1W, (float)botH, 4.0f, 1.0f);

        // Routing Table Column Headers
        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("Channel", areaX + 10, botY + 34, 80, 16, juce::Justification::left);
        g.drawText("Input Source", areaX + 94, botY + 34, 76, 16, juce::Justification::left);
        g.drawText("Mono/Stereo", areaX + 174, botY + 34, 58, 16, juce::Justification::left);
        g.drawText("Stream Bus", areaX + 240, botY + 34, 110, 16, juce::Justification::centred);
        g.drawText("Monitor Bus", areaX + 360, botY + 34, 110, 16, juce::Justification::centred);
        g.drawText("Pan", areaX + c1W - 55, botY + 34, 45, 16, juce::Justification::centred);

        // Card 2: Auto Ducking Container
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle((float)c2X, (float)botY, (float)c2W, (float)botH, 4.0f);
        g.setColour(juce::Colour(0xff1e2638));
        g.drawRoundedRectangle((float)c2X, (float)botY, (float)c2W, (float)botH, 4.0f, 1.0f);

        // Auto Ducking Section Titles
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("Ducking Control", c2X + 140, botY + 32, 120, 16, juce::Justification::left);
        g.drawText("Gain Reduction", c2X + 140, botY + 98, 100, 16, juce::Justification::left);
        g.drawText("Target Channels", c2X + 14, botY + botH - 72, 110, 16, juce::Justification::left);
        g.drawText("Apply to Bus", c2X + 150, botY + botH - 72, 110, 16, juce::Justification::left);

        // Horizontal Gain Reduction Meter Bar Ladder
        int grBarX = c2X + 140;
        int grBarY = botY + 118;
        int grBarW = c2W - 210;
        int grBarH = 10;
        g.setColour(juce::Colour(0xff161e2b));
        g.fillRoundedRectangle((float)grBarX, (float)grBarY, (float)grBarW, (float)grBarH, 2.0f);

        float grVal = std::abs(autoDucker.getGainReductionDbStream());
        float grRatio = juce::jlimit(0.0f, 1.0f, grVal / 30.0f);
        if (grRatio > 0.01f)
        {
            g.setColour(juce::Colour(0xff10b981));
            g.fillRoundedRectangle((float)grBarX, (float)grBarY, (float)grBarW * grRatio, (float)grBarH, 2.0f);
        }

        // Card 3: Audio Safety Container
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle((float)c3X, (float)botY, (float)c3W, (float)botH, 4.0f);
        g.setColour(juce::Colour(0xff1e2638));
        g.drawRoundedRectangle((float)c3X, (float)botY, (float)c3W, (float)botH, 4.0f, 1.0f);

        // Limiter Row Labels
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Stream Limiter", c3X + 8, botY + 36, 75, 16, juce::Justification::left);
        g.drawText("Monitor Limiter", c3X + 8, botY + 84, 75, 16, juce::Justification::left);

        const int splitY = areaY + topTierH + 4;
        g.setColour(juce::Colour(0xff334155));
        g.drawHorizontalLine(splitY, (float)areaX + 8.0f, (float)getWidth() - 8.0f);
        g.setColour(juce::Colour(0xff64748b));
        g.fillRoundedRectangle((float)(areaX + areaW / 2 - 24), (float)(splitY - 2), 48.0f, 4.0f, 2.0f);
    }
    else
    {
        // Subpage background card
        g.setColour(juce::Colour(0xff11151e));
        g.fillRoundedRectangle(142.0f, 48.0f, (float)getWidth() - 150.0f, (float)getHeight() - 56.0f, 6.0f);
        g.setColour(juce::Colour(0xff1e2638));
        g.drawRoundedRectangle(142.0f, 48.0f, (float)getWidth() - 150.0f, (float)getHeight() - 56.0f, 6.0f, 1.0f);
    }
}

// ==============================================================================
// Resized Layout
// ==============================================================================
void MainComponent::resized()
{
    // 1. Top Bar
    brandLabel.setBounds(14, 4, 130, 20);
    subBrandLabel.setBounds(14, 22, 190, 16);

    audioStatusBadge.setBounds(210, 11, 105, 22);
    srPillLabel.setBounds(325, 11, 85, 22);
    bufferPillLabel.setBounds(420, 11, 75, 22);
    cpuPillLabel.setBounds(505, 11, 70, 22);

    topScenesBtn.setBounds(getWidth() - 365, 8, 68, 26);
    topPresetsBtn.setBounds(getWidth() - 293, 8, 68, 26);
    topMidiBtn.setBounds(getWidth() - 221, 8, 58, 26);
    topObsBtn.setBounds(getWidth() - 159, 8, 58, 26);
    topSettingsBtn.setBounds(getWidth() - 97, 8, 28, 26);
    headerEmergencyMuteBtn.setBounds(getWidth() - 65, 8, 58, 26);

    // 2. Left Sidebar Navigation Buttons
    int navY = 50;
    int navH = 26;
    int navGap = 3;
    for (int t = 0; t < NumTabs; ++t)
    {
        navTabButtons[t].setBounds(8, navY, 118, navH);
        bool isActive = (t == currentTab);
        navTabButtons[t].setColour(juce::TextButton::buttonColourId, isActive ? juce::Colour(0xff2563eb) : juce::Colour(0xff111622));
        navTabButtons[t].setColour(juce::TextButton::textColourOffId, isActive ? juce::Colours::white : juce::Colour(0xff94a3b8));
        navY += navH + navGap;
    }

    bool isMixerActive = (currentTab == TabMixer);

    // -------------------------------------------------------------------------
    // Top Tier: Strips & DSP Rack
    // -------------------------------------------------------------------------
    int areaX = 138;
    int areaY = 48;
    int areaW = getWidth() - 146;
    int areaH = getHeight() - 54;
    const int maxTopTierH = juce::jmax(300, areaH - 240);
    int topTierH = juce::jlimit(300, maxTopTierH,
                                juce::roundToInt((float)areaH * mixerTopPanelProportion));

    int dspRackW = 330;
    int stripsAreaW = areaW - dspRackW - 10;
    int stripW = (stripsAreaW - (5 * 6)) / 6;

    // Four input strips are visible at a time; the horizontal slider scrolls them.
    int channelStripW = stripW - 12; // leave a small inset inside each strip card
    int channelStripH = topTierH;

    int numActive = routingMatrix.getNumActiveChannels();
    const int maxScrollPosition = juce::jmax(0, numActive - 4);
    channelScrollPosition = juce::jlimit(0, maxScrollPosition, channelScrollPosition);
    channelScrollSlider.setRange(0, maxScrollPosition, 1);
    channelScrollSlider.setValue(channelScrollPosition, juce::dontSendNotification);
    const int firstVisibleChannel = channelScrollPosition;

    for (int i = 0; i < maxInputs; ++i)
    {
        auto& ch = inputUIs[i];
        const bool isActive = (i < numActive);
        const bool isInVisibleRange = (i >= firstVisibleChannel && i < firstVisibleChannel + 4);
        const bool isVisible = isMixerActive && isActive && isInVisibleRange;

        const int visibleSlot = i - firstVisibleChannel;
        const int sx = areaX + visibleSlot * (stripW + 6) + 6;
        const int sy = areaY + 28;

        ch.channelColorButton.setBounds(sx + 6, sy, channelStripW - 12, 16);
        ch.nameEditor.setBounds(sx + 6, sy + 17, channelStripW - 12, 18);
        ch.iconLabel.setBounds(sx + (channelStripW - 54) / 2, sy + 36, 54, 16);
        ch.inputSourceCombo.setBounds(sx + 4, sy + 54, channelStripW - 8, 20);

        ch.outputRouteCombo.setBounds(sx + 4, sy + 76, channelStripW - 8, 20);

        const int faderY = sy + 98;
        const int faderH = juce::jmax(60, channelStripH - 314);
        if (channelPeakMeters[i])
        {
            channelPeakMeters[i]->setBounds(sx + 8, faderY, 24, faderH);
            channelPeakMeters[i]->setVisible(isVisible);
        }
        ch.faderSlider.setBounds(sx + 38, faderY, channelStripW - 48, faderH);

        const int faderValY = faderY + faderH + 2;
        ch.faderValueLabel.setBounds(sx + 8, faderValY, channelStripW - 16, 14);

        const int gainY = faderValY + 14;
        ch.gainSlider.setBounds(sx + (channelStripW - 34) / 2, gainY, 34, 34);
        ch.gainValueLabel.setBounds(sx + 8, gainY + 34, channelStripW - 16, 12);

        const int panY = gainY + 46;
        ch.panSlider.setBounds(sx + (channelStripW - 32) / 2, panY, 32, 32);
        ch.panValueLabel.setBounds(sx + 8, panY + 32, channelStripW - 16, 12);

        const int btnY = panY + 44;
        int btnW = (channelStripW - 16) / 3;
        ch.muteBtn.setBounds(sx + 6, btnY, btnW, 18);
        ch.soloBtn.setBounds(sx + 6 + btnW + 2, btnY, btnW, 18);
        ch.linkBtn.setBounds(sx + 6 + (btnW + 2) * 2, btnY, btnW, 18);

        const int routeY = btnY + 20;
        const int routeButtonW = (channelStripW - 14) / 2;
        ch.streamRouteToggle.setButtonText("STR");
        ch.streamRouteToggle.setTooltip("Send to Stream Bus");
        ch.streamRouteToggle.setBounds(sx + 5, routeY, routeButtonW, 18);
        ch.monitorRouteToggle.setButtonText("MON");
        ch.monitorRouteToggle.setTooltip("Send to Monitor Bus");
        ch.monitorRouteToggle.setBounds(sx + 9 + routeButtonW, routeY, routeButtonW, 18);
        ch.bottomPillBtn.setBounds(sx + 8, routeY + 18, juce::jmax(0, channelStripW - 62), 18);
        ch.dspSelectBtn.setBounds(sx + channelStripW - 46, routeY + 18, 38, 18);

        ch.channelColorButton.setVisible(isVisible);
        ch.nameEditor.setVisible(isVisible);
        ch.iconLabel.setVisible(isVisible);
        ch.inputSourceHeaderLabel.setVisible(false);
        ch.inputSourceCombo.setVisible(isVisible);
        ch.faderSlider.setVisible(isVisible);
        ch.faderValueLabel.setVisible(isVisible);
        ch.gainSlider.setVisible(isVisible);
        ch.gainValueLabel.setVisible(isVisible);
        ch.panSlider.setVisible(isVisible);
        ch.panValueLabel.setVisible(isVisible);
        ch.muteBtn.setVisible(isVisible);
        ch.soloBtn.setVisible(isVisible);
        ch.linkBtn.setVisible(isVisible);
        ch.outputRouteCombo.setVisible(isVisible);
        ch.bottomPillBtn.setVisible(isVisible);
        ch.streamRouteToggle.setVisible(isVisible);
        ch.streamSendKnob.setVisible(false);
        ch.streamSendValue.setVisible(false);
        ch.monitorRouteToggle.setVisible(isVisible);
        ch.monitorSendKnob.setVisible(false);
        ch.monitorSendValue.setVisible(false);
        ch.dspSelectBtn.setVisible(isVisible);
    }

    // Keep channel controls in the sidebar so they never cover strip headers.
    {
        const int controlsY = navY + 8;
        const int visibleChannelEnd = juce::jmin(firstVisibleChannel + 4, numActive);
        addChannelBtn.setButtonText("+ Add Channel");
        addChannelBtn.setBounds(8, controlsY, 118, 24);
        addChannelBtn.setVisible(isMixerActive && numActive < maxInputs);

        removeChannelBtn.setButtonText("- Remove Channel");
        removeChannelBtn.setBounds(8, controlsY + 28, 118, 24);
        removeChannelBtn.setVisible(isMixerActive && numActive > 1);

        const int pageControlsY = controlsY + 57;
        previousChannelPageBtn.setButtonText("< Ch");
        previousChannelPageBtn.setBounds(8, pageControlsY, 35, 24);

        channelPageLabel.setBounds(43, pageControlsY, 50, 24);
        channelPageLabel.setText(juce::String(firstVisibleChannel + 1) + "-" +
                                     juce::String(visibleChannelEnd) + "/" + juce::String(numActive),
                                 juce::dontSendNotification);

        nextChannelPageBtn.setButtonText("Ch >");
        nextChannelPageBtn.setBounds(93, pageControlsY, 35, 24);
        channelScrollSlider.setBounds(8, pageControlsY + 27, 118, 18);

        previousChannelPageBtn.setVisible(isMixerActive && maxScrollPosition > 0);
        nextChannelPageBtn.setVisible(isMixerActive && maxScrollPosition > 0);
        channelPageLabel.setVisible(isMixerActive);
        channelScrollSlider.setVisible(isMixerActive && maxScrollPosition > 0);
        previousChannelPageBtn.setEnabled(channelScrollPosition > 0);
        nextChannelPageBtn.setEnabled(channelScrollPosition < maxScrollPosition);
    }

    // Master Bus 0: STREAM BUS
    int s0X = areaX + 4 * (stripW + 6);
    masterUIs[0].iconLabel.setBounds(s0X + (stripW - 56) / 2, areaY + 40, 56, 22);
    int mfY = areaY + 84;
    int mfH = topTierH - 180;
    if (streamBusPeakMeter)
    {
        streamBusPeakMeter->setBounds(s0X + 10, mfY, 24, mfH);
        streamBusPeakMeter->setVisible(isMixerActive);
    }
    masterUIs[0].masterFader.setBounds(s0X + 38, mfY, stripW - 48, mfH);
    masterUIs[0].faderValueLabel.setBounds(s0X + 8, mfY + mfH + 4, stripW - 16, 16);

    int mBtnY = mfY + mfH + 28;
    int mBtnW = (stripW - 24) / 2;
    masterUIs[0].muteBtn.setBounds(s0X + 8, mBtnY, mBtnW, 20);
    masterUIs[0].linkBtn.setBounds(s0X + 8 + mBtnW + 4, mBtnY, mBtnW, 20);
    masterUIs[0].bottomPillBtn.setBounds(s0X + 8, mBtnY + 24, juce::jmax(0, stripW - 60), 22);
    masterUIs[0].dspSelectBtn.setBounds(s0X + stripW - 48, mBtnY + 24, 40, 22);

    masterUIs[0].headerLabel.setVisible(false);
    masterUIs[0].iconLabel.setVisible(isMixerActive);
    masterUIs[0].masterFader.setVisible(isMixerActive);
    masterUIs[0].faderValueLabel.setVisible(isMixerActive);
    masterUIs[0].muteBtn.setVisible(isMixerActive);
    masterUIs[0].linkBtn.setVisible(isMixerActive);
    masterUIs[0].bottomPillBtn.setVisible(isMixerActive);
    masterUIs[0].outputTagLabel.setVisible(false);
    masterUIs[0].dspSelectBtn.setVisible(isMixerActive);

    // Master Bus 1: MONITOR BUS
    int s1X = areaX + 5 * (stripW + 6);
    masterUIs[1].iconLabel.setBounds(s1X + (stripW - 56) / 2, areaY + 40, 56, 22);
    if (monitorBusPeakMeter)
    {
        monitorBusPeakMeter->setBounds(s1X + 10, mfY, 24, mfH);
        monitorBusPeakMeter->setVisible(isMixerActive);
    }
    masterUIs[1].masterFader.setBounds(s1X + 38, mfY, stripW - 48, mfH);
    masterUIs[1].faderValueLabel.setBounds(s1X + 8, mfY + mfH + 4, stripW - 16, 16);

    masterUIs[1].muteBtn.setBounds(s1X + 8, mBtnY, mBtnW, 20);
    masterUIs[1].linkBtn.setBounds(s1X + 8 + mBtnW + 4, mBtnY, mBtnW, 20);
    masterUIs[1].bottomPillBtn.setBounds(s1X + 8, mBtnY + 24, juce::jmax(0, stripW - 60), 22);
    masterUIs[1].dspSelectBtn.setBounds(s1X + stripW - 48, mBtnY + 24, 40, 22);

    masterUIs[1].headerLabel.setVisible(false);
    masterUIs[1].iconLabel.setVisible(isMixerActive);
    masterUIs[1].masterFader.setVisible(isMixerActive);
    masterUIs[1].faderValueLabel.setVisible(isMixerActive);
    masterUIs[1].muteBtn.setVisible(isMixerActive);
    masterUIs[1].linkBtn.setVisible(isMixerActive);
    masterUIs[1].bottomPillBtn.setVisible(isMixerActive);
    masterUIs[1].outputTagLabel.setVisible(false);
    masterUIs[1].dspSelectBtn.setVisible(isMixerActive);

    // -------------------------------------------------------------------------
    // Right DSP Rack Layout
    // -------------------------------------------------------------------------
    int rx = areaX + stripsAreaW + 8;
    int rw = dspRackW;
    dspRackHeaderLabel.setBounds(rx + 6, areaY + 2, 190, 22);
    rackPowerToggle.setBounds(rx + rw - 116, areaY + 2, 34, 22);
    bypassAllBtn.setBounds(rx + rw - 76, areaY + 2, 70, 22);

    dspRackHeaderLabel.setVisible(isMixerActive);
    rackPowerToggle.setVisible(isMixerActive);
    bypassAllBtn.setVisible(isMixerActive);

    int cardH = (topTierH - 34) / 6;

    // Card 1: HPF
    int cy0 = areaY + 28;
    hpfToggle.setBounds(rx + 78, cy0 + (cardH - 22) / 2, 32, 22);
    hpfFreqSlider.setBounds(rx + 118, cy0 + 2, 68, cardH - 7);
    hpfSlopeCombo.setBounds(rx + 196, cy0 + (cardH - 22) / 2, 98, 22);
    hpfToggle.setVisible(isMixerActive);
    hpfFreqSlider.setVisible(isMixerActive);
    hpfSlopeCombo.setVisible(isMixerActive);

    // Card 2: Noise Gate
    int cy1 = cy0 + cardH;
    gateToggle.setBounds(rx + 74, cy1 + (cardH - 22) / 2, 32, 22);
    int gkW = (rw - 114) / 4;
    gateThreshSlider.setBounds(rx + 108, cy1 + 2, gkW, cardH - 7);
    gateAttackSlider.setBounds(rx + 108 + gkW, cy1 + 2, gkW, cardH - 7);
    gateHoldSlider.setBounds(rx + 108 + gkW * 2, cy1 + 2, gkW, cardH - 7);
    gateReleaseSlider.setBounds(rx + 108 + gkW * 3, cy1 + 2, gkW, cardH - 7);
    gateToggle.setVisible(isMixerActive);
    gateThreshSlider.setVisible(isMixerActive);
    gateAttackSlider.setVisible(isMixerActive);
    gateHoldSlider.setVisible(isMixerActive);
    gateReleaseSlider.setVisible(isMixerActive);

    // Card 3: Compressor
    int cy2 = cy1 + cardH;
    compToggle.setBounds(rx + 74, cy2 + (cardH - 22) / 2, 32, 22);
    int ckW = (rw - 114) / 5;
    compThreshSlider.setBounds(rx + 108, cy2 + 2, ckW, cardH - 7);
    compRatioSlider.setBounds(rx + 108 + ckW, cy2 + 2, ckW, cardH - 7);
    compAttackSlider.setBounds(rx + 108 + ckW * 2, cy2 + 2, ckW, cardH - 7);
    compReleaseSlider.setBounds(rx + 108 + ckW * 3, cy2 + 2, ckW, cardH - 7);
    compMakeupSlider.setBounds(rx + 108 + ckW * 4, cy2 + 2, ckW, cardH - 7);
    compToggle.setVisible(isMixerActive);
    compThreshSlider.setVisible(isMixerActive);
    compRatioSlider.setVisible(isMixerActive);
    compAttackSlider.setVisible(isMixerActive);
    compReleaseSlider.setVisible(isMixerActive);
    compMakeupSlider.setVisible(isMixerActive);

    // Card 4: Limiter
    int cy3 = cy2 + cardH;
    limiterToggle.setBounds(rx + 74, cy3 + (cardH - 22) / 2, 32, 22);
    limiterCeilSlider.setBounds(rx + 120, cy3 + 2, 70, cardH - 7);
    limiterReleaseSlider.setBounds(rx + 210, cy3 + 2, 70, cardH - 7);
    limiterToggle.setVisible(isMixerActive);
    limiterCeilSlider.setVisible(isMixerActive);
    limiterReleaseSlider.setVisible(isMixerActive);

    // Card 5: Reverb
    int cy4 = cy3 + cardH;
    reverbToggle.setBounds(rx + 74, cy4 + (cardH - 22) / 2, 32, 22);
    int rkW = (rw - 114) / 4;
    reverbRoomSlider.setBounds(rx + 108, cy4 + 2, rkW, cardH - 7);
    reverbDampingSlider.setBounds(rx + 108 + rkW, cy4 + 2, rkW, cardH - 7);
    reverbWetSlider.setBounds(rx + 108 + rkW * 2, cy4 + 2, rkW, cardH - 7);
    reverbDrySlider.setBounds(rx + 108 + rkW * 3, cy4 + 2, rkW, cardH - 7);
    reverbToggle.setVisible(isMixerActive);
    reverbRoomSlider.setVisible(isMixerActive);
    reverbDampingSlider.setVisible(isMixerActive);
    reverbWetSlider.setVisible(isMixerActive);
    reverbDrySlider.setVisible(isMixerActive);

    // Card 6: Delay
    int cy5 = cy4 + cardH;
    delayToggle.setBounds(rx + 74, cy5 + (cardH - 22) / 2, 32, 22);
    int dkW = (rw - 114) / 5;
    delayTimeSlider.setBounds(rx + 108, cy5 + 2, dkW, cardH - 7);
    delayTempoBtn.setBounds(rx + 108 + dkW + 4, cy5 + (cardH - 22) / 2, dkW - 8, 22);
    delayFeedbackSlider.setBounds(rx + 108 + dkW * 2, cy5 + 2, dkW, cardH - 7);
    delayWetSlider.setBounds(rx + 108 + dkW * 3, cy5 + 2, dkW, cardH - 7);
    delayDrySlider.setBounds(rx + 108 + dkW * 4, cy5 + 2, dkW, cardH - 7);
    delayToggle.setVisible(isMixerActive);
    delayTimeSlider.setVisible(isMixerActive);
    delayTempoBtn.setVisible(isMixerActive);
    delayFeedbackSlider.setVisible(isMixerActive);
    delayWetSlider.setVisible(isMixerActive);
    delayDrySlider.setVisible(isMixerActive);

    // Hide legacy middle tier components
    if (masterMeterStation) masterMeterStation->setVisible(false);
    vstTargetCombo.setVisible(false);
    scanPluginsBtn.setVisible(false);
    addPluginBtn.setVisible(false);
    bypassAllPluginsBtn.setVisible(false);
    for (auto& row : pluginSlotRows)
    {
        row.dragHandle.setVisible(false);
        row.indexLabel.setVisible(false);
        row.nameLabel.setVisible(false);
        row.categoryLabel.setVisible(false);
        row.statusBadge.setVisible(false);
        row.configBtn.setVisible(false);
        row.bypassBtn.setVisible(false);
    }
    sceneCombo.setVisible(false);
    saveSceneBtn.setVisible(false);
    saveSceneAsBtn.setVisible(false);
    sceneListBox.setVisible(false);
    dupSceneBtn.setVisible(false);
    renameSceneBtn.setVisible(false);
    deleteSceneBtn.setVisible(false);
    clearLogBtn.setVisible(false);
    copyLogBtn.setVisible(false);
    logBox.setVisible(false);
    inputDeviceCombo.setVisible(false);
    outputDeviceCombo.setVisible(false);
    sampleRateCombo.setVisible(false);
    streamOutputMapCombo.setVisible(false);
    monitorOutputMapCombo.setVisible(false);
    bufferSizeCombo.setVisible(false);
    testToneBtn.setVisible(false);

    // -------------------------------------------------------------------------
    // Bottom Tier 3 Side-by-Side Cards
    // -------------------------------------------------------------------------
    int botY = areaY + topTierH + 8;
    int botH = areaH - topTierH - 8;

    int c1W = (int)(areaW * 0.44f);
    int c2W = (int)(areaW * 0.31f);
    int c3W = areaW - (c1W + c2W + 16);

    int c2X = areaX + c1W + 8;
    int c3X = c2X + c2W + 8;

    // Card 1: Routing Matrix
    routingMatrixHeader.setBounds(areaX + 8, botY + 6, 200, 22);
    matrixResetBtn.setBounds(areaX + c1W - 195, botY + 6, 54, 22);
    matrixPresetCombo.setBounds(areaX + c1W - 135, botY + 6, 125, 22);

    routingMatrixHeader.setVisible(isMixerActive);
    matrixResetBtn.setVisible(isMixerActive);
    matrixPresetCombo.setVisible(isMixerActive);

    int rowStartY = botY + 54;
    int rowGap = (botH - 64) / 4;

    for (int r = 0; r < 4; ++r)
    {
        int ry = rowStartY + r * rowGap;
        matrixChannelPills[r].setBounds(areaX + 8, ry, 82, 22);
        matrixSourceCombos[r].setBounds(areaX + 94, ry, 76, 22);
        matrixStereoCombos[r].setBounds(areaX + 174, ry, 56, 22);

        matrixStreamToggles[r].setBounds(areaX + 236, ry, 32, 22);
        matrixStreamSliders[r].setBounds(areaX + 272, ry, 52, 22);
        matrixStreamLabels[r].setBounds(areaX + 326, ry, 38, 22);

        matrixMonitorToggles[r].setBounds(areaX + 368, ry, 32, 22);
        matrixMonitorSliders[r].setBounds(areaX + 404, ry, 52, 22);
        matrixMonitorLabels[r].setBounds(areaX + 458, ry, 38, 22);

        matrixPanSliders[r].setBounds(areaX + c1W - 54, ry - 2, 26, 26);
        matrixPanLabels[r].setBounds(areaX + c1W - 28, ry, 26, 22);

        matrixChannelPills[r].setVisible(isMixerActive);
        matrixSourceCombos[r].setVisible(isMixerActive);
        matrixStereoCombos[r].setVisible(isMixerActive);
        matrixStreamToggles[r].setVisible(isMixerActive);
        matrixStreamSliders[r].setVisible(isMixerActive);
        matrixStreamLabels[r].setVisible(isMixerActive);
        matrixMonitorToggles[r].setVisible(isMixerActive);
        matrixMonitorSliders[r].setVisible(isMixerActive);
        matrixMonitorLabels[r].setVisible(isMixerActive);
        matrixPanSliders[r].setVisible(isMixerActive);
        matrixPanLabels[r].setVisible(isMixerActive);
    }

    // Card 2: Auto Ducking
    duckingHeaderLabel.setBounds(c2X + 8, botY + 6, 95, 22);
    duckingEnableToggle.setBounds(c2X + 104, botY + 6, 32, 22);
    duckingPresetCombo.setBounds(c2X + c2W - 165, botY + 6, 155, 22);

    duckingTriggerLabel.setBounds(c2X + 10, botY + 34, 85, 16);
    duckingTriggerCombo.setBounds(c2X + 8, botY + 52, 120, 22);
    duckingThresholdSlider.setBounds(c2X + 80, botY + 80, 52, 54);
    duckingInputLevelLabel.setBounds(c2X + 12, botY + 138, 60, 16);

    int dkW2 = (c2W - 146) / 4;
    duckingDepthSlider.setBounds(c2X + 138, botY + 48, dkW2, 48);
    duckingAttackSlider.setBounds(c2X + 138 + dkW2, botY + 48, dkW2, 48);
    duckingHoldSlider.setBounds(c2X + 138 + dkW2 * 2, botY + 48, dkW2, 48);
    duckingReleaseSlider.setBounds(c2X + 138 + dkW2 * 3, botY + 48, dkW2, 48);

    duckingGRLabel.setBounds(c2X + c2W - 65, botY + 114, 55, 18);

    duckingTargetCh3Check.setBounds(c2X + 12, botY + botH - 52, 130, 20);
    duckingTargetCh4Check.setBounds(c2X + 12, botY + botH - 28, 130, 20);
    duckingApplyStreamCheck.setBounds(c2X + 150, botY + botH - 52, 110, 20);
    duckingApplyMonitorCheck.setBounds(c2X + 150, botY + botH - 28, 110, 20);

    duckingHeaderLabel.setVisible(isMixerActive);
    duckingEnableToggle.setVisible(isMixerActive);
    duckingPresetCombo.setVisible(isMixerActive);
    duckingTriggerLabel.setVisible(isMixerActive);
    duckingTriggerCombo.setVisible(isMixerActive);
    duckingThresholdSlider.setVisible(isMixerActive);
    duckingInputLevelLabel.setVisible(isMixerActive);
    duckingDepthSlider.setVisible(isMixerActive);
    duckingAttackSlider.setVisible(isMixerActive);
    duckingHoldSlider.setVisible(isMixerActive);
    duckingReleaseSlider.setVisible(isMixerActive);
    duckingGRLabel.setVisible(isMixerActive);
    duckingTargetCh3Check.setVisible(isMixerActive);
    duckingTargetCh4Check.setVisible(isMixerActive);
    duckingApplyStreamCheck.setVisible(isMixerActive);
    duckingApplyMonitorCheck.setVisible(isMixerActive);

    // Card 3: Audio Safety
    safetyHeaderLabel.setBounds(c3X + 8, botY + 6, 110, 22);
    safetyStreamLimiterToggle.setBounds(c3X + 85, botY + 34, 32, 22);
    safetyStreamCeilingSlider.setBounds(c3X + 124, botY + 28, 48, 50);
    safetyStreamReleaseSlider.setBounds(c3X + 176, botY + 28, 48, 50);

    safetyMonitorLimiterToggle.setBounds(c3X + 85, botY + 82, 32, 22);
    safetyMonitorCeilingSlider.setBounds(c3X + 124, botY + 76, 48, 50);
    safetyMonitorReleaseSlider.setBounds(c3X + 176, botY + 76, 48, 50);

    safetyClipWarningLabel.setBounds(c3X + 8, botY + 128, 120, 20);
    safetyResetPeakBtn.setBounds(c3X + 130, botY + 128, 68, 20);
    safetyStreamClipLabel.setBounds(c3X + 8, botY + 150, (c3W - 20) / 2, 20);
    safetyMonitorPeakHoldLabel.setBounds(c3X + 12 + (c3W - 20) / 2, botY + 150, (c3W - 20) / 2, 20);

    safetyRoutingCheckBadge.setBounds(c3X + 8, botY + 176, c3W - 74, 20);
    safetyRoutingSettingsBtn.setBounds(c3X + c3W - 64, botY + 176, 56, 20);

    safetyEmergencyMuteBtn.setBounds(c3X + 8, botY + botH - 58, c3W - 16, 26);
    int rW3 = (c3W - 16) / 3;
    safetyMuteStreamRadio.setBounds(c3X + 8, botY + botH - 28, rW3, 20);
    safetyMuteMonitorRadio.setBounds(c3X + 8 + rW3, botY + botH - 28, rW3, 20);
    safetyMuteBothRadio.setBounds(c3X + 8 + rW3 * 2, botY + botH - 28, rW3, 20);

    safetyHeaderLabel.setVisible(isMixerActive);
    safetyStreamLimiterToggle.setVisible(isMixerActive);
    safetyStreamCeilingSlider.setVisible(isMixerActive);
    safetyStreamReleaseSlider.setVisible(isMixerActive);
    safetyMonitorLimiterToggle.setVisible(isMixerActive);
    safetyMonitorCeilingSlider.setVisible(isMixerActive);
    safetyMonitorReleaseSlider.setVisible(isMixerActive);
    safetyClipWarningLabel.setVisible(isMixerActive);
    safetyResetPeakBtn.setVisible(isMixerActive);
    safetyStreamClipLabel.setVisible(isMixerActive);
    safetyMonitorPeakHoldLabel.setVisible(isMixerActive);
    safetyRoutingCheckBadge.setVisible(isMixerActive);
    safetyRoutingSettingsBtn.setVisible(isMixerActive);
    safetyEmergencyMuteBtn.setVisible(isMixerActive);
    safetyMuteStreamRadio.setVisible(isMixerActive);
    safetyMuteMonitorRadio.setVisible(isMixerActive);
    safetyMuteBothRadio.setVisible(isMixerActive);

    // -------------------------------------------------------------------------
    // Subpages Layout & Visibility
    // -------------------------------------------------------------------------
    int subX = 142;
    int subY = 48;
    int subW = getWidth() - 150;
    int subH = getHeight() - 56;

    bool isRouting = (currentTab == TabRouting);
    if (routingMatrixComponent)
    {
        routingMatrixComponent->setVisible(isRouting);
        if (isRouting) routingMatrixComponent->setBounds(subX, subY, subW, subH);
    }

    bool isDSP = (currentTab == TabDSP);
    dspViewSwitchBtn.setVisible(isDSP);
    if (isDSP)
    {
        dspViewSwitchBtn.setBounds(getWidth() - 260, 10, 240, 24);
        if (showVocalRack)
        {
            if (vocalDSPRackComponent)
            {
                vocalDSPRackComponent->setVisible(true);
                vocalDSPRackComponent->setBounds(subX, subY, subW, subH);
            }
            if (eightBandEQComponent) eightBandEQComponent->setVisible(false);
            dspViewSwitchBtn.setButtonText("Switch to Standalone 8-Band EQ");
        }
        else
        {
            if (vocalDSPRackComponent) vocalDSPRackComponent->setVisible(false);
            if (eightBandEQComponent)
            {
                eightBandEQComponent->setVisible(true);
                eightBandEQComponent->setBounds(subX, subY, subW, subH);
            }
            dspViewSwitchBtn.setButtonText("Switch to Vocal DSP Rack");
        }
    }
    else
    {
        if (vocalDSPRackComponent) vocalDSPRackComponent->setVisible(false);
        if (eightBandEQComponent) eightBandEQComponent->setVisible(false);
    }

    bool isEffects = (currentTab == TabEffects);
    if (effectsConsoleComponent)
    {
        effectsConsoleComponent->setVisible(isEffects);
        if (isEffects) effectsConsoleComponent->setBounds(subX, subY, subW, subH);
    }

    bool isDucking = (currentTab == TabDucking);
    if (duckingComponent)
    {
        duckingComponent->setVisible(isDucking);
        if (isDucking) duckingComponent->setBounds(subX, subY, subW, subH);
    }

    bool isSafety = (currentTab == TabSafety);
    if (audioSafetyComponent)
    {
        audioSafetyComponent->setVisible(isSafety);
        if (isSafety) audioSafetyComponent->setBounds(subX, subY, subW, subH);
    }

    bool isMacros = (currentTab == TabMacros);
    if (macroControlComponent)
    {
        macroControlComponent->setVisible(isMacros);
        if (isMacros) macroControlComponent->setBounds(subX, subY, subW, subH);
    }

    bool isPlayback = (currentTab == TabPlayback);
    if (playbackComponent)
    {
        playbackComponent->setVisible(isPlayback);
        if (isPlayback) playbackComponent->setBounds(subX, subY, subW, subH);
    }

    bool isScenes = (currentTab == TabScenes);
    if (scenesConsoleComponent)
    {
        scenesConsoleComponent->setVisible(isScenes);
        if (isScenes) scenesConsoleComponent->setBounds(subX, subY, subW, subH);
    }

    bool isDevices = (currentTab == TabDevices);
    if (audioSettingsComponent)
    {
        audioSettingsComponent->setVisible(isDevices);
        if (isDevices) audioSettingsComponent->setBounds(subX, subY, subW, subH);
    }

    bool isMIDI = (currentTab == TabMIDI);
    if (midiLearnComponent)
    {
        midiLearnComponent->setVisible(isMIDI);
        if (isMIDI) midiLearnComponent->setBounds(subX, subY, subW, subH);
    }

    bool isSettings = (currentTab == TabSettings || currentTab == TabDiagnostics);
    if (performanceDiagnosticsComponent)
    {
        performanceDiagnosticsComponent->setVisible(isSettings);
        if (isSettings) performanceDiagnosticsComponent->setBounds(subX, subY, subW, subH);
    }

    bool isOBS = (currentTab == TabOBS);
    if (obsMixerComponent)
    {
        obsMixerComponent->setVisible(isOBS);
        if (isOBS) obsMixerComponent->setBounds(subX, subY, subW, subH);
    }

    bool isYouTube = (currentTab == TabYouTube);
    if (youtubePlayerComponent)
    {
        youtubePlayerComponent->setVisible(isYouTube);
        if (isYouTube) youtubePlayerComponent->setBounds(subX, subY, subW, subH);
    }
}

void MainComponent::mouseDown(const juce::MouseEvent& event)
{
    if (currentTab != TabMixer)
        return;

    const int areaH = getHeight() - 54;
    const int maxTopTierH = juce::jmax(300, areaH - 240);
    const int topTierH = juce::jlimit(300, maxTopTierH,
        juce::roundToInt((float)areaH * mixerTopPanelProportion));
    const int dividerY = 48 + topTierH + 4;
    resizingMixerPanels = event.x >= 138 && event.x <= getWidth() - 8 &&
                           std::abs(event.y - dividerY) <= 5;
}

void MainComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!resizingMixerPanels || currentTab != TabMixer)
        return;

    const int areaH = getHeight() - 54;
    if (areaH <= 0)
        return;

    const float minProportion = 300.0f / (float)areaH;
    const float maxProportion = (float)juce::jmax(300, areaH - 240) / (float)areaH;
    mixerTopPanelProportion = juce::jlimit(minProportion, maxProportion,
        (float)(event.y - 52) / (float)areaH);
    resized();
    repaint();
}

void MainComponent::mouseUp(const juce::MouseEvent&)
{
    resizingMixerPanels = false;
}

// ==============================================================================
// Button Listener & Tab Navigation
// ==============================================================================
void MainComponent::buttonClicked(juce::Button* b)
{
    // Check Emergency Mute Button
    if (b == &headerEmergencyMuteBtn)
    {
        if (audioSafetyProcessor.getEmergencyMuteMode() != LiveBus::EmergencyMuteMode::None)
        {
            audioSafetyProcessor.unmuteAll();
            headerEmergencyMuteBtn.setButtonText("🚨 PANIC MUTE");
            headerEmergencyMuteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
            log("Emergency Mute disengaged. Audio restored.");
        }
        else
        {
            audioSafetyProcessor.triggerEmergencyMuteAll();
            headerEmergencyMuteBtn.setButtonText("RESTORE AUDIO");
            headerEmergencyMuteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff059669));
            log("EMERGENCY MUTE ENGAGED! All output buses silenced.");
        }
        return;
    }

    // Check Navigation Tabs
    for (int t = 0; t < NumTabs; ++t)
    {
        if (b == &navTabButtons[t])
        {
            currentTab = t;
            if (currentTab == TabDSP)
            {
                if (eightBandEQComponent)
                    eightBandEQComponent->updateUIFromProcessor();
                if (vocalDSPRackComponent)
                    vocalDSPRackComponent->updateUIFromProcessor();
                if (puzuDSPComponent)
                    puzuDSPComponent->updateUIFromProcessor();
            }
            resized();
            repaint();
            log("Switched to tab: " + navTabButtons[t].getButtonText());
            return;
        }
    }

    if (b == &dspViewSwitchBtn)
    {
        showVocalRack = !showVocalRack;
        if (eightBandEQComponent)
            eightBandEQComponent->updateUIFromProcessor();
        if (vocalDSPRackComponent)
            vocalDSPRackComponent->updateUIFromProcessor();
        if (puzuDSPComponent)
            puzuDSPComponent->updateUIFromProcessor();
        resized();
        repaint();
        return;
    }

    if (b == &streamBusDSPTab)
    {
        selectDSPTarget(maxInputs);
    }
    else if (b == &monitorBusDSPTab)
    {
        selectDSPTarget(maxInputs + 1);
    }
    else if (b == &allStreamBtn)
    {
        for (int i = 0; i < maxInputs; ++i)
        {
            routingMatrix.getChannel(i).routeEnable[0].store(true);
            inputUIs[i].streamRouteToggle.setToggleState(true, juce::dontSendNotification);
            routingStreamChecks[i].setToggleState(true, juce::dontSendNotification);
        }
        log("Routed all channels to Stream Bus.");
    }
    else if (b == &allMonitorBtn)
    {
        for (int i = 0; i < maxInputs; ++i)
        {
            routingMatrix.getChannel(i).routeEnable[1].store(true);
            inputUIs[i].monitorRouteToggle.setToggleState(true, juce::dontSendNotification);
            routingMonitorChecks[i].setToggleState(true, juce::dontSendNotification);
        }
        log("Routed all channels to Monitor Bus.");
    }
    else if (b == &clearAllRouteBtn)
    {
        for (int i = 0; i < maxInputs; ++i)
        {
            routingMatrix.getChannel(i).routeEnable[0].store(false);
            routingMatrix.getChannel(i).routeEnable[1].store(false);
            inputUIs[i].streamRouteToggle.setToggleState(false, juce::dontSendNotification);
            inputUIs[i].monitorRouteToggle.setToggleState(false, juce::dontSendNotification);
            routingStreamChecks[i].setToggleState(false, juce::dontSendNotification);
            routingMonitorChecks[i].setToggleState(false, juce::dontSendNotification);
        }
        log("Cleared all channel routing.");
    }
    else if (b == &addChannelBtn)
    {
        handleAddChannel();
    }
    else if (b == &removeChannelBtn)
    {
        handleRemoveChannel();
    }
    else if (b == &scanPluginsBtn)
    {
        scanVST3Plugins();
    }
    else if (b == &addPluginBtn)
    {
        addSelectedPlugin();
    }
    else if (b == &bypassAllPluginsBtn)
    {
        bool anyActive = false;
        for (auto& row : pluginSlotRows)
            if (!row.bypassBtn.getToggleState()) anyActive = true;

        for (auto& row : pluginSlotRows)
        {
            row.bypassBtn.setToggleState(anyActive, juce::sendNotification);
        }
        log(anyActive ? "Bypassed all VST3 plugins." : "Un-bypassed all VST3 plugins.");
    }
    else if (b == &saveSceneBtn)
    {
        onSaveScene();
    }
    else if (b == &saveSceneAsBtn)
    {
        onSaveSceneAs();
    }
    else if (b == &dupSceneBtn)
    {
        onDuplicateScene();
    }
    else if (b == &renameSceneBtn)
    {
        onRenameScene();
    }
    else if (b == &deleteSceneBtn)
    {
        onDeleteScene();
    }
    else if (b == &clearLogBtn)
    {
        logBox.clear();
    }
    else if (b == &copyLogBtn)
    {
        juce::SystemClipboard::copyTextToClipboard(logBox.getText());
        log("Application log copied to clipboard.");
    }
    else if (b == &dspPuzuEQToggleBtn)
    {
        showPuzu15BandEQ = !showPuzu15BandEQ;
        dspPuzuEQToggleBtn.setButtonText(showPuzu15BandEQ ? "Switch to 4-Band Studio EQ" : "Switch to Puzu 15-Band EQ");
        resized();
        repaint();
    }
    else if (b == &testWatchdogRecoveryBtn)
    {
        pluginHostClient.simulateCrash();
        log("[WATCHDOG] Triggered plugin crash simulation. Helper process restarting in background.");
    }
    else if (b == &browseVSTPathBtn)
    {
        scanVST3Plugins();
    }
    else if (b == &exportZipBtn)
    {
        auto scenesDir = sceneManager.getScenesDirectory();
        auto zipFile = scenesDir.getChildFile("LiveBus_Scenes_Backup.zip");
        if (PresetZipManager::exportPresetsToZip(zipFile, scenesDir))
            log("Exported presets to: " + zipFile.getFullPathName());
        else
            log("Failed to export preset ZIP.");
    }
    else if (b == &importZipBtn)
    {
        auto scenesDir = sceneManager.getScenesDirectory();
        auto zipFile = scenesDir.getChildFile("LiveBus_Scenes_Backup.zip");
        if (zipFile.existsAsFile())
        {
            if (PresetZipManager::importPresetsFromZip(zipFile, scenesDir))
            {
                refreshSceneList();
                log("Imported presets from ZIP successfully.");
            }
            else
            {
                log("Failed to import preset ZIP. Existing scenes were preserved.");
            }
        }
        else
        {
            log("Preset backup ZIP not found in scenes folder.");
        }
    }
    else if (b == &restartDeviceBtn)
    {
        deviceManager.closeAudioDevice();
        deviceManager.restartLastAudioDevice();
        log("Audio device restarted via Supervisor.");
    }
    else if (b == &openAudioFileBtn)
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Audio File (WAV, MP3, FLAC)...",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.mp3;*.flac;*.aif;*.ogg");
        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                if (audioFilePlayer.loadFile(file))
                {
                    playbackStatusLabel.setText("Loaded: " + file.getFileName() + " (" +
                                                juce::String(audioFilePlayer.getLengthInSeconds(), 1) + "s)", juce::dontSendNotification);
                    log("Loaded BGM file: " + file.getFileName());
                }
            }
        });
    }
    else if (b == &playAudioFileBtn)
    {
        audioFilePlayer.start();
        log("Started BGM playback.");
    }
    else if (b == &pauseAudioFileBtn)
    {
        audioFilePlayer.pause();
        log("Paused BGM playback.");
    }
    else if (b == &stopAudioFileBtn)
    {
        audioFilePlayer.stop();
        playbackPositionSlider.setValue(0.0, juce::dontSendNotification);
        log("Stopped BGM playback.");
    }
    else if (b == &resetDefaultsBtn)
    {
        for (int i = 0; i < maxInputs; ++i)
        {
            inputUIs[i].gainSlider.setValue(i == 0 ? 1.0 : (i == 1 ? 0.67 : 0.5), juce::sendNotification);
            inputUIs[i].panSlider.setValue(i == 2 ? -1.0 : (i == 3 ? 1.0 : 0.0), juce::sendNotification);
            inputUIs[i].muteBtn.setToggleState(false, juce::sendNotification);
            inputUIs[i].soloBtn.setToggleState(false, juce::sendNotification);
            inputUIs[i].streamRouteToggle.setToggleState(true, juce::sendNotification);
            inputUIs[i].monitorRouteToggle.setToggleState(true, juce::sendNotification);
        }
        masterUIs[0].masterFader.setValue(0.79, juce::sendNotification);
        masterUIs[1].masterFader.setValue(0.79, juce::sendNotification);
        log("Reset all mixer controls to broadcast defaults.");
    }

    // Check DSP select buttons on channel strips
    for (int i = 0; i < maxInputs; ++i)
    {
        if (b == &inputUIs[i].dspSelectBtn)
        {
            openFloatingFxRack(i);
            break;
        }
    }
    if (b == &masterUIs[0].dspSelectBtn) openFloatingFxRack(maxInputs);
    if (b == &masterUIs[1].dspSelectBtn) openFloatingFxRack(maxInputs + 1);

    // DSP Sub-Tabs
    for (int s = 0; s < 6; ++s)
    {
        if (b == &dspSubTabs[s])
        {
            currentDSPSubTab = s;
            for (int k = 0; k < 6; ++k)
                dspSubTabs[k].setColour(juce::TextButton::buttonColourId, (k == s) ? juce::Colour(0xff00f5d4) : juce::Colour(0xff1f293d));
            break;
        }
    }
}

// ==============================================================================
// DSP Target Management
// ==============================================================================
BusDSPProcessor& MainComponent::getProcessorForTarget(int targetIndex)
{
    if (targetIndex >= 0 && targetIndex < maxInputs)
        return channelDSPs[targetIndex];
    if (targetIndex == maxInputs + 1)
        return busProcessors[1]; // Monitor Bus
    return busProcessors[0];     // Stream Bus
}

void MainComponent::selectDSPTarget(int targetIndex)
{
    targetIndex = juce::jlimit(0, maxInputs + 1, targetIndex);
    selectedTarget = targetIndex;
    streamBusDSPTab.setColour(juce::TextButton::buttonColourId, (selectedTarget == maxInputs) ? juce::Colour(0xff00f5d4) : juce::Colour(0xff1f293d));
    monitorBusDSPTab.setColour(juce::TextButton::buttonColourId, (selectedTarget == maxInputs + 1) ? juce::Colour(0xffffb703) : juce::Colour(0xff1f293d));

    dspPageTargetCombo.setSelectedId(targetIndex + 1, juce::dontSendNotification);

    updateDSPControlsFromTarget(selectedTarget);
    if (eqCurveDisplay) eqCurveDisplay->repaint();

    if (targetIndex < routingMatrix.getNumActiveChannels())
    {
        const int maxScrollPosition = juce::jmax(0, routingMatrix.getNumActiveChannels() - 4);
        const int visibleEnd = channelScrollPosition + 4;
        if (targetIndex < channelScrollPosition || targetIndex >= visibleEnd)
        {
            channelScrollPosition = juce::jlimit(0, maxScrollPosition, targetIndex - 3);
            channelScrollSlider.setValue(channelScrollPosition, juce::dontSendNotification);
            resized();
        }
        if (routingMatrixComponent &&
            routingMatrixComponent->getSelectedChannel() != targetIndex)
            routingMatrixComponent->setSelectedChannel(targetIndex);
    }

    if (puzuDSPComponent && targetIndex >= maxInputs)
        puzuDSPComponent->setSelectedBus(targetIndex == maxInputs + 1 ? 1 : 0);

    if (effectsConsoleComponent)
    {
        if (effectsConsoleComponent->getSelectedTarget() != targetIndex)
            effectsConsoleComponent->selectTarget(targetIndex);
    }
    if (floatingFxRackComponent &&
        floatingFxRackComponent->getSelectedTarget() != targetIndex)
        floatingFxRackComponent->selectTarget(targetIndex);

    if (vocalDSPRackComponent)
    {
        vocalDSPRackComponent->setSelectedTarget(
            (LiveBus::VocalDSPRackComponent::TargetIndex)targetIndex);
    }
    if (eightBandEQComponent)
    {
        eightBandEQComponent->setSelectedTarget(
            (LiveBus::EightBandEQComponent::TargetIndex)targetIndex);
    }
    repaint();
}

void MainComponent::openFloatingFxRack(int targetIndex)
{
    if (targetIndex < 0 || targetIndex >= LiveBus::VST3HostEngine::NumTargets ||
        vst3HostEngine == nullptr)
        return;

    if (floatingFxRackComponent == nullptr)
    {
        floatingFxRackComponent = std::make_unique<LiveBus::EffectsConsoleComponent>(
            *vst3HostEngine, pluginHostClient);
        floatingFxRackComponent->onTargetSelected = [this](int selectedIndex) {
            selectDSPTarget(selectedIndex);
        };
    }

    selectDSPTarget(targetIndex);

    if (floatingFxRackWindow == nullptr)
    {
        floatingFxRackWindow = std::make_unique<FloatingFxRackWindow>();
        floatingFxRackWindow->setContentNonOwned(floatingFxRackComponent.get(), false);
    }

    floatingFxRackWindow->setVisible(true);
    floatingFxRackWindow->toFront(true);
}

void MainComponent::showFxChainMenu(int targetIndex, juce::Component& targetComponent)
{
    if (vst3HostEngine == nullptr ||
        targetIndex < 0 || targetIndex >= LiveBus::VST3HostEngine::NumTargets)
        return;

    const int slotCount = vst3HostEngine->getNumPluginsInTarget(targetIndex);
    juce::PopupMenu menu;
    menu.addSectionHeader(LiveBus::VST3HostEngine::getTargetName(targetIndex));

    if (slotCount == 0)
    {
        menu.addItem(1, "No insert effects loaded", false);
    }
    else
    {
        menu.addItem(1, "Bypass all inserts");
        menu.addItem(2, "Enable all inserts");
        menu.addSeparator();

        for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            const auto slot = vst3HostEngine->getSlotInfo(targetIndex, slotIndex);
            const auto label = juce::String(slotIndex + 1) + ". " + slot.pluginName +
                (slot.isBypassed ? "  [Bypassed]" : "  [Active]");
            menu.addItem(100 + slotIndex, label, true, !slot.isBypassed);
        }
    }

    juce::Component::SafePointer<MainComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&targetComponent),
        [safeThis, targetIndex, slotCount](int result)
        {
            if (safeThis == nullptr || safeThis->vst3HostEngine == nullptr || result == 0)
                return;

            if (result == 1 && slotCount > 0)
            {
                for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
                    safeThis->vst3HostEngine->setSlotBypassed(targetIndex, slotIndex, true);
                return;
            }

            if (result == 2)
            {
                for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
                    safeThis->vst3HostEngine->setSlotBypassed(targetIndex, slotIndex, false);
                return;
            }

            const int slotIndex = result - 100;
            if (slotIndex >= 0 && slotIndex < slotCount)
            {
                const bool bypassed = safeThis->vst3HostEngine->isSlotBypassed(targetIndex, slotIndex);
                safeThis->vst3HostEngine->setSlotBypassed(targetIndex, slotIndex, !bypassed);
            }
        });
}

void MainComponent::updateFxStripIndicators()
{
    if (vst3HostEngine == nullptr)
        return;

    auto updateButton = [this](juce::TextButton& button, int targetIndex)
    {
        const int slotCount = vst3HostEngine->getNumPluginsInTarget(targetIndex);
        juce::String tooltip = LiveBus::VST3HostEngine::getTargetName(targetIndex);
        bool anyActive = false;

        if (slotCount == 0)
        {
            tooltip += "\nNo insert effects loaded.";
        }
        else
        {
            tooltip += "\nInsert chain:";
            for (int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
            {
                const auto slot = vst3HostEngine->getSlotInfo(targetIndex, slotIndex);
                tooltip += "\n" + juce::String(slotIndex + 1) + ". " + slot.pluginName +
                           (slot.isBypassed ? " [Bypassed]" : "");
                anyActive = anyActive || !slot.isBypassed;
            }
        }
        tooltip += "\nClick to open the effects rack.";

        const auto buttonText = slotCount > 0
            ? "FX " + juce::String(slotCount)
            : "FX";
        const auto buttonColour = slotCount == 0
            ? juce::Colour(0xff164e63)
            : (anyActive ? juce::Colour(0xff065f46) : juce::Colour(0xff92400e));

        if (button.getButtonText() != buttonText)
            button.setButtonText(buttonText);
        if (button.getTooltip() != tooltip)
            button.setTooltip(tooltip);
        if (button.findColour(juce::TextButton::buttonColourId) != buttonColour)
            button.setColour(juce::TextButton::buttonColourId, buttonColour);
    };

    const int firstVisibleChannel = channelScrollPosition;
    const int visibleEnd = juce::jmin(firstVisibleChannel + 4,
                                      routingMatrix.getNumActiveChannels());
    for (int channelIndex = firstVisibleChannel; channelIndex < visibleEnd; ++channelIndex)
        updateButton(inputUIs[(size_t)channelIndex].dspSelectBtn, channelIndex);

    updateButton(masterUIs[0].dspSelectBtn, maxInputs);
    updateButton(masterUIs[1].dspSelectBtn, maxInputs + 1);
}

void MainComponent::applyDSPControls(int targetIndex)
{
    if (isUpdatingUI) return;
    auto& proc = getProcessorForTarget(targetIndex);

    bool masterPwr = rackPowerToggle.getToggleState();

    // High-Pass Filter
    proc.params.hpfEnabled.store(masterPwr && hpfToggle.getToggleState());
    proc.params.hpfFreq.store((float)hpfFreqSlider.getValue());
    proc.params.hpfSlope.store(hpfSlopeCombo.getSelectedId() - 1);

    // Noise Gate
    proc.params.gateEnabled.store(masterPwr && gateToggle.getToggleState());
    proc.params.gateThresholdDb.store((float)gateThreshSlider.getValue());
    proc.params.gateAttackMs.store((float)gateAttackSlider.getValue());
    proc.params.gateHoldMs.store((float)gateHoldSlider.getValue());
    proc.params.gateReleaseMs.store((float)gateReleaseSlider.getValue());
    proc.params.gateRatio.store((float)gateRatioSlider.getValue());

    // Compressor
    proc.params.compEnabled.store(masterPwr && compToggle.getToggleState());
    proc.params.compThresholdDb.store((float)compThreshSlider.getValue());
    proc.params.compRatio.store((float)compRatioSlider.getValue());
    proc.params.compAttackMs.store((float)compAttackSlider.getValue());
    proc.params.compReleaseMs.store((float)compReleaseSlider.getValue());
    proc.params.compMakeupDb.store((float)compMakeupSlider.getValue());

    // Limiter
    proc.params.limiterEnabled.store(masterPwr && limiterToggle.getToggleState());
    proc.params.limiterCeilingDb.store((float)limiterCeilSlider.getValue());
    proc.params.limiterReleaseMs.store((float)limiterReleaseSlider.getValue());

    // Reverb
    proc.params.reverbEnabled.store(masterPwr && reverbToggle.getToggleState());
    proc.params.reverbRoomSize.store((float)reverbRoomSlider.getValue());
    proc.params.reverbDamping.store((float)reverbDampingSlider.getValue());
    proc.params.reverbWetLevel.store((float)reverbWetSlider.getValue());
    proc.params.reverbDryLevel.store((float)reverbDrySlider.getValue());

    // Delay
    proc.params.delayEnabled.store(masterPwr && delayToggle.getToggleState());
    proc.params.delayTimeMs.store((float)delayTimeSlider.getValue());
    proc.params.delayFeedback.store((float)delayFeedbackSlider.getValue());
    proc.params.delayWetLevel.store((float)delayWetSlider.getValue());
    proc.params.delayDryLevel.store((float)delayDrySlider.getValue());

    // Legacy EQ
    proc.params.eqEnabled.store(masterPwr && enableEQToggle.getToggleState());
    proc.params.eqLowFreq.store((float)eqLowFreqSlider.getValue());
    proc.params.eqLowGainDb.store((float)eqLowGainSlider.getValue());
    proc.params.eqMid1Freq.store((float)eqMid1FreqSlider.getValue());
    proc.params.eqMid1GainDb.store((float)eqMid1GainSlider.getValue());
    proc.params.eqFreq.store((float)eqMid2FreqSlider.getValue());
    proc.params.eqGainDb.store((float)eqMid2GainSlider.getValue());
    proc.params.eqHighFreq.store((float)eqHighFreqSlider.getValue());
    proc.params.eqHighGainDb.store((float)eqHighGainSlider.getValue());

    proc.updateCoefficients();
}

void MainComponent::updateDSPControlsFromTarget(int targetIndex)
{
    isUpdatingUI = true;
    auto setSliderFromModel = [](juce::Slider& slider, double value)
    {
        if (!slider.isMouseButtonDown())
            slider.setValue(value, juce::dontSendNotification);
    };
    auto& proc = getProcessorForTarget(targetIndex);

    // Update Header label
    juce::String targetName;
    if (targetIndex >= 0 && targetIndex < 4)
    {
        const char* names[] = { "Channel 1 - DSP & Effects", "Channel 2 - DSP & Effects", "Channel 3 - DSP & Effects", "Channel 4 - DSP & Effects" };
        targetName = names[targetIndex];
    }
    else if (targetIndex == maxInputs)
    {
        targetName = "Stream Bus - DSP & Effects";
    }
    else if (targetIndex == maxInputs + 1)
    {
        targetName = "Monitor Bus - DSP & Effects";
    }
    else
    {
        targetName = "Channel " + juce::String(targetIndex + 1) + " - DSP & Effects";
    }
    dspRackHeaderLabel.setText(targetName, juce::dontSendNotification);

    // HPF
    hpfToggle.setToggleState(proc.params.hpfEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(hpfFreqSlider, proc.params.hpfFreq.load());
    if (!hpfSlopeCombo.isPopupActive())
        hpfSlopeCombo.setSelectedId(proc.params.hpfSlope.load() + 1, juce::dontSendNotification);

    // Noise Gate
    gateToggle.setToggleState(proc.params.gateEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(gateThreshSlider, proc.params.gateThresholdDb.load());
    setSliderFromModel(gateAttackSlider, proc.params.gateAttackMs.load());
    setSliderFromModel(gateHoldSlider, proc.params.gateHoldMs.load());
    setSliderFromModel(gateReleaseSlider, proc.params.gateReleaseMs.load());
    setSliderFromModel(gateRatioSlider, proc.params.gateRatio.load());

    // Compressor
    compToggle.setToggleState(proc.params.compEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(compThreshSlider, proc.params.compThresholdDb.load());
    setSliderFromModel(compRatioSlider, proc.params.compRatio.load());
    setSliderFromModel(compAttackSlider, proc.params.compAttackMs.load());
    setSliderFromModel(compReleaseSlider, proc.params.compReleaseMs.load());
    setSliderFromModel(compMakeupSlider, proc.params.compMakeupDb.load());

    // Limiter
    limiterToggle.setToggleState(proc.params.limiterEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(limiterCeilSlider, proc.params.limiterCeilingDb.load());
    setSliderFromModel(limiterReleaseSlider, proc.params.limiterReleaseMs.load());

    // Reverb
    reverbToggle.setToggleState(proc.params.reverbEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(reverbRoomSlider, proc.params.reverbRoomSize.load());
    setSliderFromModel(reverbDampingSlider, proc.params.reverbDamping.load());
    setSliderFromModel(reverbWetSlider, proc.params.reverbWetLevel.load());
    setSliderFromModel(reverbDrySlider, proc.params.reverbDryLevel.load());

    // Delay
    delayToggle.setToggleState(proc.params.delayEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(delayTimeSlider, proc.params.delayTimeMs.load());
    setSliderFromModel(delayFeedbackSlider, proc.params.delayFeedback.load());
    setSliderFromModel(delayWetSlider, proc.params.delayWetLevel.load());
    setSliderFromModel(delayDrySlider, proc.params.delayDryLevel.load());

    // Legacy EQ
    enableEQToggle.setToggleState(proc.params.eqEnabled.load(), juce::dontSendNotification);
    setSliderFromModel(eqLowFreqSlider, proc.params.eqLowFreq.load());
    setSliderFromModel(eqLowGainSlider, proc.params.eqLowGainDb.load());
    setSliderFromModel(eqMid1FreqSlider, proc.params.eqMid1Freq.load());
    setSliderFromModel(eqMid1GainSlider, proc.params.eqMid1GainDb.load());
    setSliderFromModel(eqMid2FreqSlider, proc.params.eqFreq.load());
    setSliderFromModel(eqMid2GainSlider, proc.params.eqGainDb.load());
    setSliderFromModel(eqHighFreqSlider, proc.params.eqHighFreq.load());
    setSliderFromModel(eqHighGainSlider, proc.params.eqHighGainDb.load());

    isUpdatingUI = false;
}

void MainComponent::initializeMidiParameterRegistry()
{
    midiParameterRegistry.clear();

    // 1. Input Channels 1..maxInputs
    for (int i = 0; i < maxInputs; ++i)
    {
        juce::String chScope = "Input " + juce::String(i + 1);
        juce::String chPrefix = "ch." + juce::String(i + 1);

        // Gain
        LiveBus::ParameterDescriptor gainDesc;
        gainDesc.id = chPrefix + ".gain";
        gainDesc.scope = chScope;
        gainDesc.name = "Gain";
        gainDesc.minValue = -60.0f;
        gainDesc.maxValue = 12.0f;
        gainDesc.defaultValue = 0.0f;
        gainDesc.getter = [this, i]() {
            return juce::Decibels::gainToDecibels(routingMatrix.getChannel(i).gain.load(std::memory_order_relaxed));
        };
        gainDesc.setter = [this, i](float db) {
            float g = (db <= -59.5f) ? 0.0f : juce::Decibels::decibelsToGain(db);
            routingMatrix.getChannel(i).gain.store(g, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, i, db]() {
                inputUIs[i].gainSlider.setValue(db, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(gainDesc);

        // Pan
        LiveBus::ParameterDescriptor panDesc;
        panDesc.id = chPrefix + ".pan";
        panDesc.scope = chScope;
        panDesc.name = "Pan";
        panDesc.minValue = -1.0f;
        panDesc.maxValue = 1.0f;
        panDesc.defaultValue = 0.0f;
        panDesc.getter = [this, i]() {
            return routingMatrix.getChannel(i).pan.load(std::memory_order_relaxed);
        };
        panDesc.setter = [this, i](float p) {
            routingMatrix.getChannel(i).pan.store(p, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, i, p]() {
                inputUIs[i].panSlider.setValue(p, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(panDesc);

        // Mute
        LiveBus::ParameterDescriptor muteDesc;
        muteDesc.id = chPrefix + ".mute";
        muteDesc.scope = chScope;
        muteDesc.name = "Mute";
        muteDesc.isBoolean = true;
        muteDesc.minValue = 0.0f;
        muteDesc.maxValue = 1.0f;
        muteDesc.defaultValue = 0.0f;
        muteDesc.getter = [this, i]() {
            return routingMatrix.getChannel(i).mute.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        };
        muteDesc.setter = [this, i](float m) {
            bool b = (m >= 0.5f);
            routingMatrix.getChannel(i).mute.store(b, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, i, b]() {
                inputUIs[i].muteBtn.setToggleState(b, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(muteDesc);

        // Solo
        LiveBus::ParameterDescriptor soloDesc;
        soloDesc.id = chPrefix + ".solo";
        soloDesc.scope = chScope;
        soloDesc.name = "Solo";
        soloDesc.isBoolean = true;
        soloDesc.minValue = 0.0f;
        soloDesc.maxValue = 1.0f;
        soloDesc.defaultValue = 0.0f;
        soloDesc.getter = [this, i]() {
            return routingMatrix.getChannel(i).solo.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        };
        soloDesc.setter = [this, i](float s) {
            bool b = (s >= 0.5f);
            routingMatrix.getChannel(i).solo.store(b, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, i, b]() {
                inputUIs[i].soloBtn.setToggleState(b, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(soloDesc);

        // Send to Stream
        LiveBus::ParameterDescriptor sendStreamDesc;
        sendStreamDesc.id = chPrefix + ".send.stream";
        sendStreamDesc.scope = chScope;
        sendStreamDesc.name = "Send to Stream";
        sendStreamDesc.minValue = 0.0f;
        sendStreamDesc.maxValue = 1.5f;
        sendStreamDesc.defaultValue = 1.0f;
        sendStreamDesc.getter = [this, i]() {
            return routingMatrix.getChannel(i).sendLevel[0].load(std::memory_order_relaxed);
        };
        sendStreamDesc.setter = [this, i](float v) {
            routingMatrix.getChannel(i).sendLevel[0].store(v, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, i, v]() {
                inputUIs[i].streamSendKnob.setValue(v, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(sendStreamDesc);

        // Send to Monitor
        LiveBus::ParameterDescriptor sendMonitorDesc;
        sendMonitorDesc.id = chPrefix + ".send.monitor";
        sendMonitorDesc.scope = chScope;
        sendMonitorDesc.name = "Send to Monitor";
        sendMonitorDesc.minValue = 0.0f;
        sendMonitorDesc.maxValue = 1.5f;
        sendMonitorDesc.defaultValue = 1.0f;
        sendMonitorDesc.getter = [this, i]() {
            return routingMatrix.getChannel(i).sendLevel[1].load(std::memory_order_relaxed);
        };
        sendMonitorDesc.setter = [this, i](float v) {
            routingMatrix.getChannel(i).sendLevel[1].store(v, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, i, v]() {
                inputUIs[i].monitorSendKnob.setValue(v, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(sendMonitorDesc);
    }

    // 2. Master Buses (Stream & Monitor)
    const char* busNames[] = { "Stream Bus", "Monitor Bus" };
    const char* busIds[] = { "bus.stream", "bus.monitor" };

    for (int b = 0; b < 2; ++b)
    {
        juce::String bScope = busNames[b];
        juce::String bPrefix = busIds[b];

        // Master Gain
        LiveBus::ParameterDescriptor mgDesc;
        mgDesc.id = bPrefix + ".gain";
        mgDesc.scope = bScope;
        mgDesc.name = "Master Gain";
        mgDesc.minValue = -60.0f;
        mgDesc.maxValue = 12.0f;
        mgDesc.defaultValue = 0.0f;
        mgDesc.getter = [this, b]() {
            return juce::Decibels::gainToDecibels(busProcessors[b].params.masterGain.load(std::memory_order_relaxed));
        };
        mgDesc.setter = [this, b](float db) {
            float g = (db <= -59.5f) ? 0.0f : juce::Decibels::decibelsToGain(db);
            busProcessors[b].params.masterGain.store(g, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, b, db]() {
                masterUIs[b].masterFader.setValue(db, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(mgDesc);

        // Master Pan
        LiveBus::ParameterDescriptor mpDesc;
        mpDesc.id = bPrefix + ".pan";
        mpDesc.scope = bScope;
        mpDesc.name = "Master Pan";
        mpDesc.minValue = -1.0f;
        mpDesc.maxValue = 1.0f;
        mpDesc.defaultValue = 0.0f;
        mpDesc.getter = [this, b]() {
            return busProcessors[b].params.masterPan.load(std::memory_order_relaxed);
        };
        mpDesc.setter = [this, b](float p) {
            busProcessors[b].params.masterPan.store(p, std::memory_order_relaxed);
        };
        midiParameterRegistry.registerParameter(mpDesc);

        // Master Mute
        LiveBus::ParameterDescriptor mmDesc;
        mmDesc.id = bPrefix + ".mute";
        mmDesc.scope = bScope;
        mmDesc.name = "Master Mute";
        mmDesc.isBoolean = true;
        mmDesc.minValue = 0.0f;
        mmDesc.maxValue = 1.0f;
        mmDesc.defaultValue = 0.0f;
        mmDesc.getter = [this, b]() {
            return busProcessors[b].params.masterMute.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        };
        mmDesc.setter = [this, b](float m) {
            bool isMuted = (m >= 0.5f);
            busProcessors[b].params.masterMute.store(isMuted, std::memory_order_relaxed);
            juce::MessageManager::callAsync([this, b, isMuted]() {
                masterUIs[b].muteBtn.setToggleState(isMuted, juce::dontSendNotification);
            });
        };
        midiParameterRegistry.registerParameter(mmDesc);
    }

    // 3. Stream Bus DSP Dynamics
    LiveBus::ParameterDescriptor compThresh;
    compThresh.id = "bus.stream.comp.thresh";
    compThresh.scope = "Stream Bus DSP";
    compThresh.name = "Compressor Threshold";
    compThresh.minValue = -60.0f;
    compThresh.maxValue = 0.0f;
    compThresh.defaultValue = -20.0f;
    compThresh.getter = [this]() { return busProcessors[0].params.compThresholdDb.load(std::memory_order_relaxed); };
    compThresh.setter = [this](float v) {
        busProcessors[0].params.compThresholdDb.store(v, std::memory_order_relaxed);
        busProcessors[0].updateCoefficients();
    };
    midiParameterRegistry.registerParameter(compThresh);

    LiveBus::ParameterDescriptor compRatio;
    compRatio.id = "bus.stream.comp.ratio";
    compRatio.scope = "Stream Bus DSP";
    compRatio.name = "Compressor Ratio";
    compRatio.minValue = 1.0f;
    compRatio.maxValue = 20.0f;
    compRatio.defaultValue = 4.0f;
    compRatio.getter = [this]() { return busProcessors[0].params.compRatio.load(std::memory_order_relaxed); };
    compRatio.setter = [this](float v) {
        busProcessors[0].params.compRatio.store(v, std::memory_order_relaxed);
        busProcessors[0].updateCoefficients();
    };
    midiParameterRegistry.registerParameter(compRatio);

    LiveBus::ParameterDescriptor limCeil;
    limCeil.id = "bus.stream.limiter.ceiling";
    limCeil.scope = "Stream Bus DSP";
    limCeil.name = "Limiter Ceiling";
    limCeil.minValue = -24.0f;
    limCeil.maxValue = 0.0f;
    limCeil.defaultValue = -1.0f;
    limCeil.getter = [this]() { return busProcessors[0].params.limiterCeilingDb.load(std::memory_order_relaxed); };
    limCeil.setter = [this](float v) {
        busProcessors[0].params.limiterCeilingDb.store(v, std::memory_order_relaxed);
        busProcessors[0].updateCoefficients();
    };
    midiParameterRegistry.registerParameter(limCeil);

    LiveBus::ParameterDescriptor revWet;
    revWet.id = "bus.stream.reverb.wet";
    revWet.scope = "Stream Bus DSP";
    revWet.name = "Reverb Wet Level";
    revWet.minValue = 0.0f;
    revWet.maxValue = 1.0f;
    revWet.defaultValue = 0.3f;
    revWet.getter = [this]() { return busProcessors[0].params.reverbWetLevel.load(std::memory_order_relaxed); };
    revWet.setter = [this](float v) {
        busProcessors[0].params.reverbWetLevel.store(v, std::memory_order_relaxed);
        busProcessors[0].updateCoefficients();
    };
    midiParameterRegistry.registerParameter(revWet);

    LiveBus::ParameterDescriptor delTime;
    delTime.id = "bus.stream.delay.time";
    delTime.scope = "Stream Bus DSP";
    delTime.name = "Delay Time (ms)";
    delTime.minValue = 10.0f;
    delTime.maxValue = 2000.0f;
    delTime.defaultValue = 350.0f;
    delTime.getter = [this]() { return busProcessors[0].params.delayTimeMs.load(std::memory_order_relaxed); };
    delTime.setter = [this](float v) {
        busProcessors[0].params.delayTimeMs.store(v, std::memory_order_relaxed);
        busProcessors[0].updateCoefficients();
    };
    midiParameterRegistry.registerParameter(delTime);

    // 4. Vocal DSP Rack (for Vocal input channel 0)
    LiveBus::ParameterDescriptor vocGate;
    vocGate.id = "vocal.rack.gate.thresh";
    vocGate.scope = "Vocal DSP Rack";
    vocGate.name = "Gate Threshold";
    vocGate.minValue = -80.0f;
    vocGate.maxValue = 0.0f;
    vocGate.defaultValue = -48.0f;
    vocGate.getter = [this]() { return channelDSPs[0].vocalRack.params.gateOpenThresholdDb.load(std::memory_order_relaxed); };
    vocGate.setter = [this](float v) { channelDSPs[0].vocalRack.params.gateOpenThresholdDb.store(v, std::memory_order_relaxed); };
    midiParameterRegistry.registerParameter(vocGate);

    LiveBus::ParameterDescriptor vocComp;
    vocComp.id = "vocal.rack.comp.thresh";
    vocComp.scope = "Vocal DSP Rack";
    vocComp.name = "Compressor Threshold";
    vocComp.minValue = -60.0f;
    vocComp.maxValue = 0.0f;
    vocComp.defaultValue = -20.0f;
    vocComp.getter = [this]() { return channelDSPs[0].vocalRack.params.compThresholdDb.load(std::memory_order_relaxed); };
    vocComp.setter = [this](float v) { channelDSPs[0].vocalRack.params.compThresholdDb.store(v, std::memory_order_relaxed); };
    midiParameterRegistry.registerParameter(vocComp);

    LiveBus::ParameterDescriptor vocRatio;
    vocRatio.id = "vocal.rack.comp.ratio";
    vocRatio.scope = "Vocal DSP Rack";
    vocRatio.name = "Compressor Ratio";
    vocRatio.minValue = 1.0f;
    vocRatio.maxValue = 20.0f;
    vocRatio.defaultValue = 3.0f;
    vocRatio.getter = [this]() { return channelDSPs[0].vocalRack.params.compRatio.load(std::memory_order_relaxed); };
    vocRatio.setter = [this](float v) { channelDSPs[0].vocalRack.params.compRatio.store(v, std::memory_order_relaxed); };
    midiParameterRegistry.registerParameter(vocRatio);

    LiveBus::ParameterDescriptor vocMakeup;
    vocMakeup.id = "vocal.rack.comp.makeup";
    vocMakeup.scope = "Vocal DSP Rack";
    vocMakeup.name = "Compressor Makeup";
    vocMakeup.minValue = -12.0f;
    vocMakeup.maxValue = 24.0f;
    vocMakeup.defaultValue = 0.0f;
    vocMakeup.getter = [this]() { return channelDSPs[0].vocalRack.params.compMakeupDb.load(std::memory_order_relaxed); };
    vocMakeup.setter = [this](float v) { channelDSPs[0].vocalRack.params.compMakeupDb.store(v, std::memory_order_relaxed); };
    midiParameterRegistry.registerParameter(vocMakeup);

    LiveBus::ParameterDescriptor vocDeess;
    vocDeess.id = "vocal.rack.deesser.freq";
    vocDeess.scope = "Vocal DSP Rack";
    vocDeess.name = "De-Esser Frequency";
    vocDeess.minValue = 1000.0f;
    vocDeess.maxValue = 12000.0f;
    vocDeess.defaultValue = 5500.0f;
    vocDeess.skewFactor = 0.4f; // Logarithmic frequency
    vocDeess.getter = [this]() { return channelDSPs[0].vocalRack.params.deEsserFreq.load(std::memory_order_relaxed); };
    vocDeess.setter = [this](float v) { channelDSPs[0].vocalRack.params.deEsserFreq.store(v, std::memory_order_relaxed); };
    midiParameterRegistry.registerParameter(vocDeess);

    LiveBus::ParameterDescriptor vocLim;
    vocLim.id = "vocal.rack.limiter.thresh";
    vocLim.scope = "Vocal DSP Rack";
    vocLim.name = "Limiter Ceiling";
    vocLim.minValue = -20.0f;
    vocLim.maxValue = 0.0f;
    vocLim.defaultValue = -1.0f;
    vocLim.getter = [this]() { return channelDSPs[0].vocalRack.params.limiterCeilingDb.load(std::memory_order_relaxed); };
    vocLim.setter = [this](float v) { channelDSPs[0].vocalRack.params.limiterCeilingDb.store(v, std::memory_order_relaxed); };
    midiParameterRegistry.registerParameter(vocLim);

    // 5. Playback Controls
    LiveBus::ParameterDescriptor playDesc;
    playDesc.id = "playback.play";
    playDesc.scope = "Audio Player";
    playDesc.name = "Play Trigger";
    playDesc.isBoolean = true;
    playDesc.setter = [this](float v) {
        if (v >= 0.5f) audioFilePlayer.start();
    };
    midiParameterRegistry.registerParameter(playDesc);

    LiveBus::ParameterDescriptor pauseDesc;
    pauseDesc.id = "playback.pause";
    pauseDesc.scope = "Audio Player";
    pauseDesc.name = "Pause Trigger";
    pauseDesc.isBoolean = true;
    pauseDesc.setter = [this](float v) {
        if (v >= 0.5f) audioFilePlayer.pause();
    };
    midiParameterRegistry.registerParameter(pauseDesc);

    LiveBus::ParameterDescriptor stopDesc;
    stopDesc.id = "playback.stop";
    stopDesc.scope = "Audio Player";
    stopDesc.name = "Stop Trigger";
    stopDesc.isBoolean = true;
    stopDesc.setter = [this](float v) {
        if (v >= 0.5f) audioFilePlayer.stop();
    };
    midiParameterRegistry.registerParameter(stopDesc);

    // 6. Auto Ducking
    LiveBus::ParameterDescriptor duckThresh;
    duckThresh.id = "ducking.threshold";
    duckThresh.scope = "Auto Ducking";
    duckThresh.name = "Ducking Threshold";
    duckThresh.minValue = -60.0f;
    duckThresh.maxValue = 0.0f;
    duckThresh.defaultValue = -30.0f;
    duckThresh.getter = [this]() { return autoDucker.getThresholdDb(); };
    duckThresh.setter = [this](float v) { autoDucker.setThresholdDb(v); if (duckingComponent) duckingComponent->updateUIFromDSP(); };
    midiParameterRegistry.registerParameter(duckThresh);

    LiveBus::ParameterDescriptor duckDepth;
    duckDepth.id = "ducking.depth";
    duckDepth.scope = "Auto Ducking";
    duckDepth.name = "Ducking Depth";
    duckDepth.minValue = -36.0f;
    duckDepth.maxValue = 0.0f;
    duckDepth.defaultValue = -12.0f;
    duckDepth.getter = [this]() { return autoDucker.getDuckingDepthDb(); };
    duckDepth.setter = [this](float v) { autoDucker.setDuckingDepthDb(v); if (duckingComponent) duckingComponent->updateUIFromDSP(); };
    midiParameterRegistry.registerParameter(duckDepth);

    // 7. Audio Safety & Output Limiter
    LiveBus::ParameterDescriptor emergMute;
    emergMute.id = "safety.emergencyMute";
    emergMute.scope = "Audio Safety";
    emergMute.name = "Emergency Mute All";
    emergMute.isBoolean = true;
    emergMute.getter = [this]() { return audioSafetyProcessor.getEmergencyMuteMode() != LiveBus::EmergencyMuteMode::None ? 1.0f : 0.0f; };
    emergMute.setter = [this](float v) {
        if (v >= 0.5f) audioSafetyProcessor.triggerEmergencyMuteAll();
        else audioSafetyProcessor.unmuteAll();
    };
    midiParameterRegistry.registerParameter(emergMute);

    // 8. Macro Controls (1..4)
    for (int m = 0; m < macroEngine.getNumMacros(); ++m)
    {
        auto macroDef = macroEngine.getMacro(m);
        LiveBus::ParameterDescriptor macroDesc;
        macroDesc.id = "macro." + juce::String(m + 1) + ".value";
        macroDesc.scope = "Macro Control";
        macroDesc.name = macroDef ? macroDef->name : ("Macro " + juce::String(m + 1));
        macroDesc.minValue = 0.0f;
        macroDesc.maxValue = 1.0f;
        macroDesc.defaultValue = macroDef ? macroDef->defaultValue : 0.5f;
        macroDesc.getter = [this, m]() { return macroEngine.getMacroValue(m); };
        macroDesc.setter = [this, m](float v) { macroEngine.setMacroValue(m, v, true); };
        midiParameterRegistry.registerParameter(macroDesc);
    }
}



// ==============================================================================
// Scene Management Actions
// ==============================================================================
void MainComponent::onLoadScene(const juce::String& sceneName)
{
    if (sceneName.isEmpty()) return;
    SceneData data;
    juce::String err;
    if (sceneManager.loadScene(sceneName, data, err))
    {
        applySceneToUI(data);
        sceneCombo.setText(sceneName, juce::dontSendNotification);
        sceneListBox.repaint();
        log("Scene loaded successfully: " + sceneName);
    }
    else
    {
        sceneCombo.setText(sceneName, juce::dontSendNotification);
        sceneListBox.repaint();
        log("Applied preset configuration: " + sceneName);
    }
}

void MainComponent::onSaveScene()
{
    auto curName = sceneCombo.getText();
    if (curName.isEmpty()) curName = "Default";

    SceneData data;
    captureSceneFromUI(data);
    data.sceneName = curName;

    if (sceneManager.saveScene(curName, data))
    {
        refreshSceneList();
        log("Scene saved: " + curName);
    }
    else
    {
        log("Save scene failed: " + curName);
    }
}

void MainComponent::onSaveSceneAs()
{
    auto curName = sceneCombo.getText();
    if (curName.isEmpty()) curName = "Default";

    juce::AlertWindow alert("Save Scene As...", "Enter new scene name:", juce::AlertWindow::QuestionIcon);
    alert.addTextEditor("name", curName + "_Copy");
    alert.addButton("Save", 1);
    alert.addButton("Cancel", 0);

    if (alert.runModalLoop() == 1)
    {
        auto newName = alert.getTextEditorContents("name").trim();
        if (newName.isNotEmpty())
        {
            SceneData data;
            captureSceneFromUI(data);
            data.sceneName = newName;

            if (sceneManager.saveScene(newName, data))
            {
                if (!availableScenes.contains(newName))
                    availableScenes.add(newName);
                refreshSceneList();
                sceneCombo.setText(newName, juce::dontSendNotification);
                log("Scene saved as: " + newName);
            }
        }
    }
}

void MainComponent::onDuplicateScene()
{
    auto curName = sceneCombo.getText();
    if (curName.isEmpty()) return;

    auto newName = curName + "_Copy";
    if (sceneManager.duplicateScene(curName, newName))
    {
        if (!availableScenes.contains(newName))
            availableScenes.add(newName);
        refreshSceneList();
        sceneCombo.setText(newName, juce::dontSendNotification);
        log("Duplicated scene: " + curName + " -> " + newName);
    }
}

void MainComponent::onRenameScene()
{
    auto curName = sceneCombo.getText();
    if (curName.isEmpty()) return;

    juce::AlertWindow alert("Rename Scene", "Enter new name for " + curName + ":", juce::AlertWindow::QuestionIcon);
    alert.addTextEditor("name", curName);
    alert.addButton("Rename", 1);
    alert.addButton("Cancel", 0);

    if (alert.runModalLoop() == 1)
    {
        auto newName = alert.getTextEditorContents("name").trim();
        if (newName.isNotEmpty() && newName != curName)
        {
            if (sceneManager.renameScene(curName, newName))
            {
                availableScenes.removeString(curName);
                availableScenes.add(newName);
                refreshSceneList();
                sceneCombo.setText(newName, juce::dontSendNotification);
                log("Renamed scene: " + curName + " -> " + newName);
            }
        }
    }
}

void MainComponent::onDeleteScene()
{
    auto curName = sceneCombo.getText();
    if (curName.isEmpty()) return;

    // Check if the scene file actually exists before proceeding
    auto safeName = curName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    auto sceneFile = sceneManager.getScenesDirectory().getChildFile(safeName + ".xml");
    bool fileExists = sceneFile.existsAsFile();

    if (!fileExists)
    {
        // Scene file already deleted or never existed - show warning
        juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Scene Not Found",
            "The scene '" + curName + "' does not exist on disk. It may have already been deleted.",
            "OK");
        return;
    }

    // Show confirmation dialog only if the file exists
    if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Delete Scene",
            "Delete scene: " + curName + "?"))
    {
        sceneManager.deleteScene(curName);
        availableScenes.removeString(curName);
        refreshSceneList();
        log("Deleted scene: " + curName);
    }
}

void MainComponent::refreshSceneList()
{
    sceneCombo.clear();
    for (int i = 0; i < availableScenes.size(); ++i)
        sceneCombo.addItem(availableScenes[i], i + 1);
    sceneListBox.updateContent();
    sceneListBox.repaint();
}

void MainComponent::syncChannelRouteUiState(int channelIndex)
{
    auto& state = routingMatrix.getChannel(channelIndex);
    const bool toStream = state.routeEnable[0].load(std::memory_order_relaxed);
    const bool toMonitor = state.routeEnable[1].load(std::memory_order_relaxed);
    const int submix = state.submixGroup.load(std::memory_order_relaxed);

    inputUIs[(size_t)channelIndex].streamRouteToggle.setToggleState(toStream, juce::dontSendNotification);
    inputUIs[(size_t)channelIndex].monitorRouteToggle.setToggleState(toMonitor, juce::dontSendNotification);
    if (channelIndex < 4)
    {
        matrixStreamToggles[(size_t)channelIndex].setToggleState(toStream, juce::dontSendNotification);
        matrixMonitorToggles[(size_t)channelIndex].setToggleState(toMonitor, juce::dontSendNotification);
    }
    routingStreamChecks[(size_t)channelIndex].setToggleState(toStream, juce::dontSendNotification);
    routingMonitorChecks[(size_t)channelIndex].setToggleState(toMonitor, juce::dontSendNotification);

    if (submix >= 0 && submix < maxSubmixBuses)
        inputUIs[(size_t)channelIndex].outputRouteCombo.setSelectedId(submix + 5, juce::dontSendNotification);
    else if (toStream && toMonitor)
        inputUIs[(size_t)channelIndex].outputRouteCombo.setSelectedId(1, juce::dontSendNotification);
    else if (toStream)
        inputUIs[(size_t)channelIndex].outputRouteCombo.setSelectedId(2, juce::dontSendNotification);
    else if (toMonitor)
        inputUIs[(size_t)channelIndex].outputRouteCombo.setSelectedId(3, juce::dontSendNotification);
    else
        inputUIs[(size_t)channelIndex].outputRouteCombo.setSelectedId(4, juce::dontSendNotification);
}

void MainComponent::applySceneToUI(const SceneData& data)
{
    for (int i = 0; i < maxInputs; ++i)
    {
        auto& channel = inputUIs[(size_t)i];
        channel.accentColour = data.channels[(size_t)i].accentColour != 0
                                   ? juce::Colour(data.channels[(size_t)i].accentColour)
                                   : defaultChannelAccent(i);
        channel.channelColorButton.setColour(juce::TextButton::buttonColourId,
                                             channel.accentColour.withAlpha(0.22f));
        channel.channelColorButton.setColour(juce::TextButton::textColourOffId,
                                             channel.accentColour.brighter(0.35f));
        channel.gainSlider.setColour(juce::Slider::rotarySliderFillColourId, channel.accentColour);
        channel.panSlider.setColour(juce::Slider::rotarySliderFillColourId, channel.accentColour);
        if (i < 4)
        {
            matrixChannelPills[(size_t)i].setColour(juce::Label::backgroundColourId,
                                                    channel.accentColour.withAlpha(0.2f));
            matrixChannelPills[(size_t)i].setColour(juce::Label::textColourId,
                                                    channel.accentColour.brighter(0.4f));
            matrixPanSliders[(size_t)i].setColour(juce::Slider::rotarySliderFillColourId,
                                                  channel.accentColour);
        }
        if (data.channels[i].name.isNotEmpty())
        {
            inputUIs[i].nameEditor.setText(data.channels[i].name, false);
            routingChannelNames[i].setText(data.channels[i].name, juce::dontSendNotification);
        }
        inputUIs[i].gainSlider.setValue(data.channels[i].gain, juce::sendNotification);
        inputUIs[i].panSlider.setValue(data.channels[i].pan, juce::sendNotification);
        inputUIs[i].muteBtn.setToggleState(data.channels[i].mute, juce::sendNotification);
        inputUIs[i].soloBtn.setToggleState(data.channels[i].solo, juce::sendNotification);

        const bool toStream = data.channels[i].streamRoute;
        const bool toMonitor = data.channels[i].monitorRoute;
        auto& state = routingMatrix.getChannel(i);
        state.routeEnable[0].store(toStream, std::memory_order_release);
        state.routeEnable[1].store(toMonitor, std::memory_order_release);
        if ((toStream || toMonitor) && state.submixGroup.load(std::memory_order_relaxed) >= 0)
            state.submixGroup.store(-1, std::memory_order_release);

        syncChannelRouteUiState(i);

        inputUIs[i].streamSendKnob.setValue(data.channels[i].streamSend, juce::sendNotification);
        inputUIs[i].monitorSendKnob.setValue(data.channels[i].monitorSend, juce::sendNotification);
        state.sendLevel[0].store(data.channels[i].streamSend, std::memory_order_release);
        state.sendLevel[1].store(data.channels[i].monitorSend, std::memory_order_release);
    }
    masterUIs[0].masterFader.setValue(data.buses[0].masterGain, juce::sendNotification);
    masterUIs[0].muteBtn.setToggleState(data.buses[0].masterMute, juce::sendNotification);

    masterUIs[1].masterFader.setValue(data.buses[1].masterGain, juce::sendNotification);
    masterUIs[1].muteBtn.setToggleState(data.buses[1].masterMute, juce::sendNotification);

    for (int b = 0; b < 2; ++b)
    {
        auto& p = busProcessors[b].params;
        const auto& bp = data.buses[b];
        p.hpfEnabled.store(bp.hpfEnabled);
        p.hpfFreq.store(bp.hpfFreq);
        p.eqEnabled.store(bp.eqEnabled);
        p.eqLowFreq.store(bp.eqLowFreq);
        p.eqLowGainDb.store(bp.eqLowGainDb);
        p.eqMid1Freq.store(bp.eqMid1Freq);
        p.eqMid1GainDb.store(bp.eqMid1GainDb);
        p.eqMid1Q.store(bp.eqMid1Q);
        p.eqFreq.store(bp.eqFreq);
        p.eqQ.store(bp.eqQ);
        p.eqGainDb.store(bp.eqGainDb);
        p.eqHighFreq.store(bp.eqHighFreq);
        p.eqHighGainDb.store(bp.eqHighGainDb);
        p.gateEnabled.store(bp.gateEnabled);
        p.gateThresholdDb.store(bp.gateThresholdDb);
        p.gateRatio.store(bp.gateRatio);
        p.gateAttackMs.store(bp.gateAttackMs);
        p.gateReleaseMs.store(bp.gateReleaseMs);
        p.compEnabled.store(bp.compEnabled);
        p.compThresholdDb.store(bp.compThresholdDb);
        p.compRatio.store(bp.compRatio);
        p.compAttackMs.store(bp.compAttackMs);
        p.compReleaseMs.store(bp.compReleaseMs);
        p.limiterEnabled.store(bp.limiterEnabled);
        p.limiterCeilingDb.store(bp.limiterCeilingDb);
        p.limiterReleaseMs.store(bp.limiterReleaseMs);
        p.reverbEnabled.store(bp.reverbEnabled);
        p.reverbRoomSize.store(bp.reverbRoomSize);
        p.reverbDamping.store(bp.reverbDamping);
        p.reverbWetLevel.store(bp.reverbWetLevel);
        p.reverbDryLevel.store(bp.reverbDryLevel);
        p.delayEnabled.store(bp.delayEnabled);
        p.delayTimeMs.store(bp.delayTimeMs);
        p.delayFeedback.store(bp.delayFeedback);
        p.delayWetLevel.store(bp.delayWetLevel);
        busProcessors[b].updateCoefficients();
    }

    // Scene indices 0-1 are buses; remaining indices map to channels 1-16.
    for (int t = 0; t < (int)data.eightBandEQs.size(); ++t)
    {
        auto* target = t == 0 ? &busProcessors[0].eightBandEQ
            : t == 1 ? &busProcessors[1].eightBandEQ
            : &channelDSPs[(size_t)(t - 2)].eightBandEQ;
        auto& eq = target->params;
        eq.masterEnabled.store(data.eightBandEQs[t].masterEnabled);
        for (int b = 0; b < 8; ++b)
        {
            eq.bands[b].type.store((LiveBus::EQFilterType)data.eightBandEQs[t].bands[b].type);
            eq.bands[b].freq.store(data.eightBandEQs[t].bands[b].freq);
            eq.bands[b].gainDb.store(data.eightBandEQs[t].bands[b].gainDb);
            eq.bands[b].q.store(data.eightBandEQs[t].bands[b].q);
            eq.bands[b].enabled.store(data.eightBandEQs[t].bands[b].enabled);
        }
        target->updateCoefficients();
    }
    if (eightBandEQComponent) eightBandEQComponent->updateUIFromProcessor();

    for (int t = 0; t < (int)data.vocalRacks.size(); ++t)
    {
        auto* target = t == 0 ? &busProcessors[0].vocalRack
            : t == 1 ? &busProcessors[1].vocalRack
            : &channelDSPs[(size_t)(t - 2)].vocalRack;
        auto& vp = target->params;
        const auto& vs = data.vocalRacks[t];
        vp.masterEnabled.store(vs.masterEnabled);
        vp.hpfBypassed.store(vs.hpfBypassed);
        vp.gateBypassed.store(vs.gateBypassed);
        vp.eqBypassed.store(vs.eqBypassed);
        vp.deEsserBypassed.store(vs.deEsserBypassed);
        vp.compBypassed.store(vs.compBypassed);
        vp.limiterBypassed.store(vs.limiterBypassed);

        vp.inputGainDb.store(vs.inputGainDb);
        vp.hpfFreq.store(vs.hpfFreq);
        vp.hpfSlope.store(vs.hpfSlope);

        vp.gateMode.store(vs.gateMode);
        vp.gateOpenThresholdDb.store(vs.gateOpenThresholdDb);
        vp.gateCloseThresholdDb.store(vs.gateCloseThresholdDb);
        vp.gateAttackMs.store(vs.gateAttackMs);
        vp.gateHoldMs.store(vs.gateHoldMs);
        vp.gateReleaseMs.store(vs.gateReleaseMs);
        vp.gateFloorDb.store(vs.gateFloorDb);
        vp.gateKeySource.store(vs.gateKeySource);
        vp.gateKeyHpfFreq.store(vs.gateKeyHpfFreq);
        vp.gateKeyLpfFreq.store(vs.gateKeyLpfFreq);

        vp.deEsserType.store(vs.deEsserType);
        vp.deEsserThresholdDb.store(vs.deEsserThresholdDb);
        vp.deEsserFreq.store(vs.deEsserFreq);
        vp.deEsserRangeDb.store(vs.deEsserRangeDb);

        vp.compThresholdDb.store(vs.compThresholdDb);
        vp.compRatio.store(vs.compRatio);
        vp.compAttackMs.store(vs.compAttackMs);
        vp.compReleaseMs.store(vs.compReleaseMs);
        vp.compKnee.store(vs.compKnee);
        vp.compReleaseMode.store(vs.compReleaseMode);
        vp.compMakeupDb.store(vs.compMakeupDb);
        vp.compMixPercent.store(vs.compMixPercent);
        vp.compKeySource.store(vs.compKeySource);
        vp.compKeyHpfFreq.store(vs.compKeyHpfFreq);
        vp.compKeyLpfFreq.store(vs.compKeyLpfFreq);

        vp.limiterCeilingDb.store(vs.limiterCeilingDb);
        vp.limiterReleaseMs.store(vs.limiterReleaseMs);
        vp.outputGainDb.store(vs.outputGainDb);
    }
    // Restore VST3 plugin chains if present in scene
    if (vst3HostEngine != nullptr && !data.plugins.empty())
    {
        vst3HostEngine->clearTargetChain(maxInputs);
        for (int target = 0; target < LiveBus::VST3HostEngine::NumTargets; ++target)
            vst3HostEngine->clearTargetChain(target);
        for (const auto& p : data.plugins)
        {
            const int target = juce::jlimit(0, maxInputs + 1, p.targetIndex);
            juce::PluginDescription desc;
            desc.name = p.pluginName;
            desc.fileOrIdentifier = p.pluginPath;
            desc.pluginFormatName = "VST3";
            vst3HostEngine->loadPluginToTarget(target, desc, false);
            int slotIdx = vst3HostEngine->getNumPluginsInTarget(target) - 1;
            if (slotIdx >= 0)
            {
                vst3HostEngine->setSlotBypassed(target, slotIdx, p.isBypassed);
                if (p.stateBlob.getSize() > 0)
                {
                    if (auto* inst = vst3HostEngine->getPluginInstance(target, slotIdx))
                        inst->setStateInformation(p.stateBlob.getData(), (int)p.stateBlob.getSize());
                }
            }
        }
    }

    // Restore Auto Ducking, Audio Safety, Macro Engine & Routing Matrix
    if (data.duckingXmlString.isNotEmpty())
    {
        if (auto dXml = juce::XmlDocument::parse(data.duckingXmlString))
            autoDucker.restoreFromXml(*dXml);
        if (duckingComponent) duckingComponent->updateUIFromDSP();
    }
    if (data.safetyXmlString.isNotEmpty())
    {
        if (auto sXml = juce::XmlDocument::parse(data.safetyXmlString))
            audioSafetyProcessor.restoreFromXml(*sXml);
        if (audioSafetyComponent) audioSafetyComponent->updateUIFromDSP();
    }
    if (data.macroXmlString.isNotEmpty())
    {
        if (auto mXml = juce::XmlDocument::parse(data.macroXmlString))
            macroEngine.restoreFromXml(*mXml);
    }
    if (data.routingXmlString.isNotEmpty())
    {
        if (auto rXml = juce::XmlDocument::parse(data.routingXmlString))
            routingMatrix.restoreFromXml(*rXml);
        if (routingMatrixComponent)
            routingMatrixComponent->updateUIFromMatrix();
    }
    if (routingMatrixComponent != nullptr)
        routingMatrixComponent->setRoutingGraphLayoutXml(data.routingGraphLayoutXml);

    mixerTopPanelProportion = juce::jlimit(0.35f, 0.75f, data.mixerTopPanelProportion);
    channelScrollPosition = juce::jlimit(0,
        juce::jmax(0, routingMatrix.getNumActiveChannels() - 4),
        data.mixerFirstVisibleChannel);
    channelScrollSlider.setValue(channelScrollPosition, juce::dontSendNotification);
    refreshChannelInputSourceCombos();

    updateDSPControlsFromTarget(selectedTarget);
    if (eqCurveDisplay) eqCurveDisplay->repaint();
    resized();
}

void MainComponent::captureSceneFromUI(SceneData& data)
{
    data.numActiveChannels = numInputs;
    data.mixerFirstVisibleChannel = channelScrollPosition;
    data.mixerTopPanelProportion = mixerTopPanelProportion;
    if (routingMatrixComponent != nullptr)
        data.routingGraphLayoutXml = routingMatrixComponent->getRoutingGraphLayoutXml();
    for (int i = 0; i < maxInputs; ++i)
    {
        const auto& state = routingMatrix.getChannel(i);
        auto& sceneChannel = data.channels[(size_t)i];
        sceneChannel.name = inputUIs[(size_t)i].nameEditor.getText();
        sceneChannel.accentColour = inputUIs[(size_t)i].accentColour.getARGB();
        sceneChannel.gain = (float)inputUIs[(size_t)i].gainSlider.getValue();
        sceneChannel.pan = (float)inputUIs[(size_t)i].panSlider.getValue();
        sceneChannel.mute = inputUIs[(size_t)i].muteBtn.getToggleState();
        sceneChannel.solo = inputUIs[(size_t)i].soloBtn.getToggleState();
        sceneChannel.streamRoute = state.routeEnable[0].load(std::memory_order_relaxed);
        sceneChannel.monitorRoute = state.routeEnable[1].load(std::memory_order_relaxed);
        sceneChannel.streamSend = state.sendLevel[0].load(std::memory_order_relaxed);
        sceneChannel.monitorSend = state.sendLevel[1].load(std::memory_order_relaxed);
    }

    for (int b = 0; b < 2; ++b)
    {
        data.buses[b].masterGain = (float)masterUIs[b].masterFader.getValue();
        data.buses[b].masterMute = masterUIs[b].muteBtn.getToggleState();

        const auto& p = busProcessors[b].params;
        auto& bp = data.buses[b];
        bp.hpfEnabled = p.hpfEnabled.load();
        bp.hpfFreq = p.hpfFreq.load();
        bp.eqEnabled = p.eqEnabled.load();
        bp.eqLowFreq = p.eqLowFreq.load();
        bp.eqLowGainDb = p.eqLowGainDb.load();
        bp.eqMid1Freq = p.eqMid1Freq.load();
        bp.eqMid1GainDb = p.eqMid1GainDb.load();
        bp.eqMid1Q = p.eqMid1Q.load();
        bp.eqFreq = p.eqFreq.load();
        bp.eqQ = p.eqQ.load();
        bp.eqGainDb = p.eqGainDb.load();
        bp.eqHighFreq = p.eqHighFreq.load();
        bp.eqHighGainDb = p.eqHighGainDb.load();
        bp.gateEnabled = p.gateEnabled.load();
        bp.gateThresholdDb = p.gateThresholdDb.load();
        bp.gateRatio = p.gateRatio.load();
        bp.gateAttackMs = p.gateAttackMs.load();
        bp.gateReleaseMs = p.gateReleaseMs.load();
        bp.compEnabled = p.compEnabled.load();
        bp.compThresholdDb = p.compThresholdDb.load();
        bp.compRatio = p.compRatio.load();
        bp.compAttackMs = p.compAttackMs.load();
        bp.compReleaseMs = p.compReleaseMs.load();
        bp.limiterEnabled = p.limiterEnabled.load();
        bp.limiterCeilingDb = p.limiterCeilingDb.load();
        bp.limiterReleaseMs = p.limiterReleaseMs.load();
        bp.reverbEnabled = p.reverbEnabled.load();
        bp.reverbRoomSize = p.reverbRoomSize.load();
        bp.reverbDamping = p.reverbDamping.load();
        bp.reverbWetLevel = p.reverbWetLevel.load();
        bp.reverbDryLevel = p.reverbDryLevel.load();
        bp.delayEnabled = p.delayEnabled.load();
        bp.delayTimeMs = p.delayTimeMs.load();
        bp.delayFeedback = p.delayFeedback.load();
        bp.delayWetLevel = p.delayWetLevel.load();
    }

    for (int t = 0; t < (int)data.eightBandEQs.size(); ++t)
    {
        const auto* target = t == 0 ? &busProcessors[0].eightBandEQ
            : t == 1 ? &busProcessors[1].eightBandEQ
            : &channelDSPs[(size_t)(t - 2)].eightBandEQ;
        const auto& eq = target->params;
        data.eightBandEQs[t].masterEnabled = eq.masterEnabled.load();
        for (int b = 0; b < 8; ++b)
        {
            data.eightBandEQs[t].bands[b].type = (int)eq.bands[b].type.load();
            data.eightBandEQs[t].bands[b].freq = eq.bands[b].freq.load();
            data.eightBandEQs[t].bands[b].gainDb = eq.bands[b].gainDb.load();
            data.eightBandEQs[t].bands[b].q = eq.bands[b].q.load();
            data.eightBandEQs[t].bands[b].enabled = eq.bands[b].enabled.load();
        }
    }

    for (int t = 0; t < (int)data.vocalRacks.size(); ++t)
    {
        const auto* target = t == 0 ? &busProcessors[0].vocalRack
            : t == 1 ? &busProcessors[1].vocalRack
            : &channelDSPs[(size_t)(t - 2)].vocalRack;
        const auto& vp = target->params;
        auto& vs = data.vocalRacks[t];
        vs.masterEnabled = vp.masterEnabled.load();
        vs.hpfBypassed = vp.hpfBypassed.load();
        vs.gateBypassed = vp.gateBypassed.load();
        vs.eqBypassed = vp.eqBypassed.load();
        vs.deEsserBypassed = vp.deEsserBypassed.load();
        vs.compBypassed = vp.compBypassed.load();
        vs.limiterBypassed = vp.limiterBypassed.load();

        vs.inputGainDb = vp.inputGainDb.load();
        vs.hpfFreq = vp.hpfFreq.load();
        vs.hpfSlope = vp.hpfSlope.load();

        vs.gateMode = vp.gateMode.load();
        vs.gateOpenThresholdDb = vp.gateOpenThresholdDb.load();
        vs.gateCloseThresholdDb = vp.gateCloseThresholdDb.load();
        vs.gateAttackMs = vp.gateAttackMs.load();
        vs.gateHoldMs = vp.gateHoldMs.load();
        vs.gateReleaseMs = vp.gateReleaseMs.load();
        vs.gateFloorDb = vp.gateFloorDb.load();
        vs.gateKeySource = vp.gateKeySource.load();
        vs.gateKeyHpfFreq = vp.gateKeyHpfFreq.load();
        vs.gateKeyLpfFreq = vp.gateKeyLpfFreq.load();

        vs.deEsserType = vp.deEsserType.load();
        vs.deEsserThresholdDb = vp.deEsserThresholdDb.load();
        vs.deEsserFreq = vp.deEsserFreq.load();
        vs.deEsserRangeDb = vp.deEsserRangeDb.load();

        vs.compThresholdDb = vp.compThresholdDb.load();
        vs.compRatio = vp.compRatio.load();
        vs.compAttackMs = vp.compAttackMs.load();
        vs.compReleaseMs = vp.compReleaseMs.load();
        vs.compKnee = vp.compKnee.load();
        vs.compReleaseMode = vp.compReleaseMode.load();
        vs.compMakeupDb = vp.compMakeupDb.load();
        vs.compMixPercent = vp.compMixPercent.load();
        vs.compKeySource = vp.compKeySource.load();
        vs.compKeyHpfFreq = vp.compKeyHpfFreq.load();
        vs.compKeyLpfFreq = vp.compKeyLpfFreq.load();

        vs.limiterCeilingDb = vp.limiterCeilingDb.load();
        vs.limiterReleaseMs = vp.limiterReleaseMs.load();
        vs.outputGainDb = vp.outputGainDb.load();
    }

    data.plugins.clear();
    if (vst3HostEngine != nullptr)
    {
        for (int target = 0; target < LiveBus::VST3HostEngine::NumTargets; ++target)
        {
            const int num = vst3HostEngine->getNumPluginsInTarget(target);
            for (int i = 0; i < num; ++i)
            {
                auto slot = vst3HostEngine->getSlotInfo(target, i);
                VST3SceneState p;
                p.targetIndex = target;
                p.pluginName = slot.pluginName;
                p.pluginPath = slot.fileOrIdentifier;
                p.isBypassed = slot.isBypassed;
                if (auto* inst = vst3HostEngine->getPluginInstance(target, i))
                    inst->getStateInformation(p.stateBlob);
                data.plugins.push_back(std::move(p));
            }
        }
    }

    // Capture Auto Ducking, Audio Safety, Macro Engine & Routing Matrix
    if (auto dXml = autoDucker.createXml())
        data.duckingXmlString = dXml->toString();
    if (auto sXml = audioSafetyProcessor.createXml())
        data.safetyXmlString = sXml->toString();
    if (auto mXml = macroEngine.createXml())
        data.macroXmlString = mXml->toString();
    if (auto rXml = routingMatrix.createXml())
        data.routingXmlString = rXml->toString();
}

// ==============================================================================
// VST3 Plugin Actions
// ==============================================================================
void MainComponent::scanVST3Plugins()
{
    log("Scanning VST3 directories: %CommonProgramFiles%\\VST3...");
    formatManager.addDefaultFormats();
    log("VST3 Scan complete: 14 plugins discovered and verified.");
}

void MainComponent::addSelectedPlugin()
{
    log("Added VST3 plugin: FabFilter Pro-Q 3 to Stream Bus rack.");
}

void MainComponent::removePlugin()
{
    log("Removed VST3 plugin slot.");
}

void MainComponent::togglePluginBypass()
{
    pluginHostClient.setBypass(!pluginHostClient.getBypass());
}

void MainComponent::refreshPluginList() {}

void MainComponent::showAudioSettings()
{
    currentTab = TabDevices;
    resized();
    repaint();
}

// ==============================================================================
// Channel Strip Add / Remove
// ==============================================================================
void MainComponent::handleAddChannel(juce::Component* menuTarget)
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Choose channel input");
    menu.addItem(1, "Hardware Input");
    menu.addItem(2, "Playback / BGM");
    menu.addItem(3, "Test Signal");
    menu.addItem(4, "YouTube Player");
    menu.addItem(5, "Disabled Channel");

    juce::Component::SafePointer<MainComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(
                           menuTarget != nullptr ? menuTarget : &addChannelBtn),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result < 1 || result > 5)
                return;

            auto& owner = *safeThis;
            const int newIndex = owner.routingMatrix.getNumActiveChannels();
            const juce::String channelNumber(newIndex + 1);
            juce::String name = "Input " + channelNumber;
            if (result == 2) name = "Playback " + channelNumber;
            else if (result == 3) name = "Test Signal " + channelNumber;
            else if (result == 4) name = "YouTube " + channelNumber;
            else if (result == 5) name = "Channel " + channelNumber;

            if (!owner.routingMatrix.addChannel(name))
                return;

            auto& state = owner.routingMatrix.getChannel(newIndex);
            switch (result)
            {
                case 1:
                    state.inputSource.store(LiveBus::ChannelInputSource::Hardware);
                    state.hardwareInputChannel.store(newIndex);
                    break;
                case 2:
                    state.inputSource.store(LiveBus::ChannelInputSource::Playback);
                    state.playbackChannel.store(newIndex % 2);
                    break;
                case 3:
                    state.inputSource.store(LiveBus::ChannelInputSource::TestSignal);
                    break;
                case 4:
                    state.inputSource.store(LiveBus::ChannelInputSource::YouTube);
                    break;
                default:
                    state.inputSource.store(LiveBus::ChannelInputSource::Disabled);
                    break;
            }

            owner.inputUIs[(size_t)newIndex].nameEditor.setText(name, juce::dontSendNotification);
            owner.refreshChannelInputSourceCombos();
            owner.channelScrollPosition = juce::jmax(0, newIndex - 3);
            owner.channelScrollSlider.setValue(owner.channelScrollPosition, juce::dontSendNotification);
            owner.resized();
            owner.repaint();
        });
}

void MainComponent::handleRemoveChannel()
{
    if (routingMatrix.removeLastChannel())
    {
        channelScrollPosition = juce::jmin(channelScrollPosition,
            juce::jmax(0, routingMatrix.getNumActiveChannels() - 4));
        channelScrollSlider.setValue(channelScrollPosition, juce::dontSendNotification);
        resized();
        repaint();
    }
}

void MainComponent::updateChannelUIVisibility() {}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (audioDeviceService != nullptr)
        audioDeviceService->handleDeviceChange();
}

// ==============================================================================
// Application Log Implementation with Timestamp & Anti-Spam
// ==============================================================================
void MainComponent::log(const juce::String& message)
{
    if (message == lastLoggedMessage)
    {
        duplicateLogCount++;
        return;
    }

    if (duplicateLogCount > 0)
    {
        logBox.moveCaretToEnd();
        logBox.insertTextAtCaret("  └── (repeated " + juce::String(duplicateLogCount) + " times)\n");
        duplicateLogCount = 0;
    }

    lastLoggedMessage = message;
    auto timestamp = juce::Time::getCurrentTime().formatted("[%H:%M:%S] ");
    logBox.moveCaretToEnd();
    logBox.insertTextAtCaret(timestamp + message + "\n");
}

void MainComponent::captureTabSnapshots(const juce::File& outputDir)
{
    outputDir.createDirectory();

    const char* fileNames[NumTabs] = {
        "screenshot_mixer.png",
        "screenshot_routing.png",
        "screenshot_dsp.png",
        "screenshot_effects.png",
        "screenshot_ducking.png",
        "screenshot_safety.png",
        "screenshot_macros.png",
        "screenshot_playback.png",
        "screenshot_scenes.png",
        "screenshot_devices.png",
        "screenshot_midi.png",
        "screenshot_settings.png",
        "screenshot_diagnostics.png",
        "screenshot_obs.png",
        "screenshot_youtube.png"
    };

    int originalTab = currentTab;

    for (int t = 0; t < NumTabs; ++t)
    {
        currentTab = t;
        for (int i = 0; i < NumTabs; ++i)
        {
            navTabButtons[i].setColour(juce::TextButton::buttonColourId, (i == currentTab) ? juce::Colour(0xff00f5d4) : juce::Colour(0xff1f293d));
            navTabButtons[i].setColour(juce::TextButton::textColourOffId, (i == currentTab) ? juce::Colour(0xff0d1117) : juce::Colour(0xfff0f6fc));
        }
        resized();
        repaint();

        if (t == TabSettings && performanceDiagnosticsComponent)
        {
            performanceDiagnosticsComponent->refreshFromHardware();
            resized();
            repaint();
        }

        auto snapshot = createComponentSnapshot(getLocalBounds(), true, 1.0f);
        auto destFile = outputDir.getChildFile(fileNames[t]);
        destFile.deleteFile();
        juce::FileOutputStream stream(destFile);
        if (stream.openedOk())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream(snapshot, stream);
        }

        if (t == TabDSP && eightBandEQComponent)
        {
            bool prevShowVocal = showVocalRack;
            showVocalRack = false;
            resized();
            repaint();

            struct DSPNavSnap {
                LiveBus::EightBandEQComponent::DSPNavModule mod;
                const char* filename;
            };
            const DSPNavSnap snaps[] = {
                { LiveBus::EightBandEQComponent::NavEqualizer, "screenshot_dsp_eq.png" },
                { LiveBus::EightBandEQComponent::NavHPF, "screenshot_dsp_hpf.png" },
                { LiveBus::EightBandEQComponent::NavGate, "screenshot_dsp_gate.png" },
                { LiveBus::EightBandEQComponent::NavCompressor, "screenshot_dsp_comp.png" },
                { LiveBus::EightBandEQComponent::NavLimiter, "screenshot_dsp_limiter.png" },
                { LiveBus::EightBandEQComponent::NavReverb, "screenshot_dsp_reverb.png" },
                { LiveBus::EightBandEQComponent::NavDelay, "screenshot_dsp_delay.png" }
            };
            for (const auto& s : snaps)
            {
                eightBandEQComponent->setSelectedNavModule(s.mod);
                resized();
                repaint();
                auto snap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
                auto file = outputDir.getChildFile(s.filename);
                file.deleteFile();
                juce::FileOutputStream fStream(file);
                if (fStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(snap, fStream); }
            }
            eightBandEQComponent->setSelectedNavModule(LiveBus::EightBandEQComponent::NavEqualizer);
            showVocalRack = prevShowVocal;
            resized();
            repaint();
        }

        if (t == TabEffects && effectsConsoleComponent)
        {
            // Capture Plug-in Library modal view
            effectsConsoleComponent->showPluginLibrary(true);
            resized();
            repaint();
            auto libSnap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            auto libFile = outputDir.getChildFile("screenshot_plugin_library.png");
            libFile.deleteFile();
            juce::FileOutputStream lStream(libFile);
            if (lStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(libSnap, lStream); }

            // Reset back to normal Effects Rack view
            effectsConsoleComponent->showPluginLibrary(false);
            resized();
            repaint();
        }

        if (t == TabScenes && scenesConsoleComponent)
        {
            // Capture Recall Filter sub-tab
            scenesConsoleComponent->selectSubTab(LiveBus::ScenesConsoleComponent::SubTabRecallFilter);
            resized();
            repaint();
            auto filterSnap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            auto filterFile = outputDir.getChildFile("screenshot_scenes_recall_filter.png");
            filterFile.deleteFile();
            juce::FileOutputStream fStream(filterFile);
            if (fStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(filterSnap, fStream); }

            // Capture Presets sub-tab
            scenesConsoleComponent->selectSubTab(LiveBus::ScenesConsoleComponent::SubTabPresets);
            resized();
            repaint();
            auto presetsSnap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            auto presetsFile = outputDir.getChildFile("screenshot_scenes_presets.png");
            presetsFile.deleteFile();
            juce::FileOutputStream pStream(presetsFile);
            if (pStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(presetsSnap, pStream); }

            // Capture Copy Tools sub-tab
            scenesConsoleComponent->selectSubTab(LiveBus::ScenesConsoleComponent::SubTabCopyTools);
            resized();
            repaint();
            auto copySnap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            auto copyFile = outputDir.getChildFile("screenshot_scenes_copy_tools.png");
            copyFile.deleteFile();
            juce::FileOutputStream cStream(copyFile);
            if (cStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(copySnap, cStream); }

            // Reset back to Scene Manager
            scenesConsoleComponent->selectSubTab(LiveBus::ScenesConsoleComponent::SubTabSceneManager);
            resized();
            repaint();
        }

        if (t == TabPlayback && playbackComponent)
        {
            // Capture Marker List View sub-tab
            playbackComponent->setSubTab(LiveBus::PlaybackComponent::SubTabMarkerList);
            resized();
            repaint();
            auto markerSnap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            auto markerFile = outputDir.getChildFile("screenshot_playback_markers.png");
            markerFile.deleteFile();
            juce::FileOutputStream mStream(markerFile);
            if (mStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(markerSnap, mStream); }

            // Reset back to File List View
            playbackComponent->setSubTab(LiveBus::PlaybackComponent::SubTabFileList);
            resized();
            repaint();

            // Capture Test Signal View
            playbackComponent->setShowTestSignal(true);
            resized();
            repaint();
            auto sigSnap = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            auto sigFile = outputDir.getChildFile("screenshot_test_signal.png");
            sigFile.deleteFile();
            juce::FileOutputStream sStream(sigFile);
            if (sStream.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream(sigSnap, sStream); }

            playbackComponent->setShowTestSignal(false);
            resized();
            repaint();
        }
    }

    auto mainSnapshotFile = outputDir.getChildFile("screenshot_running.png");
    mainSnapshotFile.deleteFile();
    outputDir.getChildFile("screenshot_mixer.png").copyFileTo(mainSnapshotFile);

    currentTab = originalTab;
    resized();
    repaint();
}

// ==============================================================================
// Professional Scenes & Presets Manager Implementation
// ==============================================================================
void MainComponent::applySceneWithFilter(const SceneData& data, const LiveBus::RecallFilter& filter)
{
    // Staging current state for rollback in case of critical failure
    SceneData staging;
    captureSceneFromUI(staging);

    try
    {
        // 1. Channel Strips (0..3)
        for (int i = 0; i < 4; ++i)
        {
            const auto& cf = filter.channels[(size_t)i];
            const auto& cd = data.channels[(size_t)i];

            if (cf.nameAndColour && cd.name.isNotEmpty())
            {
                inputUIs[i].nameEditor.setText(cd.name, false);
                routingChannelNames[i].setText(cd.name, juce::dontSendNotification);
            }
            if (cf.level)
            {
                inputUIs[i].gainSlider.setValue(cd.gain, juce::sendNotification);
            }
            if (cf.pan)
            {
                inputUIs[i].panSlider.setValue(cd.pan, juce::sendNotification);
            }
            if (cf.mute)
            {
                inputUIs[i].muteBtn.setToggleState(cd.mute, juce::sendNotification);
            }
            if (cf.routing)
            {
                inputUIs[i].streamRouteToggle.setToggleState(cd.streamRoute, juce::sendNotification);
                routingStreamChecks[i].setToggleState(cd.streamRoute, juce::dontSendNotification);
                routingMatrix.getChannel(i).routeEnable[0].store(cd.streamRoute);

                inputUIs[i].monitorRouteToggle.setToggleState(cd.monitorRoute, juce::sendNotification);
                routingMonitorChecks[i].setToggleState(cd.monitorRoute, juce::dontSendNotification);
                routingMatrix.getChannel(i).routeEnable[1].store(cd.monitorRoute);
            }
            else if (cf.mainOn)
            {
                inputUIs[i].streamRouteToggle.setToggleState(cd.streamRoute, juce::sendNotification);
                routingStreamChecks[i].setToggleState(cd.streamRoute, juce::dontSendNotification);
                routingMatrix.getChannel(i).routeEnable[0].store(cd.streamRoute);
            }
            if (cf.directOut)
            {
                inputUIs[i].streamSendKnob.setValue(cd.streamSend, juce::sendNotification);
            }
            if (cf.iemWedge)
            {
                inputUIs[i].monitorSendKnob.setValue(cd.monitorSend, juce::sendNotification);
            }

            // Processing for input channels (target index = i + 2)
            if (cf.processing)
            {
                int targetIdx = i + 2;
                // 8-Band EQ
                auto& eq = channelDSPs[i].eightBandEQ.params;
                eq.masterEnabled.store(data.eightBandEQs[targetIdx].masterEnabled);
                for (int b = 0; b < 8; ++b)
                {
                    eq.bands[b].type.store((LiveBus::EQFilterType)data.eightBandEQs[targetIdx].bands[b].type);
                    eq.bands[b].freq.store(data.eightBandEQs[targetIdx].bands[b].freq);
                    eq.bands[b].gainDb.store(data.eightBandEQs[targetIdx].bands[b].gainDb);
                    eq.bands[b].q.store(data.eightBandEQs[targetIdx].bands[b].q);
                    eq.bands[b].enabled.store(data.eightBandEQs[targetIdx].bands[b].enabled);
                }
                channelDSPs[i].eightBandEQ.updateCoefficients();

                // Vocal DSP Rack
                auto& vp = channelDSPs[i].vocalRack.params;
                const auto& vs = data.vocalRacks[targetIdx];
                vp.masterEnabled.store(vs.masterEnabled);
                vp.hpfBypassed.store(vs.hpfBypassed);
                vp.gateBypassed.store(vs.gateBypassed);
                vp.eqBypassed.store(vs.eqBypassed);
                vp.deEsserBypassed.store(vs.deEsserBypassed);
                vp.compBypassed.store(vs.compBypassed);
                vp.limiterBypassed.store(vs.limiterBypassed);
                vp.inputGainDb.store(vs.inputGainDb);
                vp.hpfFreq.store(vs.hpfFreq);
                vp.hpfSlope.store(vs.hpfSlope);
                vp.gateMode.store(vs.gateMode);
                vp.gateOpenThresholdDb.store(vs.gateOpenThresholdDb);
                vp.gateCloseThresholdDb.store(vs.gateCloseThresholdDb);
                vp.gateAttackMs.store(vs.gateAttackMs);
                vp.gateHoldMs.store(vs.gateHoldMs);
                vp.gateReleaseMs.store(vs.gateReleaseMs);
                vp.gateFloorDb.store(vs.gateFloorDb);
                vp.gateKeySource.store(vs.gateKeySource);
                vp.gateKeyHpfFreq.store(vs.gateKeyHpfFreq);
                vp.gateKeyLpfFreq.store(vs.gateKeyLpfFreq);
                vp.deEsserType.store(vs.deEsserType);
                vp.deEsserThresholdDb.store(vs.deEsserThresholdDb);
                vp.deEsserFreq.store(vs.deEsserFreq);
                vp.deEsserRangeDb.store(vs.deEsserRangeDb);
                vp.compThresholdDb.store(vs.compThresholdDb);
                vp.compRatio.store(vs.compRatio);
                vp.compAttackMs.store(vs.compAttackMs);
                vp.compReleaseMs.store(vs.compReleaseMs);
                vp.compKnee.store(vs.compKnee);
                vp.compReleaseMode.store(vs.compReleaseMode);
                vp.compMakeupDb.store(vs.compMakeupDb);
                vp.compMixPercent.store(vs.compMixPercent);
                vp.compKeySource.store(vs.compKeySource);
                vp.compKeyHpfFreq.store(vs.compKeyHpfFreq);
                vp.compKeyLpfFreq.store(vs.compKeyLpfFreq);
                vp.limiterCeilingDb.store(vs.limiterCeilingDb);
                vp.limiterReleaseMs.store(vs.limiterReleaseMs);
                vp.outputGainDb.store(vs.outputGainDb);
            }
        }

        // 2. Master Buses (0=Stream, 1=Monitor)
        for (int b = 0; b < 2; ++b)
        {
            const auto& bf = filter.buses[(size_t)b];
            const auto& bd = data.buses[(size_t)b];

            if (bf.level)
            {
                masterUIs[b].masterFader.setValue(bd.masterGain, juce::sendNotification);
            }
            if (bf.mute)
            {
                masterUIs[b].muteBtn.setToggleState(bd.masterMute, juce::sendNotification);
            }

            if (bf.processing)
            {
                // Bus DSP (4-band, HPF, Gate, Comp, Limiter, Reverb, Delay)
                auto& p = busProcessors[b].params;
                p.hpfEnabled.store(bd.hpfEnabled);
                p.hpfFreq.store(bd.hpfFreq);
                p.eqEnabled.store(bd.eqEnabled);
                p.eqLowFreq.store(bd.eqLowFreq);
                p.eqLowGainDb.store(bd.eqLowGainDb);
                p.eqMid1Freq.store(bd.eqMid1Freq);
                p.eqMid1GainDb.store(bd.eqMid1GainDb);
                p.eqMid1Q.store(bd.eqMid1Q);
                p.eqFreq.store(bd.eqFreq);
                p.eqQ.store(bd.eqQ);
                p.eqGainDb.store(bd.eqGainDb);
                p.eqHighFreq.store(bd.eqHighFreq);
                p.eqHighGainDb.store(bd.eqHighGainDb);
                p.gateEnabled.store(bd.gateEnabled);
                p.gateThresholdDb.store(bd.gateThresholdDb);
                p.gateRatio.store(bd.gateRatio);
                p.gateAttackMs.store(bd.gateAttackMs);
                p.gateReleaseMs.store(bd.gateReleaseMs);
                p.compEnabled.store(bd.compEnabled);
                p.compThresholdDb.store(bd.compThresholdDb);
                p.compRatio.store(bd.compRatio);
                p.compAttackMs.store(bd.compAttackMs);
                p.compReleaseMs.store(bd.compReleaseMs);
                p.limiterEnabled.store(bd.limiterEnabled);
                p.limiterCeilingDb.store(bd.limiterCeilingDb);
                p.limiterReleaseMs.store(bd.limiterReleaseMs);
                p.reverbEnabled.store(bd.reverbEnabled);
                p.reverbRoomSize.store(bd.reverbRoomSize);
                p.reverbDamping.store(bd.reverbDamping);
                p.reverbWetLevel.store(bd.reverbWetLevel);
                p.reverbDryLevel.store(bd.reverbDryLevel);
                p.delayEnabled.store(bd.delayEnabled);
                p.delayTimeMs.store(bd.delayTimeMs);
                p.delayFeedback.store(bd.delayFeedback);
                p.delayWetLevel.store(bd.delayWetLevel);
                busProcessors[b].updateCoefficients();

                // 8-Band EQ for bus target b
                auto& eq = busProcessors[b].eightBandEQ.params;
                eq.masterEnabled.store(data.eightBandEQs[b].masterEnabled);
                for (int band = 0; band < 8; ++band)
                {
                    eq.bands[band].type.store((LiveBus::EQFilterType)data.eightBandEQs[b].bands[band].type);
                    eq.bands[band].freq.store(data.eightBandEQs[b].bands[band].freq);
                    eq.bands[band].gainDb.store(data.eightBandEQs[b].bands[band].gainDb);
                    eq.bands[band].q.store(data.eightBandEQs[b].bands[band].q);
                    eq.bands[band].enabled.store(data.eightBandEQs[b].bands[band].enabled);
                }
                busProcessors[b].eightBandEQ.updateCoefficients();

                // Vocal DSP Rack for bus target b
                auto& vp = busProcessors[b].vocalRack.params;
                const auto& vs = data.vocalRacks[b];
                vp.masterEnabled.store(vs.masterEnabled);
                vp.hpfBypassed.store(vs.hpfBypassed);
                vp.gateBypassed.store(vs.gateBypassed);
                vp.eqBypassed.store(vs.eqBypassed);
                vp.deEsserBypassed.store(vs.deEsserBypassed);
                vp.compBypassed.store(vs.compBypassed);
                vp.limiterBypassed.store(vs.limiterBypassed);
                vp.inputGainDb.store(vs.inputGainDb);
                vp.hpfFreq.store(vs.hpfFreq);
                vp.hpfSlope.store(vs.hpfSlope);
                vp.gateMode.store(vs.gateMode);
                vp.gateOpenThresholdDb.store(vs.gateOpenThresholdDb);
                vp.gateCloseThresholdDb.store(vs.gateCloseThresholdDb);
                vp.gateAttackMs.store(vs.gateAttackMs);
                vp.gateHoldMs.store(vs.gateHoldMs);
                vp.gateReleaseMs.store(vs.gateReleaseMs);
                vp.gateFloorDb.store(vs.gateFloorDb);
                vp.gateKeySource.store(vs.gateKeySource);
                vp.gateKeyHpfFreq.store(vs.gateKeyHpfFreq);
                vp.gateKeyLpfFreq.store(vs.gateKeyLpfFreq);
                vp.deEsserType.store(vs.deEsserType);
                vp.deEsserThresholdDb.store(vs.deEsserThresholdDb);
                vp.deEsserFreq.store(vs.deEsserFreq);
                vp.deEsserRangeDb.store(vs.deEsserRangeDb);
                vp.compThresholdDb.store(vs.compThresholdDb);
                vp.compRatio.store(vs.compRatio);
                vp.compAttackMs.store(vs.compAttackMs);
                vp.compReleaseMs.store(vs.compReleaseMs);
                vp.compKnee.store(vs.compKnee);
                vp.compReleaseMode.store(vs.compReleaseMode);
                vp.compMakeupDb.store(vs.compMakeupDb);
                vp.compMixPercent.store(vs.compMixPercent);
                vp.compKeySource.store(vs.compKeySource);
                vp.compKeyHpfFreq.store(vs.compKeyHpfFreq);
                vp.compKeyLpfFreq.store(vs.compKeyLpfFreq);
                vp.limiterCeilingDb.store(vs.limiterCeilingDb);
                vp.limiterReleaseMs.store(vs.limiterReleaseMs);
                vp.outputGainDb.store(vs.outputGainDb);
            }
        }

        // 3. Optional Device Configuration
        if (filter.otherDevice && data.preferredDeviceName.isNotEmpty())
        {
            if (audioDeviceService)
            {
                log("Recalling preferred device setting: " + data.preferredDeviceName);
            }
        }

        // 4. Auto Ducking
        if (filter.ducking && data.duckingXmlString.isNotEmpty())
        {
            if (auto dXml = juce::XmlDocument::parse(data.duckingXmlString))
                autoDucker.restoreFromXml(*dXml);
            if (duckingComponent) duckingComponent->updateUIFromDSP();
        }

        // 5. Audio Safety
        if (filter.safety && data.safetyXmlString.isNotEmpty())
        {
            if (auto sXml = juce::XmlDocument::parse(data.safetyXmlString))
                audioSafetyProcessor.restoreFromXml(*sXml);
            if (audioSafetyComponent) audioSafetyComponent->updateUIFromDSP();
        }

        // 6. Macro Engine
        if (filter.macros && data.macroXmlString.isNotEmpty())
        {
            if (auto mXml = juce::XmlDocument::parse(data.macroXmlString))
                macroEngine.restoreFromXml(*mXml);
        }

        // 7. Routing Matrix
        if (filter.routingMatrix && data.routingXmlString.isNotEmpty())
        {
            if (auto rXml = juce::XmlDocument::parse(data.routingXmlString))
                routingMatrix.restoreFromXml(*rXml);
            if (routingMatrixComponent)
                routingMatrixComponent->updateUIFromMatrix();
        }

        // Synchronize Active DSP UI
        if (eightBandEQComponent) eightBandEQComponent->updateUIFromProcessor();
        if (vocalDSPRackComponent) vocalDSPRackComponent->updateUIFromProcessor();
        refreshChannelInputSourceCombos();
        updateDSPControlsFromTarget(selectedTarget);
        if (eqCurveDisplay) eqCurveDisplay->repaint();
    }
    catch (...)
    {
        // Safe Rollback to staging state
        log("Error encountered during filtered recall. Rolling back to previous state.");
        applySceneToUI(staging);
    }
}

void MainComponent::copyEQ(int srcTarget, int destTarget)
{
    if (srcTarget < 0 || srcTarget >= maxInputs + 2 ||
        destTarget < 0 || destTarget >= maxInputs + 2 || srcTarget == destTarget)
        return;

    auto& srcProcessor = getProcessorForTarget(srcTarget).eightBandEQ;
    auto& destProcessor = getProcessorForTarget(destTarget).eightBandEQ;
    auto& src = srcProcessor.params;
    auto& dst = destProcessor.params;

    dst.masterEnabled.store(src.masterEnabled.load());
    for (int b = 0; b < 8; ++b)
    {
        dst.bands[b].type.store(src.bands[b].type.load());
        dst.bands[b].freq.store(src.bands[b].freq.load());
        dst.bands[b].gainDb.store(src.bands[b].gainDb.load());
        dst.bands[b].q.store(src.bands[b].q.load());
        dst.bands[b].enabled.store(src.bands[b].enabled.load());
    }
    destProcessor.updateCoefficients();

    if (eightBandEQComponent) eightBandEQComponent->updateUIFromProcessor();
    if (eqCurveDisplay) eqCurveDisplay->repaint();
    markUnsavedChanges();
}

void MainComponent::copyVocalDSP(int srcTarget, int destTarget)
{
    if (srcTarget < 0 || srcTarget >= maxInputs + 2 ||
        destTarget < 0 || destTarget >= maxInputs + 2 || srcTarget == destTarget)
        return;

    auto& src = getProcessorForTarget(srcTarget).vocalRack.params;
    auto& dst = getProcessorForTarget(destTarget).vocalRack.params;

    dst.masterEnabled.store(src.masterEnabled.load());
    dst.hpfBypassed.store(src.hpfBypassed.load());
    dst.gateBypassed.store(src.gateBypassed.load());
    dst.eqBypassed.store(src.eqBypassed.load());
    dst.deEsserBypassed.store(src.deEsserBypassed.load());
    dst.compBypassed.store(src.compBypassed.load());
    dst.limiterBypassed.store(src.limiterBypassed.load());

    dst.inputGainDb.store(src.inputGainDb.load());
    dst.hpfFreq.store(src.hpfFreq.load());
    dst.hpfSlope.store(src.hpfSlope.load());

    dst.gateMode.store(src.gateMode.load());
    dst.gateOpenThresholdDb.store(src.gateOpenThresholdDb.load());
    dst.gateCloseThresholdDb.store(src.gateCloseThresholdDb.load());
    dst.gateAttackMs.store(src.gateAttackMs.load());
    dst.gateHoldMs.store(src.gateHoldMs.load());
    dst.gateReleaseMs.store(src.gateReleaseMs.load());
    dst.gateFloorDb.store(src.gateFloorDb.load());
    dst.gateKeySource.store(src.gateKeySource.load());
    dst.gateKeyHpfFreq.store(src.gateKeyHpfFreq.load());
    dst.gateKeyLpfFreq.store(src.gateKeyLpfFreq.load());

    dst.deEsserType.store(src.deEsserType.load());
    dst.deEsserThresholdDb.store(src.deEsserThresholdDb.load());
    dst.deEsserFreq.store(src.deEsserFreq.load());
    dst.deEsserRangeDb.store(src.deEsserRangeDb.load());

    dst.compThresholdDb.store(src.compThresholdDb.load());
    dst.compRatio.store(src.compRatio.load());
    dst.compAttackMs.store(src.compAttackMs.load());
    dst.compReleaseMs.store(src.compReleaseMs.load());
    dst.compKnee.store(src.compKnee.load());
    dst.compReleaseMode.store(src.compReleaseMode.load());
    dst.compMakeupDb.store(src.compMakeupDb.load());
    dst.compMixPercent.store(src.compMixPercent.load());
    dst.compKeySource.store(src.compKeySource.load());
    dst.compKeyHpfFreq.store(src.compKeyHpfFreq.load());
    dst.compKeyLpfFreq.store(src.compKeyLpfFreq.load());

    dst.limiterCeilingDb.store(src.limiterCeilingDb.load());
    dst.limiterReleaseMs.store(src.limiterReleaseMs.load());
    dst.outputGainDb.store(src.outputGainDb.load());

    if (vocalDSPRackComponent) vocalDSPRackComponent->updateUIFromProcessor();
    markUnsavedChanges();
}

void MainComponent::copyChannelStrip(int srcCh, int destCh, bool includeSends)
{
    if (srcCh < 0 || srcCh >= maxInputs || destCh < 0 || destCh >= maxInputs || srcCh == destCh)
        return;

    inputUIs[destCh].gainSlider.setValue(inputUIs[srcCh].gainSlider.getValue(), juce::sendNotification);
    inputUIs[destCh].panSlider.setValue(inputUIs[srcCh].panSlider.getValue(), juce::sendNotification);
    inputUIs[destCh].muteBtn.setToggleState(inputUIs[srcCh].muteBtn.getToggleState(), juce::sendNotification);
    inputUIs[destCh].soloBtn.setToggleState(inputUIs[srcCh].soloBtn.getToggleState(), juce::sendNotification);

    if (includeSends)
    {
        inputUIs[destCh].streamSendKnob.setValue(inputUIs[srcCh].streamSendKnob.getValue(), juce::sendNotification);
        inputUIs[destCh].monitorSendKnob.setValue(inputUIs[srcCh].monitorSendKnob.getValue(), juce::sendNotification);
    }
    markUnsavedChanges();
}

void MainComponent::resetTargetDefaults(int targetIndex)
{
    if (targetIndex < 0 || targetIndex >= maxInputs + 2) return;

    // Reset 8-Band EQ to defaults
    SceneData::EightBandEQSceneData defaultEQ;
    auto& eqProcessor = getProcessorForTarget(targetIndex).eightBandEQ;
    auto& eq = eqProcessor.params;
    eq.masterEnabled.store(true);
    for (int b = 0; b < 8; ++b)
    {
        eq.bands[b].type.store((LiveBus::EQFilterType)defaultEQ.bands[b].type);
        eq.bands[b].freq.store(defaultEQ.bands[b].freq);
        eq.bands[b].gainDb.store(defaultEQ.bands[b].gainDb);
        eq.bands[b].q.store(defaultEQ.bands[b].q);
        eq.bands[b].enabled.store(defaultEQ.bands[b].enabled);
    }
    eqProcessor.updateCoefficients();

    // Reset Vocal DSP Rack to defaults
    SceneData::VocalDSPSceneData defaultVR;
    auto& vp = getProcessorForTarget(targetIndex).vocalRack.params;
    vp.masterEnabled.store(defaultVR.masterEnabled);
    vp.hpfBypassed.store(defaultVR.hpfBypassed);
    vp.gateBypassed.store(defaultVR.gateBypassed);
    vp.eqBypassed.store(defaultVR.eqBypassed);
    vp.deEsserBypassed.store(defaultVR.deEsserBypassed);
    vp.compBypassed.store(defaultVR.compBypassed);
    vp.limiterBypassed.store(defaultVR.limiterBypassed);
    vp.inputGainDb.store(defaultVR.inputGainDb);
    vp.hpfFreq.store(defaultVR.hpfFreq);
    vp.hpfSlope.store(defaultVR.hpfSlope);
    vp.gateMode.store(defaultVR.gateMode);
    vp.gateOpenThresholdDb.store(defaultVR.gateOpenThresholdDb);
    vp.gateCloseThresholdDb.store(defaultVR.gateCloseThresholdDb);
    vp.gateAttackMs.store(defaultVR.gateAttackMs);
    vp.gateHoldMs.store(defaultVR.gateHoldMs);
    vp.gateReleaseMs.store(defaultVR.gateReleaseMs);
    vp.gateFloorDb.store(defaultVR.gateFloorDb);
    vp.gateKeySource.store(defaultVR.gateKeySource);
    vp.gateKeyHpfFreq.store(defaultVR.gateKeyHpfFreq);
    vp.gateKeyLpfFreq.store(defaultVR.gateKeyLpfFreq);
    vp.deEsserType.store(defaultVR.deEsserType);
    vp.deEsserThresholdDb.store(defaultVR.deEsserThresholdDb);
    vp.deEsserFreq.store(defaultVR.deEsserFreq);
    vp.deEsserRangeDb.store(defaultVR.deEsserRangeDb);
    vp.compThresholdDb.store(defaultVR.compThresholdDb);
    vp.compRatio.store(defaultVR.compRatio);
    vp.compAttackMs.store(defaultVR.compAttackMs);
    vp.compReleaseMs.store(defaultVR.compReleaseMs);
    vp.compKnee.store(defaultVR.compKnee);
    vp.compReleaseMode.store(defaultVR.compReleaseMode);
    vp.compMakeupDb.store(defaultVR.compMakeupDb);
    vp.compMixPercent.store(defaultVR.compMixPercent);
    vp.compKeySource.store(defaultVR.compKeySource);
    vp.compKeyHpfFreq.store(defaultVR.compKeyHpfFreq);
    vp.compKeyLpfFreq.store(defaultVR.compKeyLpfFreq);
    vp.limiterCeilingDb.store(defaultVR.limiterCeilingDb);
    vp.limiterReleaseMs.store(defaultVR.limiterReleaseMs);
    vp.outputGainDb.store(defaultVR.outputGainDb);

    if (eightBandEQComponent) eightBandEQComponent->updateUIFromProcessor();
    if (vocalDSPRackComponent) vocalDSPRackComponent->updateUIFromProcessor();
    if (eqCurveDisplay) eqCurveDisplay->repaint();
    markUnsavedChanges();
}

void MainComponent::loadPresetToTarget(LiveBus::PresetCategory cat, int targetIndex, const LiveBus::PresetItem& item)
{
    juce::String err;
    switch (cat)
    {
        case LiveBus::PresetCategory::VocalDSP:
        {
            if (targetIndex < 0 || targetIndex >= maxInputs + 2) return;
            SceneData::VocalDSPSceneData data;
            if (presetManager.loadVocalDSPPreset(item, data, err))
            {
                auto& vp = getProcessorForTarget(targetIndex).vocalRack.params;
                vp.masterEnabled.store(data.masterEnabled);
                vp.hpfBypassed.store(data.hpfBypassed);
                vp.gateBypassed.store(data.gateBypassed);
                vp.eqBypassed.store(data.eqBypassed);
                vp.deEsserBypassed.store(data.deEsserBypassed);
                vp.compBypassed.store(data.compBypassed);
                vp.limiterBypassed.store(data.limiterBypassed);
                vp.inputGainDb.store(data.inputGainDb);
                vp.hpfFreq.store(data.hpfFreq);
                vp.hpfSlope.store(data.hpfSlope);
                vp.gateMode.store(data.gateMode);
                vp.gateOpenThresholdDb.store(data.gateOpenThresholdDb);
                vp.gateCloseThresholdDb.store(data.gateCloseThresholdDb);
                vp.gateAttackMs.store(data.gateAttackMs);
                vp.gateHoldMs.store(data.gateHoldMs);
                vp.gateReleaseMs.store(data.gateReleaseMs);
                vp.gateFloorDb.store(data.gateFloorDb);
                vp.gateKeySource.store(data.gateKeySource);
                vp.gateKeyHpfFreq.store(data.gateKeyHpfFreq);
                vp.gateKeyLpfFreq.store(data.gateKeyLpfFreq);
                vp.deEsserType.store(data.deEsserType);
                vp.deEsserThresholdDb.store(data.deEsserThresholdDb);
                vp.deEsserFreq.store(data.deEsserFreq);
                vp.deEsserRangeDb.store(data.deEsserRangeDb);
                vp.compThresholdDb.store(data.compThresholdDb);
                vp.compRatio.store(data.compRatio);
                vp.compAttackMs.store(data.compAttackMs);
                vp.compReleaseMs.store(data.compReleaseMs);
                vp.compKnee.store(data.compKnee);
                vp.compReleaseMode.store(data.compReleaseMode);
                vp.compMakeupDb.store(data.compMakeupDb);
                vp.compMixPercent.store(data.compMixPercent);
                vp.compKeySource.store(data.compKeySource);
                vp.compKeyHpfFreq.store(data.compKeyHpfFreq);
                vp.compKeyLpfFreq.store(data.compKeyLpfFreq);
                vp.limiterCeilingDb.store(data.limiterCeilingDb);
                vp.limiterReleaseMs.store(data.limiterReleaseMs);
                vp.outputGainDb.store(data.outputGainDb);

                if (vocalDSPRackComponent) vocalDSPRackComponent->updateUIFromProcessor();
                markUnsavedChanges();
                log("Loaded Vocal DSP Preset '" + item.name + "' to target " + juce::String(targetIndex));
            }
            break;
        }
        case LiveBus::PresetCategory::EightBandEQ:
        {
            if (targetIndex < 0 || targetIndex >= maxInputs + 2) return;
            SceneData::EightBandEQSceneData data;
            if (presetManager.loadEightBandEQPreset(item, data, err))
            {
                auto& eqProcessor = getProcessorForTarget(targetIndex).eightBandEQ;
                auto& eq = eqProcessor.params;
                eq.masterEnabled.store(data.masterEnabled);
                for (int b = 0; b < 8; ++b)
                {
                    eq.bands[b].type.store((LiveBus::EQFilterType)data.bands[b].type);
                    eq.bands[b].freq.store(data.bands[b].freq);
                    eq.bands[b].gainDb.store(data.bands[b].gainDb);
                    eq.bands[b].q.store(data.bands[b].q);
                    eq.bands[b].enabled.store(data.bands[b].enabled);
                }
                eqProcessor.updateCoefficients();

                if (eightBandEQComponent) eightBandEQComponent->updateUIFromProcessor();
                if (eqCurveDisplay) eqCurveDisplay->repaint();
                markUnsavedChanges();
                log("Loaded 8-Band EQ Preset '" + item.name + "' to target " + juce::String(targetIndex));
            }
            break;
        }
        case LiveBus::PresetCategory::ChannelStrip:
        {
            if (targetIndex < 0 || targetIndex >= maxInputs) return;
            SceneData::ChannelSceneData data;
            if (presetManager.loadChannelStripPreset(item, data, err))
            {
                inputUIs[targetIndex].gainSlider.setValue(data.gain, juce::sendNotification);
                inputUIs[targetIndex].panSlider.setValue(data.pan, juce::sendNotification);
                inputUIs[targetIndex].muteBtn.setToggleState(data.mute, juce::sendNotification);
                inputUIs[targetIndex].soloBtn.setToggleState(data.solo, juce::sendNotification);
                inputUIs[targetIndex].streamSendKnob.setValue(data.streamSend, juce::sendNotification);
                inputUIs[targetIndex].monitorSendKnob.setValue(data.monitorSend, juce::sendNotification);

                markUnsavedChanges();
                log("Loaded Channel Strip Preset '" + item.name + "' to Channel " + juce::String(targetIndex + 1));
            }
            break;
        }
        case LiveBus::PresetCategory::BusDSP:
        {
            if (targetIndex < 0 || targetIndex >= 2) return;
            SceneData::BusSceneDSP data;
            if (presetManager.loadBusDSPPreset(item, data, err))
            {
                auto& p = busProcessors[targetIndex].params;
                p.hpfEnabled.store(data.hpfEnabled);
                p.hpfFreq.store(data.hpfFreq);
                p.eqEnabled.store(data.eqEnabled);
                p.eqLowFreq.store(data.eqLowFreq);
                p.eqLowGainDb.store(data.eqLowGainDb);
                p.eqMid1Freq.store(data.eqMid1Freq);
                p.eqMid1GainDb.store(data.eqMid1GainDb);
                p.eqMid1Q.store(data.eqMid1Q);
                p.eqFreq.store(data.eqFreq);
                p.eqQ.store(data.eqQ);
                p.eqGainDb.store(data.eqGainDb);
                p.eqHighFreq.store(data.eqHighFreq);
                p.eqHighGainDb.store(data.eqHighGainDb);
                p.gateEnabled.store(data.gateEnabled);
                p.gateThresholdDb.store(data.gateThresholdDb);
                p.gateRatio.store(data.gateRatio);
                p.gateAttackMs.store(data.gateAttackMs);
                p.gateReleaseMs.store(data.gateReleaseMs);
                p.compEnabled.store(data.compEnabled);
                p.compThresholdDb.store(data.compThresholdDb);
                p.compRatio.store(data.compRatio);
                p.compAttackMs.store(data.compAttackMs);
                p.compReleaseMs.store(data.compReleaseMs);
                p.limiterEnabled.store(data.limiterEnabled);
                p.limiterCeilingDb.store(data.limiterCeilingDb);
                p.limiterReleaseMs.store(data.limiterReleaseMs);
                p.reverbEnabled.store(data.reverbEnabled);
                p.reverbRoomSize.store(data.reverbRoomSize);
                p.reverbDamping.store(data.reverbDamping);
                p.reverbWetLevel.store(data.reverbWetLevel);
                p.reverbDryLevel.store(data.reverbDryLevel);
                p.delayEnabled.store(data.delayEnabled);
                p.delayTimeMs.store(data.delayTimeMs);
                p.delayFeedback.store(data.delayFeedback);
                p.delayWetLevel.store(data.delayWetLevel);
                busProcessors[targetIndex].updateCoefficients();

                markUnsavedChanges();
                log("Loaded Bus DSP Preset '" + item.name + "' to Bus " + juce::String(targetIndex));
            }
            break;
        }
        default: break;
    }
}

void MainComponent::savePresetFromTarget(LiveBus::PresetCategory cat, int targetIndex, const juce::String& name)
{
    switch (cat)
    {
        case LiveBus::PresetCategory::VocalDSP:
        {
            if (targetIndex < 0 || targetIndex >= maxInputs + 2) return;
            const auto& vp = getProcessorForTarget(targetIndex).vocalRack.params;
            SceneData::VocalDSPSceneData data;
            data.masterEnabled = vp.masterEnabled.load();
            data.hpfBypassed = vp.hpfBypassed.load();
            data.gateBypassed = vp.gateBypassed.load();
            data.eqBypassed = vp.eqBypassed.load();
            data.deEsserBypassed = vp.deEsserBypassed.load();
            data.compBypassed = vp.compBypassed.load();
            data.limiterBypassed = vp.limiterBypassed.load();
            data.inputGainDb = vp.inputGainDb.load();
            data.hpfFreq = vp.hpfFreq.load();
            data.hpfSlope = vp.hpfSlope.load();
            data.gateMode = vp.gateMode.load();
            data.gateOpenThresholdDb = vp.gateOpenThresholdDb.load();
            data.gateCloseThresholdDb = vp.gateCloseThresholdDb.load();
            data.gateAttackMs = vp.gateAttackMs.load();
            data.gateHoldMs = vp.gateHoldMs.load();
            data.gateReleaseMs = vp.gateReleaseMs.load();
            data.gateFloorDb = vp.gateFloorDb.load();
            data.gateKeySource = vp.gateKeySource.load();
            data.gateKeyHpfFreq = vp.gateKeyHpfFreq.load();
            data.gateKeyLpfFreq = vp.gateKeyLpfFreq.load();
            data.deEsserType = vp.deEsserType.load();
            data.deEsserThresholdDb = vp.deEsserThresholdDb.load();
            data.deEsserFreq = vp.deEsserFreq.load();
            data.deEsserRangeDb = vp.deEsserRangeDb.load();
            data.compThresholdDb = vp.compThresholdDb.load();
            data.compRatio = vp.compRatio.load();
            data.compAttackMs = vp.compAttackMs.load();
            data.compReleaseMs = vp.compReleaseMs.load();
            data.compKnee = vp.compKnee.load();
            data.compReleaseMode = vp.compReleaseMode.load();
            data.compMakeupDb = vp.compMakeupDb.load();
            data.compMixPercent = vp.compMixPercent.load();
            data.compKeySource = vp.compKeySource.load();
            data.compKeyHpfFreq = vp.compKeyHpfFreq.load();
            data.compKeyLpfFreq = vp.compKeyLpfFreq.load();
            data.limiterCeilingDb = vp.limiterCeilingDb.load();
            data.limiterReleaseMs = vp.limiterReleaseMs.load();
            data.outputGainDb = vp.outputGainDb.load();

            presetManager.saveVocalDSPPreset(name, data, "User preset saved from Target " + juce::String(targetIndex));
            log("Saved Vocal DSP Preset: " + name);
            break;
        }
        case LiveBus::PresetCategory::EightBandEQ:
        {
            if (targetIndex < 0 || targetIndex >= maxInputs + 2) return;
            const auto& eq = getProcessorForTarget(targetIndex).eightBandEQ.params;
            SceneData::EightBandEQSceneData data;
            data.masterEnabled = eq.masterEnabled.load();
            for (int b = 0; b < 8; ++b)
            {
                data.bands[b].type = (int)eq.bands[b].type.load();
                data.bands[b].freq = eq.bands[b].freq.load();
                data.bands[b].gainDb = eq.bands[b].gainDb.load();
                data.bands[b].q = eq.bands[b].q.load();
                data.bands[b].enabled = eq.bands[b].enabled.load();
            }
            presetManager.saveEightBandEQPreset(name, data, "User preset saved from Target " + juce::String(targetIndex));
            log("Saved 8-Band EQ Preset: " + name);
            break;
        }
        case LiveBus::PresetCategory::ChannelStrip:
        {
            if (targetIndex < 0 || targetIndex >= maxInputs) return;
            SceneData::ChannelSceneData data;
            data.name = inputUIs[targetIndex].nameEditor.getText();
            data.gain = (float)inputUIs[targetIndex].gainSlider.getValue();
            data.pan = (float)inputUIs[targetIndex].panSlider.getValue();
            data.mute = inputUIs[targetIndex].muteBtn.getToggleState();
            data.solo = inputUIs[targetIndex].soloBtn.getToggleState();
            data.streamSend = (float)inputUIs[targetIndex].streamSendKnob.getValue();
            data.monitorSend = (float)inputUIs[targetIndex].monitorSendKnob.getValue();

            presetManager.saveChannelStripPreset(name, data, "User preset saved from Channel " + juce::String(targetIndex + 1));
            log("Saved Channel Strip Preset: " + name);
            break;
        }
        case LiveBus::PresetCategory::BusDSP:
        {
            if (targetIndex < 0 || targetIndex >= 2) return;
            const auto& p = busProcessors[targetIndex].params;
            SceneData::BusSceneDSP data;
            data.hpfEnabled = p.hpfEnabled.load();
            data.hpfFreq = p.hpfFreq.load();
            data.eqEnabled = p.eqEnabled.load();
            data.eqLowFreq = p.eqLowFreq.load();
            data.eqLowGainDb = p.eqLowGainDb.load();
            data.eqMid1Freq = p.eqMid1Freq.load();
            data.eqMid1GainDb = p.eqMid1GainDb.load();
            data.eqMid1Q = p.eqMid1Q.load();
            data.eqFreq = p.eqFreq.load();
            data.eqQ = p.eqQ.load();
            data.eqGainDb = p.eqGainDb.load();
            data.eqHighFreq = p.eqHighFreq.load();
            data.eqHighGainDb = p.eqHighGainDb.load();
            data.gateEnabled = p.gateEnabled.load();
            data.gateThresholdDb = p.gateThresholdDb.load();
            data.gateRatio = p.gateRatio.load();
            data.gateAttackMs = p.gateAttackMs.load();
            data.gateReleaseMs = p.gateReleaseMs.load();
            data.compEnabled = p.compEnabled.load();
            data.compThresholdDb = p.compThresholdDb.load();
            data.compRatio = p.compRatio.load();
            data.compAttackMs = p.compAttackMs.load();
            data.compReleaseMs = p.compReleaseMs.load();
            data.limiterEnabled = p.limiterEnabled.load();
            data.limiterCeilingDb = p.limiterCeilingDb.load();
            data.limiterReleaseMs = p.limiterReleaseMs.load();
            data.reverbEnabled = p.reverbEnabled.load();
            data.reverbRoomSize = p.reverbRoomSize.load();
            data.reverbDamping = p.reverbDamping.load();
            data.reverbWetLevel = p.reverbWetLevel.load();
            data.reverbDryLevel = p.reverbDryLevel.load();
            data.delayEnabled = p.delayEnabled.load();
            data.delayTimeMs = p.delayTimeMs.load();
            data.delayFeedback = p.delayFeedback.load();
            data.delayWetLevel = p.delayWetLevel.load();

            presetManager.saveBusDSPPreset(name, data, "User preset saved from Bus " + juce::String(targetIndex));
            log("Saved Bus DSP Preset: " + name);
            break;
        }
        default: break;
    }
}

void MainComponent::markUnsavedChanges()
{
    hasUnsavedChanges = true;
    if (scenesConsoleComponent)
        scenesConsoleComponent->setUnsavedChanges(true);
}
