#pragma once
#include <JuceHeader.h>
#include "SceneManager.h"
#include "EQPresetCatalog.h"
#include <vector>

namespace LiveBus
{

enum class PresetCategory
{
    VocalDSP = 0,
    EightBandEQ,
    ChannelStrip,
    BusDSP,
    EQCatalog,
    Count
};

inline constexpr int eqCatalogPuzuTargetOffset = 100;
inline constexpr int eqCatalogMonitor31Target = 200;
inline constexpr int eqCatalogMain24Target = 201;

inline juce::String getCategoryName(PresetCategory cat)
{
    switch (cat)
    {
        case PresetCategory::VocalDSP:    return "Vocal DSP Rack";
        case PresetCategory::EightBandEQ: return "8-Band EQ";
        case PresetCategory::ChannelStrip:return "Channel Strip";
        case PresetCategory::BusDSP:      return "Bus Master DSP";
        case PresetCategory::EQCatalog:   return "EQ Preset Catalog";
        default:                          return "Unknown";
    }
}

inline juce::String getCategoryDirectoryName(PresetCategory cat)
{
    switch (cat)
    {
        case PresetCategory::VocalDSP:    return "VocalDSP";
        case PresetCategory::EightBandEQ: return "EightBandEQ";
        case PresetCategory::ChannelStrip:return "ChannelStrip";
        case PresetCategory::BusDSP:      return "BusDSP";
        case PresetCategory::EQCatalog:   return "EQCatalog";
        default:                          return "Misc";
    }
}

struct PresetItem
{
    juce::String name;
    PresetCategory category;
    bool isFactory = false;
    juce::String description;
    juce::File userFile; // exists if user preset
    juce::String factoryId;
    bool isFavorite = false;
};

/**
 * Dedicated Preset Manager for component-specific snapshots.
 * Ensures strict scoping: loading a preset affects ONLY the target processor/channel
 * and NEVER overwrites routing matrix, device configuration, or other channels.
 */
class PresetManager
{
public:
    PresetManager();

    juce::File getPresetsBaseDirectory() const;
    juce::File getCategoryDirectory(PresetCategory cat) const;

    /** Returns all available presets (Factory + User) for the given category. */
    std::vector<PresetItem> getPresetsForCategory(PresetCategory cat) const;

    // --- Save & Load for Vocal DSP ---
    bool saveVocalDSPPreset(const juce::String& name, const SceneData::VocalDSPSceneData& data, const juce::String& description = "");
    bool loadVocalDSPPreset(const PresetItem& item, SceneData::VocalDSPSceneData& outData, juce::String& outError);

    // --- Save & Load for 8-Band EQ ---
    bool saveEightBandEQPreset(const juce::String& name, const SceneData::EightBandEQSceneData& data, const juce::String& description = "");
    bool loadEightBandEQPreset(const PresetItem& item, SceneData::EightBandEQSceneData& outData, juce::String& outError);

    // --- Save & Load for Channel Strip ---
    bool saveChannelStripPreset(const juce::String& name, const SceneData::ChannelSceneData& data, const juce::String& description = "");
    bool loadChannelStripPreset(const PresetItem& item, SceneData::ChannelSceneData& outData, juce::String& outError);

    // --- Save & Load for Bus DSP ---
    bool saveBusDSPPreset(const juce::String& name, const SceneData::BusSceneDSP& data, const juce::String& description = "");
    bool loadBusDSPPreset(const PresetItem& item, SceneData::BusSceneDSP& outData, juce::String& outError);

    bool saveEQCatalogPreset(const juce::String& name,
                             const EQPresetDefinition& preset,
                             const juce::String& description,
                             bool isFavorite,
                             bool overwrite,
                             juce::String& outError);
    bool loadEQCatalogPreset(const PresetItem& item,
                             EQPresetDefinition& outPreset,
                             juce::String& outError) const;
    bool setEQCatalogPresetFavorite(const PresetItem& item, bool isFavorite,
                                    juce::String& outError);

    // --- User Preset CRUD ---
    bool deleteUserPreset(const PresetItem& item);
    bool renameUserPreset(const PresetItem& item, const juce::String& newName);
    bool duplicateUserPreset(const PresetItem& item, const juce::String& newName);

    // --- Import / Export ---
    bool exportPresetToFile(const PresetItem& item, const juce::File& destFile);
    bool importPresetFromFile(PresetCategory cat, const juce::File& srcFile, juce::String& outName, juce::String& outError);

private:
    std::vector<PresetItem> getFactoryPresets(PresetCategory cat) const;

    // XML Serializers/Deserializers
    juce::String serializeVocalDSP(const SceneData::VocalDSPSceneData& data, const juce::String& name, const juce::String& desc);
    bool deserializeVocalDSP(const juce::XmlElement& xml, SceneData::VocalDSPSceneData& outData, juce::String& outName);

    juce::String serializeEightBandEQ(const SceneData::EightBandEQSceneData& data, const juce::String& name, const juce::String& desc);
    bool deserializeEightBandEQ(const juce::XmlElement& xml, SceneData::EightBandEQSceneData& outData, juce::String& outName);

    juce::String serializeChannelStrip(const SceneData::ChannelSceneData& data, const juce::String& name, const juce::String& desc);
    bool deserializeChannelStrip(const juce::XmlElement& xml, SceneData::ChannelSceneData& outData, juce::String& outName);

    juce::String serializeBusDSP(const SceneData::BusSceneDSP& data, const juce::String& name, const juce::String& desc);
    bool deserializeBusDSP(const juce::XmlElement& xml, SceneData::BusSceneDSP& outData, juce::String& outName);
    juce::String serializeEQCatalogPreset(const EQPresetDefinition& preset,
                                          const juce::String& description,
                                          bool isFavorite) const;
    bool deserializeEQCatalogPreset(const juce::XmlElement& xml,
                                    EQPresetDefinition& outPreset,
                                    bool& outFavorite,
                                    juce::String& outError) const;
};

} // namespace LiveBus
