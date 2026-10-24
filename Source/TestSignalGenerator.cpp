#include "TestSignalGenerator.h"
#include <cmath>

namespace LiveBus
{

TestSignalGenerator::TestSignalGenerator()
{
    // Initialize PRNG state with non-zero seed
    prngState = 0x853c49e6;
}

void TestSignalGenerator::prepareToPlay(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    currentPhase = 0.0;

    double freq = juce::jlimit(20.0, currentSampleRate * 0.49, frequencyHz.load(std::memory_order_relaxed));
    currentPhaseDelta = (freq * juce::MathConstants<double>::twoPi) / currentSampleRate;
    targetPhaseDelta = currentPhaseDelta;

    // 15ms anti-click ramp
    gainRampPerSample = (float)(1.0 / (0.015 * currentSampleRate));

    // Reset filter states
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    impulseCounter = 0;
    currentGain = 0.0f;
    targetGain = 0.0f;
}

void TestSignalGenerator::setEnabled(bool enable)
{
    if (enable)
    {
        isRunning.store(true, std::memory_order_release);
    }
    else
    {
        isRunning.store(false, std::memory_order_release);
    }
}

void TestSignalGenerator::setSignalType(SignalType type)
{
    signalType.store(static_cast<int>(type), std::memory_order_relaxed);
}

void TestSignalGenerator::setFrequency(double freqHz)
{
    double safeFreq = juce::jlimit(20.0, currentSampleRate * 0.49, freqHz);
    frequencyHz.store(safeFreq, std::memory_order_relaxed);
    targetPhaseDelta = (safeFreq * juce::MathConstants<double>::twoPi) / currentSampleRate;
}

void TestSignalGenerator::setLevelDb(float db)
{
    float clamped = juce::jlimit(-90.0f, 0.0f, db);
    levelDbfs.store(clamped, std::memory_order_relaxed);
}

void TestSignalGenerator::setDestination(SignalDestination dest)
{
    destination.store(static_cast<int>(dest), std::memory_order_relaxed);
}

void TestSignalGenerator::setChannel(SignalChannel chan)
{
    channelMode.store(static_cast<int>(chan), std::memory_order_relaxed);
}

void TestSignalGenerator::setMute(bool mute)
{
    isMute.store(mute, std::memory_order_relaxed);
}

void TestSignalGenerator::setContinuous(bool cont)
{
    continuousMode.store(cont, std::memory_order_relaxed);
}

void TestSignalGenerator::triggerBurst(double durationSeconds)
{
    int samples = (int)(durationSeconds * currentSampleRate);
    burstSamplesRemaining.store(juce::jmax(1, samples), std::memory_order_release);
    isRunning.store(true, std::memory_order_release);
}

void TestSignalGenerator::renderBlock(float* const* channelData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0 || channelData == nullptr)
        return;

    bool active = isRunning.load(std::memory_order_acquire);
    bool muted = isMute.load(std::memory_order_relaxed);
    bool isCont = continuousMode.load(std::memory_order_relaxed);

    if (!isCont)
    {
        int remaining = burstSamplesRemaining.load(std::memory_order_relaxed);
        if (remaining <= 0)
            active = false;
    }

    float targetLinGain = 0.0f;
    if (active && !muted)
    {
        float db = levelDbfs.load(std::memory_order_relaxed);
        targetLinGain = std::pow(10.0f, db / 20.0f);
    }
    targetGain = targetLinGain;

    // Fast check: If currently silent and target is silent, clear meters and return
    if (currentGain <= 0.00001f && targetGain <= 0.00001f)
    {
        peakMeterL.store(0.0f, std::memory_order_relaxed);
        peakMeterR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    SignalType type = static_cast<SignalType>(signalType.load(std::memory_order_relaxed));
    SignalChannel chanMode = static_cast<SignalChannel>(channelMode.load(std::memory_order_relaxed));

    // Smooth frequency adjustments
    double phaseDeltaStep = (targetPhaseDelta - currentPhaseDelta) / (double)numSamples;

    float blockPeakL = 0.0f;
    float blockPeakR = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // Smooth gain ramp
        if (currentGain < targetGain)
            currentGain = juce::jmin(targetGain, currentGain + gainRampPerSample);
        else if (currentGain > targetGain)
            currentGain = juce::jmax(targetGain, currentGain - gainRampPerSample);

        currentPhaseDelta += phaseDeltaStep;

        float rawSample = 0.0f;

        switch (type)
        {
            case SignalType::Sine:
            {
                rawSample = (float)std::sin(currentPhase);
                currentPhase += currentPhaseDelta;
                if (currentPhase >= juce::MathConstants<double>::twoPi)
                    currentPhase -= juce::MathConstants<double>::twoPi;
                break;
            }

            case SignalType::WhiteNoise:
            {
                rawSample = generateWhiteNoise();
                break;
            }

            case SignalType::PinkNoise:
            {
                rawSample = generatePinkNoise();
                break;
            }

            case SignalType::Impulse:
            {
                // Generates a periodic Dirac impulse every 1.0 second (sampleRate samples)
                if (++impulseCounter >= (int)currentSampleRate)
                {
                    impulseCounter = 0;
                    rawSample = 1.0f; // Impulse peak
                }
                else
                {
                    rawSample = 0.0f;
                }
                break;
            }
        }

        float outSample = rawSample * currentGain;

        float sampleL = (chanMode == SignalChannel::RightOnly) ? 0.0f : outSample;
        float sampleR = (chanMode == SignalChannel::LeftOnly)  ? 0.0f : outSample;

        // Add to audio channel buffers
        if (channelData[0] != nullptr) channelData[0][s] += sampleL;
        if (numChannels > 1 && channelData[1] != nullptr) channelData[1][s] += sampleR;

        // Metering tracking
        float absL = std::abs(sampleL);
        float absR = std::abs(sampleR);
        if (absL > blockPeakL) blockPeakL = absL;
        if (absR > blockPeakR) blockPeakR = absR;

        if (!isCont)
        {
            int r = burstSamplesRemaining.load(std::memory_order_relaxed);
            if (r > 0)
                burstSamplesRemaining.store(r - 1, std::memory_order_relaxed);
            else
                targetGain = 0.0f;
        }
    }

    peakMeterL.store(blockPeakL, std::memory_order_relaxed);
    peakMeterR.store(blockPeakR, std::memory_order_relaxed);
    if (blockPeakL >= 0.999f || blockPeakR >= 0.999f)
        clipDetected.store(true, std::memory_order_relaxed);
}

} // namespace LiveBus
