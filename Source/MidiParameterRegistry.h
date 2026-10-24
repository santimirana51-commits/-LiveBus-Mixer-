#pragma once
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <vector>

namespace LiveBus
{

/**
 * Metadata descriptor for any parameter controllable via MIDI.
 */
struct ParameterDescriptor
{
    juce::String id;
    juce::String scope; // e.g. "Input 1", "Stream Bus", "Monitor Bus", "DSP Studio", "Playback"
    juce::String name;  // e.g. "Gain (dB)", "Pan", "Mute", "Compressor Threshold"
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    float skewFactor{1.0f}; // 1.0 = linear, ~0.3 for logarithmic frequency
    bool isBoolean{false};

    std::function<float()> getter;
    std::function<void(float)> setter;

    juce::String getFormattedValue(float val) const
    {
        if (isBoolean)
            return (val >= 0.5f) ? "ON" : "OFF";

        if (id.containsIgnoreCase(".pan"))
        {
            if (std::abs(val) < 0.05f) return "C";
            return (val < 0.0f) ? "L " + juce::String((int)std::round(-val * 100)) + "%"
                                : "R " + juce::String((int)std::round(val * 100)) + "%";
        }

        if (id.containsIgnoreCase(".gain") || id.containsIgnoreCase(".threshold") || id.containsIgnoreCase(".ceiling"))
        {
            if (val <= minValue + 0.5f && minValue <= -50.0f) return "-inf dB";
            return juce::String(val, 1) + " dB";
        }

        if (id.containsIgnoreCase(".freq"))
        {
            if (val >= 1000.0f) return juce::String(val * 0.001f, 2) + " kHz";
            return juce::String((int)val) + " Hz";
        }

        if (id.containsIgnoreCase(".ratio"))
            return juce::String(val, 1) + ":1";

        return juce::String(val, 2);
    }
};

/**
 * Central registry mapping parameter IDs to engine bindings and descriptors.
 */
class MidiParameterRegistry
{
public:
    MidiParameterRegistry() = default;

    void registerParameter(const ParameterDescriptor& desc)
    {
        parameterMap[desc.id] = desc;
        orderedIds.push_back(desc.id);
    }

    bool hasParameter(const juce::String& paramId) const
    {
        return parameterMap.find(paramId) != parameterMap.end();
    }

    const ParameterDescriptor* getDescriptor(const juce::String& paramId) const
    {
        auto it = parameterMap.find(paramId);
        if (it != parameterMap.end())
            return &it->second;
        return nullptr;
    }

    const std::vector<juce::String>& getAllParameterIds() const { return orderedIds; }

    float getParameterValue(const juce::String& paramId) const
    {
        auto it = parameterMap.find(paramId);
        if (it != parameterMap.end() && it->second.getter)
            return it->second.getter();
        return 0.0f;
    }

    void setParameterValue(const juce::String& paramId, float value)
    {
        auto it = parameterMap.find(paramId);
        if (it != parameterMap.end() && it->second.setter)
        {
            float clamped = juce::jlimit(it->second.minValue, it->second.maxValue, value);
            it->second.setter(clamped);
        }
    }

    void clear()
    {
        parameterMap.clear();
        orderedIds.clear();
    }

private:
    std::map<juce::String, ParameterDescriptor> parameterMap;
    std::vector<juce::String> orderedIds;
};

} // namespace LiveBus
