#pragma once
#include <JuceHeader.h>
#include "PitchDetector.h"
#include "PitchCorrection.h"

class VocalFXProAudioProcessor : public juce::AudioProcessor
{
public:
    VocalFXProAudioProcessor();
    ~VocalFXProAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocal FX Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 5; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak{0.0f}, outputPeak{0.0f};
    std::atomic<float> currentDetectedHz{0.0f};
    std::atomic<float> currentTargetHz{0.0f};

    float getDetectedPitch() const noexcept { return currentDetectedHz.load(); }
    float getTargetPitch() const noexcept { return currentTargetHz.load(); }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowFilter, midFilter, highFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> deessSidechainFilter;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 192000 };
    juce::dsp::ProcessSpec spec {};
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> detectorBuffer;
    juce::Random random;
    double sr = 44100.0;
    int currentProgram = 0;
    float env = 0.0f;
    float pitchPhaseL = 0.0f, pitchPhaseR = 0.37f;

    VocalPitchDetector pitchDetector;
    VocalPitchCorrection pitchCorrection;

    std::atomic<float>* p(const char* id) { return parameters.getRawParameterValue(id); }
    void updateFilters();
    void applyPitchAndRobot(juce::AudioBuffer<float>& buffer, int numSamples);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalFXProAudioProcessor)
};
