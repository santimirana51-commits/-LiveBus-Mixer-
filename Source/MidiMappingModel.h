#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace LiveBus
{

enum class MidiMessageType
{
    CC = 0,
    Note,
    PitchBend,
    ProgramChange
};

inline juce::String midiMessageTypeToString(MidiMessageType type)
{
    switch (type)
    {
        case MidiMessageType::CC: return "CC";
        case MidiMessageType::Note: return "Note";
        case MidiMessageType::PitchBend: return "PitchBend";
        case MidiMessageType::ProgramChange: return "ProgChange";
        default: return "CC";
    }
}

inline MidiMessageType stringToMidiMessageType(const juce::String& str)
{
    if (str.equalsIgnoreCase("Note")) return MidiMessageType::Note;
    if (str.equalsIgnoreCase("PitchBend")) return MidiMessageType::PitchBend;
    if (str.equalsIgnoreCase("ProgChange") || str.equalsIgnoreCase("ProgramChange")) return MidiMessageType::ProgramChange;
    return MidiMessageType::CC;
}

enum class MidiMode
{
    Absolute = 0,
    RelativeSigned,
    RelativeTwosComplement,
    RelativeBinaryOffset
};

inline juce::String midiModeToString(MidiMode mode)
{
    switch (mode)
    {
        case MidiMode::Absolute: return "Absolute";
        case MidiMode::RelativeSigned: return "Relative Signed";
        case MidiMode::RelativeTwosComplement: return "Relative 2's Compl";
        case MidiMode::RelativeBinaryOffset: return "Relative Bin Offset";
        default: return "Absolute";
    }
}

inline MidiMode stringToMidiMode(const juce::String& str)
{
    if (str.containsIgnoreCase("Signed")) return MidiMode::RelativeSigned;
    if (str.containsIgnoreCase("2's") || str.containsIgnoreCase("Twos")) return MidiMode::RelativeTwosComplement;
    if (str.containsIgnoreCase("Offset") || str.containsIgnoreCase("Binary")) return MidiMode::RelativeBinaryOffset;
    return MidiMode::Absolute;
}

enum class NoteTriggerMode
{
    Toggle = 0,
    Momentary
};

inline juce::String noteTriggerModeToString(NoteTriggerMode mode)
{
    return (mode == NoteTriggerMode::Toggle) ? "Toggle" : "Momentary";
}

inline NoteTriggerMode stringToNoteTriggerMode(const juce::String& str)
{
    return str.equalsIgnoreCase("Momentary") ? NoteTriggerMode::Momentary : NoteTriggerMode::Toggle;
}

/**
 * Single MIDI mapping definition linking a MIDI source message to an application parameter target.
 */
struct MidiMappingEntry
{
    juce::String id;
    juce::String deviceIdentifier{"All"};
    MidiMessageType messageType{MidiMessageType::CC};
    int midiChannel{0}; // 0 = All channels, 1..16 = specific channel
    int controllerOrNoteNumber{0}; // 0..127
    juce::String targetParameterId;
    juce::String targetScope{"Channel"};
    juce::String targetParamName;
    MidiMode mode{MidiMode::Absolute};
    NoteTriggerMode noteMode{NoteTriggerMode::Toggle};

    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    float skewFactor{1.0f}; // 1.0 = linear, < 1.0 or > 1.0 for logarithmic response

    bool invert{false};
    bool pickup{false}; // Soft takeover to avoid jumps
    bool enabled{true};

    // Runtime state tracking (non-serialized)
    mutable bool hasAcquiredPickup{false};
    mutable bool noteToggleState{false};

    MidiMappingEntry()
    {
        id = juce::Uuid().toString();
    }

    bool matches(const juce::String& sourceDevice, const juce::MidiMessage& msg) const
    {
        if (!enabled) return false;

        // Check device scope
        if (deviceIdentifier.isNotEmpty() && !deviceIdentifier.equalsIgnoreCase("All"))
        {
            if (sourceDevice.isNotEmpty() && !deviceIdentifier.equalsIgnoreCase(sourceDevice))
                return false;
        }

        // Check Channel (0 = All)
        if (midiChannel > 0 && msg.getChannel() != midiChannel)
            return false;

        // Check Message Type and Number
        if (messageType == MidiMessageType::CC)
        {
            return msg.isController() && (msg.getControllerNumber() == controllerOrNoteNumber);
        }
        else if (messageType == MidiMessageType::Note)
        {
            return (msg.isNoteOn() || msg.isNoteOff()) && (msg.getNoteNumber() == controllerOrNoteNumber);
        }
        else if (messageType == MidiMessageType::PitchBend)
        {
            return msg.isPitchWheel();
        }
        else if (messageType == MidiMessageType::ProgramChange)
        {
            return msg.isProgramChange();
        }

        return false;
    }

    /**
     * Calculates the new target parameter value from an incoming raw MIDI message.
     */
    float calculateNewValue(int rawValue, float currentParamVal) const
    {
        float targetVal = currentParamVal;

        if (mode == MidiMode::Absolute)
        {
            float norm = juce::jlimit(0.0f, 1.0f, (float)rawValue / 127.0f);
            if (invert) norm = 1.0f - norm;

            if (std::abs(skewFactor - 1.0f) > 0.001f && norm > 0.0f)
            {
                if (skewFactor > 0.0f)
                    norm = std::pow(norm, 1.0f / skewFactor);
            }

            float mapped = minValue + norm * (maxValue - minValue);

            if (pickup)
            {
                // Soft takeover: do not update until incoming value crosses or nears current value
                float currentNorm = (currentParamVal - minValue) / juce::jmax(0.0001f, (maxValue - minValue));
                if (!hasAcquiredPickup)
                {
                    if (std::abs(norm - currentNorm) <= 0.08f)
                        hasAcquiredPickup = true;
                    else
                        return currentParamVal; // Hold current
                }
            }

            targetVal = mapped;
        }
        else if (mode == MidiMode::RelativeSigned)
        {
            // Standard Relative Signed: 1..63 = +1..+63, 65..127 = -1..-63
            float delta = 0.0f;
            if (rawValue > 0 && rawValue < 64)
                delta = (float)rawValue;
            else if (rawValue > 64 && rawValue <= 127)
                delta = -((float)(rawValue - 64));

            if (invert) delta = -delta;
            float step = delta * (maxValue - minValue) * 0.015f; // ~1.5% of range per tick
            targetVal = currentParamVal + step;
        }
        else if (mode == MidiMode::RelativeTwosComplement)
        {
            // Two's complement 7-bit: 1..63 = positive (+1..+63), 64..127 = negative (-64..-1)
            float delta = 0.0f;
            if (rawValue >= 0 && rawValue < 64)
                delta = (float)rawValue;
            else if (rawValue >= 64 && rawValue <= 127)
                delta = (float)(rawValue - 128);

            if (invert) delta = -delta;
            float step = delta * (maxValue - minValue) * 0.015f;
            targetVal = currentParamVal + step;
        }
        else if (mode == MidiMode::RelativeBinaryOffset)
        {
            // Binary Offset: 64 = 0 center. >64 = positive, <64 = negative
            float delta = (float)(rawValue - 64);
            if (invert) delta = -delta;
            float step = delta * (maxValue - minValue) * 0.015f;
            targetVal = currentParamVal + step;
        }

        // Clamp to allowed range and guarantee safe float
        if (std::isnan(targetVal) || std::isinf(targetVal))
            targetVal = defaultValue;

        return juce::jlimit(minValue, maxValue, targetVal);
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("MidiMapping");
        xml->setAttribute("id", id);
        xml->setAttribute("device", deviceIdentifier);
        xml->setAttribute("type", midiMessageTypeToString(messageType));
        xml->setAttribute("channel", midiChannel);
        xml->setAttribute("controller", controllerOrNoteNumber);
        xml->setAttribute("targetId", targetParameterId);
        xml->setAttribute("targetScope", targetScope);
        xml->setAttribute("targetName", targetParamName);
        xml->setAttribute("mode", midiModeToString(mode));
        xml->setAttribute("noteMode", noteTriggerModeToString(noteMode));
        xml->setAttribute("min", (double)minValue);
        xml->setAttribute("max", (double)maxValue);
        xml->setAttribute("default", (double)defaultValue);
        xml->setAttribute("skew", (double)skewFactor);
        xml->setAttribute("invert", invert);
        xml->setAttribute("pickup", pickup);
        xml->setAttribute("enabled", enabled);
        return xml;
    }

    static MidiMappingEntry fromXml(const juce::XmlElement& xml)
    {
        MidiMappingEntry e;
        e.id = xml.getStringAttribute("id", juce::Uuid().toString());
        e.deviceIdentifier = xml.getStringAttribute("device", "All");
        e.messageType = stringToMidiMessageType(xml.getStringAttribute("type", "CC"));
        e.midiChannel = xml.getIntAttribute("channel", 0);
        e.controllerOrNoteNumber = xml.getIntAttribute("controller", 0);
        e.targetParameterId = xml.getStringAttribute("targetId", "");
        e.targetScope = xml.getStringAttribute("targetScope", "Channel");
        e.targetParamName = xml.getStringAttribute("targetName", "");
        e.mode = stringToMidiMode(xml.getStringAttribute("mode", "Absolute"));
        e.noteMode = stringToNoteTriggerMode(xml.getStringAttribute("noteMode", "Toggle"));
        e.minValue = (float)xml.getDoubleAttribute("min", 0.0);
        e.maxValue = (float)xml.getDoubleAttribute("max", 1.0);
        e.defaultValue = (float)xml.getDoubleAttribute("default", 0.0);
        e.skewFactor = (float)xml.getDoubleAttribute("skew", 1.0);
        e.invert = xml.getBoolAttribute("invert", false);
        e.pickup = xml.getBoolAttribute("pickup", false);
        e.enabled = xml.getBoolAttribute("enabled", true);
        return e;
    }
};

} // namespace LiveBus
