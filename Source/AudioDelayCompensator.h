#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <atomic>
#include <vector>

namespace LiveBus
{

class AudioDelayCompensator
{
public:
    static constexpr int maxDelaySamples = 65536;
    static constexpr int transitionSamples = 128;

    void prepare(int numChannels, int maximumBlockSize)
    {
        juce::ignoreUnused(maximumBlockSize);
        channels = juce::jmax(1, numChannels);
        ringSize = maxDelaySamples + 1;
        delayBuffer.assign((size_t)channels * (size_t)ringSize, 0.0f);
        writePosition = 0;
        requestedDelay.store(0, std::memory_order_relaxed);
        previousDelay = 0;
        currentDelay = 0;
        transitionPosition = transitionSamples;
    }

    bool setDelaySamples(int samples) noexcept
    {
        if (samples < 0 || samples > maxDelaySamples)
        {
            requestedDelay.store(0, std::memory_order_release);
            return false;
        }

        requestedDelay.store(samples, std::memory_order_release);
        return true;
    }

    int getRequestedDelaySamples() const noexcept
    {
        return requestedDelay.load(std::memory_order_acquire);
    }

    void reset() noexcept
    {
        std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
        writePosition = 0;
        previousDelay = 0;
        currentDelay = requestedDelay.load(std::memory_order_acquire);
        transitionPosition = transitionSamples;
    }

    void process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        if (ringSize == 0 || numSamples <= 0 || numSamples > buffer.getNumSamples())
            return;

        const int targetDelay = requestedDelay.load(std::memory_order_acquire);
        if (targetDelay != currentDelay)
        {
            previousDelay = currentDelay;
            currentDelay = targetDelay;
            transitionPosition = 0;
        }

        const int channelCount = juce::jmin(channels, buffer.getNumChannels());
        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < channelCount; ++channel)
            {
                auto* channelData = buffer.getWritePointer(channel);
                const float input = channelData[sample];
                const size_t channelOffset = (size_t)channel * (size_t)ringSize;
                delayBuffer[channelOffset + (size_t)writePosition] = input;

                const auto readDelayed = [this, channelOffset](int delay) noexcept
                {
                    int position = writePosition - delay;
                    if (position < 0)
                        position += ringSize;
                    return delayBuffer[channelOffset + (size_t)position];
                };

                if (transitionPosition < transitionSamples)
                {
                    const float mix = (float)(transitionPosition + 1) /
                                      (float)transitionSamples;
                    channelData[sample] = readDelayed(previousDelay) * (1.0f - mix) +
                                          readDelayed(currentDelay) * mix;
                }
                else
                {
                    channelData[sample] = readDelayed(currentDelay);
                }
            }

            if (++writePosition == ringSize)
                writePosition = 0;
            if (transitionPosition < transitionSamples)
                ++transitionPosition;
        }
    }

private:
    std::vector<float> delayBuffer;
    std::atomic<int> requestedDelay{0};
    int channels = 0;
    int ringSize = 0;
    int writePosition = 0;
    int previousDelay = 0;
    int currentDelay = 0;
    int transitionPosition = transitionSamples;
};

} // namespace LiveBus
