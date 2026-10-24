#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace LiveBus
{

enum class DuckingState
{
    Idle = 0,
    Attack,
    Hold,
    Release
};

enum class DuckingDetectorMode
{
    Peak = 0,
    RMS
};

struct DuckingPreset
{
    juce::String name;
    float thresholdDb;
    float depthDb;
    float attackMs;
    float holdMs;
    float releaseMs;
    bool filterEnabled;
    float filterHpf;
    float filterLpf;
};

class AutoDucker
{
public:
    AutoDucker();
    ~AutoDucker() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Audio callback processing
    // Evaluates trigger channel, tracks sidechain envelope, and computes ducking multipliers
    void processSidechain(const float* triggerAudio, int numSamples);

    // Get current ducking attenuation multipliers for target channels (0.0f .. 1.0f)
    float getGainMultiplierStream() const noexcept { return currentGainMultStream.load(std::memory_order_relaxed); }
    float getGainMultiplierMonitor() const noexcept { return currentGainMultMonitor.load(std::memory_order_relaxed); }

    // Configuration - Master & Target enables
    void setEnabled(bool en) noexcept { masterEnabled.store(en, std::memory_order_release); }
    bool isEnabled() const noexcept { return masterEnabled.load(std::memory_order_relaxed); }

    void setStreamEnabled(bool en) noexcept { streamEnabled.store(en, std::memory_order_release); }
    bool isStreamEnabled() const noexcept { return streamEnabled.load(std::memory_order_relaxed); }

    void setMonitorEnabled(bool en) noexcept { monitorEnabled.store(en, std::memory_order_release); }
    bool isMonitorEnabled() const noexcept { return monitorEnabled.load(std::memory_order_relaxed); }

    // Trigger & Target channel routing
    void setTriggerSource(int channelIndex) noexcept { triggerSource.store(juce::jlimit(0, 15, channelIndex), std::memory_order_release); }
    int getTriggerSource() const noexcept { return triggerSource.load(std::memory_order_relaxed); }

    void setTargetChannelsMask(uint16_t mask) noexcept { targetChannelsMask.store(mask, std::memory_order_release); }
    uint16_t getTargetChannelsMask() const noexcept { return targetChannelsMask.load(std::memory_order_relaxed); }
    bool isTargetChannel(int channelIndex) const noexcept
    {
        return (targetChannelsMask.load(std::memory_order_relaxed) & (1 << channelIndex)) != 0;
    }
    void setTargetChannel(int channelIndex, bool isTarget) noexcept
    {
        uint16_t cur = targetChannelsMask.load(std::memory_order_relaxed);
        if (isTarget) cur |= (1 << channelIndex);
        else cur &= ~(1 << channelIndex);
        targetChannelsMask.store(cur, std::memory_order_release);
    }

    // Parameters
    void setThresholdDb(float threshDb) noexcept { thresholdDb.store(juce::jlimit(-60.0f, 0.0f, threshDb), std::memory_order_release); }
    float getThresholdDb() const noexcept { return thresholdDb.load(std::memory_order_relaxed); }

    void setDuckingDepthDb(float depthDb) noexcept
    {
        float d = juce::jlimit(-36.0f, 0.0f, depthDb);
        duckingDepthDb.store(d, std::memory_order_release);
        streamDepthDb.store(d, std::memory_order_release);
        monitorDepthDb.store(d, std::memory_order_release);
    }
    float getDuckingDepthDb() const noexcept { return duckingDepthDb.load(std::memory_order_relaxed); }

    void setStreamDepthDb(float depthDb) noexcept { streamDepthDb.store(juce::jlimit(-36.0f, 0.0f, depthDb), std::memory_order_release); }
    float getStreamDepthDb() const noexcept { return streamDepthDb.load(std::memory_order_relaxed); }

    void setMonitorDepthDb(float depthDb) noexcept { monitorDepthDb.store(juce::jlimit(-36.0f, 0.0f, depthDb), std::memory_order_release); }
    float getMonitorDepthDb() const noexcept { return monitorDepthDb.load(std::memory_order_relaxed); }

    void setAttackMs(float ms) noexcept { attackMs.store(juce::jlimit(0.5f, 200.0f, ms), std::memory_order_release); updateCoefficients(); }
    float getAttackMs() const noexcept { return attackMs.load(std::memory_order_relaxed); }

    void setHoldMs(float ms) noexcept { holdMs.store(juce::jlimit(0.0f, 3000.0f, ms), std::memory_order_release); }
    float getHoldMs() const noexcept { return holdMs.load(std::memory_order_relaxed); }

    void setReleaseMs(float ms) noexcept { releaseMs.store(juce::jlimit(10.0f, 5000.0f, ms), std::memory_order_release); updateCoefficients(); }
    float getReleaseMs() const noexcept { return releaseMs.load(std::memory_order_relaxed); }

    void setDetectorMode(DuckingDetectorMode mode) noexcept { detectorMode.store(mode, std::memory_order_release); }
    DuckingDetectorMode getDetectorMode() const noexcept { return detectorMode.load(std::memory_order_relaxed); }

    // Sidechain Key Filter
    void setFilterEnabled(bool en) noexcept { filterEnabled.store(en, std::memory_order_release); }
    bool isFilterEnabled() const noexcept { return filterEnabled.load(std::memory_order_relaxed); }

    void setFilterHpfFreq(float freqHz) noexcept { filterHpfFreq.store(juce::jlimit(20.0f, 2000.0f, freqHz), std::memory_order_release); updateFilterCoeffs(); }
    float getFilterHpfFreq() const noexcept { return filterHpfFreq.load(std::memory_order_relaxed); }

    void setFilterLpfFreq(float freqHz) noexcept { filterLpfFreq.store(juce::jlimit(200.0f, 20000.0f, freqHz), std::memory_order_release); updateFilterCoeffs(); }
    float getFilterLpfFreq() const noexcept { return filterLpfFreq.load(std::memory_order_relaxed); }

    // Real-Time Telemetry & Monitoring (Atomic)
    float getTriggerLevelDb() const noexcept { return currentTriggerLevelDb.load(std::memory_order_relaxed); }
    bool isSidechainActive() const noexcept { return sidechainActive.load(std::memory_order_relaxed); }
    float getGainReductionDbStream() const noexcept { return currentGrDbStream.load(std::memory_order_relaxed); }
    float getGainReductionDbMonitor() const noexcept { return currentGrDbMonitor.load(std::memory_order_relaxed); }
    DuckingState getCurrentState() const noexcept { return currentState.load(std::memory_order_relaxed); }

    // Preset Management
    static std::vector<DuckingPreset> getFactoryPresets();
    void applyPreset(const DuckingPreset& preset);

    // Serialization
    std::unique_ptr<juce::XmlElement> createXml() const;
    void restoreFromXml(const juce::XmlElement& xml);

private:
    void updateCoefficients();
    void updateFilterCoeffs();

    double sampleRate = 48000.0;
    int maxBlockSize = 512;

    // Parameters
    std::atomic<bool> masterEnabled{false};
    std::atomic<bool> streamEnabled{true};
    std::atomic<bool> monitorEnabled{true};
    std::atomic<int> triggerSource{0}; // Input 1 (Microphone) default
    std::atomic<uint16_t> targetChannelsMask{0x000C}; // Bits 2 and 3 = Music L and Music R
    std::atomic<float> thresholdDb{-30.0f};
    std::atomic<float> duckingDepthDb{-12.0f};
    std::atomic<float> streamDepthDb{-12.0f};
    std::atomic<float> monitorDepthDb{-12.0f};
    std::atomic<float> attackMs{15.0f};
    std::atomic<float> holdMs{350.0f};
    std::atomic<float> releaseMs{600.0f};
    std::atomic<DuckingDetectorMode> detectorMode{DuckingDetectorMode::Peak};

    std::atomic<bool> filterEnabled{true};
    std::atomic<float> filterHpfFreq{80.0f};
    std::atomic<float> filterLpfFreq{8000.0f};

    // Filter states
    juce::dsp::IIR::Filter<float> hpfFilter;
    juce::dsp::IIR::Filter<float> lpfFilter;

    // Envelope & Gain States (Audio Thread)
    float attackCoeff = 0.05f;
    float releaseCoeff = 0.005f;
    int holdSamplesRemaining = 0;
    float currentGainLinear = 1.0f; // 1.0f = 0 dB (no reduction)
    float rmsState = 0.0f;

    // Telemetry
    std::atomic<float> currentTriggerLevelDb{-100.0f};
    std::atomic<bool> sidechainActive{false};
    std::atomic<float> currentGrDbStream{0.0f};
    std::atomic<float> currentGrDbMonitor{0.0f};
    std::atomic<float> currentGainMultStream{1.0f};
    std::atomic<float> currentGainMultMonitor{1.0f};
    std::atomic<DuckingState> currentState{DuckingState::Idle};
};

} // namespace LiveBus
