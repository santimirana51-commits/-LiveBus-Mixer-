#pragma once
#include <JuceHeader.h>
#include "PluginLibraryModel.h"
#include "PluginHostClient.h"
#include "RoutingMatrix.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace LiveBus
{

/**
 * High-performance, real-time safe VST3 Plugin Host and Processing Chain Engine.
 * Supports In-Process hosting and Out-of-Process isolation via LiveBusPluginHost.
 */
class VST3HostEngine final : public juce::ChangeBroadcaster
{
public:
    static constexpr int NumTargets = maxInputs + 2;

    static juce::String getTargetName(int targetIndex)
    {
        if (targetIndex >= 0 && targetIndex < maxInputs)
            return "Channel " + juce::String(targetIndex + 1);
        if (targetIndex == maxInputs)
            return "Stream Bus Master";
        if (targetIndex == maxInputs + 1)
            return "Monitor Bus Master";
        return "Unknown Target";
    }

    struct ActiveInstance
    {
        PluginChainSlot slotInfo;
        std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
        std::unique_ptr<PluginHostClient> isolatedClient;
        std::atomic<bool> isBypassed{false};
        std::atomic<int> latency{0};
    };

    VST3HostEngine(PluginHostClient& isolatedClient);
    ~VST3HostEngine();

    void prepareToPlay(double sampleRate, int maxBlockSize);
    void releaseResources();

    // =========================================================================
    // Library Management
    // =========================================================================
    void loadLibraryCache();
    void saveLibraryCache();
    void clearLibraryCache();

    std::vector<PluginInfoItem> getLibraryPlugins() const;
    void registerBundledPlugin(const juce::PluginDescription& description);
    void setFavorite(const juce::String& identifier, bool favorite);
    void setDefaultPlugin(const juce::String& identifier);
    juce::String getDefaultPluginIdentifier() const { return defaultPluginId; }

    PluginScanSettings getScanSettings() const;
    void setScanSettings(const PluginScanSettings& settings);

    // =========================================================================
    // Background Scanning
    // =========================================================================
    bool isScanning() const { return scanningActive.load(); }
    float getScanProgress() const { return scanProgress.load(); }
    juce::String getScanCurrentFile() const { return scanCurrentFile; }
    void startScan(bool fullRescan = false);
    void cancelScan();

    // =========================================================================
    // Plugin Chain Management
    // =========================================================================
    bool loadPluginToTarget(int targetIndex, const juce::PluginDescription& desc, bool isolated = false);
    bool removePluginFromTarget(int targetIndex, int slotIndex);
    bool movePluginInTarget(int targetIndex, int fromSlot, int toSlot);
    void setSlotBypassed(int targetIndex, int slotIndex, bool bypassed);
    bool isSlotBypassed(int targetIndex, int slotIndex) const;
    void clearTargetChain(int targetIndex);

    int getNumPluginsInTarget(int targetIndex) const;
    PluginChainSlot getSlotInfo(int targetIndex, int slotIndex) const;
    juce::AudioPluginInstance* getPluginInstance(int targetIndex, int slotIndex) const;
    int getTargetTotalLatency(int targetIndex) const;
    bool hasActivePluginNamed(int targetIndex, const juce::String& pluginName) const noexcept;
    juce::String getLastLoadError() const;
    double getCurrentSampleRate() const { return currentSampleRate.load(std::memory_order_relaxed); }
    PluginHostClient::PluginState getTargetIsolatedState(int targetIndex) const;
    bool simulateTargetIsolatedCrash(int targetIndex);
    void checkIsolatedClientsHealthAndRecover();

    // =========================================================================
    // Real-Time Audio Processing
    // Real-time safe: Zero allocations, lock-free bypass execution
    // =========================================================================
    void processTarget(int targetIndex, juce::AudioBuffer<float>& buffer, int numSamples);

    // =========================================================================
    // State Persistence (Scenes & Presets)
    // =========================================================================
    std::unique_ptr<juce::XmlElement> exportTargetChainXml(int targetIndex) const;
    bool importTargetChainXml(int targetIndex, const juce::XmlElement& xml);

    // Access to internal JUCE format manager
    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }
    const juce::KnownPluginList& getKnownPluginList() const { return knownPluginList; }

private:
    using ChainSnapshot = std::vector<std::shared_ptr<ActiveInstance>>;

    void populateReferenceLibraryIfEmpty();
    void updateLibraryFromKnownList();
    void publishTargetChainLocked(int targetIndex);
    void cleanupRetiredChains();
    void pauseAudioProcessing();
    void resumeAudioProcessing(bool wasPrepared);

    class ScannerThread : public juce::Thread
    {
    public:
        ScannerThread(VST3HostEngine& owner);
        void run() override;
        bool fullRescan{false};
    private:
        VST3HostEngine& hostEngine;
    };

    PluginHostClient& outOfProcessClient;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    mutable std::mutex libraryMutex;
    std::vector<PluginInfoItem> pluginLibrary;
    PluginScanSettings scanSettings;
    juce::String defaultPluginId;

    std::unique_ptr<ScannerThread> scannerThread;
    std::atomic<bool> scanningActive{false};
    std::atomic<float> scanProgress{0.0f};
    juce::String scanCurrentFile;

    // Target Chains (Targets 0..5)
    mutable std::mutex chainMutex;
    std::array<std::vector<std::shared_ptr<ActiveInstance>>, NumTargets> targetChains;
    std::array<std::atomic<const ChainSnapshot*>, NumTargets> publishedChains{};
    std::array<bool, NumTargets> chainPublishingSuspended{};
    std::vector<std::unique_ptr<ChainSnapshot>> retiredSnapshots;
    struct RetiredInstance
    {
        int targetIndex = -1;
        std::shared_ptr<ActiveInstance> instance;
    };
    std::vector<RetiredInstance> retiredInstances;
    std::atomic<unsigned int> audioReaders{0};

    // Preallocated temporary audio buffers for DSP chain rendering
    juce::AudioBuffer<float> tempChainBuffer;
    juce::MidiBuffer emptyMidiBuffer;

    mutable std::mutex loadErrorMutex;
    juce::String lastLoadError;
    std::atomic<double> currentSampleRate{48000.0};
    std::atomic<int> currentBlockSize{512};
    std::atomic<bool> isPrepared{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VST3HostEngine)
};

} // namespace LiveBus
