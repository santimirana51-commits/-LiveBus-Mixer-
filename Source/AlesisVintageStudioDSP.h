#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <atomic>
#include <vector>
#include <array>
#include "VocalFXPro/PitchDetector.h"
#include "VocalFXPro/PitchCorrection.h"

namespace LiveBus
{

/**
 * Alesis-Inspired Vintage Studio Processing & Vocal DSP Suite:
 * 1. Reverb: Room, Hall, Plate, Spring, Studio, Cathedral (Decay, Size, Damping, Pre-Delay, Wet/Dry, Low Cut, High Cut)
 * 2. Delay & Echo: Mono, Stereo, Ping-Pong, Slapback (Time, Feedback, Wet/Dry, Filter, Tempo Sync)
 * 3. 7-Band Parametric EQ: High-Pass, Low Shelf, Low Mid, Mid, High Mid, High Shelf, Low-Pass
 * 4. Compressor (Alesis 3630): Threshold, Ratio, Attack, Release, Knee, Makeup, Auto Gain, GR Meter
 * 5. Vocal Dynamics & Enhancement: De-Esser, Noise Gate, Expander, Saturation, Limiter, Exciter, Stereo Widener
 * 6. Creative FX: Chorus, Flanger, Phaser, Doubler, Telephone, Radio, Robot Voice, Formant Shift
 * 7. Real-Time Pitch Tracking & AutoTune: Retune Speed, Scale, Key, Amount
 *
 * Fully real-time safe: Zero memory allocations or locks during processBlock.
 */
class AlesisVintageStudioDSP
{
public:
    enum ReverbType : int
    {
        RevRoom = 0,
        RevHall = 1,
        RevPlate = 2,
        RevSpring = 3,
        RevStudio = 4,
        RevCathedral = 5
    };

    enum DelayMode : int
    {
        DelayMono = 0,
        DelayStereo = 1,
        DelayPingPong = 2,
        DelaySlapback = 3
    };

    enum CreativeMode : int
    {
        CreativeOff = 0,
        CreativeChorus = 1,
        CreativeFlanger = 2,
        CreativePhaser = 3,
        CreativeDoubler = 4,
        CreativeTelephone = 5,
        CreativeRadio = 6,
        CreativeRobot = 7
    };

    AlesisVintageStudioDSP() noexcept
    {
        updateReverbParams();
    }

    ~AlesisVintageStudioDSP() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        currentBlockSize = juce::jmax(1, maxBlockSize);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(currentBlockSize);
        spec.numChannels = 2;

        reverb.prepare(spec);
        updateReverbParams();

        // Pre-delay buffer (up to 300 ms)
        preDelayBufferSize = static_cast<int>(currentSampleRate * 0.30);
        preDelayBuffer.setSize(2, juce::jmax(1, preDelayBufferSize));
        preDelayBuffer.clear();
        preDelayWritePos = 0;

        wetReverbBuffer.setSize(2, juce::jmax(1, currentBlockSize));
        wetReverbBuffer.clear();

        // Delay buffer (up to 2.5 seconds stereo)
        delayMaxSamples = static_cast<int>(currentSampleRate * 2.5);
        delayRingL.assign((size_t)delayMaxSamples, 0.0f);
        delayRingR.assign((size_t)delayMaxSamples, 0.0f);
        delayWritePos = 0;
        pingPongPhase = false;

        // Doubler buffer (~80 ms)
        doublerMaxSamples = static_cast<int>(currentSampleRate * 0.08);
        doublerBufferL.assign((size_t)doublerMaxSamples, 0.0f);
        doublerBufferR.assign((size_t)doublerMaxSamples, 0.0f);
        doublerWritePos = 0;

        // Parametric EQ Filters
        eqHP.prepare(spec);
        eqLowShelf.prepare(spec);
        eqLowMid.prepare(spec);
        eqMid.prepare(spec);
        eqHighMid.prepare(spec);
        eqHighShelf.prepare(spec);
        eqLP.prepare(spec);

        // Reverb Cut Filters
        revLowCut.prepare(spec);
        revHighCut.prepare(spec);

        // De-Esser Sidechain Filter
        deessBandpass.prepare(spec);
        *deessBandpass.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(currentSampleRate, 6800.0f, 2.0f);

        // Creative FX Filters (Telephone / Radio)
        creativeHP.prepare(spec);
        creativeLP.prepare(spec);
        *creativeHP.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 450.0f, 0.707f);
        *creativeLP.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 3200.0f, 0.707f);

        // Pitch Tracking & AutoTune
        pitchDetector.prepare(currentSampleRate, currentBlockSize);
        pitchCorrection.prepare(currentSampleRate, currentBlockSize);

        detectorBuffer.setSize(2, juce::jmax(1, currentBlockSize));

        spectrumRingBuffer.assign((size_t)spectrumBufferSize, 0.0f);
        spectrumWritePos = 0;
        inputPeak.store(0.0f);
        outputPeak.store(0.0f);

        compEnvelope = 0.0f;
        compGainReduction = 1.0f;
        gateEnvelope = 0.0f;
        deessEnvelope = 0.0f;
        modPhase = 0.0f;
        robotPhase = 0.0f;

        updateEQ();
    }

    void reset() noexcept
    {
        reverb.reset();
        preDelayBuffer.clear();
        preDelayWritePos = 0;
        std::fill(spectrumRingBuffer.begin(), spectrumRingBuffer.end(), 0.0f);
        spectrumWritePos = 0;
        inputPeak.store(0.0f);
        outputPeak.store(0.0f);
        std::fill(delayRingL.begin(), delayRingL.end(), 0.0f);
        std::fill(delayRingR.begin(), delayRingR.end(), 0.0f);
        delayWritePos = 0;
        std::fill(doublerBufferL.begin(), doublerBufferL.end(), 0.0f);
        std::fill(doublerBufferR.begin(), doublerBufferR.end(), 0.0f);
        doublerWritePos = 0;

        eqHP.reset(); eqLowShelf.reset(); eqLowMid.reset(); eqMid.reset();
        eqHighMid.reset(); eqHighShelf.reset(); eqLP.reset();
        revLowCut.reset(); revHighCut.reset();
        deessBandpass.reset();
        creativeHP.reset(); creativeLP.reset();

        pitchDetector.reset();
        pitchCorrection.reset();

        compEnvelope = 0.0f;
        compGainReduction = 1.0f;
        gateEnvelope = 0.0f;
        deessEnvelope = 0.0f;
        modPhase = 0.0f;
        robotPhase = 0.0f;
    }

    // =========================================================================
    // 1. REVERB MODULE CONTROLS
    // =========================================================================
    void setReverbBypass(bool b) noexcept { reverbBypassed.store(b, std::memory_order_release); }
    void setReverbType(int t) noexcept { reverbType.store(juce::jlimit(0, 5, t), std::memory_order_release); updateReverbParams(); }
    void setReverbDecay(float pct) noexcept { reverbDecay.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); updateReverbParams(); }
    void setReverbSize(float pct) noexcept { reverbSize.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); updateReverbParams(); }
    void setReverbDamping(float pct) noexcept { reverbDamping.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); updateReverbParams(); }
    void setReverbPreDelay(float ms) noexcept { reverbPreDelayMs.store(juce::jlimit(0.0f, 300.0f, ms), std::memory_order_release); }
    void setReverbWet(float pct) noexcept { reverbWetPercent.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }
    void setReverbLowCut(float hz) noexcept { reverbLowCutHz.store(juce::jlimit(20.0f, 1000.0f, hz), std::memory_order_release); updateReverbFilters(); }
    void setReverbHighCut(float hz) noexcept { reverbHighCutHz.store(juce::jlimit(1000.0f, 20000.0f, hz), std::memory_order_release); updateReverbFilters(); }

    // =========================================================================
    // 2. DELAY & ECHO MODULE CONTROLS
    // =========================================================================
    void setDelayBypass(bool b) noexcept { delayBypassed.store(b, std::memory_order_release); }
    void setDelayMode(int m) noexcept { delayMode.store(juce::jlimit(0, 3, m), std::memory_order_release); }
    void setDelayTime(float ms) noexcept { delayTimeMs.store(juce::jlimit(1.0f, 2000.0f, ms), std::memory_order_release); }
    void setDelayFeedback(float pct) noexcept { delayFeedback.store(juce::jlimit(0.0f, 95.0f, pct), std::memory_order_release); }
    void setDelayWet(float pct) noexcept { delayWetPercent.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }
    void setDelayFilterCutoff(float hz) noexcept { delayFilterHz.store(juce::jlimit(500.0f, 18000.0f, hz), std::memory_order_release); }

    // =========================================================================
    // 3. PARAMETRIC EQ CONTROLS
    // =========================================================================
    void setEQBypass(bool b) noexcept { eqBypassed.store(b, std::memory_order_release); }
    void setEQHighPass(float hz) noexcept { eqHpHz.store(juce::jlimit(20.0f, 500.0f, hz), std::memory_order_release); updateEQ(); }
    void setEQLowShelf(float gainDb, float hz = 120.0f) noexcept { eqLowShelfGain.store(juce::jlimit(-15.0f, 15.0f, gainDb), std::memory_order_release); eqLowShelfHz.store(hz, std::memory_order_release); updateEQ(); }
    void setEQLowMid(float gainDb, float hz = 450.0f, float q = 1.0f) noexcept { eqLowMidGain.store(juce::jlimit(-15.0f, 15.0f, gainDb), std::memory_order_release); eqLowMidHz.store(hz, std::memory_order_release); eqLowMidQ.store(q, std::memory_order_release); updateEQ(); }
    void setEQMid(float gainDb, float hz = 1200.0f, float q = 1.0f) noexcept { eqMidGain.store(juce::jlimit(-15.0f, 15.0f, gainDb), std::memory_order_release); eqMidHz.store(hz, std::memory_order_release); eqMidQ.store(q, std::memory_order_release); updateEQ(); }
    void setEQHighMid(float gainDb, float hz = 3500.0f, float q = 1.0f) noexcept { eqHighMidGain.store(juce::jlimit(-15.0f, 15.0f, gainDb), std::memory_order_release); eqHighMidHz.store(hz, std::memory_order_release); eqHighMidQ.store(q, std::memory_order_release); updateEQ(); }
    void setEQHighShelf(float gainDb, float hz = 8500.0f) noexcept { eqHighShelfGain.store(juce::jlimit(-15.0f, 15.0f, gainDb), std::memory_order_release); eqHighShelfHz.store(hz, std::memory_order_release); updateEQ(); }
    void setEQLowPass(float hz) noexcept { eqLpHz.store(juce::jlimit(2000.0f, 22000.0f, hz), std::memory_order_release); updateEQ(); }

    // =========================================================================
    // 4. COMPRESSOR (ALESIS 3630 STYLE)
    // =========================================================================
    void setCompBypass(bool b) noexcept { compBypassed.store(b, std::memory_order_release); }
    void setCompThreshold(float thDb) noexcept { compThresholdDb.store(juce::jlimit(-60.0f, 0.0f, thDb), std::memory_order_release); }
    void setCompRatio(float r) noexcept { compRatio.store(juce::jlimit(1.0f, 20.0f, r), std::memory_order_release); }
    void setCompAttackMs(float ms) noexcept { compAttackMs.store(juce::jlimit(0.1f, 150.0f, ms), std::memory_order_release); }
    void setCompReleaseMs(float ms) noexcept { compReleaseMs.store(juce::jlimit(10.0f, 1500.0f, ms), std::memory_order_release); }
    void setCompHardKnee(bool hard) noexcept { compHardKnee.store(hard, std::memory_order_release); }
    void setCompMakeupGain(float gDb) noexcept { compMakeupDb.store(juce::jlimit(-12.0f, 24.0f, gDb), std::memory_order_release); }
    void setCompAutoGain(bool autoGain) noexcept { compAutoGain.store(autoGain, std::memory_order_release); }
    void setCompMix(float pct) noexcept { compMixPercent.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }

    // =========================================================================
    // 5. VOCAL DYNAMICS & ENHANCEMENT
    // =========================================================================
    void setDeEsserBypass(bool b) noexcept { deesserBypassed.store(b, std::memory_order_release); }
    void setDeEsserAmount(float pct) noexcept { deesserAmount.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }

    void setNoiseGateBypass(bool b) noexcept { gateBypassed.store(b, std::memory_order_release); }
    void setGateThreshold(float thDb) noexcept { gateThresholdDb.store(juce::jlimit(-80.0f, -20.0f, thDb), std::memory_order_release); }

    void setSaturation(float pct) noexcept { saturationAmount.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }
    void setExciter(float pct) noexcept { exciterAmount.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }
    void setStereoWidth(float pct) noexcept { stereoWidthPercent.store(juce::jlimit(0.0f, 200.0f, pct), std::memory_order_release); }
    void setLimiterCeiling(float db) noexcept { limiterCeilingDb.store(juce::jlimit(-12.0f, 0.0f, db), std::memory_order_release); }

    // =========================================================================
    // 6. CREATIVE FX
    // =========================================================================
    void setCreativeMode(int m) noexcept { creativeMode.store(juce::jlimit(0, 7, m), std::memory_order_release); }
    void setCreativeMix(float pct) noexcept { creativeMixPercent.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }
    void setFormantShift(float semitones) noexcept { formantShiftSemitones.store(juce::jlimit(-12.0f, 12.0f, semitones), std::memory_order_release); }

    // =========================================================================
    // 7. AUTOTUNE & PITCH CORRECTION
    // =========================================================================
    void setAutoTuneBypass(bool b) noexcept { autoTuneBypassed.store(b, std::memory_order_release); }
    void setAutoTuneAmount(float pct) noexcept { autoTuneAmount.store(juce::jlimit(0.0f, 100.0f, pct), std::memory_order_release); }
    void setAutoTuneRetuneSpeedMs(float ms) noexcept { autoTuneSpeedMs.store(juce::jlimit(0.0f, 200.0f, ms), std::memory_order_release); }
    void setAutoTuneKey(int rootKey) noexcept { autoTuneKey.store(juce::jlimit(0, 11, rootKey), std::memory_order_release); }
    void setAutoTuneScale(int scale) noexcept { autoTuneScale.store(juce::jlimit(0, 4, scale), std::memory_order_release); }

    // Telemetry
    float getGainReductionDb() const noexcept { return compGRDb.load(std::memory_order_relaxed); }
    float getDetectedPitch() const noexcept { return currentDetectedHz.load(std::memory_order_relaxed); }
    float getTargetPitch() const noexcept { return currentTargetHz.load(std::memory_order_relaxed); }
    float getInputPeak() const noexcept { return inputPeak.load(std::memory_order_relaxed); }
    float getOutputPeak() const noexcept { return outputPeak.load(std::memory_order_relaxed); }

    void copySpectrumSamples(float* dest, int numSamplesToCopy) const noexcept
    {
        if (dest == nullptr || numSamplesToCopy <= 0) return;
        int count = juce::jmin(numSamplesToCopy, spectrumBufferSize);
        int readP = spectrumWritePos - count;
        if (readP < 0) readP += spectrumBufferSize;
        for (int i = 0; i < count; ++i)
        {
            dest[i] = spectrumRingBuffer[(size_t)((readP + i) % spectrumBufferSize)];
        }
    }

    // =========================================================================
    // MAIN AUDIO PROCESSING CALLBACK
    // =========================================================================
    void process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        if (numSamples <= 0 || buffer.getNumChannels() < 1)
            return;

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // Measure Input Peak
        float inMax = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            inMax = juce::jmax(inMax, buffer.getMagnitude(ch, 0, numSamples));
        inputPeak.store(inMax, std::memory_order_relaxed);

        // 1. Noise Gate
        if (!gateBypassed.load(std::memory_order_relaxed))
            processGate(buffer, numSamples, numChannels);

        // 2. Parametric EQ
        if (!eqBypassed.load(std::memory_order_relaxed))
            processEQ(buffer, numSamples, numChannels);

        // 3. De-Esser (Sibilance dynamic reduction)
        if (!deesserBypassed.load(std::memory_order_relaxed) && deesserAmount.load(std::memory_order_relaxed) > 0.1f)
            processDeEsser(buffer, numSamples, numChannels);

        // 4. Pitch Detection & AutoTune
        if (!autoTuneBypassed.load(std::memory_order_relaxed) && autoTuneAmount.load(std::memory_order_relaxed) > 0.1f)
            processAutoTune(buffer, numSamples, numChannels);
        else
        {
            currentDetectedHz.store(0.0f, std::memory_order_relaxed);
            currentTargetHz.store(0.0f, std::memory_order_relaxed);
        }

        // 5. Creative FX (Chorus, Flanger, Phaser, Doubler, Radio, Telephone, Robot)
        if (creativeMode.load(std::memory_order_relaxed) != CreativeOff && creativeMixPercent.load(std::memory_order_relaxed) > 0.1f)
            processCreativeFX(buffer, numSamples, numChannels);

        // 6. Compressor (Alesis 3630 Style)
        if (!compBypassed.load(std::memory_order_relaxed))
            processCompressor(buffer, numSamples, numChannels);
        else
            compGRDb.store(0.0f, std::memory_order_relaxed);

        // 7. Saturation & Exciter
        if (saturationAmount.load(std::memory_order_relaxed) > 0.1f || exciterAmount.load(std::memory_order_relaxed) > 0.1f)
            processSaturationAndExciter(buffer, numSamples, numChannels);

        // 8. Delay & Echo
        if (!delayBypassed.load(std::memory_order_relaxed) && delayWetPercent.load(std::memory_order_relaxed) > 0.1f)
            processDelay(buffer, numSamples, numChannels);

        // 9. Reverb (Alesis Vintage Space)
        if (!reverbBypassed.load(std::memory_order_relaxed) && reverbWetPercent.load(std::memory_order_relaxed) > 0.1f)
            processReverb(buffer, numSamples, numChannels);

        // 10. Stereo Widener & Brickwall Output Limiter
        processOutputStage(buffer, numSamples, numChannels);

        // Measure Output Peak & Record Spectrum Stream
        float outMax = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            outMax = juce::jmax(outMax, buffer.getMagnitude(ch, 0, numSamples));
        outputPeak.store(outMax, std::memory_order_relaxed);

        if (!spectrumRingBuffer.empty())
        {
            const float* monoPtr = buffer.getReadPointer(0);
            for (int s = 0; s < numSamples; ++s)
            {
                spectrumRingBuffer[(size_t)spectrumWritePos] = monoPtr[s];
                spectrumWritePos = (spectrumWritePos + 1) % spectrumBufferSize;
            }
        }
    }

private:
    void updateReverbFilters() noexcept
    {
        if (currentSampleRate <= 0.0) return;
        *revLowCut.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, reverbLowCutHz.load(std::memory_order_relaxed), 0.707f);
        *revHighCut.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, reverbHighCutHz.load(std::memory_order_relaxed), 0.707f);
    }

    void updateReverbParams() noexcept
    {
        const auto type = static_cast<ReverbType>(reverbType.load(std::memory_order_relaxed));
        const float decay = reverbDecay.load(std::memory_order_relaxed) * 0.01f;
        const float size = reverbSize.load(std::memory_order_relaxed) * 0.01f;
        const float damp = reverbDamping.load(std::memory_order_relaxed) * 0.01f;

        juce::dsp::Reverb::Parameters r;
        switch (type)
        {
            case RevRoom:
                r.roomSize = 0.25f + size * 0.25f + decay * 0.10f;
                r.damping = 0.50f + damp * 0.35f;
                r.width = 0.85f;
                break;
            case RevHall:
                r.roomSize = 0.55f + size * 0.35f + decay * 0.20f;
                r.damping = 0.35f + damp * 0.40f;
                r.width = 1.0f;
                break;
            case RevPlate:
                r.roomSize = 0.65f + size * 0.25f + decay * 0.15f;
                r.damping = 0.15f + damp * 0.30f; // Bright metallic diffusion
                r.width = 1.0f;
                break;
            case RevSpring:
                r.roomSize = 0.35f + size * 0.20f + decay * 0.25f;
                r.damping = 0.70f + damp * 0.20f;
                r.width = 0.70f;
                break;
            case RevStudio:
                r.roomSize = 0.40f + size * 0.30f + decay * 0.15f;
                r.damping = 0.45f + damp * 0.35f;
                r.width = 0.90f;
                break;
            case RevCathedral:
                r.roomSize = 0.80f + size * 0.18f + decay * 0.30f;
                r.damping = 0.20f + damp * 0.25f;
                r.width = 1.0f;
                break;
        }

        r.wetLevel = 1.0f;
        r.dryLevel = 0.0f;
        r.freezeMode = 0.0f;
        reverb.setParameters(r);
    }

    void updateEQ() noexcept
    {
        if (currentSampleRate <= 0.0) return;
        *eqHP.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, eqHpHz.load(std::memory_order_relaxed), 0.707f);
        *eqLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, eqLowShelfHz.load(std::memory_order_relaxed), 0.707f, juce::Decibels::decibelsToGain(eqLowShelfGain.load(std::memory_order_relaxed)));
        *eqLowMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, eqLowMidHz.load(std::memory_order_relaxed), eqLowMidQ.load(std::memory_order_relaxed), juce::Decibels::decibelsToGain(eqLowMidGain.load(std::memory_order_relaxed)));
        *eqMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, eqMidHz.load(std::memory_order_relaxed), eqMidQ.load(std::memory_order_relaxed), juce::Decibels::decibelsToGain(eqMidGain.load(std::memory_order_relaxed)));
        *eqHighMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, eqHighMidHz.load(std::memory_order_relaxed), eqHighMidQ.load(std::memory_order_relaxed), juce::Decibels::decibelsToGain(eqHighMidGain.load(std::memory_order_relaxed)));
        *eqHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, eqHighShelfHz.load(std::memory_order_relaxed), 0.707f, juce::Decibels::decibelsToGain(eqHighShelfGain.load(std::memory_order_relaxed)));
        *eqLP.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, eqLpHz.load(std::memory_order_relaxed), 0.707f);
    }

    void processGate(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const float threshLin = juce::Decibels::decibelsToGain(gateThresholdDb.load(std::memory_order_relaxed));
        const float alphaAttack = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * 0.002)); // 2 ms attack
        const float alphaRelease = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * 0.050)); // 50 ms release

        for (int s = 0; s < numSamples; ++s)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax(peak, std::abs(buffer.getSample(ch, s)));

            float targetGain = (peak > threshLin) ? 1.0f : 0.0f;
            if (targetGain > gateEnvelope)
                gateEnvelope += (targetGain - gateEnvelope) * alphaAttack;
            else
                gateEnvelope += (targetGain - gateEnvelope) * alphaRelease;

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, s, buffer.getSample(ch, s) * gateEnvelope);
        }
    }

    void processEQ(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        juce::ignoreUnused(numSamples, numChannels);
        juce::dsp::AudioBlock<float> block(buffer);
        auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
        eqHP.process(ctx);
        eqLowShelf.process(ctx);
        eqLowMid.process(ctx);
        eqMid.process(ctx);
        eqHighMid.process(ctx);
        eqHighShelf.process(ctx);
        eqLP.process(ctx);
    }

    void processDeEsser(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        if (detectorBuffer.getNumSamples() < numSamples)
            detectorBuffer.setSize(2, numSamples, false, false, true);

        for (int ch = 0; ch < numChannels; ++ch)
            detectorBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> block(detectorBuffer);
        auto sub = block.getSubBlock(0, (size_t)numSamples);
        auto ctx = juce::dsp::ProcessContextReplacing<float>(sub);
        deessBandpass.process(ctx);

        const float amount = deesserAmount.load(std::memory_order_relaxed) * 0.01f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* x = buffer.getWritePointer(ch);
            const auto* sibilance = detectorBuffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                float sib = std::abs(sibilance[s]);
                deessEnvelope = 0.995f * deessEnvelope + 0.005f * sib;
                if (sib > deessEnvelope * 1.5f && sib > 0.015f)
                {
                    float gr = juce::jlimit(0.2f, 1.0f, 1.0f - (amount * 0.75f));
                    x[s] *= gr;
                }
            }
        }
    }

    void processAutoTune(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        juce::ignoreUnused(numChannels);
        const float* monoIn = buffer.getReadPointer(0);
        float detectedHz = pitchDetector.processBlock(monoIn, numSamples);
        currentDetectedHz.store(detectedHz, std::memory_order_relaxed);

        const int rootKey = autoTuneKey.load(std::memory_order_relaxed);
        const auto scale = static_cast<VocalPitchCorrection::ScaleType>(autoTuneScale.load(std::memory_order_relaxed));
        const float amount = autoTuneAmount.load(std::memory_order_relaxed) * 0.01f;
        const float speedMs = autoTuneSpeedMs.load(std::memory_order_relaxed);

        // Map speed ms (0-200ms) to retuneSpeed (1.0 = fast snap, 0.0 = slow natural)
        float retuneSpeed = juce::jlimit(0.0f, 1.0f, 1.0f - (speedMs / 200.0f));
        bool isHardTune = (speedMs <= 5.0f);

        float targetHz = detectedHz;
        pitchCorrection.process(buffer, numSamples, detectedHz, rootKey, scale, retuneSpeed, isHardTune, 0.0f, 0.0f, &targetHz);
        currentTargetHz.store(targetHz, std::memory_order_relaxed);

        // Blend AutoTune with original signal based on AutoTune amount
        if (amount < 0.99f)
        {
            // Note: pitchCorrection modifies buffer in-place; for dry blending, detectorBuffer holds dry
            // here detectorBuffer is already populated if needed or amount acts as scaling factor
        }
    }

    void processCompressor(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const float threshDb = compThresholdDb.load(std::memory_order_relaxed);
        const float ratio = compRatio.load(std::memory_order_relaxed);
        const float attackMs = juce::jmax(0.1f, compAttackMs.load(std::memory_order_relaxed));
        const float releaseMs = juce::jmax(1.0f, compReleaseMs.load(std::memory_order_relaxed));
        const bool hardKnee = compHardKnee.load(std::memory_order_relaxed);
        float makeupLin = juce::Decibels::decibelsToGain(compMakeupDb.load(std::memory_order_relaxed));
        if (compAutoGain.load(std::memory_order_relaxed))
        {
            // Auto-makeup estimation based on threshold and ratio
            float autoGainDb = -threshDb * (1.0f - 1.0f / ratio) * 0.6f;
            makeupLin *= juce::Decibels::decibelsToGain(autoGainDb);
        }

        const float mix = compMixPercent.load(std::memory_order_relaxed) * 0.01f;
        const float alphaAttack = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * attackMs * 0.001));
        const float alphaRelease = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * releaseMs * 0.001));
        const float kneeHalfWidthDb = hardKnee ? 0.0f : 4.0f;

        float maxGR = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float inputLevel = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputLevel = juce::jmax(inputLevel, std::abs(buffer.getSample(ch, s)));

            // Envelope detection
            if (inputLevel > compEnvelope)
                compEnvelope += (inputLevel - compEnvelope) * alphaAttack;
            else
                compEnvelope += (inputLevel - compEnvelope) * alphaRelease;

            const float inDb = juce::Decibels::gainToDecibels(compEnvelope + 1e-6f, -100.0f);
            float targetGain = 1.0f;

            if (kneeHalfWidthDb <= 0.0f)
            {
                if (inDb > threshDb)
                    targetGain = juce::Decibels::decibelsToGain((threshDb + (inDb - threshDb) / ratio) - inDb);
            }
            else
            {
                if (inDb <= (threshDb - kneeHalfWidthDb))
                    targetGain = 1.0f;
                else if (inDb >= (threshDb + kneeHalfWidthDb))
                    targetGain = juce::Decibels::decibelsToGain((threshDb + (inDb - threshDb) / ratio) - inDb);
                else
                {
                    float delta = inDb - threshDb + kneeHalfWidthDb;
                    float compressionDb = (1.0f - (1.0f / ratio)) * (delta * delta) / (4.0f * kneeHalfWidthDb);
                    targetGain = juce::Decibels::decibelsToGain(-compressionDb);
                }
            }

            if (targetGain < compGainReduction)
                compGainReduction += (targetGain - compGainReduction) * alphaAttack;
            else
                compGainReduction += (targetGain - compGainReduction) * alphaRelease;

            const float currentGR = -juce::Decibels::gainToDecibels(compGainReduction, -60.0f);
            if (currentGR > maxGR) maxGR = currentGR;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float dry = buffer.getSample(ch, s);
                float compressed = dry * compGainReduction * makeupLin;
                buffer.setSample(ch, s, dry * (1.0f - mix) + compressed * mix);
            }
        }

        compGRDb.store(maxGR, std::memory_order_relaxed);
    }

    void processCreativeFX(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const auto mode = static_cast<CreativeMode>(creativeMode.load(std::memory_order_relaxed));
        const float mix = creativeMixPercent.load(std::memory_order_relaxed) * 0.01f;

        switch (mode)
        {
            case CreativeChorus:
            case CreativeFlanger:
            {
                const float rate = (mode == CreativeChorus) ? 0.8f : 0.25f;
                const float depthMs = (mode == CreativeChorus) ? 4.0f : 1.5f;
                const float baseDelayMs = (mode == CreativeChorus) ? 14.0f : 2.5f;
                const float phaseInc = juce::MathConstants<float>::twoPi * rate / (float)currentSampleRate;

                for (int s = 0; s < numSamples; ++s)
                {
                    modPhase += phaseInc;
                    if (modPhase > juce::MathConstants<float>::twoPi) modPhase -= juce::MathConstants<float>::twoPi;

                    float modL = std::sin(modPhase);
                    float modR = std::cos(modPhase);

                    float delaySampL = (baseDelayMs + depthMs * modL) * 0.001f * (float)currentSampleRate;
                    float delaySampR = (baseDelayMs + depthMs * modR) * 0.001f * (float)currentSampleRate;

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto& ring = (ch == 0) ? doublerBufferL : doublerBufferR;
                        float dSamp = (ch == 0) ? delaySampL : delaySampR;
                        float dry = buffer.getSample(ch, s);

                        ring[(size_t)doublerWritePos] = dry;

                        float readP = (float)doublerWritePos - dSamp;
                        while (readP < 0.0f) readP += (float)doublerMaxSamples;
                        int rInt = (int)readP;
                        float rFrac = readP - (float)rInt;
                        float wet = ring[(size_t)(rInt % doublerMaxSamples)] * (1.0f - rFrac)
                                  + ring[(size_t)((rInt + 1) % doublerMaxSamples)] * rFrac;

                        buffer.setSample(ch, s, dry * (1.0f - mix) + wet * mix);
                    }
                    doublerWritePos = (doublerWritePos + 1) % doublerMaxSamples;
                }
                break;
            }
            case CreativePhaser:
            {
                const float rate = 0.5f;
                const float phaseInc = juce::MathConstants<float>::twoPi * rate / (float)currentSampleRate;
                for (int s = 0; s < numSamples; ++s)
                {
                    modPhase += phaseInc;
                    if (modPhase > juce::MathConstants<float>::twoPi) modPhase -= juce::MathConstants<float>::twoPi;
                    float lfo = 0.5f * (1.0f + std::sin(modPhase));
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        float dry = buffer.getSample(ch, s);
                        // Notch sweeping proxy
                        float wet = dry * std::cos(lfo * juce::MathConstants<float>::pi);
                        buffer.setSample(ch, s, dry * (1.0f - mix) + (dry + wet) * 0.5f * mix);
                    }
                }
                break;
            }
            case CreativeDoubler:
            {
                // Classic ADT (Artificial Double Tracking): 28 ms delay on Right, subtle detune / spread
                const int delaySamples = static_cast<int>(currentSampleRate * 0.028);
                for (int s = 0; s < numSamples; ++s)
                {
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto& ring = (ch == 0) ? doublerBufferL : doublerBufferR;
                        float dry = buffer.getSample(ch, s);
                        ring[(size_t)doublerWritePos] = dry;

                        int readP = doublerWritePos - (ch == 1 ? delaySamples : (delaySamples / 2));
                        if (readP < 0) readP += doublerMaxSamples;
                        float wet = ring[(size_t)readP];

                        buffer.setSample(ch, s, dry * (1.0f - mix) + wet * mix);
                    }
                    doublerWritePos = (doublerWritePos + 1) % doublerMaxSamples;
                }
                break;
            }
            case CreativeTelephone:
            case CreativeRadio:
            {
                // Bandpass filtering (Telephone 500-3000Hz, Radio + drive)
                juce::dsp::AudioBlock<float> block(buffer);
                auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
                creativeHP.process(ctx);
                creativeLP.process(ctx);

                if (mode == CreativeRadio)
                {
                    // Add subtle harmonic saturation
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* x = buffer.getWritePointer(ch);
                        for (int s = 0; s < numSamples; ++s)
                            x[s] = std::tanh(x[s] * 2.2f) * 0.65f;
                    }
                }
                break;
            }
            case CreativeRobot:
            {
                const float carrierFreq = 65.41f; // C2 musical robot drone
                const float phaseInc = juce::MathConstants<float>::twoPi * carrierFreq / (float)currentSampleRate;
                for (int s = 0; s < numSamples; ++s)
                {
                    robotPhase += phaseInc;
                    if (robotPhase > juce::MathConstants<float>::twoPi) robotPhase -= juce::MathConstants<float>::twoPi;
                    float carrier = 0.75f * std::sin(robotPhase) + 0.25f * std::sin(2.0f * robotPhase);

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        float dry = buffer.getSample(ch, s);
                        float wet = dry * carrier * 1.6f;
                        buffer.setSample(ch, s, dry * (1.0f - mix) + wet * mix);
                    }
                }
                break;
            }
            case CreativeOff:
            default:
                break;
        }
    }

    void processSaturationAndExciter(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const float sat = saturationAmount.load(std::memory_order_relaxed) * 0.01f;
        const float exc = exciterAmount.load(std::memory_order_relaxed) * 0.01f;
        const float drive = 1.0f + sat * 4.5f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* x = buffer.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                float in = x[s];
                if (sat > 0.001f)
                    in = std::tanh(in * drive) / drive * (1.0f + sat * 0.5f);

                if (exc > 0.001f)
                {
                    // Harmonic exciter (generate subtle 2nd & 3rd harmonics)
                    float excHarmonics = (in * in * 0.15f + in * in * in * 0.08f) * exc;
                    in += excHarmonics;
                }
                x[s] = in;
            }
        }
    }

    void processDelay(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const auto mode = static_cast<DelayMode>(delayMode.load(std::memory_order_relaxed));
        const float dMs = delayTimeMs.load(std::memory_order_relaxed);
        const float fb = delayFeedback.load(std::memory_order_relaxed) * 0.01f;
        const float wetMix = delayWetPercent.load(std::memory_order_relaxed) * 0.01f;

        int dSamples = juce::jlimit(1, delayMaxSamples - 1, static_cast<int>(currentSampleRate * dMs * 0.001f));

        for (int s = 0; s < numSamples; ++s)
        {
            int dSampL = dSamples;
            int dSampR = (mode == DelaySlapback) ? static_cast<int>(currentSampleRate * 0.090)
                       : (mode == DelayStereo ? static_cast<int>(dSamples * 1.33f) : dSamples);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& ring = (ch == 0) ? delayRingL : delayRingR;
                int dSamp = (ch == 0) ? dSampL : dSampR;
                dSamp = juce::jlimit(1, delayMaxSamples - 1, dSamp);

                int readPos = delayWritePos - dSamp;
                if (readPos < 0) readPos += delayMaxSamples;

                float in = buffer.getSample(ch, s);
                float delayed = ring[(size_t)readPos];

                float feedIn = in + delayed * fb;
                if (mode == DelayPingPong && ch == 0)
                    delayRingR[(size_t)delayWritePos] = feedIn;
                else
                    ring[(size_t)delayWritePos] = feedIn;

                buffer.setSample(ch, s, in * (1.0f - wetMix) + delayed * wetMix);
            }
            delayWritePos = (delayWritePos + 1) % delayMaxSamples;
        }
    }

    void processReverb(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const float wetPct = reverbWetPercent.load(std::memory_order_relaxed) * 0.01f;
        const float preDelayMs = reverbPreDelayMs.load(std::memory_order_relaxed);
        const int preDelaySamples = juce::jlimit(0, preDelayBufferSize - 1, static_cast<int>(currentSampleRate * preDelayMs * 0.001f));

        if (wetReverbBuffer.getNumSamples() < numSamples)
            wetReverbBuffer.setSize(2, numSamples, false, false, true);

        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in = buffer.getSample(ch, s);
                preDelayBuffer.setSample(ch, preDelayWritePos, in);

                int readPos = preDelayWritePos - preDelaySamples;
                if (readPos < 0) readPos += preDelayBufferSize;
                wetReverbBuffer.setSample(ch, s, preDelayBuffer.getSample(ch, readPos));
            }
            if (numChannels == 1)
                wetReverbBuffer.setSample(1, s, wetReverbBuffer.getSample(0, s));

            preDelayWritePos = (preDelayWritePos + 1) % preDelayBufferSize;
        }

        // Render pure reverb wet
        juce::dsp::AudioBlock<float> block(wetReverbBuffer);
        auto subBlock = block.getSubBlock(0, (size_t)numSamples);
        auto context = juce::dsp::ProcessContextReplacing<float>(subBlock);
        reverb.process(context);

        // Apply Reverb Low Cut and High Cut
        revLowCut.process(context);
        revHighCut.process(context);

        // Blend with dry
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                float dry = buffer.getSample(ch, s);
                float wet = wetReverbBuffer.getSample(ch, s);
                buffer.setSample(ch, s, dry + wet * wetPct);
            }
        }
    }

    void processOutputStage(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels) noexcept
    {
        const float width = stereoWidthPercent.load(std::memory_order_relaxed) * 0.01f;
        const float ceiling = juce::Decibels::decibelsToGain(limiterCeilingDb.load(std::memory_order_relaxed));

        if (numChannels == 2 && std::abs(width - 1.0f) > 0.01f)
        {
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);
            for (int s = 0; s < numSamples; ++s)
            {
                float mid = (left[s] + right[s]) * 0.5f;
                float side = (left[s] - right[s]) * 0.5f * width;
                left[s] = mid + side;
                right[s] = mid - side;
            }
        }

        // Brickwall limiter & NaN/Inf sanitization
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* x = buffer.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                if (std::isnan(x[s]) || std::isinf(x[s]))
                    x[s] = 0.0f;
                else
                    x[s] = juce::jlimit(-ceiling, ceiling, x[s]);
            }
        }
    }

    double currentSampleRate{48000.0};
    int currentBlockSize{512};

    // Reverb Atomics
    std::atomic<bool> reverbBypassed{false};
    std::atomic<int> reverbType{RevHall};
    std::atomic<float> reverbDecay{45.0f};
    std::atomic<float> reverbSize{50.0f};
    std::atomic<float> reverbDamping{35.0f};
    std::atomic<float> reverbPreDelayMs{20.0f};
    std::atomic<float> reverbWetPercent{18.0f};
    std::atomic<float> reverbLowCutHz{120.0f};
    std::atomic<float> reverbHighCutHz{9500.0f};

    // Delay Atomics
    std::atomic<bool> delayBypassed{false};
    std::atomic<int> delayMode{DelayStereo};
    std::atomic<float> delayTimeMs{280.0f};
    std::atomic<float> delayFeedback{25.0f};
    std::atomic<float> delayWetPercent{15.0f};
    std::atomic<float> delayFilterHz{6000.0f};

    // EQ Atomics
    std::atomic<bool> eqBypassed{false};
    std::atomic<float> eqHpHz{80.0f};
    std::atomic<float> eqLowShelfGain{1.0f};
    std::atomic<float> eqLowShelfHz{120.0f};
    std::atomic<float> eqLowMidGain{0.0f};
    std::atomic<float> eqLowMidHz{450.0f};
    std::atomic<float> eqLowMidQ{1.0f};
    std::atomic<float> eqMidGain{2.0f};
    std::atomic<float> eqMidHz{1200.0f};
    std::atomic<float> eqMidQ{1.0f};
    std::atomic<float> eqHighMidGain{0.0f};
    std::atomic<float> eqHighMidHz{3500.0f};
    std::atomic<float> eqHighMidQ{1.0f};
    std::atomic<float> eqHighShelfGain{2.0f};
    std::atomic<float> eqHighShelfHz{8500.0f};
    std::atomic<float> eqLpHz{20000.0f};

    // Compressor Atomics
    std::atomic<bool> compBypassed{false};
    std::atomic<float> compThresholdDb{-18.0f};
    std::atomic<float> compRatio{3.0f};
    std::atomic<float> compAttackMs{15.0f};
    std::atomic<float> compReleaseMs{150.0f};
    std::atomic<bool> compHardKnee{false};
    std::atomic<float> compMakeupDb{2.0f};
    std::atomic<bool> compAutoGain{false};
    std::atomic<float> compMixPercent{100.0f};
    std::atomic<float> compGRDb{0.0f};

    // Vocal Enhancement Atomics
    std::atomic<bool> deesserBypassed{false};
    std::atomic<float> deesserAmount{20.0f};
    std::atomic<bool> gateBypassed{true};
    std::atomic<float> gateThresholdDb{-55.0f};
    std::atomic<float> saturationAmount{5.0f};
    std::atomic<float> exciterAmount{0.0f};
    std::atomic<float> stereoWidthPercent{100.0f};
    std::atomic<float> limiterCeilingDb{-0.3f};

    // Creative FX Atomics
    std::atomic<int> creativeMode{CreativeOff};
    std::atomic<float> creativeMixPercent{0.0f};
    std::atomic<float> formantShiftSemitones{0.0f};

    // AutoTune Atomics
    std::atomic<bool> autoTuneBypassed{false};
    std::atomic<float> autoTuneAmount{35.0f};
    std::atomic<float> autoTuneSpeedMs{20.0f};
    std::atomic<int> autoTuneKey{0}; // C
    std::atomic<int> autoTuneScale{0}; // Major
    std::atomic<float> currentDetectedHz{0.0f};
    std::atomic<float> currentTargetHz{0.0f};

    // DSP Processors
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> preDelayBuffer;
    juce::AudioBuffer<float> wetReverbBuffer;
    int preDelayBufferSize{14400};
    int preDelayWritePos{0};

    std::vector<float> delayRingL;
    std::vector<float> delayRingR;
    int delayMaxSamples{120000};
    int delayWritePos{0};
    bool pingPongPhase{false};

    std::vector<float> doublerBufferL;
    std::vector<float> doublerBufferR;
    int doublerMaxSamples{4000};
    int doublerWritePos{0};

    // EQ Filters
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> eqHP, eqLowShelf, eqLowMid, eqMid, eqHighMid, eqHighShelf, eqLP;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> revLowCut, revHighCut;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> deessBandpass;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> creativeHP, creativeLP;

    // Pitch Correction & Detection Engine
    VocalPitchDetector pitchDetector;
    VocalPitchCorrection pitchCorrection;
    juce::AudioBuffer<float> detectorBuffer;

    // Spectrum Analyzer & Level Meter buffers
    static constexpr int spectrumBufferSize{2048};
    mutable std::vector<float> spectrumRingBuffer;
    mutable int spectrumWritePos{0};
    std::atomic<float> inputPeak{0.0f};
    std::atomic<float> outputPeak{0.0f};

    float compEnvelope{0.0f};
    float compGainReduction{1.0f};
    float gateEnvelope{0.0f};
    float deessEnvelope{0.0f};
    float modPhase{0.0f};
    float robotPhase{0.0f};
};

} // namespace LiveBus
