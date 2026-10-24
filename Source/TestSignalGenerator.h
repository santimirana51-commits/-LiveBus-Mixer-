#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace LiveBus
{

enum class SignalType : int
{
    Sine = 0,
    WhiteNoise = 1,
    PinkNoise = 2,
    Impulse = 3
};

enum class SignalDestination : int
{
    InputChannel1 = 0,
    InputChannel2 = 1,
    InputChannel3 = 2,
    InputChannel4 = 3,
    StreamBus = 4,
    MonitorBus = 5,
    BothBuses = 6
};

enum class SignalChannel : int
{
    Stereo = 0,
    LeftOnly = 1,
    RightOnly = 2
};

/**
 * Professional Audio Test Signal Generator for LiveBus Mixer.
 * Generates calibrated Sine, White Noise, Pink Noise, and Impulse signals.
 * Real-time safe: Zero heap allocation and lock-free in audio callback.
 */
class TestSignalGenerator
{
public:
    TestSignalGenerator();
    ~TestSignalGenerator() = default;

    void prepareToPlay(double sampleRate, int maxBlockSize);

    // Audio callback generation (Called from audio thread)
    void renderBlock(float* const* channelData, int numChannels, int numSamples);

    // Control parameters (Thread-safe atomics)
    void setEnabled(bool enable);
    bool isEnabled() const { return isRunning.load(std::memory_order_relaxed); }

    void setSignalType(SignalType type);
    SignalType getSignalType() const { return static_cast<SignalType>(signalType.load(std::memory_order_relaxed)); }

    void setFrequency(double freqHz);
    double getFrequency() const { return frequencyHz.load(std::memory_order_relaxed); }

    void setLevelDb(float db);
    float getLevelDb() const { return levelDbfs.load(std::memory_order_relaxed); }

    void setDestination(SignalDestination dest);
    SignalDestination getDestination() const { return static_cast<SignalDestination>(destination.load(std::memory_order_relaxed)); }

    void setChannel(SignalChannel chan);
    SignalChannel getChannel() const { return static_cast<SignalChannel>(channelMode.load(std::memory_order_relaxed)); }

    void setMute(bool mute);
    bool isMuted() const { return isMute.load(std::memory_order_relaxed); }

    void setContinuous(bool cont);
    bool isContinuous() const { return continuousMode.load(std::memory_order_relaxed); }

    void triggerBurst(double durationSeconds = 1.0);

    // Real-time Metering
    float getPeakLevelL() const { return peakMeterL.load(std::memory_order_relaxed); }
    float getPeakLevelR() const { return peakMeterR.load(std::memory_order_relaxed); }
    bool isClipping() const { return clipDetected.load(std::memory_order_relaxed); }
    void resetClip() { clipDetected.store(false, std::memory_order_relaxed); }

private:
    std::atomic<bool> isRunning{false};
    std::atomic<int> signalType{static_cast<int>(SignalType::Sine)};
    std::atomic<double> frequencyHz{440.0};
    std::atomic<float> levelDbfs{-24.0f}; // Safe default -24 dBFS
    std::atomic<int> destination{static_cast<int>(SignalDestination::StreamBus)};
    std::atomic<int> channelMode{static_cast<int>(SignalChannel::Stereo)};
    std::atomic<bool> isMute{false};
    std::atomic<bool> continuousMode{true};
    std::atomic<int> burstSamplesRemaining{0};

    // State for oscillators and noise generators (Audio thread only)
    double currentSampleRate{48000.0};
    double currentPhase{0.0};
    double targetPhaseDelta{0.0};
    double currentPhaseDelta{0.0};

    // Anti-click Gain Ramp
    float currentGain{0.0f};
    float targetGain{0.0f};
    float gainRampPerSample{0.001f};

    // Paul Kellet Pink Noise Filter State
    float b0{0.0f}, b1{0.0f}, b2{0.0f}, b3{0.0f}, b4{0.0f}, b5{0.0f}, b6{0.0f};

    // Impulse generator timing
    int impulseCounter{0};

    // Fast PRNG for White & Pink noise
    uint32_t prngState{123456789};
    inline float generateWhiteNoise()
    {
        // 32-bit Xorshift PRNG (-1.0 to +1.0)
        prngState ^= prngState << 13;
        prngState ^= prngState >> 17;
        prngState ^= prngState << 5;
        return (float)((int32_t)prngState) * (1.0f / 2147483648.0f);
    }

    inline float generatePinkNoise()
    {
        float white = generateWhiteNoise();
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        return pink * 0.11f; // Normalize gain
    }

    // Atomic meters
    std::atomic<float> peakMeterL{0.0f};
    std::atomic<float> peakMeterR{0.0f};
    std::atomic<bool> clipDetected{false};
};

} // namespace LiveBus
