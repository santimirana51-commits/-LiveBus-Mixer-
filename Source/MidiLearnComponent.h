#pragma once
#include <JuceHeader.h>
#include "MidiManager.h"
#include "MidiParameterRegistry.h"

namespace LiveBus
{

/**
 * Single interactive row inside the MIDI Mappings Table.
 */
class MidiMappingRowComponent : public juce::Component
{
public:
    MidiMappingRowComponent(MidiManager& manager,
                            MidiParameterRegistry& registry,
                            int mappingIndex,
                            bool showAdvanced);
    ~MidiMappingRowComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateFromMapping();

    int getMappingIndex() const { return index; }
    void setMappingIndex(int newIndex) { index = newIndex; }
    void setShowAdvanced(bool show);

    std::function<void(int)> onRemoveClicked;
    std::function<void(int)> onLearnClicked;

private:
    MidiManager& midiManager;
    MidiParameterRegistry& paramRegistry;
    int index;
    bool advancedMode = false;

    juce::ComboBox typeCombo;
    juce::ComboBox channelCombo;
    juce::Label partScopeLabel;
    juce::Label controllerNumberLabel;
    juce::ComboBox modeCombo;
    juce::ComboBox parameterCombo;
    juce::ToggleButton enableToggle;
    juce::TextButton learnBtn{"Learn"};
    juce::TextButton removeBtn{"Remove"};

    // Advanced options controls
    juce::ToggleButton invertToggle{"Invert"};
    juce::ToggleButton pickupToggle{"Pickup"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMappingRowComponent)
};

/**
 * Professional MIDI Controller & MIDI Learn Console Component.
 * Faithfully designed after the reference screenshot with dark console aesthetic.
 */
class MidiLearnComponent : public juce::Component,
                           public juce::ChangeListener,
                           public juce::Timer
{
public:
    enum class SubTab
    {
        Skin = 0,
        GUI,
        MIDI,
        MIDILearn,
        DSPAudio,
        DSPBridge
    };

    MidiLearnComponent(MidiManager& manager, MidiParameterRegistry& registry);
    ~MidiLearnComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void refreshMappingTable();
    void selectSubTab(SubTab tab);

    std::function<void()> onCloseRequested;

private:
    MidiManager& midiManager;
    MidiParameterRegistry& parameterRegistry;

    SubTab currentSubTab{SubTab::MIDILearn};
    bool advancedOptionsEnabled = false;

    // Top Header & Preset Toolbar
    juce::Label titleLabel{"title", "MIDI Learn"};
    juce::TextButton closeBtn{"X"};

    juce::Label presetLabel{"presetLbl", "Preset:"};
    juce::ComboBox presetCombo;
    juce::TextButton createPresetBtn{"Create"};
    juce::TextButton savePresetBtn{"Save"};
    juce::TextButton deletePresetBtn{"Delete"};
    juce::TextButton browsePresetBtn{"Browse..."};
    juce::TextButton exportPresetBtn{"Export..."};

    // Left Navigation Sidebar
    juce::Label settingsNavLabel{"settings", "Settings"};
    std::array<juce::TextButton, 6> subTabButtons;
    juce::ToggleButton enableAdvancedToggle{"Enable Advanced Options"};

    // Center Area: Mappings SubTab
    juce::Label mappingsHeader{"mappings", "Mappings"};
    juce::Label colType{"cType", "Type"};
    juce::Label colChannel{"cChan", "Channel"};
    juce::Label colPart{"cPart", "Part"};
    juce::Label colController{"cCtrl", "Controller"};
    juce::Label colMode{"cMode", "Mode"};
    juce::Label colParameter{"cParam", "Parameter"};
    juce::Label colActions{"cAct", "Actions"};

    juce::Viewport tableViewport;
    std::unique_ptr<juce::Component> tableContent;
    std::vector<std::unique_ptr<MidiMappingRowComponent>> rowComponents;

    juce::TextButton addMappingBtn{"+ Add Mapping"};
    juce::TextButton clearAllBtn{"Clear All"};

    // Bottom Sections (from screenshot)
    juce::Label midiInputHeader{"midiIn", "MIDI Input"};
    juce::Label learnFromLabel{"learnFrom", "Learn from:"};
    juce::ToggleButton learnHostToggle{"Host"};
    juce::ToggleButton learnPhysicalToggle{"Physical"};

    juce::Label midiFeedbackHeader{"midiFb", "MIDI Feedback"};
    juce::Label sendFeedbackLabel{"sendFb", "Send feedback to:"};
    juce::ToggleButton feedbackHostToggle{"Host"};
    juce::ToggleButton feedbackPhysicalToggle{"Physical"};

    // SubTab 2: MIDI Hardware Devices & Monitor Activity
    juce::Label midiDevicesHeader{"devHdr", "Connected MIDI Input Devices"};
    juce::ComboBox midiDeviceSelector;
    juce::TextButton refreshDevicesBtn{"Refresh Devices"};
    juce::Label midiActivityHeader{"actHdr", "Live MIDI Activity Log"};
    juce::TextEditor midiActivityLog;
    juce::TextButton clearActivityBtn{"Clear Log"};

    // SubTab 3: DSP & Audio Quick Mapping Matrix
    juce::Label dspMappingHeader{"dspHdr", "Quick DSP MIDI Assignment"};
    juce::TextEditor dspHelpEditor;
    juce::TextButton mapAllFadersBtn{"Quick Map 4 Faders + Master"};
    juce::TextButton mapNanoKontrolBtn{"Apply Korg nanoKONTROL2 Preset"};
    juce::TextButton mapRelativeRotaryBtn{"Apply Relative Rotary Encoders"};

    // SubTab 4: DSP Bridge
    juce::Label dspBridgeHeader{"bridgeHdr", "VST3 DSP Bridge MIDI Routing"};
    juce::TextEditor dspBridgeInfo;

    // SubTab 0 & 1: Skin & GUI Settings
    juce::Label skinHeader{"skinHdr", "Console Theme & Visual Ergonomics"};
    juce::Label guiHeader{"guiHdr", "User Interface Scaling & Animation"};

    // Banner: Learn Active Banner
    juce::Label learnBanner;
    juce::TextButton cancelLearnBtn{"Cancel Learn"};

    // Banner: Warning notification
    juce::Label warningBanner;
    int warningDismissTimer = 0;

    void updateSubTabVisibility();
    void updatePresetList();
    void layoutMappingsTable();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnComponent)
};

} // namespace LiveBus
