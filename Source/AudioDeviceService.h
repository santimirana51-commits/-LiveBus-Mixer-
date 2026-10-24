#pragma once
#include <JuceHeader.h>
#include "DeviceSupervisor.h"
#include <atomic>
#include <functional>

namespace LiveBus
{

/**
 * Listener interface for AudioDeviceService events.
 */
class AudioDeviceServiceListener
{
public:
    virtual ~AudioDeviceServiceListener() = default;
    virtual void onDeviceStateChanged(DeviceState newState, const juce::String& message) = 0;
    virtual void onDeviceSetupChanged() = 0;
};

/**
 * Professional Audio Device Manager Controller.
 *
 * Centralizes all hardware audio device lifecycle management:
 * - Device backend enumeration and selection (WASAPI, ASIO, DirectSound)
 * - Input/Output device discovery and selection
 * - Channel bitmask configuration (active channels)
 * - Sample rate and buffer size validation & application
 * - Buffer duration calculation (ms)
 * - Safe test tone generator (-18 dBFS sine wave with anti-click ramping)
 * - Live input audio level metering (RMS and Peak dBFS)
 * - Disconnect detection and auto-recovery supervisor (exponential backoff)
 */
class AudioDeviceService : public juce::ChangeListener
{
public:
    explicit AudioDeviceService(juce::AudioDeviceManager& devManager);
    ~AudioDeviceService() override;

    void setListener(AudioDeviceServiceListener* newListener);

    // =========================================================================
    // Backend Management
    // =========================================================================
    juce::StringArray getAvailableBackendTypes() const;
    juce::String getCurrentBackendType() const;
    juce::String setBackendType(const juce::String& typeName);

    // =========================================================================
    // Device Enumeration & Selection
    // =========================================================================
    juce::StringArray getAvailableOutputDevices() const;
    juce::StringArray getAvailableInputDevices() const;
    juce::String getCurrentOutputDevice() const;
    juce::String getCurrentInputDevice() const;

    juce::String setOutputDevice(const juce::String& deviceName);
    juce::String setInputDevice(const juce::String& deviceName);

    // =========================================================================
    // Channel Configuration
    // =========================================================================
    juce::StringArray getAvailableOutputChannelNames() const;
    juce::StringArray getAvailableInputChannelNames() const;
    juce::BigInteger getActiveOutputChannels() const;
    juce::BigInteger getActiveInputChannels() const;

    juce::String setOutputChannelActive(int channelIndex, bool active);
    juce::String setInputChannelActive(int channelIndex, bool active);
    juce::String setOutputChannels(const juce::BigInteger& activeChannels);
    juce::String setInputChannels(const juce::BigInteger& activeChannels);

    // =========================================================================
    // Sample Rate & Buffer Size
    // =========================================================================
    juce::Array<double> getAvailableSampleRates() const;
    double getCurrentSampleRate() const;
    juce::String setSampleRate(double newSampleRate);

    juce::Array<int> getAvailableBufferSizes() const;
    int getCurrentBufferSize() const;
    juce::String setBufferSize(int newBufferSize);

    /**
     * Calculates the single-buffer processing time in milliseconds.
     * Note: This is buffer duration, not total system/driver round-trip latency.
     */
    static float calculateBufferDurationMs(int bufferSamples, double sampleRate) noexcept
    {
        if (sampleRate <= 0.0 || bufferSamples <= 0)
            return 0.0f;
        return (float)((double)bufferSamples / sampleRate * 1000.0);
    }

    // =========================================================================
    // Driver Latency Reporting
    // =========================================================================
    juce::AudioIODevice* getCurrentAudioDevice() const { return deviceManager.getCurrentAudioDevice(); }
    int getInputLatencySamples() const;
    int getOutputLatencySamples() const;
    float getInputLatencyMs() const;
    float getOutputLatencyMs() const;

    // =========================================================================
    // Control Panel (ASIO)
    // =========================================================================
    bool hasControlPanel() const;
    void openControlPanel();

    // =========================================================================
    // Safe Test Tone Generator (-18 dBFS)
    // =========================================================================
    void setTestToneEnabled(bool enabled);
    bool isTestToneEnabled() const noexcept { return testToneActive.load(); }
    void renderTestTone(float* const* outputChannels, int numChannels, int numSamples, double sampleRate);

    // =========================================================================
    // Live Input Audio Metering
    // =========================================================================
    void processInputMeter(const float* const* inputChannels, int numChannels, int numSamples);
    float getInputPeakDb() const noexcept;
    float getInputRmsDb() const noexcept;

    // =========================================================================
    // Stability, Disconnect Detection & Auto-Recovery
    // =========================================================================
    DeviceState getDeviceState() const noexcept;
    juce::String getStatusMessage() const;
    void triggerReconnection();
    void manualRetry();
    void handleDeviceChange();

    DeviceSupervisor& getSupervisor() noexcept { return *supervisor; }

    // juce::ChangeListener callback from AudioDeviceManager
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<DeviceSupervisor> supervisor;
    AudioDeviceServiceListener* listener = nullptr;

    // Test tone generator state (Real-Time Safe)
    std::atomic<bool> testToneActive{false};
    float testTonePhase = 0.0f;
    float testToneCurrentGain = 0.0f;
    static constexpr float targetTestToneLevel = 0.12589f; // -18.0 dBFS safe reference level

    // Live input metering state (Real-Time Safe Atomics)
    std::atomic<float> inputPeakMagnitude{0.0f};
    std::atomic<float> inputRmsMagnitude{0.0f};

    juce::String lastStatusMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDeviceService)
};

} // namespace LiveBus
