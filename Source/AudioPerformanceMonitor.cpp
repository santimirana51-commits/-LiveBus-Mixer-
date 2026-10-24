#include "AudioPerformanceMonitor.h"
#include <cmath>
#include <algorithm>

namespace LiveBus
{

AudioPerformanceMonitor::AudioPerformanceMonitor()
{
    clearStatistics();
}

void AudioPerformanceMonitor::callbackStart(int numSamples, double sampleRate) noexcept
{
    if (sampleRate > 0.0 && numSamples > 0)
    {
        currentSampleRate = sampleRate;
        currentNumSamples = numSamples;
        currentBlockBudgetMs = ((double)numSamples / sampleRate) * 1000.0;
        availableBlockTimeMs.store(currentBlockBudgetMs, std::memory_order_relaxed);
    }
    startTicks = juce::Time::getHighResolutionTicks();
}

void AudioPerformanceMonitor::callbackEnd() noexcept
{
    const juce::int64 endTicks = juce::Time::getHighResolutionTicks();
    const double elapsedSec = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks);
    const double elapsedMs = elapsedSec * 1000.0;

    if (currentBlockBudgetMs <= 0.001)
        return;

    // Calculate instantaneous load %
    float load = (float)((elapsedMs / currentBlockBudgetMs) * 100.0);
    if (std::isnan(load) || std::isinf(load) || load < 0.0f)
        load = 0.0f;
    else if (load > 500.0f)
        load = 500.0f;

    currentLoadPercent.store(load, std::memory_order_relaxed);
    lastProcessingTimeMs.store(elapsedMs, std::memory_order_relaxed);

    // Exponential Moving Average (EMA) smoothing for stable display
    float currentAvg = averageLoadPercent.load(std::memory_order_relaxed);
    float newAvg = (currentAvg <= 0.0f) ? load : (currentAvg * 0.94f + load * 0.06f);
    averageLoadPercent.store(newAvg, std::memory_order_relaxed);

    double currentAvgMs = averageProcessingTimeMs.load(std::memory_order_relaxed);
    double newAvgMs = (currentAvgMs <= 0.0) ? elapsedMs : (currentAvgMs * 0.94 + elapsedMs * 0.06);
    averageProcessingTimeMs.store(newAvgMs, std::memory_order_relaxed);

    // Peak hold & update
    float currentMax = maxLoadPercent.load(std::memory_order_relaxed);
    if (load > currentMax)
    {
        maxLoadPercent.store(load, std::memory_order_relaxed);
        peakHoldCounter = 0;
    }
    else
    {
        // Decay peak hold slowly after ~100 blocks
        if (++peakHoldCounter > 120)
        {
            maxLoadPercent.store(std::max(newAvg, currentMax * 0.98f), std::memory_order_relaxed);
            peakHoldCounter = 100;
        }
    }

    double currentMaxMs = maxProcessingTimeMs.load(std::memory_order_relaxed);
    if (elapsedMs > currentMaxMs)
        maxProcessingTimeMs.store(elapsedMs, std::memory_order_relaxed);

    // Real-time Deadline Overrun Detection
    if (elapsedMs > currentBlockBudgetMs)
    {
        deadlineMissCount.fetch_add(1, std::memory_order_relaxed);

        const auto now = juce::Time::currentTimeMillis();
        // Rate-limit logging to maximum 1 event every 250ms to prevent queue flooding
        if (now - lastDeadlineMissLogMs > 250)
        {
            lastDeadlineMissLogMs = now;
            char msgBuf[128];
            #if defined(_MSC_VER)
            sprintf_s(msgBuf, sizeof(msgBuf), "Audio callback overrun: %.2f ms (budget: %.2f ms)", elapsedMs, currentBlockBudgetMs);
            #else
            snprintf(msgBuf, sizeof(msgBuf), "Audio callback overrun: %.2f ms (budget: %.2f ms)", elapsedMs, currentBlockBudgetMs);
            #endif
            diagnosticQueue.push(DiagnosticEvent::Severity::Error, DiagnosticEvent::Category::AudioEngine, msgBuf);
        }
    }
}

void AudioPerformanceMonitor::recordDropout(const char* reason)
{
    dropoutCount.fetch_add(1, std::memory_order_relaxed);
    diagnosticQueue.push(DiagnosticEvent::Severity::Warning, DiagnosticEvent::Category::Device,
                         reason != nullptr ? reason : "Audio dropout / buffer underrun observed");
}

void AudioPerformanceMonitor::recordDeviceError(const char* error)
{
    deviceErrorCount.fetch_add(1, std::memory_order_relaxed);
    diagnosticQueue.push(DiagnosticEvent::Severity::Error, DiagnosticEvent::Category::Device,
                         error != nullptr ? error : "Audio device hardware / driver error");
}

void AudioPerformanceMonitor::recordPluginError(const char* pluginName, const char* error)
{
    pluginErrorCount.fetch_add(1, std::memory_order_relaxed);
    char buf[128];
    #if defined(_MSC_VER)
    sprintf_s(buf, sizeof(buf), "%s: %s", pluginName != nullptr ? pluginName : "Plugin", error != nullptr ? error : "Processing error");
    #else
    snprintf(buf, sizeof(buf), "%s: %s", pluginName != nullptr ? pluginName : "Plugin", error != nullptr ? error : "Processing error");
    #endif
    diagnosticQueue.push(DiagnosticEvent::Severity::Warning, DiagnosticEvent::Category::VST3, buf);
}

AudioPerformanceSnapshot AudioPerformanceMonitor::getSnapshot() const
{
    AudioPerformanceSnapshot s;
    s.currentCpuLoadPercent = currentLoadPercent.load(std::memory_order_relaxed);
    s.averageCpuLoadPercent = averageLoadPercent.load(std::memory_order_relaxed);
    s.maxCpuLoadPercent = maxLoadPercent.load(std::memory_order_relaxed);

    s.currentProcessingTimeMs = lastProcessingTimeMs.load(std::memory_order_relaxed);
    s.averageProcessingTimeMs = averageProcessingTimeMs.load(std::memory_order_relaxed);
    s.maxProcessingTimeMs = maxProcessingTimeMs.load(std::memory_order_relaxed);
    s.availableBlockTimeMs = availableBlockTimeMs.load(std::memory_order_relaxed);

    s.deadlineMissCount = deadlineMissCount.load(std::memory_order_relaxed);
    s.dropoutCount = dropoutCount.load(std::memory_order_relaxed);
    s.deviceErrorCount = deviceErrorCount.load(std::memory_order_relaxed);
    s.pluginErrorCount = pluginErrorCount.load(std::memory_order_relaxed);

    // Evaluate performance warnings
    if (s.deadlineMissCount > 0)
    {
        s.hasWarning = true;
        s.activeWarningMessage = "Audio deadline missed (" + juce::String(s.deadlineMissCount) + " times). Increase buffer size.";
    }
    else if (s.averageCpuLoadPercent >= 85.0f)
    {
        s.hasWarning = true;
        s.activeWarningMessage = "High audio processing load (" + juce::String(s.averageCpuLoadPercent, 1) + "%). Increase buffer size or bypass heavy plugins.";
    }
    else if (s.dropoutCount > 0)
    {
        s.hasWarning = true;
        s.activeWarningMessage = "Audio dropout detected. Check USB cable and audio driver latency.";
    }
    else
    {
        s.hasWarning = false;
        s.activeWarningMessage = "";
    }

    return s;
}

void AudioPerformanceMonitor::clearStatistics()
{
    currentLoadPercent.store(0.0f, std::memory_order_relaxed);
    averageLoadPercent.store(0.0f, std::memory_order_relaxed);
    maxLoadPercent.store(0.0f, std::memory_order_relaxed);

    lastProcessingTimeMs.store(0.0, std::memory_order_relaxed);
    averageProcessingTimeMs.store(0.0, std::memory_order_relaxed);
    maxProcessingTimeMs.store(0.0, std::memory_order_relaxed);

    deadlineMissCount.store(0, std::memory_order_relaxed);
    dropoutCount.store(0, std::memory_order_relaxed);
    deviceErrorCount.store(0, std::memory_order_relaxed);
    pluginErrorCount.store(0, std::memory_order_relaxed);

    lastDeadlineMissLogMs = 0;
    peakHoldCounter = 0;
}

} // namespace LiveBus
