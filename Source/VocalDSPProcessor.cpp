#include "VocalDSPProcessor.h"
#include <algorithm>

namespace LiveBus
{
    VocalDSPProcessor::VocalDSPProcessor()
    {
        smoothedInputGain.setCurrentAndTargetValue(1.0f);
        smoothedOutputGain.setCurrentAndTargetValue(1.0f);
    }

    void VocalDSPProcessor::prepare(double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        blockSize = juce::jmax(1, maxBlockSize);

        if (eqProcessor != nullptr)
            eqProcessor->prepare(sampleRate, blockSize);

        compDryBuffer.setSize(2, blockSize);
        compDryBuffer.clear();

        smoothedInputGain.reset(sampleRate, 0.02);
        smoothedInputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(params.inputGainDb.load(std::memory_order_relaxed)));

        smoothedOutputGain.reset(sampleRate, 0.02);
        smoothedOutputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(params.outputGainDb.load(std::memory_order_relaxed)));

        reset();
        updateHPFCoefficients();
        updateGateKeyCoefficients();
        updateCompKeyCoefficients();
        updateDeEsserCoefficients();
    }

    void VocalDSPProcessor::reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hpfStage1[ch].reset();
            hpfStage2[ch].reset();

            gateState[ch] = StateClosed;
            gateGain[ch] = 0.001f;
            gateHoldCount[ch] = 0;
            gateKeyHpf[ch].reset();
            gateKeyLpf[ch].reset();

            deEsserSidechainFilter[ch].reset();
            deEsserBellFilter[ch].reset();
            deEsserGainEnv[ch] = 1.0f;

            compKeyHpf[ch].reset();
            compKeyLpf[ch].reset();
            compGainEnv[ch] = 1.0f;
            autoReleaseMs[ch] = 120.0f;

            limiterEnvelope[ch] = 0.0f;
            gateKeyEnvelope[ch] = 0.0f;
            compKeyEnvelope[ch] = 0.0f;
            deEsserSidechainEnvelope[ch] = 0.0f;
        }

        if (eqProcessor != nullptr)
            eqProcessor->reset();
        gateGR.store(0.0f, std::memory_order_relaxed);
        compGR.store(0.0f, std::memory_order_relaxed);
        deEsserGR.store(0.0f, std::memory_order_relaxed);
        limiterGR.store(0.0f, std::memory_order_relaxed);
        inPeakDb.store(-100.0f, std::memory_order_relaxed);
        outPeakDb.store(-100.0f, std::memory_order_relaxed);
        clipFlag.store(false, std::memory_order_relaxed);
    }

    void VocalDSPProcessor::updateHPFCoefficients()
    {
        const float freq = juce::jlimit(20.0f, 500.0f, params.hpfFreq.load(std::memory_order_relaxed));
        const int slope = params.hpfSlope.load(std::memory_order_relaxed);

        if (std::abs(freq - lastHpfFreq) > 0.5f || slope != lastHpfSlope)
        {
            lastHpfFreq = freq;
            lastHpfSlope = slope;

            auto c1 = (slope == 0)
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freq, 0.707f);
            auto c2 = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freq, (slope >= 3) ? 1.3065f : 0.707f);

            for (int ch = 0; ch < 2; ++ch)
            {
                *hpfStage1[ch].coefficients = *c1;
                *hpfStage2[ch].coefficients = *c2;
            }
        }
    }

    void VocalDSPProcessor::updateGateKeyCoefficients()
    {
        const float hpfF = juce::jlimit(20.0f, 1000.0f, params.gateKeyHpfFreq.load(std::memory_order_relaxed));
        const float lpfF = juce::jlimit(200.0f, 20000.0f, params.gateKeyLpfFreq.load(std::memory_order_relaxed));

        auto hpfC = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpfF, 0.707f);
        auto lpfC = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpfF, 0.707f);

        for (int ch = 0; ch < 2; ++ch)
        {
            *gateKeyHpf[ch].coefficients = *hpfC;
            *gateKeyLpf[ch].coefficients = *lpfC;
        }
    }

    void VocalDSPProcessor::updateCompKeyCoefficients()
    {
        const float hpfF = juce::jlimit(20.0f, 500.0f, params.compKeyHpfFreq.load(std::memory_order_relaxed));
        const float lpfF = juce::jlimit(2000.0f, 20000.0f, params.compKeyLpfFreq.load(std::memory_order_relaxed));

        auto hpfC = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpfF, 0.707f);
        auto lpfC = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpfF, 0.707f);

        for (int ch = 0; ch < 2; ++ch)
        {
            *compKeyHpf[ch].coefficients = *hpfC;
            *compKeyLpf[ch].coefficients = *lpfC;
        }
    }

    void VocalDSPProcessor::updateDeEsserCoefficients()
    {
        const float freq = juce::jlimit(1000.0f, 12000.0f, params.deEsserFreq.load(std::memory_order_relaxed));
        const int type = params.deEsserType.load(std::memory_order_relaxed);

        if (std::abs(freq - lastDeEsserFreq) > 1.0f || type != lastDeEsserType)
        {
            lastDeEsserFreq = freq;
            lastDeEsserType = type;

            // Detector sidechain: Band-pass centered at sibilance frequency
            auto sidechainCoeff = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, freq, 2.0f);
            for (int ch = 0; ch < 2; ++ch)
            {
                *deEsserSidechainFilter[ch].coefficients = *sidechainCoeff;
            }
        }
    }

    void VocalDSPProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        juce::ScopedNoDenormals noDenormals;

        if (numSamples <= 0 || buffer.getNumChannels() < 1)
            return;

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
        float inDb = (maxIn > 0.00001f) ? 20.0f * std::log10(maxIn) : -100.0f;
        inPeakDb.store(inDb, std::memory_order_relaxed);

        // If globally bypassed, pass through untouched
        if (!params.masterEnabled.load(std::memory_order_relaxed))
        {
            gateGR.store(0.0f, std::memory_order_relaxed);
            compGR.store(0.0f, std::memory_order_relaxed);
            deEsserGR.store(0.0f, std::memory_order_relaxed);
            limiterGR.store(0.0f, std::memory_order_relaxed);
            outPeakDb.store(inDb, std::memory_order_relaxed);
            return;
        }

        // Apply input gain
        smoothedInputGain.setTargetValue(juce::Decibels::decibelsToGain(params.inputGainDb.load(std::memory_order_relaxed)));
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] *= smoothedInputGain.getNextValue();
        }

        // Chain Execution:
        // 1. High-Pass Filter
        if (!params.hpfBypassed.load(std::memory_order_relaxed))
            processHPF(buffer, numSamples);

        // 2. Noise Gate / Expander
        if (!params.gateBypassed.load(std::memory_order_relaxed))
            processGate(buffer, numSamples);
        else
            gateGR.store(0.0f, std::memory_order_relaxed);

        // 3. 8-Band Parametric EQ
        if (!params.eqBypassed.load(std::memory_order_relaxed) && eqProcessor != nullptr)
            eqProcessor->process(buffer, numSamples);

        // 4. De-Esser
        if (!params.deEsserBypassed.load(std::memory_order_relaxed))
            processDeEsser(buffer, numSamples);
        else
            deEsserGR.store(0.0f, std::memory_order_relaxed);

        // 5. Compressor
        if (!params.compBypassed.load(std::memory_order_relaxed))
            processCompressor(buffer, numSamples);
        else
            compGR.store(0.0f, std::memory_order_relaxed);

        // 6. Limiter
        if (!params.limiterBypassed.load(std::memory_order_relaxed))
            processLimiter(buffer, numSamples);
        else
            limiterGR.store(0.0f, std::memory_order_relaxed);

        // Apply output gain
        smoothedOutputGain.setTargetValue(juce::Decibels::decibelsToGain(params.outputGainDb.load(std::memory_order_relaxed)));
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] *= smoothedOutputGain.getNextValue();
        }

        // Measure output peak & clip
        float maxOut = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float m = buffer.getMagnitude(ch, 0, numSamples);
            if (m > maxOut) maxOut = m;
        }
        float outDb = (maxOut > 0.00001f) ? 20.0f * std::log10(maxOut) : -100.0f;
        outPeakDb.store(outDb, std::memory_order_relaxed);

        if (maxOut >= 0.999f)
            clipFlag.store(true, std::memory_order_relaxed);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (!std::isfinite(data[i]))
                    data[i] = 0.0f;
            }
        }
    }

    void VocalDSPProcessor::processHPF(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        updateHPFCoefficients();

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const int slope = params.hpfSlope.load(std::memory_order_relaxed);

        juce::dsp::AudioBlock<float> block(buffer);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto singleBlock = block.getSingleChannelBlock((size_t)ch).getSubBlock(0, (size_t)numSamples);
            juce::dsp::ProcessContextReplacing<float> context(singleBlock);

            hpfStage1[ch].process(context);
            if (slope >= 2)
                hpfStage2[ch].process(context);
        }
    }

    void VocalDSPProcessor::processGate(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        updateGateKeyCoefficients();

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const bool isExpander = (params.gateMode.load(std::memory_order_relaxed) == VocalDSPParameters::ModeExpander);
        const float openThresh = params.gateOpenThresholdDb.load(std::memory_order_relaxed);
        const float closeThresh = params.gateCloseThresholdDb.load(std::memory_order_relaxed);
        const float floorDb = params.gateFloorDb.load(std::memory_order_relaxed);
        const float floorLinear = (floorDb <= -79.0f) ? 0.00001f : juce::Decibels::decibelsToGain(floorDb);

        const float attMs = juce::jmax(0.1f, params.gateAttackMs.load(std::memory_order_relaxed));
        const float relMs = juce::jmax(1.0f, params.gateReleaseMs.load(std::memory_order_relaxed));
        const float holdMs = juce::jmax(0.0f, params.gateHoldMs.load(std::memory_order_relaxed));

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * attMs * 0.001));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * relMs * 0.001));
        const int holdSamples = (int)(sampleRate * holdMs * 0.001);

        float minGainInBlock = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // Detector signal: max of filtered L and R key
            float keyL = buffer.getSample(0, i);
            float keyR = (numChannels > 1) ? buffer.getSample(1, i) : keyL;

            if (params.gateKeyHpfFreq.load(std::memory_order_relaxed) > 22.0f)
            {
                keyL = gateKeyHpf[0].processSample(keyL);
                keyR = gateKeyHpf[1].processSample(keyR);
            }
            if (params.gateKeyLpfFreq.load(std::memory_order_relaxed) < 19500.0f)
            {
                keyL = gateKeyLpf[0].processSample(keyL);
                keyR = gateKeyLpf[1].processSample(keyR);
            }

            const float keyLevelRaw = std::max(std::abs(keyL), std::abs(keyR));
            const float envAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.0005));
            const float envRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.030));
            if (keyLevelRaw > gateKeyEnvelope[0])
                gateKeyEnvelope[0] += envAtt * (keyLevelRaw - gateKeyEnvelope[0]);
            else
                gateKeyEnvelope[0] += envRel * (keyLevelRaw - gateKeyEnvelope[0]);

            const float keyLevelDb = (gateKeyEnvelope[0] > 0.00001f) ? 20.0f * std::log10(gateKeyEnvelope[0]) : -100.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Gate state machine with hysteresis & hold
                switch (gateState[ch])
                {
                    case StateClosed:
                        if (keyLevelDb >= openThresh)
                            gateState[ch] = StateAttack;
                        break;

                    case StateAttack:
                        gateGain[ch] += alphaAtt * (1.0f - gateGain[ch]);
                        if (gateGain[ch] >= 0.99f)
                        {
                            gateGain[ch] = 1.0f;
                            gateState[ch] = StateOpen;
                        }
                        break;

                    case StateOpen:
                        if (keyLevelDb < closeThresh)
                        {
                            gateHoldCount[ch] = holdSamples;
                            gateState[ch] = StateHold;
                        }
                        break;

                    case StateHold:
                        if (keyLevelDb >= openThresh)
                        {
                            gateState[ch] = StateOpen;
                        }
                        else if (--gateHoldCount[ch] <= 0)
                        {
                            gateState[ch] = StateRelease;
                        }
                        break;

                    case StateRelease:
                        if (keyLevelDb >= openThresh)
                        {
                            gateState[ch] = StateAttack;
                        }
                        else
                        {
                            float target = floorLinear;
                            if (isExpander && keyLevelDb > floorDb && keyLevelDb < closeThresh)
                            {
                                // Expander 1:2 downward expansion
                                float diff = closeThresh - keyLevelDb;
                                target = juce::Decibels::decibelsToGain(closeThresh - diff * 2.0f);
                                target = std::max(floorLinear, target);
                            }
                            gateGain[ch] += alphaRel * (target - gateGain[ch]);
                            if (gateGain[ch] <= floorLinear * 1.01f)
                            {
                                gateGain[ch] = floorLinear;
                                gateState[ch] = StateClosed;
                            }
                        }
                        break;
                }

                float sample = buffer.getSample(ch, i);
                buffer.setSample(ch, i, sample * gateGain[ch]);

                if (gateGain[ch] < minGainInBlock)
                    minGainInBlock = gateGain[ch];
            }
        }

        // Calculate and publish gate gain reduction in dB
        float grDb = (minGainInBlock > 0.00001f) ? 20.0f * std::log10(minGainInBlock) : -60.0f;
        gateGR.store(std::min(0.0f, grDb), std::memory_order_relaxed);
    }

    void VocalDSPProcessor::processDeEsser(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        updateDeEsserCoefficients();

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const float thresholdDb = params.deEsserThresholdDb.load(std::memory_order_relaxed);
        const float rangeDb = params.deEsserRangeDb.load(std::memory_order_relaxed); // e.g. -8.0 dB
        const int type = params.deEsserType.load(std::memory_order_relaxed);
        const float centerFreq = params.deEsserFreq.load(std::memory_order_relaxed);

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.001));  // Fast 1ms attack
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.040)); // 40ms release

        float maxReductionInBlock = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sibilanceBand[2] = { 0.0f, 0.0f };
            float sibilanceEnergyRaw = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                sibilanceBand[ch] = deEsserSidechainFilter[ch].processSample(buffer.getSample(ch, i));
                sibilanceEnergyRaw = std::max(sibilanceEnergyRaw, std::abs(sibilanceBand[ch]));
            }

            const float envAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.0005));
            const float envRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.030));
            if (sibilanceEnergyRaw > deEsserSidechainEnvelope[0])
                deEsserSidechainEnvelope[0] += envAtt * (sibilanceEnergyRaw - deEsserSidechainEnvelope[0]);
            else
                deEsserSidechainEnvelope[0] += envRel * (sibilanceEnergyRaw - deEsserSidechainEnvelope[0]);

            const float sibilanceDb = (deEsserSidechainEnvelope[0] > 0.00001f) ? 20.0f * std::log10(deEsserSidechainEnvelope[0]) : -100.0f;

            float targetReductionDb = 0.0f;
            if (sibilanceDb > thresholdDb)
            {
                float excess = sibilanceDb - thresholdDb;
                targetReductionDb = std::min(std::abs(rangeDb), excess * 1.5f);
            }

            float targetGainLin = juce::Decibels::decibelsToGain(-targetReductionDb);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (targetGainLin < deEsserGainEnv[ch])
                    deEsserGainEnv[ch] += alphaAtt * (targetGainLin - deEsserGainEnv[ch]);
                else
                    deEsserGainEnv[ch] += alphaRel * (targetGainLin - deEsserGainEnv[ch]);

                float currentGrDb = juce::Decibels::gainToDecibels(deEsserGainEnv[ch], -30.0f);
                if (-currentGrDb > maxReductionInBlock)
                    maxReductionInBlock = -currentGrDb;

                if (type == VocalDSPParameters::TypeBell)
                {
                    float inSample = buffer.getSample(ch, i);
                    float attenuatedSib = sibilanceBand[ch] * deEsserGainEnv[ch];
                    buffer.setSample(ch, i, inSample - sibilanceBand[ch] + attenuatedSib);
                }
                else
                {
                    buffer.setSample(ch, i, buffer.getSample(ch, i) * deEsserGainEnv[ch]);
                }
            }
        }

        deEsserGR.store(-maxReductionInBlock, std::memory_order_relaxed);
    }

    void VocalDSPProcessor::processCompressor(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        updateCompKeyCoefficients();

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const float thresholdDb = params.compThresholdDb.load(std::memory_order_relaxed);
        const float ratio = juce::jmax(1.0f, params.compRatio.load(std::memory_order_relaxed));
        const int kneeType = params.compKnee.load(std::memory_order_relaxed);
        const float kneeWidthDb = (kneeType == VocalDSPParameters::KneeHard) ? 0.0f : ((kneeType == VocalDSPParameters::KneeNormal) ? 6.0f : 12.0f);

        const float attMs = juce::jmax(0.1f, params.compAttackMs.load(std::memory_order_relaxed));
        const float baseRelMs = juce::jmax(1.0f, params.compReleaseMs.load(std::memory_order_relaxed));
        const bool autoRel = (params.compReleaseMode.load(std::memory_order_relaxed) == VocalDSPParameters::ReleaseAuto);

        const float makeupLin = juce::Decibels::decibelsToGain(params.compMakeupDb.load(std::memory_order_relaxed));
        const float mix = juce::jlimit(0.0f, 1.0f, params.compMixPercent.load(std::memory_order_relaxed) * 0.01f);

        // Keep copy of dry buffer for parallel compression
        if (mix < 0.999f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                compDryBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
        }

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * attMs * 0.001));

        float maxReductionInBlock = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // Sidechain level detection with HPF & LPF
            float scL = buffer.getSample(0, i);
            float scR = (numChannels > 1) ? buffer.getSample(1, i) : scL;

            if (params.compKeyHpfFreq.load(std::memory_order_relaxed) > 22.0f)
            {
                scL = compKeyHpf[0].processSample(scL);
                scR = compKeyHpf[1].processSample(scR);
            }
            if (params.compKeyLpfFreq.load(std::memory_order_relaxed) < 19500.0f)
            {
                scL = compKeyLpf[0].processSample(scL);
                scR = compKeyLpf[1].processSample(scR);
            }

            const float scRaw = std::max(std::abs(scL), std::abs(scR));
            const float envAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.0005));
            const float envRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.040));
            if (scRaw > compKeyEnvelope[0])
                compKeyEnvelope[0] += envAtt * (scRaw - compKeyEnvelope[0]);
            else
                compKeyEnvelope[0] += envRel * (scRaw - compKeyEnvelope[0]);

            const float scDb = (compKeyEnvelope[0] > 0.00001f) ? 20.0f * std::log10(compKeyEnvelope[0]) : -100.0f;

            // Gain Computer with smooth knee
            float targetGrDb = 0.0f;
            if (kneeWidthDb > 0.01f)
            {
                if (2.0f * (scDb - thresholdDb) < -kneeWidthDb)
                {
                    targetGrDb = 0.0f;
                }
                else if (2.0f * std::abs(scDb - thresholdDb) <= kneeWidthDb)
                {
                    float d = scDb - thresholdDb + 0.5f * kneeWidthDb;
                    targetGrDb = (d * d) / (2.0f * kneeWidthDb) * (1.0f - 1.0f / ratio);
                }
                else
                {
                    targetGrDb = (scDb - thresholdDb) * (1.0f - 1.0f / ratio);
                }
            }
            else
            {
                if (scDb > thresholdDb)
                    targetGrDb = (scDb - thresholdDb) * (1.0f - 1.0f / ratio);
            }

            const float targetGainLin = juce::Decibels::decibelsToGain(-targetGrDb);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float relMs = baseRelMs;
                if (autoRel)
                {
                    // Auto release: adapt release speed based on gain reduction depth
                    relMs = baseRelMs * (1.0f + 0.5f * (targetGrDb / 10.0f));
                    relMs = juce::jlimit(15.0f, 1500.0f, relMs);
                }
                const float alphaRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * relMs * 0.001));

                if (targetGainLin < compGainEnv[ch])
                    compGainEnv[ch] += alphaAtt * (targetGainLin - compGainEnv[ch]);
                else
                    compGainEnv[ch] += alphaRel * (targetGainLin - compGainEnv[ch]);

                float wetSample = buffer.getSample(ch, i) * compGainEnv[ch] * makeupLin;
                if (mix < 0.999f)
                {
                    float drySample = compDryBuffer.getSample(ch, i);
                    buffer.setSample(ch, i, (1.0f - mix) * drySample + mix * wetSample);
                }
                else
                {
                    buffer.setSample(ch, i, wetSample);
                }

                float currentGr = juce::Decibels::gainToDecibels(compGainEnv[ch], -40.0f);
                if (-currentGr > maxReductionInBlock)
                    maxReductionInBlock = -currentGr;
            }
        }

        compGR.store(-maxReductionInBlock, std::memory_order_relaxed);
    }

    void VocalDSPProcessor::processLimiter(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const float ceilingDb = params.limiterCeilingDb.load(std::memory_order_relaxed);
        const float ceilingLin = juce::Decibels::decibelsToGain(ceilingDb);
        const float relMs = juce::jmax(1.0f, params.limiterReleaseMs.load(std::memory_order_relaxed));

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(sampleRate * 0.0003)); // 0.3ms fast attack
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(sampleRate * relMs * 0.001));

        float maxReductionInBlock = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peakIn = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float absVal = std::abs(buffer.getSample(ch, i));
                if (absVal > peakIn) peakIn = absVal;
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (peakIn > limiterEnvelope[ch])
                    limiterEnvelope[ch] += alphaAtt * (peakIn - limiterEnvelope[ch]);
                else
                    limiterEnvelope[ch] += alphaRel * (peakIn - limiterEnvelope[ch]);

                float gain = 1.0f;
                if (limiterEnvelope[ch] > ceilingLin)
                {
                    gain = ceilingLin / limiterEnvelope[ch];
                }

                buffer.setSample(ch, i, buffer.getSample(ch, i) * gain);

                float grDb = juce::Decibels::gainToDecibels(gain, -40.0f);
                if (-grDb > maxReductionInBlock)
                    maxReductionInBlock = -grDb;
            }
        }

        limiterGR.store(-maxReductionInBlock, std::memory_order_relaxed);
    }

    float VocalDSPProcessor::getCompressionTransferDb(float inDb) const
    {
        const float thresholdDb = params.compThresholdDb.load(std::memory_order_relaxed);
        const float ratio = juce::jmax(1.0f, params.compRatio.load(std::memory_order_relaxed));
        const int kneeType = params.compKnee.load(std::memory_order_relaxed);
        const float kneeWidthDb = (kneeType == VocalDSPParameters::KneeHard) ? 0.0f : ((kneeType == VocalDSPParameters::KneeNormal) ? 6.0f : 12.0f);
        const float makeupDb = params.compMakeupDb.load(std::memory_order_relaxed);

        float grDb = 0.0f;
        if (kneeWidthDb > 0.01f)
        {
            if (2.0f * (inDb - thresholdDb) < -kneeWidthDb)
            {
                grDb = 0.0f;
            }
            else if (2.0f * std::abs(inDb - thresholdDb) <= kneeWidthDb)
            {
                float d = inDb - thresholdDb + 0.5f * kneeWidthDb;
                grDb = (d * d) / (2.0f * kneeWidthDb) * (1.0f - 1.0f / ratio);
            }
            else
            {
                grDb = (inDb - thresholdDb) * (1.0f - 1.0f / ratio);
            }
        }
        else
        {
            if (inDb > thresholdDb)
                grDb = (inDb - thresholdDb) * (1.0f - 1.0f / ratio);
        }

        return inDb - grDb + makeupDb;
    }
}
