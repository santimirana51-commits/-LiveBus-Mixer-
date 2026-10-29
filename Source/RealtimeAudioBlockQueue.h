#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace LiveBus
{

/**
 * Fixed-storage SPSC queue for transferring audio blocks between the audio
 * callback and a worker thread. A full queue rejects new blocks immediately.
 */
template <size_t Capacity, size_t MaxChannels, size_t MaxSamples>
class RealtimeAudioBlockQueue
{
    static_assert(Capacity > 1 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(MaxChannels > 0, "At least one channel is required");
    static_assert(MaxSamples > 0, "At least one sample is required");
    static_assert(std::atomic<uint64_t>::is_always_lock_free,
                  "Audio queue indices must use lock-free atomics");

public:
    // Call only while producer and consumer are both stopped.
    void reset() noexcept
    {
        readIndex.store(0, std::memory_order_relaxed);
        writeIndex.store(0, std::memory_order_relaxed);
    }

    bool tryPush(const float* const* input, size_t numChannels, size_t numSamples,
                 uint64_t sequence) noexcept
    {
        if (input == nullptr || numChannels == 0 || numChannels > MaxChannels ||
            numSamples == 0 || numSamples > MaxSamples)
            return false;

        for (size_t channel = 0; channel < numChannels; ++channel)
            if (input[channel] == nullptr)
                return false;

        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto read = readIndex.load(std::memory_order_acquire);
        if (write - read >= Capacity)
            return false;

        auto& slot = slots[static_cast<size_t>(write) & (Capacity - 1)];
        slot.numChannels = numChannels;
        slot.numSamples = numSamples;
        slot.sequence = sequence;
        for (size_t channel = 0; channel < numChannels; ++channel)
            for (size_t sample = 0; sample < numSamples; ++sample)
                slot.samples[channel][sample] = input[channel][sample];

        writeIndex.store(write + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(float* const* output, size_t outputChannels, size_t outputCapacity,
                size_t& numChannels, size_t& numSamples, uint64_t& sequence) noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        const auto write = writeIndex.load(std::memory_order_acquire);
        if (read == write)
            return false;

        const auto& slot = slots[static_cast<size_t>(read) & (Capacity - 1)];
        if (output == nullptr || outputChannels < slot.numChannels ||
            outputCapacity < slot.numSamples)
            return false;
        for (size_t channel = 0; channel < slot.numChannels; ++channel)
            if (output[channel] == nullptr)
                return false;

        for (size_t channel = 0; channel < slot.numChannels; ++channel)
            for (size_t sample = 0; sample < slot.numSamples; ++sample)
                output[channel][sample] = slot.samples[channel][sample];

        numChannels = slot.numChannels;
        numSamples = slot.numSamples;
        sequence = slot.sequence;
        readIndex.store(read + 1, std::memory_order_release);
        return true;
    }

    bool tryPopExpected(float* const* output, size_t outputChannels, size_t outputCapacity,
                        uint64_t expectedSequence, size_t& numChannels,
                        size_t& numSamples) noexcept
    {
        auto read = readIndex.load(std::memory_order_relaxed);
        const auto write = writeIndex.load(std::memory_order_acquire);

        while (read != write)
        {
            const auto& slot = slots[static_cast<size_t>(read) & (Capacity - 1)];
            if (slot.sequence > expectedSequence)
                return false;

            if (slot.sequence < expectedSequence)
            {
                readIndex.store(++read, std::memory_order_release);
                continue;
            }

            if (output == nullptr || outputChannels < slot.numChannels ||
                outputCapacity < slot.numSamples)
                return false;
            for (size_t channel = 0; channel < slot.numChannels; ++channel)
                if (output[channel] == nullptr)
                    return false;

            for (size_t channel = 0; channel < slot.numChannels; ++channel)
                for (size_t sample = 0; sample < slot.numSamples; ++sample)
                    output[channel][sample] = slot.samples[channel][sample];

            numChannels = slot.numChannels;
            numSamples = slot.numSamples;
            readIndex.store(read + 1, std::memory_order_release);
            return true;
        }

        return false;
    }

    static constexpr size_t getCapacity() noexcept { return Capacity; }
    static constexpr size_t getMaxChannels() noexcept { return MaxChannels; }
    static constexpr size_t getMaxSamples() noexcept { return MaxSamples; }

private:
    struct Slot
    {
        std::array<std::array<float, MaxSamples>, MaxChannels> samples{};
        size_t numChannels{0};
        size_t numSamples{0};
        uint64_t sequence{0};
    };

    std::array<Slot, Capacity> slots{};
    std::atomic<uint64_t> writeIndex{0};
    std::atomic<uint64_t> readIndex{0};
};

} // namespace LiveBus
