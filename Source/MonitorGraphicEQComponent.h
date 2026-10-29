#pragma once

#include <JuceHeader.h>
#include "ChannelStripDSP.h"
#include "PresetManager.h"

class MonitorGraphicEQComponent final : public juce::Component,
                                        private juce::Timer,
                                        private juce::Slider::Listener,
                                        private juce::Button::Listener,
                                        private juce::ComboBox::Listener
{
public:
    using ABState = std::array<MonitorGraphicEQ31State, 2>;

    MonitorGraphicEQComponent(BusDSPProcessor& processor,
                              LiveBus::PresetManager& presetManager);
    ~MonitorGraphicEQComponent() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void updateUIFromProcessor();
    ABState getABStates();
    int getActiveABSlot() const noexcept { return activeABSlot; }
    void setABStates(const ABState& states, int activeSlot);

private:
    class ResponseDisplay final : public juce::Component
    {
    public:
        explicit ResponseDisplay(MonitorGraphicEQComponent& component)
            : owner(component) {}
        void paint(juce::Graphics& graphics) override;

    private:
        MonitorGraphicEQComponent& owner;
    };

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;
    void timerCallback() override;
    void updateAnalyzerSpectrum(bool preEQ);

    BusDSPProcessor& processor;
    LiveBus::PresetManager& presetManager;
    std::vector<LiveBus::PresetItem> catalogPresetItems;
    juce::ToggleButton enabledToggle{"Enable Monitor EQ"};
    juce::ToggleButton analyzerEnabledToggle{"Analyzer"};
    juce::ToggleButton preAnalyzerToggle{"Pre"};
    juce::ToggleButton postAnalyzerToggle{"Post"};
    juce::ComboBox analyzerModeCombo;
    juce::ComboBox presetCombo;
    juce::TextButton slotAButton{"A"};
    juce::TextButton slotBButton{"B"};
    juce::TextButton copyButton{"Copy"};
    juce::TextButton pasteButton{"Paste"};
    juce::TextButton resetButton{"Flat"};
    std::array<juce::Slider, numMonitorGraphicEQBands> bandSliders;
    std::array<juce::Label, numMonitorGraphicEQBands> bandLabels;
    std::unique_ptr<ResponseDisplay> responseDisplay;
    juce::dsp::FFT analyzerFFT{11};
    std::array<float, 4096> analyzerFFTData{};
    std::array<float, 1024> preAnalyzerSpectrum{};
    std::array<float, 1024> preAnalyzerRightSpectrum{};
    std::array<float, 1024> postAnalyzerSpectrum{};
    std::array<float, 1024> postAnalyzerRightSpectrum{};
    ABState abStates{};
    MonitorGraphicEQ31State copiedState{};
    int activeABSlot = 0;
    bool hasCopiedState = false;
    bool updatingUI = false;

    void storeActiveABSlot();
    void selectABSlot(int slot);
    void updateABButtonColours();
    void refreshPresetCatalog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonitorGraphicEQComponent)
};
