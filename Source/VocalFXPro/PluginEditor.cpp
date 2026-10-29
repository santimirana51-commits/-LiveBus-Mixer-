#include "PluginEditor.h"

// ==============================================================================
// Custom Modern Dark Navy Look & Feel
// ==============================================================================
VocalFXProLookAndFeel::VocalFXProLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff090d16));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff121829));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff24304f));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff38bdf8));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff121829));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff1f5f9));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff0284c7));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void VocalFXProLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle,
                                             juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto center = bounds.getCentre();

    // Outer glow / background track
    float trackThickness = 4.0f;
    juce::Path trackPath;
    trackPath.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f,
                           startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff1c263c));
    g.strokePath(trackPath, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active arc with vibrant gradient (Cyan to Purple)
    auto currentAngle = startAngle + sliderPos * (endAngle - startAngle);
    if (currentAngle > startAngle)
    {
        juce::Path activePath;
        activePath.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f,
                                startAngle, currentAngle, true);

        juce::Colour accentColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (accentColour.isTransparent())
            accentColour = juce::Colour(0xff38bdf8); // default cyan

        g.setColour(accentColour);
        g.strokePath(activePath, juce::PathStrokeType(trackThickness + 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Inner dial body
    auto dialBounds = bounds.reduced(trackThickness + 5.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1f2a44), dialBounds.getX(), dialBounds.getY(),
                                           juce::Colour(0xff101524), dialBounds.getRight(), dialBounds.getBottom(), false));
    g.fillEllipse(dialBounds);

    g.setColour(juce::Colour(0xff334155).withAlpha(0.6f));
    g.drawEllipse(dialBounds, 1.2f);

    // Indicator pointer line / dot
    float pointerLength = dialBounds.getWidth() * 0.35f;
    juce::Point<float> p1(center.x + (dialBounds.getWidth() * 0.15f) * std::cos(currentAngle - juce::MathConstants<float>::halfPi),
                          center.y + (dialBounds.getWidth() * 0.15f) * std::sin(currentAngle - juce::MathConstants<float>::halfPi));
    juce::Point<float> p2(center.x + pointerLength * std::cos(currentAngle - juce::MathConstants<float>::halfPi),
                          center.y + pointerLength * std::sin(currentAngle - juce::MathConstants<float>::halfPi));

    g.setColour(juce::Colours::white);
    g.drawLine(juce::Line<float>(p1, p2), 2.5f);
}

void VocalFXProLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                             juce::Slider::SliderStyle, juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    float centerY = bounds.getCentreY();
    float trackH = 8.0f;
    auto trackRect = juce::Rectangle<float>(bounds.getX() + 4.0f, centerY - trackH * 0.5f, bounds.getWidth() - 8.0f, trackH);

    // Track background
    g.setColour(juce::Colour(0xff1a233a));
    g.fillRoundedRectangle(trackRect, 4.0f);

    // Track active fill
    auto activeRect = juce::Rectangle<float>(trackRect.getX(), trackRect.getY(),
                                            juce::jmax(0.0f, sliderPos - trackRect.getX()), trackH);
    juce::ColourGradient grad(juce::Colour(0xff0284c7), activeRect.getX(), activeRect.getY(),
                              juce::Colour(0xff38bdf8), activeRect.getRight(), activeRect.getY(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(activeRect, 4.0f);

    // Thumb thumb handle
    float thumbRadius = 10.0f;
    juce::Rectangle<float> thumbRect(sliderPos - thumbRadius, centerY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
    g.setColour(juce::Colour(0xff090d16));
    g.fillEllipse(thumbRect.expanded(1.5f));

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff8fafc), thumbRect.getX(), thumbRect.getY(),
                                           juce::Colour(0xffcbd5e1), thumbRect.getRight(), thumbRect.getBottom(), false));
    g.fillEllipse(thumbRect);
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawEllipse(thumbRect, 2.0f);
}

void VocalFXProLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    bool isToggled = button.getToggleState();

    juce::Colour base = isToggled ? juce::Colour(0xff0284c7) : backgroundColour;
    if (shouldDrawButtonAsHighlighted) base = base.brighter(0.15f);
    if (shouldDrawButtonAsDown) base = base.darker(0.15f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(isToggled ? juce::Colour(0xff38bdf8) : juce::Colour(0xff334155));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.2f);
}

void VocalFXProLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(juce::Colour(0xff131c31));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(box.hasKeyboardFocus(true) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff223050));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.2f);

    // Arrow icon
    juce::Path arrow;
    float arrowX = buttonX + buttonW * 0.5f;
    float arrowY = buttonY + buttonH * 0.5f;
    arrow.startNewSubPath(arrowX - 4.5f, arrowY - 2.5f);
    arrow.lineTo(arrowX, arrowY + 2.5f);
    arrow.lineTo(arrowX + 4.5f, arrowY - 2.5f);
    g.setColour(juce::Colour(0xff94a3b8));
    g.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void VocalFXProLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                             bool /*shouldDrawButtonAsHighlighted*/,
                                             bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    bool isToggled = button.getToggleState();

    float toggleW = 40.0f;
    float toggleH = 22.0f;
    float toggleX = bounds.getX() + 2.0f;
    float toggleY = bounds.getCentreY() - toggleH * 0.5f;
    auto switchRect = juce::Rectangle<float>(toggleX, toggleY, toggleW, toggleH);

    // Background pill track
    juce::Colour trackCol = isToggled ? juce::Colour(0xff0284c7) : juce::Colour(0xff1e293b);
    g.setColour(trackCol);
    g.fillRoundedRectangle(switchRect, toggleH * 0.5f);

    g.setColour(isToggled ? juce::Colour(0xff38bdf8) : juce::Colour(0xff334155));
    g.drawRoundedRectangle(switchRect, toggleH * 0.5f, 1.0f);

    // Sliding knob / thumb
    float thumbDiameter = toggleH - 4.0f;
    float thumbX = isToggled ? (switchRect.getRight() - thumbDiameter - 2.0f) : (switchRect.getX() + 2.0f);
    float thumbY = switchRect.getY() + 2.0f;

    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbX, thumbY, thumbDiameter, thumbDiameter);

    // Button label text
    auto textRect = bounds.withTrimmedLeft(toggleW + 10.0f);
    g.setColour(isToggled ? juce::Colour(0xfff8fafc) : juce::Colour(0xff94a3b8));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), textRect.toNearestInt(), juce::Justification::centredLeft, 1);
}

// ==============================================================================
// Pitch Waveform Visualizer
// ==============================================================================
VocalFXPitchDisplay::VocalFXPitchDisplay()
{
    inputHistory.resize(70, 0.5f);
    targetHistory.resize(70, 0.5f);
}

void VocalFXPitchDisplay::setPitchData(float in, float target, bool active)
{
    currentIn = in;
    currentTarget = target;
    isActive = active;

    inputHistory.erase(inputHistory.begin());
    inputHistory.push_back(currentIn);

    targetHistory.erase(targetHistory.begin());
    targetHistory.push_back(currentTarget);

    repaint();
}

void VocalFXPitchDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background display screen
    g.setColour(juce::Colour(0xff0c1322));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Subtle grid lines
    g.setColour(juce::Colour(0xff172238));
    for (int y = 1; y <= 4; ++y)
    {
        float lineY = bounds.getY() + bounds.getHeight() * (y / 5.0f);
        g.drawHorizontalLine((int)lineY, bounds.getX(), bounds.getRight());
    }

    if (!isActive)
    {
        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("AUTOTUNE BYPASSED", bounds, juce::Justification::centred);
        return;
    }

    // Draw Target Pitch Curve (Magenta/Purple)
    juce::Path targetPath;
    float step = bounds.getWidth() / (float)(targetHistory.size() - 1);
    for (size_t i = 0; i < targetHistory.size(); ++i)
    {
        float x = bounds.getX() + i * step;
        float y = bounds.getY() + bounds.getHeight() * (1.0f - targetHistory[i]);
        if (i == 0) targetPath.startNewSubPath(x, y);
        else targetPath.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xffc084fc)); // Purple
    g.strokePath(targetPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Draw Input Pitch Curve (Cyan)
    juce::Path inPath;
    for (size_t i = 0; i < inputHistory.size(); ++i)
    {
        float x = bounds.getX() + i * step;
        float y = bounds.getY() + bounds.getHeight() * (1.0f - inputHistory[i]);
        if (i == 0) inPath.startNewSubPath(x, y);
        else inPath.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xff38bdf8)); // Cyan
    g.strokePath(inPath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ==============================================================================
// LED Ladder Level Meter
// ==============================================================================
VocalFXLedMeter::VocalFXLedMeter() {}

void VocalFXLedMeter::setLevel(float peakLinear)
{
    float targetLevel = juce::jlimit(0.0f, 1.0f, peakLinear * 1.5f);
    currentLevel = currentLevel * 0.7f + targetLevel * 0.3f;
    repaint();
}

void VocalFXLedMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d1322));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    const int numLeds = 14;
    float ledSpacing = 2.0f;
    float ledHeight = (bounds.getHeight() - (numLeds + 1) * ledSpacing) / (float)numLeds;
    float activeLeds = currentLevel * (float)numLeds;

    for (int i = 0; i < numLeds; ++i)
    {
        int ledIndexFromBottom = i;
        float y = bounds.getBottom() - (i + 1) * (ledHeight + ledSpacing);
        auto ledRect = juce::Rectangle<float>(bounds.getX() + 2.0f, y, bounds.getWidth() - 4.0f, ledHeight);

        bool isActive = (float)ledIndexFromBottom < activeLeds;

        juce::Colour ledColor;
        if (ledIndexFromBottom >= numLeds - 2)
            ledColor = isActive ? juce::Colour(0xffef4444) : juce::Colour(0xff3b1414); // Red Clip
        else if (ledIndexFromBottom >= numLeds - 5)
            ledColor = isActive ? juce::Colour(0xfff59e0b) : juce::Colour(0xff382405); // Amber Warning
        else
            ledColor = isActive ? juce::Colour(0xff10b981) : juce::Colour(0xff06281b); // Green Normal

        g.setColour(ledColor);
        g.fillRoundedRectangle(ledRect, 1.5f);
    }
}

// ==============================================================================
// VocalFXProAudioProcessorEditor Implementation
// ==============================================================================
VocalFXProAudioProcessorEditor::VocalFXProAudioProcessorEditor(VocalFXProAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&customLookAndFeel);
    // 16:9 Landscape Aspect Ratio (960 x 540)
    setSize(960, 540);

    // 1. Header Section
    titleLabel.setText("Vocal FX Pro", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("PROFESSIONAL VOCAL PROCESSOR", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(subtitleLabel);

    presetDropdown.addItemList({"Clean Vocal", "Pop Vocal", "Rap Vocal", "AutoTune", "Robot Voice"}, 1);
    presetDropdown.setSelectedItemIndex(0, juce::dontSendNotification);
    presetDropdown.setTooltip("Preset Vokal: Pilih cepat preset vokal Clean, Pop, Rap, AutoTune, atau Suara Robot.");
    presetDropdown.onChange = [this]() {
        loadPresetByIndex(presetDropdown.getSelectedItemIndex());
    };
    addAndMakeVisible(presetDropdown);

    prevPresetBtn.setTooltip("Preset Sebelumnya.");
    prevPresetBtn.onClick = [this]() {
        int idx = presetDropdown.getSelectedItemIndex();
        if (idx > 0) presetDropdown.setSelectedItemIndex(idx - 1);
    };
    nextPresetBtn.setTooltip("Preset Berikutnya.");
    nextPresetBtn.onClick = [this]() {
        int idx = presetDropdown.getSelectedItemIndex();
        if (idx < presetDropdown.getNumItems() - 1) presetDropdown.setSelectedItemIndex(idx + 1);
    };
    addAndMakeVisible(prevPresetBtn);
    addAndMakeVisible(nextPresetBtn);

    resetBtn.setTooltip("Reset Parameter: Kembalikan seluruh pengaturan ke nilai bawaan pabrik.");
    resetBtn.onClick = [this]() { resetAllToDefault(); };
    resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    resetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(resetBtn);

    // 2. Volume Section (Left Input, Right Output)
    auto setupRotary = [this](juce::Slider& s, juce::Label& nameL, juce::Label& valL, const juce::Colour& fillCol) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setColour(juce::Slider::rotarySliderFillColourId, fillCol);
        addAndMakeVisible(s);

        nameL.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        nameL.setJustificationType(juce::Justification::centred);
        nameL.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(nameL);

        valL.setFont(juce::Font(juce::FontOptions(10.5f)));
        valL.setJustificationType(juce::Justification::centred);
        valL.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
        addAndMakeVisible(valL);
    };

    setupRotary(inputGainSlider, inputGainLabel, inputGainValLabel, juce::Colour(0xff38bdf8));
    inputGainSlider.setTooltip("Input Preamp Gain: Atur volume masuk mikrofon (-18 dB s/d +18 dB).");
    setupRotary(outputGainSlider, outputGainLabel, outputGainValLabel, juce::Colour(0xffc084fc));
    outputGainSlider.setTooltip("Master Output Gain: Atur level volume akhir setelah diproses (-18 dB s/d +18 dB).");
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    // 3. Main Vocal Section (AutoTune)
    autoTuneToggle.setTooltip("AutoTune ON/OFF: Aktifkan atau nonaktifkan koreksi nada pitch otomatis.");
    addAndMakeVisible(autoTuneToggle);

    keySelector.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 1);
    scaleSelector.addItemList({"Major", "Minor", "Chromatic", "Harmonic Minor", "Melodic Minor"}, 1);
    keySelector.setTooltip("Root Key: Pilih nada dasar lagu yang dinyanyikan.");
    scaleSelector.setTooltip("Musical Scale: Tangga nada musik (Major, Minor, Chromatic).");
    addAndMakeVisible(keySelector);
    addAndMakeVisible(scaleSelector);

    keyLabel.setFont(juce::Font(juce::FontOptions(10.5f)));
    keyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(keyLabel);

    scaleLabel.setFont(juce::Font(juce::FontOptions(10.5f)));
    scaleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(scaleLabel);

    setupRotary(retuneSpeedSlider, retuneSpeedLabel, retuneSpeedValLabel, juce::Colour(0xff38bdf8));
    retuneSpeedSlider.setTooltip("Retune Speed: Kecepatan koreksi nada (0-10 ms Hard Tune tajam, 30-60 ms Natural pop halus).");

    naturalModeBtn.setClickingTogglesState(true);
    naturalModeBtn.setRadioGroupId(101);
    naturalModeBtn.setToggleState(true, juce::dontSendNotification);
    naturalModeBtn.setTooltip("Natural Mode: Koreksi nada yang halus dan alami tanpa terdengar seperti robot.");
    naturalModeBtn.onClick = [this]() {
        if (auto* p = processor.parameters.getParameter("tune_mode"))
            p->setValueNotifyingHost(0.0f);
    };
    addAndMakeVisible(naturalModeBtn);

    hardTuneBtn.setClickingTogglesState(true);
    hardTuneBtn.setRadioGroupId(101);
    hardTuneBtn.setTooltip("Hard Tune Mode: Efek AutoTune instan bergaya dangdut koplo, trap, dan hip-hop modern.");
    hardTuneBtn.onClick = [this]() {
        if (auto* p = processor.parameters.getParameter("tune_mode"))
            p->setValueNotifyingHost(1.0f);
        if (auto* rs = processor.parameters.getParameter("retune"))
            rs->setValueNotifyingHost(1.0f);
    };
    addAndMakeVisible(hardTuneBtn);

    addAndMakeVisible(pitchDisplay);
    pitchInLegend.setFont(juce::Font(juce::FontOptions(10.0f)));
    pitchInLegend.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(pitchInLegend);

    pitchTargetLegend.setFont(juce::Font(juce::FontOptions(10.0f)));
    pitchTargetLegend.setColour(juce::Label::textColourId, juce::Colour(0xffc084fc));
    addAndMakeVisible(pitchTargetLegend);

    // 4. Simple Effects Section
    reverbToggle.setTooltip("Reverb ON/OFF: Aktifkan efek gema ruangan studio.");
    addAndMakeVisible(reverbToggle);
    setupRotary(reverbKnob, reverbKnobLabel, reverbKnobValLabel, juce::Colour(0xff38bdf8));
    reverbKnob.setTooltip("Reverb Mix (%): Banyaknya gema ruangan pada vokal.");

    delayToggle.setTooltip("Delay ON/OFF: Aktifkan efek pantulan echo.");
    addAndMakeVisible(delayToggle);
    setupRotary(delayKnob, delayKnobLabel, delayKnobValLabel, juce::Colour(0xffc084fc));
    delayKnob.setTooltip("Delay Mix (%): Banyaknya pantulan suara echo.");

    compToggle.setTooltip("Compressor ON/OFF: Aktifkan perata dinamika suara vokal.");
    addAndMakeVisible(compToggle);
    setupRotary(compKnob, compKnobLabel, compKnobValLabel, juce::Colour(0xffa855f7));
    compKnob.setTooltip("Compressor Amount: Intensitas kompresi suara.");

    // 5. Bottom Section
    dryWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    dryWetSlider.setTooltip("Master Dry / Wet Mix: Keseimbangan antara suara mikrofon asli (Dry) dan suara terproses efek (Wet).");
    addAndMakeVisible(dryWetSlider);

    dryWetLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    dryWetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(dryWetLabel);

    dryWetValLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    dryWetValLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(dryWetValLabel);

    // Bind parameters
    inputGainAtt = std::make_unique<SliderAttachment>(processor.parameters, "input", inputGainSlider);
    outputGainAtt = std::make_unique<SliderAttachment>(processor.parameters, "output", outputGainSlider);
    retuneSpeedAtt = std::make_unique<SliderAttachment>(processor.parameters, "retune", retuneSpeedSlider);
    reverbMixAtt = std::make_unique<SliderAttachment>(processor.parameters, "reverbMix", reverbKnob);
    delayMixAtt = std::make_unique<SliderAttachment>(processor.parameters, "delayMix", delayKnob);
    compThresholdAtt = std::make_unique<SliderAttachment>(processor.parameters, "threshold", compKnob);
    dryWetAtt = std::make_unique<SliderAttachment>(processor.parameters, "drywet", dryWetSlider);

    autoTuneToggleAtt = std::make_unique<ButtonAttachment>(processor.parameters, "autotune_on", autoTuneToggle);
    reverbToggleAtt = std::make_unique<ButtonAttachment>(processor.parameters, "reverb_on", reverbToggle);
    delayToggleAtt = std::make_unique<ButtonAttachment>(processor.parameters, "delay_on", delayToggle);
    compToggleAtt = std::make_unique<ButtonAttachment>(processor.parameters, "comp_on", compToggle);

    keyAtt = std::make_unique<ComboBoxAttachment>(processor.parameters, "key", keySelector);
    scaleAtt = std::make_unique<ComboBoxAttachment>(processor.parameters, "scale", scaleSelector);

    // Refresh dynamic labels on slider change
    inputGainSlider.onValueChange = [this]() {
        inputGainValLabel.setText(juce::String(inputGainSlider.getValue(), 1) + " dB", juce::dontSendNotification);
    };
    outputGainSlider.onValueChange = [this]() {
        outputGainValLabel.setText(juce::String(outputGainSlider.getValue(), 1) + " dB", juce::dontSendNotification);
    };
    retuneSpeedSlider.onValueChange = [this]() {
        retuneSpeedValLabel.setText(juce::String((int)(retuneSpeedSlider.getValue() * 100.0f)) + " %", juce::dontSendNotification);
    };
    reverbKnob.onValueChange = [this]() {
        reverbKnobValLabel.setText(juce::String((int)(reverbKnob.getValue() * 100.0f)) + " %", juce::dontSendNotification);
    };
    delayKnob.onValueChange = [this]() {
        delayKnobValLabel.setText(juce::String((int)(delayKnob.getValue() * 100.0f)) + " %", juce::dontSendNotification);
    };
    compKnob.onValueChange = [this]() {
        compKnobValLabel.setText(juce::String(compKnob.getValue(), 1) + " dB", juce::dontSendNotification);
    };
    dryWetSlider.onValueChange = [this]() {
        dryWetValLabel.setText(juce::String((int)(dryWetSlider.getValue() * 100.0f)) + " %", juce::dontSendNotification);
    };

    // Initial label refresh
    inputGainSlider.onValueChange();
    outputGainSlider.onValueChange();
    retuneSpeedSlider.onValueChange();
    reverbKnob.onValueChange();
    delayKnob.onValueChange();
    compKnob.onValueChange();
    dryWetSlider.onValueChange();

    startTimerHz(30);
}

VocalFXProAudioProcessorEditor::~VocalFXProAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void VocalFXProAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Clean and minimal dark navy background
    g.fillAll(juce::Colour(0xff090d16));
    juce::ColourGradient bgGrad(juce::Colour(0xff111827), 0, 0,
                               juce::Colour(0xff090d16), 960, 540, false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Top Header separator
    g.setColour(juce::Colour(0xff1e293b));
    g.drawHorizontalLine(58, 20.0f, 940.0f);

    // Left Input Card Container
    juce::Rectangle<float> inputCard(20, 70, 96, 360);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(inputCard, 10.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(inputCard, 10.0f, 1.0f);

    // Right Output Card Container
    juce::Rectangle<float> outputCard(844, 70, 96, 360);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(outputCard, 10.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(outputCard, 10.0f, 1.0f);

    // Center Main AutoTune Card Container
    juce::Rectangle<float> autoTuneCard(126, 70, 708, 210);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(autoTuneCard, 10.0f);
    g.setColour(autoTuneToggle.getToggleState() ? juce::Colour(0xff0284c7).withAlpha(0.6f) : juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(autoTuneCard, 10.0f, 1.2f);

    // Center Bottom 3 Effects Cards (Compressor, Reverb, Delay)
    float effW = 230.0f;
    float effH = 140.0f;
    float effY = 290.0f;

    // 1. Compressor Card
    juce::Rectangle<float> compCard(126, effY, effW, effH);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(compCard, 10.0f);
    g.setColour(compToggle.getToggleState() ? juce::Colour(0xffa855f7).withAlpha(0.6f) : juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(compCard, 10.0f, 1.0f);

    // 2. Reverb Card
    juce::Rectangle<float> revCard(126 + effW + 9, effY, effW, effH);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(revCard, 10.0f);
    g.setColour(reverbToggle.getToggleState() ? juce::Colour(0xff38bdf8).withAlpha(0.6f) : juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(revCard, 10.0f, 1.0f);

    // 3. Delay Card
    juce::Rectangle<float> delayCard(126 + (effW + 9) * 2, effY, effW, effH);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(delayCard, 10.0f);
    g.setColour(delayToggle.getToggleState() ? juce::Colour(0xffc084fc).withAlpha(0.6f) : juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(delayCard, 10.0f, 1.0f);

    // Bottom Bar Container (Dry/Wet)
    juce::Rectangle<float> bottomBar(20, 442, 920, 80);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(bottomBar, 10.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bottomBar, 10.0f, 1.0f);
}

void VocalFXProAudioProcessorEditor::resized()
{
    // 1. Header Layout
    titleLabel.setBounds(24, 10, 150, 26);
    subtitleLabel.setBounds(26, 34, 200, 16);

    presetDropdown.setBounds(330, 16, 260, 30);
    prevPresetBtn.setBounds(294, 16, 30, 30);
    nextPresetBtn.setBounds(596, 16, 30, 30);

    resetBtn.setBounds(860, 16, 80, 30);

    // 2. Volume Section (Left Input)
    inputMeter.setBounds(46, 84, 44, 210);
    inputGainSlider.setBounds(34, 304, 68, 68);
    inputGainLabel.setBounds(24, 376, 88, 16);
    inputGainValLabel.setBounds(24, 394, 88, 16);

    // Volume Section (Right Output)
    outputMeter.setBounds(870, 84, 44, 210);
    outputGainSlider.setBounds(858, 304, 68, 68);
    outputGainLabel.setBounds(848, 376, 88, 16);
    outputGainValLabel.setBounds(848, 394, 88, 16);

    // 3. Main Vocal Section (AutoTune)
    autoTuneToggle.setBounds(142, 84, 160, 28);

    keyLabel.setBounds(142, 120, 40, 16);
    keySelector.setBounds(142, 138, 80, 28);

    scaleLabel.setBounds(232, 120, 50, 16);
    scaleSelector.setBounds(232, 138, 100, 28);

    retuneSpeedSlider.setBounds(344, 94, 92, 92);
    retuneSpeedLabel.setBounds(340, 188, 100, 16);
    retuneSpeedValLabel.setBounds(340, 204, 100, 16);

    naturalModeBtn.setBounds(142, 178, 88, 28);
    hardTuneBtn.setBounds(236, 178, 96, 28);

    pitchDisplay.setBounds(456, 104, 360, 120);
    pitchInLegend.setBounds(460, 84, 90, 18);
    pitchTargetLegend.setBounds(560, 84, 100, 18);

    // 4. Simple Effects Section
    float effW = 230.0f;
    float effY = 290.0f;

    // Compressor
    compToggle.setBounds(138, (int)effY + 12, 150, 28);
    compKnob.setBounds(206, (int)effY + 44, 70, 70);
    compKnobLabel.setBounds(138, (int)effY + 54, 60, 16);
    compKnobValLabel.setBounds(138, (int)effY + 72, 60, 16);

    // Reverb
    reverbToggle.setBounds(138 + (int)effW + 9, (int)effY + 12, 140, 28);
    reverbKnob.setBounds(206 + (int)effW + 9, (int)effY + 44, 70, 70);
    reverbKnobLabel.setBounds(138 + (int)effW + 9, (int)effY + 54, 60, 16);
    reverbKnobValLabel.setBounds(138 + (int)effW + 9, (int)effY + 72, 60, 16);

    // Delay
    delayToggle.setBounds(138 + ((int)effW + 9) * 2, (int)effY + 12, 140, 28);
    delayKnob.setBounds(206 + ((int)effW + 9) * 2, (int)effY + 44, 70, 70);
    delayKnobLabel.setBounds(138 + ((int)effW + 9) * 2, (int)effY + 54, 60, 16);
    delayKnobValLabel.setBounds(138 + ((int)effW + 9) * 2, (int)effY + 72, 60, 16);

    // 5. Bottom Section
    dryWetLabel.setBounds(40, 468, 120, 28);
    dryWetSlider.setBounds(160, 468, 680, 28);
    dryWetValLabel.setBounds(856, 468, 70, 28);
}

void VocalFXProAudioProcessorEditor::timerCallback()
{
    // Update real-time LED meters
    inputMeter.setLevel(processor.inputPeak.load());
    outputMeter.setLevel(processor.outputPeak.load());

    // Connect real-time pitch data from AudioProcessor
    bool isTuning = autoTuneToggle.getToggleState();
    float detectedHz = processor.getDetectedPitch();
    float targetHz = processor.getTargetPitch();

    // Map frequency (approx 80 Hz - 800 Hz) logarithmically to 0.0 - 1.0 vertical display space
    auto freqToNorm = [](float hz) -> float {
        if (hz <= 50.0f) return 0.5f;
        float norm = (std::log10(juce::jlimit(65.0f, 1000.0f, hz)) - std::log10(65.0f)) / (std::log10(1000.0f) - std::log10(65.0f));
        return juce::jlimit(0.05f, 0.95f, norm);
    };

    float inNorm = freqToNorm(detectedHz);
    float targetNorm = freqToNorm(targetHz);

    // If vocal is silent or unvoiced, show idle baseline
    if (detectedHz <= 40.0f)
    {
        inNorm = 0.5f;
        targetNorm = 0.5f;
    }

    pitchDisplay.setPitchData(inNorm, targetNorm, isTuning);
}

void VocalFXProAudioProcessorEditor::loadPresetByIndex(int index)
{
    // Presets: Clean Vocal, Pop Vocal, Rap Vocal, AutoTune, Robot Voice
    switch (index)
    {
        case 0: // Clean Vocal
            if (auto* p = processor.parameters.getParameter("autotune_on")) p->setValueNotifyingHost(0.0f);
            if (auto* p = processor.parameters.getParameter("retune")) p->setValueNotifyingHost(0.3f);
            if (auto* p = processor.parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("threshold")) p->setValueNotifyingHost(processor.parameters.getParameterRange("threshold").convertTo0to1(-16.0f));
            if (auto* p = processor.parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.12f);
            if (auto* p = processor.parameters.getParameter("delay_on")) p->setValueNotifyingHost(0.0f);
            if (auto* p = processor.parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            naturalModeBtn.setToggleState(true, juce::sendNotification);
            break;

        case 1: // Pop Vocal
            if (auto* p = processor.parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("retune")) p->setValueNotifyingHost(0.65f);
            if (auto* p = processor.parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("threshold")) p->setValueNotifyingHost(processor.parameters.getParameterRange("threshold").convertTo0to1(-20.0f));
            if (auto* p = processor.parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.22f);
            if (auto* p = processor.parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.15f);
            if (auto* p = processor.parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            naturalModeBtn.setToggleState(true, juce::sendNotification);
            break;

        case 2: // Rap Vocal
            if (auto* p = processor.parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("retune")) p->setValueNotifyingHost(0.85f);
            if (auto* p = processor.parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("threshold")) p->setValueNotifyingHost(processor.parameters.getParameterRange("threshold").convertTo0to1(-24.0f));
            if (auto* p = processor.parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.10f);
            if (auto* p = processor.parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.10f);
            if (auto* p = processor.parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            hardTuneBtn.setToggleState(true, juce::sendNotification);
            break;

        case 3: // AutoTune
            if (auto* p = processor.parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("retune")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("threshold")) p->setValueNotifyingHost(processor.parameters.getParameterRange("threshold").convertTo0to1(-18.0f));
            if (auto* p = processor.parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.18f);
            if (auto* p = processor.parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.12f);
            if (auto* p = processor.parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            hardTuneBtn.setToggleState(true, juce::sendNotification);
            break;

        case 4: // Robot Voice
            if (auto* p = processor.parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("retune")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("threshold")) p->setValueNotifyingHost(processor.parameters.getParameterRange("threshold").convertTo0to1(-22.0f));
            if (auto* p = processor.parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.25f);
            if (auto* p = processor.parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = processor.parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.20f);
            if (auto* p = processor.parameters.getParameter("robot")) p->setValueNotifyingHost(0.85f);
            hardTuneBtn.setToggleState(true, juce::sendNotification);
            break;

        default:
            break;
    }
}

void VocalFXProAudioProcessorEditor::resetAllToDefault()
{
    if (auto* p = processor.parameters.getParameter("input")) p->setValueNotifyingHost(processor.parameters.getParameterRange("input").convertTo0to1(0.0f));
    if (auto* p = processor.parameters.getParameter("output")) p->setValueNotifyingHost(processor.parameters.getParameterRange("output").convertTo0to1(0.0f));
    if (auto* p = processor.parameters.getParameter("drywet")) p->setValueNotifyingHost(1.0f);
    if (auto* p = processor.parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
    if (auto* p = processor.parameters.getParameter("retune")) p->setValueNotifyingHost(0.5f);
    if (auto* p = processor.parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
    if (auto* p = processor.parameters.getParameter("threshold")) p->setValueNotifyingHost(processor.parameters.getParameterRange("threshold").convertTo0to1(-18.0f));
    if (auto* p = processor.parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
    if (auto* p = processor.parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.18f);
    if (auto* p = processor.parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
    if (auto* p = processor.parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.12f);
    if (auto* p = processor.parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
    naturalModeBtn.setToggleState(true, juce::sendNotification);
}
