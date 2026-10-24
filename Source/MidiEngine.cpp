#include "MidiEngine.h"

MidiEngine::MidiEngine() = default;

MidiEngine::~MidiEngine()
{
    closeCurrentDevice();
}

juce::StringArray MidiEngine::getAvailableInputDevices()
{
    juce::StringArray names;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& d : devices)
    {
        names.add(d.name);
    }
    return names;
}

bool MidiEngine::openInputDevice(int deviceIndex)
{
    closeCurrentDevice();

    auto devices = juce::MidiInput::getAvailableDevices();
    if (deviceIndex < 0 || deviceIndex >= devices.size())
        return false;

    auto deviceInfo = devices[deviceIndex];
    currentMidiInput = juce::MidiInput::openDevice(deviceInfo.identifier, this);
    if (currentMidiInput != nullptr)
    {
        currentDeviceIdentifier = deviceInfo.name;
        currentMidiInput->start();
        sendChangeMessage();
        return true;
    }

    return false;
}

void MidiEngine::closeCurrentDevice()
{
    if (currentMidiInput != nullptr)
    {
        currentMidiInput->stop();
        currentMidiInput.reset();
        currentDeviceIdentifier.clear();
        sendChangeMessage();
    }
}

juce::String MidiEngine::getCurrentDeviceName() const
{
    return currentDeviceIdentifier.isEmpty() ? "No MIDI Device" : currentDeviceIdentifier;
}

void MidiEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    recentActivity.store(true, std::memory_order_relaxed);

    if (message.isNoteOn())
    {
        lastNote.store(message.getNoteNumber(), std::memory_order_relaxed);
        lastVelocity.store(message.getVelocity(), std::memory_order_relaxed);
    }
    else if (message.isController())
    {
        lastCcNumber.store(message.getControllerNumber(), std::memory_order_relaxed);
        lastCcValue.store(message.getControllerValue(), std::memory_order_relaxed);
    }

    // Try-lock or quick lock into buffer with sample offset 0
    const juce::ScopedLock sl(midiLock);
    incomingMidiCollector.addEvent(message, 0);
}

void MidiEngine::pullMidiBlock(juce::MidiBuffer& destMidiBuffer, int /*numSamples*/)
{
    // Real-time audio thread: try-lock ensures zero audio callback blocking
    if (midiLock.tryEnter())
    {
        if (!incomingMidiCollector.isEmpty())
        {
            destMidiBuffer.addEvents(incomingMidiCollector, 0, -1, 0);
            incomingMidiCollector.clear();
        }
        midiLock.exit();
    }
}
