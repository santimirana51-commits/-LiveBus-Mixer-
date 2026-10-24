#pragma once
#include <JuceHeader.h>
#include "PerformanceDiagnosticsModel.h"
#include <atomic>

namespace LiveBus
{

/**
 * Real-time Audio Performance Monitor.
 * Accurately measures audio thread processing time, CPU load %, available block budget,
 * and detects callback overruns, deadline misses, and dropouts without allocating memory.
 */
class AudioPerformanceMonitor
{
public:
    AudioPerformanceMonitor();
    ~AudioPerformanceMonitor() = default;

    // Real-Time Audio Callback hooks (Zero heap allocation, lock-free)
    void callbackStart(int numSamples, double sampleRate) noexcept;
    void callbackEnd() noexcept;

    // Diagnostic event recording
    void recordDropout(const char* reason = "Driver audio underrun / glitch detected");
    void recordDeviceError(const char* error = "Audio device error observed");
    void recordPluginError(const char* pluginName = "Plugin", const char* error = "Processing error");

    // Diagnostics queue access
    RealtimeDiagnosticQueue& getQueue() noexcept { return diagnosticQueue; }

    // Telemetry Snapshot retrieval (Thread-Safe)
    AudioPerformanceSnapshot getSnapshot() const;
    void clearStatistics();
    void resetStatistics() { clearStatistics(); }

    float getCurrentLoadPercent() const noexcept { return currentLoadPercent.load(std::memory_order_relaxed); }
    float getAverageLoadPercent() const noexcept { return averageLoadPercent.load(std::memory_order_relaxed); }
    float getMaxLoadPercent() const noexcept { return maxLoadPercent.load(std::memory_order_relaxed); }
    bool hasRecentWarning() const noexcept { return getSnapshot().hasWarning; }

    static float calculateLoadPercent(double durationMs, double budgetMs) noexcept
    {
        if (budgetMs <= 0.0001 || durationMs <= 0.0) return 0.0f;
        float load = (float)((durationMs / budgetMs) * 100.0);
        if (std::isnan(load) || std::isinf(load)) return 0.0f;
        return load;
    }

private:
    RealtimeDiagnosticQueue diagnosticQueue;

    // Audio thread timing state
    juce::int64 startTicks{0};
    double currentSampleRate{48000.0};
    int currentNumSamples{512};
    double currentBlockBudgetMs{10.666};

    // Telemetry Atomics
    std::atomic<float> currentLoadPercent{0.0f};
    std::atomic<float> averageLoadPercent{0.0f};
    std::atomic<float> maxLoadPercent{0.0f};

    std::atomic<double> lastProcessingTimeMs{0.0};
    std::atomic<double> averageProcessingTimeMs{0.0};
    std::atomic<double> maxProcessingTimeMs{0.0};
    std::atomic<double> availableBlockTimeMs{10.666};

    std::atomic<juce::uint32> deadlineMissCount{0};
    std::atomic<juce::uint32> dropoutCount{0};
    std::atomic<juce::uint32> deviceErrorCount{0};
    std::atomic<juce::uint32> pluginErrorCount{0};

    // Rate limiter for deadline miss log spamming
    juce::int64 lastDeadlineMissLogMs{0};

    // Internal stats decay counter
    int peakHoldCounter{0};
};

} // namespace LiveBus
