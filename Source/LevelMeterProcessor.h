#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

namespace LiveBus
{
    /**
     * Snapshot of audio metering data for UI consumption.
     * Contains linear levels, dBFS peak/RMS, peak-hold, clip indicators, and VU ballistics.
     */
    struct MeterSnapshot
    {
        float peakL = 0.0f;
        float peakR = 0.0f;
        float peakDbL = -100.0f;
        float peakDbR = -100.0f;

        float rmsL = 0.0f;
        float rmsR = 0.0f;
        float rmsDbL = -100.0f;
        float rmsDbR = -100.0f;

        float peakHoldDbL = -100.0f;
        float peakHoldDbR = -100.0f;

        float vuLevelL = -20.0f;    // In VU scale: -20 VU to +3 VU
        float vuLevelR = -20.0f;
        float vuNormalizedL = 0.0f; // 0.0 (-20 VU) to 1.0 (+3 VU) for needle deflection
        float vuNormalizedR = 0.0f;

        bool clippedL = false;
        bool clippedR = false;
    };

    /**
     * Real-time safe audio level metering processor.
     * Audio thread computes block peak, RMS sum of squares, and clip flags with zero lock blocking.
     * Message thread timer runs IEC 60268-17 second-order physical VU ballistics and peak decay.
     */
    class LevelMeterProcessor
    {
    public:
        LevelMeterProcessor();
        ~LevelMeterProcessor() = default;

        void prepare(double sampleRate, int blockSize);
        void reset();

        // Audio Thread Methods (Real-time safe, lock-free, zero allocations)
        void processBlock(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
        inline void processBlock(const juce::AudioBuffer<float>& buffer, int numSamples) { processBlock(buffer, 0, numSamples); }
        void processBlock(const float* const* channelData, int numChannels, int numSamples);

        // Message Thread Methods (Timer-driven UI updates)
        void updateBallistics(float dtSeconds);
        MeterSnapshot getSnapshot() const;

        // Calibration & Settings
        void setVuReference(float refDbFS) noexcept { vuReferenceDb.store(refDbFS, std::memory_order_relaxed); }
        float getVuReference() const noexcept { return vuReferenceDb.load(std::memory_order_relaxed); }

        void setPeakHoldEnabled(bool enable) noexcept { peakHoldEnabled.store(enable, std::memory_order_relaxed); }
        bool isPeakHoldEnabled() const noexcept { return peakHoldEnabled.load(std::memory_order_relaxed); }

        void setPeakDecayRate(float dbPerSecond) noexcept { peakDecayRate.store(dbPerSecond, std::memory_order_relaxed); }
        float getPeakDecayRate() const noexcept { return peakDecayRate.load(std::memory_order_relaxed); }

        void resetPeakHold();
        void resetClip();

        // Helper: Convert linear magnitude to dBFS with safe clamping
        static float linearToDb(float linearValue, float minDb = -100.0f);
        static float dbToLinear(float dbValue);

        // Helper: Convert VU units (-20 to +3) to normalized deflection [0..1]
        static float vuToNormalized(float vuVal);
        static float normalizedToVu(float norm);

    private:
        // Audio thread communication atomics
        std::atomic<float> blockPeakL{0.0f};
        std::atomic<float> blockPeakR{0.0f};
        std::atomic<float> blockRmsL{0.0f};
        std::atomic<float> blockRmsR{0.0f};
        std::atomic<bool> clipFlagL{false};
        std::atomic<bool> clipFlagR{false};

        // Configuration
        std::atomic<float> vuReferenceDb{-18.0f};  // 0 VU = -18.0 dBFS default
        std::atomic<bool> peakHoldEnabled{true};
        std::atomic<float> peakDecayRate{20.0f};    // 20 dB/sec decay after hold time
        float clipThreshold = 0.999f;              // >= -0.01 dBFS

        // Ballistics state (Message thread only)
        float currentPeakDbL = -100.0f;
        float currentPeakDbR = -100.0f;
        float currentRmsDbL = -100.0f;
        float currentRmsDbR = -100.0f;

        float peakHoldDbL = -100.0f;
        float peakHoldDbR = -100.0f;
        float peakHoldTimerL = 0.0f;
        float peakHoldTimerR = 0.0f;
        const float peakHoldDuration = 1.8f; // Seconds before hold starts decaying

        // IEC 60268-17 2nd-order analog needle physical model
        // d^2(theta)/dt^2 + 2*zeta*omega*d(theta)/dt + omega^2 * theta = omega^2 * target
        // zeta ~ 0.707, omega ~ 13.5 rad/s gives 300ms rise time with 1.2% overshoot
        float vuPositionL = 0.0f;
        float vuVelocityL = 0.0f;
        float vuPositionR = 0.0f;
        float vuVelocityR = 0.0f;

        const float vuNaturalFreq = 13.5f; // rad/sec (~2.15 Hz)
        const float vuDamping = 0.72f;      // Critical damping ratio for ~1.2% overshoot

        double currentSampleRate = 48000.0;
    };
}
