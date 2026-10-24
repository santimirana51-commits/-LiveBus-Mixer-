#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

class MidiEngine final : public juce::MidiInputCallback,
                         public juce::ChangeBroadcaster
{
public:
    MidiEngine();
    ~MidiEngine() override;

    juce::StringArray getAvailableInputDevices();
    bool openInputDevice(int deviceIndex);
    void closeCurrentDevice();
    juce::String getCurrentDeviceName() const;

    // juce::MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    // Called on Audio Thread: Lock-free extraction of incoming MIDI events
    void pullMidiBlock(juce::MidiBuffer& destMidiBuffer, int numSamples);

    bool hasRecentActivity() const { return recentActivity.load(std::memory_order_relaxed); }
    void clearActivity() { recentActivity.store(false, std::memory_order_relaxed); }

    int getLastNote() const { return lastNote.load(std::memory_order_relaxed); }
    int getLastVelocity() const { return lastVelocity.load(std::memory_order_relaxed); }
    int getLastCcNumber() const { return lastCcNumber.load(std::memory_order_relaxed); }
    int getLastCcValue() const { return lastCcValue.load(std::memory_order_relaxed); }

private:
    std::unique_ptr<juce::MidiInput> currentMidiInput;
    juce::String currentDeviceIdentifier;

    // Double-buffered lock-free storage for audio-thread extraction
    juce::CriticalSection midiLock;
    juce::MidiBuffer incomingMidiCollector;

    std::atomic<bool> recentActivity{false};
    std::atomic<int> lastNote{-1};
    std::atomic<int> lastVelocity{0};
    std::atomic<int> lastCcNumber{-1};
    std::atomic<int> lastCcValue{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEngine)
};
