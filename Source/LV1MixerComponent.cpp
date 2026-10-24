#include "LV1MixerComponent.h"
#include <cmath>

LV1MixerComponent::LV1MixerComponent(RoutingMatrix& matrix,
                                     std::array<BusDSPProcessor, numBuses>& dspProcessors,
                                     PluginHostClient& pluginClient,
                                     SceneManager& scenes)
    : routingMatrix(matrix),
      busProcessors(dspProcessors),
      pluginHostClient(pluginClient),
      sceneManager(scenes)
{
    // Ensure all 16 channels are active for the LV1 layout
    while (routingMatrix.getNumActiveChannels() < maxInputs)
    {
        if (!routingMatrix.addChannel()) break;
    }

    // 1. Top Header Components
    chSelectBadge.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00acc1)); // Cyan badge
    chSelectBadge.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    chSelectBadge.addListener(this);
    addAndMakeVisible(chSelectBadge);

    for (auto* btn : { &chPrevBtn, &chNextBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2632));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
        btn->addListener(this);
        addAndMakeVisible(btn);
    }

    presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    presetBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    presetBtn.addListener(this);
    addAndMakeVisible(presetBtn);

    clrSoloBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    clrSoloBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffca28));
    clrSoloBtn.addListener(this);
    addAndMakeVisible(clrSoloBtn);

    talkBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    talkBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    talkBtn.addListener(this);
    addAndMakeVisible(talkBtn);

    tapBpmBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    tapBpmBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
    tapBpmBtn.addListener(this);
    addAndMakeVisible(tapBpmBtn);

    // Screen navigation tabs
    const char* tabNames[] = { "MIXER 1", "MIXER 2", "CHANNEL", "SHOW", "PATCH", "SETUP" };
    juce::TextButton* tabs[] = { &mixer1TabBtn, &mixer2TabBtn, &channelTabBtn, &showTabBtn, &patchTabBtn, &setupTabBtn };
    for (size_t i = 0; i < 6; ++i)
    {
        tabs[i]->setButtonText(tabNames[i]);
        tabs[i]->setColour(juce::TextButton::buttonColourId, (i == 0) ? juce::Colour(0xff00b4d8) : juce::Colour(0xff1a222d));
        tabs[i]->setColour(juce::TextButton::textColourOffId, (i == 0) ? juce::Colours::black : juce::Colour(0xffcbd5e1));
        tabs[i]->addListener(this);
        addAndMakeVisible(tabs[i]);
    }

    // Scene Selector & Engine Status
    sceneSelector.addItem("No Scenes", 1);
    sceneSelector.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(sceneSelector);

    serverIoLabel.setText("SERVER IO  A:42  B:24  C:76  D:25", juce::dontSendNotification);
    serverIoLabel.setFont(juce::FontOptions(10.0f));
    serverIoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e676)); // Green DSP meters
    addAndMakeVisible(serverIoLabel);

    clockLabel.setText("17:16:36", juce::dontSendNotification);
    clockLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    clockLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
    addAndMakeVisible(clockLabel);

    brandLabel.setText("eMOTION LV1", juce::dontSendNotification);
    brandLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold | juce::Font::italic));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
    addAndMakeVisible(brandLabel);

    // 2. Setup 16 Channel Strips
    for (int i = 0; i < maxInputs; ++i)
    {
        auto& ch = channelStrips[(size_t)i];
        const auto& state = routingMatrix.getChannel(i);

        // Top source tag (1 Mic 1..16)
        ch.sourceLabel.setText("1 Mic " + juce::String(i + 1), juce::dontSendNotification);
        ch.sourceLabel.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        ch.sourceLabel.setJustificationType(juce::Justification::centred);
        ch.sourceLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff00838f)); // Cyan header
        ch.sourceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(ch.sourceLabel);

        // Filters button
        ch.filterBtn.setButtonText("FILTERS");
        ch.filterBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffb300)); // Amber
        ch.filterBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        ch.filterBtn.addListener(this);
        addAndMakeVisible(ch.filterBtn);

        // Channel ID Badge (Chn 1..16)
        ch.idBadge.setButtonText("Chn " + juce::String(i + 1));
        ch.idBadge.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0097a7));
        ch.idBadge.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        ch.idBadge.addListener(this);
        addAndMakeVisible(ch.idBadge);

        // Rotary Pan Knob
        ch.panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ch.panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ch.panKnob.setRange(-1.0, 1.0, 0.01);
        ch.panKnob.setValue(state.pan.load());
        ch.panKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00e5ff));
        ch.panKnob.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00e5ff));
        ch.panKnob.addListener(this);
        addAndMakeVisible(ch.panKnob);

        // Mute button
        bool isMuted = state.mute.load();
        ch.muteBtn.setButtonText("MUTE");
        ch.muteBtn.setColour(juce::TextButton::buttonColourId, isMuted ? juce::Colour(0xffd50000) : juce::Colour(0xff1e2632));
        ch.muteBtn.setColour(juce::TextButton::textColourOffId, isMuted ? juce::Colours::white : juce::Colour(0xff94a3b8));
        ch.muteBtn.addListener(this);
        addAndMakeVisible(ch.muteBtn);

        // dB Readout
        ch.dbReadout.setText("-3.8", juce::dontSendNotification);
        ch.dbReadout.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        ch.dbReadout.setJustificationType(juce::Justification::centred);
        ch.dbReadout.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0a0e14));
        ch.dbReadout.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
        addAndMakeVisible(ch.dbReadout);

        // Long-throw fader
        ch.fader.setSliderStyle(juce::Slider::LinearVertical);
        ch.fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ch.fader.setRange(0.0, 1.5, 0.005);
        ch.fader.setValue(state.gain.load());
        ch.fader.setColour(juce::Slider::trackColourId, juce::Colour(0xff0d131a));
        ch.fader.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe2e8f0)); // Metallic thumb
        ch.fader.addListener(this);
        addAndMakeVisible(ch.fader);

        // Cue / Solo button
        ch.cueBtn.setButtonText(juce::String(i + 1));
        ch.cueBtn.setColour(juce::TextButton::buttonColourId, state.solo.load() ? juce::Colour(0xffffca28) : juce::Colour(0xff00838f));
        ch.cueBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        ch.cueBtn.addListener(this);
        addAndMakeVisible(ch.cueBtn);
    }

    // 3. Master Section (LR)
    masterTag.setText("LR MASTER", juce::dontSendNotification);
    masterTag.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    masterTag.setJustificationType(juce::Justification::centred);
    masterTag.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1e2632));
    masterTag.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(masterTag);

    masterPanKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterPanKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterPanKnob.setRange(-1.0, 1.0, 0.01);
    masterPanKnob.setValue(0.0);
    masterPanKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00e5ff));
    masterPanKnob.addListener(this);
    addAndMakeVisible(masterPanKnob);

    masterMuteBtn.setButtonText("MUTE");
    masterMuteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2632));
    masterMuteBtn.addListener(this);
    addAndMakeVisible(masterMuteBtn);

    masterDbReadout.setText("-0.0", juce::dontSendNotification);
    masterDbReadout.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    masterDbReadout.setJustificationType(juce::Justification::centred);
    masterDbReadout.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0a0e14));
    masterDbReadout.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
    addAndMakeVisible(masterDbReadout);

    masterFader.setSliderStyle(juce::Slider::LinearVertical);
    masterFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterFader.setRange(0.0, 1.5, 0.005);
    masterFader.setValue(1.0);
    masterFader.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe2e8f0));
    masterFader.addListener(this);
    addAndMakeVisible(masterFader);

    masterCueBtn.setButtonText("LR");
    masterCueBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00838f));
    masterCueBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    masterCueBtn.addListener(this);
    addAndMakeVisible(masterCueBtn);

    // 4. Right Sidebar: MODES, LAYERS, MUTE GRP
    const char* modeNames[] = { "INPUT", "RACK", "DYN/EQ", "EFX 1-8", "MON 1-8", "MON 9-16", "ROUTE", "CHANNEL" };
    for (size_t i = 0; i < 8; ++i)
    {
        modeBtns[i].setButtonText(modeNames[i]);
        modeBtns[i].setColour(juce::TextButton::buttonColourId, (i == 2) ? juce::Colour(0xff00b4d8) : juce::Colour(0xff161e28));
        modeBtns[i].setColour(juce::TextButton::textColourOffId, (i == 2) ? juce::Colours::black : juce::Colour(0xff94a3b8));
        modeBtns[i].addListener(this);
        addAndMakeVisible(modeBtns[i]);
    }

    const char* layerNames[] = { "CUSTOM", "SPILL", "CH 1-16", "CH 17-32", "GRP/FX", "MONITOR" };
    for (size_t i = 0; i < 6; ++i)
    {
        layerBtns[i].setButtonText(layerNames[i]);
        layerBtns[i].setColour(juce::TextButton::buttonColourId, (i == 2) ? juce::Colour(0xff00838f) : juce::Colour(0xff161e28));
        layerBtns[i].setColour(juce::TextButton::textColourOffId, (i == 2) ? juce::Colours::white : juce::Colour(0xff94a3b8));
        layerBtns[i].addListener(this);
        addAndMakeVisible(layerBtns[i]);
    }

    for (size_t i = 0; i < 8; ++i)
    {
        muteGrpBtns[i].setButtonText(juce::String(i + 1));
        muteGrpBtns[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e28));
        muteGrpBtns[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
        muteGrpBtns[i].addListener(this);
        addAndMakeVisible(muteGrpBtns[i]);
    }

    // 5. Left Sidebar: USER Buttons
    for (size_t i = 0; i < 16; ++i)
    {
        userBtns[i].setButtonText(juce::String(i + 1));
        userBtns[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e28));
        userBtns[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff64748b));
        userBtns[i].addListener(this);
        addAndMakeVisible(userBtns[i]);
    }

    startTimerHz(30);
    updateUIFromState();
}

LV1MixerComponent::~LV1MixerComponent()
{
    stopTimer();
}

void LV1MixerComponent::paint(juce::Graphics& g)
{
    // High-tech charcoal slate console chassis
    g.fillAll(juce::Colour(0xff0c1017));

    // Outer framing lines
    g.setColour(juce::Colour(0xff1a2332));
    g.drawRect(getLocalBounds().toFloat(), 1.0f);

    // Left sidebar USER section
    g.setColour(juce::Colour(0xff131a24));
    g.fillRect(0, 36, 26, getHeight() - 36);
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
    g.drawText("USER", 2, 40, 22, 12, juce::Justification::centred);

    // Channel separator grid lines
    int startX = 28;
    int chW = (getWidth() - startX - 145) / maxInputs;

    for (int i = 0; i < maxInputs; ++i)
    {
        int x = startX + (i * chW);

        // Strip background
        g.setColour((i % 2 == 0) ? juce::Colour(0xff0f141d) : juce::Colour(0xff121822));
        g.fillRect(x, 36, chW, getHeight() - 36);

        // Right border line
        g.setColour(juce::Colour(0xff1e2838));
        g.drawVerticalLine(x + chW, 36.0f, (float)getHeight());

        // Draw Dynamics block thumbnail (Gate & Comp)
        int dynY = 56;
        int dynH = 70;
        g.setColour(juce::Colour(0xff002b4d)); // Dynamics blue bar
        g.fillRect(x + 2, dynY, chW - 4, 12);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
        g.drawText("DYNAMICS", x + 2, dynY, chW - 4, 12, juce::Justification::centred);

        // Mini comp transfer curve (Orange knee graph)
        g.setColour(juce::Colour(0xff0a1017));
        g.fillRect(x + 2, dynY + 12, chW - 4, 34);

        g.setColour(juce::Colour(0xffff9100)); // Compression orange curve
        juce::Path compCurve;
        compCurve.startNewSubPath((float)(x + 4), (float)(dynY + 44));
        compCurve.lineTo((float)(x + chW / 2), (float)(dynY + 30));
        compCurve.lineTo((float)(x + chW - 6), (float)(dynY + 22));
        g.strokePath(compCurve, juce::PathStrokeType(1.5f));

        // Sub-badges: LVL (green), LIM (red), DeESSER (purple)
        g.setColour(juce::Colour(0xff2e7d32));
        g.fillRect(x + 3, dynY + 48, (chW - 8) / 2, 8);
        g.setColour(juce::Colour(0xffc62828));
        g.fillRect(x + (chW / 2), dynY + 48, (chW - 8) / 2, 8);

        g.setColour(juce::Colour(0xff4a148c)); // Purple DeEsser
        g.fillRect(x + 2, dynY + 58, chW - 4, 10);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(7.0f));
        g.drawText("DeESSER", x + 2, dynY + 58, chW - 4, 10, juce::Justification::centred);

        // Draw EQ curve thumbnail block
        int eqY = dynY + dynH + 2;
        g.setColour(juce::Colour(0xffffb300)); // Yellow EQ header
        g.fillRect(x + 2, eqY, chW - 4, 10);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
        g.drawText("EQ", x + 2, eqY, chW - 4, 10, juce::Justification::centred);

        // Mini EQ curve shape
        g.setColour(juce::Colour(0xff0a1017));
        g.fillRect(x + 2, eqY + 10, chW - 4, 26);

        g.setColour(juce::Colour(0xff00e5ff)); // Cyan EQ curve
        juce::Path eqPath;
        float cy = (float)(eqY + 23);
        eqPath.startNewSubPath((float)(x + 4), cy);
        eqPath.quadraticTo((float)(x + chW * 0.4f), cy - 6.0f, (float)(x + chW * 0.7f), cy + 4.0f);
        eqPath.lineTo((float)(x + chW - 4), cy);
        g.strokePath(eqPath, juce::PathStrokeType(1.2f));

        // Draw dB scale markers & LED meter next to fader
        int faderY = eqY + 125;
        int faderH = getHeight() - faderY - 32;
        drawFaderScale(g, x + 2, faderY, faderH);
        drawLedMeter(g, x + chW - 8, faderY, 5, faderH, channelStrips[(size_t)i].currentMeterVal);
    }

    // Master LR Section background
    int masterX = startX + (maxInputs * chW) + 4;
    int masterW = 68;
    g.setColour(juce::Colour(0xff101722));
    g.fillRect(masterX, 36, masterW, getHeight() - 36);
    g.setColour(juce::Colour(0xff00e5ff));
    g.drawRect(masterX, 36, masterW, getHeight() - 36, 1);

    // Master Dual LED Meters (L and R)
    int mFaderY = 175;
    int mFaderH = getHeight() - mFaderY - 32;
    drawLedMeter(g, masterX + masterW - 14, mFaderY, 4, mFaderH, masterMeterL);
    drawLedMeter(g, masterX + masterW - 8, mFaderY, 4, mFaderH, masterMeterR);

    // Right Sidebar Background (MODES, LAYERS, MUTE GRP)
    int rX = masterX + masterW + 4;
    int rW = getWidth() - rX;
    g.setColour(juce::Colour(0xff0e131b));
    g.fillRect(rX, 36, rW, getHeight() - 36);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("MODES", rX + 4, 38, rW - 8, 12, juce::Justification::centred);
    g.drawText("LAYERS", rX + 4, 218, rW - 8, 12, juce::Justification::centred);
    g.drawText("MUTE GRP", rX + 4, 385, rW - 8, 12, juce::Justification::centred);
}

void LV1MixerComponent::drawFaderScale(juce::Graphics& g, int x, int y, int h)
{
    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::FontOptions(7.5f));

    const struct { float norm; const char* txt; } marks[] = {
        { 0.05f, "+10" }, { 0.20f, "0" }, { 0.45f, "-10" }, { 0.70f, "-30" }, { 0.95f, "-inf" }
    };

    for (const auto& m : marks)
    {
        int my = y + (int)(m.norm * (float)h);
        g.drawHorizontalLine(my, (float)x, (float)(x + 4));
        g.drawText(m.txt, x + 5, my - 5, 20, 10, juce::Justification::left);
    }
}

void LV1MixerComponent::drawLedMeter(juce::Graphics& g, int x, int y, int w, int h, float level)
{
    // Draw background track
    g.setColour(juce::Colour(0xff070a0e));
    g.fillRect(x, y, w, h);

    // Calculate filled height
    int fillH = juce::jlimit(0, h, (int)(level * (float)h));
    if (fillH <= 0) return;

    int fillY = y + h - fillH;

    // Green segments
    int greenH = juce::jmin(fillH, (int)((float)h * 0.75f));
    g.setColour(juce::Colour(0xff00e676));
    g.fillRect(x, y + h - greenH, w, greenH);

    // Yellow segments (-12dB to 0dB)
    if (fillH > (int)((float)h * 0.75f))
    {
        int yellowH = juce::jmin(fillH - (int)((float)h * 0.75f), (int)((float)h * 0.20f));
        g.setColour(juce::Colour(0xffffca28));
        g.fillRect(x, y + h - (int)((float)h * 0.75f) - yellowH, w, yellowH);
    }

    // Red clipping segment (above 0dB)
    if (fillH > (int)((float)h * 0.95f))
    {
        int redH = fillH - (int)((float)h * 0.95f);
        g.setColour(juce::Colour(0xffff1744));
        g.fillRect(x, fillY, w, redH);
    }
}

void LV1MixerComponent::resized()
{
    int topH = 32;

    // 1. Top Header Layout
    chSelectBadge.setBounds(4, 4, 85, 24);
    chPrevBtn.setBounds(91, 4, 18, 24);
    chNextBtn.setBounds(111, 4, 18, 24);
    presetBtn.setBounds(133, 4, 75, 24);
    clrSoloBtn.setBounds(212, 4, 60, 24);
    talkBtn.setBounds(276, 4, 42, 24);
    tapBpmBtn.setBounds(322, 4, 75, 24);

    // Screen tabs
    int tabX = 405;
    int tabW = 55;
    mixer1TabBtn.setBounds(tabX, 4, tabW, 24);
    mixer2TabBtn.setBounds(tabX + 57, 4, tabW, 24);
    channelTabBtn.setBounds(tabX + 114, 4, tabW + 5, 24);
    showTabBtn.setBounds(tabX + 176, 4, tabW - 5, 24);
    patchTabBtn.setBounds(tabX + 228, 4, tabW - 5, 24);
    setupTabBtn.setBounds(tabX + 280, 4, tabW - 5, 24);

    // Status bar & Clock on right
    sceneSelector.setBounds(tabX + 340, 4, 110, 24);
    serverIoLabel.setBounds(tabX + 455, 4, 175, 24);
    clockLabel.setBounds(getWidth() - 170, 4, 55, 24);
    brandLabel.setBounds(getWidth() - 110, 4, 105, 24);

    // 2. 16 Channel Strips Layout
    int startX = 28;
    int chW = (getWidth() - startX - 145) / maxInputs;

    for (int i = 0; i < maxInputs; ++i)
    {
        int x = startX + (i * chW);
        auto& ch = channelStrips[(size_t)i];

        ch.sourceLabel.setBounds(x + 2, 38, chW - 4, 16);
        ch.filterBtn.setBounds(x + 2, 55, chW - 4, 14);

        // ID badge, pan, mute, db readout
        int cY = 180;
        ch.idBadge.setBounds(x + 2, cY, chW - 4, 16);
        ch.panKnob.setBounds(x + (chW - 24) / 2, cY + 18, 24, 24);
        ch.muteBtn.setBounds(x + 3, cY + 44, chW - 6, 18);
        ch.dbReadout.setBounds(x + 3, cY + 64, chW - 6, 14);

        int faderY = cY + 80;
        int faderH = getHeight() - faderY - 30;
        ch.fader.setBounds(x + 18, faderY, 20, faderH);
        ch.cueBtn.setBounds(x + 3, getHeight() - 26, chW - 6, 22);
    }

    // 3. Master Section Layout
    int masterX = startX + (maxInputs * chW) + 4;
    int masterW = 68;

    masterTag.setBounds(masterX + 4, 38, masterW - 8, 16);
    masterPanKnob.setBounds(masterX + (masterW - 26) / 2, 60, 26, 26);
    masterMuteBtn.setBounds(masterX + 4, 90, masterW - 8, 20);
    masterDbReadout.setBounds(masterX + 4, 114, masterW - 8, 16);

    int mFaderY = 135;
    int mFaderH = getHeight() - mFaderY - 30;
    masterFader.setBounds(masterX + 16, mFaderY, 24, mFaderH);
    masterCueBtn.setBounds(masterX + 4, getHeight() - 26, masterW - 8, 22);

    // 4. Right Sidebar Layout
    int rX = masterX + masterW + 4;
    int rW = getWidth() - rX - 4;

    int mY = 52;
    for (size_t i = 0; i < 8; ++i)
    {
        modeBtns[i].setBounds(rX, mY + (int)i * 20, rW, 18);
    }

    int lY = 232;
    for (size_t i = 0; i < 6; ++i)
    {
        layerBtns[i].setBounds(rX, lY + (int)i * 22, rW, 20);
    }

    int mgY = 400;
    int btnW = (rW - 6) / 2;
    for (size_t i = 0; i < 8; ++i)
    {
        int col = (int)(i % 2);
        int row = (int)(i / 2);
        muteGrpBtns[i].setBounds(rX + (col * (btnW + 4)), mgY + (row * 22), btnW, 20);
    }

    // 5. Left Sidebar USER Buttons
    int uH = (getHeight() - 60) / 16;
    for (size_t i = 0; i < 16; ++i)
    {
        userBtns[i].setBounds(2, 54 + (int)i * uH, 22, uH - 2);
    }
}

void LV1MixerComponent::updateUIFromState()
{
    isUpdatingUI = true;

    for (int i = 0; i < maxInputs; ++i)
    {
        const auto& state = routingMatrix.getChannel(i);
        auto& ch = channelStrips[(size_t)i];

        ch.fader.setValue(state.gain.load(), juce::dontSendNotification);
        ch.panKnob.setValue(state.pan.load(), juce::dontSendNotification);

        bool isMuted = state.mute.load();
        ch.muteBtn.setColour(juce::TextButton::buttonColourId, isMuted ? juce::Colour(0xffd50000) : juce::Colour(0xff1e2632));
        ch.muteBtn.setColour(juce::TextButton::textColourOffId, isMuted ? juce::Colours::white : juce::Colour(0xff94a3b8));

        bool isSolo = state.solo.load();
        ch.cueBtn.setColour(juce::TextButton::buttonColourId, isSolo ? juce::Colour(0xffffca28) : juce::Colour(0xff00838f));

        float gainVal = state.gain.load();
        float db = (gainVal > 0.0001f) ? juce::Decibels::gainToDecibels(gainVal) : -100.0f;
        ch.dbReadout.setText(db <= -60.0f ? "-inf" : juce::String(db, 1), juce::dontSendNotification);
    }

    float mGain = busProcessors[0].params.masterGain.load();
    masterFader.setValue(mGain, juce::dontSendNotification);
    float mDb = (mGain > 0.0001f) ? juce::Decibels::gainToDecibels(mGain) : -100.0f;
    masterDbReadout.setText(mDb <= -60.0f ? "-inf" : juce::String(mDb, 1), juce::dontSendNotification);

    isUpdatingUI = false;
}

void LV1MixerComponent::sliderValueChanged(juce::Slider* slider)
{
    if (isUpdatingUI) return;

    for (int i = 0; i < maxInputs; ++i)
    {
        auto& ch = channelStrips[(size_t)i];
        if (slider == &ch.fader)
        {
            float val = (float)slider->getValue();
            routingMatrix.getChannel(i).gain.store(val, std::memory_order_relaxed);
            float db = (val > 0.0001f) ? juce::Decibels::gainToDecibels(val) : -100.0f;
            ch.dbReadout.setText(db <= -60.0f ? "-inf" : juce::String(db, 1), juce::dontSendNotification);
            return;
        }
        else if (slider == &ch.panKnob)
        {
            routingMatrix.getChannel(i).pan.store((float)slider->getValue(), std::memory_order_relaxed);
            return;
        }
    }

    if (slider == &masterFader)
    {
        float val = (float)slider->getValue();
        busProcessors[0].params.masterGain.store(val, std::memory_order_relaxed);
        busProcessors[1].params.masterGain.store(val, std::memory_order_relaxed);
        float db = (val > 0.0001f) ? juce::Decibels::gainToDecibels(val) : -100.0f;
        masterDbReadout.setText(db <= -60.0f ? "-inf" : juce::String(db, 1), juce::dontSendNotification);
    }
    else if (slider == &masterPanKnob)
    {
        busProcessors[0].params.masterPan.store((float)slider->getValue(), std::memory_order_relaxed);
    }
}

void LV1MixerComponent::buttonClicked(juce::Button* button)
{
    // Screen tab buttons
    if (button == &channelTabBtn || button == &modeBtns[2] || button == &modeBtns[7])
    {
        if (onOpenDSPStudio) onOpenDSPStudio();
        return;
    }
    else if (button == &patchTabBtn || button == &setupTabBtn)
    {
        if (onOpenAudioSettings) onOpenAudioSettings();
        return;
    }
    else if (button == &showTabBtn)
    {
        if (onSaveScene) onSaveScene();
        return;
    }
    else if (button == &clrSoloBtn)
    {
        // Clear all solos
        for (int i = 0; i < maxInputs; ++i)
            routingMatrix.getChannel(i).solo.store(false, std::memory_order_relaxed);
        updateUIFromState();
        return;
    }
    else if (button == &tapBpmBtn)
    {
        juce::int64 now = juce::Time::currentTimeMillis();
        if (lastTapTimeMs > 0)
        {
            juce::int64 diff = now - lastTapTimeMs;
            if (diff > 200 && diff < 2000)
            {
                currentBPM = (int)(60000 / diff);
                tapBpmBtn.setButtonText(juce::String(currentBPM) + " BPM TAP");
            }
        }
        lastTapTimeMs = now;
        return;
    }
    else if (button == &chPrevBtn)
    {
        selectedChannel = (selectedChannel > 0) ? selectedChannel - 1 : maxInputs - 1;
        chSelectBadge.setButtonText("Channel " + juce::String(selectedChannel + 1));
        return;
    }
    else if (button == &chNextBtn)
    {
        selectedChannel = (selectedChannel + 1) % maxInputs;
        chSelectBadge.setButtonText("Channel " + juce::String(selectedChannel + 1));
        return;
    }

    // Channel specific buttons
    for (int i = 0; i < maxInputs; ++i)
    {
        auto& ch = channelStrips[(size_t)i];
        if (button == &ch.muteBtn)
        {
            bool current = routingMatrix.getChannel(i).mute.load();
            routingMatrix.getChannel(i).mute.store(!current, std::memory_order_relaxed);
            ch.muteBtn.setColour(juce::TextButton::buttonColourId, !current ? juce::Colour(0xffd50000) : juce::Colour(0xff1e2632));
            ch.muteBtn.setColour(juce::TextButton::textColourOffId, !current ? juce::Colours::white : juce::Colour(0xff94a3b8));
            return;
        }
        else if (button == &ch.cueBtn)
        {
            selectedChannel = i;
            chSelectBadge.setButtonText("Channel " + juce::String(selectedChannel + 1));
            bool curSolo = routingMatrix.getChannel(i).solo.load();
            routingMatrix.getChannel(i).solo.store(!curSolo, std::memory_order_relaxed);
            ch.cueBtn.setColour(juce::TextButton::buttonColourId, !curSolo ? juce::Colour(0xffffca28) : juce::Colour(0xff00838f));
            return;
        }
        else if (button == &ch.idBadge || button == &ch.filterBtn)
        {
            selectedChannel = i;
            chSelectBadge.setButtonText("Channel " + juce::String(selectedChannel + 1));
            if (onOpenDSPStudio) onOpenDSPStudio();
            return;
        }
    }

    if (button == &masterMuteBtn)
    {
        bool cur = busProcessors[0].params.masterMute.load();
        busProcessors[0].params.masterMute.store(!cur, std::memory_order_relaxed);
        busProcessors[1].params.masterMute.store(!cur, std::memory_order_relaxed);
        masterMuteBtn.setColour(juce::TextButton::buttonColourId, !cur ? juce::Colour(0xffd50000) : juce::Colour(0xff1e2632));
    }
}

void LV1MixerComponent::timerCallback()
{
    // Update live clock
    auto now = juce::Time::getCurrentTime();
    clockLabel.setText(now.formatted("%H:%M:%S"), juce::dontSendNotification);

    // Update channel meters smoothly
    for (int i = 0; i < maxInputs; ++i)
    {
        float target = routingMatrix.getChannel(i).peakMeter.load(std::memory_order_relaxed);
        auto& ch = channelStrips[(size_t)i];
        ch.currentMeterVal = juce::jmax(target, ch.currentMeterVal * 0.82f);
    }

    // Update master meters
    float pL = busProcessors[0].getPeakL();
    float pR = busProcessors[0].getPeakR();
    masterMeterL = juce::jmax(pL, masterMeterL * 0.82f);
    masterMeterR = juce::jmax(pR, masterMeterR * 0.82f);

    repaint();
}
