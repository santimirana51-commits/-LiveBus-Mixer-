#include "VST3HostEngine.h"
#include <algorithm>
#include <thread>

namespace LiveBus
{

VST3HostEngine::VST3HostEngine(PluginHostClient& isolatedClient)
    : outOfProcessClient(isolatedClient)
{
    for (int target = 0; target < NumTargets; ++target)
        publishTargetChainLocked(target);

    // Register VST3 format in JUCE format manager
    formatManager.addDefaultFormats();

    // Default VST3 paths on Windows
    scanSettings.customScanPaths.add("C:\\Program Files\\Common Files\\VST3");
    scanSettings.customScanPaths.add("C:\\Program Files (x86)\\Common Files\\VST3");

    loadLibraryCache();
    const auto bundledPluginsDirectory =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("Plugins");
    scanSettings.customScanPaths.addIfNotAlreadyThere(
        bundledPluginsDirectory.getFullPathName());
}

VST3HostEngine::~VST3HostEngine()
{
    cancelScan();
    releaseResources();
    std::lock_guard<std::mutex> lock(chainMutex);
    for (auto& published : publishedChains)
        delete published.exchange(nullptr, std::memory_order_seq_cst);
    retiredSnapshots.clear();
    for (auto& retired : retiredInstances)
    {
        if (retired.instance && retired.instance->isolatedClient)
            retired.instance->isolatedClient->releasePlugin();
    }
    retiredInstances.clear();
}

void VST3HostEngine::publishTargetChainLocked(int targetIndex)
{
    auto next = std::make_unique<ChainSnapshot>(targetChains[(size_t)targetIndex]);
    const auto* previous = publishedChains[(size_t)targetIndex].exchange(
        next.get(), std::memory_order_seq_cst);
    if (previous != nullptr)
        retiredSnapshots.emplace_back(const_cast<ChainSnapshot*>(previous));
    next.release();
}

void VST3HostEngine::cleanupRetiredChains()
{
    std::vector<std::shared_ptr<ActiveInstance>> readyToRelease;
    {
        std::lock_guard<std::mutex> lock(chainMutex);
        if (audioReaders.load(std::memory_order_seq_cst) != 0)
            return;

        retiredSnapshots.clear();
        for (auto it = retiredInstances.begin(); it != retiredInstances.end();)
        {
            const auto targetIndex = it->targetIndex;
            const auto* published = targetIndex >= 0 && targetIndex < NumTargets
                ? publishedChains[(size_t)targetIndex].load(std::memory_order_seq_cst)
                : nullptr;
            const bool stillActive = targetIndex >= 0 && targetIndex < NumTargets &&
                (chainPublishingSuspended[(size_t)targetIndex] ||
                 std::find(targetChains[(size_t)targetIndex].begin(),
                           targetChains[(size_t)targetIndex].end(), it->instance) !=
                     targetChains[(size_t)targetIndex].end() ||
                 (published != nullptr &&
                  std::find(published->begin(), published->end(), it->instance) !=
                      published->end()));
            if (stillActive)
            {
                ++it;
                continue;
            }
            readyToRelease.push_back(std::move(it->instance));
            it = retiredInstances.erase(it);
        }
    }

    for (auto& instance : readyToRelease)
    {
        if (instance && instance->pluginInstance)
            instance->pluginInstance->releaseResources();
        if (instance && instance->isolatedClient)
            instance->isolatedClient->releasePlugin();
    }
}

void VST3HostEngine::pauseAudioProcessing()
{
    isPrepared.store(false, std::memory_order_seq_cst);
    while (audioReaders.load(std::memory_order_seq_cst) != 0)
        std::this_thread::yield();
}

void VST3HostEngine::resumeAudioProcessing(bool wasPrepared)
{
    isPrepared.store(wasPrepared, std::memory_order_seq_cst);
}

void VST3HostEngine::prepareToPlay(double sampleRate, int maxBlockSize)
{
    pauseAudioProcessing();
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(maxBlockSize, std::memory_order_relaxed);

    tempChainBuffer.setSize(2, maxBlockSize, false, false, true);

    {
        std::lock_guard<std::mutex> lock(chainMutex);
        for (int t = 0; t < NumTargets; ++t)
        {
            for (auto& inst : targetChains[(size_t)t])
            {
                if (inst->pluginInstance)
                {
                    inst->pluginInstance->setPlayConfigDetails(2, 2, sampleRate, maxBlockSize);
                    inst->pluginInstance->prepareToPlay(sampleRate, maxBlockSize);
                    inst->latency.store(inst->pluginInstance->getLatencySamples(), std::memory_order_relaxed);
                    inst->slotInfo.latencySamples = inst->latency.load(std::memory_order_relaxed);
                }
                if (inst->isolatedClient)
                {
                    const auto pluginPath = inst->slotInfo.fileOrIdentifier;
                    juce::MemoryBlock savedState;
                    inst->isolatedClient->saveState(savedState);
                    inst->isolatedClient->prepare(sampleRate, maxBlockSize);
                    if (inst->isolatedClient->loadPlugin(pluginPath, sampleRate, maxBlockSize) && savedState.getSize() > 0)
                        inst->isolatedClient->restoreState(savedState.getData(), savedState.getSize());
                    if (inst->isolatedClient->isPluginLoaded())
                    {
                        const int totalLatency = maxBlockSize +
                            inst->isolatedClient->getPluginLatencySamples();
                        inst->latency.store(totalLatency, std::memory_order_relaxed);
                        inst->slotInfo.latencySamples = totalLatency;
                    }
                }
            }
        }
    }

    outOfProcessClient.prepare(sampleRate, maxBlockSize);
    resumeAudioProcessing(true);
    cleanupRetiredChains();
}

void VST3HostEngine::releaseResources()
{
    pauseAudioProcessing();
    std::lock_guard<std::mutex> lock(chainMutex);
    for (int t = 0; t < NumTargets; ++t)
    {
        for (auto& inst : targetChains[t])
        {
            if (inst->pluginInstance)
                inst->pluginInstance->releaseResources();
        }
    }
    for (auto& retired : retiredInstances)
    {
        if (retired.instance && retired.instance->pluginInstance)
            retired.instance->pluginInstance->releaseResources();
    }
}

// =============================================================================
// Library Management
// =============================================================================

void VST3HostEngine::loadLibraryCache()
{
    std::lock_guard<std::mutex> lock(libraryMutex);

    auto userDoc = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("LiveBusMixer");
    auto libFile = userDoc.getChildFile("plugin_library.xml");

    pluginLibrary.clear();

    if (libFile.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(libFile))
        {
            defaultPluginId = xml->getStringAttribute("defaultPluginId", "");

            if (auto* sXml = xml->getChildByName("ScanSettings"))
                scanSettings = PluginScanSettings::fromXml(*sXml);

            if (auto* pNode = xml->getChildByName("Plugins"))
            {
                for (auto* c = pNode->getFirstChildElement(); c != nullptr; c = c->getNextElement())
                {
                    if (c->hasTagName("PluginItem"))
                    {
                        auto loadedItem = PluginInfoItem::fromXml(*c);
                        const auto name = loadedItem.getDisplayName();
                        const auto id = loadedItem.description.fileOrIdentifier;

                        bool isDup = false;
                        for (auto& existing : pluginLibrary)
                        {
                            if ((id.isNotEmpty() && existing.description.fileOrIdentifier == id) ||
                                (name.isNotEmpty() && existing.getDisplayName() == name))
                            {
                                isDup = true;
                                break;
                            }
                        }

                        if (!isDup)
                            pluginLibrary.push_back(std::move(loadedItem));
                    }
                }
            }
        }
    }

    // Also load KnownPluginList cache if available
    auto cacheFile = userDoc.getChildFile("plugins_cache.xml");
    if (cacheFile.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(cacheFile))
            knownPluginList.recreateFromXml(*xml);
    }

    if (pluginLibrary.empty())
        populateReferenceLibraryIfEmpty();
}

void VST3HostEngine::saveLibraryCache()
{
    std::lock_guard<std::mutex> lock(libraryMutex);

    auto userDoc = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("LiveBusMixer");
    userDoc.createDirectory();

    auto libFile = userDoc.getChildFile("plugin_library.xml");
    juce::XmlElement xml("PluginLibrary");
    xml.setAttribute("defaultPluginId", defaultPluginId);
    xml.addChildElement(scanSettings.toXml().release());

    auto* pNode = xml.createNewChildElement("Plugins");
    for (const auto& item : pluginLibrary)
        pNode->addChildElement(item.toXml().release());

    xml.writeTo(libFile);

    // Save KnownPluginList cache
    auto cacheFile = userDoc.getChildFile("plugins_cache.xml");
    if (auto kXml = knownPluginList.createXml())
        kXml->writeTo(cacheFile);
}

void VST3HostEngine::clearLibraryCache()
{
    {
        std::lock_guard<std::mutex> lock(libraryMutex);
        pluginLibrary.clear();
        knownPluginList.clear();
        populateReferenceLibraryIfEmpty();
    }
    saveLibraryCache();
    sendChangeMessage();
}

std::vector<PluginInfoItem> VST3HostEngine::getLibraryPlugins() const
{
    std::lock_guard<std::mutex> lock(libraryMutex);
    return pluginLibrary;
}

void VST3HostEngine::registerBundledPlugin(const juce::PluginDescription& description)
{
    knownPluginList.addType(description);
    {
        std::lock_guard<std::mutex> lock(libraryMutex);
        const auto existing = std::find_if(pluginLibrary.begin(), pluginLibrary.end(),
            [&description](const PluginInfoItem& item)
            {
                return item.description.isDuplicateOf(description) ||
                       (description.name.isNotEmpty() && item.description.name == description.name) ||
                       (description.fileOrIdentifier.isNotEmpty() && item.description.fileOrIdentifier == description.fileOrIdentifier);
            });
        if (existing != pluginLibrary.end())
        {
            existing->description = description;
            existing->profile = "Bundled";
        }
        else
        {
            PluginInfoItem item;
            item.description = description;
            item.profile = "Bundled";
            item.isFavorite = true;
            pluginLibrary.push_back(std::move(item));
        }
    }
    saveLibraryCache();
    sendChangeMessage();
}

void VST3HostEngine::setFavorite(const juce::String& identifier, bool favorite)
{
    {
        std::lock_guard<std::mutex> lock(libraryMutex);
        for (auto& p : pluginLibrary)
        {
            if (p.description.fileOrIdentifier == identifier || p.description.name == identifier)
            {
                p.isFavorite = favorite;
                break;
            }
        }
    }
    saveLibraryCache();
    sendChangeMessage();
}

void VST3HostEngine::setDefaultPlugin(const juce::String& identifier)
{
    {
        std::lock_guard<std::mutex> lock(libraryMutex);
        defaultPluginId = identifier;
        for (auto& p : pluginLibrary)
        {
            p.isDefault = (p.description.fileOrIdentifier == identifier || p.description.name == identifier);
        }
    }
    saveLibraryCache();
    sendChangeMessage();
}

PluginScanSettings VST3HostEngine::getScanSettings() const
{
    std::lock_guard<std::mutex> lock(libraryMutex);
    return scanSettings;
}

void VST3HostEngine::setScanSettings(const PluginScanSettings& settings)
{
    {
        std::lock_guard<std::mutex> lock(libraryMutex);
        scanSettings = settings;
    }
    saveLibraryCache();
    sendChangeMessage();
}

// =============================================================================
// Background Scanning
// =============================================================================

void VST3HostEngine::startScan(bool fullRescan)
{
    if (scanningActive.load())
        return;

    cancelScan();

    scannerThread = std::make_unique<ScannerThread>(*this);
    scannerThread->fullRescan = fullRescan;
    scanningActive.store(true);
    scanProgress.store(0.0f);
    scannerThread->startThread();
}

void VST3HostEngine::cancelScan()
{
    if (scannerThread)
    {
        scannerThread->signalThreadShouldExit();
        scannerThread->stopThread(3000);
        scannerThread.reset();
    }
    scanningActive.store(false);
}

VST3HostEngine::ScannerThread::ScannerThread(VST3HostEngine& owner)
    : juce::Thread("VST3 Scanner Thread"), hostEngine(owner)
{
}

void VST3HostEngine::ScannerThread::run()
{
    juce::FileSearchPath searchPath;

    {
        std::lock_guard<std::mutex> lock(hostEngine.libraryMutex);
        if (hostEngine.scanSettings.scanStandardVst3)
        {
            searchPath.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
            searchPath.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));
        }
        for (const auto& p : hostEngine.scanSettings.customScanPaths)
            searchPath.add(juce::File(p));
    }

    if (fullRescan)
    {
        std::lock_guard<std::mutex> lock(hostEngine.libraryMutex);
        hostEngine.knownPluginList.clear();
    }

    // Enumerate .vst3 files on disk
    juce::Array<juce::File> vstFiles;
    for (int i = 0; i < searchPath.getNumPaths(); ++i)
    {
        auto dir = searchPath[i];
        if (dir.isDirectory())
            dir.findChildFiles(vstFiles, juce::File::findFilesAndDirectories, true, "*.vst3");
    }

    int totalFiles = vstFiles.size();
    for (int i = 0; i < totalFiles; ++i)
    {
        if (threadShouldExit())
            break;

        const auto& file = vstFiles[i];
        hostEngine.scanCurrentFile = file.getFileName();
        hostEngine.scanProgress.store((float)(i + 1) / (float)juce::jmax(1, totalFiles));

        // Use JUCE format manager to scan types for this file
        juce::OwnedArray<juce::PluginDescription> foundTypes;
        for (int f = 0; f < hostEngine.formatManager.getNumFormats(); ++f)
        {
            if (auto* format = hostEngine.formatManager.getFormat(f))
            {
                format->findAllTypesForFile(foundTypes, file.getFullPathName());
            }
        }

        for (auto* desc : foundTypes)
        {
            hostEngine.knownPluginList.addType(*desc);

            std::lock_guard<std::mutex> lock(hostEngine.libraryMutex);
            bool alreadyExists = false;
            for (auto& item : hostEngine.pluginLibrary)
            {
                if (item.description.isDuplicateOf(*desc))
                {
                    item.description = *desc;
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists)
            {
                PluginInfoItem item;
                item.description = *desc;
                item.isFavorite = false;
                item.isDefault = false;
                item.profile = "Custom";
                hostEngine.pluginLibrary.push_back(item);
            }
        }

        juce::Thread::sleep(5);
    }

    hostEngine.scanningActive.store(false);
    hostEngine.scanProgress.store(1.0f);
    hostEngine.scanCurrentFile = "Scan complete";

    hostEngine.saveLibraryCache();
    hostEngine.sendChangeMessage();
}

void VST3HostEngine::populateReferenceLibraryIfEmpty()
{
    // Populate rich reference VST3 catalog matching user screenshot
    struct RefEntry
    {
        const char* name;
        const char* category;
        const char* manufacturer;
        const char* profile;
        bool isDefault;
        bool isFavorite;
    };

    static const RefEntry catalog[] = {
        { "Harrison 32 Bus", "Fx|Dynamics|EQ", "Harrison Audio", "Factory", false, false },
        { "Harrison Bass Flow", "Fx|Dynamics|EQ", "Harrison Audio", "Factory", false, false },
        { "Harrison De-Esser", "Fx|Dynamics", "Harrison Audio", "Factory", false, false },
        { "Harrison Drum Flow", "Fx|Dynamics|EQ", "Harrison Audio", "Factory", false, false },
        { "Harrison Dyno Mite", "Fx|Dynamics", "Harrison Audio", "Factory", false, false },
        { "Harrison Legacy EQ", "Fx|EQ", "Harrison Audio", "Factory", false, false },
        { "Harrison Lookahead Compressor", "Fx|Dynamics", "Harrison Audio", "Factory", false, false },
        { "Harrison Mastering EQ", "Fx|EQ|Mastering", "Harrison Audio", "Factory", false, false },
        { "Harrison Micro Glide", "Fx|Modulation", "Harrison Audio", "Factory", false, false },
        { "Harrison MPC Channel Strip", "Fx|Dynamics|EQ|Restoration", "Harrison Audio", "Factory", false, false },
        { "Harrison MPC Compressor", "Fx|Dynamics", "Harrison Audio", "User", true, true },
        { "Harrison MPC Spectral Compressor", "Fx|Dynamics", "Harrison Audio", "Factory", false, false },
        { "Harrison Multi Band Compressor", "Fx|Dynamics", "Harrison Audio", "Factory", false, false },
        { "Harrison Tremolo Panner", "Fx|Modulation", "Harrison Audio", "Factory", false, false },
        { "Harrison Vocal Flow", "Fx|Dynamics|EQ", "Harrison Audio", "Factory", false, false },
        { "Harrison Vocal Intensity Processor", "Fx|Dynamics|Filter", "Harrison Audio", "Factory", false, false },
        { "Heatwave", "Fx|Distortion", "Slate Digital", "Factory", false, false },
        { "IDR360 5.0", "Fx|Surround", "Waves", "Factory", false, false },
        { "IDR360 5.1", "Fx|Surround", "Waves", "Factory", false, false },
        { "IMPusher Mono", "Fx|Mastering", "Waves", "Factory", false, false },
        { "IMPusher Stereo", "Fx|Mastering", "Waves", "Factory", false, false },
        { "Inf Bass", "Fx|Bass", "Slate Digital", "Factory", false, false },
        { "Inf EQ", "Fx|EQ", "Slate Digital", "Factory", false, false },
        { "LiveBus Studio Reverb", "Fx|Reverb", "LiveBus Audio", "Factory", false, true },
        { "LiveBus Tape Warmth", "Fx|Tape|Saturation", "LiveBus Audio", "Factory", false, false }
    };

    for (const auto& e : catalog)
    {
        PluginInfoItem item;
        item.description.name = e.name;
        item.description.fileOrIdentifier = "vst3://" + juce::String(e.manufacturer) + "/" + juce::String(e.name);
        item.description.pluginFormatName = "VST3";
        item.description.category = e.category;
        item.description.manufacturerName = e.manufacturer;
        item.description.version = "1.0.0";
        item.profile = e.profile;
        item.isDefault = e.isDefault;
        item.isFavorite = e.isFavorite;

        if (e.isDefault)
            defaultPluginId = item.description.fileOrIdentifier;

        pluginLibrary.push_back(item);
    }
}

// =============================================================================
// Plugin Chain Management
// =============================================================================

bool VST3HostEngine::loadPluginToTarget(int targetIndex, const juce::PluginDescription& desc, bool isolated)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    {
        std::lock_guard<std::mutex> errorLock(loadErrorMutex);
        lastLoadError.clear();
    }

    auto activeInst = std::make_shared<ActiveInstance>();
    activeInst->slotInfo.instanceId = juce::Uuid().toString();
    activeInst->slotInfo.pluginName = desc.name;
    activeInst->slotInfo.fileOrIdentifier = desc.fileOrIdentifier;
    activeInst->slotInfo.isIsolatedMode = isolated;
    activeInst->slotInfo.isBypassed = false;
    activeInst->isBypassed.store(false, std::memory_order_relaxed);

    juce::String errorMsg;

    // Check if the file actually exists as a real plugin binary on disk
    juce::File pluginFile(desc.fileOrIdentifier);
    if (pluginFile.exists())
    {
        if (isolated)
        {
            activeInst->isolatedClient = std::make_unique<PluginHostClient>();
            activeInst->isolatedClient->prepare(currentSampleRate.load(std::memory_order_relaxed),
                                                currentBlockSize.load(std::memory_order_relaxed));
            if (activeInst->isolatedClient->loadPlugin(pluginFile.getFullPathName(),
                                                       currentSampleRate.load(std::memory_order_relaxed),
                                                       currentBlockSize.load(std::memory_order_relaxed)))
            {
                const int pipelineLatency = currentBlockSize.load(std::memory_order_relaxed) +
                    activeInst->isolatedClient->getPluginLatencySamples();
                activeInst->latency.store(pipelineLatency, std::memory_order_relaxed);
                activeInst->slotInfo.latencySamples = pipelineLatency;
            }
            else
            {
                errorMsg = activeInst->isolatedClient->getLastError();
            }
        }
        else
        {
            // In-Process Hosting
            activeInst->pluginInstance = formatManager.createPluginInstance(desc,
                                                                             currentSampleRate.load(std::memory_order_relaxed),
                                                                             currentBlockSize.load(std::memory_order_relaxed),
                                                                             errorMsg);
            if (activeInst->pluginInstance)
            {
                activeInst->pluginInstance->setPlayConfigDetails(2, 2,
                                                                 currentSampleRate.load(std::memory_order_relaxed),
                                                                 currentBlockSize.load(std::memory_order_relaxed));
                activeInst->pluginInstance->prepareToPlay(currentSampleRate.load(std::memory_order_relaxed),
                                                         currentBlockSize.load(std::memory_order_relaxed));
                activeInst->latency.store(activeInst->pluginInstance->getLatencySamples(), std::memory_order_relaxed);
                activeInst->slotInfo.latencySamples = activeInst->latency.load();
            }
        }

        if ((isolated && !activeInst->isolatedClient->isPluginLoaded()) ||
            (!isolated && activeInst->pluginInstance == nullptr))
        {
            if (errorMsg.isEmpty())
                errorMsg = "The plug-in binary exists, but the plug-in could not be loaded.";
            std::lock_guard<std::mutex> errorLock(loadErrorMutex);
            lastLoadError = desc.name + ": " + errorMsg;
            return false;
        }
    }
    else
    {
        // Safe placeholder mode: File not yet installed or bundled reference plugin
        activeInst->slotInfo.latencySamples = 0;
    }

    bool publishChange = false;
    {
        std::lock_guard<std::mutex> lock(chainMutex);
        targetChains[targetIndex].push_back(activeInst);
        publishChange = !chainPublishingSuspended[(size_t)targetIndex];
        if (publishChange)
            publishTargetChainLocked(targetIndex);
    }

    if (publishChange)
    {
        cleanupRetiredChains();
        sendChangeMessage();
    }
    return true;
}

bool VST3HostEngine::removePluginFromTarget(int targetIndex, int slotIndex)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    {
        std::lock_guard<std::mutex> lock(chainMutex);
        if (slotIndex < 0 || slotIndex >= (int)targetChains[targetIndex].size())
            return false;

        auto removedInst = targetChains[targetIndex][(size_t)slotIndex];
        targetChains[targetIndex].erase(targetChains[targetIndex].begin() + slotIndex);
        retiredInstances.push_back({ targetIndex, std::move(removedInst) });
        if (!chainPublishingSuspended[(size_t)targetIndex])
            publishTargetChainLocked(targetIndex);
    }

    cleanupRetiredChains();
    sendChangeMessage();
    return true;
}

bool VST3HostEngine::movePluginInTarget(int targetIndex, int fromSlot, int toSlot)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    {
        std::lock_guard<std::mutex> lock(chainMutex);
        int count = (int)targetChains[targetIndex].size();
        if (fromSlot < 0 || fromSlot >= count || toSlot < 0 || toSlot >= count)
            return false;

        auto item = targetChains[targetIndex][(size_t)fromSlot];
        targetChains[targetIndex].erase(targetChains[targetIndex].begin() + fromSlot);
        targetChains[targetIndex].insert(targetChains[targetIndex].begin() + toSlot, item);
        if (!chainPublishingSuspended[(size_t)targetIndex])
            publishTargetChainLocked(targetIndex);
    }

    cleanupRetiredChains();
    sendChangeMessage();
    return true;
}

void VST3HostEngine::setSlotBypassed(int targetIndex, int slotIndex, bool bypassed)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return;

    std::lock_guard<std::mutex> lock(chainMutex);
    if (slotIndex >= 0 && slotIndex < (int)targetChains[targetIndex].size())
    {
        auto& inst = targetChains[targetIndex][slotIndex];
        inst->isBypassed.store(bypassed, std::memory_order_relaxed);
        inst->slotInfo.isBypassed = bypassed;
    }
}

bool VST3HostEngine::isSlotBypassed(int targetIndex, int slotIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    std::lock_guard<std::mutex> lock(chainMutex);
    if (slotIndex >= 0 && slotIndex < (int)targetChains[targetIndex].size())
        return targetChains[targetIndex][slotIndex]->isBypassed.load(std::memory_order_relaxed);

    return false;
}

void VST3HostEngine::clearTargetChain(int targetIndex)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return;

    bool publishChange = false;
    {
        std::lock_guard<std::mutex> lock(chainMutex);
        for (auto& instance : targetChains[(size_t)targetIndex])
            retiredInstances.push_back({ targetIndex, std::move(instance) });
        targetChains[targetIndex].clear();
        publishChange = !chainPublishingSuspended[(size_t)targetIndex];
        if (publishChange)
            publishTargetChainLocked(targetIndex);
    }

    if (publishChange)
    {
        cleanupRetiredChains();
        sendChangeMessage();
    }
}

int VST3HostEngine::getNumPluginsInTarget(int targetIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return 0;

    std::lock_guard<std::mutex> lock(chainMutex);
    return (int)targetChains[targetIndex].size();
}

PluginChainSlot VST3HostEngine::getSlotInfo(int targetIndex, int slotIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return {};

    std::lock_guard<std::mutex> lock(chainMutex);
    if (slotIndex >= 0 && slotIndex < (int)targetChains[targetIndex].size())
        return targetChains[targetIndex][slotIndex]->slotInfo;

    return {};
}

juce::AudioPluginInstance* VST3HostEngine::getPluginInstance(int targetIndex, int slotIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return nullptr;

    std::lock_guard<std::mutex> lock(chainMutex);
    if (slotIndex >= 0 && slotIndex < (int)targetChains[targetIndex].size())
        return targetChains[targetIndex][slotIndex]->pluginInstance.get();

    return nullptr;
}

int VST3HostEngine::getTargetTotalLatency(int targetIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return 0;

    int total = 0;
    std::lock_guard<std::mutex> lock(chainMutex);
    for (const auto& inst : targetChains[targetIndex])
    {
        if (!inst->isBypassed.load(std::memory_order_relaxed))
            total += inst->latency.load(std::memory_order_relaxed);
    }
    return total;
}

bool VST3HostEngine::hasActivePluginNamed(int targetIndex,
                                          const juce::String& pluginName) const noexcept
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    const auto* chain = publishedChains[(size_t)targetIndex].load(std::memory_order_seq_cst);
    if (chain == nullptr)
        return false;

    for (const auto& instance : *chain)
        if (instance != nullptr && instance->slotInfo.pluginName == pluginName &&
            !instance->isBypassed.load(std::memory_order_relaxed))
            return true;
    return false;
}

juce::String VST3HostEngine::getLastLoadError() const
{
    std::lock_guard<std::mutex> errorLock(loadErrorMutex);
    return lastLoadError;
}

PluginHostClient::PluginState VST3HostEngine::getTargetIsolatedState(int targetIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return PluginHostClient::PluginState::NoPluginLoaded;

    std::lock_guard<std::mutex> lock(chainMutex);
    for (const auto& inst : targetChains[targetIndex])
    {
        if (inst && inst->isolatedClient)
            return inst->isolatedClient->getPluginState();
    }
    return PluginHostClient::PluginState::NoPluginLoaded;
}

bool VST3HostEngine::simulateTargetIsolatedCrash(int targetIndex)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    const bool wasPrepared = isPrepared.load(std::memory_order_seq_cst);
    pauseAudioProcessing();
    bool simulated = false;
    {
        std::lock_guard<std::mutex> lock(chainMutex);
        for (const auto& inst : targetChains[(size_t)targetIndex])
        {
            if (inst && inst->isolatedClient)
            {
                simulated = inst->isolatedClient->simulateCrash();
                break;
            }
        }
    }
    resumeAudioProcessing(wasPrepared);
    return simulated;
}

void VST3HostEngine::checkIsolatedClientsHealthAndRecover()
{
    const bool wasPrepared = isPrepared.load(std::memory_order_seq_cst);
    pauseAudioProcessing();
    std::vector<std::shared_ptr<ActiveInstance>> instances;
    {
        std::lock_guard<std::mutex> lock(chainMutex);
        for (const auto& target : targetChains)
        {
            for (const auto& inst : target)
            {
                if (inst && inst->isolatedClient)
                    instances.push_back(inst);
            }
        }
    }

    const auto sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    const auto blockSize = currentBlockSize.load(std::memory_order_relaxed);
    for (const auto& inst : instances)
        inst->isolatedClient->checkHealthAndRecover(sampleRate, blockSize);
    resumeAudioProcessing(wasPrepared);
}

// =============================================================================
// Real-Time Audio Processing
// Zero heap allocations, real-time safe
// =============================================================================

void VST3HostEngine::processTarget(int targetIndex, juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return;

    audioReaders.fetch_add(1, std::memory_order_seq_cst);
    struct AudioReaderGuard
    {
        std::atomic<unsigned int>& readers;
        ~AudioReaderGuard() { readers.fetch_sub(1, std::memory_order_seq_cst); }
    } readerGuard{ audioReaders };
    if (!isPrepared.load(std::memory_order_seq_cst) || numSamples <= 0 ||
        numSamples > buffer.getNumSamples())
        return;

    const auto* chain = publishedChains[(size_t)targetIndex].load(std::memory_order_seq_cst);
    if (chain == nullptr)
        return;

    for (const auto& inst : *chain)
    {
        if (!inst || inst->isBypassed.load(std::memory_order_relaxed))
            continue;

        if (inst->slotInfo.isIsolatedMode)
        {
            if (inst->isolatedClient)
                inst->isolatedClient->processBlock(buffer, numSamples);
        }
        else if (inst->pluginInstance != nullptr)
        {
            // Process via In-Process JUCE AudioProcessor
            emptyMidiBuffer.clear();
            inst->pluginInstance->processBlock(buffer, emptyMidiBuffer);
        }
    }
}

// =============================================================================
// State Persistence (Scenes & Presets)
// =============================================================================

std::unique_ptr<juce::XmlElement> VST3HostEngine::exportTargetChainXml(int targetIndex) const
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return nullptr;

    auto* self = const_cast<VST3HostEngine*>(this);
    const bool wasPrepared = isPrepared.load(std::memory_order_seq_cst);
    self->pauseAudioProcessing();
    auto xml = std::make_unique<juce::XmlElement>("TargetChain");
    xml->setAttribute("targetIndex", targetIndex);
    xml->setAttribute("targetName", getTargetName(targetIndex));

    {
        std::lock_guard<std::mutex> lock(chainMutex);
        for (const auto& inst : targetChains[(size_t)targetIndex])
        {
            auto slot = inst->slotInfo;
            slot.isBypassed = inst->isBypassed.load(std::memory_order_relaxed);

            if (inst->pluginInstance != nullptr)
            {
                inst->pluginInstance->getStateInformation(slot.savedState);
            }
            else if (inst->isolatedClient)
            {
                inst->isolatedClient->saveState(slot.savedState);
            }
            xml->addChildElement(slot.toXml().release());
        }
    }

    self->resumeAudioProcessing(wasPrepared);
    return xml;
}

bool VST3HostEngine::importTargetChainXml(int targetIndex, const juce::XmlElement& xml)
{
    if (targetIndex < 0 || targetIndex >= NumTargets)
        return false;

    {
        std::lock_guard<std::mutex> lock(chainMutex);
        chainPublishingSuspended[(size_t)targetIndex] = true;
    }
    clearTargetChain(targetIndex);

    for (auto* c = xml.getFirstChildElement(); c != nullptr; c = c->getNextElement())
    {
        if (c->hasTagName("Slot"))
        {
            auto slot = PluginChainSlot::fromXml(*c);
            juce::PluginDescription desc;
            desc.name = slot.pluginName;
            desc.fileOrIdentifier = slot.fileOrIdentifier;
            desc.pluginFormatName = "VST3";

            if (!loadPluginToTarget(targetIndex, desc, slot.isIsolatedMode))
                continue;

            int slotIdx = getNumPluginsInTarget(targetIndex) - 1;
            setSlotBypassed(targetIndex, slotIdx, slot.isBypassed);

            if (auto* inst = getPluginInstance(targetIndex, slotIdx))
            {
                if (slot.savedState.getSize() > 0)
                    inst->setStateInformation(slot.savedState.getData(), (int)slot.savedState.getSize());
            }
            else if (slot.savedState.getSize() > 0)
            {
                std::lock_guard<std::mutex> lock(chainMutex);
                if (slotIdx >= 0 && slotIdx < (int)targetChains[targetIndex].size())
                {
                    auto& isolatedClient = targetChains[targetIndex][(size_t)slotIdx]->isolatedClient;
                    if (isolatedClient)
                        isolatedClient->restoreState(slot.savedState.getData(), slot.savedState.getSize());
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(chainMutex);
        chainPublishingSuspended[(size_t)targetIndex] = false;
        publishTargetChainLocked(targetIndex);
    }
    cleanupRetiredChains();
    sendChangeMessage();
    return true;
}

} // namespace LiveBus
