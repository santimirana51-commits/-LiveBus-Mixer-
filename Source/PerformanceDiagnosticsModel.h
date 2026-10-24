#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cstring>

namespace LiveBus
{

/**
 * Diagnostic Event structure designed for real-time safe zero-allocation logging.
 */
struct DiagnosticEvent
{
    juce::int64 timestampMs{0};

    enum class Severity
    {
        Info = 0,
        Warning,
        Error
    };

    enum class Category
    {
        Device = 0,
        AudioEngine,
        Buffer,
        DSP,
        VST3,
        Scene,
        Routing
    };

    Severity severity{Severity::Info};
    Category category{Category::Device};
    char message[160]{0};

    juce::String getSeverityString() const
    {
        switch (severity)
        {
            case Severity::Info:    return "INFO";
            case Severity::Warning: return "WARN";
            case Severity::Error:   return "ERROR";
            default:                return "INFO";
        }
    }

    juce::String getCategoryString() const
    {
        switch (category)
        {
            case Category::Device:      return "Device";
            case Category::AudioEngine: return "AudioEngine";
            case Category::Buffer:      return "Buffer";
            case Category::DSP:         return "DSP";
            case Category::VST3:        return "VST3";
            case Category::Scene:       return "Scene";
            case Category::Routing:     return "Routing";
            default:                    return "General";
        }
    }

    juce::String getFormattedTime() const
    {
        auto t = juce::Time(timestampMs);
        return t.formatted("%H:%M:%S.") + juce::String::formatted("%03d", (int)(timestampMs % 1000));
    }

    juce::Colour getSeverityColour() const
    {
        switch (severity)
        {
            case Severity::Info:    return juce::Colour(0xff00f5d4); // Cyan / Teal
            case Severity::Warning: return juce::Colour(0xffffb703); // Amber / Yellow
            case Severity::Error:   return juce::Colour(0xffff4d6d); // Red
            default:                return juce::Colour(0xff94a3b8);
        }
    }

    juce::String toString() const
    {
        return "[" + getFormattedTime() + "] [" + getSeverityString() + "] [" + getCategoryString() + "] " + juce::String(message);
    }
};

using DiagnosticSeverity = DiagnosticEvent::Severity;
using DiagnosticCategory = DiagnosticEvent::Category;

/**
 * Lock-free, bounded Single-Producer Single-Consumer (SPSC) Ring Buffer.
 * Guaranteed zero-heap allocation, lock-free, and safe for audio callbacks.
 */
class RealtimeDiagnosticQueue
{
public:
    static constexpr size_t Capacity = 512;

    RealtimeDiagnosticQueue()
    {
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
        droppedEvents.store(0, std::memory_order_relaxed);
    }

    bool push(DiagnosticEvent::Severity sev, DiagnosticEvent::Category cat, const char* msg)
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) % Capacity;
        const size_t currentRead = readIndex.load(std::memory_order_acquire);

        if (nextWrite == currentRead)
        {
            // Queue is full; drop without locking or allocating
            droppedEvents.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        auto& ev = buffer[currentWrite];
        ev.timestampMs = juce::Time::currentTimeMillis();
        ev.severity = sev;
        ev.category = cat;

        if (msg != nullptr)
        {
            #if defined(_MSC_VER)
            strncpy_s(ev.message, sizeof(ev.message), msg, sizeof(ev.message) - 1);
            #else
            strncpy(ev.message, msg, sizeof(ev.message) - 1);
            ev.message[sizeof(ev.message) - 1] = '\0';
            #endif
        }
        else
        {
            ev.message[0] = '\0';
        }

        writeIndex.store(nextWrite, std::memory_order_release);
        return true;
    }

    bool pop(DiagnosticEvent& outEvent)
    {
        const size_t currentRead = readIndex.load(std::memory_order_relaxed);
        const size_t currentWrite = writeIndex.load(std::memory_order_acquire);

        if (currentRead == currentWrite)
            return false; // Empty

        outEvent = buffer[currentRead];
        const size_t nextRead = (currentRead + 1) % Capacity;
        readIndex.store(nextRead, std::memory_order_release);
        return true;
    }

    int getDroppedCount() const noexcept
    {
        return droppedEvents.load(std::memory_order_relaxed);
    }

    int getDropCount() const noexcept
    {
        return getDroppedCount();
    }

    void clear()
    {
        readIndex.store(writeIndex.load(std::memory_order_relaxed), std::memory_order_relaxed);
        droppedEvents.store(0, std::memory_order_relaxed);
    }

private:
    std::array<DiagnosticEvent, Capacity> buffer;
    std::atomic<size_t> writeIndex{0};
    std::atomic<size_t> readIndex{0};
    std::atomic<int> droppedEvents{0};
};

/**
 * Snapshot of real-time audio performance metrics.
 */
struct AudioPerformanceSnapshot
{
    float currentCpuLoadPercent{0.0f};
    float averageCpuLoadPercent{0.0f};
    float maxCpuLoadPercent{0.0f};

    double currentProcessingTimeMs{0.0};
    double averageProcessingTimeMs{0.0};
    double maxProcessingTimeMs{0.0};
    double availableBlockTimeMs{0.0};

    juce::uint32 deadlineMissCount{0};
    juce::uint32 dropoutCount{0};
    juce::uint32 deviceErrorCount{0};
    juce::uint32 pluginErrorCount{0};

    bool hasWarning{false};
    juce::String activeWarningMessage;
};

} // namespace LiveBus
