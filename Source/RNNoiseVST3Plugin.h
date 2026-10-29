#pragma once

#include <JuceHeader.h>
#include "AsyncAudioWorker.h"
#include "RNNoiseNoiseSuppressor.h"
#include "VocalDSPProcessor.h"
#include "VocalEnhancerDSP.h"
#include "VocalStudioPresets.h"
#include <cmath>
#include <memory>

class RNNoiseVST3Plugin final : public juce::AudioProcessor
{
public:
    RNNoiseVST3Plugin();
    ~RNNoiseVST3Plugin() override;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LiveBus RNNoise"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Preset & State helper methods
    void loadFactoryPreset(int index);
    juce::String exportPresetXml(const juce::String& presetName);
    bool importPresetXml(const juce::String& xmlString, juce::String& loadedName);
    int getActiveProgramIndex() const noexcept { return currentProgramIndex.load(std::memory_order_relaxed); }

    bool isProcessingAvailable() const noexcept
    {
        return workerStarted.load(std::memory_order_acquire);
    }
    bool isSampleRateSupported() const noexcept
    {
        return std::abs(currentSampleRate.load(std::memory_order_acquire) -
                        LiveBus::RNNoiseNoiseSuppressor::RequiredSampleRate) < 0.5;
    }
    float getSpeechProbability() const noexcept
    {
        return suppressor.getLastSpeechProbability();
    }

    // Direct access to Vocal DSP processors for UI visualization & meter reading
    LiveBus::VocalDSPProcessor& getVocalDSP() noexcept { return vocalProcessor; }
    const LiveBus::VocalDSPProcessor& getVocalDSP() const noexcept { return vocalProcessor; }

    LiveBus::VocalEnhancerDSP& getEnhancerDSP() noexcept { return enhancer; }
    const LiveBus::VocalEnhancerDSP& getEnhancerDSP() const noexcept { return enhancer; }

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncParametersFromTreeState() noexcept;
    void updateLatency();

    // 1. Core RNNoise AI Engine
    LiveBus::RNNoiseNoiseSuppressor suppressor;
    std::unique_ptr<LiveBus::AsyncAudioWorker> worker;

    // 2. Studio Grade Vocal DSP Suite (HPF, Gate, 8-Band EQ, De-Esser, Comp, Limiter)
    LiveBus::VocalDSPProcessor vocalProcessor;

    // 3. Vocal Enhancer Module (Warmth, Body, Clarity, Presence, Air)
    LiveBus::VocalEnhancerDSP enhancer;

    // Raw atomic parameter pointers for real-time safety
    std::atomic<float>* masterBypassParam{nullptr};

    // RNNoise
    std::atomic<float>* rnnoiseBypassParam{nullptr};
    std::atomic<float>* enableVADParam{nullptr};
    std::atomic<float>* vadThresholdParam{nullptr};
    std::atomic<float>* gracePeriodParam{nullptr};
    std::atomic<float>* wetLevelParam{nullptr};

    // Input Trim & HPF
    std::atomic<float>* inputGainParam{nullptr};
    std::atomic<float>* hpfBypassParam{nullptr};
    std::atomic<float>* hpfFreqParam{nullptr};
    std::atomic<float>* hpfSlopeParam{nullptr};

    // Noise Gate
    std::atomic<float>* gateBypassParam{nullptr};
    std::atomic<float>* gateThresholdParam{nullptr};
    std::atomic<float>* gateAttackParam{nullptr};
    std::atomic<float>* gateHoldParam{nullptr};
    std::atomic<float>* gateReleaseParam{nullptr};
    std::atomic<float>* gateFloorParam{nullptr};

    // 8-Band Parametric EQ
    std::atomic<float>* eqBypassParam{nullptr};
    struct BandParamPtrs
    {
        std::atomic<float>* freq{nullptr};
        std::atomic<float>* gain{nullptr};
        std::atomic<float>* q{nullptr};
        std::atomic<float>* type{nullptr};
        std::atomic<float>* enabled{nullptr};
    };
    std::array<BandParamPtrs, LiveBus::numEQBands> eqBandParams;

    // De-Esser
    std::atomic<float>* deEsserBypassParam{nullptr};
    std::atomic<float>* deEsserThresholdParam{nullptr};
    std::atomic<float>* deEsserFreqParam{nullptr};
    std::atomic<float>* deEsserRangeParam{nullptr};
    std::atomic<float>* deEsserTypeParam{nullptr};

    // Compressor
    std::atomic<float>* compBypassParam{nullptr};
    std::atomic<float>* compThresholdParam{nullptr};
    std::atomic<float>* compRatioParam{nullptr};
    std::atomic<float>* compAttackParam{nullptr};
    std::atomic<float>* compReleaseParam{nullptr};
    std::atomic<float>* compKneeParam{nullptr};
    std::atomic<float>* compMakeupParam{nullptr};
    std::atomic<float>* compMixParam{nullptr};
    std::atomic<float>* compAutoReleaseParam{nullptr};

    // Vocal Enhancement (Clarity, Presence, Warmth, Body, Air)
    std::atomic<float>* enhanceBypassParam{nullptr};
    std::atomic<float>* warmthParam{nullptr};
    std::atomic<float>* bodyParam{nullptr};
    std::atomic<float>* clarityParam{nullptr};
    std::atomic<float>* presenceParam{nullptr};
    std::atomic<float>* airParam{nullptr};

    // Output Limiter & Gain
    std::atomic<float>* limiterBypassParam{nullptr};
    std::atomic<float>* limiterCeilingParam{nullptr};
    std::atomic<float>* limiterReleaseParam{nullptr};
    std::atomic<float>* outputGainParam{nullptr};

    std::atomic<double> currentSampleRate{48000.0};
    int maximumBlockSize{512};
    std::atomic<bool> workerStarted{false};
    std::atomic<int> currentProgramIndex{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RNNoiseVST3Plugin)
};
