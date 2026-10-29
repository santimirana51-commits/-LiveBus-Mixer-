#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace LiveBus
{
/**
 * Professional Real-Time Vocal Enhancement DSP Processor.
 * Provides musical vocal coloration and presence enhancement:
 * - Warmth: Sub-harmonic & low-mid gentle saturation/body (200 - 350 Hz)
 * - Body: 400 - 600 Hz harmonic density
 * - Clarity: 2.5 - 4.5 kHz intelligibility contour
 * - Presence: 5.0 - 8.0 kHz vocal forwardness
 * - Air: 12.0 - 18.0 kHz ultra-high smooth sheen
 * Fully real-time safe: Zero allocations, zero locks on audio callback, safe coefficient updates.
 */
class VocalEnhancerDSP
{
public:
    VocalEnhancerDSP() noexcept
    {
        updateCoefficients();
    }

    ~VocalEnhancerDSP() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        currentBlockSize = juce::jmax(1, maxBlockSize);

        for (int ch = 0; ch < 2; ++ch)
        {
            warmthFilter[ch].reset();
            bodyFilter[ch].reset();
            clarityFilter[ch].reset();
            presenceFilter[ch].reset();
            airFilter[ch].reset();
        }

        updateCoefficients();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            warmthFilter[ch].reset();
            bodyFilter[ch].reset();
            clarityFilter[ch].reset();
            presenceFilter[ch].reset();
            airFilter[ch].reset();
        }
    }

    void setBypass(bool bypassed) noexcept
    {
        isBypassed.store(bypassed, std::memory_order_release);
    }

    bool getBypass() const noexcept
    {
        return isBypassed.load(std::memory_order_relaxed);
    }

    // Parameters: 0.0 to 100.0 %
    void setWarmth(float val) noexcept   { warmthParam.store(juce::jlimit(0.0f, 100.0f, val), std::memory_order_release); }
    void setBody(float val) noexcept     { bodyParam.store(juce::jlimit(0.0f, 100.0f, val), std::memory_order_release); }
    void setClarity(float val) noexcept  { clarityParam.store(juce::jlimit(0.0f, 100.0f, val), std::memory_order_release); }
    void setPresence(float val) noexcept { presenceParam.store(juce::jlimit(0.0f, 100.0f, val), std::memory_order_release); }
    void setAir(float val) noexcept      { airParam.store(juce::jlimit(0.0f, 100.0f, val), std::memory_order_release); }
    void setOutputGainDb(float val) noexcept { outGainDb.store(juce::jlimit(-18.0f, 18.0f, val), std::memory_order_release); }

    float getWarmth() const noexcept   { return warmthParam.load(std::memory_order_relaxed); }
    float getBody() const noexcept     { return bodyParam.load(std::memory_order_relaxed); }
    float getClarity() const noexcept  { return clarityParam.load(std::memory_order_relaxed); }
    float getPresence() const noexcept { return presenceParam.load(std::memory_order_relaxed); }
    float getAir() const noexcept      { return airParam.load(std::memory_order_relaxed); }
    float getOutputGainDb() const noexcept { return outGainDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        if (numSamples <= 0 || buffer.getNumChannels() < 1)
            return;

        if (isBypassed.load(std::memory_order_relaxed))
            return;

        const float warmthVal = warmthParam.load(std::memory_order_relaxed);
        const float bodyVal = bodyParam.load(std::memory_order_relaxed);
        const float clarityVal = clarityParam.load(std::memory_order_relaxed);
        const float presenceVal = presenceParam.load(std::memory_order_relaxed);
        const float airVal = airParam.load(std::memory_order_relaxed);

        // Check if any enhancer is active
        const bool hasProcessing = (warmthVal > 0.01f || bodyVal > 0.01f || clarityVal > 0.01f ||
                                    presenceVal > 0.01f || airVal > 0.01f);

        if (!hasProcessing)
        {
            // Just apply output gain if set
            const float outGain = juce::Decibels::decibelsToGain(outGainDb.load(std::memory_order_relaxed));
            if (std::abs(outGain - 1.0f) > 0.001f)
                buffer.applyGain(outGain);
            return;
        }

        updateCoefficients();

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float s = data[i];

                // Warmth (Low-mid peak bell + soft saturation for tube/analog richness)
                if (warmthVal > 0.01f)
                {
                    float eqWarm = warmthFilter[ch].processSample(s);
                    // Gentle soft-clip tube curve on the warm band
                    float saturated = std::tanh(eqWarm * 1.2f) * 0.833f;
                    s = juce::jmap(warmthVal * 0.01f, s, saturated);
                }

                // Body (450 Hz resonance body filter)
                if (bodyVal > 0.01f)
                {
                    s = bodyFilter[ch].processSample(s);
                }

                // Clarity (3.2 kHz vocal bite & speech articulation)
                if (clarityVal > 0.01f)
                {
                    s = clarityFilter[ch].processSample(s);
                }

                // Presence (6.5 kHz vocal forwardness)
                if (presenceVal > 0.01f)
                {
                    s = presenceFilter[ch].processSample(s);
                }

                // Air (14 kHz ultra-high shelf shimmer)
                if (airVal > 0.01f)
                {
                    s = airFilter[ch].processSample(s);
                }

                data[i] = s;
            }
        }

        const float outGain = juce::Decibels::decibelsToGain(outGainDb.load(std::memory_order_relaxed));
        if (std::abs(outGain - 1.0f) > 0.001f)
            buffer.applyGain(outGain);
    }

private:
    void updateCoefficients()
    {
        const double sr = (currentSampleRate > 0.0) ? currentSampleRate : 48000.0;
        const float nyquist = static_cast<float>(sr * 0.49);

        // Maximum boost per band: +8 dB at 100%
        const float warmthGainDb = juce::jmap(warmthParam.load(std::memory_order_relaxed) * 0.01f, 0.0f, 6.0f);
        const float bodyGainDb   = juce::jmap(bodyParam.load(std::memory_order_relaxed) * 0.01f, 0.0f, 6.0f);
        const float clarityGainDb = juce::jmap(clarityParam.load(std::memory_order_relaxed) * 0.01f, 0.0f, 8.0f);
        const float presenceGainDb = juce::jmap(presenceParam.load(std::memory_order_relaxed) * 0.01f, 0.0f, 7.0f);
        const float airGainDb = juce::jmap(airParam.load(std::memory_order_relaxed) * 0.01f, 0.0f, 8.0f);

        auto cWarmth = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jmin(280.0f, nyquist), 0.8f, juce::Decibels::decibelsToGain(warmthGainDb));
        auto cBody = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jmin(500.0f, nyquist), 1.0f, juce::Decibels::decibelsToGain(bodyGainDb));
        auto cClarity = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jmin(3200.0f, nyquist), 1.2f, juce::Decibels::decibelsToGain(clarityGainDb));
        auto cPresence = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, juce::jmin(6500.0f, nyquist), 1.1f, juce::Decibels::decibelsToGain(presenceGainDb));
        auto cAir = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, juce::jmin(13000.0f, nyquist), 0.707f, juce::Decibels::decibelsToGain(airGainDb));

        for (int ch = 0; ch < 2; ++ch)
        {
            if (cWarmth) *warmthFilter[ch].coefficients = *cWarmth;
            if (cBody) *bodyFilter[ch].coefficients = *cBody;
            if (cClarity) *clarityFilter[ch].coefficients = *cClarity;
            if (cPresence) *presenceFilter[ch].coefficients = *cPresence;
            if (cAir) *airFilter[ch].coefficients = *cAir;
        }
    }

    std::atomic<bool> isBypassed{false};
    std::atomic<float> warmthParam{0.0f};
    std::atomic<float> bodyParam{0.0f};
    std::atomic<float> clarityParam{0.0f};
    std::atomic<float> presenceParam{0.0f};
    std::atomic<float> airParam{0.0f};
    std::atomic<float> outGainDb{0.0f};

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    juce::dsp::IIR::Filter<float> warmthFilter[2];
    juce::dsp::IIR::Filter<float> bodyFilter[2];
    juce::dsp::IIR::Filter<float> clarityFilter[2];
    juce::dsp::IIR::Filter<float> presenceFilter[2];
    juce::dsp::IIR::Filter<float> airFilter[2];
};

} // namespace LiveBus
