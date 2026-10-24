#include "EightBandEQ.h"

namespace LiveBus
{
    EightBandEQProcessor::EightBandEQProcessor()
    {
        updateCoefficients();
    }

    void EightBandEQProcessor::prepare(double sampleRate, int maxBlockSize)
    {
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        currentBlockSize = juce::jmax(1, maxBlockSize);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate;
        spec.maximumBlockSize = (juce::uint32)currentBlockSize;
        spec.numChannels = 1;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int b = 0; b < numEQBands; ++b)
            {
                filters[ch][b].prepare(spec);
                filters[ch][b].reset();
            }
        }

        updateCoefficients();

        meterInL.store(0.0f);
        meterInR.store(0.0f);
        meterOutL.store(0.0f);
        meterOutR.store(0.0f);
        analyzerFifo.reset();
        analyzerBuffer.fill(0.0f);
    }

    void EightBandEQProcessor::reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int b = 0; b < numEQBands; ++b)
            {
                filters[ch][b].reset();
            }
        }
        meterInL.store(0.0f);
        meterInR.store(0.0f);
        meterOutL.store(0.0f);
        meterOutR.store(0.0f);
        analyzerFifo.reset();
        analyzerBuffer.fill(0.0f);
    }

    float EightBandEQProcessor::validateFrequency(float freq, double sampleRate)
    {
        if (!std::isfinite(freq))
            return 1000.0f;

        const float minF = 20.0f;
        const float maxF = (sampleRate > 0.0)
            ? (float)juce::jmin(20000.0, sampleRate * 0.495)
            : 20000.0f;

        return juce::jlimit(minF, maxF, freq);
    }

    float EightBandEQProcessor::validateGain(float gainDb)
    {
        if (!std::isfinite(gainDb))
            return 0.0f;

        return juce::jlimit(-18.0f, 18.0f, gainDb);
    }

    float EightBandEQProcessor::validateQ(float q)
    {
        if (!std::isfinite(q))
            return 0.707f;

        return juce::jlimit(0.1f, 10.0f, q);
    }

    void EightBandEQProcessor::updateCoefficients()
    {
        const double sr = (currentSampleRate > 0.0) ? currentSampleRate : 48000.0;

        for (int b = 0; b < numEQBands; ++b)
        {
            const auto type = params.bands[b].type.load(std::memory_order_relaxed);
            const float rawFreq = params.bands[b].freq.load(std::memory_order_relaxed);
            const float rawGain = params.bands[b].gainDb.load(std::memory_order_relaxed);
            const float rawQ = params.bands[b].q.load(std::memory_order_relaxed);

            const float f = validateFrequency(rawFreq, sr);
            const float g = validateGain(rawGain);
            const float q = validateQ(rawQ);
            const float gainFactor = juce::Decibels::decibelsToGain(g);

            juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> newCoeff;

            switch (type)
            {
                case EQFilterType::HighPass:
                    newCoeff = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, f, q);
                    break;

                case EQFilterType::LowPass:
                    newCoeff = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, f, q);
                    break;

                case EQFilterType::LowShelf:
                    newCoeff = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, f, q, gainFactor);
                    break;

                case EQFilterType::HighShelf:
                    newCoeff = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, f, q, gainFactor);
                    break;

                case EQFilterType::Bell:
                default:
                    newCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, f, q, gainFactor);
                    break;
            }

            if (newCoeff)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    filters[ch][b].coefficients = newCoeff;
                }
            }
        }
    }

    void EightBandEQProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        juce::ScopedNoDenormals noDenormals;

        if (numSamples <= 0 || buffer.getNumChannels() < 1)
            return;

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // 1. Measure Input Peak Levels
        const float inL = buffer.getMagnitude(0, 0, numSamples);
        const float inR = (numChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : inL;
        meterInL.store(inL, std::memory_order_relaxed);
        meterInR.store(inR, std::memory_order_relaxed);

        // Pre-EQ Spectrum Analyzer Feed
        if (analyzerEnabled.load(std::memory_order_relaxed) && !analyzerPost.load(std::memory_order_relaxed))
        {
            pushToAnalyzer(buffer.getReadPointer(0), numSamples);
        }

        // 2. Master Bypass Check
        if (!params.masterEnabled.load(std::memory_order_relaxed))
        {
            // Bit-transparent dry audio pass-through
            meterOutL.store(inL, std::memory_order_relaxed);
            meterOutR.store(inR, std::memory_order_relaxed);

            if (analyzerEnabled.load(std::memory_order_relaxed) && analyzerPost.load(std::memory_order_relaxed))
            {
                pushToAnalyzer(buffer.getReadPointer(0), numSamples);
            }
            return;
        }

        // 3. Process Active Bands
        juce::dsp::AudioBlock<float> block(buffer);

        for (int b = 0; b < numEQBands; ++b)
        {
            if (!params.bands[b].enabled.load(std::memory_order_relaxed))
                continue; // Band is individually bypassed

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto singleBlock = block.getSingleChannelBlock((size_t)ch).getSubBlock(0, (size_t)numSamples);
                juce::dsp::ProcessContextReplacing<float> context(singleBlock);
                filters[ch][b].process(context);
            }
        }

        // 4. Measure Output Peak Levels
        const float outL = buffer.getMagnitude(0, 0, numSamples);
        const float outR = (numChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : outL;
        meterOutL.store(outL, std::memory_order_relaxed);
        meterOutR.store(outR, std::memory_order_relaxed);

        // Post-EQ Spectrum Analyzer Feed
        if (analyzerEnabled.load(std::memory_order_relaxed) && analyzerPost.load(std::memory_order_relaxed))
        {
            pushToAnalyzer(buffer.getReadPointer(0), numSamples);
        }
    }

    float EightBandEQProcessor::getMagnitudeForFrequency(float frequency) const
    {
        if (!params.masterEnabled.load(std::memory_order_relaxed))
            return 0.0f; // 0 dB flat line when bypassed

        double totalMag = 1.0;
        const double sr = (currentSampleRate > 0.0) ? currentSampleRate : 48000.0;

        for (int b = 0; b < numEQBands; ++b)
        {
            if (params.bands[b].enabled.load(std::memory_order_relaxed) && filters[0][b].coefficients)
            {
                totalMag *= filters[0][b].coefficients->getMagnitudeForFrequency((double)frequency, sr);
            }
        }

        return (float)juce::Decibels::gainToDecibels(totalMag, -60.0);
    }

    float EightBandEQProcessor::getBandMagnitudeForFrequency(int bandIndex, float frequency) const
    {
        if (bandIndex < 0 || bandIndex >= numEQBands)
            return 0.0f;

        if (!params.bands[bandIndex].enabled.load(std::memory_order_relaxed))
            return 0.0f;

        const double sr = (currentSampleRate > 0.0) ? currentSampleRate : 48000.0;
        if (filters[0][bandIndex].coefficients)
        {
            double mag = filters[0][bandIndex].coefficients->getMagnitudeForFrequency((double)frequency, sr);
            return (float)juce::Decibels::gainToDecibels(mag, -60.0);
        }

        return 0.0f;
    }

    void EightBandEQProcessor::pushToAnalyzer(const float* channelData, int numSamples)
    {
        if (channelData == nullptr || numSamples <= 0)
            return;

        int start1, size1, start2, size2;
        analyzerFifo.prepareToWrite(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            std::memcpy(analyzerBuffer.data() + start1, channelData, (size_t)size1 * sizeof(float));

        if (size2 > 0)
            std::memcpy(analyzerBuffer.data() + start2, channelData + size1, (size_t)size2 * sizeof(float));

        analyzerFifo.finishedWrite(size1 + size2);
    }

    bool EightBandEQProcessor::pullFromAnalyzer(float* destBuffer, int numSamples)
    {
        if (destBuffer == nullptr || numSamples <= 0)
            return false;

        if (analyzerFifo.getNumReady() < numSamples)
            return false;

        int start1, size1, start2, size2;
        analyzerFifo.prepareToRead(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            std::memcpy(destBuffer, analyzerBuffer.data() + start1, (size_t)size1 * sizeof(float));

        if (size2 > 0)
            std::memcpy(destBuffer + size1, analyzerBuffer.data() + start2, (size_t)size2 * sizeof(float));

        analyzerFifo.finishedRead(size1 + size2);
        return true;
    }
}
