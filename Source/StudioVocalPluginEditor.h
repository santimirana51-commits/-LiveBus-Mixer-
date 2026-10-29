#pragma once
#include <JuceHeader.h>
#include "RNNoiseVST3Plugin.h"
#include <vector>

namespace LiveBus
{

/**
 * Modern Multi-Tab Studio GUI for LiveBus RNNoise Vocal Studio VST.
 * Tabs:
 * 1. HOME: Master I/O Meters, VAD Activity Bar, Quick Status Badges, Module Bypass Toggles
 * 2. NOISE SUPPRESSION: RNNoise Wet/Dry, VAD Gate Threshold, Grace Period
 * 3. VOCAL ENHANCE: Warmth, Body, Clarity, Presence, Air rotary controls
 * 4. PARAMETRIC EQ: Interactive 8-Band Composite EQ response curve with draggable nodes
 * 5. COMPRESSOR: Dynamic transfer curve, Threshold, Ratio, Attack, Release, Knee, Mix, Auto-Release
 * 6. DE-ESSER: Threshold, Frequency, Range, Bell/Shelf toggle, GR ladder meter
 * 7. NOISE GATE: Open Threshold, Attack, Hold, Release, Floor, GR ladder meter
 * 8. LIMITER: Ceiling, Release, GR ladder meter, Output Gain
 * 9. PRESETS: 13 Factory presets dropdown + XML Save/Load buttons
 */
class StudioVocalPluginEditor final : public juce::AudioProcessorEditor,
                                     public juce::Button::Listener,
                                     public juce::ComboBox::Listener,
                                     private juce::Timer
{
public:
    explicit StudioVocalPluginEditor(RNNoiseVST3Plugin& plugin);
    ~StudioVocalPluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;

private:
    void timerCallback() override;

    // Helper to create rotary sliders with consistent dark modern studio styling
    void setupRotary(juce::Slider& slider, juce::Label& label, const juce::String& text,
                     const juce::String& suffix, juce::Colour fillCol);
    void setupToggle(juce::ToggleButton& btn, const juce::String& text);

    RNNoiseVST3Plugin& processor;

    // UI Tab Navigation
    enum TabIndex
    {
        TabHome = 0,
        TabRNNoise,
        TabEnhance,
        TabEQ,
        TabComp,
        TabDeEsser,
        TabGate,
        TabLimiter,
        TabPresets,
        NumTabs
    };
    int currentTab = TabHome;
    std::array<juce::TextButton, NumTabs> tabButtons;

    // --- Header ---
    juce::Label titleLabel;
    juce::Label subTitleLabel;
    juce::ToggleButton masterBypassToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAttachment;

    // --- Visualizer Components ---
    // 1. Dynamic EQ Curve
    class EQCurveView final : public juce::Component
    {
    public:
        explicit EQCurveView(StudioVocalPluginEditor& owner) : editor(owner) {}
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
    private:
        float freqToX(float f) const;
        float xToFreq(float x) const;
        float gainToY(float gDb) const;
        float yToGain(float y) const;
        StudioVocalPluginEditor& editor;
        int draggedBand = -1;
    };
    EQCurveView eqCurveView;

    // 2. Compression Transfer Curve
    class CompCurveView final : public juce::Component
    {
    public:
        explicit CompCurveView(StudioVocalPluginEditor& owner) : editor(owner) {}
        void paint(juce::Graphics& g) override;
    private:
        StudioVocalPluginEditor& editor;
    };
    CompCurveView compCurveView;

    // 3. Vertical LED Ladder Meter
    class LedLadderMeter final : public juce::Component
    {
    public:
        enum Mode { MeterGR, MeterLevel };
        explicit LedLadderMeter(const juce::String& label, Mode m, juce::Colour col);
        void setLevel(float valDb);
        void paint(juce::Graphics& g) override;
    private:
        juce::String title;
        Mode mode;
        juce::Colour activeColor;
        float currentDb{-60.0f};
    };
    LedLadderMeter inMeterL{"IN L", LedLadderMeter::MeterLevel, juce::Colour(0xff38bdf8)};
    LedLadderMeter inMeterR{"IN R", LedLadderMeter::MeterLevel, juce::Colour(0xff38bdf8)};
    LedLadderMeter outMeterL{"OUT L", LedLadderMeter::MeterLevel, juce::Colour(0xff00f5d4)};
    LedLadderMeter outMeterR{"OUT R", LedLadderMeter::MeterLevel, juce::Colour(0xff00f5d4)};
    LedLadderMeter gateGrMeter{"GATE GR", LedLadderMeter::MeterGR, juce::Colour(0xfff59e0b)};
    LedLadderMeter compGrMeter{"COMP GR", LedLadderMeter::MeterGR, juce::Colour(0xfff43f5e)};
    LedLadderMeter deEsserGrMeter{"DE-ESS GR", LedLadderMeter::MeterGR, juce::Colour(0xffe879f9)};
    LedLadderMeter limiterGrMeter{"LIM GR", LedLadderMeter::MeterGR, juce::Colour(0xffef4444)};

    // --- Tab 1: HOME Components ---
    juce::GroupComponent homeStatusGroup{"statusGroup", "ENGINE METRICS & TELEMETRY"};
    juce::Label vadMetricLabel;
    juce::Label latencyMetricLabel;
    juce::Label sampleRateMetricLabel;
    juce::ToggleButton homeRnnoiseToggle{"RNNoise AI Denoise"};
    juce::ToggleButton homeHpfToggle{"High-Pass Filter"};
    juce::ToggleButton homeGateToggle{"Noise Gate"};
    juce::ToggleButton homeEqToggle{"8-Band EQ"};
    juce::ToggleButton homeDeEsserToggle{"De-Esser"};
    juce::ToggleButton homeCompToggle{"Compressor"};
    juce::ToggleButton homeEnhanceToggle{"Vocal Enhancer"};
    juce::ToggleButton homeLimiterToggle{"Brickwall Limiter"};

    // --- Tab 2: NOISE SUPPRESSION Controls ---
    juce::ToggleButton rnnoiseBypassBtn{"Bypass RNNoise"};
    juce::ToggleButton vadToggle{"VAD Intelligent Gate"};
    juce::Slider vadThresholdSlider;
    juce::Label vadThresholdLabel;
    juce::Slider gracePeriodSlider;
    juce::Label gracePeriodLabel;
    juce::Slider wetSlider;
    juce::Label wetLabel;

    // --- Tab 3: VOCAL ENHANCE Controls ---
    juce::ToggleButton enhanceBypassBtn{"Bypass Enhancer"};
    juce::Slider warmthSlider, bodySlider, claritySlider, presenceSlider, airSlider;
    juce::Label warmthLabel, bodyLabel, clarityLabel, presenceLabel, airLabel;

    // --- Tab 4: PARAMETRIC EQ Controls ---
    juce::ToggleButton eqBypassBtn{"Bypass 8-Band EQ"};
    struct BandControls
    {
        juce::ToggleButton enableBtn;
        juce::Slider freqSlider;
        juce::Slider gainSlider;
        juce::Slider qSlider;
        juce::ComboBox typeCombo;
    };
    std::array<BandControls, 8> eqControls;

    // --- Tab 5: COMPRESSOR Controls ---
    juce::ToggleButton compBypassBtn{"Bypass Compressor"};
    juce::Slider compThreshSlider, compRatioSlider, compAttackSlider, compReleaseSlider;
    juce::Slider compMakeupSlider, compMixSlider;
    juce::Label compThreshLabel, compRatioLabel, compAttackLabel, compReleaseLabel;
    juce::Label compMakeupLabel, compMixLabel;
    juce::ComboBox compKneeCombo;
    juce::ToggleButton compAutoReleaseBtn{"Auto Release"};

    // --- Tab 6: DE-ESSER Controls ---
    juce::ToggleButton deEsserBypassBtn{"Bypass De-Esser"};
    juce::Slider deEsserThreshSlider, deEsserFreqSlider, deEsserRangeSlider;
    juce::Label deEsserThreshLabel, deEsserFreqLabel, deEsserRangeLabel;
    juce::ComboBox deEsserTypeCombo;

    // --- Tab 7: NOISE GATE Controls ---
    juce::ToggleButton gateBypassBtn{"Bypass Gate"};
    juce::Slider gateThreshSlider, gateAttackSlider, gateHoldSlider, gateReleaseSlider, gateFloorSlider;
    juce::Label gateThreshLabel, gateAttackLabel, gateHoldLabel, gateReleaseLabel, gateFloorLabel;

    // --- Tab 8: LIMITER & TRIM Controls ---
    juce::ToggleButton limiterBypassBtn{"Bypass Limiter"};
    juce::Slider inputGainSlider, hpfFreqSlider, limiterCeilingSlider, limiterReleaseSlider, outputGainSlider;
    juce::Label inputGainLabel, hpfFreqLabel, limiterCeilingLabel, limiterReleaseLabel, outputGainLabel;
    juce::ComboBox hpfSlopeCombo;

    // --- Tab 9: PRESETS Controls ---
    juce::ComboBox presetCombo;
    juce::TextButton loadPresetBtn{"Load Selected Preset"};
    juce::TextButton savePresetBtn{"Export Preset (XML)"};
    juce::TextButton importPresetBtn{"Import Preset (XML)"};
    juce::Label presetInfoLabel;

    // Attachments for ValueTreeState
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioVocalPluginEditor)
};

} // namespace LiveBus
