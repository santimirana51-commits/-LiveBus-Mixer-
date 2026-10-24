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
    void populateReferenceLibraryIfEmpty();
    void updateLibraryFromKnownList();

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

    // Preallocated temporary audio buffers for DSP chain rendering
    juce::AudioBuffer<float> tempChainBuffer;
    juce::MidiBuffer emptyMidiBuffer;

    double currentSampleRate{48000.0};
    int currentBlockSize{512};
    std::atomic<bool> isPrepared{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VST3HostEngine)
};

} // namespace LiveBus
