#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Modern Studio Look & Feel for Vocal FX Pro
class VocalFXProLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VocalFXProLookAndFeel();
    ~VocalFXProLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

// Interactive Pitch Waveform Visualizer
class VocalFXPitchDisplay : public juce::Component
{
public:
    VocalFXPitchDisplay();
    void setPitchData(float inputPitchNorm, float targetPitchNorm, bool active);
    void paint(juce::Graphics& g) override;
private:
    float currentIn = 0.5f;
    float currentTarget = 0.5f;
    bool isActive = true;
    std::vector<float> inputHistory;
    std::vector<float> targetHistory;
    float phase = 0.0f;
};

// Vertical Segmented LED Level Meter
class VocalFXLedMeter : public juce::Component
{
public:
    VocalFXLedMeter();
    void setLevel(float peakLinear);
    void paint(juce::Graphics& g) override;
private:
    float currentLevel = 0.0f;
};

// Main Modern Beginner-Friendly Vocal FX Pro Editor
class VocalFXProAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit VocalFXProAudioProcessorEditor(VocalFXProAudioProcessor&);
    ~VocalFXProAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void loadPresetByIndex(int index);
    void resetAllToDefault();

    VocalFXProAudioProcessor& processor;
    VocalFXProLookAndFeel customLookAndFeel;

    // Header Components
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::ComboBox presetDropdown;
    juce::TextButton prevPresetBtn{"<"};
    juce::TextButton nextPresetBtn{">"};
    juce::TextButton resetBtn{"Reset"};

    // Volume Section (Left: Input, Right: Output)
    juce::Slider inputGainSlider;
    juce::Label inputGainLabel{"Input Gain"};
    juce::Label inputGainValLabel;
    VocalFXLedMeter inputMeter;

    juce::Slider outputGainSlider;
    juce::Label outputGainLabel{"Output Gain"};
    juce::Label outputGainValLabel;
    VocalFXLedMeter outputMeter;

    // Main Vocal Section (AutoTune)
    juce::ToggleButton autoTuneToggle{"AUTOTUNE"};
    juce::ComboBox keySelector;
    juce::ComboBox scaleSelector;
    juce::Label keyLabel{"Key"};
    juce::Label scaleLabel{"Scale"};

    juce::Slider retuneSpeedSlider;
    juce::Label retuneSpeedLabel{"Retune Speed"};
    juce::Label retuneSpeedValLabel;

    juce::TextButton naturalModeBtn{"Natural"};
    juce::TextButton hardTuneBtn{"Hard Tune"};

    VocalFXPitchDisplay pitchDisplay;
    juce::Label pitchInLegend{"● Input Pitch"};
    juce::Label pitchTargetLegend{"● Target Pitch"};

    // Simple Effects Section (Compressor, Reverb, Delay)
    // 1. Reverb Card
    juce::ToggleButton reverbToggle{"REVERB"};
    juce::Slider reverbKnob;
    juce::Label reverbKnobLabel{"Mix"};
    juce::Label reverbKnobValLabel;

    // 2. Delay Card
    juce::ToggleButton delayToggle{"DELAY"};
    juce::Slider delayKnob;
    juce::Label delayKnobLabel{"Mix"};
    juce::Label delayKnobValLabel;

    // 3. Compressor Card
    juce::ToggleButton compToggle{"COMPRESSOR"};
    juce::Slider compKnob;
    juce::Label compKnobLabel{"Amount"};
    juce::Label compKnobValLabel;

    // Bottom Section
    juce::Slider dryWetSlider;
    juce::Label dryWetLabel{"Dry / Wet Mix"};
    juce::Label dryWetValLabel;

    // Attachments for Parameter Automation Safety
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> inputGainAtt;
    std::unique_ptr<SliderAttachment> outputGainAtt;
    std::unique_ptr<SliderAttachment> retuneSpeedAtt;
    std::unique_ptr<SliderAttachment> reverbMixAtt;
    std::unique_ptr<SliderAttachment> delayMixAtt;
    std::unique_ptr<SliderAttachment> compThresholdAtt;
    std::unique_ptr<SliderAttachment> dryWetAtt;

    std::unique_ptr<ButtonAttachment> autoTuneToggleAtt;
    std::unique_ptr<ButtonAttachment> reverbToggleAtt;
    std::unique_ptr<ButtonAttachment> delayToggleAtt;
    std::unique_ptr<ButtonAttachment> compToggleAtt;

    std::unique_ptr<ComboBoxAttachment> keyAtt;
    std::unique_ptr<ComboBoxAttachment> scaleAtt;

    // Interactive Hover Tooltips
    juce::TooltipWindow tooltipWindow{this, 350};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalFXProAudioProcessorEditor)
};
