#pragma once

#include "SceneManager.h"
#include <array>
#include <vector>

namespace LiveBus
{

enum class EQPresetCategory
{
    Vocal,
    Streaming,
    Music,
    Monitoring,
    TonalCorrection,
    Character
};

enum class EQPresetFilterType
{
    HighPass,
    LowShelf,
    Bell,
    HighShelf,
    LowPass
};

struct EQPresetBand
{
    EQPresetFilterType type = EQPresetFilterType::Bell;
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
    bool enabled = true;
};

struct EQPresetDefinition
{
    juce::String id;
    juce::String name;
    juce::String description;
    EQPresetCategory category = EQPresetCategory::Vocal;
    int formatVersion = 1;
    std::array<EQPresetBand, 8> bands{};
};

class EQPresetCatalog final
{
public:
    static const std::vector<EQPresetDefinition>& getFactoryPresets();
    static const EQPresetDefinition* findFactoryPreset(const juce::String& id);

    static bool validate(const EQPresetDefinition& preset,
                         juce::String& error);
    static double getMagnitudeDb(const EQPresetDefinition& preset,
                                 float frequencyHz,
                                 double sampleRate = 48000.0);

    static bool mapToEightBand(
        const EQPresetDefinition& preset,
        SceneData::EightBandEQSceneData& destination,
        juce::String& error);
    static bool mapToPuzu15Band(
        const EQPresetDefinition& preset,
        PuzuEQState& destination,
        juce::String& error,
        double sampleRate = 48000.0);
    static bool mapToMonitor31Band(
        const EQPresetDefinition& preset,
        MonitorGraphicEQ31State& destination,
        juce::String& error,
        double sampleRate = 48000.0);
    static bool mapToMainOutput24Band(
        const EQPresetDefinition& preset,
        MainOutputEQ24State& destination,
        juce::String& error,
        double sampleRate = 48000.0);
};

} // namespace LiveBus
