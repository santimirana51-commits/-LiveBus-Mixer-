#include "AlesisStudioPluginEditor.h"

namespace LiveBus
{

// ============================================================================
// LedLadderMeter Implementation
// ============================================================================
AlesisStudioPluginEditor::LedLadderMeter::LedLadderMeter(const juce::String& label, Mode m, juce::Colour col)
    : title(label), mode(m), activeColor(col)
{
}

void AlesisStudioPluginEditor::LedLadderMeter::setLevel(float valDb)
{
    currentDb = valDb;
    repaint();
}

void AlesisStudioPluginEditor::LedLadderMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Meter track background
    g.setColour(juce::Colour(0xff0d131f));
    g.fillRoundedRectangle(b, 3.0f);

    const int numLeds = 14;
    const float ledGap = 2.0f;
    const float totalGap = (numLeds - 1) * ledGap;
    const float ledHeight = (b.getHeight() - 18.0f - totalGap) / (float)numLeds;

    const float minDb = (mode == MeterGR) ? 0.0f : -48.0f;
    const float maxDb = (mode == MeterGR) ? 24.0f : 6.0f;

    for (int i = 0; i < numLeds; ++i)
    {
        const float ledFrac = (float)i / (float)(numLeds - 1);
        const float segDb = minDb + ledFrac * (maxDb - minDb);
        const float yPos = b.getHeight() - 18.0f - (i + 1) * ledHeight - i * ledGap;
        const auto ledRect = juce::Rectangle<float>(b.getX() + 1.0f, yPos, b.getWidth() - 2.0f, ledHeight);

        bool isLit = false;
        if (mode == MeterGR)
            isLit = (currentDb >= segDb && currentDb > 0.1f);
        else
            isLit = (currentDb >= segDb);

        if (isLit)
            g.setColour(activeColor);
        else
            g.setColour(juce::Colour(0xff161e2e));

        g.fillRoundedRectangle(ledRect, 1.5f);
    }

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
    g.drawText(title, 0, getHeight() - 16, getWidth(), 14, juce::Justification::centred);
}

// ============================================================================
// AlesisStudioPluginEditor
// ============================================================================
AlesisStudioPluginEditor::AlesisStudioPluginEditor(AlesisStudioVST3Plugin& plugin)
    : AudioProcessorEditor(plugin), processor(plugin)
{
    setOpaque(true);

    // --- Header ---
    titleLabel.setText("ALESIS VINTAGE VOCAL & STUDIO RACK", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff06b6d4));
    addAndMakeVisible(titleLabel);

    subTitleLabel.setText("Reverb, Delay, 7-Band Parametric EQ, 3630 Comp, Vocal FX & AutoTune", juce::dontSendNotification);
    subTitleLabel.setFont(juce::FontOptions(11.0f));
    subTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
    addAndMakeVisible(subTitleLabel);

    // Preset Selector Dropdown
    presetDropdown.addItemList({
        "1. DANGDUT KOPLO LIVE",
        "2. POP VOCAL PRO",
        "3. STUDIO CLEAN",
        "4. HARD TUNE",
        "5. VOCAL MEGA FX"
    }, 1);
    presetDropdown.setSelectedId(1, juce::dontSendNotification);
    presetDropdown.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111827));
    presetDropdown.setColour(juce::ComboBox::textColourId, juce::Colour(0xff38bdf8));
    presetDropdown.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff0284c7));
    presetDropdown.setTooltip("Preset Vokal Alesis: Pilih konfigurasi siap pakai (Dangdut Koplo, Pop Vocal, Studio Clean, dll).");
    presetDropdown.onChange = [this]() {
        int idx = presetDropdown.getSelectedId() - 1;
        if (idx >= 0 && idx < 5)
            processor.setCurrentProgram(idx);
    };
    addAndMakeVisible(presetDropdown);

    setupToggle(masterBypassToggle, "MASTER BYPASS");
    masterBypassToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffef4444));
    masterBypassToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff87171));
    masterBypassToggle.setTooltip("Master Bypass: Lewati seluruh prosesor efek Alesis (sinyal audio murni tanpa efek).");
    addAndMakeVisible(masterBypassToggle);
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "master_bypass", masterBypassToggle));

    // =========================================================================
    // TAB 1: COMPRESSOR & VOCAL DYNAMICS
    // =========================================================================
    setupToggle(compBypassBtn, "Bypass 3630");
    compBypassBtn.setTooltip("Bypass Compressor: Matikan kompresor Alesis 3630.");
    setupToggle(compHardKneeToggle, "Hard Knee Mode");
    compHardKneeToggle.setTooltip("Knee Mode: Hard Knee (kompresi tegas seketika) atau Soft Knee (kompresi lembut bertahap).");
    setupToggle(compAutoGainToggle, "Auto Gain");
    compAutoGainToggle.setTooltip("Auto Makeup Gain: Otomatis menyeimbangkan level output terhadap kompresi.");
    setupRotary(compThreshSlider, compThreshLabel, "Threshold", " dB", juce::Colour(0xff06b6d4));
    compThreshSlider.setTooltip("3630 Threshold: Batas volume audio di mana kompresor mulai bekerja.");
    setupRotary(compRatioSlider, compRatioLabel, "Ratio", ":1", juce::Colour(0xff06b6d4));
    compRatioSlider.setTooltip("3630 Ratio: Perbandingan kompresi (misal 3:1 atau 4:1 untuk vokal padat).");
    setupRotary(compAttackSlider, compAttackLabel, "Attack", " ms", juce::Colour(0xff38bdf8));
    compAttackSlider.setTooltip("3630 Attack (ms): Kecepatan meredam lonjakan suara awal.");
    setupRotary(compReleaseSlider, compReleaseLabel, "Release", " ms", juce::Colour(0xff38bdf8));
    compReleaseSlider.setTooltip("3630 Release (ms): Kecepatan kembali ke level normal setelah kompresi.");
    setupRotary(compMakeupSlider, compMakeupLabel, "Output Gain", " dB", juce::Colour(0xff10b981));
    compMakeupSlider.setTooltip("Output Makeup Gain (dB): Penguatan output pasca-kompresi.");
    setupRotary(compMixSlider, compMixLabel, "Parallel Mix", "%", juce::Colour(0xff00f5d4));
    compMixSlider.setTooltip("Parallel Mix (%): Campuran antara suara asli (dry) dan suara terkompresi (wet).");

    compTab.addAndMakeVisible(compGroup);
    compTab.addAndMakeVisible(compBypassBtn);
    compTab.addAndMakeVisible(compHardKneeToggle);
    compTab.addAndMakeVisible(compAutoGainToggle);
    compTab.addAndMakeVisible(compThreshSlider); compTab.addAndMakeVisible(compThreshLabel);
    compTab.addAndMakeVisible(compRatioSlider); compTab.addAndMakeVisible(compRatioLabel);
    compTab.addAndMakeVisible(compAttackSlider); compTab.addAndMakeVisible(compAttackLabel);
    compTab.addAndMakeVisible(compReleaseSlider); compTab.addAndMakeVisible(compReleaseLabel);
    compTab.addAndMakeVisible(compMakeupSlider); compTab.addAndMakeVisible(compMakeupLabel);
    compTab.addAndMakeVisible(compMixSlider); compTab.addAndMakeVisible(compMixLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "comp_bypass", compBypassBtn));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "comp_hardknee", compHardKneeToggle));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "comp_autogain", compAutoGainToggle));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_threshold", compThreshSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_ratio", compRatioSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_attack", compAttackSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_release", compReleaseSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_makeup", compMakeupSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "comp_mix", compMixSlider));

    // Vocal Enhancement Group
    setupToggle(deessBypassBtn, "Bypass De-Esser");
    deessBypassBtn.setTooltip("Bypass De-Esser: Matikan peredam suara desis 's' dan 'c'.");
    setupRotary(deessSlider, deessLabel, "De-Esser", "%", juce::Colour(0xffec4899));
    deessSlider.setTooltip("De-Esser Amount: Menjinakkan frekuensi tajam dan menusuk telinga pada vokal.");
    setupToggle(gateBypassBtn, "Bypass Gate");
    gateBypassBtn.setTooltip("Bypass Noise Gate: Matikan gerbang peredam desis.");
    setupRotary(gateSlider, gateLabel, "Gate Thresh", " dB", juce::Colour(0xfff59e0b));
    gateSlider.setTooltip("Gate Threshold (dB): Redam suara latar ruangan saat tidak ada yang bernyanyi.");
    setupRotary(satSlider, satLabel, "Saturation", "%", juce::Colour(0xffef4444));
    satSlider.setTooltip("Tube Saturation (%): Tambahkan kehangatan analog dan karakter tebal khas tabung.");
    setupRotary(exciterSlider, exciterLabel, "Exciter", "%", juce::Colour(0xffeab308));
    exciterSlider.setTooltip("Harmonic Exciter (%): Kilau frekuensi tinggi agar vokal terdengar lebih jernih dan hadir di depan.");
    setupRotary(widthSlider, widthLabel, "Stereo Width", "%", juce::Colour(0xff8b5cf6));
    widthSlider.setTooltip("Stereo Widener (%): Lebarkan dimensi panggung vokal dan efek modulasi.");

    compTab.addAndMakeVisible(dynGroup);
    compTab.addAndMakeVisible(deessBypassBtn); compTab.addAndMakeVisible(deessSlider); compTab.addAndMakeVisible(deessLabel);
    compTab.addAndMakeVisible(gateBypassBtn); compTab.addAndMakeVisible(gateSlider); compTab.addAndMakeVisible(gateLabel);
    compTab.addAndMakeVisible(satSlider); compTab.addAndMakeVisible(satLabel);
    compTab.addAndMakeVisible(exciterSlider); compTab.addAndMakeVisible(exciterLabel);
    compTab.addAndMakeVisible(widthSlider); compTab.addAndMakeVisible(widthLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "deess_bypass", deessBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "deess_amount", deessSlider));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "gate_bypass", gateBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gate_thresh", gateSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "sat_amount", satSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "exciter_amount", exciterSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "stereo_width", widthSlider));

    // =========================================================================
    // TAB 2: REVERB & DELAY
    // =========================================================================
    setupToggle(verbBypassBtn, "Bypass Reverb");
    verbBypassBtn.setTooltip("Bypass Reverb: Matikan efek simulasi ruangan reverb Alesis.");
    verbTypeCombo.addItemList({"Room", "Hall", "Plate", "Spring", "Studio", "Cathedral"}, 1);
    verbTypeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    verbTypeCombo.setTooltip("Algoritma Reverb: Room, Hall konser, Plate vintage, Spring gitar, Studio, atau Katedral megah.");
    setupRotary(verbDecaySlider, verbDecayLabel, "Decay", "%", juce::Colour(0xffa855f7));
    verbDecaySlider.setTooltip("Reverb Decay (%): Panjang ekor gema yang memudar.");
    setupRotary(verbSizeSlider, verbSizeLabel, "Size", "%", juce::Colour(0xffa855f7));
    verbSizeSlider.setTooltip("Reverb Room Size (%): Ukuran dimensi ruangan virtual.");
    setupRotary(verbDampSlider, verbDampLabel, "Damping", "%", juce::Colour(0xff818cf8));
    verbDampSlider.setTooltip("Reverb Damping (%): Redaman pantulan nada tinggi di dinding ruangan.");
    setupRotary(verbPreDelaySlider, verbPreDelayLabel, "Pre-Delay", " ms", juce::Colour(0xffc084fc));
    verbPreDelaySlider.setTooltip("Reverb Pre-Delay (ms): Jeda sebelum gema pertama muncul (menjaga artikulasi kata tetap jelas).");
    setupRotary(verbWetSlider, verbWetLabel, "Wet Mix", "%", juce::Colour(0xffe879f9));
    verbWetSlider.setTooltip("Reverb Wet Level (%): Volume efek gema ruangan.");
    setupRotary(verbLowCutSlider, verbLowCutLabel, "Low Cut", " Hz", juce::Colour(0xff94a3b8));
    verbLowCutSlider.setTooltip("Reverb Low Cut (Hz): Potong nada bass pada gema agar suara tidak berlumpur.");
    setupRotary(verbHighCutSlider, verbHighCutLabel, "High Cut", " Hz", juce::Colour(0xff94a3b8));
    verbHighCutSlider.setTooltip("Reverb High Cut (Hz): Jinakkan nada treble berlebihan pada gema.");

    spaceTab.addAndMakeVisible(verbGroup);
    spaceTab.addAndMakeVisible(verbBypassBtn);
    spaceTab.addAndMakeVisible(verbTypeCombo);
    spaceTab.addAndMakeVisible(verbDecaySlider); spaceTab.addAndMakeVisible(verbDecayLabel);
    spaceTab.addAndMakeVisible(verbSizeSlider); spaceTab.addAndMakeVisible(verbSizeLabel);
    spaceTab.addAndMakeVisible(verbDampSlider); spaceTab.addAndMakeVisible(verbDampLabel);
    spaceTab.addAndMakeVisible(verbPreDelaySlider); spaceTab.addAndMakeVisible(verbPreDelayLabel);
    spaceTab.addAndMakeVisible(verbWetSlider); spaceTab.addAndMakeVisible(verbWetLabel);
    spaceTab.addAndMakeVisible(verbLowCutSlider); spaceTab.addAndMakeVisible(verbLowCutLabel);
    spaceTab.addAndMakeVisible(verbHighCutSlider); spaceTab.addAndMakeVisible(verbHighCutLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "verb_bypass", verbBypassBtn));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "verb_type", verbTypeCombo));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_decay", verbDecaySlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_size", verbSizeSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_damping", verbDampSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_predelay", verbPreDelaySlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_wet", verbWetSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_lowcut", verbLowCutSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "verb_highcut", verbHighCutSlider));

    // Delay Module
    setupToggle(delayBypassBtn, "Bypass Delay");
    delayBypassBtn.setTooltip("Bypass Delay: Matikan efek pantulan echo.");
    delayModeCombo.addItemList({"Mono", "Stereo", "Ping-Pong", "Slapback"}, 1);
    delayModeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    delayModeCombo.setTooltip("Mode Delay: Mono, Stereo L/R, Ping-Pong bolak-balik, atau Slapback cepat khas dangdut/rockabilly.");
    setupRotary(delayTimeSlider, delayTimeLabel, "Time", " ms", juce::Colour(0xff38bdf8));
    delayTimeSlider.setTooltip("Delay Time (ms): Tempo jeda pantulan suara.");
    setupRotary(delayFeedbackSlider, delayFeedbackLabel, "Feedback", "%", juce::Colour(0xff06b6d4));
    delayFeedbackSlider.setTooltip("Delay Feedback (%): Banyaknya pengulangan pantulan sebelum habis.");
    setupRotary(delayWetSlider, delayWetLabel, "Wet Mix", "%", juce::Colour(0xff38bdf8));
    delayWetSlider.setTooltip("Delay Wet Level (%): Volume pantulan suara.");
    setupRotary(delayFilterSlider, delayFilterLabel, "Tone Filter", " Hz", juce::Colour(0xff64748b));
    delayFilterSlider.setTooltip("Delay Tone Filter (Hz): Mengatur warna nada pantulan (hangat/redup atau cerah).");

    spaceTab.addAndMakeVisible(delayGroup);
    spaceTab.addAndMakeVisible(delayBypassBtn);
    spaceTab.addAndMakeVisible(delayModeCombo);
    spaceTab.addAndMakeVisible(delayTimeSlider); spaceTab.addAndMakeVisible(delayTimeLabel);
    spaceTab.addAndMakeVisible(delayFeedbackSlider); spaceTab.addAndMakeVisible(delayFeedbackLabel);
    spaceTab.addAndMakeVisible(delayWetSlider); spaceTab.addAndMakeVisible(delayWetLabel);
    spaceTab.addAndMakeVisible(delayFilterSlider); spaceTab.addAndMakeVisible(delayFilterLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "delay_bypass", delayBypassBtn));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "delay_mode", delayModeCombo));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "delay_time", delayTimeSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "delay_feedback", delayFeedbackSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "delay_wet", delayWetSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "delay_filter", delayFilterSlider));

    // =========================================================================
    // TAB 3: PARAMETRIC EQ
    // =========================================================================
    setupToggle(eqBypassBtn, "Bypass EQ");
    eqBypassBtn.setTooltip("Bypass EQ: Matikan seluruh filter equalizer 7-band.");
    setupRotary(eqHpSlider, eqHpLabel, "High-Pass", " Hz", juce::Colour(0xfff43f5e));
    eqHpSlider.setTooltip("High-Pass Filter (Hz): Potong nada bass gemuruh yang tidak diinginkan.");
    setupRotary(eqLowShelfSlider, eqLowShelfLabel, "Low Shelf", " dB", juce::Colour(0xfffb923c));
    eqLowShelfSlider.setTooltip("Low Shelf Gain (dB): Penguatan atau peredaman bass tebal.");
    setupRotary(eqLowMidSlider, eqLowMidLabel, "Low Mid", " dB", juce::Colour(0xfffacc15));
    eqLowMidSlider.setTooltip("Low Mid Gain (dB): Mengatur kejelasan vokal agar tidak sengau/berdengung.");
    setupRotary(eqMidSlider, eqMidLabel, "Mid", " dB", juce::Colour(0xff4ade80));
    eqMidSlider.setTooltip("Mid Gain (dB): Inti tubuh vokal (body vokal).");
    setupRotary(eqHighMidSlider, eqHighMidLabel, "High Mid", " dB", juce::Colour(0xff38bdf8));
    eqHighMidSlider.setTooltip("High Mid Gain (dB): Kejelasan lafal kata (artikulasi konsonan).");
    setupRotary(eqHighShelfSlider, eqHighShelfLabel, "High Shelf", " dB", juce::Colour(0xff818cf8));
    eqHighShelfSlider.setTooltip("High Shelf Gain (dB): Kilau udara (air band) nada tinggi.");
    setupRotary(eqLpSlider, eqLpLabel, "Low-Pass", " Hz", juce::Colour(0xffc084fc));
    eqLpSlider.setTooltip("Low-Pass Filter (Hz): Potong desis frekuensi ultra-tinggi.");

    // Add FFT Spectrum Analyzer to EQ Tab
    eqTab.addAndMakeVisible(spectrumAnalyzer);

    eqTab.addAndMakeVisible(eqGroup);
    eqTab.addAndMakeVisible(eqBypassBtn);
    eqTab.addAndMakeVisible(eqHpSlider); eqTab.addAndMakeVisible(eqHpLabel);
    eqTab.addAndMakeVisible(eqLowShelfSlider); eqTab.addAndMakeVisible(eqLowShelfLabel);
    eqTab.addAndMakeVisible(eqLowMidSlider); eqTab.addAndMakeVisible(eqLowMidLabel);
    eqTab.addAndMakeVisible(eqMidSlider); eqTab.addAndMakeVisible(eqMidLabel);
    eqTab.addAndMakeVisible(eqHighMidSlider); eqTab.addAndMakeVisible(eqHighMidLabel);
    eqTab.addAndMakeVisible(eqHighShelfSlider); eqTab.addAndMakeVisible(eqHighShelfLabel);
    eqTab.addAndMakeVisible(eqLpSlider); eqTab.addAndMakeVisible(eqLpLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "eq_bypass", eqBypassBtn));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_hp", eqHpSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_lowshelf", eqLowShelfSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_lowmid", eqLowMidSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_mid", eqMidSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_highmid", eqHighMidSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_highshelf", eqHighShelfSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "eq_lp", eqLpSlider));

    // =========================================================================
    // TAB 4: AUTOTUNE & CREATIVE FX
    // =========================================================================
    setupToggle(atBypassBtn, "Bypass AutoTune");
    atBypassBtn.setTooltip("Bypass AutoTune: Matikan koreksi nada otomatis.");
    atKeyCombo.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 1);
    atScaleCombo.addItemList({"Major", "Minor", "Chromatic", "Harmonic Minor", "Melodic Minor"}, 1);
    atKeyCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    atScaleCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    atKeyCombo.setTooltip("Root Key: Nada dasar lagu yang sedang dinyanyikan (C, D, G, A, dll).");
    atScaleCombo.setTooltip("Musical Scale: Tangga nada lagu (Major ceria, Minor syahdu, Chromatic bebas, dsb).");

    atKeyLabel.setText("Root Key", juce::dontSendNotification);
    atKeyLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    atKeyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    atScaleLabel.setText("Musical Scale", juce::dontSendNotification);
    atScaleLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    atScaleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));

    setupRotary(atAmountSlider, atAmountLabel, "Correction Amount", "%", juce::Colour(0xff06b6d4));
    atAmountSlider.setTooltip("AutoTune Correction Amount (%): Kekuatan koreksi nada (0% natural s/d 100% full robot tune).");
    setupRotary(atSpeedSlider, atSpeedLabel, "Retune Speed", " ms", juce::Colour(0xff38bdf8));
    atSpeedSlider.setTooltip("Retune Speed (ms): Kecepatan menarik nada fals (0-5 ms Hard Tune dangdut/trap, 20-50 ms natural pop).");

    pitchTrackLabel.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
    pitchTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    pitchTrackLabel.setJustificationType(juce::Justification::centred);

    tuneTab.addAndMakeVisible(atGroup);
    tuneTab.addAndMakeVisible(atBypassBtn);
    tuneTab.addAndMakeVisible(atKeyLabel); tuneTab.addAndMakeVisible(atKeyCombo);
    tuneTab.addAndMakeVisible(atScaleLabel); tuneTab.addAndMakeVisible(atScaleCombo);
    tuneTab.addAndMakeVisible(atAmountSlider); tuneTab.addAndMakeVisible(atAmountLabel);
    tuneTab.addAndMakeVisible(atSpeedSlider); tuneTab.addAndMakeVisible(atSpeedLabel);
    tuneTab.addAndMakeVisible(pitchTrackLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "at_bypass", atBypassBtn));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "at_key", atKeyCombo));
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "at_scale", atScaleCombo));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "at_amount", atAmountSlider));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "at_speed", atSpeedSlider));

    // Creative FX Group
    creativeModeCombo.addItemList({"Off", "Chorus", "Flanger", "Phaser", "Doubler", "Telephone", "Radio", "Robot Voice"}, 1);
    creativeModeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161e2e));
    creativeModeCombo.setTooltip("Mode Creative FX: Pilihan efek modulasi suara unik (Chorus, Flanger, Doubler paduan suara, Telepon, Robot).");
    setupRotary(creativeMixSlider, creativeMixLabel, "Creative Mix", "%", juce::Colour(0xffe879f9));
    creativeMixSlider.setTooltip("Creative Mix (%): Intensitas percampuran efek modulasi kreatif.");

    tuneTab.addAndMakeVisible(creativeGroup);
    tuneTab.addAndMakeVisible(creativeModeCombo);
    tuneTab.addAndMakeVisible(creativeMixSlider); tuneTab.addAndMakeVisible(creativeMixLabel);

    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "creative_mode", creativeModeCombo));
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "creative_mix", creativeMixSlider));

    // =========================================================================
    // ADD TABS TO TABBED COMPONENT
    // =========================================================================
    tabComponent.setTabBarDepth(32);
    tabComponent.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff090d16));
    tabComponent.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xff94a3b8));
    tabComponent.addTab("COMPRESSOR & DYNAMICS", juce::Colour(0xff0b0f17), &compTab, false);
    tabComponent.addTab("REVERB & DELAY", juce::Colour(0xff0b0f17), &spaceTab, false);
    tabComponent.addTab("PARAMETRIC EQ", juce::Colour(0xff0b0f17), &eqTab, false);
    tabComponent.addTab("AUTOTUNE & CREATIVE FX", juce::Colour(0xff0b0f17), &tuneTab, false);
    addAndMakeVisible(tabComponent);

    // Sidebar Meters (Input, 3630 Gain Reduction, Output)
    addAndMakeVisible(inMeter);
    addAndMakeVisible(grMeter);
    addAndMakeVisible(outMeter);

    setSize(920, 500);
    startTimerHz(30);
}

AlesisStudioPluginEditor::~AlesisStudioPluginEditor()
{
    stopTimer();
}

void AlesisStudioPluginEditor::setupRotary(juce::Slider& s, juce::Label& l, const juce::String& text,
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

void AlesisStudioPluginEditor::setupToggle(juce::ToggleButton& btn, const juce::String& text)
{
    btn.setButtonText(text);
    btn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe2e8f0));
    btn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff06b6d4));
}

void AlesisStudioPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff090d16)); // Dark studio carbon background

    auto bounds = getLocalBounds().toFloat();

    // Top Header Banner
    auto header = bounds.removeFromTop(56.0f);
    g.setColour(juce::Colour(0xff111827));
    g.fillRect(header);
    g.setColour(juce::Colour(0xff1f293d));
    g.drawHorizontalLine(56, 0.0f, bounds.getWidth());

    // Right Meter Track Frame
    auto meterRect = juce::Rectangle<float>((float)getWidth() - 54.0f, 66.0f, 40.0f, (float)getHeight() - 76.0f);
    g.setColour(juce::Colour(0xff0e1422));
    g.fillRoundedRectangle(meterRect, 6.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(meterRect, 6.0f, 1.0f);
}

void AlesisStudioPluginEditor::resized()
{
    // Header
    titleLabel.setBounds(18, 6, 380, 24);
    subTitleLabel.setBounds(18, 28, 400, 16);

    presetDropdown.setBounds(430, 14, 250, 28);
    masterBypassToggle.setBounds(getWidth() - 150, 15, 130, 26);

    // Tab Component
    const int tabW = getWidth() - 72;
    const int tabH = getHeight() - 66;
    tabComponent.setBounds(12, 60, tabW, tabH);

    // Meter
    grMeter.setBounds(getWidth() - 44, 78, 20, tabH - 40);

    // ================= Layout TAB 1: Compressor & Dynamics =================
    const int pW = (tabW - 36) / 2;
    const int pH = tabH - 48;
    compGroup.setBounds(8, 6, pW, pH);
    compBypassBtn.setBounds(16, 28, 120, 24);
    compHardKneeToggle.setBounds(140, 28, 140, 24);
    compAutoGainToggle.setBounds(284, 28, 110, 24);

    const int kS = 74;
    compThreshSlider.setBounds(18, 62, kS, kS); compThreshLabel.setBounds(18, 136, kS, 16);
    compRatioSlider.setBounds(104, 62, kS, kS); compRatioLabel.setBounds(104, 136, kS, 16);
    compAttackSlider.setBounds(190, 62, kS, kS); compAttackLabel.setBounds(190, 136, kS, 16);
    compReleaseSlider.setBounds(276, 62, kS, kS); compReleaseLabel.setBounds(276, 136, kS, 16);

    compMakeupSlider.setBounds(104, 164, kS, kS); compMakeupLabel.setBounds(104, 238, kS, 16);
    compMixSlider.setBounds(190, 164, kS, kS); compMixLabel.setBounds(190, 238, kS, 16);

    dynGroup.setBounds(16 + pW, 6, pW, pH);
    deessBypassBtn.setBounds(26 + pW, 28, 120, 24);
    gateBypassBtn.setBounds(150 + pW, 28, 120, 24);

    deessSlider.setBounds(26 + pW, 62, kS, kS); deessLabel.setBounds(26 + pW, 136, kS, 16);
    gateSlider.setBounds(112 + pW, 62, kS, kS); gateLabel.setBounds(112 + pW, 136, kS, 16);
    satSlider.setBounds(198 + pW, 62, kS, kS); satLabel.setBounds(198 + pW, 136, kS, 16);
    exciterSlider.setBounds(284 + pW, 62, kS, kS); exciterLabel.setBounds(284 + pW, 136, kS, 16);
    widthSlider.setBounds(155 + pW, 164, kS, kS); widthLabel.setBounds(155 + pW, 238, kS, 16);

    // ================= Layout TAB 2: Reverb & Delay =================
    verbGroup.setBounds(8, 6, pW, pH);
    verbBypassBtn.setBounds(16, 28, 110, 24);
    verbTypeCombo.setBounds(130, 28, 130, 24);

    verbDecaySlider.setBounds(18, 62, kS, kS); verbDecayLabel.setBounds(18, 136, kS, 16);
    verbSizeSlider.setBounds(104, 62, kS, kS); verbSizeLabel.setBounds(104, 136, kS, 16);
    verbDampSlider.setBounds(190, 62, kS, kS); verbDampLabel.setBounds(190, 136, kS, 16);
    verbPreDelaySlider.setBounds(276, 62, kS, kS); verbPreDelayLabel.setBounds(276, 136, kS, 16);

    verbWetSlider.setBounds(60, 164, kS, kS); verbWetLabel.setBounds(60, 238, kS, 16);
    verbLowCutSlider.setBounds(146, 164, kS, kS); verbLowCutLabel.setBounds(146, 238, kS, 16);
    verbHighCutSlider.setBounds(232, 164, kS, kS); verbHighCutLabel.setBounds(232, 238, kS, 16);

    delayGroup.setBounds(16 + pW, 6, pW, pH);
    delayBypassBtn.setBounds(26 + pW, 28, 110, 24);
    delayModeCombo.setBounds(140 + pW, 28, 130, 24);

    delayTimeSlider.setBounds(26 + pW, 62, kS, kS); delayTimeLabel.setBounds(26 + pW, 136, kS, 16);
    delayFeedbackSlider.setBounds(112 + pW, 62, kS, kS); delayFeedbackLabel.setBounds(112 + pW, 136, kS, 16);
    delayWetSlider.setBounds(198 + pW, 62, kS, kS); delayWetLabel.setBounds(198 + pW, 136, kS, 16);
    delayFilterSlider.setBounds(284 + pW, 62, kS, kS); delayFilterLabel.setBounds(284 + pW, 136, kS, 16);

    // ================= Layout TAB 3: Parametric EQ =================
    eqGroup.setBounds(8, 6, tabW - 16, pH);
    eqBypassBtn.setBounds(18, 28, 120, 24);

    // Spectrum Analyzer Screen (occupies upper half of EQ Tab)
    spectrumAnalyzer.setBounds(18, 56, tabW - 36, 140);

    const int eqKnobW = (tabW - 40) / 7;
    const int eqKnobY = 210;
    eqHpSlider.setBounds(16 + 0 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqHpLabel.setBounds(16 + 0 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);
    eqLowShelfSlider.setBounds(16 + 1 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqLowShelfLabel.setBounds(16 + 1 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);
    eqLowMidSlider.setBounds(16 + 2 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqLowMidLabel.setBounds(16 + 2 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);
    eqMidSlider.setBounds(16 + 3 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqMidLabel.setBounds(16 + 3 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);
    eqHighMidSlider.setBounds(16 + 4 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqHighMidLabel.setBounds(16 + 4 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);
    eqHighShelfSlider.setBounds(16 + 5 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqHighShelfLabel.setBounds(16 + 5 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);
    eqLpSlider.setBounds(16 + 6 * eqKnobW, eqKnobY, eqKnobW - 8, eqKnobW - 8);
    eqLpLabel.setBounds(16 + 6 * eqKnobW, eqKnobY + eqKnobW - 6, eqKnobW - 8, 16);

    // ================= Layout TAB 4: AutoTune & Creative =================
    atGroup.setBounds(8, 6, pW, pH);
    atBypassBtn.setBounds(16, 28, 120, 24);

    atKeyLabel.setBounds(140, 24, 60, 16);
    atKeyCombo.setBounds(140, 42, 70, 26);
    atScaleLabel.setBounds(220, 24, 100, 16);
    atScaleCombo.setBounds(220, 42, 130, 26);

    atAmountSlider.setBounds(40, 86, kS + 12, kS + 12); atAmountLabel.setBounds(40, 172, kS + 12, 16);
    atSpeedSlider.setBounds(160, 86, kS + 12, kS + 12); atSpeedLabel.setBounds(160, 172, kS + 12, 16);
    pitchTrackLabel.setBounds(20, 210, pW - 40, 22);

    creativeGroup.setBounds(16 + pW, 6, pW, pH);
    creativeModeCombo.setBounds(30 + pW, 42, 160, 28);
    creativeMixSlider.setBounds(50 + pW, 90, kS + 12, kS + 12);
    creativeMixLabel.setBounds(50 + pW, 176, kS + 12, 16);

    // Sidebar Meters (IN, GR, OUT)
    const int meterY = 70;
    const int meterH = tabH - 26;
    inMeter.setBounds(getWidth() - 66, meterY, 18, meterH);
    grMeter.setBounds(getWidth() - 44, meterY, 18, meterH);
    outMeter.setBounds(getWidth() - 22, meterY, 18, meterH);
}

void AlesisStudioPluginEditor::timerCallback()
{
    // Update real-time LED meters
    float inDb = juce::Decibels::gainToDecibels(processor.getAlesisDSP().getInputPeak(), -60.0f);
    float outDb = juce::Decibels::gainToDecibels(processor.getAlesisDSP().getOutputPeak(), -60.0f);
    inMeter.setLevel(inDb);
    grMeter.setLevel(processor.getAlesisDSP().getGainReductionDb());
    outMeter.setLevel(outDb);

    // Update real-time FFT spectrum on EQ tab
    std::array<float, 1024> timeData{};
    processor.getAlesisDSP().copySpectrumSamples(timeData.data(), (int)timeData.size());
    spectrumAnalyzer.updateSpectrum(timeData.data(), (int)timeData.size());

    // Update real-time pitch telemetry
    float detHz = processor.getAlesisDSP().getDetectedPitch();
    float tgtHz = processor.getAlesisDSP().getTargetPitch();
    if (detHz > 40.0f)
        pitchTrackLabel.setText("Tracking: " + juce::String((int)detHz) + " Hz  ->  Target: " + juce::String((int)tgtHz) + " Hz", juce::dontSendNotification);
    else
        pitchTrackLabel.setText("Tracking: Voice Idle / Unvoiced", juce::dontSendNotification);
}

} // namespace LiveBus
