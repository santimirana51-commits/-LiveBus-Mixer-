#include "AutoDucker.h"

namespace LiveBus
{

AutoDucker::AutoDucker()
{
    updateCoefficients();
    updateFilterCoeffs();
}

void AutoDucker::prepare(double newSampleRate, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = newMaxBlockSize > 0 ? newMaxBlockSize : 512;

    updateCoefficients();
    updateFilterCoeffs();
    reset();
}

void AutoDucker::reset()
{
    hpfFilter.reset();
    lpfFilter.reset();
    currentGainLinear = 1.0f;
    holdSamplesRemaining = 0;
    rmsState = 0.0f;

    currentGainMultStream.store(1.0f);
    currentGainMultMonitor.store(1.0f);
    currentGrDbStream.store(0.0f);
    currentGrDbMonitor.store(0.0f);
    currentState.store(DuckingState::Idle);
    sidechainActive.store(false);
}

void AutoDucker::updateCoefficients()
{
    double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    float att = juce::jmax(0.5f, attackMs.load(std::memory_order_relaxed));
    float rel = juce::jmax(10.0f, releaseMs.load(std::memory_order_relaxed));

    attackCoeff = 1.0f - std::exp(-1.0f / (float)(att * 0.001 * sr));
    releaseCoeff = 1.0f - std::exp(-1.0f / (float)(rel * 0.001 * sr));
}

void AutoDucker::updateFilterCoeffs()
{
    double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    float hpf = juce::jlimit(20.0f, (float)(sr * 0.45), filterHpfFreq.load(std::memory_order_relaxed));
    float lpf = juce::jlimit(200.0f, (float)(sr * 0.49), filterLpfFreq.load(std::memory_order_relaxed));

    if (hpf >= lpf)
        hpf = lpf * 0.5f;

    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpf, 0.707f);
    auto lpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, lpf, 0.707f);

    if (hpfCoeffs)
        hpfFilter.coefficients = hpfCoeffs;
    if (lpfCoeffs)
        lpfFilter.coefficients = lpfCoeffs;
}

void AutoDucker::processSidechain(const float* triggerAudio, int numSamples)
{
    if (numSamples <= 0) return;

    bool enabled = masterEnabled.load(std::memory_order_relaxed);

    if (!enabled || triggerAudio == nullptr)
    {
        // Smoothly release to unity gain
        for (int s = 0; s < numSamples; ++s)
        {
            currentGainLinear += (1.0f - currentGainLinear) * releaseCoeff;
            if (currentGainLinear > 0.9999f)
                currentGainLinear = 1.0f;
        }

        currentGainMultStream.store(1.0f, std::memory_order_relaxed);
        currentGainMultMonitor.store(1.0f, std::memory_order_relaxed);
        currentGrDbStream.store(0.0f, std::memory_order_relaxed);
        currentGrDbMonitor.store(0.0f, std::memory_order_relaxed);
        currentTriggerLevelDb.store(-100.0f, std::memory_order_relaxed);
        sidechainActive.store(false, std::memory_order_relaxed);
        currentState.store(DuckingState::Idle, std::memory_order_relaxed);
        return;
    }

    const float threshDb = thresholdDb.load(std::memory_order_relaxed);
    const float depthDb = duckingDepthDb.load(std::memory_order_relaxed);
    const float targetGainLinear = std::pow(10.0f, depthDb / 20.0f);
    const float holdTime = holdMs.load(std::memory_order_relaxed);
    const int holdSamples = (int)(holdTime * 0.001f * (float)sampleRate);
    const bool useFilter = filterEnabled.load(std::memory_order_relaxed);
    const auto detMode = detectorMode.load(std::memory_order_relaxed);

    float maxSampleLevel = 0.0f;
    DuckingState state = currentState.load(std::memory_order_relaxed);
    bool active = false;

    for (int s = 0; s < numSamples; ++s)
    {
        float x = triggerAudio[s];

        if (useFilter)
        {
            x = hpfFilter.processSample(x);
            x = lpfFilter.processSample(x);
        }

        float sampleLevel = 0.0f;
        if (detMode == DuckingDetectorMode::Peak)
        {
            sampleLevel = std::abs(x);
        }
        else // RMS
        {
            float sq = x * x;
            rmsState += (sq - rmsState) * 0.01f;
            sampleLevel = std::sqrt(juce::jmax(0.0f, rmsState));
        }

        if (sampleLevel > maxSampleLevel)
            maxSampleLevel = sampleLevel;

        float sampleDb = 20.0f * std::log10(juce::jmax(1e-5f, sampleLevel));

        if (sampleDb >= threshDb)
        {
            active = true;
            holdSamplesRemaining = holdSamples;

            // Attack towards ducking depth
            if (currentGainLinear > targetGainLinear)
            {
                currentGainLinear += (targetGainLinear - currentGainLinear) * attackCoeff;
                state = DuckingState::Attack;
            }
            else
            {
                currentGainLinear = targetGainLinear;
                state = DuckingState::Attack;
            }
        }
        else
        {
            if (holdSamplesRemaining > 0)
            {
                holdSamplesRemaining--;
                state = DuckingState::Hold;
            }
            else
            {
                // Release back towards 1.0f (unity)
                currentGainLinear += (1.0f - currentGainLinear) * releaseCoeff;
                if (currentGainLinear >= 0.999f)
                {
                    currentGainLinear = 1.0f;
                    state = DuckingState::Idle;
                }
                else
                {
                    state = DuckingState::Release;
                }
            }
        }
    }

    float triggerLevelDb = 20.0f * std::log10(juce::jmax(1e-5f, maxSampleLevel));

    // Calculate Stream and Monitor specific gain multipliers
    bool strEn = streamEnabled.load(std::memory_order_relaxed);
    bool monEn = monitorEnabled.load(std::memory_order_relaxed);

    float gStream = 1.0f;
    float gMonitor = 1.0f;

    // Current attenuation in dB
    float currentAttenDb = 20.0f * std::log10(juce::jmax(1e-4f, currentGainLinear));

    if (strEn)
    {
        float strDepth = streamDepthDb.load(std::memory_order_relaxed);
        if (std::abs(depthDb) > 0.001f)
        {
            float ratio = strDepth / depthDb;
            float strAttenDb = currentAttenDb * ratio;
            gStream = std::pow(10.0f, strAttenDb / 20.0f);
        }
        else
        {
            gStream = currentGainLinear;
        }
        gStream = juce::jlimit(0.0f, 1.0f, gStream);
    }

    if (monEn)
    {
        float monDepth = monitorDepthDb.load(std::memory_order_relaxed);
        if (std::abs(depthDb) > 0.001f)
        {
            float ratio = monDepth / depthDb;
            float monAttenDb = currentAttenDb * ratio;
            gMonitor = std::pow(10.0f, monAttenDb / 20.0f);
        }
        else
        {
            gMonitor = currentGainLinear;
        }
        gMonitor = juce::jlimit(0.0f, 1.0f, gMonitor);
    }

    float grDbStr = (gStream < 0.9999f) ? (20.0f * std::log10(juce::jmax(1e-4f, gStream))) : 0.0f;
    float grDbMon = (gMonitor < 0.9999f) ? (20.0f * std::log10(juce::jmax(1e-4f, gMonitor))) : 0.0f;

    currentGainMultStream.store(gStream, std::memory_order_relaxed);
    currentGainMultMonitor.store(gMonitor, std::memory_order_relaxed);
    currentGrDbStream.store(grDbStr, std::memory_order_relaxed);
    currentGrDbMonitor.store(grDbMon, std::memory_order_relaxed);
    currentTriggerLevelDb.store(triggerLevelDb, std::memory_order_relaxed);
    sidechainActive.store(active, std::memory_order_relaxed);
    currentState.store(state, std::memory_order_relaxed);
}

std::vector<DuckingPreset> AutoDucker::getFactoryPresets()
{
    return {
        { "Speech over Music", -30.0f, -12.0f, 10.0f, 350.0f, 600.0f, true, 80.0f, 8000.0f },
        { "Podcast Studio",    -28.0f, -16.0f,  5.0f, 500.0f, 800.0f, true, 100.0f, 7000.0f },
        { "Light Background",  -32.0f,  -6.0f, 25.0f, 200.0f, 400.0f, false, 80.0f, 8000.0f },
        { "Strong Ducking",    -25.0f, -20.0f,  3.0f, 750.0f, 1200.0f, true, 80.0f, 9000.0f }
    };
}

void AutoDucker::applyPreset(const DuckingPreset& p)
{
    setThresholdDb(p.thresholdDb);
    setDuckingDepthDb(p.depthDb);
    setAttackMs(p.attackMs);
    setHoldMs(p.holdMs);
    setReleaseMs(p.releaseMs);
    setFilterEnabled(p.filterEnabled);
    setFilterHpfFreq(p.filterHpf);
    setFilterLpfFreq(p.filterLpf);
}

std::unique_ptr<juce::XmlElement> AutoDucker::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("AutoDucker");
    xml->setAttribute("masterEnabled", masterEnabled.load());
    xml->setAttribute("streamEnabled", streamEnabled.load());
    xml->setAttribute("monitorEnabled", monitorEnabled.load());
    xml->setAttribute("triggerSource", triggerSource.load());
    xml->setAttribute("targetChannelsMask", (int)targetChannelsMask.load());
    xml->setAttribute("thresholdDb", (double)thresholdDb.load());
    xml->setAttribute("duckingDepthDb", (double)duckingDepthDb.load());
    xml->setAttribute("streamDepthDb", (double)streamDepthDb.load());
    xml->setAttribute("monitorDepthDb", (double)monitorDepthDb.load());
    xml->setAttribute("attackMs", (double)attackMs.load());
    xml->setAttribute("holdMs", (double)holdMs.load());
    xml->setAttribute("releaseMs", (double)releaseMs.load());
    xml->setAttribute("detectorMode", (int)detectorMode.load());
    xml->setAttribute("filterEnabled", filterEnabled.load());
    xml->setAttribute("filterHpfFreq", (double)filterHpfFreq.load());
    xml->setAttribute("filterLpfFreq", (double)filterLpfFreq.load());
    return xml;
}

void AutoDucker::restoreFromXml(const juce::XmlElement& xml)
{
    if (xml.getTagName() != "AutoDucker") return;
    masterEnabled.store(xml.getBoolAttribute("masterEnabled", false));
    streamEnabled.store(xml.getBoolAttribute("streamEnabled", true));
    monitorEnabled.store(xml.getBoolAttribute("monitorEnabled", true));
    triggerSource.store(xml.getIntAttribute("triggerSource", 0));
    targetChannelsMask.store((uint16_t)xml.getIntAttribute("targetChannelsMask", 0x000C));
    thresholdDb.store((float)xml.getDoubleAttribute("thresholdDb", -30.0));
    duckingDepthDb.store((float)xml.getDoubleAttribute("duckingDepthDb", -12.0));
    streamDepthDb.store((float)xml.getDoubleAttribute("streamDepthDb", -12.0));
    monitorDepthDb.store((float)xml.getDoubleAttribute("monitorDepthDb", -12.0));
    attackMs.store((float)xml.getDoubleAttribute("attackMs", 15.0));
    holdMs.store((float)xml.getDoubleAttribute("holdMs", 350.0));
    releaseMs.store((float)xml.getDoubleAttribute("releaseMs", 600.0));
    detectorMode.store((DuckingDetectorMode)xml.getIntAttribute("detectorMode", 0));
    filterEnabled.store(xml.getBoolAttribute("filterEnabled", true));
    filterHpfFreq.store((float)xml.getDoubleAttribute("filterHpfFreq", 80.0));
    filterLpfFreq.store((float)xml.getDoubleAttribute("filterLpfFreq", 8000.0));

    updateCoefficients();
    updateFilterCoeffs();
}

} // namespace LiveBus
