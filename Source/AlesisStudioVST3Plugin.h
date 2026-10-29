#pragma once

#include <JuceHeader.h>
#include "AlesisVintageStudioDSP.h"
#include <atomic>

namespace LiveBus
{

class AlesisStudioVST3Plugin final : public juce::AudioProcessor
{
public:
    AlesisStudioVST3Plugin();
    ~AlesisStudioVST3Plugin() override;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LiveBus Alesis Vintage Studio"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.5; }
    int getNumPrograms() override { return 5; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }
    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    AlesisVintageStudioDSP& getAlesisDSP() noexcept { return alesisDSP; }
    const AlesisVintageStudioDSP& getAlesisDSP() const noexcept { return alesisDSP; }

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncParameters() noexcept;

    AlesisVintageStudioDSP alesisDSP;
    int currentProgram{0};

    // Master Bypass
    std::atomic<float>* masterBypassParam{nullptr};

    // 1. Reverb
    std::atomic<float>* verbBypassParam{nullptr};
    std::atomic<float>* verbTypeParam{nullptr};
    std::atomic<float>* verbDecayParam{nullptr};
    std::atomic<float>* verbSizeParam{nullptr};
    std::atomic<float>* verbDampParam{nullptr};
    std::atomic<float>* verbPreDelayParam{nullptr};
    std::atomic<float>* verbWetParam{nullptr};
    std::atomic<float>* verbLowCutParam{nullptr};
    std::atomic<float>* verbHighCutParam{nullptr};

    // 2. Delay & Echo
    std::atomic<float>* delayBypassParam{nullptr};
    std::atomic<float>* delayModeParam{nullptr};
    std::atomic<float>* delayTimeParam{nullptr};
    std::atomic<float>* delayFeedbackParam{nullptr};
    std::atomic<float>* delayWetParam{nullptr};
    std::atomic<float>* delayFilterParam{nullptr};

    // 3. Parametric EQ
    std::atomic<float>* eqBypassParam{nullptr};
    std::atomic<float>* eqHpParam{nullptr};
    std::atomic<float>* eqLowShelfParam{nullptr};
    std::atomic<float>* eqLowMidParam{nullptr};
    std::atomic<float>* eqMidParam{nullptr};
    std::atomic<float>* eqHighMidParam{nullptr};
    std::atomic<float>* eqHighShelfParam{nullptr};
    std::atomic<float>* eqLpParam{nullptr};

    // 4. Compressor
    std::atomic<float>* compBypassParam{nullptr};
    std::atomic<float>* compThreshParam{nullptr};
    std::atomic<float>* compRatioParam{nullptr};
    std::atomic<float>* compAttackParam{nullptr};
    std::atomic<float>* compReleaseParam{nullptr};
    std::atomic<float>* compHardKneeParam{nullptr};
    std::atomic<float>* compMakeupParam{nullptr};
    std::atomic<float>* compAutoGainParam{nullptr};
    std::atomic<float>* compMixParam{nullptr};

    // 5. Vocal Enhancements
    std::atomic<float>* deessBypassParam{nullptr};
    std::atomic<float>* deessAmountParam{nullptr};
    std::atomic<float>* gateBypassParam{nullptr};
    std::atomic<float>* gateThreshParam{nullptr};
    std::atomic<float>* satAmountParam{nullptr};
    std::atomic<float>* exciterAmountParam{nullptr};
    std::atomic<float>* stereoWidthParam{nullptr};

    // 6. Creative FX
    std::atomic<float>* creativeModeParam{nullptr};
    std::atomic<float>* creativeMixParam{nullptr};

    // 7. AutoTune
    std::atomic<float>* atBypassParam{nullptr};
    std::atomic<float>* atAmountParam{nullptr};
    std::atomic<float>* atSpeedParam{nullptr};
    std::atomic<float>* atKeyParam{nullptr};
    std::atomic<float>* atScaleParam{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlesisStudioVST3Plugin)
};

} // namespace LiveBus
