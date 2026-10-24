#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <functional>
#include <memory>

namespace LiveBus
{

/**
 * Capture source mode for YouTube audio.
 */
enum class YouTubeCaptureMode : int
{
    Off = 0,               // Capture disabled
    VirtualCableOrDevice,  // User-selected input endpoint, e.g. virtual cable or Stereo Mix
    SimulatedTestStream    // Built-in synthetic test stream (offline/demo verification without cables)
};

/**
 * YouTube Audio Capture Service.
 *
 * Responsibilities:
 * - Captures a user-selected audio input endpoint. This is not process-isolated capture:
 *   the endpoint may contain audio from sources other than YouTube.
 * - Real-time safe: transfers audio between the capture device thread (or generator)
 *   and the primary audio thread using a lock-free circular ring buffer (AbstractFifo).
 * - Zero allocations on audio thread: pullAudioBlock() never allocates heap memory.
 * - Anti-denormal and NaN/Inf sanitization on every sample.
 * - Built-in simulated test stream mode: allows full end-to-end verification of mixer
 *   faders, DSP, Stream/Monitor sends, and meters without requiring 3rd-party cables.
 * - Thread-safe status telemetry and peak/RMS metering.
 */
class YouTubeAudioCaptureService : public juce::AudioIODeviceCallback,
                                   public juce::ChangeListener
{
public:
    YouTubeAudioCaptureService();
    ~YouTubeAudioCaptureService() override;

    // =========================================================================
    // Capture Mode & Device Configuration
    // =========================================================================
    YouTubeCaptureMode getCaptureMode() const noexcept { return captureMode.load(); }
    void setCaptureMode(YouTubeCaptureMode mode);

    juce::StringArray getAvailableInputDevices();
    juce::String getCurrentDeviceName() const;
    bool startDeviceCapture(const juce::String& deviceName);
    void stopDeviceCapture();

    // =========================================================================
    // Simulated Stream Generator (Demo/Fallback Mode)
    // =========================================================================
    void setSimulatedStreamEnabled(bool enabled);
    bool isSimulatedStreamEnabled() const noexcept { return simulatedStreamActive.load(); }

    // =========================================================================
    // Primary Audio Engine Pull API (Called from Main Audio Callback)
    // =========================================================================
    /**
     * Pulls stereo audio samples from the capture ring buffer into destinationBuffer.
     * Guaranteed real-time safe: lock-free, zero heap allocation, no OS syscalls.
     */
    void pullAudioBlock(juce::AudioBuffer<float>& destinationBuffer, int numSamples, double currentSampleRate);

    // =========================================================================
    // Direct Push API (For testing and unit tests)
    // =========================================================================
    void pushAudioBlock(const juce::AudioBuffer<float>& sourceBuffer, int numSamples);

    // =========================================================================
    // juce::AudioIODeviceCallback (Capture Device Thread)
    // =========================================================================
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                          float* const* outputChannelData, int numOutputChannels,
                                          int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

    // juce::ChangeListener for capture device manager
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // =========================================================================
    // Telemetry & Level Metering
    // =========================================================================
    bool isCaptureActive() const noexcept;
    bool isSignalDetected() const noexcept { return signalDetected.load(); }
    float getPeakLevelL() const noexcept { return peakMeterL.load(); }
    float getPeakLevelR() const noexcept { return peakMeterR.load(); }
    float getRmsLevel() const noexcept { return rmsMeter.load(); }
    juce::String getStatusDescription() const;
    juce::String getLastError() const { return lastErrorMessage; }

    std::function<void()> onStatusChanged;

private:
    void stopDeviceCaptureUnlocked();
    std::unique_ptr<juce::AudioDeviceManager> captureDeviceManager;
    std::shared_ptr<std::atomic<bool>> lifetimeToken = std::make_shared<std::atomic<bool>>(true);

    std::atomic<YouTubeCaptureMode> captureMode{YouTubeCaptureMode::Off};
    std::atomic<bool> captureRunning{false};
    std::atomic<bool> simulatedStreamActive{false};
    std::atomic<bool> signalDetected{false};
    std::atomic<bool> hasError{false};
    juce::String lastErrorMessage;
    juce::String selectedDeviceName;

    // Lock-free circular ring buffer (capacity 16384 samples ~340ms at 48kHz)
    static constexpr int fifoCapacity = 16384;
    juce::AbstractFifo fifo{fifoCapacity};
    juce::AudioBuffer<float> fifoBuffer{2, fifoCapacity};

    // Telemetry atomics
    std::atomic<float> peakMeterL{0.0f};
    std::atomic<float> peakMeterR{0.0f};
    std::atomic<float> rmsMeter{0.0f};

    std::mutex deviceControlMutex;

    // Simulated test tone state (harmonically rich 432 Hz music-like chord)
    float synthPhase1 = 0.0f;
    float synthPhase2 = 0.0f;
    float synthPhase3 = 0.0f;
    float synthGain = 0.35f;

    std::atomic<double> captureSampleRate{48000.0};
    double resamplePhase = 0.0;

    void notifyStatus();
    void renderSimulatedAudio(juce::AudioBuffer<float>& dest, int numSamples, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouTubeAudioCaptureService)
};

} // namespace LiveBus
