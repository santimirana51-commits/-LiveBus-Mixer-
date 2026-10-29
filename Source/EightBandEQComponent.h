#pragma once
#include <JuceHeader.h>
#include "EightBandEQ.h"
#include "RoutingMatrix.h"
#include "PresetManager.h"
#include <array>
#include <functional>
#include <vector>

class BusDSPProcessor;

namespace LiveBus
{
    class EightBandEQComponent final : public juce::Component,
                                       public juce::Slider::Listener,
                                       public juce::ComboBox::Listener,
                                       public juce::Button::Listener,
                                       public juce::Timer
    {
    public:
        enum TargetIndex : int
        {
            TargetInput1Mic,
            TargetInput2Guitar,
            TargetInput3MusicL,
            TargetInput4MusicR,
            TargetStreamBus = maxInputs,
            TargetMonitorBus,
            NumTargets = maxInputs + 2
        };

        enum DSPNavModule : int
        {
            NavEqualizer = 0,
            NavHPF,
            NavGate,
            NavCompressor,
            NavLimiter,
            NavReverb,
            NavDelay,
            NumNavModules = 7
        };

        explicit EightBandEQComponent(std::array<BusDSPProcessor*, NumTargets> targetDSPs,
                                      PresetManager* presetManager = nullptr);
        explicit EightBandEQComponent(std::array<EightBandEQProcessor*, NumTargets> processors);
        ~EightBandEQComponent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void sliderValueChanged(juce::Slider* slider) override;
        void comboBoxChanged(juce::ComboBox* combo) override;
        void buttonClicked(juce::Button* button) override;
        void timerCallback() override;
        void refreshEQCatalogPresets();

        void setSelectedTarget(TargetIndex target);
        TargetIndex getSelectedTarget() const noexcept { return currentTarget; }
        std::function<void(TargetIndex)> onTargetSelected;
        std::function<void(TargetIndex)> onParametersChanged;

        void setSelectedNavModule(DSPNavModule module);
        DSPNavModule getSelectedNavModule() const noexcept { return currentNavModule; }

        // Update all UI controls from the currently selected target processor
        void updateUIFromProcessor();

        // Custom look and feel for rotary knobs with colored arcs
        class BandKnobLookAndFeel : public juce::LookAndFeel_V4
        {
        public:
            juce::Colour activeColor{0xff3b82f6};

            void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                  float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                  juce::Slider& slider) override;
        };

        // Graph Visualizer for 8-Band Equalizer
        class EQGraphVisualizer final : public juce::Component
        {
        public:
            explicit EQGraphVisualizer(EightBandEQComponent& owner);

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
            EightBandEQComponent& owner;
            int activeDragBand = -1;
            int hoveredBand = -1;
        };

        // Visualizer for High-Pass Filter Curve
        class HPFCurveVisualizer final : public juce::Component
        {
        public:
            explicit HPFCurveVisualizer(EightBandEQComponent& owner);
            void paint(juce::Graphics& g) override;
        private:
            EightBandEQComponent& owner;
        };

        // Visualizer for Noise Gate Transfer & Level
        class GateVisualizer final : public juce::Component
        {
        public:
            explicit GateVisualizer(EightBandEQComponent& owner);
            void paint(juce::Graphics& g) override;
        private:
            EightBandEQComponent& owner;
        };

        // Visualizer for Compressor Knee Transfer Curve
        class CompCurveVisualizer final : public juce::Component
        {
        public:
            explicit CompCurveVisualizer(EightBandEQComponent& owner);
            void paint(juce::Graphics& g) override;
        private:
            EightBandEQComponent& owner;
        };

        // Visualizer for Limiter Ceiling and Peaks
        class LimiterVisualizer final : public juce::Component
        {
        public:
            explicit LimiterVisualizer(EightBandEQComponent& owner);
            void paint(juce::Graphics& g) override;
        private:
            EightBandEQComponent& owner;
        };

        // Visualizer for Reverb Acoustic Space & Decay
        class ReverbVisualizer final : public juce::Component
        {
        public:
            explicit ReverbVisualizer(EightBandEQComponent& owner);
            void paint(juce::Graphics& g) override;
        private:
            EightBandEQComponent& owner;
        };

        // Visualizer for Delay Echo Taps & Trail
        class DelayVisualizer final : public juce::Component
        {
        public:
            explicit DelayVisualizer(EightBandEQComponent& owner);
            void paint(juce::Graphics& g) override;
        private:
            EightBandEQComponent& owner;
        };

        EightBandEQProcessor& getCurrentProcessor();
        const EightBandEQProcessor& getCurrentProcessor() const;

        BusDSPProcessor* getCurrentTargetDSP();
        const BusDSPProcessor* getCurrentTargetDSP() const;

        static juce::Colour getBandColour(int bandIndex);
        static juce::String getTargetName(TargetIndex target);

    private:
        std::array<BusDSPProcessor*, NumTargets> targetDSPs{};
        PresetManager* presetManager = nullptr;
        std::vector<PresetItem> catalogPresetItems;
        std::array<EightBandEQProcessor*, NumTargets> targetProcessors{};
        TargetIndex currentTarget = TargetStreamBus;
        DSPNavModule currentNavModule = NavEqualizer;
        bool isUpdatingUI = false;
        int selectedBand = 0; // 0..7

        // Left Sidebar: DSP Processor list
        juce::Label dspProcessorHeader{"DSP Processor"};
        juce::TextButton eqNavBtn{"Equalizer"};
        juce::TextButton hpfNavBtn{"High-Pass Filter"};
        juce::TextButton gateNavBtn{"Noise Gate"};
        juce::TextButton compNavBtn{"Compressor"};
        juce::TextButton limiterNavBtn{"Limiter"};
        juce::TextButton reverbNavBtn{"Reverb"};
        juce::TextButton delayNavBtn{"Delay"};

        // Left Sidebar: Apply To target buttons
        juce::Label applyToHeader{"Apply To"};
        std::array<juce::TextButton, NumTargets> targetButtons;

        // Shared Module Header Bar (for HPF, Gate, Comp, Limiter, Reverb, Delay)
        juce::TextButton modulePowerBtn{"ON"};
        juce::Label moduleTitleLabel{"Module Title"};
        juce::Label moduleTargetBadge{"[TARGET]"};
        juce::TextButton moduleResetBtn{"Reset to Default"};
        juce::Label moduleCopyLabel{"Copy To"};
        juce::ComboBox moduleCopyCombo;
        juce::TextButton moduleCopyBtn{"Copy"};

        // =====================================================================
        // 1. Equalizer UI Elements
        // =====================================================================
        juce::TextButton powerBtn{"PWR"};
        juce::Label titleLabel{"Parametric Equalizer (8 Band)"};
        juce::ToggleButton eqOnToggle{"EQ On"};
        juce::TextButton analyzerToggleBtn{"Analyzer"};
        juce::TextButton preBtn{"Pre"};
        juce::TextButton postBtn{"Post"};
        juce::Label fftSizeLabel{"FFT Size: 4096"};
        std::unique_ptr<EQGraphVisualizer> graphVisualizer;

        // Spectrum Analyzer FFT
        juce::dsp::FFT fft{fftOrder};
        juce::dsp::WindowingFunction<float> fftWindow{fftSize, juce::dsp::WindowingFunction<float>::hann};
        std::array<float, fftSize * 2> fftData{};
        std::array<float, 512> smoothedFftBins{};

        // 8 Band Bottom Strips
        struct BandStripUI
        {
            juce::Label bandPill;
            juce::ComboBox typeCombo;
            juce::Slider freqSlider;
            juce::Label freqValueLabel;
            juce::Slider gainSlider;
            juce::Label gainValueLabel;
            juce::Slider qSlider;
            juce::Label qValueLabel;
            juce::TextButton bypassBtn{"ON"};
            std::unique_ptr<BandKnobLookAndFeel> knobLaf;
        };
        std::array<BandStripUI, numEQBands> bandStrips;

        juce::Label presetLabel{"EQ Presets"};
        juce::ComboBox presetCombo;
        juce::TextButton savePresetBtn{"Save"};
        juce::TextButton saveAsPresetBtn{"Save As..."};
        juce::TextButton deletePresetBtn{"Delete"};
        juce::Label copyToLabel{"Copy To"};
        juce::ComboBox copyToCombo;
        juce::TextButton copyBtn{"Copy"};
        juce::TextButton resetEQBtn{"Reset EQ"};

        std::vector<EQPreset> userPresets;
        void loadFactoryPresets();
        bool saveCurrentEQCatalogPreset(const juce::String& name,
                                        const PresetItem* existingItem);
        void applyPreset(const EightBandEQParameters& params);

        // =====================================================================
        // 2. High-Pass Filter (HPF) UI Elements
        // =====================================================================
        std::unique_ptr<HPFCurveVisualizer> hpfVisualizer;
        juce::Slider hpfCutoffSlider;
        juce::Label hpfCutoffLabel{"Cutoff: 80 Hz"};
        juce::ComboBox hpfSlopeCombo;
        juce::Label hpfSlopeLabel{"Filter Slope"};
        juce::ComboBox hpfTypeCombo;
        juce::Label hpfTypeLabel{"Topology"};
        std::unique_ptr<BandKnobLookAndFeel> hpfKnobLaf;

        // =====================================================================
        // 3. Noise Gate UI Elements
        // =====================================================================
        std::unique_ptr<GateVisualizer> gateVisualizer;
        juce::Slider gateThreshSlider;
        juce::Label gateThreshLabel{"Thresh: -45 dB"};
        juce::Slider gateAttackSlider;
        juce::Label gateAttackLabel{"Attack: 2 ms"};
        juce::Slider gateHoldSlider;
        juce::Label gateHoldLabel{"Hold: 20 ms"};
        juce::Slider gateReleaseSlider;
        juce::Label gateReleaseLabel{"Release: 100 ms"};
        juce::Slider gateFloorSlider;
        juce::Label gateFloorLabel{"Floor: -60 dB"};
        juce::Slider gateHystSlider;
        juce::Label gateHystLabel{"Hysteresis: 4 dB"};
        juce::Label gateStatusBadge{"[ OPEN ]"};
        std::unique_ptr<BandKnobLookAndFeel> gateKnobLaf;

        // =====================================================================
        // 4. Dynamics Compressor UI Elements
        // =====================================================================
        std::unique_ptr<CompCurveVisualizer> compVisualizer;
        juce::Slider compThreshSlider;
        juce::Label compThreshLabel{"Thresh: -18 dB"};
        juce::Slider compRatioSlider;
        juce::Label compRatioLabel{"Ratio: 3.0:1"};
        juce::Slider compAttackSlider;
        juce::Label compAttackLabel{"Attack: 10 ms"};
        juce::Slider compReleaseSlider;
        juce::Label compReleaseLabel{"Release: 100 ms"};
        juce::ComboBox compKneeCombo;
        juce::Label compKneeLabel{"Knee Type"};
        juce::Slider compMakeupSlider;
        juce::Label compMakeupLabel{"Makeup: 0 dB"};
        juce::Slider compMixSlider;
        juce::Label compMixLabel{"Mix: 100%"};
        std::unique_ptr<BandKnobLookAndFeel> compKnobLaf;

        // =====================================================================
        // 5. Brickwall Peak Limiter UI Elements
        // =====================================================================
        std::unique_ptr<LimiterVisualizer> limiterVisualizer;
        juce::Slider limiterCeilSlider;
        juce::Label limiterCeilLabel{"Ceiling: -1.0 dBFS"};
        juce::Slider limiterReleaseSlider;
        juce::Label limiterReleaseLabel{"Release: 60 ms"};
        juce::Label limiterClipLed{"CLIP"};
        std::unique_ptr<BandKnobLookAndFeel> limiterKnobLaf;

        // =====================================================================
        // 6. Studio Acoustic Reverb UI Elements
        // =====================================================================
        std::unique_ptr<ReverbVisualizer> reverbVisualizer;
        juce::Slider reverbRoomSlider;
        juce::Label reverbRoomLabel{"Room: 50%"};
        juce::Slider reverbDampSlider;
        juce::Label reverbDampLabel{"Damping: 50%"};
        juce::Slider reverbWidthSlider;
        juce::Label reverbWidthLabel{"Width: 100%"};
        juce::Slider reverbWetSlider;
        juce::Label reverbWetLabel{"Wet: 33%"};
        juce::Slider reverbDrySlider;
        juce::Label reverbDryLabel{"Dry: 100%"};
        std::unique_ptr<BandKnobLookAndFeel> reverbKnobLaf;

        // =====================================================================
        // 7. Stereo Creative Delay UI Elements
        // =====================================================================
        std::unique_ptr<DelayVisualizer> delayVisualizer;
        juce::Slider delayTimeSlider;
        juce::Label delayTimeLabel{"Time: 350 ms"};
        juce::Slider delayFeedbackSlider;
        juce::Label delayFeedbackLabel{"Feedback: 35%"};
        juce::Slider delayWetSlider;
        juce::Label delayWetLabel{"Wet: 25%"};
        juce::Slider delayDrySlider;
        juce::Label delayDryLabel{"Dry: 100%"};
        juce::ToggleButton delayPingPongToggle{"Ping-Pong Stereo"};
        std::unique_ptr<BandKnobLookAndFeel> delayKnobLaf;

        void setupKnob(juce::Slider& slider, juce::Label& label, BandKnobLookAndFeel* laf,
                       double min, double max, double def, const juce::String& suffix, double skew = 1.0);
        void updateModuleVisibility();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EightBandEQComponent)
    };
}
