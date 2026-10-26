#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <functional>

namespace LiveBus
{

/**
 * Routing mode for sending the Stream Bus to OBS Studio.
 */
enum class OBSRoutingMode
{
    PrimaryMultiChannel = 0,   // Multi-channel interface: Ch 1-2 Stream (OBS), Ch 3-4 Monitor (Headphones)
    DedicatedSecondaryDevice,  // Secondary device: Stream sent to dedicated Virtual Cable, Monitor on Primary device
    PrimaryStreamOnly,         // Primary device itself is the Virtual Cable (Stream only, zero monitor bleed)
    CombinedAudition           // Stereo Audition mix: Ch 1-2 receives Stream + Monitor (audition mode)
};

enum class SecondaryOutputBus
{
    Stream = 0,
    Monitor,
    StreamAndMonitor
};

/**
 * Information about a detected audio device.
 */
struct DetectedAudioDevice
{
    juce::String name;
    bool isVirtual = false;
    juce::String virtualDriverType; // e.g. "VB-Audio", "VAC", "VoiceMeeter", "WaveLink", "Generic"
    int maxOutputChannels = 2;
    int maxInputChannels = 2;
};

/**
 * OBS Integration Service.
 *
 * Responsibilities:
 * - Detects virtual audio devices (VB-CABLE, VoiceMeeter, VAC, Elgato, etc.)
 * - Manages secondary audio device dedicated to Virtual Cable streaming
 * - Lock-free ring buffer (AbstractFifo) for safe inter-device clock domain transfer
 * - Built-in OBS calibration test tone (-18 dBFS sine wave with anti-click ramping)
 * - Safe fallbacks: never routes streaming audio to wrong physical device on disconnect
 * - Real-time safe: zero memory allocations in audio thread
 */
class OBSIntegrationService : public juce::AudioIODeviceCallback,
                              public juce::ChangeListener
{
public:
    OBSIntegrationService(juce::AudioDeviceManager& primaryDevManager);
    ~OBSIntegrationService() override;

    // =========================================================================
    // Virtual Audio Device Detection
    // =========================================================================
    static bool isVirtualAudioDevice(const juce::String& deviceName) noexcept;
    static juce::String identifyVirtualDriverType(const juce::String& deviceName);

    std::vector<DetectedAudioDevice> scanAvailableOutputDevices();
    juce::StringArray getAvailableVirtualCables();
    juce::String getDetectedVirtualCableSummary() const;

    // =========================================================================
    // Routing Mode Configuration
    // =========================================================================
    OBSRoutingMode getRoutingMode() const noexcept { return currentRoutingMode.load(); }
    void setRoutingMode(OBSRoutingMode newMode);

    // =========================================================================
    // Secondary Streaming Device (Virtual Cable) Management
    // =========================================================================
    bool isSecondaryDeviceActive() const noexcept;
    juce::String getSecondaryDeviceName() const;
    double getSecondarySampleRate() const;
    int getSecondaryBufferSize() const;
    bool hasNativeVirtualAudioDriver() const noexcept { return nativeVirtualAudioDriverInstalled.load(std::memory_order_relaxed); }
    void setNativeVirtualAudioDriverInstalled(bool installed) noexcept
    {
        nativeVirtualAudioDriverInstalled.store(installed, std::memory_order_release);
        notifyStatusChanged();
    }
    juce::String getDriverReadinessSummary() const;

    juce::String openSecondaryVirtualDevice(const juce::String& deviceName);
    juce::String openSecondaryOutputDevice(const juce::String& deviceName);
    void closeSecondaryDevice();
    void setSecondaryOutputBus(SecondaryOutputBus bus) noexcept { secondaryOutputBus.store(bus, std::memory_order_relaxed); }
    SecondaryOutputBus getSecondaryOutputBus() const noexcept { return secondaryOutputBus.load(std::memory_order_relaxed); }
    void setSecondaryOutputGain(float gain) noexcept { secondaryOutputGain.store(juce::jlimit(0.0f, 2.0f, gain), std::memory_order_relaxed); }
    float getSecondaryOutputGain() const noexcept { return secondaryOutputGain.load(std::memory_order_relaxed); }
    void setSecondaryOutputMuted(bool muted) noexcept { secondaryOutputMuted.store(muted, std::memory_order_relaxed); }
    bool isSecondaryOutputMuted() const noexcept { return secondaryOutputMuted.load(std::memory_order_relaxed); }

    // =========================================================================
    // Audio Pipeline & FIFO Bridging (Real-Time Safe)
    // =========================================================================
    /**
     * Called from the Primary Audio Callback to push Stream Bus audio into
     * the lock-free FIFO for the secondary streaming device.
     */
    void pushStreamAudioBlock(const juce::AudioBuffer<float>& streamBuffer, int numSamples);
    void pushOutputAudioBlock(const juce::AudioBuffer<float>& streamBuffer,
                              const juce::AudioBuffer<float>& monitorBuffer,
                              int numSamples);

    // juce::AudioIODeviceCallback implementation for Secondary Device
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                          float* const* outputChannelData, int numOutputChannels,
                                          int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // juce::ChangeListener for secondary device manager
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // =========================================================================
    // Test Signal Generator (-18 dBFS Anti-Click Sine Wave)
    // =========================================================================
    void setTestSignalEnabled(bool enabled);
    bool isTestSignalEnabled() const noexcept { return testSignalActive.load(); }
    void renderTestSignal(float* const* outputChannels, int numChannels, int numSamples, double sampleRate);

    // =========================================================================
    // Status & Telemetry
    // =========================================================================
    juce::String getStatusDescription() const;
    bool isStreamActive() const noexcept { return streamRunning.load(); }
    bool isDeviceError() const noexcept { return deviceErrorState.load(); }
    juce::String getErrorMessage() const { return lastErrorMessage; }

    // Stream bus level metering for OBS panel (dBFS)
    float getStreamPeakL() const noexcept { return meterPeakL.load(); }
    float getStreamPeakR() const noexcept { return meterPeakR.load(); }
    float getStreamRmsL() const noexcept { return meterRmsL.load(); }
    float getStreamRmsR() const noexcept { return meterRmsR.load(); }

    void updateStreamMeter(float peakL, float peakR, float rmsL, float rmsR) noexcept
    {
        meterPeakL.store(peakL, std::memory_order_relaxed);
        meterPeakR.store(peakR, std::memory_order_relaxed);
        meterRmsL.store(rmsL, std::memory_order_relaxed);
        meterRmsR.store(rmsR, std::memory_order_relaxed);
    }

    std::function<void()> onStatusChanged;

private:
    juce::AudioDeviceManager& primaryDeviceManager;
    std::unique_ptr<juce::AudioDeviceManager> secondaryDeviceManager;

    // A stereo primary device can only carry one independent bus pair. Default to
    // monitoring on the primary device while Stream uses an optional secondary output.
    std::atomic<OBSRoutingMode> currentRoutingMode{OBSRoutingMode::DedicatedSecondaryDevice};
    std::atomic<SecondaryOutputBus> secondaryOutputBus{SecondaryOutputBus::Stream};
    std::atomic<float> secondaryOutputGain{1.0f};
    std::atomic<bool> secondaryOutputMuted{false};
    std::atomic<bool> streamRunning{false};
    std::atomic<bool> deviceErrorState{false};
    std::atomic<bool> nativeVirtualAudioDriverInstalled{false};
    juce::String lastErrorMessage;
    juce::String chosenVirtualDeviceName;

    // Lock-free Circular Ring Buffer for clock domain decoupling
    static constexpr int fifoCapacity = 8192;
    juce::AbstractFifo fifo{fifoCapacity};
    juce::AudioBuffer<float> fifoBuffer{2, fifoCapacity};

    // Test tone generator state
    std::atomic<bool> testSignalActive{false};
    float testTonePhase = 0.0f;
    float testToneCurrentGain = 0.0f;
    static constexpr float targetTestToneGain = 0.12589f; // -18.0 dBFS

    // Metering atomics
    std::atomic<float> meterPeakL{0.0f};
    std::atomic<float> meterPeakR{0.0f};
    std::atomic<float> meterRmsL{0.0f};
    std::atomic<float> meterRmsR{0.0f};

    double secondarySampleRate = 48000.0;
    int secondaryBlockSize = 512;

    void notifyStatusChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OBSIntegrationService)
};

} // namespace LiveBus
