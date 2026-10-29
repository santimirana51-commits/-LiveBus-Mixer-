#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
 * Autocorrelation-based monophonic vocal pitch detector with parabolic interpolation.
 * Designed for real-time safety, zero heap allocations in audio loop, and pitch confidence estimation.
 */
class VocalPitchDetector
{
public:
    VocalPitchDetector() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        // Search range: 65 Hz (C2 - low male) to 1050 Hz (C6 - high female)
        minPeriod = juce::jlimit(2, 4000, (int)(sr / 1050.0));
        maxPeriod = juce::jlimit(4, 4000, (int)(sr / 65.0));

        bufferSize = maxPeriod * 3;
        ringBuffer.assign((size_t)bufferSize, 0.0f);
        writeIndex = 0;
        lastDetectedHz = 0.0f;
        lastConfidence = 0.0f;
        smoothedHz = 0.0f;
        // Pre-allocate NSDF working buffer
        nsdfBuffer.assign((size_t)(maxPeriod + 1), 0.0f);
    }

    void reset()
    {
        std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
        std::fill(nsdfBuffer.begin(), nsdfBuffer.end(), 0.0f);
        writeIndex = 0;
        lastDetectedHz = 0.0f;
        lastConfidence = 0.0f;
        smoothedHz = 0.0f;
    }

    /**
     * Process incoming samples and estimate fundamental frequency (f0).
     * @param audio Mono buffer samples
     * @param numSamples Number of samples
     * @return Detected pitch in Hz, or 0.0f if unvoiced/silent
     */
    float processBlock(const float* audio, int numSamples)
    {
        if (numSamples <= 0 || audio == nullptr || sr <= 0.0)
            return 0.0f;

        // Push into ring buffer & calculate RMS
        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float s = audio[i];
            sumSquares += s * s;
            ringBuffer[(size_t)writeIndex] = s;
            writeIndex = (writeIndex + 1) % bufferSize;
        }

        float rms = std::sqrt(sumSquares / (float)numSamples);
        // Silence or low noise floor threshold (-45 dBFS ~= 0.0056)
        if (rms < 0.005f)
        {
            lastConfidence = 0.0f;
            lastDetectedHz = 0.0f;
            return 0.0f;
        }

        // Compute normalized square difference function (NSDF) / autocorrelation
        // over [minPeriod, maxPeriod]
        const int corrLen = maxPeriod;

        // Precompute energy at zero lag
        float energyZero = 0.0f;
        for (int i = 0; i < corrLen; ++i)
        {
            int idx = (writeIndex - 1 - i + bufferSize * 4) % bufferSize;
            float s = ringBuffer[(size_t)idx];
            energyZero += s * s;
        }

        if (energyZero < 0.0001f)
            return 0.0f;

        // Compute NSDF over [minPeriod, maxPeriod] without heap allocation
        if ((int)nsdfBuffer.size() < maxPeriod + 1)
            nsdfBuffer.assign((size_t)(maxPeriod + 1), 0.0f);

        for (int lag = minPeriod; lag <= maxPeriod; ++lag)
        {
            float sum = 0.0f;
            float energyLag = 0.0f;

            for (int i = 0; i < corrLen; ++i)
            {
                int idx0 = (writeIndex - 1 - i + bufferSize * 4) % bufferSize;
                int idxLag = (writeIndex - 1 - i - lag + bufferSize * 4) % bufferSize;
                float s0 = ringBuffer[(size_t)idx0];
                float sLag = ringBuffer[(size_t)idxLag];
                sum += s0 * sLag;
                energyLag += sLag * sLag;
            }

            float denom = std::sqrt(energyZero * energyLag);
            nsdfBuffer[(size_t)lag] = (denom > 1e-6f) ? (sum / denom) : 0.0f;
        }

        // Peak picking: Find the first major peak (fundamental f0) with threshold = 0.8 * maxPeak
        float maxVal = -1.0f;
        for (int lag = minPeriod; lag <= maxPeriod; ++lag)
            if (nsdfBuffer[(size_t)lag] > maxVal) maxVal = nsdfBuffer[(size_t)lag];

        if (maxVal < 0.5f)
        {
            lastConfidence = 0.0f;
            lastDetectedHz = 0.0f;
            return 0.0f;
        }

        // McLeod Pitch Method (MPM) peak picking:
        // Find all local maxima above threshold (0.85 * maxVal), then pick the FIRST one
        // to reliably select the fundamental period rather than harmonic multiples.
        float threshold = 0.85f * maxVal;
        int bestLag = -1;
        float bestCorr = -1.0f;

        for (int lag = minPeriod + 1; lag < maxPeriod; ++lag)
        {
            if (nsdfBuffer[(size_t)lag] >= threshold &&
                nsdfBuffer[(size_t)lag] >= nsdfBuffer[(size_t)(lag - 1)] &&
                nsdfBuffer[(size_t)lag] >= nsdfBuffer[(size_t)(lag + 1)])
            {
                bestLag = lag;
                bestCorr = nsdfBuffer[(size_t)lag];
                break; // Pick the earliest prominent period (fundamental f0)
            }
        }

        // Fallback to highest peak if no local peak met criteria
        if (bestLag <= 0)
        {
            for (int lag = minPeriod; lag <= maxPeriod; ++lag)
            {
                if (nsdfBuffer[(size_t)lag] > bestCorr)
                {
                    bestCorr = nsdfBuffer[(size_t)lag];
                    bestLag = lag;
                }
            }
        }

        lastConfidence = juce::jlimit(0.0f, 1.0f, bestCorr);
        if (bestCorr < 0.5f || bestLag <= 0)
        {
            lastDetectedHz = 0.0f;
            return 0.0f;
        }

        // Parabolic interpolation around peak using already computed NSDF points
        float fineLag = (float)bestLag;
        if (bestLag > minPeriod && bestLag < maxPeriod)
        {
            float y1 = nsdfBuffer[(size_t)(bestLag - 1)];
            float y2 = bestCorr;
            float y3 = nsdfBuffer[(size_t)(bestLag + 1)];

            float denom = (2.0f * (2.0f * y2 - y1 - y3));
            if (std::abs(denom) > 1e-5f)
            {
                float delta = (y3 - y1) / denom;
                fineLag += juce::jlimit(-0.5f, 0.5f, delta);
            }
        }

        float hz = (float)(sr / (double)fineLag);
        lastDetectedHz = hz;
        return lastDetectedHz;
    }

    float getDetectedHz() const noexcept { return lastDetectedHz; }
    float getConfidence() const noexcept { return lastConfidence; }

private:
    double sr = 44100.0;
    int minPeriod = 42;
    int maxPeriod = 678;
    int bufferSize = 1356;
    std::vector<float> ringBuffer;
    std::vector<float> nsdfBuffer;
    int writeIndex = 0;
    float lastDetectedHz = 0.0f;
    float lastConfidence = 0.0f;
    float smoothedHz = 0.0f;
};
