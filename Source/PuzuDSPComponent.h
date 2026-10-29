#pragma once
#include <JuceHeader.h>
#include "ChannelStripDSP.h"
#include "RoutingMatrix.h"
#include "EQFrequencyAnalyzer.h"
#include "PresetManager.h"

class MainComponent; // Forward declaration

class PuzuDSPComponent final : public juce::Component,
                               public juce::Timer,
                               public juce::Slider::Listener,
                               public juce::Button::Listener,
                               public juce::ComboBox::Listener
{
public:
    using ABSlots =
        std::array<std::array<PuzuEQState, 2>, maxInputs + 2>;
    using ActiveABSlots = std::array<int, maxInputs + 2>;

    explicit PuzuDSPComponent(
        std::array<BusDSPProcessor*, maxInputs + 2> processors,
        LiveBus::PresetManager& presetManager);
    ~PuzuDSPComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* combo) override;

    void updateUIFromProcessor();
    void refreshPresetCatalog();
    void setSelectedTarget(int targetIndex);
    ABSlots getSceneABStates();
    ActiveABSlots getSceneActiveABSlots() const noexcept { return activeABSlots; }
    void setSceneABStates(const ABSlots& states,
                          const ActiveABSlots& activeSlots);
    std::function<void(int)> onTargetSelected;

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

    BusDSPProcessor& getCurrentProcessor() { return *targetProcessors[(size_t)selectedTarget]; }
    const BusDSPProcessor& getCurrentProcessor() const { return *targetProcessors[(size_t)selectedTarget]; }

private:
    std::array<BusDSPProcessor*, maxInputs + 2> targetProcessors;
    LiveBus::PresetManager& presetManager;
    std::vector<LiveBus::PresetItem> catalogPresetItems;
    int selectedTarget = maxInputs;
    int selectedBand = 0;
    bool isUpdatingUI = false;
    static constexpr int analyzerMaxFftSize = 4096;
    static constexpr int analyzerDisplayBins = 180;
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
    juce::ToggleButton analyzerEnabledToggle{"Analyzer"};
    juce::ToggleButton preSpectrumToggle{"Pre"};
    juce::ToggleButton postSpectrumToggle{"Post"};
    juce::ToggleButton peakHoldToggle{"Peak hold"};
    juce::ComboBox spectrumModeCombo;
    juce::ComboBox fftSizeCombo;
    juce::ComboBox averagingTimeCombo;
    double lastAnalyzerUpdateMs = 0.0;
    float analyzerElapsedSeconds = 1.0f / 30.0f;
    bool hasPreSpectrum = false;
    bool hasPostSpectrum = false;

    void updateSpectrum(BusDSPProcessor& processor, bool post);
    void drawSpectrum(juce::Graphics& graphics, const juce::Rectangle<float>& bounds);
    void initializeABForSelectedTarget();
    void storeActiveABSlot();
    void selectABSlot(int slot);
    void copyPuzuEQ();
    void pastePuzuEQ();
    void calculateSpectrum(const float* input,
                           std::array<float, analyzerDisplayBins>& spectrum,
                           bool hasPreviousSpectrum);
    void clearAnalyzerHistory();

    // Header & Logo
    juce::Label brandLabel;
    juce::Label subBrandLabel;
    juce::ComboBox targetCombo;
    juce::ComboBox presetCombo;
    juce::TextButton peqModeBtn;
    juce::TextButton rstBtn;
    juce::TextButton passBtn;
    juce::TextButton sysSetBtn;
    juce::TextButton slotAButton{"A"};
    juce::TextButton slotBButton{"B"};
    juce::TextButton copyEQButton{"Copy"};
    juce::TextButton pasteEQButton{"Paste"};

    std::array<std::array<PuzuEQState, 2>, maxInputs + 2> abStates{};
    std::array<bool, maxInputs + 2> abStateInitialized{};
    std::array<int, maxInputs + 2> activeABSlots{};
    PuzuEQState copiedPuzuEQ;
    bool hasCopiedPuzuEQ = false;

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

    juce::ComboBox selectedBandCombo;
    juce::ComboBox filterTypeCombo;
    juce::ToggleButton bandEnabledToggle{"Band enabled"};
    juce::Slider bandFrequencySlider;
    juce::Slider bandQSlider;

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
