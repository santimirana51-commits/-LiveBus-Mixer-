#pragma once
#include <JuceHeader.h>
#include "MidiMappingModel.h"
#include "MidiParameterRegistry.h"
#include <vector>
#include <memory>
#include <atomic>

namespace LiveBus
{

/**
 * Log entry for the live MIDI activity monitor.
 */
struct MidiActivityEvent
{
    juce::Time timestamp;
    juce::String deviceName;
    juce::String description;
    juce::Colour highlightColour;
};

/**
 * Professional MIDI Controller & MIDI Learn Manager.
 * Handles hardware MIDI inputs, event routing, parameter synchronization,
 * MIDI Learn state machine, and mapping preset persistence.
 */
class MidiManager : public juce::MidiInputCallback,
                    public juce::ChangeBroadcaster
{
public:
    MidiManager(MidiParameterRegistry& registry);
    ~MidiManager() override;

    // Device Management
    juce::StringArray getAvailableInputDevices();
    bool openInputDevice(const juce::String& deviceNameOrId);
    void closeCurrentDevice();
    juce::String getCurrentDeviceName() const;
    bool isDeviceOpen() const;

    // Optional Host / Physical toggles (referencing screenshot UI)
    void setLearnFromHost(bool b) { learnFromHost = b; }
    void setLearnFromPhysical(bool b) { learnFromPhysical = b; }
    bool isLearnFromHost() const { return learnFromHost; }
    bool isLearnFromPhysical() const { return learnFromPhysical; }

    void setSendFeedbackToHost(bool b) { feedbackToHost = b; }
    void setSendFeedbackToPhysical(bool b) { feedbackToPhysical = b; }
    bool isSendFeedbackToHost() const { return feedbackToHost; }
    bool isSendFeedbackToPhysical() const { return feedbackToPhysical; }

    // juce::MidiInputCallback (Real-time thread safe)
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    // Dispatches queued MIDI messages and updates parameters (Called on UI Timer or Audio Thread)
    void processQueuedEvents();

    // Pull events for audio thread / VST host
    void pullMidiBlock(juce::MidiBuffer& destMidiBuffer, int numSamples);

    // Mappings Management
    int getNumMappings() const;
    const MidiMappingEntry& getMapping(int index) const;
    MidiMappingEntry& getMapping(int index);
    void addMapping(const MidiMappingEntry& entry);
    void removeMapping(int index);
    void removeMappingById(const juce::String& id);
    void clearAllMappings();
    void setMappingEnabled(int index, bool enabled);

    // Duplicate Detection: returns list of parameter names already mapped to the same channel/controller
    juce::StringArray findDuplicateMappings(const MidiMappingEntry& entry) const;

    // MIDI Learn Workflow
    void startLearn(const juce::String& mappingId);
    void cancelLearn();
    bool isLearning() const;
    juce::String getLearningMappingId() const;
    int getLearnTimeoutRemainingMs() const;
    void tickLearnTimer(int elapsedMs);

    // Activity Stream
    bool hasRecentActivity() const { return recentActivity.load(std::memory_order_relaxed); }
    void clearActivity() { recentActivity.store(false, std::memory_order_relaxed); }
    std::vector<MidiActivityEvent> getRecentEvents() const;

    // Presets Management
    juce::String getCurrentPresetName() const { return currentPresetName; }
    juce::StringArray getAvailablePresets();
    bool isFactoryPreset(const juce::String& name) const;
    bool loadPreset(const juce::String& presetName);
    bool savePreset(const juce::String& presetName);
    bool deletePreset(const juce::String& presetName);
    bool exportPreset(const juce::File& destFile) const;
    bool importPreset(const juce::File& srcFile);
    void loadDefaultFactoryPreset();

    // Auto-Persistence
    void saveCurrentStateToXml(juce::XmlElement& xml) const;
    void restoreStateFromXml(const juce::XmlElement& xml);

    // Callbacks
    std::function<void(const MidiMappingEntry&)> onLearnCompleted;
    std::function<void(const juce::String& warningMsg)> onDuplicateWarning;
    std::function<void()> onMappingsChanged;

private:
    MidiParameterRegistry& parameterRegistry;

    std::unique_ptr<juce::MidiInput> currentMidiInput;
    juce::String currentDeviceName;
    juce::String currentDeviceIdentifier;

    bool learnFromHost = true;
    bool learnFromPhysical = true;
    bool feedbackToHost = false;
    bool feedbackToPhysical = false;

    // Active Mappings List
    std::vector<MidiMappingEntry> mappings;
    mutable juce::CriticalSection mappingLock;

    // Double-buffered lock-free message collector
    juce::CriticalSection midiQueueLock;
    juce::MidiBuffer incomingQueue;
    juce::MidiBuffer audioBufferCollector;

    std::atomic<bool> recentActivity{false};

    // MIDI Learn State
    std::atomic<bool> learningActive{false};
    juce::String learningMappingId;
    int learnTimeoutMs = 0;
    static constexpr int defaultLearnTimeoutMs = 15000; // 15 seconds

    // Activity log history (thread safe)
    mutable juce::CriticalSection activityLock;
    std::vector<MidiActivityEvent> activityHistory;
    static constexpr size_t maxActivityHistory = 60;

    juce::String currentPresetName{"Default Mixer Faders"};

    juce::File getPresetsDirectory() const;
    void populateFactoryPresets();
    void dispatchMidiMessage(const juce::String& sourceDev, const juce::MidiMessage& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManager)
};

} // namespace LiveBus
