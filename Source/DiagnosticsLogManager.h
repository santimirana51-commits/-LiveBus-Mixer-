#pragma once
#include <JuceHeader.h>
#include "PerformanceDiagnosticsModel.h"
#include <vector>
#include <mutex>

class RoutingMatrix;

namespace LiveBus
{

class AudioDeviceService;
class AudioPerformanceMonitor;
class VST3HostEngine;
using RoutingMatrix = ::RoutingMatrix;

/**
 * Manages diagnostic event history, severity filtering, log export,
 * and comprehensive system diagnostic report generation.
 */
class DiagnosticsLogManager
{
public:
    enum class FilterMode
    {
        All = 0,
        InfoOnly,
        WarningAndError,
        ErrorOnly
    };

    explicit DiagnosticsLogManager(AudioPerformanceMonitor* monitor = nullptr);
    explicit DiagnosticsLogManager(AudioPerformanceMonitor& monitor);
    ~DiagnosticsLogManager() = default;

    void setPerformanceMonitor(AudioPerformanceMonitor* monitor) { perfMonitor = monitor; }

    // Queue draining from audio callback (Called on UI thread timer)
    void drainQueue(RealtimeDiagnosticQueue& queue);
    void drainQueueFromAudioThread();

    // Message-thread manual logging
    void addEvent(DiagnosticEvent::Severity sev, DiagnosticEvent::Category cat, const juce::String& msg);
    void log(DiagnosticEvent::Severity sev, DiagnosticEvent::Category cat, const juce::String& msg)
    {
        addEvent(sev, cat, msg);
    }

    // Event retrieval & Filtering
    std::vector<DiagnosticEvent> getFilteredEvents(FilterMode mode) const;
    int getEventCount() const;
    int getTotalEventCount() const { return getEventCount(); }
    void clearLog();

    // Exporting
    bool exportLogToFile(const juce::File& targetFile) const;

    juce::String generateDiagnosticReport(const AudioDeviceService& devService,
                                         const AudioPerformanceMonitor& perfMon,
                                         const VST3HostEngine* vst3Engine,
                                         const RoutingMatrix* routing) const;

    juce::String generateReport(const AudioDeviceService& devService,
                               const AudioPerformanceMonitor& perfMon,
                               const VST3HostEngine* vst3Engine,
                               const RoutingMatrix* routing) const
    {
        return generateDiagnosticReport(devService, perfMon, vst3Engine, routing);
    }

    bool exportReportToFile(const juce::File& targetFile, const juce::String& reportText) const;
    bool exportReportToFile(const juce::File& targetFile,
                            const AudioDeviceService& devService,
                            const AudioPerformanceMonitor& perfMon,
                            const VST3HostEngine* vst3Engine,
                            const RoutingMatrix* routing) const
    {
        return exportReportToFile(targetFile, generateDiagnosticReport(devService, perfMon, vst3Engine, routing));
    }

private:
    static constexpr size_t MaxHistoryEvents = 1000;
    AudioPerformanceMonitor* perfMonitor{nullptr};
    mutable std::mutex logMutex;
    std::vector<DiagnosticEvent> history;
};

} // namespace LiveBus
