#pragma once

#include "OBSIntegrationService.h"

namespace LiveBus
{

inline void routeMasterBusesToDevice(float* const* outputs, int numOutputs,
                                     const juce::AudioBuffer<float>& stream,
                                     const juce::AudioBuffer<float>& monitor,
                                     int numSamples, OBSRoutingMode mode) noexcept
{
    if (outputs == nullptr || numOutputs <= 0 || numSamples <= 0)
        return;

    for (int channel = 0; channel < numOutputs; ++channel)
        if (outputs[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputs[channel], numSamples);

    const auto copyBus = [outputs, numOutputs, numSamples](int outputIndex,
                                                           const juce::AudioBuffer<float>& bus,
                                                           int busChannel)
    {
        if (outputIndex < numOutputs && outputs[outputIndex] != nullptr)
            juce::FloatVectorOperations::copy(outputs[outputIndex], bus.getReadPointer(busChannel), numSamples);
    };

    if (mode == OBSRoutingMode::DedicatedSecondaryDevice)
    {
        if (numOutputs == 1 && outputs[0] != nullptr)
        {
            for (int sample = 0; sample < numSamples; ++sample)
                outputs[0][sample] = 0.5f * (monitor.getSample(0, sample) + monitor.getSample(1, sample));
        }
        else
        {
            copyBus(0, monitor, 0);
            copyBus(1, monitor, 1);
        }
        return;
    }

    if (mode == OBSRoutingMode::CombinedAudition)
    {
        if (numOutputs == 1 && outputs[0] != nullptr)
        {
            for (int sample = 0; sample < numSamples; ++sample)
                outputs[0][sample] = 0.25f * (stream.getSample(0, sample) + stream.getSample(1, sample) +
                                               monitor.getSample(0, sample) + monitor.getSample(1, sample));
            return;
        }

        for (int channel = 0; channel < juce::jmin(2, numOutputs); ++channel)
        {
            if (outputs[channel] == nullptr)
                continue;
            const int busChannel = channel % 2;
            for (int sample = 0; sample < numSamples; ++sample)
                outputs[channel][sample] = 0.5f * (stream.getSample(busChannel, sample) +
                                                    monitor.getSample(busChannel, sample));
        }
        return;
    }

    if (numOutputs == 1 && outputs[0] != nullptr)
    {
        for (int sample = 0; sample < numSamples; ++sample)
            outputs[0][sample] = 0.5f * (stream.getSample(0, sample) + stream.getSample(1, sample));
        return;
    }

    copyBus(0, stream, 0);
    copyBus(1, stream, 1);

    if (numOutputs >= 4 && mode == OBSRoutingMode::PrimaryMultiChannel)
    {
        copyBus(2, monitor, 0);
        copyBus(3, monitor, 1);
    }
}

} // namespace LiveBus
