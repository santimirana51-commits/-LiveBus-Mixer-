#include "DiagnosticsLogManager.h"
#include "AudioDeviceService.h"
#include "AudioPerformanceMonitor.h"
#include "VST3HostEngine.h"
#include "RoutingMatrix.h"
#include <sstream>

namespace LiveBus
{

DiagnosticsLogManager::DiagnosticsLogManager(AudioPerformanceMonitor* monitor)
    : perfMonitor(monitor)
{
    addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, "Diagnostics logging subsystem initialized.");
}

DiagnosticsLogManager::DiagnosticsLogManager(AudioPerformanceMonitor& monitor)
    : perfMonitor(&monitor)
{
    addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, "Diagnostics logging subsystem initialized.");
}

void DiagnosticsLogManager::drainQueueFromAudioThread()
{
    if (perfMonitor != nullptr)
        drainQueue(perfMonitor->getQueue());
}

void DiagnosticsLogManager::drainQueue(RealtimeDiagnosticQueue& queue)
{
    DiagnosticEvent ev;
    std::vector<DiagnosticEvent> drained;
    drained.reserve(64);

    while (queue.pop(ev))
    {
        drained.push_back(ev);
    }

    if (!drained.empty())
    {
        std::lock_guard<std::mutex> lock(logMutex);
        for (const auto& item : drained)
            history.push_back(item);

        // Limit memory growth
        if (history.size() > MaxHistoryEvents)
        {
            size_t excess = history.size() - MaxHistoryEvents;
            history.erase(history.begin(), history.begin() + (std::ptrdiff_t)excess);
        }
    }
}

void DiagnosticsLogManager::addEvent(DiagnosticEvent::Severity sev, DiagnosticEvent::Category cat, const juce::String& msg)
{
    DiagnosticEvent ev;
    ev.timestampMs = juce::Time::currentTimeMillis();
    ev.severity = sev;
    ev.category = cat;

    #if defined(_MSC_VER)
    strncpy_s(ev.message, sizeof(ev.message), msg.toRawUTF8(), sizeof(ev.message) - 1);
    #else
    strncpy(ev.message, msg.toRawUTF8(), sizeof(ev.message) - 1);
    ev.message[sizeof(ev.message) - 1] = '\0';
    #endif

    std::lock_guard<std::mutex> lock(logMutex);
    history.push_back(ev);

    if (history.size() > MaxHistoryEvents)
    {
        size_t excess = history.size() - MaxHistoryEvents;
        history.erase(history.begin(), history.begin() + (std::ptrdiff_t)excess);
    }
}

std::vector<DiagnosticEvent> DiagnosticsLogManager::getFilteredEvents(FilterMode mode) const
{
    std::lock_guard<std::mutex> lock(logMutex);
    if (mode == FilterMode::All)
        return history;

    std::vector<DiagnosticEvent> result;
    result.reserve(history.size());

    for (const auto& ev : history)
    {
        switch (mode)
        {
            case FilterMode::InfoOnly:
                if (ev.severity == DiagnosticEvent::Severity::Info)
                    result.push_back(ev);
                break;
            case FilterMode::WarningAndError:
                if (ev.severity == DiagnosticEvent::Severity::Warning || ev.severity == DiagnosticEvent::Severity::Error)
                    result.push_back(ev);
                break;
            case FilterMode::ErrorOnly:
                if (ev.severity == DiagnosticEvent::Severity::Error)
                    result.push_back(ev);
                break;
            default:
                result.push_back(ev);
                break;
        }
    }
    return result;
}

int DiagnosticsLogManager::getEventCount() const
{
    std::lock_guard<std::mutex> lock(logMutex);
    return (int)history.size();
}

void DiagnosticsLogManager::clearLog()
{
    std::lock_guard<std::mutex> lock(logMutex);
    history.clear();
}

bool DiagnosticsLogManager::exportLogToFile(const juce::File& targetFile) const
{
    std::lock_guard<std::mutex> lock(logMutex);
    juce::String text;
    text << "======================================================================\n";
    text << " LiveBus Mixer v1.0 — Diagnostic Event Log\n";
    text << " Export Date: " << juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S") << "\n";
    text << " Total Entries: " << (int)history.size() << "\n";
    text << "======================================================================\n\n";

    for (const auto& ev : history)
    {
        text << ev.toString() << "\n";
    }

    targetFile.deleteFile();
    return targetFile.replaceWithText(text);
}

juce::String DiagnosticsLogManager::generateDiagnosticReport(const AudioDeviceService& devService,
                                                            const AudioPerformanceMonitor& perfMon,
                                                            const VST3HostEngine* vst3Engine,
                                                            const RoutingMatrix* /*routing*/) const
{
    auto snapshot = perfMon.getSnapshot();
    double sr = devService.getCurrentSampleRate();
    int bufSamples = devService.getCurrentBufferSize();
    float bufDurationMs = AudioDeviceService::calculateBufferDurationMs(bufSamples, sr);
    float inLatencyMs = devService.getInputLatencyMs();
    float outLatencyMs = devService.getOutputLatencyMs();

    juce::String report;
    report << "================================================================================\n";
    report << " LIVEBUS MIXER v1.0 — SYSTEM & PERFORMANCE DIAGNOSTIC REPORT\n";
    report << " Generated: " << juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S") << "\n";
    report << "================================================================================\n\n";

    // 1. Software Environment
    report << "[1. APPLICATION & SYSTEM ENVIRONMENT]\n";
    report << "  Application:         LiveBus Mixer v1.0 (Windows x64 Build)\n";
    report << "  JUCE Version:        JUCE " << JUCE_STRINGIFY(JUCE_MAJOR_VERSION) << "."
           << JUCE_STRINGIFY(JUCE_MINOR_VERSION) << "." << JUCE_STRINGIFY(JUCE_BUILDNUMBER) << "\n";
    report << "  Operating System:    " << juce::SystemStats::getOperatingSystemName() << "\n";
    report << "  CPU Cores:           " << juce::SystemStats::getNumCpus() << " logical cores\n";
    report << "  System Memory:       " << juce::SystemStats::getMemorySizeInMegabytes() << " MB physical RAM\n\n";

    // 2. Hardware Audio Configuration
    report << "[2. AUDIO HARDWARE & DRIVER CONFIGURATION]\n";
    report << "  Audio Backend:       " << devService.getCurrentBackendType() << "\n";
    report << "  Output Device:       " << devService.getCurrentOutputDevice() << "\n";
    report << "  Input Device:        " << devService.getCurrentInputDevice() << "\n";
    report << "  Device State:        " << devService.getStatusMessage() << "\n";
    report << "  Sample Rate:         " << juce::String(sr, 0) << " Hz\n";
    report << "  Buffer Size:         " << bufSamples << " samples (" << juce::String(bufDurationMs, 2) << " ms per buffer)\n";
    report << "  Active Out Channels: " << devService.getActiveOutputChannels().countNumberOfSetBits() << "\n";
    report << "  Active In Channels:  " << devService.getActiveInputChannels().countNumberOfSetBits() << "\n\n";

    // 3. Latency Metrics
    report << "[3. LATENCY METRICS & BREAKDOWN]\n";
    if (inLatencyMs >= 0.0f)
        report << "  Driver Input Latency:  " << juce::String(inLatencyMs, 2) << " ms (" << devService.getInputLatencySamples() << " samples)\n";
    else
        report << "  Driver Input Latency:  N/A (Driver does not report input latency)\n";

    if (outLatencyMs >= 0.0f)
        report << "  Driver Output Latency: " << juce::String(outLatencyMs, 2) << " ms (" << devService.getOutputLatencySamples() << " samples)\n";
    else
        report << "  Driver Output Latency: N/A (Driver does not report output latency)\n";

    report << "  Buffer Duration:       " << juce::String(bufDurationMs, 2) << " ms (single-buffer block time)\n";
    report << "  * Note: Buffer duration is single-block processing time, not total round-trip latency.\n\n";

    // 4. CPU & Real-Time Performance
    report << "[4. REAL-TIME CPU & PROCESSING STATISTICS]\n";
    report << "  Current Audio Load:    " << juce::String(snapshot.currentCpuLoadPercent, 1) << " %\n";
    report << "  Average Audio Load:    " << juce::String(snapshot.averageCpuLoadPercent, 1) << " %\n";
    report << "  Peak Audio Load:       " << juce::String(snapshot.maxCpuLoadPercent, 1) << " %\n";
    report << "  Current Callback Time: " << juce::String(snapshot.currentProcessingTimeMs, 3) << " ms\n";
    report << "  Average Callback Time: " << juce::String(snapshot.averageProcessingTimeMs, 3) << " ms\n";
    report << "  Max Observed Time:     " << juce::String(snapshot.maxProcessingTimeMs, 3) << " ms\n";
    report << "  Available Block Budget:" << juce::String(snapshot.availableBlockTimeMs, 3) << " ms\n";
    report << "  Deadline Misses:       " << (int)snapshot.deadlineMissCount << "\n";
    report << "  Driver Underruns:      " << (int)snapshot.dropoutCount << "\n";
    report << "  Hardware Errors:       " << (int)snapshot.deviceErrorCount << "\n";
    report << "  Plugin Failures:       " << (int)snapshot.pluginErrorCount << "\n\n";

    // 5. VST3 Host Status
    report << "[5. VST3 PLUGIN HOST ENGINE STATUS]\n";
    if (vst3Engine != nullptr)
    {
        for (int t = 0; t < 6; ++t)
        {
            int n = vst3Engine->getNumPluginsInTarget(t);
            int lat = vst3Engine->getTargetTotalLatency(t);
            report << "  Target " << t << " (" << VST3HostEngine::getTargetName(t) << "): "
                   << n << " plugins loaded, total latency: " << lat << " samples\n";
        }
    }
    else
    {
        report << "  VST3 Host Engine not initialized.\n";
    }
    report << "\n";

    // 6. Recent Diagnostic Events
    report << "[6. RECENT DIAGNOSTIC EVENTS (LAST 50)]\n";
    {
        std::lock_guard<std::mutex> lock(logMutex);
        size_t count = history.size();
        size_t startIdx = (count > 50) ? (count - 50) : 0;
        for (size_t i = startIdx; i < count; ++i)
        {
            report << "  " << history[i].toString() << "\n";
        }
    }
    report << "\n================================================================================\n";
    report << " END OF REPORT\n";
    report << "================================================================================\n";

    return report;
}

bool DiagnosticsLogManager::exportReportToFile(const juce::File& targetFile, const juce::String& reportText) const
{
    targetFile.deleteFile();
    return targetFile.replaceWithText(reportText);
}

} // namespace LiveBus
