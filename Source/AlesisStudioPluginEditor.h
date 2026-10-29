#pragma once

#include <JuceHeader.h>
#include "AlesisStudioVST3Plugin.h"

namespace LiveBus
{

class AlesisStudioPluginEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit AlesisStudioPluginEditor(AlesisStudioVST3Plugin& plugin);
    ~AlesisStudioPluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void setupRotary(juce::Slider& slider, juce::Label& label, const juce::String& text,
                     const juce::String& suffix, juce::Colour fillCol);
    void setupToggle(juce::ToggleButton& btn, const juce::String& text);

    AlesisStudioVST3Plugin& processor;

    // Header & Preset Controls
    juce::Label titleLabel;
    juce::Label subTitleLabel;
    juce::ComboBox presetDropdown;
    juce::ToggleButton masterBypassToggle{"BYPASS ALL"};

    // Tabbed Multi-Page Interface for Complete Studio Modules
    juce::TabbedComponent tabComponent{juce::TabbedButtonBar::TabsAtTop};

    // --- Tab 1: COMPRESSOR & VOCAL DYNAMICS ---
    juce::Component compTab;
    juce::GroupComponent compGroup{"compGroup", "ALESIS 3630 PUNCH COMPRESSOR"};
    juce::ToggleButton compBypassBtn{"Compressor Bypass"};
    juce::ToggleButton compHardKneeToggle{"Hard Knee (Soft if off)"};
    juce::ToggleButton compAutoGainToggle{"Auto Makeup Gain"};
    juce::Slider compThreshSlider, compRatioSlider, compAttackSlider, compReleaseSlider;
    juce::Slider compMakeupSlider, compMixSlider;
    juce::Label compThreshLabel, compRatioLabel, compAttackLabel, compReleaseLabel, compMakeupLabel, compMixLabel;

    juce::GroupComponent dynGroup{"dynGroup", "VOCAL ENHANCEMENT & DYNAMICS"};
    juce::ToggleButton deessBypassBtn{"De-Esser Bypass"};
    juce::Slider deessSlider; juce::Label deessLabel;
    juce::ToggleButton gateBypassBtn{"Noise Gate Bypass"};
    juce::Slider gateSlider; juce::Label gateLabel;
    juce::Slider satSlider; juce::Label satLabel;
    juce::Slider exciterSlider; juce::Label exciterLabel;
    juce::Slider widthSlider; juce::Label widthLabel;

    // --- Tab 2: REVERB & DELAY ---
    juce::Component spaceTab;
    juce::GroupComponent verbGroup{"verbGroup", "ALESIS VINTAGE SPACE REVERB"};
    juce::ToggleButton verbBypassBtn{"Reverb Bypass"};
    juce::ComboBox verbTypeCombo;
    juce::Slider verbDecaySlider, verbSizeSlider, verbDampSlider, verbPreDelaySlider, verbWetSlider;
    juce::Slider verbLowCutSlider, verbHighCutSlider;
    juce::Label verbDecayLabel, verbSizeLabel, verbDampLabel, verbPreDelayLabel, verbWetLabel;
    juce::Label verbLowCutLabel, verbHighCutLabel;

    juce::GroupComponent delayGroup{"delayGroup", "STEREO & PING-PONG DELAY / ECHO"};
    juce::ToggleButton delayBypassBtn{"Delay Bypass"};
    juce::ComboBox delayModeCombo;
    juce::Slider delayTimeSlider, delayFeedbackSlider, delayWetSlider, delayFilterSlider;
    juce::Label delayTimeLabel, delayFeedbackLabel, delayWetLabel, delayFilterLabel;

    // --- Tab 3: PARAMETRIC EQ & SPECTRUM ---
    juce::Component eqTab;
    juce::GroupComponent eqGroup{"eqGroup", "7-BAND STUDIO PARAMETRIC EQUALIZER"};
    juce::ToggleButton eqBypassBtn{"EQ Bypass"};
    juce::Slider eqHpSlider, eqLowShelfSlider, eqLowMidSlider, eqMidSlider, eqHighMidSlider, eqHighShelfSlider, eqLpSlider;
    juce::Label eqHpLabel, eqLowShelfLabel, eqLowMidLabel, eqMidLabel, eqHighMidLabel, eqHighShelfLabel, eqLpLabel;

    // --- Tab 4: AUTOTUNE & CREATIVE FX ---
    juce::Component tuneTab;
    juce::GroupComponent atGroup{"atGroup", "REAL-TIME VOCAL AUTOTUNE & PITCH CORRECTION"};
    juce::ToggleButton atBypassBtn{"AutoTune Bypass"};
    juce::ComboBox atKeyCombo;
    juce::ComboBox atScaleCombo;
    juce::Slider atAmountSlider, atSpeedSlider;
    juce::Label atKeyLabel, atScaleLabel, atAmountLabel, atSpeedLabel;
    juce::Label pitchTrackLabel{"Tracking: -- Hz -> Target: -- Hz"};

    juce::GroupComponent creativeGroup{"creativeGroup", "CREATIVE MODULATION & RETRO FX"};
    juce::ComboBox creativeModeCombo;
    juce::Slider creativeMixSlider;
    juce::Label creativeMixLabel;

    // Interactive FFT Spectrum Analyzer Display
    class SpectrumAnalyzerComponent final : public juce::Component
    {
    public:
        SpectrumAnalyzerComponent()
        {
            fftScope.resize(scopeSize, 0.0f);
        }

        void updateSpectrum(const float* timeDomainSamples, int numSamples)
        {
            if (timeDomainSamples == nullptr || numSamples < fftSize) return;

            std::copy(timeDomainSamples, timeDomainSamples + fftSize, fftData.data());
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

            for (int i = 0; i < scopeSize; ++i)
            {
                // Map logarithmic frequency bin
                float skew = std::pow((float)i / (float)scopeSize, 2.5f);
                int bin = juce::jlimit(1, fftSize / 2 - 1, (int)(skew * (float)(fftSize / 2)));
                float mag = fftData[(size_t)bin];
                float db = juce::Decibels::gainToDecibels(mag, -70.0f);
                float norm = juce::jmap(db, -70.0f, 10.0f, 0.0f, 1.0f);
                norm = juce::jlimit(0.0f, 1.0f, norm);

                // Decay smoothing
                if (norm > fftScope[(size_t)i])
                    fftScope[(size_t)i] = norm;
                else
                    fftScope[(size_t)i] = 0.82f * fftScope[(size_t)i] + 0.18f * norm;
            }
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour(juce::Colour(0xff090e17));
            g.fillRoundedRectangle(b, 6.0f);
            g.setColour(juce::Colour(0xff1e293b));
            g.drawRoundedRectangle(b, 6.0f, 1.0f);

            // Grid lines
            g.setColour(juce::Colour(0xff141c2c));
            for (float f : { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f })
            {
                float xFrac = std::log10(f / 20.0f) / std::log10(20000.0f / 20.0f);
                float x = b.getX() + xFrac * b.getWidth();
                g.drawVerticalLine((int)x, b.getY(), b.getBottom());
            }
            for (int y = 1; y <= 3; ++y)
                g.drawHorizontalLine((int)(b.getY() + b.getHeight() * (y / 4.0f)), b.getX(), b.getRight());

            // Render Spectrum Curve with Cyan/Purple Gradient
            juce::Path p;
            float step = b.getWidth() / (float)(scopeSize - 1);
            p.startNewSubPath(b.getX(), b.getBottom());

            for (int i = 0; i < scopeSize; ++i)
            {
                float x = b.getX() + (float)i * step;
                float y = b.getBottom() - fftScope[(size_t)i] * (b.getHeight() - 8.0f);
                p.lineTo(x, y);
            }
            p.lineTo(b.getRight(), b.getBottom());
            p.closeSubPath();

            juce::ColourGradient grad(juce::Colour(0x6006b6d4), 0, b.getY(),
                                      juce::Colour(0x1038bdf8), 0, b.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(p);

            g.setColour(juce::Colour(0xff38bdf8));
            g.strokePath(p, juce::PathStrokeType(1.5f));
        }

    private:
        static constexpr int fftOrder = 10;
        static constexpr int fftSize = 1 << fftOrder; // 1024
        static constexpr int scopeSize = 128;
        juce::dsp::FFT forwardFFT{fftOrder};
        juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann};
        std::array<float, fftSize * 2> fftData{};
        std::vector<float> fftScope;
    };

    SpectrumAnalyzerComponent spectrumAnalyzer;

    // Right Sidebar: Dual Level Meters + LED Gain Reduction
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
    LedLadderMeter inMeter{"IN", LedLadderMeter::MeterLevel, juce::Colour(0xff10b981)};
    LedLadderMeter grMeter{"3630 GR", LedLadderMeter::MeterGR, juce::Colour(0xff06b6d4)};
    LedLadderMeter outMeter{"OUT", LedLadderMeter::MeterLevel, juce::Colour(0xff38bdf8)};

    // Parameter Attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;

    // Interactive Hover Tooltips
    juce::TooltipWindow tooltipWindow{this, 350};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlesisStudioPluginEditor)
};

} // namespace LiveBus
