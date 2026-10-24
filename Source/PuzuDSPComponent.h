#pragma once
#include <JuceHeader.h>
#include "ChannelStripDSP.h"

class MainComponent; // Forward declaration

class PuzuDSPComponent final : public juce::Component,
                               public juce::Slider::Listener,
                               public juce::Button::Listener,
                               public juce::ComboBox::Listener
{
public:
    explicit PuzuDSPComponent(std::array<BusDSPProcessor, 2>& processors);
    ~PuzuDSPComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;

    void updateUIFromProcessor();
    void setSelectedBus(int busIndex); // 0 = Stream, 1 = Monitor

    // Visual Curve Component
    class PuzuCurveVisualizer final : public juce::Component
    {
    public:
        explicit PuzuCurveVisualizer(PuzuDSPComponent& owner);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        float freqToX(float freq) const;
        float xToFreq(float x) const;
        float gainToY(float gainDb) const;
        float yToGain(float y) const;

    private:
        PuzuDSPComponent& owner;
        int activeDragBand = -1;
        int hoveredBand = -1;
    };

    BusDSPProcessor& getCurrentProcessor() { return busProcessors[(size_t)selectedBus]; }
    const BusDSPProcessor& getCurrentProcessor() const { return busProcessors[(size_t)selectedBus]; }

private:
    std::array<BusDSPProcessor, 2>& busProcessors;
    int selectedBus = 0; // 0 = Stream, 1 = Monitor
    bool isUpdatingUI = false;

    // Header & Logo
    juce::Label brandLabel;
    juce::Label subBrandLabel;
    juce::TextButton peqModeBtn;
    juce::TextButton rstBtn;
    juce::TextButton passBtn;
    juce::TextButton sysSetBtn;

    // Curve visualizer
    std::unique_ptr<PuzuCurveVisualizer> curveVisualizer;

    // Left Filter & Dynamics controls
    juce::Label hpHeader;
    juce::Label hpFreqLabel;
    juce::Slider hpFreqSlider;
    juce::ComboBox hpTypeCombo;
    juce::ComboBox hpSlopeCombo;

    juce::Label lpHeader;
    juce::Label lpFreqLabel;
    juce::Slider lpFreqSlider;
    juce::ComboBox lpTypeCombo;
    juce::ComboBox lpSlopeCombo;

    juce::Label limiterHeader;
    juce::Slider limiterDbuSlider;
    juce::Label limiterAtLabel;
    juce::Slider limiterAtSlider;
    juce::Label limiterReLabel;
    juce::Slider limiterReSlider;

    // 15 EQ Band Channel Strips
    struct BandUI
    {
        juce::Label noLabel;
        juce::Label freqLabel;
        juce::Label qLabel;
        juce::Label gainLabel;
        juce::Slider fader;
    };
    std::array<BandUI, 15> bandUIs;

    // Bottom Channel Output Faders (PUZU Front Left, Front Right, Rear Left, etc.)
    struct OutputChUI
    {
        juce::TextButton tabBtn;
        juce::Slider fader;
        juce::TextButton phaseBtn;
        juce::TextButton muteBtn;
        juce::Label gainLabel;
        juce::Slider delaySlider;
    };
    static constexpr int numOutputs = 6;
    std::array<OutputChUI, numOutputs> outputUIs;

    // Master Volume & Delay Unit
    juce::Label masterVolLabel;
    juce::Slider masterVolSlider;
    juce::ComboBox delayUnitCombo;
    juce::ToggleButton groupFrontToggle;
    juce::ToggleButton groupRearToggle;
    juce::ToggleButton groupAllToggle;
    juce::ToggleButton groupBassToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PuzuDSPComponent)
};
