#pragma once
#include <JuceHeader.h>
#include "VocalDSPProcessor.h"
#include "RoutingMatrix.h"
#include <array>
#include <functional>

namespace LiveBus
{
    /**
     * Professional Vocal DSP Rack UI Component.
     * Matches the broadcast dynamics hardware console (eMo D5 style).
     * Features:
     * - Top Header: Target selection (Input 1..16, Stream Bus, Monitor Bus), Presets, Master Bypass
     * - Left Panel: Noise Gate / Expander (Open, Close/Hysteresis, Attack, Release, Hold, Floor, Key filters, Mode)
     * - Center Visualizers:
     *   - Top: Dynamics Transfer / Compression Curve graph
     *   - Middle: Flanking vertical LED GR & Level meters (Gate GR, Gate IN, Comp IN, Comp GR)
     *   - Bottom: 8-Band Parametric EQ & Filter curve visualizer with interactive nodes
     * - Right Panel: Compressor (Threshold, Ratio, Attack, Release, Knee, Auto-Release, Key filters)
     * - Bottom Row:
     *   - LVL: Input Level / Trim & HPF
     *   - DeESSER: Threshold, Freq, Range, Type, GR meter
     *   - LIM: Limiter Ceiling, Release, GR meter
     *   - OUTPUT: Parallel Comp Mix, Makeup Gain, Output Level meter, Clip LED
     */
    class VocalDSPRackComponent final : public juce::Component,
                                        public juce::Slider::Listener,
                                        public juce::Button::Listener,
                                        public juce::ComboBox::Listener,
                                        public juce::Timer
    {
    public:
        enum TargetIndex : int
        {
            TargetInput1Mic = 0,
            TargetInput2Guitar,
            TargetInput3MusicL,
            TargetInput4MusicR,
            TargetStreamBus = maxInputs,
            TargetMonitorBus,
            NumTargets = maxInputs + 2
        };

        explicit VocalDSPRackComponent(std::array<VocalDSPProcessor*, NumTargets> processors);
        ~VocalDSPRackComponent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void sliderValueChanged(juce::Slider* slider) override;
        void buttonClicked(juce::Button* button) override;
        void comboBoxChanged(juce::ComboBox* combo) override;
        void timerCallback() override;

        void setSelectedTarget(TargetIndex target);
        TargetIndex getSelectedTarget() const noexcept { return currentTarget; }
        std::function<void(TargetIndex)> onTargetSelected;
        std::function<void(TargetIndex)> onParametersChanged;

        void updateUIFromProcessor();
        void applyPreset(int presetIndex);

        // Interactive Dynamics Transfer Curve Component
        class DynamicsGraphComponent final : public juce::Component
        {
        public:
            explicit DynamicsGraphComponent(VocalDSPRackComponent& owner);
            void paint(juce::Graphics& g) override;

        private:
            VocalDSPRackComponent& owner;
        };

        // Interactive 8-Band EQ Graph Component
        class EQGraphMiniComponent final : public juce::Component
        {
        public:
            explicit EQGraphMiniComponent(VocalDSPRackComponent& owner);
            void paint(juce::Graphics& g) override;
            void mouseDown(const juce::MouseEvent& e) override;
            void mouseDrag(const juce::MouseEvent& e) override;
            void mouseUp(const juce::MouseEvent& e) override;

        private:
            float freqToX(float freq) const;
            float xToFreq(float x) const;
            float gainToY(float gainDb) const;
            float yToGain(float y) const;

            VocalDSPRackComponent& owner;
            int activeDragBand = -1;
        };

        // Vertical LED Ladder Gain Reduction Meter Component
        class VerticalLedMeterComponent final : public juce::Component
        {
        public:
            enum MeterType { MeterGainReduction, MeterLevelIn, MeterLevelOut };
            explicit VerticalLedMeterComponent(const juce::String& labelText, MeterType type, juce::Colour activeColor);
            void setReading(float valueDb);
            void paint(juce::Graphics& g) override;

        private:
            juce::String label;
            MeterType meterType;
            juce::Colour color;
            float currentDb = -100.0f;
        };

    private:
        VocalDSPProcessor& getCurrentProcessor() { return *targetProcessors[(size_t)currentTarget]; }
        const VocalDSPProcessor& getCurrentProcessor() const { return *targetProcessors[(size_t)currentTarget]; }

        std::array<VocalDSPProcessor*, NumTargets> targetProcessors;
        TargetIndex currentTarget = TargetInput1Mic;
        bool isUpdatingUI = false;

        // Custom Rotary Slider Look & Feel
        class HardwareRotaryLookAndFeel : public juce::LookAndFeel_V4
        {
        public:
            juce::Colour activeColor{0xff3b82f6};
            void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                  float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                  juce::Slider& slider) override;
        };
        HardwareRotaryLookAndFeel knobLA;

        // Helper to setup stylized rotary knobs
        void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text,
                       double minVal, double maxVal, double defaultVal, double step,
                       const juce::String& suffix, juce::Colour tint);

        // =====================================================================
        // Top Header
        // =====================================================================
        juce::TextButton masterEnableBtn{"DSP OFF"};
        juce::Label titleLabel{"LIVEBUS VOCAL DYNAMICS RACK — eMo D5 ARCHITECTURE"};
        juce::ComboBox targetCombo;
        juce::ComboBox presetCombo;
        std::array<juce::TextButton, 6> chainButtons{
            juce::TextButton{"1  HPF"},
            juce::TextButton{"2  Gate"},
            juce::TextButton{"3  EQ"},
            juce::TextButton{"4  De-Esser"},
            juce::TextButton{"5  Comp"},
            juce::TextButton{"6  Limiter"}
        };

        // =====================================================================
        // Left Panel: Noise Gate / Expander
        // =====================================================================
        juce::GroupComponent gateGroup{"gateGroup", "NOISE GATE / EXPANDER"};
        juce::Slider gateOpenThreshSlider;
        juce::Label gateOpenThreshLabel;
        juce::Slider gateCloseThreshSlider;
        juce::Label gateCloseThreshLabel;

        juce::TextButton gateModeBtn{"GATE"};
        juce::TextButton expanderModeBtn{"EXP"};

        juce::Slider gateAttackSlider;
        juce::Label gateAttackLabel;
        juce::Slider gateReleaseSlider;
        juce::Label gateReleaseLabel;
        juce::Slider gateHoldSlider;
        juce::Label gateHoldLabel;
        juce::Slider gateFloorSlider;
        juce::Label gateFloorLabel;

        juce::Label gateKeyLabel{"KEY SOURCE"};
        juce::TextButton gateKeyIntBtn{"INT"};
        juce::TextButton gateKeyExtBtn{"EXT"};
        juce::Slider gateKeyHpfSlider;
        juce::Label gateKeyHpfLabel;
        juce::Slider gateKeyLpfSlider;
        juce::Label gateKeyLpfLabel;

        // =====================================================================
        // Center: Graphs and Flanking Meters
        // =====================================================================
        std::unique_ptr<VerticalLedMeterComponent> gateGrMeter;
        std::unique_ptr<VerticalLedMeterComponent> gateInMeter;
        std::unique_ptr<DynamicsGraphComponent> dynamicsGraph;
        std::unique_ptr<EQGraphMiniComponent> eqGraph;
        std::unique_ptr<VerticalLedMeterComponent> compInMeter;
        std::unique_ptr<VerticalLedMeterComponent> compGrMeter;

        // =====================================================================
        // Right Panel: Compressor
        // =====================================================================
        juce::GroupComponent compGroup{"compGroup", "COMPRESSOR"};
        juce::Slider compThresholdSlider;
        juce::Label compThresholdLabel;
        juce::Slider compRatioSlider;
        juce::Label compRatioLabel;

        juce::TextButton kneeSoftBtn{"SOFT"};
        juce::TextButton kneeNormBtn{"NORMAL"};
        juce::TextButton kneeHardBtn{"HARD"};

        juce::Slider compAttackSlider;
        juce::Label compAttackLabel;
        juce::Slider compReleaseSlider;
        juce::Label compReleaseLabel;

        juce::TextButton relManualBtn{"MANUAL"};
        juce::TextButton relAutoBtn{"AUTO"};

        juce::Label compKeyLabel{"KEY SOURCE"};
        juce::TextButton compKeyIntBtn{"INT"};
        juce::TextButton compKeyDeSBtn{"DeS"};
        juce::TextButton compKeyExtBtn{"EXT"};
        juce::Slider compKeyHpfSlider;
        juce::Label compKeyHpfLabel;
        juce::Slider compKeyLpfSlider;
        juce::Label compKeyLpfLabel;

        // =====================================================================
        // Bottom Row: Modules
        // =====================================================================
        // 1. LVL / HPF
        juce::GroupComponent lvlGroup{"lvlGroup", "LVL & HPF"};
        juce::TextButton lvlBypassBtn{"LVL"};
        juce::Slider inputGainSlider;
        juce::Label inputGainLabel;
        juce::Slider hpfFreqSlider;
        juce::Label hpfFreqLabel;
        juce::ComboBox hpfSlopeCombo;

        // 2. De-Esser
        juce::GroupComponent deEsserGroup{"deEsserGroup", "DeESSER"};
        juce::TextButton deEsserBypassBtn{"DeESSER"};
        juce::Slider deEsserThreshSlider;
        juce::Label deEsserThreshLabel;
        juce::Slider deEsserFreqSlider;
        juce::Label deEsserFreqLabel;
        juce::Slider deEsserRangeSlider;
        juce::Label deEsserRangeLabel;
        juce::TextButton deEsserTypeBtn{"BELL"};
        std::unique_ptr<VerticalLedMeterComponent> deEsserGrMeter;

        // 3. Limiter
        juce::GroupComponent limGroup{"limGroup", "LIMITER"};
        juce::TextButton limBypassBtn{"LIM"};
        juce::Slider limCeilingSlider;
        juce::Label limCeilingLabel;
        juce::Slider limReleaseSlider;
        juce::Label limReleaseLabel;
        std::unique_ptr<VerticalLedMeterComponent> limGrMeter;

        // 4. Output
        juce::GroupComponent outputGroup{"outputGroup", "OUTPUT"};
        juce::Slider compMixSlider;
        juce::Label compMixLabel;
        juce::Slider compMakeupSlider;
        juce::Label compMakeupLabel;
        juce::Slider outputGainSlider;
        juce::Label outputGainLabel;
        std::unique_ptr<VerticalLedMeterComponent> outMeter;
        juce::Label clipLed{"CLIP"};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalDSPRackComponent)
    };
}
