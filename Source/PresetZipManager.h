#pragma once
#include <JuceHeader.h>

class PresetZipManager
{
public:
    static bool exportPresetsToZip(const juce::File& destZipFile, const juce::File& scenesDirectory);
    static bool importPresetsFromZip(const juce::File& sourceZipFile, const juce::File& scenesDirectory);
};
