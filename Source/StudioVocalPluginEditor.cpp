#include "StudioVocalPluginEditor.h"
#include <cmath>

namespace LiveBus
{

StudioVocalPluginEditor::StudioVocalPluginEditor(RNNoiseVST3Plugin& plugin)
    : AudioProcessorEditor(plugin),
      processor(plugin),
      eqCurveView(*this),
      compCurveView(*this)
{
    setOpaque(true);

    // --- Header ---
    titleLabel.setText("LIVEBUS RNNOISE VOCAL STUDIO", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(titleLabel);

    subTitleLabel.setText("AI Suppression & Broadcast DSP Engine | 48 kHz Native", juce::dontSendNotification);
    subTitleLabel.setFont(juce::FontOptions(11.0f));
    subTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
    addAndMakeVisible(subTitleLabel);

    setupToggle(masterBypassToggle, "MASTER BYPASS");
    masterBypassToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffef4444));
    masterBypassToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff87171));
    addAndMakeVisible(masterBypassToggle);
    masterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "master_bypass", masterBypassToggle);

    // --- Tab Navigation ---
    const char* tabNames[NumTabs] = {
        "HOME", "RNNOISE", "ENHANCE", "EQ 8-BAND", "COMPRESSOR", "DE-ESSER", "GATE", "LIMITER", "PRESETS"
    };

    for (int i = 0; i < NumTabs; ++i)
    {
        tabButtons[(size_t)i].setButtonText(tabNames[i]);
        tabButtons[(size_t)i].addListener(this);
        tabButtons[(size_t)i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
        tabButtons[(size_t)i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(tabButtons[(size_t)i]);
    }

    // --- Ladder Meters ---
    addAndMakeVisible(inMeterL);
    addAndMakeVisible(inMeterR);
    addAndMakeVisible(gateGrMeter);
    addAndMakeVisible(compGrMeter);
    addAndMakeVisible(deEsserGrMeter);
    addAndMakeVisible(limiterGrMeter);
    addAndMakeVisible(outMeterL);
    addAndMakeVisible(outMeterR);

    // --- Visualizers ---
    addChildComponent(eqCurveView);
    addChildComponent(compCurveView);

    // --- HOME Group ---
    addChildComponent(homeStatusGroup);
    vadMetricLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    vadMetricLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addChildComponent(vadMetricLabel);

    latencyMetricLabel.setFont(juce::FontOptions(12.0f));
    latencyMetricLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addChildComponent(latencyMetricLabel);

    sampleRateMetricLabel.setFont(juce::FontOptions(12.0f));
    sampleRateMetricLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addChildComponent(sampleRateMetricLabel);

    setupToggle(homeRnnoiseToggle, "RNNoise AI Noise Reduction");
    setupToggle(homeHpfToggle, "High-Pass Filter");
    setupToggle(homeGateToggle, "Noise Gate / Expander");
    setupToggle(homeEqToggle, "8-Band Parametric EQ");
    setupToggle(homeDeEsserToggle, "Vocal De-Esser");
    setupToggle(homeCompToggle, "Vocal Compressor");
    setupToggle(homeEnhanceToggle, "Vocal Enhancer (Air/Warmth)");
    setupToggle(homeLimiterToggle, "Brickwall Output Limiter");

    addChildComponent(homeRnnoiseToggle);
    addChildComponent(homeHpfToggle);
    addChildComponent(homeGateToggle);
    addChildComponent(homeEqToggle);
    addChildComponent(homeDeEsserToggle);
    addChildComponent(homeCompToggle);
    addChildComponent(homeEnhanceToggle);
    addChildComponent(homeLimiterToggle);

    // Attach home toggles
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "rnnoise_bypass", homeRnnoiseToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "hpf_bypass", homeHpfToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "gate_bypass", homeGateToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "eq_bypass", homeEqToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "deesser_bypass", homeDeEsserToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "comp_bypass", homeCompToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "enhance_bypass", homeEnhanceToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "limiter_bypass", homeLimiterToggle));

    // --- Tab 2: RNNOISE ---
    setupToggle(rnnoiseBypassBtn, "Bypass RNNoise Engine");
    setupToggle(vadToggle, "Enable Intelligent VAD Gate");
    setupRotary(wetSlider, wetLabel, "RNNoise Wet Mix", "%", juce::Colour(0xff00f5d4));
    setupRotary(vadThresholdSlider, vadThresholdLabel, "VAD Threshold", "", juce::Colour(0xff38bdf8));
    setupRotary(gracePeriodSlider, gracePeriodLabel, "Hold / Grace", " ms", juce::Colour(0xff818cf8));

    addChildComponent(rnnoiseBypassBtn);
    addChildComponent(vadToggle);
    addChildComponent(wetSlider);
    addChildComponent(wetLabel);
    addChildComponent(vadThresholdSlider);
    addChildComponent(vadThresholdLabel);
    addChildComponent(gracePeriodSlider);
    addChildComponent(gracePeriodLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "rnnoise_bypass", rnnoiseBypassBtn));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "enable_vad", vadToggle));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "wet_percent", wetSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "vad_threshold", vadThresholdSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "grace_period_ms", gracePeriodSlider));

    // --- Tab 3: VOCAL ENHANCE ---
    setupToggle(enhanceBypassBtn, "Bypass Vocal Enhancer");
    setupRotary(warmthSlider, warmthLabel, "Warmth (280Hz)", "%", juce::Colour(0xfff97316));
    setupRotary(bodySlider, bodyLabel, "Body (500Hz)", "%", juce::Colour(0xffeab308));
    setupRotary(claritySlider, clarityLabel, "Clarity (3.2k)", "%", juce::Colour(0xff06b6d4));
    setupRotary(presenceSlider, presenceLabel, "Presence (6.5k)", "%", juce::Colour(0xff3b82f6));
    setupRotary(airSlider, airLabel, "Air Sheen (13k)", "%", juce::Colour(0xffa855f7));

    addChildComponent(enhanceBypassBtn);
    addChildComponent(warmthSlider); addChildComponent(warmthLabel);
    addChildComponent(bodySlider); addChildComponent(bodyLabel);
    addChildComponent(claritySlider); addChildComponent(clarityLabel);
    addChildComponent(presenceSlider); addChildComponent(presenceLabel);
    addChildComponent(airSlider); addChildComponent(airLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "enhance_bypass", enhanceBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "enhance_warmth", warmthSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "enhance_body", bodySlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "enhance_clarity", claritySlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "enhance_presence", presenceSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "enhance_air", airSlider));

    // --- Tab 4: 8-BAND EQ ---
    setupToggle(eqBypassBtn, "Bypass 8-Band Equalizer");
    addChildComponent(eqBypassBtn);
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "eq_bypass", eqBypassBtn));

    // --- Tab 5: COMPRESSOR ---
    setupToggle(compBypassBtn, "Bypass Compressor");
    setupToggle(compAutoReleaseBtn, "Auto-Release");
    setupRotary(compThreshSlider, compThreshLabel, "Threshold", " dB", juce::Colour(0xfff43f5e));
    setupRotary(compRatioSlider, compRatioLabel, "Ratio", ":1", juce::Colour(0xfff43f5e));
    setupRotary(compAttackSlider, compAttackLabel, "Attack", " ms", juce::Colour(0xfff59e0b));
    setupRotary(compReleaseSlider, compReleaseLabel, "Release", " ms", juce::Colour(0xfff59e0b));
    setupRotary(compMakeupSlider, compMakeupLabel, "Makeup", " dB", juce::Colour(0xff10b981));
    setupRotary(compMixSlider, compMixLabel, "Parallel Mix", "%", juce::Colour(0xff06b6d4));

    compKneeCombo.addItem("Soft Knee", 1);
    compKneeCombo.addItem("Normal Knee", 2);
    compKneeCombo.addItem("Hard Knee", 3);
    compKneeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    compKneeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);

    addChildComponent(compBypassBtn);
    addChildComponent(compAutoReleaseBtn);
    addChildComponent(compThreshSlider); addChildComponent(compThreshLabel);
    addChildComponent(compRatioSlider); addChildComponent(compRatioLabel);
    addChildComponent(compAttackSlider); addChildComponent(compAttackLabel);
    addChildComponent(compReleaseSlider); addChildComponent(compReleaseLabel);
    addChildComponent(compMakeupSlider); addChildComponent(compMakeupLabel);
    addChildComponent(compMixSlider); addChildComponent(compMixLabel);
    addChildComponent(compKneeCombo);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "comp_bypass", compBypassBtn));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "comp_autorelease", compAutoReleaseBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_threshold", compThreshSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_ratio", compRatioSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_attack", compAttackSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_release", compReleaseSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_makeup", compMakeupSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_mix", compMixSlider));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "comp_knee", compKneeCombo));

    // --- Tab 6: DE-ESSER ---
    setupToggle(deEsserBypassBtn, "Bypass De-Esser");
    setupRotary(deEsserThreshSlider, deEsserThreshLabel, "Threshold", " dB", juce::Colour(0xffe879f9));
    setupRotary(deEsserFreqSlider, deEsserFreqLabel, "Frequency", " Hz", juce::Colour(0xffe879f9));
    setupRotary(deEsserRangeSlider, deEsserRangeLabel, "Max Range", " dB", juce::Colour(0xffa855f7));
    deEsserTypeCombo.addItem("Bell Filter", 1);
    deEsserTypeCombo.addItem("High-Shelf Filter", 2);
    deEsserTypeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    deEsserTypeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);

    addChildComponent(deEsserBypassBtn);
    addChildComponent(deEsserThreshSlider); addChildComponent(deEsserThreshLabel);
    addChildComponent(deEsserFreqSlider); addChildComponent(deEsserFreqLabel);
    addChildComponent(deEsserRangeSlider); addChildComponent(deEsserRangeLabel);
    addChildComponent(deEsserTypeCombo);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "deesser_bypass", deEsserBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "deesser_threshold", deEsserThreshSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "deesser_freq", deEsserFreqSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "deesser_range", deEsserRangeSlider));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "deesser_type", deEsserTypeCombo));

    // --- Tab 7: NOISE GATE ---
    setupToggle(gateBypassBtn, "Bypass Noise Gate");
    setupRotary(gateThreshSlider, gateThreshLabel, "Open Thresh", " dB", juce::Colour(0xfff59e0b));
    setupRotary(gateAttackSlider, gateAttackLabel, "Attack", " ms", juce::Colour(0xfff59e0b));
    setupRotary(gateHoldSlider, gateHoldLabel, "Hold", " ms", juce::Colour(0xfffbbf24));
    setupRotary(gateReleaseSlider, gateReleaseLabel, "Release", " ms", juce::Colour(0xfffbbf24));
    setupRotary(gateFloorSlider, gateFloorLabel, "Floor / Depth", " dB", juce::Colour(0xffd97706));

    addChildComponent(gateBypassBtn);
    addChildComponent(gateThreshSlider); addChildComponent(gateThreshLabel);
    addChildComponent(gateAttackSlider); addChildComponent(gateAttackLabel);
    addChildComponent(gateHoldSlider); addChildComponent(gateHoldLabel);
    addChildComponent(gateReleaseSlider); addChildComponent(gateReleaseLabel);
    addChildComponent(gateFloorSlider); addChildComponent(gateFloorLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "gate_bypass", gateBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gate_threshold", gateThreshSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gate_attack", gateAttackSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gate_hold", gateHoldSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gate_release", gateReleaseSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gate_floor", gateFloorSlider));

    // --- Tab 8: LIMITER & TRIM ---
    setupToggle(limiterBypassBtn, "Bypass Brickwall Limiter");
    setupRotary(inputGainSlider, inputGainLabel, "Input Trim", " dB", juce::Colour(0xff38bdf8));
    setupRotary(hpfFreqSlider, hpfFreqLabel, "HPF Cutoff", " Hz", juce::Colour(0xff06b6d4));
    setupRotary(limiterCeilingSlider, limiterCeilingLabel, "Ceiling", " dBFS", juce::Colour(0xffef4444));
    setupRotary(limiterReleaseSlider, limiterReleaseLabel, "Release", " ms", juce::Colour(0xffef4444));
    setupRotary(outputGainSlider, outputGainLabel, "Master Out", " dB", juce::Colour(0xff10b981));

    hpfSlopeCombo.addItem("12 dB/oct", 1);
    hpfSlopeCombo.addItem("24 dB/oct", 2);
    hpfSlopeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    hpfSlopeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);

    addChildComponent(limiterBypassBtn);
    addChildComponent(inputGainSlider); addChildComponent(inputGainLabel);
    addChildComponent(hpfFreqSlider); addChildComponent(hpfFreqLabel);
    addChildComponent(hpfSlopeCombo);
    addChildComponent(limiterCeilingSlider); addChildComponent(limiterCeilingLabel);
    addChildComponent(limiterReleaseSlider); addChildComponent(limiterReleaseLabel);
    addChildComponent(outputGainSlider); addChildComponent(outputGainLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "limiter_bypass", limiterBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "input_gain_db", inputGainSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "hpf_freq", hpfFreqSlider));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "hpf_slope", hpfSlopeCombo));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "limiter_ceiling", limiterCeilingSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "limiter_release", limiterReleaseSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "output_gain_db", outputGainSlider));

    // --- Tab 9: PRESETS ---
    presetCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    presetCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    for (int p = 0; p < VocalStudioPresetManager::NumFactoryPresets; ++p)
    {
        const auto& pr = VocalStudioPresetManager::getFactoryPreset(p);
        presetCombo.addItem(juce::String(p + 1) + ". " + pr.name + " (" + pr.category + ")", p + 1);
    }
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    presetCombo.addListener(this);

    loadPresetBtn.addListener(this);
    loadPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f5d4));
    loadPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0b0f17));

    savePresetBtn.addListener(this);
    savePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));

    importPresetBtn.addListener(this);
    importPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));

    presetInfoLabel.setFont(juce::FontOptions(12.0f));
    presetInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    presetInfoLabel.setText(VocalStudioPresetManager::getFactoryPreset(0).description, juce::dontSendNotification);

    addChildComponent(presetCombo);
    addChildComponent(loadPresetBtn);
    addChildComponent(savePresetBtn);
    addChildComponent(importPresetBtn);
    addChildComponent(presetInfoLabel);

    // Initial Tab Selection
    buttonClicked(&tabButtons[TabHome]);

    setSize(820, 520);
    startTimerHz(25);
}

StudioVocalPluginEditor::~StudioVocalPluginEditor()
{
    stopTimer();
}

void StudioVocalPluginEditor::setupRotary(juce::Slider& s, juce::Label& l, const juce::String& text,
                                         const juce::String& suffix, juce::Colour fillCol)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::rotarySliderFillColourId, fillCol);
    s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1e293b));
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff1f5f9));

    l.setText(text, juce::dontSendNotification);
    l.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
}

void StudioVocalPluginEditor::setupToggle(juce::ToggleButton& btn, const juce::String& text)
{
    btn.setButtonText(text);
    btn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe2e8f0));
    btn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));
}

void StudioVocalPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0f17)); // Modern deep carbon dark

    auto bounds = getLocalBounds().toFloat();

    // Top Header Banner
    auto header = bounds.removeFromTop(54.0f);
    g.setColour(juce::Colour(0xff111827));
    g.fillRect(header);
    g.setColour(juce::Colour(0xff1f293d));
    g.drawHorizontalLine(54, 0.0f, bounds.getWidth());

    // Tab Navigation Bar
    auto navRect = juce::Rectangle<float>(0.0f, 54.0f, bounds.getWidth(), 36.0f);
    g.setColour(juce::Colour(0xff0d131f));
    g.fillRect(navRect);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawHorizontalLine(90, 0.0f, bounds.getWidth());

    // Center Display Area Card
    auto contentCard = juce::Rectangle<float>(14.0f, 100.0f, bounds.getWidth() - 170.0f, bounds.getHeight() - 114.0f);
    g.setColour(juce::Colour(0xff111827));
    g.fillRoundedRectangle(contentCard, 6.0f);
    g.setColour(juce::Colour(0xff1f293d));
    g.drawRoundedRectangle(contentCard, 6.0f, 1.0f);

    // Right Meter Bay Card
    auto meterCard = juce::Rectangle<float>(bounds.getWidth() - 146.0f, 100.0f, 132.0f, bounds.getHeight() - 114.0f);
    g.setColour(juce::Colour(0xff0e1422));
    g.fillRoundedRectangle(meterCard, 6.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(meterCard, 6.0f, 1.0f);

    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
    g.drawText("OUTPUT DYNAMICS", (int)meterCard.getX(), (int)meterCard.getY() + 4, (int)meterCard.getWidth(), 12, juce::Justification::centred);

    // Highlight Active Tab Button
    for (int i = 0; i < NumTabs; ++i)
    {
        if (i == currentTab)
        {
            tabButtons[(size_t)i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f5d4));
            tabButtons[(size_t)i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0b0f17));
        }
        else
        {
            tabButtons[(size_t)i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
            tabButtons[(size_t)i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        }
    }
}

void StudioVocalPluginEditor::resized()
{
    titleLabel.setBounds(18, 6, 420, 24);
    subTitleLabel.setBounds(18, 28, 420, 16);
    masterBypassToggle.setBounds(getWidth() - 160, 14, 140, 26);

    // Tab buttons
    const int tabW = (getWidth() - 20) / NumTabs;
    for (int i = 0; i < NumTabs; ++i)
        tabButtons[(size_t)i].setBounds(10 + i * tabW, 58, tabW - 4, 28);

    // Right Meter Bay (4 Stereo & GR Ladders)
    const int mY = 120;
    const int mH = getHeight() - 140;
    const int mW = 14;
    const int startX = getWidth() - 142;

    inMeterL.setBounds(startX, mY, mW, mH);
    inMeterR.setBounds(startX + 16, mY, mW, mH);
    gateGrMeter.setBounds(startX + 34, mY, mW, mH);
    compGrMeter.setBounds(startX + 50, mY, mW, mH);
    deEsserGrMeter.setBounds(startX + 66, mY, mW, mH);
    limiterGrMeter.setBounds(startX + 82, mY, mW, mH);
    outMeterL.setBounds(startX + 100, mY, mW, mH);
    outMeterR.setBounds(startX + 116, mY, mW, mH);

    // Content bounds
    const auto card = juce::Rectangle<int>(24, 110, getWidth() - 190, getHeight() - 130);

    // HOME Tab Layout
    homeStatusGroup.setBounds(card.getX(), card.getY(), card.getWidth(), 120);
    vadMetricLabel.setBounds(card.getX() + 16, card.getY() + 30, 400, 24);
    latencyMetricLabel.setBounds(card.getX() + 16, card.getY() + 58, 400, 20);
    sampleRateMetricLabel.setBounds(card.getX() + 16, card.getY() + 80, 400, 20);

    const int toggleStartX = card.getX() + 10;
    const int toggleStartY = card.getY() + 140;
    homeRnnoiseToggle.setBounds(toggleStartX, toggleStartY, 220, 26);
    homeHpfToggle.setBounds(toggleStartX, toggleStartY + 32, 220, 26);
    homeGateToggle.setBounds(toggleStartX, toggleStartY + 64, 220, 26);
    homeEqToggle.setBounds(toggleStartX, toggleStartY + 96, 220, 26);

    homeDeEsserToggle.setBounds(toggleStartX + 260, toggleStartY, 220, 26);
    homeCompToggle.setBounds(toggleStartX + 260, toggleStartY + 32, 220, 26);
    homeEnhanceToggle.setBounds(toggleStartX + 260, toggleStartY + 64, 220, 26);
    homeLimiterToggle.setBounds(toggleStartX + 260, toggleStartY + 96, 220, 26);

    // RNNOISE Tab Layout
    rnnoiseBypassBtn.setBounds(card.getX() + 20, card.getY() + 20, 180, 26);
    vadToggle.setBounds(card.getX() + 220, card.getY() + 20, 220, 26);
    wetSlider.setBounds(card.getX() + 30, card.getY() + 70, 110, 110);
    wetLabel.setBounds(card.getX() + 30, card.getY() + 175, 110, 20);
    vadThresholdSlider.setBounds(card.getX() + 180, card.getY() + 70, 110, 110);
    vadThresholdLabel.setBounds(card.getX() + 180, card.getY() + 175, 110, 20);
    gracePeriodSlider.setBounds(card.getX() + 330, card.getY() + 70, 110, 110);
    gracePeriodLabel.setBounds(card.getX() + 330, card.getY() + 175, 110, 20);

    // ENHANCE Tab Layout
    enhanceBypassBtn.setBounds(card.getX() + 20, card.getY() + 20, 200, 26);
    const int enhKnobW = 95;
    warmthSlider.setBounds(card.getX() + 10, card.getY() + 70, enhKnobW, enhKnobW);
    warmthLabel.setBounds(card.getX() + 10, card.getY() + 165, enhKnobW, 20);
    bodySlider.setBounds(card.getX() + 115, card.getY() + 70, enhKnobW, enhKnobW);
    bodyLabel.setBounds(card.getX() + 115, card.getY() + 165, enhKnobW, 20);
    claritySlider.setBounds(card.getX() + 220, card.getY() + 70, enhKnobW, enhKnobW);
    clarityLabel.setBounds(card.getX() + 220, card.getY() + 165, enhKnobW, 20);
    presenceSlider.setBounds(card.getX() + 325, card.getY() + 70, enhKnobW, enhKnobW);
    presenceLabel.setBounds(card.getX() + 325, card.getY() + 165, enhKnobW, 20);
    airSlider.setBounds(card.getX() + 430, card.getY() + 70, enhKnobW, enhKnobW);
    airLabel.setBounds(card.getX() + 430, card.getY() + 165, enhKnobW, 20);

    // EQ Tab Layout
    eqBypassBtn.setBounds(card.getX() + 20, card.getY() + 10, 180, 24);
    eqCurveView.setBounds(card.getX() + 10, card.getY() + 40, card.getWidth() - 20, card.getHeight() - 50);

    // COMPRESSOR Tab Layout
    compBypassBtn.setBounds(card.getX() + 20, card.getY() + 10, 160, 24);
    compAutoReleaseBtn.setBounds(card.getX() + 200, card.getY() + 10, 140, 24);
    compKneeCombo.setBounds(card.getX() + 360, card.getY() + 10, 140, 24);

    compCurveView.setBounds(card.getX() + 10, card.getY() + 44, 240, card.getHeight() - 60);

    const int cKx = card.getX() + 265;
    const int cKw = 90;
    compThreshSlider.setBounds(cKx, card.getY() + 44, cKw, cKw);
    compThreshLabel.setBounds(cKx, card.getY() + 130, cKw, 18);
    compRatioSlider.setBounds(cKx + 100, card.getY() + 44, cKw, cKw);
    compRatioLabel.setBounds(cKx + 100, card.getY() + 130, cKw, 18);
    compAttackSlider.setBounds(cKx + 200, card.getY() + 44, cKw, cKw);
    compAttackLabel.setBounds(cKx + 200, card.getY() + 130, cKw, 18);

    compReleaseSlider.setBounds(cKx, card.getY() + 160, cKw, cKw);
    compReleaseLabel.setBounds(cKx, card.getY() + 246, cKw, 18);
    compMakeupSlider.setBounds(cKx + 100, card.getY() + 160, cKw, cKw);
    compMakeupLabel.setBounds(cKx + 100, card.getY() + 246, cKw, 18);
    compMixSlider.setBounds(cKx + 200, card.getY() + 160, cKw, cKw);
    compMixLabel.setBounds(cKx + 200, card.getY() + 246, cKw, 18);

    // DE-ESSER Tab Layout
    deEsserBypassBtn.setBounds(card.getX() + 20, card.getY() + 20, 180, 26);
    deEsserTypeCombo.setBounds(card.getX() + 220, card.getY() + 20, 160, 26);
    const int deW = 110;
    deEsserThreshSlider.setBounds(card.getX() + 30, card.getY() + 80, deW, deW);
    deEsserThreshLabel.setBounds(card.getX() + 30, card.getY() + 185, deW, 20);
    deEsserFreqSlider.setBounds(card.getX() + 180, card.getY() + 80, deW, deW);
    deEsserFreqLabel.setBounds(card.getX() + 180, card.getY() + 185, deW, 20);
    deEsserRangeSlider.setBounds(card.getX() + 330, card.getY() + 80, deW, deW);
    deEsserRangeLabel.setBounds(card.getX() + 330, card.getY() + 185, deW, 20);

    // NOISE GATE Tab Layout
    gateBypassBtn.setBounds(card.getX() + 20, card.getY() + 20, 180, 26);
    const int gKw = 95;
    gateThreshSlider.setBounds(card.getX() + 10, card.getY() + 70, gKw, gKw);
    gateThreshLabel.setBounds(card.getX() + 10, card.getY() + 165, gKw, 20);
    gateAttackSlider.setBounds(card.getX() + 115, card.getY() + 70, gKw, gKw);
    gateAttackLabel.setBounds(card.getX() + 115, card.getY() + 165, gKw, 20);
    gateHoldSlider.setBounds(card.getX() + 220, card.getY() + 70, gKw, gKw);
    gateHoldLabel.setBounds(card.getX() + 220, card.getY() + 165, gKw, 20);
    gateReleaseSlider.setBounds(card.getX() + 325, card.getY() + 70, gKw, gKw);
    gateReleaseLabel.setBounds(card.getX() + 325, card.getY() + 165, gKw, 20);
    gateFloorSlider.setBounds(card.getX() + 430, card.getY() + 70, gKw, gKw);
    gateFloorLabel.setBounds(card.getX() + 430, card.getY() + 165, gKw, 20);

    // LIMITER & TRIM Tab Layout
    limiterBypassBtn.setBounds(card.getX() + 20, card.getY() + 20, 180, 26);
    hpfSlopeCombo.setBounds(card.getX() + 220, card.getY() + 20, 130, 26);
    const int limW = 95;
    inputGainSlider.setBounds(card.getX() + 10, card.getY() + 70, limW, limW);
    inputGainLabel.setBounds(card.getX() + 10, card.getY() + 165, limW, 20);
    hpfFreqSlider.setBounds(card.getX() + 115, card.getY() + 70, limW, limW);
    hpfFreqLabel.setBounds(card.getX() + 115, card.getY() + 165, limW, 20);
    limiterCeilingSlider.setBounds(card.getX() + 220, card.getY() + 70, limW, limW);
    limiterCeilingLabel.setBounds(card.getX() + 220, card.getY() + 165, limW, 20);
    limiterReleaseSlider.setBounds(card.getX() + 325, card.getY() + 70, limW, limW);
    limiterReleaseLabel.setBounds(card.getX() + 325, card.getY() + 165, limW, 20);
    outputGainSlider.setBounds(card.getX() + 430, card.getY() + 70, limW, limW);
    outputGainLabel.setBounds(card.getX() + 430, card.getY() + 165, limW, 20);

    // PRESETS Tab Layout
    presetCombo.setBounds(card.getX() + 30, card.getY() + 30, 360, 28);
    loadPresetBtn.setBounds(card.getX() + 400, card.getY() + 30, 160, 28);
    savePresetBtn.setBounds(card.getX() + 30, card.getY() + 80, 180, 28);
    importPresetBtn.setBounds(card.getX() + 220, card.getY() + 80, 180, 28);
    presetInfoLabel.setBounds(card.getX() + 30, card.getY() + 130, card.getWidth() - 60, 80);
}

void StudioVocalPluginEditor::buttonClicked(juce::Button* button)
{
    for (int i = 0; i < NumTabs; ++i)
    {
        if (button == &tabButtons[(size_t)i])
        {
            currentTab = i;

            // Visibility switches
            const bool isHome = (currentTab == TabHome);
            homeStatusGroup.setVisible(isHome);
            vadMetricLabel.setVisible(isHome);
            latencyMetricLabel.setVisible(isHome);
            sampleRateMetricLabel.setVisible(isHome);
            homeRnnoiseToggle.setVisible(isHome);
            homeHpfToggle.setVisible(isHome);
            homeGateToggle.setVisible(isHome);
            homeEqToggle.setVisible(isHome);
            homeDeEsserToggle.setVisible(isHome);
            homeCompToggle.setVisible(isHome);
            homeEnhanceToggle.setVisible(isHome);
            homeLimiterToggle.setVisible(isHome);

            const bool isRnnoise = (currentTab == TabRNNoise);
            rnnoiseBypassBtn.setVisible(isRnnoise);
            vadToggle.setVisible(isRnnoise);
            wetSlider.setVisible(isRnnoise);
            wetLabel.setVisible(isRnnoise);
            vadThresholdSlider.setVisible(isRnnoise);
            vadThresholdLabel.setVisible(isRnnoise);
            gracePeriodSlider.setVisible(isRnnoise);
            gracePeriodLabel.setVisible(isRnnoise);

            const bool isEnhance = (currentTab == TabEnhance);
            enhanceBypassBtn.setVisible(isEnhance);
            warmthSlider.setVisible(isEnhance); warmthLabel.setVisible(isEnhance);
            bodySlider.setVisible(isEnhance); bodyLabel.setVisible(isEnhance);
            claritySlider.setVisible(isEnhance); clarityLabel.setVisible(isEnhance);
            presenceSlider.setVisible(isEnhance); presenceLabel.setVisible(isEnhance);
            airSlider.setVisible(isEnhance); airLabel.setVisible(isEnhance);

            const bool isEq = (currentTab == TabEQ);
            eqBypassBtn.setVisible(isEq);
            eqCurveView.setVisible(isEq);

            const bool isComp = (currentTab == TabComp);
            compBypassBtn.setVisible(isComp);
            compAutoReleaseBtn.setVisible(isComp);
            compKneeCombo.setVisible(isComp);
            compCurveView.setVisible(isComp);
            compThreshSlider.setVisible(isComp); compThreshLabel.setVisible(isComp);
            compRatioSlider.setVisible(isComp); compRatioLabel.setVisible(isComp);
            compAttackSlider.setVisible(isComp); compAttackLabel.setVisible(isComp);
            compReleaseSlider.setVisible(isComp); compReleaseLabel.setVisible(isComp);
            compMakeupSlider.setVisible(isComp); compMakeupLabel.setVisible(isComp);
            compMixSlider.setVisible(isComp); compMixLabel.setVisible(isComp);

            const bool isDeEsser = (currentTab == TabDeEsser);
            deEsserBypassBtn.setVisible(isDeEsser);
            deEsserTypeCombo.setVisible(isDeEsser);
            deEsserThreshSlider.setVisible(isDeEsser); deEsserThreshLabel.setVisible(isDeEsser);
            deEsserFreqSlider.setVisible(isDeEsser); deEsserFreqLabel.setVisible(isDeEsser);
            deEsserRangeSlider.setVisible(isDeEsser); deEsserRangeLabel.setVisible(isDeEsser);

            const bool isGate = (currentTab == TabGate);
            gateBypassBtn.setVisible(isGate);
            gateThreshSlider.setVisible(isGate); gateThreshLabel.setVisible(isGate);
            gateAttackSlider.setVisible(isGate); gateAttackLabel.setVisible(isGate);
            gateHoldSlider.setVisible(isGate); gateHoldLabel.setVisible(isGate);
            gateReleaseSlider.setVisible(isGate); gateReleaseLabel.setVisible(isGate);
            gateFloorSlider.setVisible(isGate); gateFloorLabel.setVisible(isGate);

            const bool isLimiter = (currentTab == TabLimiter);
            limiterBypassBtn.setVisible(isLimiter);
            hpfSlopeCombo.setVisible(isLimiter);
            inputGainSlider.setVisible(isLimiter); inputGainLabel.setVisible(isLimiter);
            hpfFreqSlider.setVisible(isLimiter); hpfFreqLabel.setVisible(isLimiter);
            limiterCeilingSlider.setVisible(isLimiter); limiterCeilingLabel.setVisible(isLimiter);
            limiterReleaseSlider.setVisible(isLimiter); limiterReleaseLabel.setVisible(isLimiter);
            outputGainSlider.setVisible(isLimiter); outputGainLabel.setVisible(isLimiter);

            const bool isPresets = (currentTab == TabPresets);
            presetCombo.setVisible(isPresets);
            loadPresetBtn.setVisible(isPresets);
            savePresetBtn.setVisible(isPresets);
            importPresetBtn.setVisible(isPresets);
            presetInfoLabel.setVisible(isPresets);

            repaint();
            return;
        }
    }

    if (button == &loadPresetBtn)
    {
        int pIdx = presetCombo.getSelectedId() - 1;
        if (pIdx >= 0 && pIdx < VocalStudioPresetManager::NumFactoryPresets)
            processor.setCurrentProgram(pIdx);
    }
    else if (button == &savePresetBtn)
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Export Vocal Studio Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.xml");
        const auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                auto xmlStr = processor.exportPresetXml("User Preset");
                file.replaceWithText(xmlStr);
                presetInfoLabel.setText("Exported preset to: " + file.getFileName(), juce::dontSendNotification);
            }
        });
    }
    else if (button == &importPresetBtn)
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Import Vocal Studio Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.xml");
        const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                auto xmlStr = file.loadFileAsString();
                juce::String name;
                if (processor.importPresetXml(xmlStr, name))
                    presetInfoLabel.setText("Loaded Custom Preset: " + name, juce::dontSendNotification);
                else
                    presetInfoLabel.setText("Failed to parse preset XML.", juce::dontSendNotification);
            }
        });
    }
}

void StudioVocalPluginEditor::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &presetCombo)
    {
        int pIdx = presetCombo.getSelectedId() - 1;
        if (pIdx >= 0 && pIdx < VocalStudioPresetManager::NumFactoryPresets)
            presetInfoLabel.setText(VocalStudioPresetManager::getFactoryPreset(pIdx).description, juce::dontSendNotification);
    }
}

void StudioVocalPluginEditor::timerCallback()
{
    // Update meters
    const auto& vocal = processor.getVocalDSP();
    inMeterL.setLevel(vocal.getInputPeakDb());
    inMeterR.setLevel(vocal.getInputPeakDb());
    outMeterL.setLevel(vocal.getOutputPeakDb());
    outMeterR.setLevel(vocal.getOutputPeakDb());

    gateGrMeter.setLevel(vocal.getGateGainReductionDb());
    compGrMeter.setLevel(vocal.getCompGainReductionDb());
    deEsserGrMeter.setLevel(vocal.getDeEsserGainReductionDb());
    limiterGrMeter.setLevel(vocal.getLimiterGainReductionDb());

    // Update telemetry in HOME
    const float prob = processor.getSpeechProbability();
    vadMetricLabel.setText("AI SPEECH DETECT: " + juce::String((int)(prob * 100.0f)) + "% (" +
                           (prob > 0.5f ? "Speech Active" : "Noise/Silence") + ")", juce::dontSendNotification);
    latencyMetricLabel.setText("DSP LATENCY: " + juce::String(processor.getLatencySamples()) + " samples (" +
                              juce::String((float)processor.getLatencySamples() / 48.0f, 1) + " ms)", juce::dontSendNotification);
    sampleRateMetricLabel.setText("SAMPLE RATE: " + juce::String((int)processor.getSampleRate()) + " Hz | ENGINE: " +
                                 (processor.isProcessingAvailable() ? "RNNoise Active" : "Standby/Pass-Through"), juce::dontSendNotification);

    if (currentTab == TabEQ)
        eqCurveView.repaint();
    else if (currentTab == TabComp)
        compCurveView.repaint();
}

// ============================================================================
// 1. Dynamic EQ Response Curve
// ============================================================================
float StudioVocalPluginEditor::EQCurveView::freqToX(float f) const
{
    const float minF = std::log10(20.0f);
    const float maxF = std::log10(20000.0f);
    const float frac = (std::log10(juce::jlimit(20.0f, 20000.0f, f)) - minF) / (maxF - minF);
    return frac * (float)getWidth();
}

float StudioVocalPluginEditor::EQCurveView::xToFreq(float x) const
{
    const float minF = std::log10(20.0f);
    const float maxF = std::log10(20000.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, x / (float)getWidth());
    return std::pow(10.0f, minF + frac * (maxF - minF));
}

float StudioVocalPluginEditor::EQCurveView::gainToY(float gDb) const
{
    const float frac = (18.0f - juce::jlimit(-18.0f, 18.0f, gDb)) / 36.0f;
    return frac * (float)getHeight();
}

float StudioVocalPluginEditor::EQCurveView::yToGain(float y) const
{
    const float frac = 1.0f - (y / (float)getHeight());
    return -18.0f + frac * 36.0f;
}

void StudioVocalPluginEditor::EQCurveView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090d14));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Frequency grid
    g.setColour(juce::Colour(0xff182230));
    const float fGrid[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float f : fGrid)
        g.drawVerticalLine((int)freqToX(f), bounds.getY(), bounds.getBottom());

    // 0 dB reference
    const float zeroY = gainToY(0.0f);
    g.setColour(juce::Colour(0xff2d3b4e));
    g.drawHorizontalLine((int)zeroY, bounds.getX(), bounds.getRight());

    // Curve
    const auto& eq = editor.processor.getVocalDSP().getEQ();
    juce::Path curve;
    const int numPts = getWidth();
    for (int x = 0; x < numPts; ++x)
    {
        float f = xToFreq((float)x);
        float gDb = eq.getMagnitudeForFrequency(f);
        float py = gainToY(gDb);
        if (x == 0) curve.startNewSubPath((float)x, py);
        else curve.lineTo((float)x, py);
    }

    // Under-curve fill
    juce::Path fill = curve;
    fill.lineTo(bounds.getRight(), zeroY);
    fill.lineTo(bounds.getX(), zeroY);
    fill.closeSubPath();
    juce::ColourGradient grad(juce::Colour(0x3300f5d4), 0.0f, zeroY - 20.0f,
                              juce::Colour(0x0500f5d4), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillPath(fill);

    g.setColour(juce::Colour(0xff00f5d4));
    g.strokePath(curve, juce::PathStrokeType(1.8f));

    // Nodes
    const juce::Colour bandCols[] = {
        juce::Colour(0xfff43f5e), juce::Colour(0xfffb923c), juce::Colour(0xfffacc15),
        juce::Colour(0xff4ade80), juce::Colour(0xff2dd4bf), juce::Colour(0xff38bdf8),
        juce::Colour(0xff818cf8), juce::Colour(0xffe879f9)
    };

    for (int b = 0; b < 8; ++b)
    {
        float f = eq.params.bands[b].freq.load(std::memory_order_relaxed);
        float gDb = eq.params.bands[b].gainDb.load(std::memory_order_relaxed);
        bool en = eq.params.bands[b].enabled.load(std::memory_order_relaxed);

        float bx = freqToX(f);
        float by = gainToY(gDb);

        g.setColour(en ? bandCols[b] : juce::Colour(0xff475569));
        g.fillEllipse(bx - 4.5f, by - 4.5f, 9.0f, 9.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(bx - 4.5f, by - 4.5f, 9.0f, 9.0f, 1.0f);
    }
}

void StudioVocalPluginEditor::EQCurveView::mouseDown(const juce::MouseEvent& e)
{
    draggedBand = -1;
    const auto& eq = editor.processor.getVocalDSP().getEQ();
    for (int b = 0; b < 8; ++b)
    {
        float f = eq.params.bands[b].freq.load(std::memory_order_relaxed);
        float gDb = eq.params.bands[b].gainDb.load(std::memory_order_relaxed);
        float bx = freqToX(f);
        float by = gainToY(gDb);
        if (e.position.getDistanceFrom(juce::Point<float>(bx, by)) <= 12.0f)
        {
            draggedBand = b;
            break;
        }
    }
}

void StudioVocalPluginEditor::EQCurveView::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedBand >= 0 && draggedBand < 8)
    {
        float newF = juce::jlimit(20.0f, 20000.0f, xToFreq(e.position.x));
        float newG = juce::jlimit(-18.0f, 18.0f, yToGain(e.position.y));

        const juce::String prefix = "eq_band_" + juce::String(draggedBand + 1) + "_";
        if (auto* p = editor.processor.parameters.getParameter(prefix + "freq"))
            p->setValueNotifyingHost(p->convertTo0to1(newF));
        if (auto* p = editor.processor.parameters.getParameter(prefix + "gain"))
            p->setValueNotifyingHost(p->convertTo0to1(newG));

        repaint();
    }
}

void StudioVocalPluginEditor::EQCurveView::mouseUp(const juce::MouseEvent&)
{
    draggedBand = -1;
}

// ============================================================================
// 2. Dynamics Transfer Curve
// ============================================================================
void StudioVocalPluginEditor::CompCurveView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff090d14));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Reference grid
    g.setColour(juce::Colour(0xff182230));
    for (float db = -60.0f; db <= 0.0f; db += 15.0f)
    {
        float norm = (db + 60.0f) / 60.0f;
        g.drawVerticalLine((int)(bounds.getX() + norm * bounds.getWidth()), bounds.getY(), bounds.getBottom());
        g.drawHorizontalLine((int)(bounds.getBottom() - norm * bounds.getHeight()), bounds.getX(), bounds.getRight());
    }

    // 1:1 Unity Line
    g.setColour(juce::Colour(0xff2d3b4e));
    g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 1.0f);

    // Compression transfer
    const auto& proc = editor.processor.getVocalDSP();
    juce::Path p;
    const int w = getWidth();
    for (int x = 0; x < w; ++x)
    {
        float inDb = -60.0f + ((float)x / (float)w) * 60.0f;
        float outDb = proc.getCompressionTransferDb(inDb);
        outDb = juce::jlimit(-60.0f, 10.0f, outDb);

        float py = bounds.getBottom() - ((outDb + 60.0f) / 60.0f) * bounds.getHeight();
        if (x == 0) p.startNewSubPath((float)x, py);
        else p.lineTo((float)x, py);
    }

    g.setColour(juce::Colour(0xfff43f5e));
    g.strokePath(p, juce::PathStrokeType(2.0f));
}

// ============================================================================
// 3. LED Ladder Meter
// ============================================================================
StudioVocalPluginEditor::LedLadderMeter::LedLadderMeter(const juce::String& lbl, Mode m, juce::Colour col)
    : title(lbl), mode(m), activeColor(col)
{
}

void StudioVocalPluginEditor::LedLadderMeter::setLevel(float valDb)
{
    currentDb = valDb;
    repaint();
}

void StudioVocalPluginEditor::LedLadderMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Top text label
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText(title, 0, 0, getWidth(), 12, juce::Justification::centred);

    auto meterRect = bounds.withTrimmedTop(14.0f).withTrimmedBottom(4.0f).reduced(2.0f);
    g.setColour(juce::Colour(0xff070a10));
    g.fillRoundedRectangle(meterRect, 2.0f);

    const int numSegs = 18;
    const float segH = (meterRect.getHeight() - (numSegs - 1)) / (float)numSegs;

    for (int s = 0; s < numSegs; ++s)
    {
        float y = meterRect.getY() + s * (segH + 1.0f);
        float segFrac = (float)(numSegs - 1 - s) / (float)(numSegs - 1);

        bool isLit = false;
        if (mode == MeterGR)
        {
            float targetRed = -currentDb; // Positive dB reduction
            float segDb = (float)s / (float)(numSegs - 1) * 24.0f;
            isLit = (targetRed >= segDb && targetRed > 0.2f);
        }
        else
        {
            float segDb = -60.0f + segFrac * 60.0f;
            isLit = (currentDb >= segDb);
        }

        juce::Colour segCol = activeColor;
        if (mode == MeterLevel)
        {
            if (segFrac > 0.90f) segCol = juce::Colour(0xffef4444); // Clip/Red
            else if (segFrac > 0.75f) segCol = juce::Colour(0xfff59e0b); // Amber
        }

        g.setColour(isLit ? segCol : juce::Colour(0xff161e2b));
        g.fillRect(meterRect.getX() + 1.0f, y, meterRect.getWidth() - 2.0f, segH);
    }
}

} // namespace LiveBus
