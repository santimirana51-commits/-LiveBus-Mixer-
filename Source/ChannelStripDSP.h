#pragma once
#include <JuceHeader.h>
#include "EightBandEQ.h"
#include "VocalDSPProcessor.h"
#include <array>
#include <atomic>

static constexpr int numPuzuBands = 15;

inline const float defaultPuzuFreqs[15] = {
    25.3f, 40.1f, 63.7f, 101.0f, 184.0f, 260.0f, 405.0f, 764.0f,
    1000.0f, 1620.0f, 2520.0f, 4000.0f, 5140.0f, 10100.0f, 16000.0f
};

inline const float defaultPuzuQs[15] = {
    1.19f, 1.19f, 1.19f, 1.19f, 3.00f, 1.19f, 1.19f, 2.00f,
    1.19f, 1.19f, 1.19f, 1.19f, 2.70f, 1.19f, 1.19f
};

struct PuzuBandParam
{
    std::atomic<float> freq{1000.0f};
    std::atomic<float> q{1.19f};
    std::atomic<float> gainDb{0.0f};
    std::atomic<bool> enabled{true};

    PuzuBandParam() = default;
    PuzuBandParam(float f, float qVal, float g = 0.0f)
        : freq(f), q(qVal), gainDb(g), enabled(true) {}
};

struct BusDSPParameters
{
    // Master enables
    std::atomic<bool> hpfEnabled{false};
    std::atomic<bool> eqEnabled{false};
    std::atomic<bool> gateEnabled{false};
    std::atomic<bool> compEnabled{false};
    std::atomic<bool> limiterEnabled{false};
    std::atomic<bool> reverbEnabled{false};
    std::atomic<bool> delayEnabled{false};

    // HPF (High-Pass / Low-Cut)
    std::atomic<float> hpfFreq{80.0f};           // 20.0 to 1000.0 Hz
    std::atomic<int> hpfSlope{1};                // 0=6dB, 1=12dB, 2=18dB, 3=24dB/Oct
    std::atomic<int> hpfType{0};                 // 0=Butterworth, 1=Bessel, 2=Linkwitz-Riley

    // LPF (Low-Pass / High-Cut)
    std::atomic<bool> lpfEnabled{false};
    std::atomic<float> lpfFreq{20600.0f};
    std::atomic<int> lpfSlope{0}; // 0=OFF, 1=6dB, 2=12dB, 3=18dB, 4=24dB/Oct
    std::atomic<int> lpfType{0};

    // 15-Band Studio Parametric Equalizer (PUZU Style)
    std::array<PuzuBandParam, 15> puzuBands;

    // Legacy 4-Band Studio Parametric Equalizer compatibility fields
    std::atomic<float> eqLowFreq{100.0f};
    std::atomic<float> eqLowGainDb{0.0f};
    std::atomic<float> eqMid1Freq{500.0f};
    std::atomic<float> eqMid1GainDb{0.0f};
    std::atomic<float> eqMid1Q{0.707f};
    std::atomic<float> eqFreq{2500.0f};
    std::atomic<float> eqQ{0.707f};
    std::atomic<float> eqGainDb{0.0f};
    std::atomic<float> eqHighFreq{10000.0f};
    std::atomic<float> eqHighGainDb{0.0f};

    // Noise Gate
    std::atomic<float> gateThresholdDb{-45.0f};  // -80.0 to 0.0 dB
    std::atomic<float> gateRatio{4.0f};          // 1.0 to 20.0 (expander downward slope)
    std::atomic<float> gateAttackMs{2.0f};       // 0.1 to 100.0 ms
    std::atomic<float> gateHoldMs{20.0f};        // 0.0 to 500.0 ms
    std::atomic<float> gateReleaseMs{100.0f};    // 10.0 to 1000.0 ms
    std::atomic<float> gateFloorDb{-60.0f};      // -80.0 to 0.0 dB (-80 is mute)
    std::atomic<float> gateHysteresisDb{4.0f};   // 1.0 to 12.0 dB

    // Compressor
    std::atomic<float> compThresholdDb{-18.0f};  // -60.0 to 0.0 dB
    std::atomic<float> compRatio{3.0f};          // 1.0 to 20.0
    std::atomic<float> compAttackMs{10.0f};      // 0.1 to 100.0 ms
    std::atomic<float> compReleaseMs{100.0f};    // 10.0 to 1000.0 ms
    std::atomic<int> compKnee{1};                // 0=Soft(10dB), 1=Normal(5dB), 2=Hard(0dB)
    std::atomic<float> compMakeupDb{0.0f};       // -12.0 to +24.0 dB
    std::atomic<float> compMixPercent{100.0f};   // 0.0 to 100.0% (parallel compression)

    // Limiter (Brickwall peak ceiling)
    std::atomic<float> limiterCeilingDb{-1.0f};  // -20.0 to 0.0 dBFS
    std::atomic<float> limiterDbu{-30.0f};
    std::atomic<float> limiterAttackMs{0.3f};
    std::atomic<float> limiterReleaseMs{60.0f};  // 1.0 to 500.0 ms

    // Time Alignment Delay & Phase
    std::atomic<float> timeDelayMs{0.0f};
    std::atomic<float> timeDelayDistanceCm{0.0f};
    std::atomic<bool> phaseInverted{false};

    // Reverb
    std::atomic<float> reverbRoomSize{0.5f};     // 0.0 to 1.0
    std::atomic<float> reverbDamping{0.5f};      // 0.0 to 1.0
    std::atomic<float> reverbWetLevel{0.33f};    // 0.0 to 1.0
    std::atomic<float> reverbDryLevel{1.0f};     // 0.0 to 1.0
    std::atomic<float> reverbWidth{1.0f};        // 0.0 to 1.0

    // Creative Delay
    std::atomic<float> delayTimeMs{350.0f};      // 10.0 to 1500.0 ms
    std::atomic<float> delayFeedback{0.35f};     // 0.0 to 0.90 (clamped <= 0.90)
    std::atomic<float> delayWetLevel{0.25f};     // 0.0 to 1.0
    std::atomic<float> delayDryLevel{1.0f};      // 0.0 to 1.0
    std::atomic<bool> delayPingPong{false};      // Mono vs Ping-Pong Stereo

    // Master bus gain & pan
    std::atomic<float> masterGain{1.0f};
    std::atomic<float> masterPan{0.0f};          // -1.0 to +1.0
    std::atomic<bool> masterMute{false};

    BusDSPParameters()
    {
        for (int i = 0; i < 15; ++i)
        {
            puzuBands[i].freq.store(defaultPuzuFreqs[i]);
            puzuBands[i].q.store(defaultPuzuQs[i]);
            puzuBands[i].gainDb.store(0.0f);
            puzuBands[i].enabled.store(true);
        }
    }
};

class BusDSPProcessor
{
public:
    BusDSPProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        currentBlockSize = juce::jmax(1, maxBlockSize);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate;
        spec.maximumBlockSize = (juce::uint32)currentBlockSize;
        spec.numChannels = 2;

        for (int ch = 0; ch < 2; ++ch)
        {
            hpfFilterStage1[ch].reset();
            hpfFilterStage2[ch].reset();
            lpfFilterStage1[ch].reset();
            lpfFilterStage2[ch].reset();

            for (int b = 0; b < numPuzuBands; ++b)
                puzuBandFilters[ch][b].reset();

            legacyEqLowFilter[ch].reset();
            legacyEqMid1Filter[ch].reset();
            legacyEqMid2Filter[ch].reset();
            legacyEqHighFilter[ch].reset();
        }

        reverb.prepare(spec);

        // Prepare delay buffer (max 2 seconds)
        int maxDelaySamples = (int)(currentSampleRate * 2.0);
        delayBuffer.setSize(2, maxDelaySamples);
        delayBuffer.clear();
        delayWritePos = 0;
        currentDelaySamples = params.delayTimeMs.load(std::memory_order_relaxed) * 0.001f * (float)currentSampleRate;

        // Time alignment buffer (max 100ms)
        int maxAlignSamples = (int)(currentSampleRate * 0.1);
        timeAlignBuffer.setSize(2, juce::jmax(16, maxAlignSamples));
        timeAlignBuffer.clear();
        timeAlignWritePos = 0;

        smoothedMasterGain.reset(currentSampleRate, 0.02); // 20ms ramp
        smoothedMasterGain.setCurrentAndTargetValue(params.masterGain.load(std::memory_order_relaxed));

        resetInternalEnvelopes();
        updateCoefficients();

        eightBandEQ.prepare(currentSampleRate, currentBlockSize);
        vocalRack.setEQ(&eightBandEQ);
        vocalRack.prepare(currentSampleRate, currentBlockSize);
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hpfFilterStage1[ch].reset();
            hpfFilterStage2[ch].reset();
            lpfFilterStage1[ch].reset();
            lpfFilterStage2[ch].reset();

            for (int b = 0; b < numPuzuBands; ++b)
                puzuBandFilters[ch][b].reset();

            legacyEqLowFilter[ch].reset();
            legacyEqMid1Filter[ch].reset();
            legacyEqMid2Filter[ch].reset();
            legacyEqHighFilter[ch].reset();
        }
        reverb.reset();
        delayBuffer.clear();
        timeAlignBuffer.clear();
        resetInternalEnvelopes();
        eightBandEQ.reset();
        vocalRack.reset();

        peakL.store(0.0f);
        peakR.store(0.0f);
        clipping.store(false);
    }

    void updateCoefficients()
    {
        // Clamp delay feedback to 0.90 to strictly prevent runaway oscillation
        if (params.delayFeedback.load(std::memory_order_relaxed) > 0.90f)
            params.delayFeedback.store(0.90f, std::memory_order_relaxed);

        // 1. High Pass Filter (up to 24dB/Oct)
        float hpfF = juce::jlimit(20.0f, (float)(currentSampleRate * 0.49), params.hpfFreq.load(std::memory_order_relaxed));
        int hpfSlope = params.hpfSlope.load(std::memory_order_relaxed);
        auto hpfCoeff1 = (hpfSlope == 0)
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, hpfF)
            : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfF, (hpfSlope >= 3) ? 0.5412f : 0.7071f);
        auto hpfCoeff2 = (hpfSlope == 2)
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, hpfF)
            : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfF, 1.3065f);

        // 2. Low Pass Filter (up to 24dB/Oct)
        float lpfF = juce::jlimit(1000.0f, 22000.0f, params.lpfFreq.load(std::memory_order_relaxed));
        int lpfSlope = params.lpfSlope.load(std::memory_order_relaxed);
        auto lpfCoeff1 = (lpfSlope <= 1)
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(currentSampleRate, lpfF)
            : juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, lpfF, 0.707f);
        auto lpfCoeff2 = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, lpfF, (lpfSlope >= 4) ? 1.3065f : 0.707f);

        // 3. 15-Band PUZU Parametric Equalizer
        std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>, numPuzuBands> puzuCoeffs;
        for (int b = 0; b < numPuzuBands; ++b)
        {
            float f = juce::jlimit(20.0f, 20000.0f, params.puzuBands[b].freq.load(std::memory_order_relaxed));
            float q = juce::jlimit(0.1f, 10.0f, params.puzuBands[b].q.load(std::memory_order_relaxed));
            float g = juce::Decibels::decibelsToGain(params.puzuBands[b].gainDb.load(std::memory_order_relaxed));
            puzuCoeffs[b] = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, f, q, g);
        }

        // 4. Legacy 4-Band EQ (Low Shelf, Mid 1, Mid 2, High Shelf)
        float lowF = juce::jlimit(20.0f, 800.0f, params.eqLowFreq.load(std::memory_order_relaxed));
        float lowG = juce::Decibels::decibelsToGain(params.eqLowGainDb.load(std::memory_order_relaxed));
        auto lowCoeff = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, lowF, 0.707f, lowG);

        float mid1F = juce::jlimit(100.0f, 5000.0f, params.eqMid1Freq.load(std::memory_order_relaxed));
        float mid1Q = juce::jlimit(0.1f, 10.0f, params.eqMid1Q.load(std::memory_order_relaxed));
        float mid1G = juce::Decibels::decibelsToGain(params.eqMid1GainDb.load(std::memory_order_relaxed));
        auto mid1Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, mid1F, mid1Q, mid1G);

        float mid2F = juce::jlimit(200.0f, 16000.0f, params.eqFreq.load(std::memory_order_relaxed));
        float mid2Q = juce::jlimit(0.1f, 10.0f, params.eqQ.load(std::memory_order_relaxed));
        float mid2G = juce::Decibels::decibelsToGain(params.eqGainDb.load(std::memory_order_relaxed));
        auto mid2Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, mid2F, mid2Q, mid2G);

        float highF = juce::jlimit(2000.0f, 20000.0f, params.eqHighFreq.load(std::memory_order_relaxed));
        float highG = juce::Decibels::decibelsToGain(params.eqHighGainDb.load(std::memory_order_relaxed));
        auto highCoeff = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, highF, 0.707f, highG);

        for (int ch = 0; ch < 2; ++ch)
        {
            *hpfFilterStage1[ch].coefficients = *hpfCoeff1;
            *hpfFilterStage2[ch].coefficients = *hpfCoeff2;

            *lpfFilterStage1[ch].coefficients = *lpfCoeff1;
            *lpfFilterStage2[ch].coefficients = *lpfCoeff2;

            for (int b = 0; b < numPuzuBands; ++b)
                *puzuBandFilters[ch][b].coefficients = *puzuCoeffs[b];

            *legacyEqLowFilter[ch].coefficients = *lowCoeff;
            *legacyEqMid1Filter[ch].coefficients = *mid1Coeff;
            *legacyEqMid2Filter[ch].coefficients = *mid2Coeff;
            *legacyEqHighFilter[ch].coefficients = *highCoeff;
        }

        lastHpfFreq = hpfF;
        lastHpfSlope = hpfSlope;

        // Reverb Parameters
        juce::dsp::Reverb::Parameters rParams;
        rParams.roomSize = juce::jlimit(0.0f, 1.0f, params.reverbRoomSize.load(std::memory_order_relaxed));
        rParams.damping = juce::jlimit(0.0f, 1.0f, params.reverbDamping.load(std::memory_order_relaxed));
        rParams.wetLevel = juce::jlimit(0.0f, 1.0f, params.reverbWetLevel.load(std::memory_order_relaxed));
        rParams.dryLevel = juce::jlimit(0.0f, 1.0f, params.reverbDryLevel.load(std::memory_order_relaxed));
        rParams.width = juce::jlimit(0.0f, 1.0f, params.reverbWidth.load(std::memory_order_relaxed));
        rParams.freezeMode = 0.0f;
        reverb.setParameters(rParams);
    }

    float getMagnitudeForFrequency(float frequency) const
    {
        float mag = 1.0f;

        // High Pass Filter magnitude
        if (params.hpfEnabled.load(std::memory_order_relaxed))
        {
            if (hpfFilterStage1[0].coefficients)
                mag *= (float)hpfFilterStage1[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (params.hpfSlope.load(std::memory_order_relaxed) >= 2 && hpfFilterStage2[0].coefficients)
                mag *= (float)hpfFilterStage2[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
        }

        // Low Pass Filter magnitude
        if (params.lpfEnabled.load(std::memory_order_relaxed) && params.lpfSlope.load(std::memory_order_relaxed) > 0)
        {
            if (lpfFilterStage1[0].coefficients)
                mag *= (float)lpfFilterStage1[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (params.lpfSlope.load(std::memory_order_relaxed) >= 3 && lpfFilterStage2[0].coefficients)
                mag *= (float)lpfFilterStage2[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
        }

        // 15-Band Parametric Equalizer magnitude
        if (params.eqEnabled.load(std::memory_order_relaxed))
        {
            for (int b = 0; b < numPuzuBands; ++b)
            {
                if (params.puzuBands[b].enabled.load(std::memory_order_relaxed) && puzuBandFilters[0][b].coefficients)
                    mag *= (float)puzuBandFilters[0][b].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            }

            // Legacy 4-band components
            if (legacyEqLowFilter[0].coefficients)
                mag *= (float)legacyEqLowFilter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (legacyEqMid1Filter[0].coefficients)
                mag *= (float)legacyEqMid1Filter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (legacyEqMid2Filter[0].coefficients)
                mag *= (float)legacyEqMid2Filter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (legacyEqHighFilter[0].coefficients)
                mag *= (float)legacyEqHighFilter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
        }

        return juce::Decibels::gainToDecibels(mag, -40.0f);
    }

    void process(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        juce::ScopedNoDenormals noDenormals;

        if (numSamples <= 0 || buffer.getNumChannels() < 1) return;

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // 0. Protect against non-finite values (NaN / Inf)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (!std::isfinite(data[i]))
                    data[i] = 0.0f;
            }
        }

        // Measure input peak level
        float maxIn = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float m = buffer.getMagnitude(ch, 0, numSamples);
            if (m > maxIn) maxIn = m;
        }
        float inDb = (maxIn > 0.00001f) ? juce::Decibels::gainToDecibels(maxIn, -100.0f) : -100.0f;
        inPeakDb.store(inDb, std::memory_order_relaxed);

        // Phase Inversion
        if (params.phaseInverted.load(std::memory_order_relaxed))
        {
            buffer.applyGain(-1.0f);
        }

        // Time Alignment Delay
        float alignMs = params.timeDelayMs.load(std::memory_order_relaxed);
        if (alignMs > 0.01f)
        {
            int alignSamples = juce::jlimit(0, timeAlignBuffer.getNumSamples() - 1, (int)(alignMs * 0.001f * currentSampleRate));
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* in = buffer.getReadPointer(ch);
                float* out = buffer.getWritePointer(ch);
                float* delayData = timeAlignBuffer.getWritePointer(ch);
                int bufSize = timeAlignBuffer.getNumSamples();

                for (int s = 0; s < numSamples; ++s)
                {
                    int readPos = timeAlignWritePos - alignSamples + s;
                    while (readPos < 0) readPos += bufSize;
                    while (readPos >= bufSize) readPos -= bufSize;

                    int writePos = (timeAlignWritePos + s) % bufSize;
                    delayData[writePos] = in[s];
                    out[s] = delayData[readPos];
                }
            }
            timeAlignWritePos = (timeAlignWritePos + numSamples) % timeAlignBuffer.getNumSamples();
        }

        // Check if Waves eMo D5 hardware vocal rack is explicitly enabled
        if (vocalRack.params.masterEnabled.load(std::memory_order_relaxed))
        {
            vocalRack.process(buffer, numSamples);
            gateGR.store(vocalRack.getGateGainReductionDb(), std::memory_order_relaxed);
            compGR.store(vocalRack.getCompGainReductionDb(), std::memory_order_relaxed);
            limiterGR.store(vocalRack.getLimiterGainReductionDb(), std::memory_order_relaxed);
        }
        else
        {
            // Standard LiveBus Mixer 7-Stage DSP Signal Chain:
            // 1. High-Pass Filter (HPF)
            if (params.hpfEnabled.load(std::memory_order_relaxed))
            {
                processHPF(buffer, numSamples);
            }

            // 2. Noise Gate / Expander
            if (params.gateEnabled.load(std::memory_order_relaxed))
            {
                processGate(buffer, numSamples);
            }
            else
            {
                gateGR.store(0.0f, std::memory_order_relaxed);
                gateCurrentState.store(2, std::memory_order_relaxed); // Open when disabled
            }

            // 3. 8-Band Studio Parametric Equalizer (and legacy PUZU if enabled)
            if (params.eqEnabled.load(std::memory_order_relaxed))
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    juce::dsp::AudioBlock<float> block(buffer);
                    auto singleBlock = block.getSingleChannelBlock((size_t)ch).getSubBlock(0, (size_t)numSamples);
                    juce::dsp::ProcessContextReplacing<float> context(singleBlock);

                    for (int b = 0; b < numPuzuBands; ++b)
                    {
                        if (params.puzuBands[b].enabled.load(std::memory_order_relaxed))
                            puzuBandFilters[ch][b].process(context);
                    }

                    legacyEqLowFilter[ch].process(context);
                    legacyEqMid1Filter[ch].process(context);
                    legacyEqMid2Filter[ch].process(context);
                    legacyEqHighFilter[ch].process(context);
                }
                if (eightBandEQ.params.masterEnabled.load(std::memory_order_relaxed))
                    eightBandEQ.process(buffer, numSamples);
            }

            // 4. Dynamics Compressor
            if (params.compEnabled.load(std::memory_order_relaxed))
            {
                processCompressor(buffer, numSamples);
            }
            else
            {
                compGR.store(0.0f, std::memory_order_relaxed);
            }

            // 5. Studio Reverb
            if (params.reverbEnabled.load(std::memory_order_relaxed))
            {
                processReverb(buffer, numSamples);
            }

            // 6. Stereo Creative Delay
            if (params.delayEnabled.load(std::memory_order_relaxed))
            {
                processDelay(buffer, numSamples);
            }

            // 7. Brickwall Peak Limiter
            if (params.limiterEnabled.load(std::memory_order_relaxed))
            {
                processLimiter(buffer, numSamples);
            }
            else
            {
                limiterGR.store(0.0f, std::memory_order_relaxed);
            }
        }

        // 8. Master Gain, Pan, and Mute
        if (params.masterMute.load(std::memory_order_relaxed))
        {
            buffer.clear(0, numSamples);
            peakL.store(0.0f, std::memory_order_relaxed);
            peakR.store(0.0f, std::memory_order_relaxed);
            outPeakDb.store(-100.0f, std::memory_order_relaxed);
            clipping.store(false, std::memory_order_relaxed);
            return;
        }

        float targetGain = params.masterGain.load(std::memory_order_relaxed);
        smoothedMasterGain.setTargetValue(targetGain);

        float pan = juce::jlimit(-1.0f, 1.0f, params.masterPan.load(std::memory_order_relaxed));
        float panGainL = 1.0f;
        float panGainR = 1.0f;
        if (std::abs(pan) > 0.0001f)
        {
            float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // -3dB equal power
            panGainL = std::cos(panAngle) * 1.41421356f;
            panGainR = std::sin(panAngle) * 1.41421356f;
        }

        float* left = buffer.getWritePointer(0);
        float* right = (numChannels > 1) ? buffer.getWritePointer(1) : left;

        float maxL = 0.0f;
        float maxR = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float g = smoothedMasterGain.getNextValue();
            left[i] *= (g * panGainL);
            if (numChannels > 1)
                right[i] *= (g * panGainR);

            float absL = std::abs(left[i]);
            float absR = (numChannels > 1) ? std::abs(right[i]) : absL;
            if (absL > maxL) maxL = absL;
            if (absR > maxR) maxR = absR;
        }

        peakL.store(maxL, std::memory_order_relaxed);
        peakR.store(maxR, std::memory_order_relaxed);

        float outMax = juce::jmax(maxL, maxR);
        float outDb = (outMax > 0.00001f) ? juce::Decibels::gainToDecibels(outMax, -100.0f) : -100.0f;
        outPeakDb.store(outDb, std::memory_order_relaxed);

        bool clip = (maxL >= 0.999f || maxR >= 0.999f);
        if (clip) clipping.store(true, std::memory_order_relaxed);
    }

    // Real-time safe telemetry and metrics
    float getGateGainReductionDb() const noexcept { return gateGR.load(std::memory_order_relaxed); }
    float getCompGainReductionDb() const noexcept { return compGR.load(std::memory_order_relaxed); }
    float getLimiterGainReductionDb() const noexcept { return limiterGR.load(std::memory_order_relaxed); }
    int getGateState() const noexcept { return gateCurrentState.load(std::memory_order_relaxed); }
    float getInputPeakDb() const noexcept { return inPeakDb.load(std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept { return outPeakDb.load(std::memory_order_relaxed); }
    float getPeakL() const noexcept { return peakL.load(std::memory_order_relaxed); }
    float getPeakR() const noexcept { return peakR.load(std::memory_order_relaxed); }
    bool isClipping() const noexcept { return clipping.load(std::memory_order_relaxed); }
    void resetClipping() noexcept { clipping.store(false, std::memory_order_relaxed); }

    BusDSPParameters params;
    LiveBus::EightBandEQProcessor eightBandEQ;
    LiveBus::VocalDSPProcessor vocalRack;

private:
    void resetInternalEnvelopes()
    {
        gateDetectorEnv = 0.0f;
        gateCurrentGain = 1.0f;
        gateHoldRemaining = 0;
        gateState = 2; // Open

        compCurrentGain = 1.0f;
        limiterCurrentGain = 1.0f;

        gateGR.store(0.0f, std::memory_order_relaxed);
        compGR.store(0.0f, std::memory_order_relaxed);
        limiterGR.store(0.0f, std::memory_order_relaxed);
        gateCurrentState.store(2, std::memory_order_relaxed);
        inPeakDb.store(-100.0f, std::memory_order_relaxed);
        outPeakDb.store(-100.0f, std::memory_order_relaxed);
    }

    void processHPF(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        float freq = juce::jlimit(20.0f, (float)(currentSampleRate * 0.49), params.hpfFreq.load(std::memory_order_relaxed));
        int slope = params.hpfSlope.load(std::memory_order_relaxed);

        if (std::abs(freq - lastHpfFreq) > 0.5f || slope != lastHpfSlope)
        {
            lastHpfFreq = freq;
            lastHpfSlope = slope;
            auto c1 = (slope == 0)
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, freq, (slope >= 3) ? 0.5412f : 0.7071f);
            auto c2 = (slope == 2)
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, freq, 1.3065f);

            for (int ch = 0; ch < 2; ++ch)
            {
                *hpfFilterStage1[ch].coefficients = *c1;
                *hpfFilterStage2[ch].coefficients = *c2;
            }
        }

        juce::dsp::AudioBlock<float> block(buffer);
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto singleBlock = block.getSingleChannelBlock((size_t)ch).getSubBlock(0, (size_t)numSamples);
            juce::dsp::ProcessContextReplacing<float> context(singleBlock);
            hpfFilterStage1[ch].process(context);
            if (slope >= 2)
                hpfFilterStage2[ch].process(context);
        }
    }

    void processGate(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float threshDb = params.gateThresholdDb.load(std::memory_order_relaxed);
        const float openThreshLinear = juce::Decibels::decibelsToGain(threshDb);
        const float hystDb = juce::jlimit(1.0f, 12.0f, params.gateHysteresisDb.load(std::memory_order_relaxed));
        const float closeThreshLinear = juce::Decibels::decibelsToGain(threshDb - hystDb);
        const float floorDb = params.gateFloorDb.load(std::memory_order_relaxed);
        const float floorLinear = (floorDb <= -79.0f) ? 0.0f : juce::Decibels::decibelsToGain(floorDb);

        const float attMs = juce::jmax(0.1f, params.gateAttackMs.load(std::memory_order_relaxed));
        const float holdMs = juce::jmax(0.0f, params.gateHoldMs.load(std::memory_order_relaxed));
        const float relMs = juce::jmax(1.0f, params.gateReleaseMs.load(std::memory_order_relaxed));

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * attMs * 0.001));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * relMs * 0.001));
        const int holdSamples = (int)(currentSampleRate * holdMs * 0.001);

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        float minGainObserved = 1.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float sampleMax = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float a = std::abs(buffer.getSample(ch, s));
                if (a > sampleMax) sampleMax = a;
            }

            gateDetectorEnv = sampleMax;

            // State Machine: 0 = Closed, 1 = Hold, 2 = Open
            if (gateDetectorEnv >= openThreshLinear)
            {
                gateState = 2; // Open
                gateHoldRemaining = holdSamples;
            }
            else if (gateDetectorEnv < closeThreshLinear)
            {
                if (gateHoldRemaining > 0)
                {
                    gateState = 1; // Hold
                    gateHoldRemaining--;
                }
                else
                {
                    gateState = 0; // Closed
                }
            }

            float targetGain = (gateState == 2 || gateState == 1) ? 1.0f : floorLinear;

            if (targetGain > gateCurrentGain)
                gateCurrentGain += (targetGain - gateCurrentGain) * alphaAtt;
            else
                gateCurrentGain += (targetGain - gateCurrentGain) * alphaRel;

            if (gateCurrentGain < minGainObserved)
                minGainObserved = gateCurrentGain;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.setSample(ch, s, buffer.getSample(ch, s) * gateCurrentGain);
            }
        }

        gateCurrentState.store(gateState, std::memory_order_relaxed);
        float grDb = (gateCurrentGain > 0.00001f) ? -juce::Decibels::gainToDecibels(gateCurrentGain, -80.0f) : 80.0f;
        gateGR.store(juce::jmax(0.0f, grDb), std::memory_order_relaxed);
    }

    void processCompressor(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float threshDb = params.compThresholdDb.load(std::memory_order_relaxed);
        const float ratio = juce::jmax(1.0f, params.compRatio.load(std::memory_order_relaxed));
        const float attMs = juce::jmax(0.1f, params.compAttackMs.load(std::memory_order_relaxed));
        const float relMs = juce::jmax(1.0f, params.compReleaseMs.load(std::memory_order_relaxed));
        const int kneeType = params.compKnee.load(std::memory_order_relaxed);
        const float kneeWidthDb = (kneeType == 0) ? 10.0f : (kneeType == 1) ? 5.0f : 0.0f;
        const float makeupLinear = juce::Decibels::decibelsToGain(params.compMakeupDb.load(std::memory_order_relaxed));
        const float mixPercent = juce::jlimit(0.0f, 100.0f, params.compMixPercent.load(std::memory_order_relaxed)) * 0.01f;

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * attMs * 0.001));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * relMs * 0.001));

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        float maxGRDb = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float samplePeak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float a = std::abs(buffer.getSample(ch, s));
                if (a > samplePeak) samplePeak = a;
            }

            float inDb = (samplePeak > 0.00001f) ? juce::Decibels::gainToDecibels(samplePeak, -90.0f) : -90.0f;

            float targetGRDb = 0.0f;
            if (kneeWidthDb > 0.01f)
            {
                float halfKnee = kneeWidthDb * 0.5f;
                if (inDb <= threshDb - halfKnee)
                {
                    targetGRDb = 0.0f;
                }
                else if (inDb >= threshDb + halfKnee)
                {
                    targetGRDb = (inDb - threshDb) * (1.0f - 1.0f / ratio);
                }
                else
                {
                    float delta = inDb - threshDb + halfKnee;
                    float quad = (delta * delta) / (2.0f * kneeWidthDb);
                    targetGRDb = quad * (1.0f - 1.0f / ratio);
                }
            }
            else
            {
                if (inDb > threshDb)
                    targetGRDb = (inDb - threshDb) * (1.0f - 1.0f / ratio);
            }

            float targetGainLinear = juce::Decibels::decibelsToGain(-targetGRDb);

            if (targetGainLinear < compCurrentGain)
                compCurrentGain += (targetGainLinear - compCurrentGain) * alphaAtt;
            else
                compCurrentGain += (targetGainLinear - compCurrentGain) * alphaRel;

            float currentGRDb = -juce::Decibels::gainToDecibels(compCurrentGain, -90.0f);
            if (currentGRDb > maxGRDb) maxGRDb = currentGRDb;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float dry = buffer.getSample(ch, s);
                float wet = dry * compCurrentGain * makeupLinear;
                buffer.setSample(ch, s, dry * (1.0f - mixPercent) + wet * mixPercent);
            }
        }

        compGR.store(juce::jmax(0.0f, maxGRDb), std::memory_order_relaxed);
    }

    void processReverb(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        juce::dsp::Reverb::Parameters rParams;
        rParams.roomSize = juce::jlimit(0.0f, 1.0f, params.reverbRoomSize.load(std::memory_order_relaxed));
        rParams.damping = juce::jlimit(0.0f, 1.0f, params.reverbDamping.load(std::memory_order_relaxed));
        rParams.wetLevel = juce::jlimit(0.0f, 1.0f, params.reverbWetLevel.load(std::memory_order_relaxed));
        rParams.dryLevel = juce::jlimit(0.0f, 1.0f, params.reverbDryLevel.load(std::memory_order_relaxed));
        rParams.width = juce::jlimit(0.0f, 1.0f, params.reverbWidth.load(std::memory_order_relaxed));
        rParams.freezeMode = 0.0f;
        reverb.setParameters(rParams);

        juce::dsp::AudioBlock<float> block(buffer);
        auto subBlock = block.getSubBlock(0, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> context(subBlock);
        reverb.process(context);
    }

    void processDelay(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float delayMs = juce::jlimit(10.0f, 1500.0f, params.delayTimeMs.load(std::memory_order_relaxed));
        const float feedback = juce::jlimit(0.0f, 0.90f, params.delayFeedback.load(std::memory_order_relaxed));
        const float wet = juce::jlimit(0.0f, 1.0f, params.delayWetLevel.load(std::memory_order_relaxed));
        const float dry = juce::jlimit(0.0f, 1.0f, params.delayDryLevel.load(std::memory_order_relaxed));
        const bool pingPong = params.delayPingPong.load(std::memory_order_relaxed);

        const int bufSize = delayBuffer.getNumSamples();
        if (bufSize <= 0) return;

        targetDelaySamples = delayMs * 0.001f * (float)currentSampleRate;
        const float alphaSmooth = 0.005f;

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        for (int s = 0; s < numSamples; ++s)
        {
            currentDelaySamples += (targetDelaySamples - currentDelaySamples) * alphaSmooth;
            float dSamples = juce::jlimit(1.0f, (float)(bufSize - 2), currentDelaySamples);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float readIndex = (float)delayWritePos - dSamples;
                while (readIndex < 0.0f) readIndex += (float)bufSize;
                while (readIndex >= (float)bufSize) readIndex -= (float)bufSize;

                int idx0 = (int)readIndex;
                int idx1 = (idx0 + 1) % bufSize;
                float frac = readIndex - (float)idx0;

                const float* dData = delayBuffer.getReadPointer(ch);
                float delayedSample = dData[idx0] * (1.0f - frac) + dData[idx1] * frac;

                float inSample = buffer.getSample(ch, s);

                int writeCh = (pingPong && numChannels == 2) ? (1 - ch) : ch;
                float* dWriteData = delayBuffer.getWritePointer(writeCh);
                dWriteData[delayWritePos] = inSample + (delayedSample * feedback);

                buffer.setSample(ch, s, inSample * dry + delayedSample * wet);
            }

            delayWritePos = (delayWritePos + 1) % bufSize;
        }
    }

    void processLimiter(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float ceilingDb = juce::jlimit(-20.0f, 0.0f, params.limiterCeilingDb.load(std::memory_order_relaxed));
        const float ceilingLinear = juce::Decibels::decibelsToGain(ceilingDb);
        const float relMs = juce::jmax(1.0f, params.limiterReleaseMs.load(std::memory_order_relaxed));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * relMs * 0.001));

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        float maxGRDb = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float a = std::abs(buffer.getSample(ch, s));
                if (a > peak) peak = a;
            }

            float targetGain = 1.0f;
            if (peak > ceilingLinear)
                targetGain = ceilingLinear / (peak + 0.00001f);

            if (targetGain < limiterCurrentGain)
                limiterCurrentGain = targetGain;
            else
                limiterCurrentGain += (targetGain - limiterCurrentGain) * alphaRel;

            float grDb = -juce::Decibels::gainToDecibels(limiterCurrentGain, -60.0f);
            if (grDb > maxGRDb) maxGRDb = grDb;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float outVal = buffer.getSample(ch, s) * limiterCurrentGain;
                if (outVal > ceilingLinear) outVal = ceilingLinear;
                if (outVal < -ceilingLinear) outVal = -ceilingLinear;
                buffer.setSample(ch, s, outVal);
            }
        }

        limiterGR.store(juce::jmax(0.0f, maxGRDb), std::memory_order_relaxed);
    }

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    float lastHpfFreq = -1.0f;
    int lastHpfSlope = -1;

    juce::dsp::IIR::Filter<float> hpfFilterStage1[2];
    juce::dsp::IIR::Filter<float> hpfFilterStage2[2];
    juce::dsp::IIR::Filter<float> lpfFilterStage1[2];
    juce::dsp::IIR::Filter<float> lpfFilterStage2[2];

    juce::dsp::IIR::Filter<float> puzuBandFilters[2][numPuzuBands];

    juce::dsp::IIR::Filter<float> legacyEqLowFilter[2];
    juce::dsp::IIR::Filter<float> legacyEqMid1Filter[2];
    juce::dsp::IIR::Filter<float> legacyEqMid2Filter[2];
    juce::dsp::IIR::Filter<float> legacyEqHighFilter[2];

    // Real envelope states
    float gateDetectorEnv = 0.0f;
    float gateCurrentGain = 1.0f;
    int gateHoldRemaining = 0;
    int gateState = 2; // 0=Closed, 1=Hold, 2=Open

    float compCurrentGain = 1.0f;
    float limiterCurrentGain = 1.0f;

    juce::dsp::Reverb reverb;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    float currentDelaySamples = 0.0f;
    float targetDelaySamples = 0.0f;

    juce::AudioBuffer<float> timeAlignBuffer;
    int timeAlignWritePos = 0;

    juce::SmoothedValue<float> smoothedMasterGain;
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};
    std::atomic<bool> clipping{false};

    // Real-time telemetry atomics
    std::atomic<float> gateGR{0.0f};
    std::atomic<float> compGR{0.0f};
    std::atomic<float> limiterGR{0.0f};
    std::atomic<int> gateCurrentState{2};
    std::atomic<float> inPeakDb{-100.0f};
    std::atomic<float> outPeakDb{-100.0f};
};
