#pragma once
#include <JuceHeader.h>
#include "SharedMemoryChannel.h"
#include <atomic>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class PluginHostClient
{
public:
    PluginHostClient();
    ~PluginHostClient();

    void prepare(double sampleRate, int blockSize);

    bool launchHelper();
    void terminateHelper();
    bool simulateCrash();

    bool isRunning() const;
    bool isCrashed() const { return crashed.load(); }
    void clearCrash() { crashed.store(false); }

    bool loadPlugin(const juce::String& path, double sampleRate, int blockSize);
    void releasePlugin();
    bool saveState(juce::MemoryBlock& outBlock);
    bool restoreState(const void* data, size_t size);
    void setBypass(bool bypass);
    bool getBypass() const { return bypassed.load(); }
    bool isPluginLoaded() const { return pluginLoaded.load(); }
    int getPluginLatencySamples() const noexcept
    {
        return pluginLatencySamples.load(std::memory_order_relaxed);
    }

    enum class PluginState
    {
        NoPluginLoaded,
        Loaded,
        ActiveProcessing,
        BypassedDry,
        HelperRestarting,
        HelperCrashed
    };

    PluginState getPluginState() const
    {
        if (crashed.load()) return PluginState::HelperCrashed;
        if (!isRunning()) return PluginState::HelperRestarting;
        if (!pluginLoaded.load()) return PluginState::NoPluginLoaded;
        if (bypassed.load()) return PluginState::BypassedDry;
        return PluginState::ActiveProcessing;
    }

    juce::String getPluginName() const;
    juce::String getLastError() const;

    // Real-time asynchronous non-blocking audio processing.
    // 0ms wait on the audio thread!
    // If helper response is not ready or helper crashes, dry-signal fallback is used immediately.
    void processBlock(juce::AudioBuffer<float>& buffer, int numSamples);

    void checkHealthAndRecover(double sampleRate, int blockSize);

private:
    LiveBusIPC::SharedMemoryChannel channel;
    std::string channelId;
    std::atomic<bool> crashed{false};
    std::atomic<bool> bypassed{false};
    std::atomic<bool> pluginLoaded{false};
    std::atomic<int> pluginLatencySamples{0};
    juce::String loadedPluginPath;
    juce::String currentPluginName;
    juce::String lastErrorMessage;
    juce::MemoryBlock lastKnownState;

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    std::uint32_t activeSequence = 0;
    LiveBusIPC::PendingAudioBlock pendingBlock;
    LiveBusIPC::FixedBlockAudioDelay dryDelay;
    juce::AudioBuffer<float> currentInputBuffer;

#if defined(_WIN32)
    PROCESS_INFORMATION processInfo{};
#endif
};
