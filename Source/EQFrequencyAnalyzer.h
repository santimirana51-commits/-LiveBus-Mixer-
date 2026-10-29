#pragma once

#include <array>
#include <cmath>
#include <cstddef>

struct EQFrequencyPeakCandidate
{
    float frequencyHz = 0.0f;
    float levelDb = -90.0f;
    float prominenceDb = 0.0f;
};

struct EQFrequencyPeakAnalysis
{
    std::array<EQFrequencyPeakCandidate, 3> candidates{};
    std::size_t count = 0;
};

inline int getEQAnalyzerFFTSize(int selectionId) noexcept
{
    switch (selectionId)
    {
        case 1: return 1024;
        case 3: return 4096;
        case 2:
        default: return 2048;
    }
}

template <std::size_t NumBins>
void updateEQSpectrumAverage(
    const std::array<float, NumBins>& currentSpectrumDb,
    std::array<float, NumBins>& averagedSpectrumDb,
    bool hasPreviousSpectrum,
    float elapsedSeconds,
    float averagingTimeSeconds)
{
    const float elapsed = std::isfinite(elapsedSeconds)
        ? std::fmax(0.0f, elapsedSeconds) : 0.0f;
    const float averagingTime = std::isfinite(averagingTimeSeconds)
        ? std::fmax(0.001f, averagingTimeSeconds) : 0.25f;
    const float alpha = 1.0f - std::exp(-elapsed / averagingTime);

    for (std::size_t bin = 0; bin < NumBins; ++bin)
    {
        const float current = std::isfinite(currentSpectrumDb[bin])
            ? currentSpectrumDb[bin] : -90.0f;
        const float previous = std::isfinite(averagedSpectrumDb[bin])
            ? averagedSpectrumDb[bin] : -90.0f;
        averagedSpectrumDb[bin] = hasPreviousSpectrum
            ? previous + alpha * (current - previous)
            : current;
    }
}

template <std::size_t NumBins>
void updateEQSpectrumPeakHold(
    const std::array<float, NumBins>& spectrumDb,
    std::array<float, NumBins>& peakHoldDb,
    float decayDbPerSecond,
    float elapsedSeconds)
{
    const float decayDb = std::isfinite(decayDbPerSecond)
        ? std::fmax(0.0f, decayDbPerSecond) *
              (std::isfinite(elapsedSeconds) ? std::fmax(0.0f, elapsedSeconds) : 0.0f)
        : 0.0f;
    for (std::size_t bin = 0; bin < NumBins; ++bin)
    {
        const float current = std::isfinite(spectrumDb[bin])
            ? spectrumDb[bin] : -90.0f;
        const float held = std::isfinite(peakHoldDb[bin])
            ? peakHoldDb[bin] : -90.0f;
        peakHoldDb[bin] = std::fmax(current, held - decayDb);
    }
}

template <std::size_t NumBins>
EQFrequencyPeakAnalysis findEQFrequencyPeakCandidates(
    const std::array<float, NumBins>& spectrumDb,
    float minimumFrequencyHz,
    float maximumFrequencyHz,
    float minimumLevelDb = -65.0f,
    float minimumProminenceDb = 4.0f)
{
    EQFrequencyPeakAnalysis result;
    constexpr std::size_t comparisonRadius = 5;
    if constexpr (NumBins <= comparisonRadius * 2)
        return result;

    if (!std::isfinite(minimumFrequencyHz) ||
        !std::isfinite(maximumFrequencyHz) ||
        minimumFrequencyHz <= 0.0f ||
        maximumFrequencyHz <= minimumFrequencyHz ||
        !std::isfinite(minimumLevelDb) ||
        !std::isfinite(minimumProminenceDb))
        return result;

    for (std::size_t bin = comparisonRadius;
         bin < NumBins - comparisonRadius; ++bin)
    {
        const float level = spectrumDb[bin];
        if (!std::isfinite(level) || level < minimumLevelDb ||
            level <= spectrumDb[bin - 1] || level < spectrumDb[bin + 1])
            continue;

        const float sideLevel =
            (spectrumDb[bin - comparisonRadius] +
             spectrumDb[bin - comparisonRadius + 1] +
             spectrumDb[bin + comparisonRadius - 1] +
             spectrumDb[bin + comparisonRadius]) * 0.25f;
        const float prominence = level - sideLevel;
        if (!std::isfinite(prominence) || prominence < minimumProminenceDb)
            continue;

        const float proportion = static_cast<float>(bin) /
                                 static_cast<float>(NumBins - 1);
        const float frequencyHz = minimumFrequencyHz *
            std::pow(maximumFrequencyHz / minimumFrequencyHz, proportion);

        bool tooClose = false;
        for (std::size_t index = 0; index < result.count; ++index)
        {
            const float ratio = frequencyHz /
                                result.candidates[index].frequencyHz;
            if (ratio > 0.9f && ratio < 1.1f)
            {
                tooClose = true;
                break;
            }
        }
        if (tooClose)
            continue;

        std::size_t insertionIndex = 0;
        while (insertionIndex < result.count &&
               result.candidates[insertionIndex].prominenceDb >= prominence)
            ++insertionIndex;
        if (insertionIndex >= result.candidates.size())
            continue;

        const std::size_t newCount =
            result.count < result.candidates.size()
                ? result.count + 1
                : result.count;
        for (std::size_t index = newCount - 1;
             index > insertionIndex; --index)
            result.candidates[index] = result.candidates[index - 1];
        result.candidates[insertionIndex] = { frequencyHz, level, prominence };
        result.count = newCount;
    }

    return result;
}
