#pragma once
#include <JuceHeader.h>
#include "RoutingMatrix.h"
#include "ChannelStripDSP.h"
#include "RecallFilter.h"
#include <vector>

struct VST3SceneState
{
    juce::String pluginPath;
    juce::String pluginName;
    int targetIndex = maxInputs;
    bool isBypassed = false;
    juce::MemoryBlock stateBlob;
};

struct SceneData
{
    int version = 3;
    juce::String schemaVersion = "3.0";
    juce::String sceneName;
    juce::String description;
    juce::int64 timestamp = 0;
    juce::String timestampFormatted;
    int numActiveChannels = defaultNumInputs;
    int mixerFirstVisibleChannel = 0;
    float mixerTopPanelProportion = 0.54f;

    // Optional Recall Filter saved with the scene
    LiveBus::RecallFilter savedFilter;

    // Channels
    struct ChannelSceneData
    {
        juce::String name;
        juce::uint32 accentColour = 0;
        float gain = 0.707f;
        float pan = 0.0f;
        bool mute = false;
        bool solo = false;
        bool streamRoute = true;
        bool monitorRoute = true;
        float streamSend = 1.0f;
        float monitorSend = 1.0f;
    };
    std::array<ChannelSceneData, numInputs> channels;

    // Bus DSP parameters for Stream (0) and Monitor (1)
    struct BusSceneDSP
    {
        bool hpfEnabled = true;
        float hpfFreq = 80.0f;

        bool eqEnabled = true;
        float eqLowFreq = 100.0f;
        float eqLowGainDb = 0.0f;
        float eqMid1Freq = 500.0f;
        float eqMid1GainDb = 0.0f;
        float eqMid1Q = 0.707f;
        float eqFreq = 2500.0f;
        float eqQ = 0.707f;
        float eqGainDb = 0.0f;
        float eqHighFreq = 10000.0f;
        float eqHighGainDb = 0.0f;

        bool gateEnabled = false;
        float gateThresholdDb = -45.0f;
        float gateRatio = 4.0f;
        float gateAttackMs = 5.0f;
        float gateReleaseMs = 100.0f;

        bool compEnabled = true;
        float compThresholdDb = -18.0f;
        float compRatio = 3.0f;
        float compAttackMs = 10.0f;
        float compReleaseMs = 100.0f;

        bool limiterEnabled = true;
        float limiterCeilingDb = -1.0f;
        float limiterReleaseMs = 80.0f;

        bool reverbEnabled = false;
        float reverbRoomSize = 0.5f;
        float reverbDamping = 0.5f;
        float reverbWetLevel = 0.3f;
        float reverbDryLevel = 1.0f;

        bool delayEnabled = false;
        float delayTimeMs = 350.0f;
        float delayFeedback = 0.35f;
        float delayWetLevel = 0.25f;

        float masterGain = 1.0f;
        float masterPan = 0.0f;
        bool masterMute = false;
    };
    std::array<BusSceneDSP, numBuses> buses;

    // Legacy scene indices 0-5: Stream, Monitor, Channels 1-4; 6-17: Channels 5-16.
    struct EightBandEQSceneData
    {
        bool masterEnabled = true;
        struct BandSceneData
        {
            int type = 2; // Bell
            float freq = 1000.0f;
            float gainDb = 0.0f;
            float q = 0.707f;
            bool enabled = true;
        };
        std::array<BandSceneData, 8> bands;

        EightBandEQSceneData()
        {
            masterEnabled = true;
            bands[0] = { 0, 80.0f, 0.0f, 0.71f, true };     // HPF
            bands[1] = { 1, 120.0f, 4.5f, 0.71f, true };    // Low Shelf
            bands[2] = { 2, 350.0f, -6.0f, 1.20f, true };   // Bell
            bands[3] = { 2, 1200.0f, 5.5f, 1.00f, true };   // Bell
            bands[4] = { 2, 2500.0f, -4.0f, 1.20f, true };  // Bell
            bands[5] = { 2, 5000.0f, 3.5f, 1.00f, true };   // Bell
            bands[6] = { 2, 10000.0f, 2.0f, 0.71f, true };  // Bell
            bands[7] = { 4, 16000.0f, 0.0f, 0.71f, true };  // LPF
        }
    };
    // Legacy indices 0-5 remain Stream, Monitor, and Channels 1-4; 6-17 are Channels 5-16.
    std::array<EightBandEQSceneData, maxInputs + 2> eightBandEQs;

    // Legacy indices 0-5 remain Stream, Monitor, and Channels 1-4; 6-17 are Channels 5-16.
    struct VocalDSPSceneData
    {
        bool masterEnabled = true;
        bool hpfBypassed = false;
        bool gateBypassed = false;
        bool eqBypassed = false;
        bool deEsserBypassed = false;
        bool compBypassed = false;
        bool limiterBypassed = false;

        // HPF / Level
        float inputGainDb = 0.0f;
        float hpfFreq = 80.0f;
        int hpfSlope = 1;

        // Gate / Expander
        int gateMode = 0;
        float gateOpenThresholdDb = -42.0f;
        float gateCloseThresholdDb = -50.0f;
        float gateAttackMs = 1.0f;
        float gateHoldMs = 20.0f;
        float gateReleaseMs = 100.0f;
        float gateFloorDb = -60.0f;
        int gateKeySource = 0;
        float gateKeyHpfFreq = 70.0f;
        float gateKeyLpfFreq = 4000.0f;

        // De-Esser
        int deEsserType = 0;
        float deEsserThresholdDb = -16.0f;
        float deEsserFreq = 5500.0f;
        float deEsserRangeDb = -8.0f;

        // Compressor
        float compThresholdDb = -24.0f;
        float compRatio = 3.0f;
        float compAttackMs = 10.0f;
        float compReleaseMs = 120.0f;
        int compKnee = 1;
        int compReleaseMode = 0;
        float compMakeupDb = 3.0f;
        float compMixPercent = 100.0f;
        int compKeySource = 0;
        float compKeyHpfFreq = 60.0f;
        float compKeyLpfFreq = 15000.0f;

        // Limiter & Output
        float limiterCeilingDb = -1.0f;
        float limiterReleaseMs = 50.0f;
        float outputGainDb = 0.0f;
    };
    std::array<VocalDSPSceneData, maxInputs + 2> vocalRacks;

    // VST3 Plugin chain
    std::vector<VST3SceneState> plugins;

    // Device preferences
    double preferredSampleRate = 48000.0;
    int preferredBlockSize = 512;
    juce::String preferredDeviceName;

    // XML strings for Auto Ducking, Audio Safety, Macro Engine, and Routing Matrix
    juce::String duckingXmlString;
    juce::String safetyXmlString;
    juce::String macroXmlString;
    juce::String routingXmlString;
    juce::String routingGraphLayoutXml;
};

struct SceneSummaryInfo
{
    juce::String name;
    juce::String description;
    juce::String timestampFormatted;
    juce::int64 timestamp = 0;
    juce::File file;
};

class SceneManager
{
public:
    SceneManager();

    juce::File getScenesDirectory() const;
    juce::StringArray getSceneList() const;
    std::vector<SceneSummaryInfo> getSceneSummaries() const;

    bool saveScene(const juce::String& name, const SceneData& data);
    bool loadScene(const juce::String& name, SceneData& outData, juce::String& outError);
    bool deleteScene(const juce::String& name);
    bool renameScene(const juce::String& oldName, const juce::String& newName);
    bool duplicateScene(const juce::String& sourceName, const juce::String& targetName);

    // Safe backup and atomic protection
    bool createBackup(const juce::String& sceneName);
    bool restoreBackup(const juce::String& sceneName);

    // Export & Import
    bool exportSceneToFile(const juce::String& sceneName, const juce::File& targetFile);
    bool importSceneFromFile(const juce::File& sourceFile, juce::String& outImportedName, juce::String& outError);

    juce::String serializeToXmlString(const SceneData& data);
    bool deserializeFromXml(const juce::XmlElement& xml, SceneData& outData, juce::String& outError);
};
