#pragma once

#include <JuceHeader.h>
#include "ChannelStripDSP.h"
#include "EQFrequencyAnalyzer.h"
#include "PresetManager.h"

class MainOutputEQComponent final : public juce::Component,
                                    public juce::Timer,
                                    private juce::Slider::Listener,
                                    private juce::Button::Listener,
                                    private juce::ComboBox::Listener
{
public:
    using ABSlots = std::array<MainOutputEQ24State, 2>;

    MainOutputEQComponent(BusDSPProcessor& processor,
                          LiveBus::PresetManager& presetManager);
    ~MainOutputEQComponent() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void timerCallback() override;
    void updateUIFromProcessor();
    void refreshPresetCatalog();
    ABSlots getSceneABStates();
    int getSceneActiveABSlot() const noexcept { return activeSlot; }
    void setSceneABStates(const ABSlots& states, int active);

private:
    static constexpr int analyzerMaxFftSize = 4096;
    static constexpr int analyzerDisplayBins = 180;

    class ResponseDisplay final : public juce::Component
    {
    public:
        explicit ResponseDisplay(MainOutputEQComponent& component)
            : owner(component) {}
        void paint(juce::Graphics& graphics) override;

    private:
        MainOutputEQComponent& owner;
    };

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;
    void setSelectedBand(int band);
    void storeActiveSlot();
    void selectSlot(int slot);
    void updateSpectrum(bool post);
    void calculateSpectrum(const float* input, std::array<float, analyzerDisplayBins>& spectrum,
                           bool hasPreviousSpectrum);

    BusDSPProcessor& processor;
    LiveBus::PresetManager& presetManager;
    std::vector<LiveBus::PresetItem> catalogPresetItems;
    juce::ComboBox presetCombo;
    juce::ToggleButton enabledToggle{"Enable Main Output EQ"};
    juce::ToggleButton analyzerEnabledToggle{"Analyzer"};
    juce::ToggleButton preSpectrumToggle{"Pre"};
    juce::ToggleButton postSpectrumToggle{"Post"};
    juce::ToggleButton peakHoldToggle{"Peak hold"};
    juce::ToggleButton bandSoloToggle{"Solo band"};
    juce::Label presetLabel{"EQ Preset"};
    juce::ComboBox selectedBandCombo;
    juce::ComboBox filterTypeCombo;
    juce::ToggleButton bandEnabledToggle{"Band enabled"};
    juce::ToggleButton dynamicEnabledToggle{"Dynamic"};
    juce::ComboBox averagingTimeCombo;
    juce::Slider frequencySlider;
    juce::Slider qSlider;
    juce::Slider dynamicThresholdSlider;
    juce::Slider dynamicAttackSlider;
    juce::Slider dynamicReleaseSlider;
    juce::Slider dynamicRangeSlider;
    juce::ComboBox dynamicDetectorCombo;
    juce::ComboBox fftSizeCombo;
    juce::ComboBox spectrumModeCombo;
    juce::TextButton slotAButton{"A"};
    juce::TextButton slotBButton{"B"};
    juce::TextButton copyButton{"Copy"};
    juce::TextButton pasteButton{"Paste"};
    std::array<juce::Slider, numMainOutputEQBands> gainSliders;
    std::array<juce::Label, numMainOutputEQBands> frequencyLabels;
    std::unique_ptr<ResponseDisplay> responseDisplay;
    juce::dsp::FFT analyzerFFT1024{10};
    juce::dsp::FFT analyzerFFT2048{11};
    juce::dsp::FFT analyzerFFT4096{12};
    juce::dsp::WindowingFunction<float> analyzerWindow1024{
        1024, juce::dsp::WindowingFunction<float>::hann};
    juce::dsp::WindowingFunction<float> analyzerWindow2048{
        2048, juce::dsp::WindowingFunction<float>::hann};
    juce::dsp::WindowingFunction<float> analyzerWindow4096{
        analyzerMaxFftSize, juce::dsp::WindowingFunction<float>::hann};
    std::array<float, analyzerMaxFftSize * 2> analyzerFftData{};
    std::array<float, analyzerMaxFftSize> analyzerWindowedInput{};
    std::array<float, analyzerMaxFftSize> analyzerLeftInput{};
    std::array<float, analyzerMaxFftSize> analyzerRightInput{};
    std::array<float, analyzerDisplayBins> preSpectrumDb{};
    std::array<float, analyzerDisplayBins> preRightSpectrumDb{};
    std::array<float, analyzerDisplayBins> postSpectrumDb{};
    std::array<float, analyzerDisplayBins> postRightSpectrumDb{};
    std::array<float, analyzerDisplayBins> prePeakHoldDb{};
    std::array<float, analyzerDisplayBins> preRightPeakHoldDb{};
    std::array<float, analyzerDisplayBins> postPeakHoldDb{};
    std::array<float, analyzerDisplayBins> postRightPeakHoldDb{};
    EQFrequencyPeakAnalysis peakAnalysis;
    double lastAnalyzerUpdateMs = 0.0;
    float analyzerElapsedSeconds = 1.0f / 30.0f;
    bool hasPreSpectrum = false;
    bool hasPostSpectrum = false;
    std::array<MainOutputEQ24State, 2> abStates{};
    MainOutputEQ24State copiedState{};
    int selectedBand = 0;
    int activeSlot = 0;
    bool hasCopiedState = false;
    bool updatingUI = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainOutputEQComponent)
};
