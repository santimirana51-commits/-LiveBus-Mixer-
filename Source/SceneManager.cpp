#include "SceneManager.h"
#include <cmath>

SceneManager::SceneManager()
{
    getScenesDirectory().createDirectory();
}

juce::File SceneManager::getScenesDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("LiveBusMixer")
        .getChildFile("Scenes");
}

juce::StringArray SceneManager::getSceneList() const
{
    juce::StringArray list;
    auto files = getScenesDirectory().findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
    {
        // Ignore backup and temp files
        if (!f.getFileName().endsWithIgnoreCase(".bak.xml") && !f.getFileName().endsWithIgnoreCase(".tmp"))
            list.add(f.getFileNameWithoutExtension());
    }
    return list;
}

std::vector<SceneSummaryInfo> SceneManager::getSceneSummaries() const
{
    std::vector<SceneSummaryInfo> summaries;
    auto files = getScenesDirectory().findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
    {
        if (f.getFileName().endsWithIgnoreCase(".bak.xml") || f.getFileName().endsWithIgnoreCase(".tmp"))
            continue;

        SceneSummaryInfo info;
        info.name = f.getFileNameWithoutExtension();
        info.file = f;
        info.timestamp = f.getLastModificationTime().toMilliseconds();
        info.timestampFormatted = f.getLastModificationTime().formatted("%Y-%m-%d %H:%M");

        auto xml = juce::XmlDocument::parse(f);
        if (xml && xml->hasTagName("LIVEBUS_SCENE"))
        {
            auto desc = xml->getStringAttribute("description", "");
            if (desc.isNotEmpty())
                info.description = desc;
            else
                info.description = "LiveBus Mixer Snapshot";

            auto savedTime = xml->getStringAttribute("timestampFormatted", "");
            if (savedTime.isNotEmpty())
                info.timestampFormatted = savedTime;
        }
        else
        {
            info.description = "Custom Scene";
        }
        summaries.push_back(info);
    }
    return summaries;
}

bool SceneManager::createBackup(const juce::String& sceneName)
{
    auto safe = sceneName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto targetFile = getScenesDirectory().getChildFile(safe + ".xml");
    if (!targetFile.existsAsFile()) return false;

    auto backupFile = getScenesDirectory().getChildFile(safe + ".bak.xml");
    return targetFile.copyFileTo(backupFile);
}

bool SceneManager::restoreBackup(const juce::String& sceneName)
{
    auto safe = sceneName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto backupFile = getScenesDirectory().getChildFile(safe + ".bak.xml");
    if (!backupFile.existsAsFile()) return false;

    auto targetFile = getScenesDirectory().getChildFile(safe + ".xml");
    return backupFile.copyFileTo(targetFile);
}

bool SceneManager::saveScene(const juce::String& name, const SceneData& data)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safe.isEmpty()) return false;

    auto targetFile = getScenesDirectory().getChildFile(safe + ".xml");

    // Create automatic pre-write backup if the file already exists
    if (targetFile.existsAsFile())
    {
        createBackup(safe);
    }

    auto copyData = data;
    copyData.sceneName = safe;
    if (copyData.timestamp == 0)
    {
        auto now = juce::Time::getCurrentTime();
        copyData.timestamp = now.toMilliseconds();
        copyData.timestampFormatted = now.formatted("%Y-%m-%d %H:%M:%S");
    }

    auto xmlString = serializeToXmlString(copyData);
    auto tempFile = targetFile.getSiblingFile(safe + ".xml.tmp");

    if (tempFile.replaceWithText(xmlString))
    {
        return tempFile.replaceFileIn(targetFile);
    }
    return false;
}

bool SceneManager::loadScene(const juce::String& name, SceneData& outData, juce::String& outError)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    auto file = getScenesDirectory().getChildFile(safe + ".xml");
    if (!file.existsAsFile())
    {
        outError = "Scene file not found: " + safe;
        return false;
    }

    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
    {
        outError = "Corrupted XML file syntax. Attempting backup recovery.";
        // Check for backup
        auto backup = getScenesDirectory().getChildFile(safe + ".bak.xml");
        if (backup.existsAsFile())
        {
            auto backupXml = juce::XmlDocument::parse(backup);
            if (backupXml && deserializeFromXml(*backupXml, outData, outError))
            {
                outError = "Recovered from backup: " + safe;
                return true;
            }
        }
        // Fall back to clean defaults
        outData = SceneData();
        outData.sceneName = safe;
        return false;
    }

    return deserializeFromXml(*xml, outData, outError);
}

bool SceneManager::deleteScene(const juce::String& name)
{
    auto safe = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    auto file = getScenesDirectory().getChildFile(safe + ".xml");
    auto backup = getScenesDirectory().getChildFile(safe + ".bak.xml");

    bool success = false;
    if (file.existsAsFile())
        success = file.deleteFile();
    if (backup.existsAsFile())
        backup.deleteFile();

    return success;
}

bool SceneManager::renameScene(const juce::String& oldName, const juce::String& newName)
{
    auto safeOld = oldName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    auto safeNew = newName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (safeOld.isEmpty() || safeNew.isEmpty()) return false;

    auto oldFile = getScenesDirectory().getChildFile(safeOld + ".xml");
    auto newFile = getScenesDirectory().getChildFile(safeNew + ".xml");

    if (oldFile.existsAsFile() && !newFile.existsAsFile())
    {
        // Update scene name inside the file
        SceneData data;
        juce::String err;
        if (loadScene(safeOld, data, err))
        {
            data.sceneName = safeNew;
            if (saveScene(safeNew, data))
            {
                deleteScene(safeOld);
                return true;
            }
        }
        return oldFile.moveFileTo(newFile);
    }
    return false;
}

bool SceneManager::duplicateScene(const juce::String& sourceName, const juce::String& targetName)
{
    SceneData data;
    juce::String err;
    if (loadScene(sourceName, data, err))
    {
        data.sceneName = targetName;
        data.description = "Copy of " + sourceName;
        auto now = juce::Time::getCurrentTime();
        data.timestamp = now.toMilliseconds();
        data.timestampFormatted = now.formatted("%Y-%m-%d %H:%M:%S");
        return saveScene(targetName, data);
    }
    return false;
}

bool SceneManager::exportSceneToFile(const juce::String& sceneName, const juce::File& targetFile)
{
    SceneData data;
    juce::String err;
    if (!loadScene(sceneName, data, err))
        return false;

    auto xml = serializeToXmlString(data);
    return targetFile.replaceWithText(xml);
}

bool SceneManager::importSceneFromFile(const juce::File& sourceFile, juce::String& outImportedName, juce::String& outError)
{
    if (!sourceFile.existsAsFile())
    {
        outError = "File does not exist: " + sourceFile.getFullPathName();
        return false;
    }

    auto xml = juce::XmlDocument::parse(sourceFile);
    if (!xml)
    {
        outError = "Failed to parse XML from: " + sourceFile.getFileName();
        return false;
    }

    SceneData data;
    if (!deserializeFromXml(*xml, data, outError))
        return false;

    auto destName = data.sceneName.isNotEmpty() ? data.sceneName : sourceFile.getFileNameWithoutExtension();
    destName = destName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    if (destName.isEmpty()) destName = "Imported_Scene";

    if (!saveScene(destName, data))
    {
        outError = "Failed to write imported scene to scenes directory.";
        return false;
    }

    outImportedName = destName;
    return true;
}

juce::String SceneManager::serializeToXmlString(const SceneData& data)
{
    juce::XmlElement xml("LIVEBUS_SCENE");
    xml.setAttribute("version", data.version);
    xml.setAttribute("schemaVersion", data.schemaVersion);
    xml.setAttribute("name", data.sceneName);
    xml.setAttribute("description", data.description);
    xml.setAttribute("timestamp", juce::String(data.timestamp));
    xml.setAttribute("timestampFormatted", data.timestampFormatted);
    xml.setAttribute("sampleRate", data.preferredSampleRate);
    xml.setAttribute("blockSize", data.preferredBlockSize);
    xml.setAttribute("preferredDevice", data.preferredDeviceName);
    xml.setAttribute("numActiveChannels", data.numActiveChannels);
    xml.setAttribute("mixerFirstVisibleChannel", data.mixerFirstVisibleChannel);
    xml.setAttribute("mixerTopPanelProportion", (double)data.mixerTopPanelProportion);

    if (data.routingGraphLayoutXml.isNotEmpty())
    {
        auto* graphLayout = xml.createNewChildElement("ROUTING_GRAPH_LAYOUT");
        graphLayout->addTextElement(data.routingGraphLayoutXml);
    }

    // Channels
    auto* channelsElem = xml.createNewChildElement("CHANNELS");
    for (int i = 0; i < data.numActiveChannels; ++i)
    {
        const auto& ch = data.channels[(size_t)i];
        auto* c = channelsElem->createNewChildElement("CHANNEL");
        c->setAttribute("index", i);
        c->setAttribute("name", ch.name);
        if (ch.accentColour != 0)
            c->setAttribute("accentColour", juce::String((juce::int64)ch.accentColour));
        c->setAttribute("gain", (double)ch.gain);
        c->setAttribute("pan", (double)ch.pan);
        c->setAttribute("mute", ch.mute);
        c->setAttribute("solo", ch.solo);
        c->setAttribute("streamRoute", ch.streamRoute);
        c->setAttribute("monitorRoute", ch.monitorRoute);
        c->setAttribute("streamSend", (double)ch.streamSend);
        c->setAttribute("monitorSend", (double)ch.monitorSend);
    }

    // Buses
    auto* busesElem = xml.createNewChildElement("BUSES");
    for (int b = 0; b < numBuses; ++b)
    {
        const auto& bus = data.buses[(size_t)b];
        auto* be = busesElem->createNewChildElement("BUS");
        be->setAttribute("index", b);

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
    }

    // 8-Band Parametric Equalizers for all channel and bus targets
    auto* eqElem = xml.createNewChildElement("EIGHT_BAND_EQS");
    for (int t = 0; t < (int)data.eightBandEQs.size(); ++t)
    {
        auto* targetElem = eqElem->createNewChildElement("TARGET");
        targetElem->setAttribute("index", t);
        targetElem->setAttribute("masterEnabled", data.eightBandEQs[t].masterEnabled);
        for (int b = 0; b < 8; ++b)
        {
            auto* bElem = targetElem->createNewChildElement("BAND");
            bElem->setAttribute("index", b);
            bElem->setAttribute("type", data.eightBandEQs[t].bands[b].type);
            bElem->setAttribute("freq", (double)data.eightBandEQs[t].bands[b].freq);
            bElem->setAttribute("gainDb", (double)data.eightBandEQs[t].bands[b].gainDb);
            bElem->setAttribute("q", (double)data.eightBandEQs[t].bands[b].q);
            bElem->setAttribute("enabled", data.eightBandEQs[t].bands[b].enabled);
        }
    }

    auto* puzuElem = xml.createNewChildElement("PUZU_EQS");
    for (int t = 0; t < (int)data.puzuEQs.size(); ++t)
    {
        auto* targetElem = puzuElem->createNewChildElement("TARGET");
        targetElem->setAttribute("index", t);
        targetElem->setAttribute("masterEnabled", data.puzuEQs[(size_t)t].masterEnabled);
        for (int b = 0; b < (int)data.puzuEQs[(size_t)t].bands.size(); ++b)
        {
            const auto& band = data.puzuEQs[(size_t)t].bands[(size_t)b];
            auto* bandElem = targetElem->createNewChildElement("BAND");
            bandElem->setAttribute("index", b);
            bandElem->setAttribute("type", band.type);
            bandElem->setAttribute("freq", (double)band.freq);
            bandElem->setAttribute("gainDb", (double)band.gainDb);
            bandElem->setAttribute("q", (double)band.q);
            bandElem->setAttribute("enabled", band.enabled);
        }
    }

    auto* graphicEQElem = xml.createNewChildElement("MONITOR_GRAPHIC_EQ_31");
    graphicEQElem->setAttribute("enabled", data.monitorGraphicEQ31.enabled);
    for (int band = 0; band < numMonitorGraphicEQBands; ++band)
    {
        auto* bandElem = graphicEQElem->createNewChildElement("BAND");
        bandElem->setAttribute("index", band);
        bandElem->setAttribute("freq", (double)monitorGraphicEQFrequencies[(size_t)band]);
        bandElem->setAttribute("gainDb", (double)data.monitorGraphicEQ31.gainDb[(size_t)band]);
    }

    auto* mainOutputEQElem = xml.createNewChildElement("MAIN_OUTPUT_EQ_24");
    mainOutputEQElem->setAttribute("enabled", data.mainOutputEQ24.enabled);
    for (int band = 0; band < numMainOutputEQBands; ++band)
    {
        const auto& state = data.mainOutputEQ24.bands[(size_t)band];
        auto* bandElem = mainOutputEQElem->createNewChildElement("BAND");
        bandElem->setAttribute("index", band);
        bandElem->setAttribute("type", (int)state.type);
        bandElem->setAttribute("freq", (double)state.freq);
        bandElem->setAttribute("gainDb", (double)state.gainDb);
        bandElem->setAttribute("q", (double)state.q);
        bandElem->setAttribute("enabled", state.enabled);
        bandElem->setAttribute("dynamicEnabled", state.dynamicEnabled);
        bandElem->setAttribute("dynamicThresholdDb", (double)state.dynamicThresholdDb);
        bandElem->setAttribute("dynamicAttackMs", (double)state.dynamicAttackMs);
        bandElem->setAttribute("dynamicReleaseMs", (double)state.dynamicReleaseMs);
        bandElem->setAttribute("dynamicRangeDb", (double)state.dynamicRangeDb);
        bandElem->setAttribute("dynamicDetector", (int)state.dynamicDetector);
    }

    auto* abElem = xml.createNewChildElement("EQ_AB_STATES");
    auto* puzuABElem = abElem->createNewChildElement("PUZU");
    for (int target = 0; target < (int)data.puzuEQABSlots.size(); ++target)
    {
        auto* targetElem = puzuABElem->createNewChildElement("TARGET");
        targetElem->setAttribute("index", target);
        targetElem->setAttribute("activeSlot",
            juce::jlimit(0, 1, data.puzuEQActiveABSlots[(size_t)target]));
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto& eq = data.puzuEQABSlots[(size_t)target][(size_t)slot];
            auto* slotElem = targetElem->createNewChildElement("SLOT");
            slotElem->setAttribute("index", slot);
            slotElem->setAttribute("enabled", eq.masterEnabled);
            for (int band = 0; band < (int)eq.bands.size(); ++band)
            {
                const auto& bandState = eq.bands[(size_t)band];
                auto* bandElem = slotElem->createNewChildElement("BAND");
                bandElem->setAttribute("index", band);
                bandElem->setAttribute("type", bandState.type);
                bandElem->setAttribute("freq", (double)bandState.freq);
                bandElem->setAttribute("gainDb", (double)bandState.gainDb);
                bandElem->setAttribute("q", (double)bandState.q);
                bandElem->setAttribute("enabled", bandState.enabled);
            }
        }
    }

    auto* monitorABElem = abElem->createNewChildElement("MONITOR_GRAPHIC_EQ");
    monitorABElem->setAttribute("activeSlot",
        juce::jlimit(0, 1, data.monitorGraphicEQ31ActiveABSlot));
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto& eq = data.monitorGraphicEQ31ABSlots[(size_t)slot];
        auto* slotElem = monitorABElem->createNewChildElement("SLOT");
        slotElem->setAttribute("index", slot);
        slotElem->setAttribute("enabled", eq.enabled);
        for (int band = 0; band < numMonitorGraphicEQBands; ++band)
        {
            auto* bandElem = slotElem->createNewChildElement("BAND");
            bandElem->setAttribute("index", band);
            bandElem->setAttribute("gainDb",
                                   (double)eq.gainDb[(size_t)band]);
        }
    }

    auto* mainABElem = abElem->createNewChildElement("MAIN_OUTPUT_EQ");
    mainABElem->setAttribute("activeSlot",
        juce::jlimit(0, 1, data.mainOutputEQ24ActiveABSlot));
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto& eq = data.mainOutputEQ24ABSlots[(size_t)slot];
        auto* slotElem = mainABElem->createNewChildElement("SLOT");
        slotElem->setAttribute("index", slot);
        slotElem->setAttribute("enabled", eq.enabled);
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            const auto& state = eq.bands[(size_t)band];
            auto* bandElem = slotElem->createNewChildElement("BAND");
            bandElem->setAttribute("index", band);
            bandElem->setAttribute("type", (int)state.type);
            bandElem->setAttribute("freq", (double)state.freq);
            bandElem->setAttribute("gainDb", (double)state.gainDb);
            bandElem->setAttribute("q", (double)state.q);
            bandElem->setAttribute("enabled", state.enabled);
            bandElem->setAttribute("dynamicEnabled", state.dynamicEnabled);
            bandElem->setAttribute("dynamicThresholdDb",
                                   (double)state.dynamicThresholdDb);
            bandElem->setAttribute("dynamicAttackMs",
                                   (double)state.dynamicAttackMs);
            bandElem->setAttribute("dynamicReleaseMs",
                                   (double)state.dynamicReleaseMs);
            bandElem->setAttribute("dynamicRangeDb",
                                   (double)state.dynamicRangeDb);
            bandElem->setAttribute("dynamicDetector",
                                   (int)state.dynamicDetector);
        }
    }

    // Professional Vocal DSP Racks for all channel and bus targets
    auto* vrElem = xml.createNewChildElement("VOCAL_RACKS");
    for (int t = 0; t < (int)data.vocalRacks.size(); ++t)
    {
        const auto& vr = data.vocalRacks[t];
        auto* targetElem = vrElem->createNewChildElement("TARGET");
        targetElem->setAttribute("index", t);
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
    }

    // Optional Recall Filter
    auto* rfElem = xml.createNewChildElement("RECALL_FILTER");
    rfElem->setAttribute("fxUnits", data.savedFilter.fxUnits);
    rfElem->setAttribute("midi", data.savedFilter.midi);
    rfElem->setAttribute("otherDevice", data.savedFilter.otherDevice);
    rfElem->setAttribute("otherPlayback", data.savedFilter.otherPlayback);
    rfElem->setAttribute("ducking", data.savedFilter.ducking);
    rfElem->setAttribute("safety", data.savedFilter.safety);
    rfElem->setAttribute("macros", data.savedFilter.macros);
    rfElem->setAttribute("routingMatrix", data.savedFilter.routingMatrix);

    // Auto Ducking, Audio Safety, Macro Engine & Routing Matrix
    if (data.duckingXmlString.isNotEmpty())
    {
        if (auto dXml = juce::XmlDocument::parse(data.duckingXmlString))
            xml.addChildElement(dXml.release());
    }
    if (data.safetyXmlString.isNotEmpty())
    {
        if (auto sXml = juce::XmlDocument::parse(data.safetyXmlString))
            xml.addChildElement(sXml.release());
    }
    if (data.macroXmlString.isNotEmpty())
    {
        if (auto mXml = juce::XmlDocument::parse(data.macroXmlString))
            xml.addChildElement(mXml.release());
    }
    if (data.routingXmlString.isNotEmpty())
    {
        if (auto rXml = juce::XmlDocument::parse(data.routingXmlString))
            xml.addChildElement(rXml.release());
    }

    // VST3 Plugin chain
    auto* pluginsElem = xml.createNewChildElement("PLUGINS");
    for (const auto& p : data.plugins)
    {
        auto* pe = pluginsElem->createNewChildElement("PLUGIN");
        pe->setAttribute("path", p.pluginPath);
        pe->setAttribute("name", p.pluginName);
        pe->setAttribute("targetIndex", p.targetIndex);
        pe->setAttribute("bypassed", p.isBypassed);
        if (p.stateBlob.getSize() > 0)
        {
            pe->setAttribute("stateBase64", p.stateBlob.toBase64Encoding());
        }
    }

    return xml.toString();
}

bool SceneManager::deserializeFromXml(const juce::XmlElement& xml, SceneData& outData, juce::String& outError)
{
    if (!xml.hasTagName("LIVEBUS_SCENE"))
    {
        outError = "Invalid root tag. Expected LIVEBUS_SCENE.";
        return false;
    }

    outData = SceneData();
    outData.version = xml.getIntAttribute("version", 2);
    outData.schemaVersion = xml.getStringAttribute("schemaVersion", outData.version >= 3 ? "3.0" : "2.0");
    outData.sceneName = xml.getStringAttribute("name", "Untitled");
    outData.description = xml.getStringAttribute("description", "");
    outData.timestamp = (juce::int64)xml.getStringAttribute("timestamp", "0").getLargeIntValue();
    outData.timestampFormatted = xml.getStringAttribute("timestampFormatted", "");
    outData.preferredSampleRate = xml.getDoubleAttribute("sampleRate", 48000.0);
    outData.preferredBlockSize = xml.getIntAttribute("blockSize", 512);
    outData.preferredDeviceName = xml.getStringAttribute("preferredDevice", "");
    outData.numActiveChannels = juce::jlimit(1, maxInputs, xml.getIntAttribute("numActiveChannels", defaultNumInputs));
    outData.mixerFirstVisibleChannel = juce::jlimit(0, maxInputs - 1,
        xml.getIntAttribute("mixerFirstVisibleChannel", 0));
    outData.mixerTopPanelProportion = juce::jlimit(0.35f, 0.75f,
        (float)xml.getDoubleAttribute("mixerTopPanelProportion", 0.54));
    if (auto* graphLayout = xml.getChildByName("ROUTING_GRAPH_LAYOUT"))
        outData.routingGraphLayoutXml = graphLayout->getAllSubText();

    // Channels
    if (auto* channelsElem = xml.getChildByName("CHANNELS"))
    {
        for (auto* c : channelsElem->getChildIterator())
        {
            if (c->hasTagName("CHANNEL"))
            {
                int idx = c->getIntAttribute("index", -1);
                if (idx >= 0 && idx < numInputs)
                {
                    auto& ch = outData.channels[(size_t)idx];
                    ch.name = c->getStringAttribute("name", "Input " + juce::String(idx + 1));
                    ch.accentColour = (juce::uint32)c->getStringAttribute("accentColour", "0").getLargeIntValue();
                    ch.gain = juce::jlimit(0.0f, 2.0f, (float)c->getDoubleAttribute("gain", 0.707));
                    ch.pan = juce::jlimit(-1.0f, 1.0f, (float)c->getDoubleAttribute("pan", 0.0));
                    ch.mute = c->getBoolAttribute("mute", false);
                    ch.solo = c->getBoolAttribute("solo", false);
                    ch.streamRoute = c->getBoolAttribute("streamRoute", true);
                    ch.monitorRoute = c->getBoolAttribute("monitorRoute", true);
                    ch.streamSend = juce::jlimit(0.0f, 2.0f, (float)c->getDoubleAttribute("streamSend", 1.0));
                    ch.monitorSend = juce::jlimit(0.0f, 2.0f, (float)c->getDoubleAttribute("monitorSend", 1.0));
                }
            }
        }
    }

    // Buses
    if (auto* busesElem = xml.getChildByName("BUSES"))
    {
        for (auto* be : busesElem->getChildIterator())
        {
            if (be->hasTagName("BUS"))
            {
                int idx = be->getIntAttribute("index", -1);
                if (idx >= 0 && idx < numBuses)
                {
                    auto& bus = outData.buses[(size_t)idx];
                    bus.hpfEnabled = be->getBoolAttribute("hpfEnabled", true);
                    bus.hpfFreq = juce::jlimit(20.0f, 500.0f, (float)be->getDoubleAttribute("hpfFreq", 80.0));

                    bus.eqEnabled = be->getBoolAttribute("eqEnabled", true);
                    bus.eqLowFreq = juce::jlimit(20.0f, 500.0f, (float)be->getDoubleAttribute("eqLowFreq", 100.0));
                    bus.eqLowGainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqLowGainDb", 0.0));
                    bus.eqMid1Freq = juce::jlimit(100.0f, 5000.0f, (float)be->getDoubleAttribute("eqMid1Freq", 500.0));
                    bus.eqMid1GainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqMid1GainDb", 0.0));
                    bus.eqMid1Q = juce::jlimit(0.1f, 10.0f, (float)be->getDoubleAttribute("eqMid1Q", 0.707));
                    bus.eqFreq = juce::jlimit(200.0f, 10000.0f, (float)be->getDoubleAttribute("eqFreq", 2500.0));
                    bus.eqQ = juce::jlimit(0.1f, 10.0f, (float)be->getDoubleAttribute("eqQ", 0.707));
                    bus.eqGainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqGainDb", 0.0));
                    bus.eqHighFreq = juce::jlimit(1000.0f, 20000.0f, (float)be->getDoubleAttribute("eqHighFreq", 10000.0));
                    bus.eqHighGainDb = juce::jlimit(-24.0f, 24.0f, (float)be->getDoubleAttribute("eqHighGainDb", 0.0));

                    bus.gateEnabled = be->getBoolAttribute("gateEnabled", false);
                    bus.gateThresholdDb = juce::jlimit(-80.0f, 0.0f, (float)be->getDoubleAttribute("gateThresholdDb", -45.0));
                    bus.gateRatio = juce::jlimit(1.0f, 20.0f, (float)be->getDoubleAttribute("gateRatio", 4.0));
                    bus.gateAttackMs = juce::jlimit(0.1f, 100.0f, (float)be->getDoubleAttribute("gateAttackMs", 5.0));
                    bus.gateReleaseMs = juce::jlimit(5.0f, 1000.0f, (float)be->getDoubleAttribute("gateReleaseMs", 100.0));

                    bus.compEnabled = be->getBoolAttribute("compEnabled", true);
                    bus.compThresholdDb = juce::jlimit(-60.0f, 0.0f, (float)be->getDoubleAttribute("compThresholdDb", -18.0));
                    bus.compRatio = juce::jlimit(1.0f, 20.0f, (float)be->getDoubleAttribute("compRatio", 3.0));
                    bus.compAttackMs = juce::jlimit(0.1f, 200.0f, (float)be->getDoubleAttribute("compAttackMs", 10.0));
                    bus.compReleaseMs = juce::jlimit(5.0f, 2000.0f, (float)be->getDoubleAttribute("compReleaseMs", 100.0));

                    bus.limiterEnabled = be->getBoolAttribute("limiterEnabled", true);
                    bus.limiterCeilingDb = juce::jlimit(-24.0f, 0.0f, (float)be->getDoubleAttribute("limiterCeilingDb", -1.0));
                    bus.limiterReleaseMs = juce::jlimit(5.0f, 500.0f, (float)be->getDoubleAttribute("limiterReleaseMs", 80.0));

                    bus.reverbEnabled = be->getBoolAttribute("reverbEnabled", false);
                    bus.reverbRoomSize = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbRoomSize", 0.5));
                    bus.reverbDamping = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbDamping", 0.5));
                    bus.reverbWetLevel = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbWetLevel", 0.3));
                    bus.reverbDryLevel = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("reverbDryLevel", 1.0));

                    bus.delayEnabled = be->getBoolAttribute("delayEnabled", false);
                    bus.delayTimeMs = juce::jlimit(1.0f, 1000.0f, (float)be->getDoubleAttribute("delayTimeMs", 350.0));
                    bus.delayFeedback = juce::jlimit(0.0f, 0.95f, (float)be->getDoubleAttribute("delayFeedback", 0.35));
                    bus.delayWetLevel = juce::jlimit(0.0f, 1.0f, (float)be->getDoubleAttribute("delayWetLevel", 0.25));

                    bus.masterGain = juce::jlimit(0.0f, 2.0f, (float)be->getDoubleAttribute("masterGain", 1.0));
                    bus.masterPan = juce::jlimit(-1.0f, 1.0f, (float)be->getDoubleAttribute("masterPan", 0.0));
                    bus.masterMute = be->getBoolAttribute("masterMute", false);
                }
            }
        }
    }

    // 8-Band Parametric Equalizers for all channel and bus targets
    if (auto* eqElem = xml.getChildByName("EIGHT_BAND_EQS"))
    {
        for (auto* targetElem : eqElem->getChildIterator())
        {
            if (targetElem->hasTagName("TARGET"))
            {
                int t = targetElem->getIntAttribute("index", -1);
                if (t >= 0 && t < (int)outData.eightBandEQs.size())
                {
                    outData.eightBandEQs[t].masterEnabled = targetElem->getBoolAttribute("masterEnabled", true);
                    for (auto* bElem : targetElem->getChildIterator())
                    {
                        if (bElem->hasTagName("BAND"))
                        {
                            int b = bElem->getIntAttribute("index", -1);
                            if (b >= 0 && b < 8)
                            {
                                outData.eightBandEQs[t].bands[b].type = juce::jlimit(0, 4, bElem->getIntAttribute("type", 2));
                                outData.eightBandEQs[t].bands[b].freq = juce::jlimit(20.0f, 20000.0f, (float)bElem->getDoubleAttribute("freq", 1000.0));
                                outData.eightBandEQs[t].bands[b].gainDb = juce::jlimit(-24.0f, 24.0f, (float)bElem->getDoubleAttribute("gainDb", 0.0));
                                outData.eightBandEQs[t].bands[b].q = juce::jlimit(0.1f, 20.0f, (float)bElem->getDoubleAttribute("q", 0.707));
                                outData.eightBandEQs[t].bands[b].enabled = bElem->getBoolAttribute("enabled", true);
                            }
                        }
                    }
                }
            }
        }
    }

    if (auto* puzuElem = xml.getChildByName("PUZU_EQS"))
    {
        outData.hasPuzuEQData = true;
        for (auto* targetElem : puzuElem->getChildIterator())
        {
            if (!targetElem->hasTagName("TARGET"))
                continue;

            const int target = targetElem->getIntAttribute("index", -1);
            if (target < 0 || target >= (int)outData.puzuEQs.size())
                continue;

            auto& eq = outData.puzuEQs[(size_t)target];
            eq.masterEnabled = targetElem->getBoolAttribute("masterEnabled", false);
            for (auto* bandElem : targetElem->getChildIterator())
            {
                if (!bandElem->hasTagName("BAND"))
                    continue;

                const int bandIndex = bandElem->getIntAttribute("index", -1);
                if (bandIndex < 0 || bandIndex >= (int)eq.bands.size())
                    continue;

                auto& band = eq.bands[(size_t)bandIndex];
                band.type = juce::jlimit(0, 5, bandElem->getIntAttribute("type", 2));
                const float frequency = (float)bandElem->getDoubleAttribute(
                    "freq", defaultPuzuFreqs[bandIndex]);
                const float gainDb = (float)bandElem->getDoubleAttribute("gainDb", 0.0);
                const float q = (float)bandElem->getDoubleAttribute(
                    "q", defaultPuzuQs[bandIndex]);
                band.freq = std::isfinite(frequency)
                    ? juce::jlimit(20.0f, 20000.0f, frequency)
                    : defaultPuzuFreqs[bandIndex];
                band.gainDb = std::isfinite(gainDb)
                    ? juce::jlimit(-18.0f, 18.0f, gainDb) : 0.0f;
                band.q = std::isfinite(q)
                    ? juce::jlimit(0.1f, 10.0f, q) : defaultPuzuQs[bandIndex];
                band.enabled = bandElem->getBoolAttribute("enabled", true);
            }
        }
    }

    if (auto* graphicEQElem = xml.getChildByName("MONITOR_GRAPHIC_EQ_31"))
    {
        outData.hasMonitorGraphicEQ31Data = true;
        outData.monitorGraphicEQ31.enabled =
            graphicEQElem->getBoolAttribute("enabled", false);
        for (auto* bandElem : graphicEQElem->getChildIterator())
        {
            if (!bandElem->hasTagName("BAND"))
                continue;
            const int index = bandElem->getIntAttribute("index", -1);
            if (index < 0 || index >= numMonitorGraphicEQBands)
                continue;
            const float gainDb = (float)bandElem->getDoubleAttribute("gainDb", 0.0);
            outData.monitorGraphicEQ31.gainDb[(size_t)index] =
                std::isfinite(gainDb) ? juce::jlimit(-12.0f, 12.0f, gainDb) : 0.0f;
        }
    }

    if (auto* mainOutputEQElem = xml.getChildByName("MAIN_OUTPUT_EQ_24"))
    {
        outData.hasMainOutputEQ24Data = true;
        outData.mainOutputEQ24.enabled =
            mainOutputEQElem->getBoolAttribute("enabled", false);
        for (auto* bandElem : mainOutputEQElem->getChildIterator())
        {
            if (!bandElem->hasTagName("BAND"))
                continue;
            const int index = bandElem->getIntAttribute("index", -1);
            if (index < 0 || index >= numMainOutputEQBands)
                continue;
            auto& state = outData.mainOutputEQ24.bands[(size_t)index];
            state.type = (MainOutputEQFilterType)juce::jlimit(0, 5,
                bandElem->getIntAttribute("type", (int)MainOutputEQFilterType::Bell));
            const float frequency = (float)bandElem->getDoubleAttribute(
                "freq", mainOutputEQFrequencies[(size_t)index]);
            const float gainDb = (float)bandElem->getDoubleAttribute("gainDb", 0.0);
            const float q = (float)bandElem->getDoubleAttribute("q", 0.707);
            const float thresholdDb = (float)bandElem->getDoubleAttribute(
                "dynamicThresholdDb", -24.0);
            const float attackMs = (float)bandElem->getDoubleAttribute(
                "dynamicAttackMs", 10.0);
            const float releaseMs = (float)bandElem->getDoubleAttribute(
                "dynamicReleaseMs", 100.0);
            const float rangeDb = (float)bandElem->getDoubleAttribute(
                "dynamicRangeDb", 6.0);
            const int detector = bandElem->getIntAttribute("dynamicDetector", 0);
            state.freq = std::isfinite(frequency)
                ? juce::jlimit(20.0f, 20000.0f, frequency)
                : mainOutputEQFrequencies[(size_t)index];
            state.gainDb = std::isfinite(gainDb)
                ? juce::jlimit(-18.0f, 18.0f, gainDb) : 0.0f;
            state.q = std::isfinite(q) ? juce::jlimit(0.1f, 10.0f, q) : 0.707f;
            state.enabled = bandElem->getBoolAttribute("enabled", true);
            state.dynamicEnabled =
                bandElem->getBoolAttribute("dynamicEnabled", false);
            state.dynamicThresholdDb = std::isfinite(thresholdDb)
                ? juce::jlimit(-80.0f, 0.0f, thresholdDb) : -24.0f;
            state.dynamicAttackMs = std::isfinite(attackMs)
                ? juce::jlimit(0.1f, 100.0f, attackMs) : 10.0f;
            state.dynamicReleaseMs = std::isfinite(releaseMs)
                ? juce::jlimit(5.0f, 1000.0f, releaseMs) : 100.0f;
            state.dynamicRangeDb = std::isfinite(rangeDb)
                ? juce::jlimit(0.0f, 18.0f, rangeDb) : 6.0f;
            state.dynamicDetector = detector == (int)MainOutputEQDetector::RMS
                ? MainOutputEQDetector::RMS : MainOutputEQDetector::Peak;
        }
    }

    if (auto* abElem = xml.getChildByName("EQ_AB_STATES"))
    {
        if (auto* puzuElem = abElem->getChildByName("PUZU"))
        {
            outData.hasPuzuEQABData = true;
            for (auto* targetElem : puzuElem->getChildIterator())
            {
                if (!targetElem->hasTagName("TARGET"))
                    continue;
                const int target = targetElem->getIntAttribute("index", -1);
                if (target < 0 || target >= (int)outData.puzuEQABSlots.size())
                    continue;
                outData.puzuEQActiveABSlots[(size_t)target] =
                    juce::jlimit(0, 1, targetElem->getIntAttribute("activeSlot", 0));
                for (auto* slotElem : targetElem->getChildIterator())
                {
                    if (!slotElem->hasTagName("SLOT"))
                        continue;
                    const int slot = slotElem->getIntAttribute("index", -1);
                    if (slot < 0 || slot > 1)
                        continue;
                    auto& eq = outData.puzuEQABSlots[(size_t)target][(size_t)slot];
                    eq.masterEnabled = slotElem->getBoolAttribute("enabled", false);
                    for (auto* bandElem : slotElem->getChildIterator())
                    {
                        if (!bandElem->hasTagName("BAND"))
                            continue;
                        const int bandIndex = bandElem->getIntAttribute("index", -1);
                        if (bandIndex < 0 || bandIndex >= (int)eq.bands.size())
                            continue;
                        auto& band = eq.bands[(size_t)bandIndex];
                        band.type = juce::jlimit(0, 5,
                            bandElem->getIntAttribute("type", 2));
                        const float frequency = (float)bandElem->getDoubleAttribute(
                            "freq", defaultPuzuFreqs[bandIndex]);
                        const float gainDb =
                            (float)bandElem->getDoubleAttribute("gainDb", 0.0);
                        const float q = (float)bandElem->getDoubleAttribute(
                            "q", defaultPuzuQs[bandIndex]);
                        band.freq = std::isfinite(frequency)
                            ? juce::jlimit(20.0f, 20000.0f, frequency)
                            : defaultPuzuFreqs[bandIndex];
                        band.gainDb = std::isfinite(gainDb)
                            ? juce::jlimit(-18.0f, 18.0f, gainDb) : 0.0f;
                        band.q = std::isfinite(q)
                            ? juce::jlimit(0.1f, 10.0f, q)
                            : defaultPuzuQs[bandIndex];
                        band.enabled = bandElem->getBoolAttribute("enabled", true);
                    }
                }
            }
        }

        if (auto* monitorElem = abElem->getChildByName("MONITOR_GRAPHIC_EQ"))
        {
            outData.hasMonitorGraphicEQ31ABData = true;
            outData.monitorGraphicEQ31ActiveABSlot = juce::jlimit(
                0, 1, monitorElem->getIntAttribute("activeSlot", 0));
            for (auto* slotElem : monitorElem->getChildIterator())
            {
                if (!slotElem->hasTagName("SLOT"))
                    continue;
                const int slot = slotElem->getIntAttribute("index", -1);
                if (slot < 0 || slot > 1)
                    continue;
                auto& eq = outData.monitorGraphicEQ31ABSlots[(size_t)slot];
                eq.enabled = slotElem->getBoolAttribute("enabled", false);
                for (auto* bandElem : slotElem->getChildIterator())
                {
                    if (!bandElem->hasTagName("BAND"))
                        continue;
                    const int band = bandElem->getIntAttribute("index", -1);
                    if (band < 0 || band >= numMonitorGraphicEQBands)
                        continue;
                    const float gainDb =
                        (float)bandElem->getDoubleAttribute("gainDb", 0.0);
                    eq.gainDb[(size_t)band] = std::isfinite(gainDb)
                        ? juce::jlimit(-12.0f, 12.0f, gainDb) : 0.0f;
                }
            }
        }

        if (auto* mainElem = abElem->getChildByName("MAIN_OUTPUT_EQ"))
        {
            outData.hasMainOutputEQ24ABData = true;
            outData.mainOutputEQ24ActiveABSlot = juce::jlimit(
                0, 1, mainElem->getIntAttribute("activeSlot", 0));
            for (auto* slotElem : mainElem->getChildIterator())
            {
                if (!slotElem->hasTagName("SLOT"))
                    continue;
                const int slot = slotElem->getIntAttribute("index", -1);
                if (slot < 0 || slot > 1)
                    continue;
                auto& eq = outData.mainOutputEQ24ABSlots[(size_t)slot];
                eq.enabled = slotElem->getBoolAttribute("enabled", false);
                for (auto* bandElem : slotElem->getChildIterator())
                {
                    if (!bandElem->hasTagName("BAND"))
                        continue;
                    const int index = bandElem->getIntAttribute("index", -1);
                    if (index < 0 || index >= numMainOutputEQBands)
                        continue;
                    auto& state = eq.bands[(size_t)index];
                    state.type = (MainOutputEQFilterType)juce::jlimit(0, 5,
                        bandElem->getIntAttribute(
                            "type", (int)MainOutputEQFilterType::Bell));
                    const float frequency = (float)bandElem->getDoubleAttribute(
                        "freq", mainOutputEQFrequencies[(size_t)index]);
                    const float gainDb =
                        (float)bandElem->getDoubleAttribute("gainDb", 0.0);
                    const float q =
                        (float)bandElem->getDoubleAttribute("q", 0.707);
                    state.freq = std::isfinite(frequency)
                        ? juce::jlimit(20.0f, 20000.0f, frequency)
                        : mainOutputEQFrequencies[(size_t)index];
                    state.gainDb = std::isfinite(gainDb)
                        ? juce::jlimit(-18.0f, 18.0f, gainDb) : 0.0f;
                    state.q = std::isfinite(q)
                        ? juce::jlimit(0.1f, 10.0f, q) : 0.707f;
                    state.enabled = bandElem->getBoolAttribute("enabled", true);
                    state.dynamicEnabled =
                        bandElem->getBoolAttribute("dynamicEnabled", false);
                    const float thresholdDb = (float)bandElem->getDoubleAttribute(
                        "dynamicThresholdDb", -24.0);
                    const float attackMs = (float)bandElem->getDoubleAttribute(
                        "dynamicAttackMs", 10.0);
                    const float releaseMs = (float)bandElem->getDoubleAttribute(
                        "dynamicReleaseMs", 100.0);
                    const float rangeDb = (float)bandElem->getDoubleAttribute(
                        "dynamicRangeDb", 6.0);
                    state.dynamicThresholdDb = std::isfinite(thresholdDb)
                        ? juce::jlimit(-80.0f, 0.0f, thresholdDb) : -24.0f;
                    state.dynamicAttackMs = std::isfinite(attackMs)
                        ? juce::jlimit(0.1f, 100.0f, attackMs) : 10.0f;
                    state.dynamicReleaseMs = std::isfinite(releaseMs)
                        ? juce::jlimit(5.0f, 1000.0f, releaseMs) : 100.0f;
                    state.dynamicRangeDb = std::isfinite(rangeDb)
                        ? juce::jlimit(0.0f, 18.0f, rangeDb) : 6.0f;
                    state.dynamicDetector =
                        bandElem->getIntAttribute("dynamicDetector", 0) ==
                                (int)MainOutputEQDetector::RMS
                            ? MainOutputEQDetector::RMS : MainOutputEQDetector::Peak;
                }
            }
        }
    }

    // Vocal DSP Racks for all channel and bus targets
    if (auto* vrElem = xml.getChildByName("VOCAL_RACKS"))
    {
        for (auto* targetElem : vrElem->getChildIterator())
        {
            if (targetElem->hasTagName("TARGET"))
            {
                int t = targetElem->getIntAttribute("index", -1);
                if (t >= 0 && t < (int)outData.vocalRacks.size())
                {
                    auto& vr = outData.vocalRacks[t];
                    vr.masterEnabled = targetElem->getBoolAttribute("masterEnabled", true);
                    vr.hpfBypassed = targetElem->getBoolAttribute("hpfBypassed", false);
                    vr.gateBypassed = targetElem->getBoolAttribute("gateBypassed", false);
                    vr.eqBypassed = targetElem->getBoolAttribute("eqBypassed", false);
                    vr.deEsserBypassed = targetElem->getBoolAttribute("deEsserBypassed", false);
                    vr.compBypassed = targetElem->getBoolAttribute("compBypassed", false);
                    vr.limiterBypassed = targetElem->getBoolAttribute("limiterBypassed", false);

                    vr.inputGainDb = juce::jlimit(-24.0f, 24.0f, (float)targetElem->getDoubleAttribute("inputGainDb", 0.0));
                    vr.hpfFreq = juce::jlimit(20.0f, 500.0f, (float)targetElem->getDoubleAttribute("hpfFreq", 80.0));
                    vr.hpfSlope = juce::jlimit(0, 3, targetElem->getIntAttribute("hpfSlope", 1));

                    vr.gateMode = juce::jlimit(0, 1, targetElem->getIntAttribute("gateMode", 0));
                    vr.gateOpenThresholdDb = juce::jlimit(-80.0f, 0.0f, (float)targetElem->getDoubleAttribute("gateOpenThresholdDb", -42.0));
                    vr.gateCloseThresholdDb = juce::jlimit(-80.0f, 0.0f, (float)targetElem->getDoubleAttribute("gateCloseThresholdDb", -50.0));
                    vr.gateAttackMs = juce::jlimit(0.1f, 100.0f, (float)targetElem->getDoubleAttribute("gateAttackMs", 1.0));
                    vr.gateHoldMs = juce::jlimit(0.0f, 500.0f, (float)targetElem->getDoubleAttribute("gateHoldMs", 20.0));
                    vr.gateReleaseMs = juce::jlimit(5.0f, 1000.0f, (float)targetElem->getDoubleAttribute("gateReleaseMs", 100.0));
                    vr.gateFloorDb = juce::jlimit(-90.0f, 0.0f, (float)targetElem->getDoubleAttribute("gateFloorDb", -60.0));
                    vr.gateKeySource = juce::jlimit(0, 1, targetElem->getIntAttribute("gateKeySource", 0));
                    vr.gateKeyHpfFreq = juce::jlimit(20.0f, 1000.0f, (float)targetElem->getDoubleAttribute("gateKeyHpfFreq", 70.0));
                    vr.gateKeyLpfFreq = juce::jlimit(1000.0f, 20000.0f, (float)targetElem->getDoubleAttribute("gateKeyLpfFreq", 4000.0));

                    vr.deEsserType = juce::jlimit(0, 1, targetElem->getIntAttribute("deEsserType", 0));
                    vr.deEsserThresholdDb = juce::jlimit(-60.0f, 0.0f, (float)targetElem->getDoubleAttribute("deEsserThresholdDb", -16.0));
                    vr.deEsserFreq = juce::jlimit(1000.0f, 16000.0f, (float)targetElem->getDoubleAttribute("deEsserFreq", 5500.0));
                    vr.deEsserRangeDb = juce::jlimit(-24.0f, 0.0f, (float)targetElem->getDoubleAttribute("deEsserRangeDb", -8.0));

                    vr.compThresholdDb = juce::jlimit(-60.0f, 0.0f, (float)targetElem->getDoubleAttribute("compThresholdDb", -24.0));
                    vr.compRatio = juce::jlimit(1.0f, 20.0f, (float)targetElem->getDoubleAttribute("compRatio", 3.0));
                    vr.compAttackMs = juce::jlimit(0.1f, 200.0f, (float)targetElem->getDoubleAttribute("compAttackMs", 10.0));
                    vr.compReleaseMs = juce::jlimit(5.0f, 2000.0f, (float)targetElem->getDoubleAttribute("compReleaseMs", 120.0));
                    vr.compKnee = juce::jlimit(0, 2, targetElem->getIntAttribute("compKnee", 1));
                    vr.compReleaseMode = juce::jlimit(0, 1, targetElem->getIntAttribute("compReleaseMode", 0));
                    vr.compMakeupDb = juce::jlimit(0.0f, 24.0f, (float)targetElem->getDoubleAttribute("compMakeupDb", 3.0));
                    vr.compMixPercent = juce::jlimit(0.0f, 100.0f, (float)targetElem->getDoubleAttribute("compMixPercent", 100.0));
                    vr.compKeySource = juce::jlimit(0, 1, targetElem->getIntAttribute("compKeySource", 0));
                    vr.compKeyHpfFreq = juce::jlimit(20.0f, 1000.0f, (float)targetElem->getDoubleAttribute("compKeyHpfFreq", 60.0));
                    vr.compKeyLpfFreq = juce::jlimit(1000.0f, 20000.0f, (float)targetElem->getDoubleAttribute("compKeyLpfFreq", 15000.0));

                    vr.limiterCeilingDb = juce::jlimit(-24.0f, 0.0f, (float)targetElem->getDoubleAttribute("limiterCeilingDb", -1.0));
                    vr.limiterReleaseMs = juce::jlimit(5.0f, 500.0f, (float)targetElem->getDoubleAttribute("limiterReleaseMs", 50.0));
                    vr.outputGainDb = juce::jlimit(-24.0f, 24.0f, (float)targetElem->getDoubleAttribute("outputGainDb", 0.0));
                }
            }
        }
    }

    // Optional Recall Filter
    if (auto* rfElem = xml.getChildByName("RECALL_FILTER"))
    {
        outData.savedFilter.fxUnits = rfElem->getBoolAttribute("fxUnits", true);
        outData.savedFilter.midi = rfElem->getBoolAttribute("midi", true);
        outData.savedFilter.otherDevice = rfElem->getBoolAttribute("otherDevice", false);
        outData.savedFilter.otherPlayback = rfElem->getBoolAttribute("otherPlayback", true);
        outData.savedFilter.ducking = rfElem->getBoolAttribute("ducking", true);
        outData.savedFilter.safety = rfElem->getBoolAttribute("safety", true);
        outData.savedFilter.macros = rfElem->getBoolAttribute("macros", true);
        outData.savedFilter.routingMatrix = rfElem->getBoolAttribute("routingMatrix", true);
    }

    // Auto Ducking, Audio Safety, Macro Engine & Routing Matrix
    if (auto* dXml = xml.getChildByName("AutoDucker"))
        outData.duckingXmlString = dXml->toString();
    if (auto* sXml = xml.getChildByName("AudioSafety"))
        outData.safetyXmlString = sXml->toString();
    if (auto* mXml = xml.getChildByName("MacroEngine"))
        outData.macroXmlString = mXml->toString();
    if (auto* rXml = xml.getChildByName("RoutingMatrix"))
        outData.routingXmlString = rXml->toString();

    // Plugins
    if (auto* pluginsElem = xml.getChildByName("PLUGINS"))
    {
        for (auto* pe : pluginsElem->getChildIterator())
        {
            if (pe->hasTagName("PLUGIN"))
            {
                VST3SceneState p;
                p.pluginPath = pe->getStringAttribute("path", "");
                p.pluginName = pe->getStringAttribute("name", "Unknown");
                p.targetIndex = juce::jlimit(0, maxInputs + 1,
                    pe->getIntAttribute("targetIndex", maxInputs));
                p.isBypassed = pe->getBoolAttribute("bypassed", false);
                auto base64 = pe->getStringAttribute("stateBase64", "");
                if (base64.isNotEmpty())
                {
                    p.stateBlob.fromBase64Encoding(base64);
                }
                outData.plugins.push_back(p);
            }
        }
    }

    return true;
}
