#include "LevelMeterProcessor.h"
#include <algorithm>

namespace LiveBus
{
    LevelMeterProcessor::LevelMeterProcessor()
    {
        reset();
    }

    void LevelMeterProcessor::prepare(double sampleRate, int /*blockSize*/)
    {
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        reset();
    }

    void LevelMeterProcessor::reset()
    {
        blockPeakL.store(0.0f, std::memory_order_relaxed);
        blockPeakR.store(0.0f, std::memory_order_relaxed);
        blockRmsL.store(0.0f, std::memory_order_relaxed);
        blockRmsR.store(0.0f, std::memory_order_relaxed);
        clipFlagL.store(false, std::memory_order_relaxed);
        clipFlagR.store(false, std::memory_order_relaxed);

        currentPeakDbL = -100.0f;
        currentPeakDbR = -100.0f;
        currentRmsDbL = -100.0f;
        currentRmsDbR = -100.0f;

        peakHoldDbL = -100.0f;
        peakHoldDbR = -100.0f;
        peakHoldTimerL = 0.0f;
        peakHoldTimerR = 0.0f;

        vuPositionL = 0.0f;
        vuVelocityL = 0.0f;
        vuPositionR = 0.0f;
        vuVelocityR = 0.0f;
    }

    void LevelMeterProcessor::processBlock(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (numSamples <= 0 || buffer.getNumChannels() < 1)
            return;

        const int numChannels = buffer.getNumChannels();
        const float* inL = buffer.getReadPointer(0, startSample);
        const float* inR = (numChannels > 1) ? buffer.getReadPointer(1, startSample) : inL;

        const float* channels[2] = { inL, inR };
        processBlock(channels, juce::jmin(2, numChannels), numSamples);
    }

    void LevelMeterProcessor::processBlock(const float* const* channelData, int numChannels, int numSamples)
    {
        if (channelData == nullptr || numSamples <= 0 || numChannels < 1)
            return;

        const float* inL = channelData[0];
        const float* inR = (numChannels > 1 && channelData[1] != nullptr) ? channelData[1] : inL;

        float maxL = 0.0f;
        float maxR = 0.0f;
        double sumSqL = 0.0;
        double sumSqR = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float sL = inL[i];
            const float absL = std::abs(sL);
            if (absL > maxL) maxL = absL;
            sumSqL += (double)sL * (double)sL;

            const float sR = inR[i];
            const float absR = std::abs(sR);
            if (absR > maxR) maxR = absR;
            sumSqR += (double)sR * (double)sR;
        }

        // Clip detection
        if (maxL >= clipThreshold)
            clipFlagL.store(true, std::memory_order_relaxed);
        if (maxR >= clipThreshold)
            clipFlagR.store(true, std::memory_order_relaxed);

        // Store block peaks
        blockPeakL.store(maxL, std::memory_order_relaxed);
        blockPeakR.store(maxR, std::memory_order_relaxed);

        // Calculate and store block RMS
        const float rmsL = (float)std::sqrt(sumSqL / (double)numSamples);
        const float rmsR = (float)std::sqrt(sumSqR / (double)numSamples);
        blockRmsL.store(rmsL, std::memory_order_relaxed);
        blockRmsR.store(rmsR, std::memory_order_relaxed);
    }

    void LevelMeterProcessor::updateBallistics(float dtSeconds)
    {
        if (dtSeconds <= 0.0f)
            return;

        // Limit dt to avoid numerical instability if timer was delayed
        const float dt = juce::jlimit(0.001f, 0.1f, dtSeconds);

        const float rawPeakL = blockPeakL.load(std::memory_order_relaxed);
        const float rawPeakR = blockPeakR.load(std::memory_order_relaxed);
        const float rawRmsL = blockRmsL.load(std::memory_order_relaxed);
        const float rawRmsR = blockRmsR.load(std::memory_order_relaxed);

        const float blockPeakDbL = linearToDb(rawPeakL);
        const float blockPeakDbR = linearToDb(rawPeakR);
        const float blockRmsDbL = linearToDb(rawRmsL);
        const float blockRmsDbR = linearToDb(rawRmsR);

        const float decayRate = peakDecayRate.load(std::memory_order_relaxed);
        const float decayAmount = decayRate * dt;

        // 1. Digital Peak Decay
        if (blockPeakDbL >= currentPeakDbL)
            currentPeakDbL = blockPeakDbL; // Instantaneous attack
        else
            currentPeakDbL = std::max(-100.0f, currentPeakDbL - decayAmount);

        if (blockPeakDbR >= currentPeakDbR)
            currentPeakDbR = blockPeakDbR;
        else
            currentPeakDbR = std::max(-100.0f, currentPeakDbR - decayAmount);

        // 2. RMS Ballistics (Fast attack ~30ms, smooth decay ~300ms)
        if (blockRmsDbL >= currentRmsDbL)
        {
            const float attackAlpha = 1.0f - std::exp(-dt / 0.030f);
            currentRmsDbL += attackAlpha * (blockRmsDbL - currentRmsDbL);
        }
        else
        {
            const float releaseAlpha = 1.0f - std::exp(-dt / 0.300f);
            currentRmsDbL += releaseAlpha * (blockRmsDbL - currentRmsDbL);
        }

        if (blockRmsDbR >= currentRmsDbR)
        {
            const float attackAlpha = 1.0f - std::exp(-dt / 0.030f);
            currentRmsDbR += attackAlpha * (blockRmsDbR - currentRmsDbR);
        }
        else
        {
            const float releaseAlpha = 1.0f - std::exp(-dt / 0.300f);
            currentRmsDbR += releaseAlpha * (blockRmsDbR - currentRmsDbR);
        }

        // 3. Peak-Hold Logic
        const bool holdEnabled = peakHoldEnabled.load(std::memory_order_relaxed);
        if (holdEnabled)
        {
            // Left
            if (blockPeakDbL >= peakHoldDbL)
            {
                peakHoldDbL = blockPeakDbL;
                peakHoldTimerL = peakHoldDuration;
            }
            else
            {
                peakHoldTimerL -= dt;
                if (peakHoldTimerL <= 0.0f)
                {
                    peakHoldDbL = std::max(-100.0f, peakHoldDbL - decayAmount);
                }
            }

            // Right
            if (blockPeakDbR >= peakHoldDbR)
            {
                peakHoldDbR = blockPeakDbR;
                peakHoldTimerR = peakHoldDuration;
            }
            else
            {
                peakHoldTimerR -= dt;
                if (peakHoldTimerR <= 0.0f)
                {
                    peakHoldDbR = std::max(-100.0f, peakHoldDbR - decayAmount);
                }
            }
        }
        else
        {
            peakHoldDbL = currentPeakDbL;
            peakHoldDbR = currentPeakDbR;
        }

        // 4. IEC 60268-17 Second-Order Physical VU Ballistics
        const float refDb = vuReferenceDb.load(std::memory_order_relaxed);
        const float targetVuL = blockRmsDbL - refDb;
        const float targetVuR = blockRmsDbR - refDb;

        const float targetNormL = vuToNormalized(targetVuL);
        const float targetNormR = vuToNormalized(targetVuR);

        // Left needle ODE integration (Euler-Cromer)
        const float k = vuNaturalFreq * vuNaturalFreq;
        const float c = 2.0f * vuDamping * vuNaturalFreq;

        const float accelL = k * (targetNormL - vuPositionL) - c * vuVelocityL;
        vuVelocityL += accelL * dt;
        vuPositionL += vuVelocityL * dt;
        if (vuPositionL < 0.0f) { vuPositionL = 0.0f; vuVelocityL = 0.0f; }
        if (vuPositionL > 1.25f) { vuPositionL = 1.25f; vuVelocityL = 0.0f; }

        // Right needle ODE integration
        const float accelR = k * (targetNormR - vuPositionR) - c * vuVelocityR;
        vuVelocityR += accelR * dt;
        vuPositionR += vuVelocityR * dt;
        if (vuPositionR < 0.0f) { vuPositionR = 0.0f; vuVelocityR = 0.0f; }
        if (vuPositionR > 1.25f) { vuPositionR = 1.25f; vuVelocityR = 0.0f; }
    }

    MeterSnapshot LevelMeterProcessor::getSnapshot() const
    {
        MeterSnapshot snap;
        snap.peakL = blockPeakL.load(std::memory_order_relaxed);
        snap.peakR = blockPeakR.load(std::memory_order_relaxed);
        snap.peakDbL = currentPeakDbL;
        snap.peakDbR = currentPeakDbR;

        snap.rmsL = blockRmsL.load(std::memory_order_relaxed);
        snap.rmsR = blockRmsR.load(std::memory_order_relaxed);
        snap.rmsDbL = currentRmsDbL;
        snap.rmsDbR = currentRmsDbR;

        snap.peakHoldDbL = peakHoldDbL;
        snap.peakHoldDbR = peakHoldDbR;

        snap.vuLevelL = normalizedToVu(vuPositionL);
        snap.vuLevelR = normalizedToVu(vuPositionR);

        snap.vuNormalizedL = juce::jlimit(0.0f, 1.2f, vuPositionL);
        snap.vuNormalizedR = juce::jlimit(0.0f, 1.2f, vuPositionR);

        snap.clippedL = clipFlagL.load(std::memory_order_relaxed);
        snap.clippedR = clipFlagR.load(std::memory_order_relaxed);

        return snap;
    }

    void LevelMeterProcessor::resetPeakHold()
    {
        peakHoldDbL = -100.0f;
        peakHoldDbR = -100.0f;
        peakHoldTimerL = 0.0f;
        peakHoldTimerR = 0.0f;
    }

    void LevelMeterProcessor::resetClip()
    {
        clipFlagL.store(false, std::memory_order_relaxed);
        clipFlagR.store(false, std::memory_order_relaxed);
    }

    float LevelMeterProcessor::linearToDb(float linearValue, float minDb)
    {
        if (std::isnan(linearValue) || std::isinf(linearValue) || linearValue <= 0.00001f)
            return minDb;

        const float db = 20.0f * std::log10(linearValue);
        return juce::jlimit(minDb, 12.0f, db);
    }

    float LevelMeterProcessor::dbToLinear(float dbValue)
    {
        if (std::isnan(dbValue) || std::isinf(dbValue) || dbValue <= -100.0f)
            return 0.0f;

        return std::pow(10.0f, dbValue * 0.05f);
    }

    float LevelMeterProcessor::vuToNormalized(float vuVal)
    {
        // Standard non-linear scale points of a standard VU meter dial:
        // -20 VU -> 0.00
        // -10 VU -> 0.22
        //  -7 VU -> 0.35
        //  -5 VU -> 0.46
        //  -3 VU -> 0.58
        //  -2 VU -> 0.64
        //  -1 VU -> 0.70
        //   0 VU -> 0.76 (Demarcation mark)
        //  +1 VU -> 0.83
        //  +2 VU -> 0.91
        //  +3 VU -> 1.00

        struct VUPair { float vu; float norm; };
        static const VUPair points[] = {
            { -30.0f, 0.00f },
            { -20.0f, 0.00f },
            { -10.0f, 0.22f },
            {  -7.0f, 0.35f },
            {  -5.0f, 0.46f },
            {  -3.0f, 0.58f },
            {  -2.0f, 0.64f },
            {  -1.0f, 0.70f },
            {   0.0f, 0.76f },
            {  +1.0f, 0.83f },
            {  +2.0f, 0.91f },
            {  +3.0f, 1.00f },
            {  +6.0f, 1.15f }
        };
        const int numPoints = sizeof(points) / sizeof(points[0]);

        if (vuVal <= points[0].vu)
            return 0.0f;
        if (vuVal >= points[numPoints - 1].vu)
            return points[numPoints - 1].norm;

        for (int i = 0; i < numPoints - 1; ++i)
        {
            if (vuVal >= points[i].vu && vuVal <= points[i + 1].vu)
            {
                const float frac = (vuVal - points[i].vu) / (points[i + 1].vu - points[i].vu);
                return points[i].norm + frac * (points[i + 1].norm - points[i].norm);
            }
        }

        return 0.76f;
    }

    float LevelMeterProcessor::normalizedToVu(float norm)
    {
        struct VUPair { float vu; float norm; };
        static const VUPair points[] = {
            { -30.0f, 0.00f },
            { -20.0f, 0.00f },
            { -10.0f, 0.22f },
            {  -7.0f, 0.35f },
            {  -5.0f, 0.46f },
            {  -3.0f, 0.58f },
            {  -2.0f, 0.64f },
            {  -1.0f, 0.70f },
            {   0.0f, 0.76f },
            {  +1.0f, 0.83f },
            {  +2.0f, 0.91f },
            {  +3.0f, 1.00f },
            {  +6.0f, 1.15f }
        };
        const int numPoints = sizeof(points) / sizeof(points[0]);

        if (norm <= points[0].norm)
            return points[0].vu;
        if (norm >= points[numPoints - 1].norm)
            return points[numPoints - 1].vu;

        for (int i = 0; i < numPoints - 1; ++i)
        {
            if (norm >= points[i].norm && norm <= points[i + 1].norm)
            {
                const float frac = (norm - points[i].norm) / (points[i + 1].norm - points[i].norm);
                return points[i].vu + frac * (points[i + 1].vu - points[i].vu);
            }
        }

        return 0.0f;
    }
}
