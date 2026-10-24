#include "AudioSafetyProcessor.h"

namespace LiveBus
{

AudioSafetyProcessor::AudioSafetyProcessor()
{
    updateCoeffs();
}

void AudioSafetyProcessor::prepare(double newSampleRate, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = newMaxBlockSize > 0 ? newMaxBlockSize : 512;
    updateCoeffs();
    reset();
}

void AudioSafetyProcessor::reset()
{
    streamMuteGain = 1.0f;
    monitorMuteGain = 1.0f;
    streamLimiterGain = 1.0f;
    monitorLimiterGain = 1.0f;
    streamPeakDb.store(-100.0f);
    monitorPeakDb.store(-100.0f);
    streamGrDb.store(0.0f);
    monitorGrDb.store(0.0f);
    for (int i = 0; i < 8; ++i) streamEnergyHistory[i] = 0.0f;
    energyIndex = 0;
}

void AudioSafetyProcessor::updateCoeffs()
{
    double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    // 1 ms mute ramp time constant
    rampCoeff = 1.0f - std::exp(-1.0f / (float)(0.001 * sr));

    float sRel = juce::jmax(5.0f, streamReleaseMs.load(std::memory_order_relaxed));
    streamReleaseCoeff = 1.0f - std::exp(-1.0f / (float)(sRel * 0.001 * sr));

    float mRel = juce::jmax(5.0f, monitorReleaseMs.load(std::memory_order_relaxed));
    monitorReleaseCoeff = 1.0f - std::exp(-1.0f / (float)(mRel * 0.001 * sr));
}

void AudioSafetyProcessor::setEmergencyMute(EmergencyMuteMode mode) noexcept
{
    emergencyMuteMode.store(mode, std::memory_order_release);
}

void AudioSafetyProcessor::resetClipWarnings() noexcept
{
    streamClipLatched.store(false, std::memory_order_release);
    monitorClipLatched.store(false, std::memory_order_release);
    streamClipCount.store(0, std::memory_order_release);
    monitorClipCount.store(0, std::memory_order_release);
}

void AudioSafetyProcessor::processStreamBus(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 1) return;

    int numChannels = buffer.getNumChannels();
    float* const* channelData = buffer.getArrayOfWritePointers();

    // 1. Emergency Mute Processing (Smooth anti-pop ramp)
    auto mode = emergencyMuteMode.load(std::memory_order_relaxed);
    bool shouldMute = (mode == EmergencyMuteMode::StreamOnly || mode == EmergencyMuteMode::Both);
    float targetMuteGain = shouldMute ? 0.0f : 1.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        streamMuteGain += (targetMuteGain - streamMuteGain) * rampCoeff;
        if (targetMuteGain == 0.0f && streamMuteGain < 0.0001f)
            streamMuteGain = 0.0f;
        else if (targetMuteGain == 1.0f && streamMuteGain > 0.9999f)
            streamMuteGain = 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channelData[ch])
                channelData[ch][s] *= streamMuteGain;
        }
    }

    if (shouldMute && streamMuteGain == 0.0f)
    {
        buffer.clear();
        streamPeakDb.store(-100.0f, std::memory_order_relaxed);
        streamGrDb.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // 2. Output Brickwall Limiter & Peak Detection
    bool limEn = streamLimiterEnabled.load(std::memory_order_relaxed);
    float ceilDb = streamCeilingDb.load(std::memory_order_relaxed);
    float ceilLinear = std::pow(10.0f, ceilDb / 20.0f);

    float maxPeak = 0.0f;
    float minGainFound = 1.0f;
    bool hasClip = false;

    for (int s = 0; s < numSamples; ++s)
    {
        float frameMax = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channelData[ch])
            {
                float absVal = std::abs(channelData[ch][s]);
                if (absVal > frameMax) frameMax = absVal;
            }
        }

        if (limEn && ceilLinear > 0.0f)
        {
            if (frameMax > ceilLinear)
            {
                float targetGain = ceilLinear / frameMax;
                if (targetGain < streamLimiterGain)
                    streamLimiterGain = targetGain; // instant attack
            }
            else
            {
                // Exponential release
                streamLimiterGain += (1.0f - streamLimiterGain) * streamReleaseCoeff;
                if (streamLimiterGain > 0.9999f)
                    streamLimiterGain = 1.0f;
            }

            if (streamLimiterGain < minGainFound)
                minGainFound = streamLimiterGain;

            // Apply limiter gain
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (channelData[ch])
                    channelData[ch][s] *= streamLimiterGain;
            }

            frameMax *= streamLimiterGain;
        }

        if (frameMax > maxPeak)
            maxPeak = frameMax;

        if (frameMax >= 0.9999f) // 0 dBFS clip
            hasClip = true;
    }

    float peakDb = 20.0f * std::log10(juce::jmax(1e-5f, maxPeak));
    streamPeakDb.store(peakDb, std::memory_order_relaxed);

    float grDb = (minGainFound < 0.9999f) ? (20.0f * std::log10(juce::jmax(1e-4f, minGainFound))) : 0.0f;
    streamGrDb.store(grDb, std::memory_order_relaxed);

    if (hasClip)
    {
        streamClipLatched.store(true, std::memory_order_relaxed);
        streamClipCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void AudioSafetyProcessor::processMonitorBus(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (numSamples <= 0 || buffer.getNumChannels() < 1) return;

    int numChannels = buffer.getNumChannels();
    float* const* channelData = buffer.getArrayOfWritePointers();

    // 1. Emergency Mute Processing (Smooth anti-pop ramp)
    auto mode = emergencyMuteMode.load(std::memory_order_relaxed);
    bool shouldMute = (mode == EmergencyMuteMode::MonitorOnly || mode == EmergencyMuteMode::Both);
    float targetMuteGain = shouldMute ? 0.0f : 1.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        monitorMuteGain += (targetMuteGain - monitorMuteGain) * rampCoeff;
        if (targetMuteGain == 0.0f && monitorMuteGain < 0.0001f)
            monitorMuteGain = 0.0f;
        else if (targetMuteGain == 1.0f && monitorMuteGain > 0.9999f)
            monitorMuteGain = 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channelData[ch])
                channelData[ch][s] *= monitorMuteGain;
        }
    }

    if (shouldMute && monitorMuteGain == 0.0f)
    {
        buffer.clear();
        monitorPeakDb.store(-100.0f, std::memory_order_relaxed);
        monitorGrDb.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // 2. Output Brickwall Limiter & Peak Detection
    bool limEn = monitorLimiterEnabled.load(std::memory_order_relaxed);
    float ceilDb = monitorCeilingDb.load(std::memory_order_relaxed);
    float ceilLinear = std::pow(10.0f, ceilDb / 20.0f);

    float maxPeak = 0.0f;
    float minGainFound = 1.0f;
    bool hasClip = false;

    for (int s = 0; s < numSamples; ++s)
    {
        float frameMax = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channelData[ch])
            {
                float absVal = std::abs(channelData[ch][s]);
                if (absVal > frameMax) frameMax = absVal;
            }
        }

        if (limEn && ceilLinear > 0.0f)
        {
            if (frameMax > ceilLinear)
            {
                float targetGain = ceilLinear / frameMax;
                if (targetGain < monitorLimiterGain)
                    monitorLimiterGain = targetGain;
            }
            else
            {
                monitorLimiterGain += (1.0f - monitorLimiterGain) * monitorReleaseCoeff;
                if (monitorLimiterGain > 0.9999f)
                    monitorLimiterGain = 1.0f;
            }

            if (monitorLimiterGain < minGainFound)
                minGainFound = monitorLimiterGain;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (channelData[ch])
                    channelData[ch][s] *= monitorLimiterGain;
            }

            frameMax *= monitorLimiterGain;
        }

        if (frameMax > maxPeak)
            maxPeak = frameMax;

        if (frameMax >= 0.9999f)
            hasClip = true;
    }

    float peakDb = 20.0f * std::log10(juce::jmax(1e-5f, maxPeak));
    monitorPeakDb.store(peakDb, std::memory_order_relaxed);

    float grDb = (minGainFound < 0.9999f) ? (20.0f * std::log10(juce::jmax(1e-4f, minGainFound))) : 0.0f;
    monitorGrDb.store(grDb, std::memory_order_relaxed);

    if (hasClip)
    {
        monitorClipLatched.store(true, std::memory_order_relaxed);
        monitorClipCount.fetch_add(1, std::memory_order_relaxed);
    }
}

SafetyRiskLevel AudioSafetyProcessor::evaluateRiskLevel() const noexcept
{
    if (feedbackLoopRisk.load(std::memory_order_relaxed))
        return SafetyRiskLevel::CriticalFeedbackRisk;

    if (loopbackDetected.load(std::memory_order_relaxed))
        return SafetyRiskLevel::Caution;

    return SafetyRiskLevel::Normal;
}

juce::String AudioSafetyProcessor::getRiskDescription() const
{
    auto level = evaluateRiskLevel();
    switch (level)
    {
        case SafetyRiskLevel::CriticalFeedbackRisk:
            return "CRITICAL: Potential feedback loop detected between Output Bus and Input Channel!";
        case SafetyRiskLevel::Caution:
            return "CAUTION: Loopback / Virtual audio device detected. Verify routing to avoid echo.";
        case SafetyRiskLevel::Normal:
        default:
            return "NORMAL: Output paths isolated. Zero feedback risk detected.";
    }
}

std::unique_ptr<juce::XmlElement> AudioSafetyProcessor::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("AudioSafety");
    xml->setAttribute("emergencyMuteMode", (int)emergencyMuteMode.load());
    xml->setAttribute("streamLimiterEnabled", streamLimiterEnabled.load());
    xml->setAttribute("streamCeilingDb", (double)streamCeilingDb.load());
    xml->setAttribute("streamReleaseMs", (double)streamReleaseMs.load());
    xml->setAttribute("monitorLimiterEnabled", monitorLimiterEnabled.load());
    xml->setAttribute("monitorCeilingDb", (double)monitorCeilingDb.load());
    xml->setAttribute("monitorReleaseMs", (double)monitorReleaseMs.load());
    return xml;
}

void AudioSafetyProcessor::restoreFromXml(const juce::XmlElement& xml)
{
    if (xml.getTagName() != "AudioSafety") return;
    // Note: Never restore emergency mute active silently across scenes to preserve explicit user policy
    streamLimiterEnabled.store(xml.getBoolAttribute("streamLimiterEnabled", true));
    streamCeilingDb.store((float)xml.getDoubleAttribute("streamCeilingDb", -0.5));
    streamReleaseMs.store((float)xml.getDoubleAttribute("streamReleaseMs", 50.0));
    monitorLimiterEnabled.store(xml.getBoolAttribute("monitorLimiterEnabled", true));
    monitorCeilingDb.store((float)xml.getDoubleAttribute("monitorCeilingDb", -0.5));
    monitorReleaseMs.store((float)xml.getDoubleAttribute("monitorReleaseMs", 50.0));
    updateCoeffs();
}

} // namespace LiveBus
