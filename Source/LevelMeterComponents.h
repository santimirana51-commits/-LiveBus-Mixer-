#pragma once
#include <JuceHeader.h>
#include "LevelMeterProcessor.h"

namespace LiveBus
{
    /**
     * Professional Analog VU Meter with physical needle movement,
     * calibrated -20 VU to +3 VU arc scale, red over-0 zone, and clip LED.
     */
    class AnalogVUMeterComponent : public juce::Component
    {
    public:
        AnalogVUMeterComponent(const juce::String& channelLabel = "L");
        ~AnalogVUMeterComponent() override = default;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;

        void updateReading(float vuNormalized, float vuLevel, float peakDbFS, bool isClipped);
        void setChannelLabel(const juce::String& newLabel);
        void setResetCallback(std::function<void()> callback) { onResetClicked = std::move(callback); }

    private:
        juce::String label;
        float currentNormalized = 0.0f; // 0..1+
        float currentVuLevel = -20.0f;
        float currentPeakDb = -100.0f;
        bool clipped = false;

        std::function<void()> onResetClicked;

        // Angle mapping: 0.0 (-20 VU) -> -45 deg, 1.0 (+3 VU) -> +45 deg
        const float minAngleRad = -juce::MathConstants<float>::pi * 0.25f; // -45 deg
        const float maxAngleRad =  juce::MathConstants<float>::pi * 0.25f; // +45 deg
    };

    /**
     * Precision Digital Peak Meter with stereo L/R bars,
     * dBFS scale, color-coded zones, peak-hold markers, and clip indicator.
     */
    class DigitalPeakMeterComponent : public juce::Component
    {
    public:
        enum class Orientation { Vertical, Horizontal };

        DigitalPeakMeterComponent(bool isStereo = true, Orientation orient = Orientation::Vertical);
        ~DigitalPeakMeterComponent() override = default;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;

        void updateReading(const MeterSnapshot& snapshot);
        void setResetCallback(std::function<void()> callback) { onResetClicked = std::move(callback); }
        void setShowScale(bool show) { showScale = show; repaint(); }

    private:
        bool stereo = true;
        Orientation orientation = Orientation::Vertical;
        bool showScale = true;

        MeterSnapshot currentSnapshot;
        std::function<void()> onResetClicked;

        // Helper: Convert dBFS to vertical bar fraction [0..1]
        static float dbToFraction(float db);
    };

    /**
     * Master Meter Console Panel:
     * Combines Dual Analog VU Meters (L & R), Precision Stereo Peak Meters,
     * Target routing selector, VU Calibration selector, Peak Hold toggle, and Reset.
     */
    class MasterMeterPanel : public juce::Component,
                             public juce::ComboBox::Listener,
                             public juce::Button::Listener
    {
    public:
        enum MeterMode
        {
            ModeCombined = 0,
            ModeAnalogVUOnly,
            ModeDigitalPeakOnly
        };

        MasterMeterPanel();
        ~MasterMeterPanel() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void comboBoxChanged(juce::ComboBox* combo) override;
        void buttonClicked(juce::Button* button) override;

        void setTargetProcessor(LevelMeterProcessor* processor, const juce::String& targetName);
        void updateMeters();

        // Callbacks
        std::function<void(int targetIndex)> onTargetChanged;
        std::function<void(float refDbFS)> onCalibrationChanged;
        std::function<void(bool enable)> onPeakHoldToggled;
        std::function<void()> onResetRequested;

    private:
        LevelMeterProcessor* activeProcessor = nullptr;
        MeterMode currentMode = ModeCombined;

        // Header controls
        juce::Label panelTitleLabel;
        juce::ComboBox targetCombo;
        juce::ComboBox modeCombo;
        juce::ComboBox calibrationCombo;
        juce::TextButton peakHoldBtn{"Hold: ON"};
        juce::TextButton resetBtn{"Reset Peak"};

        // Meters
        std::unique_ptr<AnalogVUMeterComponent> vuMeterL;
        std::unique_ptr<AnalogVUMeterComponent> vuMeterR;
        std::unique_ptr<DigitalPeakMeterComponent> peakMeterStereo;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterMeterPanel)
    };
}
