#include "MidiManager.h"

namespace LiveBus
{

MidiManager::MidiManager(MidiParameterRegistry& registry)
    : parameterRegistry(registry)
{
    populateFactoryPresets();
    loadDefaultFactoryPreset();
}

MidiManager::~MidiManager()
{
    closeCurrentDevice();
}

juce::StringArray MidiManager::getAvailableInputDevices()
{
    juce::StringArray names;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& d : devices)
    {
        names.add(d.name);
    }
    return names;
}

bool MidiManager::openInputDevice(const juce::String& deviceNameOrId)
{
    closeCurrentDevice();

    if (deviceNameOrId.isEmpty() || deviceNameOrId.equalsIgnoreCase("None"))
        return false;

    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& d : devices)
    {
        if (d.name.equalsIgnoreCase(deviceNameOrId) || d.identifier.equalsIgnoreCase(deviceNameOrId))
        {
            currentMidiInput = juce::MidiInput::openDevice(d.identifier, this);
            if (currentMidiInput != nullptr)
            {
                currentDeviceName = d.name;
                currentDeviceIdentifier = d.identifier;
                currentMidiInput->start();

                MidiActivityEvent ev;
                ev.timestamp = juce::Time::getCurrentTime();
                ev.deviceName = currentDeviceName;
                ev.description = "Device Connected & Listening: " + currentDeviceName;
                ev.highlightColour = juce::Colour(0xff10b981); // Green

                const juce::ScopedLock sl(activityLock);
                activityHistory.push_back(ev);

                sendChangeMessage();
                return true;
            }
        }
    }

    return false;
}

void MidiManager::closeCurrentDevice()
{
    if (currentMidiInput != nullptr)
    {
        currentMidiInput->stop();
        currentMidiInput.reset();

        MidiActivityEvent ev;
        ev.timestamp = juce::Time::getCurrentTime();
        ev.deviceName = currentDeviceName;
        ev.description = "Device Closed: " + currentDeviceName;
        ev.highlightColour = juce::Colour(0xfff59e0b); // Amber

        const juce::ScopedLock sl(activityLock);
        activityHistory.push_back(ev);

        currentDeviceName.clear();
        currentDeviceIdentifier.clear();
        sendChangeMessage();
    }
}

juce::String MidiManager::getCurrentDeviceName() const
{
    return currentDeviceName.isEmpty() ? "None" : currentDeviceName;
}

bool MidiManager::isDeviceOpen() const
{
    return currentMidiInput != nullptr;
}

void MidiManager::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    recentActivity.store(true, std::memory_order_relaxed);

    const juce::ScopedLock sl(midiQueueLock);
    incomingQueue.addEvent(message, 0);
    audioBufferCollector.addEvent(message, 0);
}

void MidiManager::processQueuedEvents()
{
    juce::MidiBuffer eventsToProcess;
    {
        const juce::ScopedLock sl(midiQueueLock);
        if (incomingQueue.isEmpty()) return;
        eventsToProcess.swapWith(incomingQueue);
    }

    for (const auto metadata : eventsToProcess)
    {
        dispatchMidiMessage(currentDeviceName, metadata.getMessage());
    }
}

void MidiManager::pullMidiBlock(juce::MidiBuffer& destMidiBuffer, int /*numSamples*/)
{
    if (midiQueueLock.tryEnter())
    {
        if (!audioBufferCollector.isEmpty())
        {
            destMidiBuffer.addEvents(audioBufferCollector, 0, -1, 0);
            audioBufferCollector.clear();
        }
        midiQueueLock.exit();
    }
}

void MidiManager::dispatchMidiMessage(const juce::String& sourceDev, const juce::MidiMessage& msg)
{
    // 1. Check if in MIDI Learn Mode
    if (learningActive.load(std::memory_order_acquire))
    {
        if (msg.isController() || (msg.isNoteOn() && msg.getVelocity() > 0))
        {
            const juce::ScopedLock sl(mappingLock);
            for (auto& m : mappings)
            {
                if (m.id == learningMappingId)
                {
                    m.deviceIdentifier = sourceDev.isEmpty() ? "All" : sourceDev;
                    m.midiChannel = msg.getChannel();

                    if (msg.isController())
                    {
                        m.messageType = MidiMessageType::CC;
                        m.controllerOrNoteNumber = msg.getControllerNumber();
                        m.mode = MidiMode::Absolute;
                    }
                    else
                    {
                        m.messageType = MidiMessageType::Note;
                        m.controllerOrNoteNumber = msg.getNoteNumber();
                        m.noteMode = NoteTriggerMode::Toggle;
                    }

                    learningActive.store(false, std::memory_order_release);
                    learningMappingId.clear();

                    MidiActivityEvent ev;
                    ev.timestamp = juce::Time::getCurrentTime();
                    ev.deviceName = sourceDev;
                    ev.description = "Learned: " + midiMessageTypeToString(m.messageType) + " #" +
                                     juce::String(m.controllerOrNoteNumber) + " (Ch " + juce::String(m.midiChannel) +
                                     ") -> " + m.targetParamName;
                    ev.highlightColour = juce::Colour(0xff00f5d4); // Cyan

                    {
                        const juce::ScopedLock al(activityLock);
                        activityHistory.push_back(ev);
                    }

                    // Check for duplicates
                    auto duplicates = findDuplicateMappings(m);
                    if (!duplicates.isEmpty() && onDuplicateWarning)
                    {
                        onDuplicateWarning("Warning: " + midiMessageTypeToString(m.messageType) + " #" +
                                           juce::String(m.controllerOrNoteNumber) + " is already mapped to: " +
                                           duplicates.joinIntoString(", "));
                    }

                    if (onLearnCompleted)
                        onLearnCompleted(m);

                    if (onMappingsChanged)
                        onMappingsChanged();

                    sendChangeMessage();
                    return;
                }
            }
        }
    }

    // 2. Normal Mapping Evaluation
    const juce::ScopedLock sl(mappingLock);
    for (auto& m : mappings)
    {
        if (m.matches(sourceDev, msg))
        {
            if (msg.isController())
            {
                float currentVal = parameterRegistry.getParameterValue(m.targetParameterId);
                float newVal = m.calculateNewValue(msg.getControllerValue(), currentVal);
                parameterRegistry.setParameterValue(m.targetParameterId, newVal);

                MidiActivityEvent ev;
                ev.timestamp = juce::Time::getCurrentTime();
                ev.deviceName = sourceDev;
                auto* desc = parameterRegistry.getDescriptor(m.targetParameterId);
                juce::String formatted = desc ? desc->getFormattedValue(newVal) : juce::String(newVal, 2);
                ev.description = "CC " + juce::String(msg.getControllerNumber()) + " = " +
                                 juce::String(msg.getControllerValue()) + " -> " + m.targetParamName + " [" + formatted + "]";
                ev.highlightColour = juce::Colour(0xff38bdf8); // Light Blue

                {
                    const juce::ScopedLock al(activityLock);
                    if (activityHistory.size() >= maxActivityHistory)
                        activityHistory.erase(activityHistory.begin());
                    activityHistory.push_back(ev);
                }
            }
            else if (msg.isNoteOn() || msg.isNoteOff())
            {
                if (m.noteMode == NoteTriggerMode::Toggle)
                {
                    if (msg.isNoteOn() && msg.getVelocity() > 0)
                    {
                        m.noteToggleState = !m.noteToggleState;
                        float val = m.noteToggleState ? 1.0f : 0.0f;
                        parameterRegistry.setParameterValue(m.targetParameterId, val);

                        MidiActivityEvent ev;
                        ev.timestamp = juce::Time::getCurrentTime();
                        ev.deviceName = sourceDev;
                        ev.description = "Note #" + juce::String(msg.getNoteNumber()) + " Toggle -> " +
                                         m.targetParamName + " (" + (m.noteToggleState ? "ON" : "OFF") + ")";
                        ev.highlightColour = m.noteToggleState ? juce::Colour(0xff22c55e) : juce::Colour(0xffef4444);

                        const juce::ScopedLock al(activityLock);
                        if (activityHistory.size() >= maxActivityHistory)
                            activityHistory.erase(activityHistory.begin());
                        activityHistory.push_back(ev);
                    }
                }
                else if (m.noteMode == NoteTriggerMode::Momentary)
                {
                    bool isPressed = msg.isNoteOn() && msg.getVelocity() > 0;
                    parameterRegistry.setParameterValue(m.targetParameterId, isPressed ? 1.0f : 0.0f);

                    MidiActivityEvent ev;
                    ev.timestamp = juce::Time::getCurrentTime();
                    ev.deviceName = sourceDev;
                    ev.description = "Note #" + juce::String(msg.getNoteNumber()) + " Momentary -> " +
                                     m.targetParamName + " (" + (isPressed ? "PRESSED" : "RELEASED") + ")";
                    ev.highlightColour = isPressed ? juce::Colour(0xff22c55e) : juce::Colour(0xff94a3b8);

                    const juce::ScopedLock al(activityLock);
                    if (activityHistory.size() >= maxActivityHistory)
                        activityHistory.erase(activityHistory.begin());
                    activityHistory.push_back(ev);
                }
            }
        }
    }
}

int MidiManager::getNumMappings() const
{
    const juce::ScopedLock sl(mappingLock);
    return (int)mappings.size();
}

const MidiMappingEntry& MidiManager::getMapping(int index) const
{
    const juce::ScopedLock sl(mappingLock);
    return mappings[(size_t)juce::jlimit(0, (int)mappings.size() - 1, index)];
}

MidiMappingEntry& MidiManager::getMapping(int index)
{
    const juce::ScopedLock sl(mappingLock);
    return mappings[(size_t)juce::jlimit(0, (int)mappings.size() - 1, index)];
}

void MidiManager::addMapping(const MidiMappingEntry& entry)
{
    {
        const juce::ScopedLock sl(mappingLock);
        mappings.push_back(entry);
    }
    if (onMappingsChanged) onMappingsChanged();
    sendChangeMessage();
}

void MidiManager::removeMapping(int index)
{
    {
        const juce::ScopedLock sl(mappingLock);
        if (juce::isPositiveAndBelow(index, (int)mappings.size()))
            mappings.erase(mappings.begin() + index);
    }
    if (onMappingsChanged) onMappingsChanged();
    sendChangeMessage();
}

void MidiManager::removeMappingById(const juce::String& id)
{
    {
        const juce::ScopedLock sl(mappingLock);
        for (auto it = mappings.begin(); it != mappings.end(); ++it)
        {
            if (it->id == id)
            {
                mappings.erase(it);
                break;
            }
        }
    }
    if (onMappingsChanged) onMappingsChanged();
    sendChangeMessage();
}

void MidiManager::clearAllMappings()
{
    {
        const juce::ScopedLock sl(mappingLock);
        mappings.clear();
    }
    if (onMappingsChanged) onMappingsChanged();
    sendChangeMessage();
}

void MidiManager::setMappingEnabled(int index, bool enabled)
{
    const juce::ScopedLock sl(mappingLock);
    if (juce::isPositiveAndBelow(index, (int)mappings.size()))
    {
        mappings[(size_t)index].enabled = enabled;
    }
}

juce::StringArray MidiManager::findDuplicateMappings(const MidiMappingEntry& entry) const
{
    juce::StringArray duplicates;
    const juce::ScopedLock sl(mappingLock);
    for (const auto& m : mappings)
    {
        if (m.id != entry.id && m.enabled)
        {
            if (m.messageType == entry.messageType &&
                m.controllerOrNoteNumber == entry.controllerOrNoteNumber)
            {
                if (m.midiChannel == 0 || entry.midiChannel == 0 || m.midiChannel == entry.midiChannel)
                {
                    duplicates.add(m.targetParamName.isNotEmpty() ? m.targetParamName : m.targetParameterId);
                }
            }
        }
    }
    return duplicates;
}

void MidiManager::startLearn(const juce::String& mappingId)
{
    learningMappingId = mappingId;
    learnTimeoutMs = defaultLearnTimeoutMs;
    learningActive.store(true, std::memory_order_release);
    sendChangeMessage();
}

void MidiManager::cancelLearn()
{
    learningActive.store(false, std::memory_order_release);
    learningMappingId.clear();
    learnTimeoutMs = 0;
    sendChangeMessage();
}

bool MidiManager::isLearning() const
{
    return learningActive.load(std::memory_order_acquire);
}

juce::String MidiManager::getLearningMappingId() const
{
    return learningMappingId;
}

int MidiManager::getLearnTimeoutRemainingMs() const
{
    return learnTimeoutMs;
}

void MidiManager::tickLearnTimer(int elapsedMs)
{
    if (learningActive.load(std::memory_order_acquire))
    {
        learnTimeoutMs -= elapsedMs;
        if (learnTimeoutMs <= 0)
        {
            cancelLearn();
        }
    }
}

std::vector<MidiActivityEvent> MidiManager::getRecentEvents() const
{
    const juce::ScopedLock al(activityLock);
    return activityHistory;
}

juce::File MidiManager::getPresetsDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("LiveBusMixer")
                   .getChildFile("MidiMappings");
    dir.createDirectory();
    return dir;
}

void MidiManager::populateFactoryPresets()
{
    auto dir = getPresetsDirectory();

    auto saveFactoryIfMissing = [this, &dir](const juce::String& name, const std::vector<MidiMappingEntry>& entries) {
        auto file = dir.getChildFile(name + ".xml");
        if (!file.existsAsFile())
        {
            juce::XmlElement root("LiveBusMidiMappingPreset");
            root.setAttribute("name", name);
            root.setAttribute("isFactory", true);
            root.setAttribute("version", "1.0");

            for (const auto& e : entries)
                root.addChildElement(e.toXml().release());

            file.replaceWithText(root.toString());
        }
    };

    // 1. Default Mixer Faders Factory Preset
    std::vector<MidiMappingEntry> defEntries;
    for (int i = 0; i < 4; ++i)
    {
        MidiMappingEntry gainMap;
        gainMap.controllerOrNoteNumber = i + 1; // CC 1..4
        gainMap.targetParameterId = "ch." + juce::String(i + 1) + ".gain";
        gainMap.targetScope = "Input " + juce::String(i + 1);
        gainMap.targetParamName = "Ch " + juce::String(i + 1) + " Gain";
        gainMap.minValue = -60.0f;
        gainMap.maxValue = 12.0f;
        gainMap.defaultValue = 0.0f;
        defEntries.push_back(gainMap);

        MidiMappingEntry panMap;
        panMap.controllerOrNoteNumber = i + 10; // CC 10..13
        panMap.targetParameterId = "ch." + juce::String(i + 1) + ".pan";
        panMap.targetScope = "Input " + juce::String(i + 1);
        panMap.targetParamName = "Ch " + juce::String(i + 1) + " Pan";
        panMap.minValue = -1.0f;
        panMap.maxValue = 1.0f;
        panMap.defaultValue = 0.0f;
        defEntries.push_back(panMap);

        MidiMappingEntry muteMap;
        muteMap.messageType = MidiMessageType::Note;
        muteMap.controllerOrNoteNumber = 36 + i; // Notes 36..39
        muteMap.targetParameterId = "ch." + juce::String(i + 1) + ".mute";
        muteMap.targetScope = "Input " + juce::String(i + 1);
        muteMap.targetParamName = "Ch " + juce::String(i + 1) + " Mute";
        muteMap.minValue = 0.0f;
        muteMap.maxValue = 1.0f;
        muteMap.noteMode = NoteTriggerMode::Toggle;
        defEntries.push_back(muteMap);
    }

    MidiMappingEntry streamMaster;
    streamMaster.controllerOrNoteNumber = 7; // Standard Master Vol CC 7
    streamMaster.targetParameterId = "bus.stream.gain";
    streamMaster.targetScope = "Stream Bus";
    streamMaster.targetParamName = "Stream Master Gain";
    streamMaster.minValue = -60.0f;
    streamMaster.maxValue = 12.0f;
    defEntries.push_back(streamMaster);

    MidiMappingEntry monitorMaster;
    monitorMaster.controllerOrNoteNumber = 8;
    monitorMaster.targetParameterId = "bus.monitor.gain";
    monitorMaster.targetScope = "Monitor Bus";
    monitorMaster.targetParamName = "Monitor Master Gain";
    monitorMaster.minValue = -60.0f;
    monitorMaster.maxValue = 12.0f;
    defEntries.push_back(monitorMaster);

    saveFactoryIfMissing("Default Mixer Faders", defEntries);

    // 2. Korg nanoKONTROL2 Preset
    std::vector<MidiMappingEntry> nanoEntries;
    for (int i = 0; i < 4; ++i)
    {
        MidiMappingEntry fader;
        fader.controllerOrNoteNumber = i; // CC 0..3
        fader.targetParameterId = "ch." + juce::String(i + 1) + ".gain";
        fader.targetScope = "Input " + juce::String(i + 1);
        fader.targetParamName = "Ch " + juce::String(i + 1) + " Gain";
        fader.minValue = -60.0f;
        fader.maxValue = 12.0f;
        nanoEntries.push_back(fader);

        MidiMappingEntry knob;
        knob.controllerOrNoteNumber = 16 + i; // CC 16..19
        knob.targetParameterId = "ch." + juce::String(i + 1) + ".pan";
        knob.targetScope = "Input " + juce::String(i + 1);
        knob.targetParamName = "Ch " + juce::String(i + 1) + " Pan";
        knob.minValue = -1.0f;
        knob.maxValue = 1.0f;
        nanoEntries.push_back(knob);

        MidiMappingEntry mute;
        mute.controllerOrNoteNumber = 48 + i; // CC 48..51
        mute.targetParameterId = "ch." + juce::String(i + 1) + ".mute";
        mute.targetScope = "Input " + juce::String(i + 1);
        mute.targetParamName = "Ch " + juce::String(i + 1) + " Mute";
        mute.minValue = 0.0f;
        mute.maxValue = 1.0f;
        nanoEntries.push_back(mute);
    }
    saveFactoryIfMissing("Korg nanoKONTROL2", nanoEntries);

    // 3. Relative Rotary Encoders Preset
    std::vector<MidiMappingEntry> relEntries;
    for (int i = 0; i < 4; ++i)
    {
        MidiMappingEntry enc;
        enc.controllerOrNoteNumber = 70 + i; // CC 70..73
        enc.mode = MidiMode::RelativeSigned;
        enc.targetParameterId = "ch." + juce::String(i + 1) + ".gain";
        enc.targetScope = "Input " + juce::String(i + 1);
        enc.targetParamName = "Ch " + juce::String(i + 1) + " Gain";
        enc.minValue = -60.0f;
        enc.maxValue = 12.0f;
        relEntries.push_back(enc);
    }
    saveFactoryIfMissing("Rotary Encoders Relative", relEntries);
}

void MidiManager::loadDefaultFactoryPreset()
{
    loadPreset("Default Mixer Faders");
}

juce::StringArray MidiManager::getAvailablePresets()
{
    juce::StringArray names;
    auto files = getPresetsDirectory().findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
    {
        names.add(f.getFileNameWithoutExtension());
    }
    return names;
}

bool MidiManager::isFactoryPreset(const juce::String& name) const
{
    return name.equalsIgnoreCase("Default Mixer Faders") ||
           name.equalsIgnoreCase("Korg nanoKONTROL2") ||
           name.equalsIgnoreCase("Rotary Encoders Relative");
}

bool MidiManager::loadPreset(const juce::String& presetName)
{
    auto file = getPresetsDirectory().getChildFile(presetName + ".xml");
    if (!file.existsAsFile()) return false;

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("LiveBusMidiMappingPreset")) return false;

    {
        const juce::ScopedLock sl(mappingLock);
        mappings.clear();

        for (auto* child : xml->getChildIterator())
        {
            if (child->hasTagName("MidiMapping"))
            {
                mappings.push_back(MidiMappingEntry::fromXml(*child));
            }
        }
    }

    currentPresetName = presetName;
    if (onMappingsChanged) onMappingsChanged();
    sendChangeMessage();
    return true;
}

bool MidiManager::savePreset(const juce::String& presetName)
{
    if (presetName.isEmpty()) return false;

    juce::XmlElement root("LiveBusMidiMappingPreset");
    root.setAttribute("name", presetName);
    root.setAttribute("isFactory", isFactoryPreset(presetName));
    root.setAttribute("version", "1.0");

    {
        const juce::ScopedLock sl(mappingLock);
        for (const auto& m : mappings)
            root.addChildElement(m.toXml().release());
    }

    auto file = getPresetsDirectory().getChildFile(presetName + ".xml");
    bool ok = file.replaceWithText(root.toString());
    if (ok) currentPresetName = presetName;
    sendChangeMessage();
    return ok;
}

bool MidiManager::deletePreset(const juce::String& presetName)
{
    if (isFactoryPreset(presetName)) return false; // Protected

    auto file = getPresetsDirectory().getChildFile(presetName + ".xml");
    if (file.existsAsFile())
    {
        return file.deleteFile();
    }
    return false;
}

bool MidiManager::exportPreset(const juce::File& destFile) const
{
    juce::XmlElement root("LiveBusMidiMappingPreset");
    root.setAttribute("name", currentPresetName);
    root.setAttribute("isFactory", false);
    root.setAttribute("version", "1.0");

    {
        const juce::ScopedLock sl(mappingLock);
        for (const auto& m : mappings)
            root.addChildElement(m.toXml().release());
    }

    return destFile.replaceWithText(root.toString());
}

bool MidiManager::importPreset(const juce::File& srcFile)
{
    if (!srcFile.existsAsFile()) return false;

    auto xml = juce::XmlDocument::parse(srcFile);
    if (xml == nullptr || !xml->hasTagName("LiveBusMidiMappingPreset")) return false;

    auto name = xml->getStringAttribute("name", srcFile.getFileNameWithoutExtension());
    auto destFile = getPresetsDirectory().getChildFile(name + ".xml");
    srcFile.copyFileTo(destFile);

    return loadPreset(name);
}

void MidiManager::saveCurrentStateToXml(juce::XmlElement& xml) const
{
    auto* midiXml = new juce::XmlElement("MidiConfiguration");
    midiXml->setAttribute("currentDevice", currentDeviceName);
    midiXml->setAttribute("currentPreset", currentPresetName);
    midiXml->setAttribute("learnFromHost", learnFromHost);
    midiXml->setAttribute("learnFromPhysical", learnFromPhysical);
    midiXml->setAttribute("feedbackToHost", feedbackToHost);
    midiXml->setAttribute("feedbackToPhysical", feedbackToPhysical);

    {
        const juce::ScopedLock sl(mappingLock);
        for (const auto& m : mappings)
            midiXml->addChildElement(m.toXml().release());
    }

    xml.addChildElement(midiXml);
}

void MidiManager::restoreStateFromXml(const juce::XmlElement& xml)
{
    auto* midiXml = xml.getChildByName("MidiConfiguration");
    if (midiXml == nullptr) return;

    auto savedDev = midiXml->getStringAttribute("currentDevice", "");
    if (savedDev.isNotEmpty() && savedDev != "None")
        openInputDevice(savedDev);

    currentPresetName = midiXml->getStringAttribute("currentPreset", "Default Mixer Faders");
    learnFromHost = midiXml->getBoolAttribute("learnFromHost", true);
    learnFromPhysical = midiXml->getBoolAttribute("learnFromPhysical", true);
    feedbackToHost = midiXml->getBoolAttribute("feedbackToHost", false);
    feedbackToPhysical = midiXml->getBoolAttribute("feedbackToPhysical", false);

    std::vector<MidiMappingEntry> loaded;
    for (auto* child : midiXml->getChildIterator())
    {
        if (child->hasTagName("MidiMapping"))
            loaded.push_back(MidiMappingEntry::fromXml(*child));
    }

    if (!loaded.empty())
    {
        const juce::ScopedLock sl(mappingLock);
        mappings = std::move(loaded);
    }

    if (onMappingsChanged) onMappingsChanged();
    sendChangeMessage();
}

} // namespace LiveBus
