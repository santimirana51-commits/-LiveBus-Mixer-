#pragma once

#include "RealtimeAudioBlockQueue.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

namespace LiveBus
{

class RealtimeAudioBlockProcessor
{
public:
    virtual ~RealtimeAudioBlockProcessor() = default;
    virtual bool isReady() const noexcept { return true; }
    virtual size_t getLatencySamples() const noexcept { return 0; }
    virtual void resetAfterDiscontinuity() noexcept {}
    virtual void process(float* const* channels, size_t numChannels,
                         size_t numSamples) noexcept = 0;
};

/**
 * Runs bounded audio-block processing off the audio callback. The callback
 * path emits a deterministic dry fallback at the processor's declared latency
 * unless a matching worker result is ready at that deadline.
 */
class AsyncAudioWorker
{
public:
    static constexpr size_t MaxChannels = 2;
    static constexpr size_t MaxSamples = 2048;
    static constexpr size_t QueueCapacity = 8;
    static constexpr size_t LatencyBlocks = 2;
    static constexpr size_t MaxProcessorLatencySamples = 2048;

    enum class BlockResult
    {
        InvalidInput,
        StartupSilence,
        DryFallback,
        Processed
    };

    explicit AsyncAudioWorker(RealtimeAudioBlockProcessor& blockProcessor)
        : processor(blockProcessor),
          inputQueue(std::make_unique<AudioQueue>()),
          outputQueue(std::make_unique<AudioQueue>()),
          dryDelay(std::make_unique<DryBuffer>())
    {
    }

    ~AsyncAudioWorker()
    {
        stop();
    }

    AsyncAudioWorker(const AsyncAudioWorker&) = delete;
    AsyncAudioWorker& operator=(const AsyncAudioWorker&) = delete;

    bool start()
    {
        if (worker.joinable() || !processor.isReady() ||
            processor.getLatencySamples() > MaxProcessorLatencySamples)
            return false;

        inputQueue->reset();
        outputQueue->reset();
        nextSequence = 0;
        processorLatencySamples = processor.getLatencySamples();
        resetDryLatency();
        previousEnabled = false;
        for (auto& slot : *dryDelay)
            slot.sequence = ~uint64_t{0};
        stopRequested.store(false, std::memory_order_release);
        worker = std::thread([this] { run(); });
        acceptingBlocks.store(true, std::memory_order_release);
        return true;
    }

    // Call after the audio callback has stopped using processBlock().
    void stop()
    {
        if (!worker.joinable())
            return;

        acceptingBlocks.store(false, std::memory_order_release);
        stopRequested.store(true, std::memory_order_release);
        worker.join();
        inputQueue->reset();
        outputQueue->reset();
    }

    bool isRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    uint64_t getProcessedBlockCount() const noexcept
    {
        return processedBlocks.load(std::memory_order_acquire);
    }

    uint64_t getInputDropCount() const noexcept
    {
        return inputDrops.load(std::memory_order_relaxed);
    }

    uint64_t getOutputDropCount() const noexcept
    {
        return outputDrops.load(std::memory_order_relaxed);
    }

    uint64_t getUnderrunCount() const noexcept
    {
        return underruns.load(std::memory_order_relaxed);
    }

    uint64_t getBypassBlockCount() const noexcept
    {
        return bypassBlocks.load(std::memory_order_relaxed);
    }

    uint64_t getStartupSilenceBlockCount() const noexcept
    {
        return startupSilenceBlocks.load(std::memory_order_relaxed);
    }

    size_t getLatencySamples(size_t numSamples) const noexcept
    {
        return LatencyBlocks * numSamples + processor.getLatencySamples();
    }

    BlockResult processBlock(const float* const* input, float* const* output,
                             size_t numChannels, size_t numSamples,
                             bool enabled = true) noexcept
    {
        if (input == nullptr || output == nullptr || numChannels == 0 ||
            numChannels > MaxChannels || numSamples == 0 || numSamples > MaxSamples)
            return BlockResult::InvalidInput;

        for (size_t channel = 0; channel < numChannels; ++channel)
            if (input[channel] == nullptr || output[channel] == nullptr)
                return BlockResult::InvalidInput;

        if (enabled != previousEnabled)
        {
            resetDryLatency();
            previousEnabled = enabled;
        }

        const auto sequence = nextSequence++;
        if (!enabled)
        {
            bypassBlocks.fetch_add(1, std::memory_order_relaxed);
            copyInput(input, output, numChannels, numSamples);
            return BlockResult::DryFallback;
        }

        auto& drySlot = (*dryDelay)[static_cast<size_t>(sequence) % dryDelay->size()];
        drySlot.sequence = sequence;
        drySlot.numChannels = numChannels;
        drySlot.numSamples = numSamples;
        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t sample = 0; sample < numSamples; ++sample)
                drySlot.samples[channel][sample] = input[channel][sample];

        if (acceptingBlocks.load(std::memory_order_acquire))
        {
            if (!inputQueue->tryPush(input, numChannels, numSamples, sequence))
                inputDrops.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            inputDrops.fetch_add(1, std::memory_order_relaxed);
        }

        if (sequence < LatencyBlocks)
        {
            startupSilenceBlocks.fetch_add(1, std::memory_order_relaxed);
            clearOutput(output, numChannels, numSamples);
            return BlockResult::StartupSilence;
        }

        const auto expectedSequence = sequence - LatencyBlocks;
        const auto& expectedDry =
            (*dryDelay)[static_cast<size_t>(expectedSequence) % dryDelay->size()];

        if (expectedDry.sequence != expectedSequence ||
            expectedDry.numChannels != numChannels ||
            expectedDry.numSamples != numSamples)
        {
            resetDryLatency();
            underruns.fetch_add(1, std::memory_order_relaxed);
            clearOutput(output, numChannels, numSamples);
            return BlockResult::DryFallback;
        }

        delayDryBlock(expectedDry, numChannels, numSamples);
        size_t processedChannels = 0;
        size_t processedSamplesCount = 0;
        if (outputQueue->tryPopExpected(processedPointers.data(), MaxChannels, MaxSamples,
                                        expectedSequence, processedChannels,
                                        processedSamplesCount) &&
            processedChannels == numChannels && processedSamplesCount == numSamples)
        {
            copyProcessed(output, numChannels, numSamples);
            return BlockResult::Processed;
        }

        copyDelayedDry(output, numChannels, numSamples);
        underruns.fetch_add(1, std::memory_order_relaxed);
        return BlockResult::DryFallback;
    }

private:
    struct DrySlot
    {
        std::array<std::array<float, MaxSamples>, MaxChannels> samples{};
        uint64_t sequence{~uint64_t{0}};
        size_t numChannels{0};
        size_t numSamples{0};
    };

    using AudioQueue = RealtimeAudioBlockQueue<QueueCapacity, MaxChannels, MaxSamples>;
    using DryBuffer = std::array<DrySlot, LatencyBlocks + 1>;

    static void clearOutput(float* const* output, size_t numChannels,
                            size_t numSamples) noexcept
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t sample = 0; sample < numSamples; ++sample)
                output[channel][sample] = 0.0f;
    }

    static void copyInput(const float* const* input, float* const* output,
                          size_t numChannels, size_t numSamples) noexcept
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t sample = 0; sample < numSamples; ++sample)
                output[channel][sample] = input[channel][sample];
    }

    void resetDryLatency() noexcept
    {
        dryLatencyPosition = 0;
        for (auto& channel : dryLatencyLine)
            channel.fill(0.0f);
        for (auto& channel : delayedDrySamples)
            channel.fill(0.0f);
    }

    void delayDryBlock(const DrySlot& input, size_t numChannels,
                       size_t numSamples) noexcept
    {
        if (processorLatencySamples == 0)
        {
            for (size_t channel = 0; channel < numChannels; ++channel)
                for (size_t sample = 0; sample < numSamples; ++sample)
                    delayedDrySamples[channel][sample] = input.samples[channel][sample];
            return;
        }

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                delayedDrySamples[channel][sample] =
                    dryLatencyLine[channel][dryLatencyPosition];
                dryLatencyLine[channel][dryLatencyPosition] =
                    input.samples[channel][sample];
            }
            if (++dryLatencyPosition == processorLatencySamples)
                dryLatencyPosition = 0;
        }
    }

    void copyDelayedDry(float* const* output, size_t numChannels,
                        size_t numSamples) const noexcept
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t sample = 0; sample < numSamples; ++sample)
                output[channel][sample] = delayedDrySamples[channel][sample];
    }

    void copyProcessed(float* const* output, size_t numChannels,
                       size_t numSamples) const noexcept
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t sample = 0; sample < numSamples; ++sample)
                output[channel][sample] = processedSamples[channel][sample];
    }

    void run() noexcept
    {
        running.store(true, std::memory_order_release);
        uint64_t previousSequence = 0;
        bool hasPreviousSequence = false;
        while (!stopRequested.load(std::memory_order_acquire))
        {
            size_t numChannels = 0;
            size_t numSamples = 0;
            uint64_t sequence = 0;
            if (!inputQueue->tryPop(workerPointers.data(), MaxChannels, MaxSamples,
                                    numChannels, numSamples, sequence))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (hasPreviousSequence && sequence != previousSequence + 1)
                processor.resetAfterDiscontinuity();

            if (!processor.isReady())
            {
                outputDrops.fetch_add(1, std::memory_order_relaxed);
                previousSequence = sequence;
                hasPreviousSequence = true;
                continue;
            }

            processor.process(workerPointers.data(), numChannels, numSamples);
            bool finiteOutput = true;
            for (size_t channel = 0; channel < numChannels && finiteOutput; ++channel)
                for (size_t sample = 0; sample < numSamples; ++sample)
                    if (!std::isfinite(workerSamples[channel][sample]))
                    {
                        finiteOutput = false;
                        break;
                    }

            if (!finiteOutput)
                processor.resetAfterDiscontinuity();
            if (!finiteOutput ||
                !outputQueue->tryPush(workerPointers.data(), numChannels, numSamples, sequence))
                outputDrops.fetch_add(1, std::memory_order_relaxed);
            else
                processedBlocks.fetch_add(1, std::memory_order_release);

            previousSequence = sequence;
            hasPreviousSequence = true;
        }
        running.store(false, std::memory_order_release);
    }

    RealtimeAudioBlockProcessor& processor;
    std::unique_ptr<AudioQueue> inputQueue;
    std::unique_ptr<AudioQueue> outputQueue;
    std::unique_ptr<DryBuffer> dryDelay;
    std::array<std::array<float, MaxSamples>, MaxChannels> workerSamples{};
    std::array<std::array<float, MaxSamples>, MaxChannels> processedSamples{};
    std::array<float*, MaxChannels> workerPointers{
        workerSamples[0].data(), workerSamples[1].data()};
    std::array<float*, MaxChannels> processedPointers{
        processedSamples[0].data(), processedSamples[1].data()};
    std::array<std::array<float, MaxProcessorLatencySamples>, MaxChannels> dryLatencyLine{};
    std::array<std::array<float, MaxSamples>, MaxChannels> delayedDrySamples{};
    uint64_t nextSequence{0};
    size_t processorLatencySamples{0};
    size_t dryLatencyPosition{0};
    bool previousEnabled{false};
    std::thread worker;
    std::atomic<bool> acceptingBlocks{false};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> running{false};
    std::atomic<uint64_t> processedBlocks{0};
    std::atomic<uint64_t> inputDrops{0};
    std::atomic<uint64_t> outputDrops{0};
    std::atomic<uint64_t> underruns{0};
    std::atomic<uint64_t> bypassBlocks{0};
    std::atomic<uint64_t> startupSilenceBlocks{0};
};

} // namespace LiveBus
