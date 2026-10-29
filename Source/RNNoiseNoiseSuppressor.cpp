#include "RNNoiseNoiseSuppressor.h"
#include <algorithm>
#include <cmath>

namespace LiveBus
{

RNNoiseNoiseSuppressor::RNNoiseNoiseSuppressor() noexcept
    : frameSize(rnnoise_get_frame_size())
{
    if (frameSize <= 0 || static_cast<size_t>(frameSize) > MaximumFrameSize)
    {
        frameSize = 0;
        return;
    }

    createStates();
    resetStreamBuffers();
}

RNNoiseNoiseSuppressor::~RNNoiseNoiseSuppressor()
{
    destroyStates();
}

void RNNoiseNoiseSuppressor::createStates() noexcept
{
    destroyStates();
    states[0] = rnnoise_create(nullptr);
    states[1] = rnnoise_create(nullptr);
    if (states[0] == nullptr || states[1] == nullptr)
        destroyStates();
}

void RNNoiseNoiseSuppressor::destroyStates() noexcept
{
    for (auto*& state : states)
    {
        if (state != nullptr)
            rnnoise_destroy(state);
        state = nullptr;
    }
}

void RNNoiseNoiseSuppressor::resetStreamBuffers() noexcept
{
    framePosition = 0;
    fifoRead.fill(0);
    fifoWrite.fill(0);
    fifoCount.fill(0);
    vadGain.fill(1.0f);
    const auto releaseFrames = static_cast<size_t>(std::ceil(
        vadGracePeriodMs.load(std::memory_order_acquire) / 10.0f));
    vadGraceFramesRemaining.fill(releaseFrames);
    for (size_t channel = 0; channel < states.size(); ++channel)
    {
        for (int sample = 0; sample < frameSize; ++sample)
            pushOutput(channel, 0.0f);
    }
}

void RNNoiseNoiseSuppressor::resetAfterDiscontinuity() noexcept
{
    createStates();
    resetStreamBuffers();
}

bool RNNoiseNoiseSuppressor::pushOutput(size_t channel, float sample) noexcept
{
    if (fifoCount[channel] >= OutputFifoSize)
        return false;

    outputFifo[channel][fifoWrite[channel]] = sample;
    fifoWrite[channel] = (fifoWrite[channel] + 1) % OutputFifoSize;
    ++fifoCount[channel];
    return true;
}

float RNNoiseNoiseSuppressor::popOutput(size_t channel) noexcept
{
    if (fifoCount[channel] == 0)
        return 0.0f;

    const auto value = outputFifo[channel][fifoRead[channel]];
    fifoRead[channel] = (fifoRead[channel] + 1) % OutputFifoSize;
    --fifoCount[channel];
    return value;
}

void RNNoiseNoiseSuppressor::process(float* const* channels, size_t numChannels,
                                     size_t numSamples) noexcept
{
    if (!isReady() || channels == nullptr || numChannels == 0 || numChannels > states.size() ||
        numSamples == 0 || numSamples > AsyncAudioWorker::MaxSamples)
        return;

    for (size_t channel = 0; channel < numChannels; ++channel)
        if (channels[channel] == nullptr)
            return;

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            const auto input = std::isfinite(channels[channel][sample])
                                   ? std::clamp(channels[channel][sample], -1.0f, 1.0f)
                                   : 0.0f;
            inputFrame[channel][framePosition] = input * 32768.0f;
        }

        if (++framePosition == static_cast<size_t>(frameSize))
        {
            const auto useVad = vadEnabled.load(std::memory_order_acquire);
            const auto threshold = vadThreshold.load(std::memory_order_acquire);
            const auto wet = wetLevel.load(std::memory_order_acquire);
            const auto releaseFrames = static_cast<size_t>(std::ceil(
                vadGracePeriodMs.load(std::memory_order_acquire) / 10.0f));
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                const auto probability = rnnoise_process_frame(
                    states[channel], outputFrame[channel].data(), inputFrame[channel].data());
                if (channel == 0)
                    lastSpeechProbability.store(probability, std::memory_order_relaxed);
                float targetGain = 1.0f;
                if (useVad)
                {
                    if (probability >= threshold)
                        vadGraceFramesRemaining[channel] = releaseFrames;
                    else if (vadGraceFramesRemaining[channel] > 0)
                        --vadGraceFramesRemaining[channel];
                    targetGain = probability >= threshold ||
                        vadGraceFramesRemaining[channel] > 0 ? 1.0f : 0.0f;
                }

                for (int frameSample = 0; frameSample < frameSize; ++frameSample)
                {
                    const auto sampleIndex = static_cast<size_t>(frameSample);
                    const auto gain = vadGain[channel] +
                        (targetGain - vadGain[channel]) *
                            (static_cast<float>(frameSample + 1) / static_cast<float>(frameSize));
                    const auto drySample = inputFrame[channel][sampleIndex];
                    outputFrame[channel][sampleIndex] =
                        (outputFrame[channel][sampleIndex] * wet + drySample * (1.0f - wet)) * gain;
                    if (!pushOutput(channel, outputFrame[channel][sampleIndex]))
                        resetAfterDiscontinuity();
                }
                vadGain[channel] = targetGain;
            }
            framePosition = 0;
        }

        for (size_t channel = 0; channel < numChannels; ++channel)
            channels[channel][sample] = std::clamp(popOutput(channel) / 32768.0f, -1.0f, 1.0f);
    }
}

} // namespace LiveBus
