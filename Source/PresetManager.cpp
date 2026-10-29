#include "PresetManager.h"
#include "EQPresetCatalog.h"

namespace LiveBus
{
namespace
{

juce::String sanitizePresetName(const juce::String& name)
{
    return name.retainCharacters(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-")
        .trim();
}

bool writeTextAtomically(const juce::File& destination,
                         const juce::String& contents,
                         juce::String& error)
{
    if (!destination.getParentDirectory().createDirectory())
    {
        error = "Could not create preset directory.";
        return false;
    }

    const auto temp = destination.getSiblingFile(
        destination.getFileName() + "." +
        juce::String(juce::Random::getSystemRandom().nextInt()) + ".tmp");
    if (!temp.replaceWithText(contents))
    {
        error = "Could not write temporary preset file.";
        return false;
    }

    const bool replaced = destination.existsAsFile()
        ? temp.replaceFileIn(destination)
        : temp.moveFileTo(destination);
    if (!replaced)
    {
        temp.deleteFile();
        error = "Could not atomically replace preset file.";
        return false;
    }
    error.clear();
    return true;
}

bool isEQUserFile(const PresetItem& item, const juce::File& expectedDirectory)
{
    return item.category == PresetCategory::EQCatalog &&
           !item.isFactory &&
           item.userFile.existsAsFile() &&
           item.userFile.getParentDirectory().getFullPathName() ==
               expectedDirectory.getFullPathName();
}

juce::String userEQPresetId(const juce::String& name)
{
    return "user." + name.toLowerCase()
        .retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 ")
        .trim().replace(" ", "-");
}

} // namespace

PresetManager::PresetManager()
{
    getPresetsBaseDirectory().createDirectory();
    for (int c = 0; c < (int)PresetCategory::Count; ++c)
    {
        getCategoryDirectory((PresetCategory)c).createDirectory();
    }
}

juce::File PresetManager::getPresetsBaseDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("LiveBusMixer")
        .getChildFile("Presets");
}

juce::File PresetManager::getCategoryDirectory(PresetCategory cat) const
{
    return getPresetsBaseDirectory().getChildFile(getCategoryDirectoryName(cat));
}

std::vector<PresetItem> PresetManager::getFactoryPresets(PresetCategory cat) const
{
    std::vector<PresetItem> list;
    switch (cat)
    {
        case PresetCategory::VocalDSP:
            list.push_back({ "Broadcast Lead Vocal", cat, true, "Gentle gate, 5.5kHz de-esser, smooth 3:1 comp, -1dB limiter" });
            list.push_back({ "Voiceover Warm & Intimate", cat, true, "Soft knee compressor, warm makeup, fast smooth release" });
            list.push_back({ "Aggressive Rock Vocal", cat, true, "Heavy 6:1 comp, fast attack, punchy release, tight gate" });
            list.push_back({ "Acoustic Guitar Dynamics", cat, true, "Gentle 2:1 optical style compression, transparent limiter" });
            list.push_back({ "Podcast Dual Host", cat, true, "Quick dialogue gate, leveler comp, ceiling -1.5 dBFS" });
            list.push_back({ "Stream Bus Master Leveler", cat, true, "Transparent broadcast bus compression and peak limiter" });
            break;

        case PresetCategory::EightBandEQ:
            list.push_back({ "Vocal Air & Clarity", cat, true, "HPF 80Hz, +2dB low shelf, -3dB boxiness cut at 400Hz, +4dB presence at 3kHz" });
            list.push_back({ "Broadcast Male Voice", cat, true, "HPF 75Hz, warm 120Hz boost, cut 350Hz, clarity boost 2.5kHz" });
            list.push_back({ "Acoustic Guitar Bright", cat, true, "HPF 90Hz, body scoop at 300Hz, sparkle at 5kHz and 10kHz" });
            list.push_back({ "Bass & Kick Scooped", cat, true, "Sub punch 60Hz, scoop 400Hz, snap at 2kHz" });
            list.push_back({ "Telephone Effect", cat, true, "Aggressive HPF 500Hz, LPF 3500Hz, midrange bandpass" });
            list.push_back({ "Flat Neutral", cat, true, "All 8 bands flat (0 dB gain) with default Q" });
            break;

        case PresetCategory::ChannelStrip:
            list.push_back({ "Standard Broadcast Mic", cat, true, "Fader 0 dB, Center pan, unmuted, Stream & Monitor enabled" });
            list.push_back({ "Acoustic Instrument DI", cat, true, "Fader -3 dB, slight Left pan, unmuted, full sends" });
            list.push_back({ "Stereo Line In", cat, true, "Fader 0 dB, unity gain stereo pairing" });
            list.push_back({ "Talkback Mic", cat, true, "Fader -6 dB, Monitor only, Stream muted" });
            break;

        case PresetCategory::BusDSP:
            list.push_back({ "Live Stream Master", cat, true, "Bus HPF 40Hz, gentle 2:1 comp, -1.0dB ceiling limiter, subtle reverb" });
            list.push_back({ "In-Ear Monitor Crisp", cat, true, "Low latency, transparent limiter, clean high-end EQ" });
            list.push_back({ "Club Broadcast Punch", cat, true, "Punchy comp 4:1, low shelf punch, limiter ceiling -0.5dB" });
            break;

        case PresetCategory::EQCatalog:
            for (const auto& preset : EQPresetCatalog::getFactoryPresets())
            {
                PresetItem item{ preset.name, cat, true, preset.description };
                item.factoryId = preset.id;
                list.push_back(std::move(item));
            }
            break;

        default:
            break;
    }
    return list;
}

std::vector<PresetItem> PresetManager::getPresetsForCategory(PresetCategory cat) const
{
    // Start with Factory presets
    auto presets = getFactoryPresets(cat);

    if (cat == PresetCategory::EQCatalog)
    {
        auto favoritesFile = getCategoryDirectory(cat).getChildFile("favorites.xml");
        auto favoriteXml = juce::XmlDocument::parse(favoritesFile);
        juce::StringArray factoryFavorites;
        if (favoriteXml && favoriteXml->hasTagName("LIVEBUS_EQ_FAVORITES"))
            for (auto* favorite = favoriteXml->getFirstChildElement();
                 favorite != nullptr; favorite = favorite->getNextElement())
                if (favorite->hasTagName("PRESET"))
                    factoryFavorites.add(
                        favorite->getStringAttribute("id"));

        for (auto& item : presets)
            item.isFavorite = factoryFavorites.contains(item.factoryId);

        auto dir = getCategoryDirectory(cat);
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
        for (const auto& file : files)
        {
            if (file == favoritesFile)
                continue;
            PresetItem item;
            item.category = cat;
            item.userFile = file;
            item.isFactory = false;
            EQPresetDefinition definition;
            juce::String error;
            if (loadEQCatalogPreset(item, definition, error))
            {
                item.name = definition.name;
                item.description = definition.description;
                auto xml = juce::XmlDocument::parse(file);
                item.isFavorite = xml != nullptr &&
                                  xml->getBoolAttribute("favorite", false);
            }
            else
            {
                item.name = file.getFileNameWithoutExtension();
                item.description = "Invalid EQ preset: " + error;
            }
            presets.push_back(std::move(item));
        }
        return presets;
    }

    if (cat != PresetCategory::EQCatalog)
    {
        auto dir = getCategoryDirectory(cat);
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
        for (const auto& f : files)
        {
            PresetItem item;
            item.name = f.getFileNameWithoutExtension();
            item.category = cat;
            item.isFactory = false;
            item.userFile = f;
            item.description = "User Preset";

            auto xml = juce::XmlDocument::parse(f);
            if (xml)
            {
                auto desc = xml->getStringAttribute("description", "");
                if (desc.isNotEmpty())
                    item.description = desc;
            }
            presets.push_back(item);
        }
    }
    return presets;
}

juce::String PresetManager::serializeEQCatalogPreset(
    const EQPresetDefinition& preset, const juce::String& description,
    bool isFavorite) const
{
    juce::XmlElement xml("LIVEBUS_EQ_PRESET");
    xml.setAttribute("formatVersion", preset.formatVersion);
    xml.setAttribute("name", preset.name);
    xml.setAttribute("description", description);
    xml.setAttribute("category", (int)preset.category);
    xml.setAttribute("favorite", isFavorite);
    for (size_t index = 0; index < preset.bands.size(); ++index)
    {
        const auto& band = preset.bands[index];
        auto* bandXml = xml.createNewChildElement("BAND");
        bandXml->setAttribute("index", (int)index);
        bandXml->setAttribute("type", (int)band.type);
        bandXml->setAttribute("frequencyHz", (double)band.frequencyHz);
        bandXml->setAttribute("gainDb", (double)band.gainDb);
        bandXml->setAttribute("q", (double)band.q);
        bandXml->setAttribute("enabled", band.enabled);
    }
    return xml.toString();
}

bool PresetManager::deserializeEQCatalogPreset(
    const juce::XmlElement& xml, EQPresetDefinition& outPreset,
    bool& outFavorite, juce::String& outError) const
{
    if (!xml.hasTagName("LIVEBUS_EQ_PRESET"))
    {
        outError = "Invalid EQ preset root element.";
        return false;
    }
    if (xml.getIntAttribute("formatVersion", 0) != 1)
    {
        outError = "Unsupported EQ preset XML version.";
        return false;
    }
    if (!xml.hasAttribute("name") || !xml.hasAttribute("description") ||
        !xml.hasAttribute("category") || !xml.hasAttribute("favorite"))
    {
        outError = "EQ preset metadata is incomplete.";
        return false;
    }

    EQPresetDefinition candidate;
    candidate.name = xml.getStringAttribute("name");
    candidate.description = xml.getStringAttribute("description");
    if (candidate.name.length() > 80 ||
        candidate.description.length() > 2048)
    {
        outError = "EQ preset name or description exceeds its size limit.";
        return false;
    }
    candidate.category = (EQPresetCategory)xml.getIntAttribute("category", -1);
    candidate.formatVersion = 1;
    candidate.id = userEQPresetId(candidate.name);
    outFavorite = xml.getBoolAttribute("favorite", false);

    std::array<bool, 8> seenBands{};
    size_t bandCount = 0;
    for (auto* bandXml = xml.getFirstChildElement();
         bandXml != nullptr; bandXml = bandXml->getNextElement())
    {
        if (!bandXml->hasTagName("BAND"))
            continue;
        const int index = bandXml->getIntAttribute("index", -1);
        if (index < 0 || index >= (int)candidate.bands.size() ||
            !bandXml->hasAttribute("type") ||
            !bandXml->hasAttribute("frequencyHz") ||
            !bandXml->hasAttribute("gainDb") ||
            !bandXml->hasAttribute("q") ||
            !bandXml->hasAttribute("enabled") ||
            seenBands[(size_t)index])
        {
            outError = "EQ preset band data is incomplete or duplicated.";
            return false;
        }
        seenBands[(size_t)index] = true;
        auto& band = candidate.bands[(size_t)index];
        band.type = (EQPresetFilterType)bandXml->getIntAttribute("type", -1);
        band.frequencyHz = (float)bandXml->getDoubleAttribute("frequencyHz");
        band.gainDb = (float)bandXml->getDoubleAttribute("gainDb");
        band.q = (float)bandXml->getDoubleAttribute("q");
        band.enabled = bandXml->getBoolAttribute("enabled", true);
        ++bandCount;
    }
    if (bandCount != candidate.bands.size())
    {
        outError = "EQ preset must contain exactly 8 bands.";
        return false;
    }
    if (!EQPresetCatalog::validate(candidate, outError))
        return false;

    outPreset = std::move(candidate);
    outError.clear();
    return true;
}

bool PresetManager::saveEQCatalogPreset(
    const juce::String& name, const EQPresetDefinition& preset,
    const juce::String& description, bool isFavorite, bool overwrite,
    juce::String& outError)
{
    const auto safeName = sanitizePresetName(name);
    if (safeName.isEmpty() || safeName.length() > 80 ||
        description.length() > 2048)
    {
        outError = "Preset name or description is empty or too long.";
        return false;
    }

    auto candidate = preset;
    candidate.id = userEQPresetId(safeName);
    candidate.name = safeName;
    candidate.description = description;
    candidate.formatVersion = 1;
    if (!EQPresetCatalog::validate(candidate, outError))
        return false;

    const auto file = getCategoryDirectory(PresetCategory::EQCatalog)
                          .getChildFile(safeName + ".xml");
    if (file.existsAsFile() && !overwrite)
    {
        outError = "A preset with that name already exists.";
        return false;
    }
    return writeTextAtomically(
        file, serializeEQCatalogPreset(candidate, description, isFavorite),
        outError);
}

bool PresetManager::loadEQCatalogPreset(
    const PresetItem& item, EQPresetDefinition& outPreset,
    juce::String& outError) const
{
    if (item.category != PresetCategory::EQCatalog)
    {
        outError = "The selected item is not an EQ catalog preset.";
        return false;
    }

    if (item.isFactory)
    {
        const auto* factory = EQPresetCatalog::findFactoryPreset(item.factoryId);
        if (factory == nullptr)
        {
            outError = "Factory EQ preset ID was not found.";
            return false;
        }
        outPreset = *factory;
        outError.clear();
        return true;
    }

    if (!isEQUserFile(item, getCategoryDirectory(PresetCategory::EQCatalog)))
    {
        outError = "EQ preset file is missing or outside the preset directory.";
        return false;
    }
    if (item.userFile.getSize() > 256 * 1024)
    {
        outError = "EQ preset file exceeds the 256 KiB size limit.";
        return false;
    }
    auto xml = juce::XmlDocument::parse(item.userFile);
    if (!xml)
    {
        outError = "Could not parse EQ preset XML.";
        return false;
    }
    bool favorite = false;
    return deserializeEQCatalogPreset(*xml, outPreset, favorite, outError);
}

bool PresetManager::setEQCatalogPresetFavorite(
    const PresetItem& item, bool isFavorite, juce::String& outError)
{
    if (item.category != PresetCategory::EQCatalog)
    {
        outError = "The selected item is not an EQ catalog preset.";
        return false;
    }
    if (!item.isFactory)
    {
        EQPresetDefinition preset;
        if (!loadEQCatalogPreset(item, preset, outError))
            return false;
        return writeTextAtomically(
            item.userFile,
            serializeEQCatalogPreset(preset, preset.description, isFavorite),
            outError);
    }

    const auto favoritesFile =
        getCategoryDirectory(PresetCategory::EQCatalog)
            .getChildFile("favorites.xml");
    juce::StringArray ids;
    if (favoritesFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(favoritesFile);
        if (!xml || !xml->hasTagName("LIVEBUS_EQ_FAVORITES"))
        {
            outError = "The EQ favorites file is invalid.";
            return false;
        }
        for (auto* child = xml->getFirstChildElement();
             child != nullptr; child = child->getNextElement())
            if (child->hasTagName("PRESET"))
                ids.addIfNotAlreadyThere(child->getStringAttribute("id"));
    }
    ids.removeString(item.factoryId);
    if (isFavorite)
        ids.addIfNotAlreadyThere(item.factoryId);

    juce::XmlElement favorites("LIVEBUS_EQ_FAVORITES");
    favorites.setAttribute("formatVersion", 1);
    for (const auto& id : ids)
    {
        auto* child = favorites.createNewChildElement("PRESET");
        child->setAttribute("id", id);
    }
    return writeTextAtomically(favoritesFile, favorites.toString(), outError);
}

// ==============================================================================
// Vocal DSP Presets
// ==============================================================================
juce::String PresetManager::serializeVocalDSP(const SceneData::VocalDSPSceneData& vr, const juce::String& name, const juce::String& desc)
{
    juce::XmlElement xml("LIVEBUS_PRESET");
    xml.setAttribute("category", "VocalDSP");
    xml.setAttribute("version", 1);
    xml.setAttribute("name", name);
    xml.setAttribute("description", desc);

    auto* targetElem = xml.createNewChildElement("VOCAL_RACK");
    targetElem->setAttribute("masterEnabled", vr.masterEnabled);
    targetElem->setAttribute("hpfBypassed", vr.hpfBypassed);
    targetElem->setAttribute("gateBypassed", vr.gateBypassed);
    targetElem->setAttribute("eqBypassed", vr.eqBypassed);
    targetElem->setAttribute("deEsserBypassed", vr.deEsserBypassed);
    targetElem->setAttribute("compBypassed", vr.compBypassed);
    targetElem->setAttribute("limiterBypassed", vr.limiterBypassed);

    targetElem->setAttribute("inputGainDb", (double)vr.inputGainDb);
    targetElem->setAttribute("hpfFreq", (double)vr.hpfFreq);
    targetElem->setAttribute("hpfSlope", vr.hpfSlope);

    targetElem->setAttribute("gateMode", vr.gateMode);
    targetElem->setAttribute("gateOpenThresholdDb", (double)vr.gateOpenThresholdDb);
    targetElem->setAttribute("gateCloseThresholdDb", (double)vr.gateCloseThresholdDb);
    targetElem->setAttribute("gateAttackMs", (double)vr.gateAttackMs);
    targetElem->setAttribute("gateHoldMs", (double)vr.gateHoldMs);
    targetElem->setAttribute("gateReleaseMs", (double)vr.gateReleaseMs);
    targetElem->setAttribute("gateFloorDb", (double)vr.gateFloorDb);
    targetElem->setAttribute("gateKeySource", vr.gateKeySource);
    targetElem->setAttribute("gateKeyHpfFreq", (double)vr.gateKeyHpfFreq);
    targetElem->setAttribute("gateKeyLpfFreq", (double)vr.gateKeyLpfFreq);

    targetElem->setAttribute("deEsserType", vr.deEsserType);
    targetElem->setAttribute("deEsserThresholdDb", (double)vr.deEsserThresholdDb);
    targetElem->setAttribute("deEsserFreq", (double)vr.deEsserFreq);
    targetElem->setAttribute("deEsserRangeDb", (double)vr.deEsserRangeDb);

    targetElem->setAttribute("compThresholdDb", (double)vr.compThresholdDb);
    targetElem->setAttribute("compRatio", (double)vr.compRatio);
    targetElem->setAttribute("compAttackMs", (double)vr.compAttackMs);
    targetElem->setAttribute("compReleaseMs", (double)vr.compReleaseMs);
    targetElem->setAttribute("compKnee", vr.compKnee);
    targetElem->setAttribute("compReleaseMode", vr.compReleaseMode);
    targetElem->setAttribute("compMakeupDb", (double)vr.compMakeupDb);
    targetElem->setAttribute("compMixPercent", (double)vr.compMixPercent);
    targetElem->setAttribute("compKeySource", vr.compKeySource);
    targetElem->setAttribute("compKeyHpfFreq", (double)vr.compKeyHpfFreq);
    targetElem->setAttribute("compKeyLpfFreq", (double)vr.compKeyLpfFreq);

    targetElem->setAttribute("limiterCeilingDb", (double)vr.limiterCeilingDb);
    targetElem->setAttribute("limiterReleaseMs", (double)vr.limiterReleaseMs);
    targetElem->setAttribute("outputGainDb", (double)vr.outputGainDb);

    return xml.toString();
}

bool PresetManager::deserializeVocalDSP(const juce::XmlElement& xml, SceneData::VocalDSPSceneData& outData, juce::String& outName)
{
    if (!xml.hasTagName("LIVEBUS_PRESET") || xml.getStringAttribute("category") != "VocalDSP")
        return false;

    outName = xml.getStringAttribute("name", "Vocal Preset");
    auto* targetElem = xml.getChildByName("VOCAL_RACK");
    if (!targetElem) return false;

    outData.masterEnabled = targetElem->getBoolAttribute("masterEnabled", true);
    outData.hpfBypassed = targetElem->getBoolAttribute("hpfBypassed", false);
    outData.gateBypassed = targetElem->getBoolAttribute("gateBypassed", false);
    outData.eqBypassed = targetElem->getBoolAttribute("eqBypassed", false);
    outData.deEsserBypassed = targetElem->getBoolAttribute("deEsserBypassed", false);
    outData.compBypassed = targetElem->getBoolAttribute("compBypassed", false);
    outData.limiterBypassed = targetElem->getBoolAttribute("limiterBypassed", false);

    outData.inputGainDb = juce::jlimit(-24.0f, 24.0f, (float)targetElem->getDoubleAttribute("inputGainDb", 0.0));
    outData.hpfFreq = juce::jlimit(20.0f, 500.0f, (float)targetElem->getDoubleAttribute("hpfFreq", 80.0));
    outData.hpfSlope = juce::jlimit(0, 3, targetElem->getIntAttribute("hpfSlope", 1));

    outData.gateMode = juce::jlimit(0, 1, targetElem->getIntAttribute("gateMode", 0));
    outData.gateOpenThresholdDb = juce::jlimit(-80.0f, 0.0f, (float)targetElem->getDoubleAttribute("gateOpenThresholdDb", -42.0));
    outData.gateCloseThresholdDb = juce::jlimit(-80.0f, 0.0f, (float)targetElem->getDoubleAttribute("gateCloseThresholdDb", -50.0));
    outData.gateAttackMs = juce::jlimit(0.1f, 100.0f, (float)targetElem->getDoubleAttribute("gateAttackMs", 1.0));
    outData.gateHoldMs = juce::jlimit(0.0f, 500.0f, (float)targetElem->getDoubleAttribute("gateHoldMs", 20.0));
    outData.gateReleaseMs = juce::jlimit(5.0f, 1000.0f, (float)targetElem->getDoubleAttribute("gateReleaseMs", 100.0));
    outData.gateFloorDb = juce::jlimit(-90.0f, 0.0f, (float)targetElem->getDoubleAttribute("gateFloorDb", -60.0));
    outData.gateKeySource = juce::jlimit(0, 1, targetElem->getIntAttribute("gateKeySource", 0));
    outData.gateKeyHpfFreq = juce::jlimit(20.0f, 1000.0f, (float)targetElem->getDoubleAttribute("gateKeyHpfFreq", 70.0));
    outData.gateKeyLpfFreq = juce::jlimit(1000.0f, 20000.0f, (float)targetElem->getDoubleAttribute("gateKeyLpfFreq", 4000.0));

    outData.deEsserType = juce::jlimit(0, 1, targetElem->getIntAttribute("deEsserType", 0));
    outData.deEsserThresholdDb = juce::jlimit(-60.0f, 0.0f, (float)targetElem->getDoubleAttribute("deEsserThresholdDb", -16.0));
    outData.deEsserFreq = juce::jlimit(1000.0f, 16000.0f, (float)targetElem->getDoubleAttribute("deEsserFreq", 5500.0));
    outData.deEsserRangeDb = juce::jlimit(-24.0f, 0.0f, (float)targetElem->getDoubleAttribute("deEsserRangeDb", -8.0));

    outData.compThresholdDb = juce::jlimit(-60.0f, 0.0f, (float)targetElem->getDoubleAttribute("compThresholdDb", -24.0));
    outData.compRatio = juce::jlimit(1.0f, 20.0f, (float)targetElem->getDoubleAttribute("compRatio", 3.0));
    outData.compAttackMs = juce::jlimit(0.1f, 200.0f, (float)targetElem->getDoubleAttribute("compAttackMs", 10.0));
    outData.compReleaseMs = juce::jlimit(5.0f, 2000.0f, (float)targetElem->getDoubleAttribute("compReleaseMs", 120.0));
    outData.compKnee = juce::jlimit(0, 2, targetElem->getIntAttribute("compKnee", 1));
    outData.compReleaseMode = juce::jlimit(0, 1, targetElem->getIntAttribute("compReleaseMode", 0));
    outData.compMakeupDb = juce::jlimit(0.0f, 24.0f, (float)targetElem->getDoubleAttribute("compMakeupDb", 3.0));
    outData.compMixPercent = juce::jlimit(0.0f, 100.0f, (float)targetElem->getDoubleAttribute("compMixPercent", 100.0));
    outData.compKeySource = juce::jlimit(0, 1, targetElem->getIntAttribute("compKeySource", 0));
    outData.compKeyHpfFreq = juce::jlimit(20.0f, 1000.0f, (float)targetElem->getDoubleAttribute("compKeyHpfFreq", 60.0));
    outData.compKeyLpfFreq = juce::jlimit(1000.0f, 20000.0f, (float)targetElem->getDoubleAttribute("compKeyLpfFreq", 15000.0));

    outData.limiterCeilingDb = juce::jlimit(-24.0f, 0.0f, (float)targetElem->getDoubleAttribute("limiterCeilingDb", -1.0));
    outData.limiterReleaseMs = juce::jlimit(5.0f, 500.0f, (float)targetElem->getDoubleAttribute("limiterReleaseMs", 50.0));
    outData.outputGainDb = juce::jlimit(-24.0f, 24.0f, (float)targetElem->getDoubleAttribute("outputGainDb", 0.0));
    return true;
}

bool PresetManager::saveVocalDSPPreset(const juce::String& name, const SceneData::VocalDSPSceneData& data, const juce::String& description)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto xml = serializeVocalDSP(data, safe, description);
    auto file = getCategoryDirectory(PresetCategory::VocalDSP).getChildFile(safe + ".xml");
    return file.replaceWithText(xml);
}

bool PresetManager::loadVocalDSPPreset(const PresetItem& item, SceneData::VocalDSPSceneData& outData, juce::String& outError)
{
    if (item.isFactory)
    {
        // Factory presets definitions
        outData = SceneData::VocalDSPSceneData();
        if (item.name == "Broadcast Lead Vocal")
        {
            outData.hpfFreq = 80.0f;
            outData.gateOpenThresholdDb = -45.0f;
            outData.gateCloseThresholdDb = -52.0f;
            outData.deEsserFreq = 5500.0f;
            outData.deEsserThresholdDb = -18.0f;
            outData.compThresholdDb = -20.0f;
            outData.compRatio = 3.0f;
            outData.compMakeupDb = 3.5f;
            outData.limiterCeilingDb = -1.0f;
            return true;
        }
        else if (item.name == "Voiceover Warm & Intimate")
        {
            outData.hpfFreq = 65.0f;
            outData.gateOpenThresholdDb = -48.0f;
            outData.compThresholdDb = -22.0f;
            outData.compRatio = 2.5f;
            outData.compKnee = 0; // Soft knee
            outData.compMakeupDb = 4.0f;
            outData.limiterCeilingDb = -1.2f;
            return true;
        }
        else if (item.name == "Aggressive Rock Vocal")
        {
            outData.hpfFreq = 100.0f;
            outData.gateOpenThresholdDb = -38.0f;
            outData.gateAttackMs = 0.5f;
            outData.compThresholdDb = -28.0f;
            outData.compRatio = 6.0f;
            outData.compAttackMs = 2.0f;
            outData.compReleaseMs = 80.0f;
            outData.compMakeupDb = 7.0f;
            outData.limiterCeilingDb = -0.5f;
            return true;
        }
        else if (item.name == "Acoustic Guitar Dynamics")
        {
            outData.hpfFreq = 85.0f;
            outData.gateBypassed = true;
            outData.deEsserBypassed = true;
            outData.compThresholdDb = -16.0f;
            outData.compRatio = 2.0f;
            outData.compAttackMs = 20.0f;
            outData.compReleaseMs = 150.0f;
            outData.compMakeupDb = 2.0f;
            outData.limiterCeilingDb = -1.0f;
            return true;
        }
        else if (item.name == "Podcast Dual Host")
        {
            outData.hpfFreq = 80.0f;
            outData.gateOpenThresholdDb = -40.0f;
            outData.gateHoldMs = 50.0f;
            outData.compThresholdDb = -24.0f;
            outData.compRatio = 3.5f;
            outData.compMakeupDb = 4.5f;
            outData.limiterCeilingDb = -1.5f;
            return true;
        }
        else if (item.name == "Stream Bus Master Leveler")
        {
            outData.hpfBypassed = true;
            outData.gateBypassed = true;
            outData.deEsserBypassed = true;
            outData.compThresholdDb = -14.0f;
            outData.compRatio = 2.0f;
            outData.compAttackMs = 30.0f;
            outData.compReleaseMs = 100.0f;
            outData.compMakeupDb = 2.0f;
            outData.limiterCeilingDb = -1.0f;
            return true;
        }
    }

    if (item.userFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(item.userFile);
        if (!xml)
        {
            outError = "Failed to parse XML preset: " + item.userFile.getFileName();
            return false;
        }
        juce::String name;
        return deserializeVocalDSP(*xml, outData, name);
    }

    outError = "Preset file not found: " + item.name;
    return false;
}

// ==============================================================================
// 8-Band EQ Presets
// ==============================================================================
juce::String PresetManager::serializeEightBandEQ(const SceneData::EightBandEQSceneData& eq, const juce::String& name, const juce::String& desc)
{
    juce::XmlElement xml("LIVEBUS_PRESET");
    xml.setAttribute("category", "EightBandEQ");
    xml.setAttribute("version", 1);
    xml.setAttribute("name", name);
    xml.setAttribute("description", desc);

    auto* targetElem = xml.createNewChildElement("EIGHT_BAND_EQ");
    targetElem->setAttribute("masterEnabled", eq.masterEnabled);
    for (int b = 0; b < 8; ++b)
    {
        auto* bElem = targetElem->createNewChildElement("BAND");
        bElem->setAttribute("index", b);
        bElem->setAttribute("type", eq.bands[b].type);
        bElem->setAttribute("freq", (double)eq.bands[b].freq);
        bElem->setAttribute("gainDb", (double)eq.bands[b].gainDb);
        bElem->setAttribute("q", (double)eq.bands[b].q);
        bElem->setAttribute("enabled", eq.bands[b].enabled);
    }

    return xml.toString();
}

bool PresetManager::deserializeEightBandEQ(const juce::XmlElement& xml, SceneData::EightBandEQSceneData& outData, juce::String& outName)
{
    if (!xml.hasTagName("LIVEBUS_PRESET") || xml.getStringAttribute("category") != "EightBandEQ")
        return false;

    outName = xml.getStringAttribute("name", "EQ Preset");
    auto* targetElem = xml.getChildByName("EIGHT_BAND_EQ");
    if (!targetElem) return false;

    outData.masterEnabled = targetElem->getBoolAttribute("masterEnabled", true);
    for (auto* bElem : targetElem->getChildIterator())
    {
        if (bElem->hasTagName("BAND"))
        {
            int b = bElem->getIntAttribute("index", -1);
            if (b >= 0 && b < 8)
            {
                outData.bands[b].type = juce::jlimit(0, 4, bElem->getIntAttribute("type", 2));
                outData.bands[b].freq = juce::jlimit(20.0f, 20000.0f, (float)bElem->getDoubleAttribute("freq", 1000.0));
                outData.bands[b].gainDb = juce::jlimit(-24.0f, 24.0f, (float)bElem->getDoubleAttribute("gainDb", 0.0));
                outData.bands[b].q = juce::jlimit(0.1f, 20.0f, (float)bElem->getDoubleAttribute("q", 0.707));
                outData.bands[b].enabled = bElem->getBoolAttribute("enabled", true);
            }
        }
    }
    return true;
}

bool PresetManager::saveEightBandEQPreset(const juce::String& name, const SceneData::EightBandEQSceneData& data, const juce::String& description)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto xml = serializeEightBandEQ(data, safe, description);
    auto file = getCategoryDirectory(PresetCategory::EightBandEQ).getChildFile(safe + ".xml");
    return file.replaceWithText(xml);
}

bool PresetManager::loadEightBandEQPreset(const PresetItem& item, SceneData::EightBandEQSceneData& outData, juce::String& outError)
{
    if (item.isFactory)
    {
        outData = SceneData::EightBandEQSceneData();
        if (item.name == "Vocal Air & Clarity")
        {
            outData.bands[0] = { 0, 80.0f, 0.0f, 0.71f, true };     // HPF
            outData.bands[1] = { 1, 150.0f, 2.0f, 0.71f, true };    // Low shelf
            outData.bands[2] = { 2, 400.0f, -3.0f, 1.4f, true };    // Cut boxiness
            outData.bands[3] = { 2, 1000.0f, 0.0f, 1.0f, false };
            outData.bands[4] = { 2, 3000.0f, 4.0f, 1.2f, true };    // Presence
            outData.bands[5] = { 2, 6000.0f, 1.5f, 1.0f, true };
            outData.bands[6] = { 3, 12000.0f, 3.5f, 0.71f, true };  // High shelf air
            outData.bands[7] = { 4, 18000.0f, 0.0f, 0.71f, false };
            return true;
        }
        else if (item.name == "Broadcast Male Voice")
        {
            outData.bands[0] = { 0, 75.0f, 0.0f, 0.71f, true };     // HPF
            outData.bands[1] = { 2, 120.0f, 3.5f, 1.2f, true };     // Warmth
            outData.bands[2] = { 2, 350.0f, -4.0f, 1.4f, true };    // Mud cut
            outData.bands[3] = { 2, 1200.0f, 1.0f, 1.0f, true };
            outData.bands[4] = { 2, 2500.0f, 3.5f, 1.2f, true };    // Clarity
            outData.bands[5] = { 2, 5000.0f, 2.0f, 1.0f, true };
            outData.bands[6] = { 3, 10000.0f, 2.5f, 0.71f, true };  // High shelf
            outData.bands[7] = { 4, 16000.0f, 0.0f, 0.71f, true };  // LPF
            return true;
        }
        else if (item.name == "Acoustic Guitar Bright")
        {
            outData.bands[0] = { 0, 90.0f, 0.0f, 0.71f, true };     // HPF
            outData.bands[1] = { 2, 220.0f, -2.5f, 1.4f, true };    // Boominess cut
            outData.bands[2] = { 2, 500.0f, -3.0f, 1.2f, true };
            outData.bands[3] = { 2, 1800.0f, 1.0f, 1.0f, true };
            outData.bands[4] = { 2, 4500.0f, 3.0f, 1.2f, true };    // String sparkle
            outData.bands[5] = { 3, 10000.0f, 4.0f, 0.71f, true };  // Air
            outData.bands[6] = { 2, 12000.0f, 0.0f, 1.0f, false };
            outData.bands[7] = { 4, 19000.0f, 0.0f, 0.71f, true };
            return true;
        }
        else if (item.name == "Bass & Kick Scooped")
        {
            outData.bands[0] = { 0, 35.0f, 0.0f, 0.71f, true };     // Sub HPF
            outData.bands[1] = { 2, 65.0f, 4.5f, 1.5f, true };      // Punch
            outData.bands[2] = { 2, 250.0f, -4.0f, 1.4f, true };    // Mud cut
            outData.bands[3] = { 2, 500.0f, -5.0f, 1.5f, true };    // Boxiness scoop
            outData.bands[4] = { 2, 2000.0f, 3.0f, 1.2f, true };    // Beater snap
            outData.bands[5] = { 2, 4000.0f, 0.0f, 1.0f, false };
            outData.bands[6] = { 4, 8000.0f, 0.0f, 0.71f, true };   // LPF
            outData.bands[7] = { 4, 15000.0f, 0.0f, 0.71f, false };
            return true;
        }
        else if (item.name == "Telephone Effect")
        {
            outData.bands[0] = { 0, 500.0f, 0.0f, 0.71f, true };    // High pass 500Hz
            outData.bands[1] = { 2, 800.0f, 3.0f, 2.0f, true };
            outData.bands[2] = { 2, 1500.0f, 6.0f, 1.8f, true };    // Resonant mid peak
            outData.bands[3] = { 2, 2500.0f, 4.0f, 2.0f, true };
            outData.bands[4] = { 4, 3500.0f, 0.0f, 0.71f, true };   // Low pass 3.5kHz
            outData.bands[5] = { 4, 4000.0f, 0.0f, 0.71f, false };
            outData.bands[6] = { 4, 8000.0f, 0.0f, 0.71f, false };
            outData.bands[7] = { 4, 12000.0f, 0.0f, 0.71f, false };
            return true;
        }
        else if (item.name == "Flat Neutral")
        {
            for (int b = 0; b < 8; ++b)
            {
                outData.bands[b].gainDb = 0.0f;
                outData.bands[b].enabled = true;
            }
            return true;
        }
    }

    if (item.userFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(item.userFile);
        if (!xml)
        {
            outError = "Failed to parse XML preset: " + item.userFile.getFileName();
            return false;
        }
        juce::String name;
        return deserializeEightBandEQ(*xml, outData, name);
    }

    outError = "Preset file not found: " + item.name;
    return false;
}

// ==============================================================================
// Channel Strip Presets
// ==============================================================================
juce::String PresetManager::serializeChannelStrip(const SceneData::ChannelSceneData& ch, const juce::String& name, const juce::String& desc)
{
    juce::XmlElement xml("LIVEBUS_PRESET");
    xml.setAttribute("category", "ChannelStrip");
    xml.setAttribute("version", 1);
    xml.setAttribute("name", name);
    xml.setAttribute("description", desc);

    auto* c = xml.createNewChildElement("CHANNEL");
    c->setAttribute("name", ch.name);
    c->setAttribute("gain", (double)ch.gain);
    c->setAttribute("pan", (double)ch.pan);
    c->setAttribute("mute", ch.mute);
    c->setAttribute("solo", ch.solo);
    c->setAttribute("streamRoute", ch.streamRoute);
    c->setAttribute("monitorRoute", ch.monitorRoute);
    c->setAttribute("streamSend", (double)ch.streamSend);
    c->setAttribute("monitorSend", (double)ch.monitorSend);

    return xml.toString();
}

bool PresetManager::deserializeChannelStrip(const juce::XmlElement& xml, SceneData::ChannelSceneData& outData, juce::String& outName)
{
    if (!xml.hasTagName("LIVEBUS_PRESET") || xml.getStringAttribute("category") != "ChannelStrip")
        return false;

    outName = xml.getStringAttribute("name", "Channel Preset");
    auto* c = xml.getChildByName("CHANNEL");
    if (!c) return false;

    outData.name = c->getStringAttribute("name", outName);
    outData.gain = juce::jlimit(0.0f, 2.0f, (float)c->getDoubleAttribute("gain", 0.707));
    outData.pan = juce::jlimit(-1.0f, 1.0f, (float)c->getDoubleAttribute("pan", 0.0));
    outData.mute = c->getBoolAttribute("mute", false);
    outData.solo = c->getBoolAttribute("solo", false);
    outData.streamRoute = c->getBoolAttribute("streamRoute", true);
    outData.monitorRoute = c->getBoolAttribute("monitorRoute", true);
    outData.streamSend = juce::jlimit(0.0f, 2.0f, (float)c->getDoubleAttribute("streamSend", 1.0));
    outData.monitorSend = juce::jlimit(0.0f, 2.0f, (float)c->getDoubleAttribute("monitorSend", 1.0));
    return true;
}

bool PresetManager::saveChannelStripPreset(const juce::String& name, const SceneData::ChannelSceneData& data, const juce::String& description)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto xml = serializeChannelStrip(data, safe, description);
    auto file = getCategoryDirectory(PresetCategory::ChannelStrip).getChildFile(safe + ".xml");
    return file.replaceWithText(xml);
}

bool PresetManager::loadChannelStripPreset(const PresetItem& item, SceneData::ChannelSceneData& outData, juce::String& outError)
{
    if (item.isFactory)
    {
        outData = SceneData::ChannelSceneData();
        if (item.name == "Standard Broadcast Mic")
        {
            outData.name = "Mic";
            outData.gain = 1.0f;
            outData.pan = 0.0f;
            outData.mute = false;
            outData.solo = false;
            outData.streamRoute = true;
            outData.monitorRoute = true;
            outData.streamSend = 1.0f;
            outData.monitorSend = 1.0f;
            return true;
        }
        else if (item.name == "Acoustic Instrument DI")
        {
            outData.name = "Guitar";
            outData.gain = 0.707f;
            outData.pan = -0.2f;
            outData.mute = false;
            outData.solo = false;
            outData.streamRoute = true;
            outData.monitorRoute = true;
            outData.streamSend = 0.9f;
            outData.monitorSend = 1.0f;
            return true;
        }
        else if (item.name == "Stereo Line In")
        {
            outData.name = "Music";
            outData.gain = 0.8f;
            outData.pan = 0.0f;
            outData.mute = false;
            outData.streamRoute = true;
            outData.monitorRoute = true;
            return true;
        }
        else if (item.name == "Talkback Mic")
        {
            outData.name = "Talkback";
            outData.gain = 0.5f;
            outData.pan = 0.0f;
            outData.mute = true;
            outData.streamRoute = false;
            outData.monitorRoute = true;
            return true;
        }
    }

    if (item.userFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(item.userFile);
        if (!xml)
        {
            outError = "Failed to parse XML preset: " + item.userFile.getFileName();
            return false;
        }
        juce::String name;
        return deserializeChannelStrip(*xml, outData, name);
    }

    outError = "Preset file not found: " + item.name;
    return false;
}

// ==============================================================================
// Bus DSP Presets
// ==============================================================================
juce::String PresetManager::serializeBusDSP(const SceneData::BusSceneDSP& bus, const juce::String& name, const juce::String& desc)
{
    juce::XmlElement xml("LIVEBUS_PRESET");
    xml.setAttribute("category", "BusDSP");
    xml.setAttribute("version", 1);
    xml.setAttribute("name", name);
    xml.setAttribute("description", desc);

    auto* be = xml.createNewChildElement("BUS");
    be->setAttribute("hpfEnabled", bus.hpfEnabled);
    be->setAttribute("hpfFreq", (double)bus.hpfFreq);

    be->setAttribute("eqEnabled", bus.eqEnabled);
    be->setAttribute("eqLowFreq", (double)bus.eqLowFreq);
    be->setAttribute("eqLowGainDb", (double)bus.eqLowGainDb);
    be->setAttribute("eqMid1Freq", (double)bus.eqMid1Freq);
    be->setAttribute("eqMid1GainDb", (double)bus.eqMid1GainDb);
    be->setAttribute("eqMid1Q", (double)bus.eqMid1Q);
    be->setAttribute("eqFreq", (double)bus.eqFreq);
    be->setAttribute("eqQ", (double)bus.eqQ);
    be->setAttribute("eqGainDb", (double)bus.eqGainDb);
    be->setAttribute("eqHighFreq", (double)bus.eqHighFreq);
    be->setAttribute("eqHighGainDb", (double)bus.eqHighGainDb);

    be->setAttribute("gateEnabled", bus.gateEnabled);
    be->setAttribute("gateThresholdDb", (double)bus.gateThresholdDb);
    be->setAttribute("gateRatio", (double)bus.gateRatio);
    be->setAttribute("gateAttackMs", (double)bus.gateAttackMs);
    be->setAttribute("gateReleaseMs", (double)bus.gateReleaseMs);

    be->setAttribute("compEnabled", bus.compEnabled);
    be->setAttribute("compThresholdDb", (double)bus.compThresholdDb);
    be->setAttribute("compRatio", (double)bus.compRatio);
    be->setAttribute("compAttackMs", (double)bus.compAttackMs);
    be->setAttribute("compReleaseMs", (double)bus.compReleaseMs);

    be->setAttribute("limiterEnabled", bus.limiterEnabled);
    be->setAttribute("limiterCeilingDb", (double)bus.limiterCeilingDb);
    be->setAttribute("limiterReleaseMs", (double)bus.limiterReleaseMs);

    be->setAttribute("reverbEnabled", bus.reverbEnabled);
    be->setAttribute("reverbRoomSize", (double)bus.reverbRoomSize);
    be->setAttribute("reverbDamping", (double)bus.reverbDamping);
    be->setAttribute("reverbWetLevel", (double)bus.reverbWetLevel);
    be->setAttribute("reverbDryLevel", (double)bus.reverbDryLevel);

    be->setAttribute("delayEnabled", bus.delayEnabled);
    be->setAttribute("delayTimeMs", (double)bus.delayTimeMs);
    be->setAttribute("delayFeedback", (double)bus.delayFeedback);
    be->setAttribute("delayWetLevel", (double)bus.delayWetLevel);

    be->setAttribute("masterGain", (double)bus.masterGain);
    be->setAttribute("masterPan", (double)bus.masterPan);
    be->setAttribute("masterMute", bus.masterMute);

    return xml.toString();
}

bool PresetManager::deserializeBusDSP(const juce::XmlElement& xml, SceneData::BusSceneDSP& outData, juce::String& outName)
{
    if (!xml.hasTagName("LIVEBUS_PRESET") || xml.getStringAttribute("category") != "BusDSP")
        return false;

    outName = xml.getStringAttribute("name", "Bus Preset");
    auto* be = xml.getChildByName("BUS");
    if (!be) return false;

    outData.hpfEnabled = be->getBoolAttribute("hpfEnabled", true);
    outData.hpfFreq = juce::jlimit(20.0f, 500.0f, (float)be->getDoubleAttribute("hpfFreq", 80.0));

    outData.eqEnabled = be->getBoolAttribute("eqEnabled", true);
    outData.eqLowFreq = juce::jlimit(20.0f, 500.0f, (float)be->getDoubleAttribute("eqLowFreq", 100.0));
    outData.eqLowGainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqLowGainDb", 0.0));
    outData.eqMid1Freq = juce::jlimit(100.0f, 5000.0f, (float)be->getDoubleAttribute("eqMid1Freq", 500.0));
    outData.eqMid1GainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqMid1GainDb", 0.0));
    outData.eqMid1Q = juce::jlimit(0.1f, 10.0f, (float)be->getDoubleAttribute("eqMid1Q", 0.707));
    outData.eqFreq = juce::jlimit(200.0f, 10000.0f, (float)be->getDoubleAttribute("eqFreq", 2500.0));
    outData.eqQ = juce::jlimit(0.1f, 10.0f, (float)be->getDoubleAttribute("eqQ", 0.707));
    outData.eqGainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqGainDb", 0.0));
    outData.eqHighFreq = juce::jlimit(1000.0f, 20000.0f, (float)be->getDoubleAttribute("eqHighFreq", 10000.0));
    outData.eqHighGainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqHighGainDb", 0.0));

    outData.gateEnabled = be->getBoolAttribute("gateEnabled", false);
    outData.gateThresholdDb = juce::jlimit(-80.0f, 0.0f, (float)be->getDoubleAttribute("gateThresholdDb", -45.0));
    outData.gateRatio = juce::jlimit(1.0f, 20.0f, (float)be->getDoubleAttribute("gateRatio", 4.0));
    outData.gateAttackMs = juce::jlimit(0.1f, 100.0f, (float)be->getDoubleAttribute("gateAttackMs", 5.0));
    outData.gateReleaseMs = juce::jlimit(5.0f, 1000.0f, (float)be->getDoubleAttribute("gateReleaseMs", 100.0));

    outData.compEnabled = be->getBoolAttribute("compEnabled", true);
    outData.compThresholdDb = juce::jlimit(-60.0f, 0.0f, (float)be->getDoubleAttribute("compThresholdDb", -18.0));
    outData.compRatio = juce::jlimit(1.0f, 20.0f, (float)be->getDoubleAttribute("compRatio", 3.0));
    outData.compAttackMs = juce::jlimit(0.1f, 200.0f, (float)be->getDoubleAttribute("compAttackMs", 10.0));
    outData.compReleaseMs = juce::jlimit(5.0f, 2000.0f, (float)be->getDoubleAttribute("compReleaseMs", 100.0));

    outData.limiterEnabled = be->getBoolAttribute("limiterEnabled", true);
    outData.limiterCeilingDb = juce::jlimit(-24.0f, 0.0f, (float)be->getDoubleAttribute("limiterCeilingDb", -1.0));
    outData.limiterReleaseMs = juce::jlimit(5.0f, 500.0f, (float)be->getDoubleAttribute("limiterReleaseMs", 80.0));

    outData.reverbEnabled = be->getBoolAttribute("reverbEnabled", false);
    outData.reverbRoomSize = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbRoomSize", 0.5));
    outData.reverbDamping = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbDamping", 0.5));
    outData.reverbWetLevel = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbWetLevel", 0.3));
    outData.reverbDryLevel = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbDryLevel", 1.0));

    outData.delayEnabled = be->getBoolAttribute("delayEnabled", false);
    outData.delayTimeMs = juce::jlimit(1.0f, 1000.0f, (float)be->getDoubleAttribute("delayTimeMs", 350.0));
    outData.delayFeedback = juce::jlimit(0.0f, 0.95f, (float)be->getDoubleAttribute("delayFeedback", 0.35));
    outData.delayWetLevel = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("delayWetLevel", 0.25));

    outData.masterGain = juce::jlimit(0.0f, 2.0f, (float)be->getDoubleAttribute("masterGain", 1.0));
    outData.masterPan = juce::jlimit(-1.0f, 1.0f, (float)be->getDoubleAttribute("masterPan", 0.0));
    outData.masterMute = be->getBoolAttribute("masterMute", false);
    return true;
}

bool PresetManager::saveBusDSPPreset(const juce::String& name, const SceneData::BusSceneDSP& data, const juce::String& description)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto xml = serializeBusDSP(data, safe, description);
    auto file = getCategoryDirectory(PresetCategory::BusDSP).getChildFile(safe + ".xml");
    return file.replaceWithText(xml);
}

bool PresetManager::loadBusDSPPreset(const PresetItem& item, SceneData::BusSceneDSP& outData, juce::String& outError)
{
    if (item.isFactory)
    {
        outData = SceneData::BusSceneDSP();
        if (item.name == "Live Stream Master")
        {
            outData.hpfEnabled = true;
            outData.hpfFreq = 40.0f;
            outData.compEnabled = true;
            outData.compThresholdDb = -16.0f;
            outData.compRatio = 2.0f;
            outData.limiterEnabled = true;
            outData.limiterCeilingDb = -1.0f;
            outData.reverbEnabled = false;
            return true;
        }
        else if (item.name == "In-Ear Monitor Crisp")
        {
            outData.hpfEnabled = true;
            outData.hpfFreq = 50.0f;
            outData.eqEnabled = true;
            outData.eqHighGainDb = 2.0f;
            outData.compEnabled = true;
            outData.compThresholdDb = -18.0f;
            outData.limiterEnabled = true;
            outData.limiterCeilingDb = -0.8f;
            return true;
        }
        else if (item.name == "Club Broadcast Punch")
        {
            outData.hpfEnabled = true;
            outData.hpfFreq = 30.0f;
            outData.eqEnabled = true;
            outData.eqLowGainDb = 3.0f;
            outData.compEnabled = true;
            outData.compThresholdDb = -20.0f;
            outData.compRatio = 4.0f;
            outData.limiterEnabled = true;
            outData.limiterCeilingDb = -0.5f;
            return true;
        }
    }

    if (item.userFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(item.userFile);
        if (!xml)
        {
            outError = "Failed to parse XML preset: " + item.userFile.getFileName();
            return false;
        }
        juce::String name;
        return deserializeBusDSP(*xml, outData, name);
    }

    outError = "Preset file not found: " + item.name;
    return false;
}

// ==============================================================================
// Preset CRUD
// ==============================================================================
bool PresetManager::deleteUserPreset(const PresetItem& item)
{
    if (item.isFactory) return false; // Factory presets cannot be deleted
    if (item.category == PresetCategory::EQCatalog)
    {
        if (!isEQUserFile(item, getCategoryDirectory(PresetCategory::EQCatalog)))
            return false;
        return item.userFile.deleteFile();
    }
    if (item.userFile.existsAsFile())
        return item.userFile.deleteFile();
    return false;
}

bool PresetManager::renameUserPreset(const PresetItem& item, const juce::String& newName)
{
    if (item.isFactory) return false;
    auto safe = sanitizePresetName(newName);
    if (safe.isEmpty()) return false;

    auto newFile = item.userFile.getSiblingFile(safe + ".xml");
    if (item.category == PresetCategory::EQCatalog)
    {
        if (!isEQUserFile(item, getCategoryDirectory(PresetCategory::EQCatalog)) ||
            newFile.existsAsFile())
            return false;
        EQPresetDefinition preset;
        juce::String error;
        if (!loadEQCatalogPreset(item, preset, error))
            return false;
        preset.name = safe;
        preset.id = userEQPresetId(safe);
        const auto xml = serializeEQCatalogPreset(
            preset, preset.description, item.isFavorite);
        if (!writeTextAtomically(newFile, xml, error))
            return false;
        if (!item.userFile.deleteFile())
        {
            newFile.deleteFile();
            return false;
        }
        return true;
    }
    if (item.userFile.existsAsFile() && !newFile.existsAsFile())
    {
        return item.userFile.moveFileTo(newFile);
    }
    return false;
}

bool PresetManager::duplicateUserPreset(const PresetItem& item, const juce::String& newName)
{
    auto safe = sanitizePresetName(newName);
    if (safe.isEmpty()) return false;

    auto newFile = getCategoryDirectory(item.category).getChildFile(safe + ".xml");
    if (newFile.existsAsFile()) return false;

    if (item.category == PresetCategory::EQCatalog)
    {
        EQPresetDefinition preset;
        juce::String error;
        if (!loadEQCatalogPreset(item, preset, error))
            return false;
        preset.name = safe;
        preset.id = userEQPresetId(safe);
        return saveEQCatalogPreset(safe, preset,
                                   "User copy of " + item.name,
                                   false, false, error);
    }

    if (item.isFactory)
    {
        // Load factory preset then save as user preset
        switch (item.category)
        {
            case PresetCategory::VocalDSP:
            {
                SceneData::VocalDSPSceneData data;
                juce::String err;
                if (loadVocalDSPPreset(item, data, err))
                    return saveVocalDSPPreset(safe, data, "User duplicate of " + item.name);
                break;
            }
            case PresetCategory::EightBandEQ:
            {
                SceneData::EightBandEQSceneData data;
                juce::String err;
                if (loadEightBandEQPreset(item, data, err))
                    return saveEightBandEQPreset(safe, data, "User duplicate of " + item.name);
                break;
            }
            case PresetCategory::ChannelStrip:
            {
                SceneData::ChannelSceneData data;
                juce::String err;
                if (loadChannelStripPreset(item, data, err))
                    return saveChannelStripPreset(safe, data, "User duplicate of " + item.name);
                break;
            }
            case PresetCategory::BusDSP:
            {
                SceneData::BusSceneDSP data;
                juce::String err;
                if (loadBusDSPPreset(item, data, err))
                    return saveBusDSPPreset(safe, data, "User duplicate of " + item.name);
                break;
            }
            default: break;
        }
        return false;
    }
    else if (item.userFile.existsAsFile())
    {
        return item.userFile.copyFileTo(newFile);
    }
    return false;
}

bool PresetManager::exportPresetToFile(const PresetItem& item, const juce::File& destFile)
{
    if (item.category == PresetCategory::EQCatalog)
    {
        EQPresetDefinition preset;
        juce::String error;
        if (!loadEQCatalogPreset(item, preset, error))
            return false;
        return writeTextAtomically(
            destFile,
            serializeEQCatalogPreset(preset, preset.description,
                                     item.isFavorite),
            error);
    }

    if (item.isFactory)
    {
        // Export by serializing factory values
        switch (item.category)
        {
            case PresetCategory::VocalDSP:
            {
                SceneData::VocalDSPSceneData data;
                juce::String err;
                if (loadVocalDSPPreset(item, data, err))
                    return destFile.replaceWithText(serializeVocalDSP(data, item.name, item.description));
                break;
            }
            case PresetCategory::EightBandEQ:
            {
                SceneData::EightBandEQSceneData data;
                juce::String err;
                if (loadEightBandEQPreset(item, data, err))
                    return destFile.replaceWithText(serializeEightBandEQ(data, item.name, item.description));
                break;
            }
            case PresetCategory::ChannelStrip:
            {
                SceneData::ChannelSceneData data;
                juce::String err;
                if (loadChannelStripPreset(item, data, err))
                    return destFile.replaceWithText(serializeChannelStrip(data, item.name, item.description));
                break;
            }
            case PresetCategory::BusDSP:
            {
                SceneData::BusSceneDSP data;
                juce::String err;
                if (loadBusDSPPreset(item, data, err))
                    return destFile.replaceWithText(serializeBusDSP(data, item.name, item.description));
                break;
            }
            default: break;
        }
        return false;
    }
    else if (item.userFile.existsAsFile())
    {
        return item.userFile.copyFileTo(destFile);
    }
    return false;
}

bool PresetManager::importPresetFromFile(PresetCategory cat, const juce::File& srcFile, juce::String& outName, juce::String& outError)
{
    if (!srcFile.existsAsFile())
    {
        outError = "File does not exist: " + srcFile.getFullPathName();
        return false;
    }
    if (cat == PresetCategory::EQCatalog &&
        srcFile.getSize() > 256 * 1024)
    {
        outError = "EQ preset import exceeds the 256 KiB size limit.";
        return false;
    }

    auto xml = juce::XmlDocument::parse(srcFile);
    if (cat == PresetCategory::EQCatalog)
    {
        if (!xml)
        {
            outError = "Could not parse EQ preset XML.";
            return false;
        }
        EQPresetDefinition preset;
        bool favorite = false;
        if (!deserializeEQCatalogPreset(*xml, preset, favorite, outError))
            return false;
        outName = sanitizePresetName(preset.name);
        if (outName.isEmpty())
        {
            outError = "Imported EQ preset name is invalid.";
            return false;
        }
        if (!saveEQCatalogPreset(outName, preset, preset.description,
                                 favorite, false, outError))
            return false;
        return true;
    }

    if (!xml || !xml->hasTagName("LIVEBUS_PRESET"))
    {
        outError = "Invalid preset format. Expected LIVEBUS_PRESET root tag.";
        return false;
    }

    auto presetCat = xml->getStringAttribute("category", "");
    if (presetCat != getCategoryDirectoryName(cat))
    {
        outError = "Preset category mismatch. File is '" + presetCat + "', expected '" + getCategoryDirectoryName(cat) + "'.";
        return false;
    }

    outName = xml->getStringAttribute("name", srcFile.getFileNameWithoutExtension());
    outName = outName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (outName.isEmpty()) outName = "Imported_Preset";

    auto destFile = getCategoryDirectory(cat).getChildFile(outName + ".xml");
    return srcFile.copyFileTo(destFile);
}

} // namespace LiveBus
