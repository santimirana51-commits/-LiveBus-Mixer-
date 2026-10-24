#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace LiveBus
{

enum class SafetyRiskLevel
{
    Normal = 0,
    Caution,
    CriticalFeedbackRisk
};

enum class EmergencyMuteMode
{
    None = 0,
    StreamOnly,
    MonitorOnly,
    Both
};

struct SafetyLimiterConfig
{
    float ceilingDb = -0.5f;    // -24.0 dBFS .. 0.0 dBFS
    float releaseMs = 50.0f;    // 5.0 ms .. 1000.0 ms
    bool enabled = true;
};

class AudioSafetyProcessor
{
public:
    AudioSafetyProcessor();
    ~AudioSafetyProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // =========================================================================
    // Real-Time Audio Processing (Stream and Monitor Buses)
    // =========================================================================
    // Processes audio buffer through emergency mute ramp, brickwall limiter, and peak/clip detector
    void processStreamBus(juce::AudioBuffer<float>& buffer, int numSamples);
    void processMonitorBus(juce::AudioBuffer<float>& buffer, int numSamples);

    // =========================================================================
    // Emergency Mute Controls (Real-Time Safe, Atomic)
    // =========================================================================
    void setEmergencyMute(EmergencyMuteMode mode) noexcept;
    void triggerEmergencyMuteAll() noexcept { setEmergencyMute(EmergencyMuteMode::Both); }
    void unmuteAll() noexcept { setEmergencyMute(EmergencyMuteMode::None); }
    EmergencyMuteMode getEmergencyMuteMode() const noexcept { return emergencyMuteMode.load(std::memory_order_relaxed); }
    bool isStreamEmergencyMuted() const noexcept { auto m = emergencyMuteMode.load(std::memory_order_relaxed); return m == EmergencyMuteMode::StreamOnly || m == EmergencyMuteMode::Both; }
    bool isMonitorEmergencyMuted() const noexcept { auto m = emergencyMuteMode.load(std::memory_order_relaxed); return m == EmergencyMuteMode::MonitorOnly || m == EmergencyMuteMode::Both; }

    // =========================================================================
    // Limiter Parameters
    // =========================================================================
    void setStreamLimiterEnabled(bool en) noexcept { streamLimiterEnabled.store(en, std::memory_order_release); }
    bool isStreamLimiterEnabled() const noexcept { return streamLimiterEnabled.load(std::memory_order_relaxed); }
    void setStreamCeilingDb(float ceilDb) noexcept { streamCeilingDb.store(juce::jlimit(-24.0f, 0.0f, ceilDb), std::memory_order_release); }
    float getStreamCeilingDb() const noexcept { return streamCeilingDb.load(std::memory_order_relaxed); }
    void setStreamReleaseMs(float ms) noexcept { streamReleaseMs.store(juce::jlimit(5.0f, 1000.0f, ms), std::memory_order_release); updateCoeffs(); }
    float getStreamReleaseMs() const noexcept { return streamReleaseMs.load(std::memory_order_relaxed); }

    void setMonitorLimiterEnabled(bool en) noexcept { monitorLimiterEnabled.store(en, std::memory_order_release); }
    bool isMonitorLimiterEnabled() const noexcept { return monitorLimiterEnabled.load(std::memory_order_relaxed); }
    void setMonitorCeilingDb(float ceilDb) noexcept { monitorCeilingDb.store(juce::jlimit(-24.0f, 0.0f, ceilDb), std::memory_order_release); }
    float getMonitorCeilingDb() const noexcept { return monitorCeilingDb.load(std::memory_order_relaxed); }
    void setMonitorReleaseMs(float ms) noexcept { monitorReleaseMs.store(juce::jlimit(5.0f, 1000.0f, ms), std::memory_order_release); updateCoeffs(); }
    float getMonitorReleaseMs() const noexcept { return monitorReleaseMs.load(std::memory_order_relaxed); }

    // =========================================================================
    // Telemetry & Monitoring (Atomic)
    // =========================================================================
    float getStreamPeakDb() const noexcept { return streamPeakDb.load(std::memory_order_relaxed); }
    float getMonitorPeakDb() const noexcept { return monitorPeakDb.load(std::memory_order_relaxed); }
    float getStreamGainReductionDb() const noexcept { return streamGrDb.load(std::memory_order_relaxed); }
    float getMonitorGainReductionDb() const noexcept { return monitorGrDb.load(std::memory_order_relaxed); }

    bool isStreamClipping() const noexcept { return streamClipLatched.load(std::memory_order_relaxed); }
    bool isMonitorClipping() const noexcept { return monitorClipLatched.load(std::memory_order_relaxed); }
    int getStreamClipCount() const noexcept { return streamClipCount.load(std::memory_order_relaxed); }
    int getMonitorClipCount() const noexcept { return monitorClipCount.load(std::memory_order_relaxed); }
    void resetClipWarnings() noexcept;

    // =========================================================================
    // Routing Risk & Anti-Feedback Detection
    // =========================================================================
    void setLoopbackDeviceDetected(bool detected) noexcept { loopbackDetected.store(detected, std::memory_order_release); }
    bool isLoopbackDeviceDetected() const noexcept { return loopbackDetected.load(std::memory_order_relaxed); }
    void setFeedbackLoopDetected(bool loop) noexcept { feedbackLoopRisk.store(loop, std::memory_order_release); }
    SafetyRiskLevel evaluateRiskLevel() const noexcept;
    juce::String getRiskDescription() const;

    // =========================================================================
    // Serialization & Presets
    // =========================================================================
    std::unique_ptr<juce::XmlElement> createXml() const;
    void restoreFromXml(const juce::XmlElement& xml);

private:
    void updateCoeffs();

    double sampleRate = 48000.0;
    int maxBlockSize = 512;

    // Emergency Mute
    std::atomic<EmergencyMuteMode> emergencyMuteMode{EmergencyMuteMode::None};
    float streamMuteGain = 1.0f; // Smooth 1ms ramp
    float monitorMuteGain = 1.0f;
    float rampCoeff = 0.05f;

    // Stream Limiter State
    std::atomic<bool> streamLimiterEnabled{true};
    std::atomic<float> streamCeilingDb{-0.5f};
    std::atomic<float> streamReleaseMs{50.0f};
    float streamLimiterGain = 1.0f;
    float streamReleaseCoeff = 0.01f;

    // Monitor Limiter State
    std::atomic<bool> monitorLimiterEnabled{true};
    std::atomic<float> monitorCeilingDb{-0.5f};
    std::atomic<float> monitorReleaseMs{50.0f};
    float monitorLimiterGain = 1.0f;
    float monitorReleaseCoeff = 0.01f;

    // Telemetry
    std::atomic<float> streamPeakDb{-100.0f};
    std::atomic<float> monitorPeakDb{-100.0f};
    std::atomic<float> streamGrDb{0.0f};
    std::atomic<float> monitorGrDb{0.0f};
    std::atomic<bool> streamClipLatched{false};
    std::atomic<bool> monitorClipLatched{false};
    std::atomic<int> streamClipCount{0};
    std::atomic<int> monitorClipCount{0};

    // Feedback & Risk state
    std::atomic<bool> loopbackDetected{false};
    std::atomic<bool> feedbackLoopRisk{false};
    float streamEnergyHistory[8] = {0.0f};
    int energyIndex = 0;
};

} // namespace LiveBus
