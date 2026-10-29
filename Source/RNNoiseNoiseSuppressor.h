#pragma once

#include "AsyncAudioWorker.h"
#include <rnnoise.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>

namespace LiveBus
{

class RNNoiseNoiseSuppressor final : public RealtimeAudioBlockProcessor
{
public:
    static constexpr int RequiredSampleRate = 48000;
    static constexpr size_t MaximumFrameSize = 480;

    RNNoiseNoiseSuppressor() noexcept;
    ~RNNoiseNoiseSuppressor() override;

    RNNoiseNoiseSuppressor(const RNNoiseNoiseSuppressor&) = delete;
    RNNoiseNoiseSuppressor& operator=(const RNNoiseNoiseSuppressor&) = delete;

    bool isReady() const noexcept override { return states[0] != nullptr && states[1] != nullptr; }
    size_t getLatencySamples() const noexcept override
    {
        return static_cast<size_t>(frameSize);
    }
    int getFrameSize() const noexcept { return frameSize; }
    void setVADEnabled(bool enabled) noexcept
    {
        vadEnabled.store(enabled, std::memory_order_release);
    }
    void setVADThreshold(float threshold) noexcept
    {
        vadThreshold.store(std::clamp(threshold, 0.0f, 1.0f), std::memory_order_release);
    }
    void setVADGracePeriodMs(float milliseconds) noexcept
    {
        vadGracePeriodMs.store(std::clamp(milliseconds, 0.0f, 5000.0f),
                               std::memory_order_release);
    }
    void setWetLevel(float wet) noexcept
    {
        wetLevel.store(std::clamp(wet, 0.0f, 1.0f), std::memory_order_release);
    }
    float getLastSpeechProbability() const noexcept
    {
        return lastSpeechProbability.load(std::memory_order_relaxed);
    }

    void resetAfterDiscontinuity() noexcept override;
    void process(float* const* channels, size_t numChannels,
                 size_t numSamples) noexcept override;

private:
    static constexpr size_t OutputFifoSize = 8192;

    void createStates() noexcept;
    void destroyStates() noexcept;
    void resetStreamBuffers() noexcept;
    bool pushOutput(size_t channel, float sample) noexcept;
    float popOutput(size_t channel) noexcept;

    std::array<DenoiseState*, 2> states{nullptr, nullptr};
    std::array<std::array<float, MaximumFrameSize>, 2> inputFrame{};
    std::array<std::array<float, MaximumFrameSize>, 2> outputFrame{};
    std::array<std::array<float, OutputFifoSize>, 2> outputFifo{};
    std::array<size_t, 2> vadGraceFramesRemaining{};
    std::array<float, 2> vadGain{1.0f, 1.0f};
    std::array<size_t, 2> fifoRead{};
    std::array<size_t, 2> fifoWrite{};
    std::array<size_t, 2> fifoCount{};
    std::atomic<bool> vadEnabled{false};
    std::atomic<float> vadThreshold{0.60f};
    std::atomic<float> vadGracePeriodMs{200.0f};
    std::atomic<float> wetLevel{1.0f};
    std::atomic<float> lastSpeechProbability{0.0f};
    size_t framePosition{0};
    int frameSize{0};
};

} // namespace LiveBus
