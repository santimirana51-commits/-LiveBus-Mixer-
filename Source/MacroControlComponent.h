#pragma once
#include <JuceHeader.h>
#include "MacroEngine.h"

namespace LiveBus
{

class MacroControlComponent : public juce::Component,
                              private juce::Button::Listener,
                              private juce::Slider::Listener,
                              private juce::ComboBox::Listener,
                              private MacroEngine::Listener
{
public:
    explicit MacroControlComponent(MacroEngine& engineRef);
    ~MacroControlComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void comboBoxChanged(juce::ComboBox* cb) override;

    void onMacroValueChanged(int macroIndex, float newValue) override;
    void onMacroStructureChanged() override;

    void selectMacroForEditing(int macroIndex);
    void refreshAssignmentList();

    MacroEngine& engine;
    int selectedMacroIndex = 0;
    bool isInternalUpdate = false;

    // Header & Quick Actions
    juce::Label headerTitle{"Macro Control & Quick Actions Engine"};
    juce::GroupComponent quickActionsGroup{"quickActionsGroup", "QUICK ACTIONS"};
    juce::TextButton qaEmergencyMuteBtn{"🚨 Emergency Mute"};
    juce::TextButton qaStreamMuteBtn{"Mute Stream"};
    juce::TextButton qaMonitorMuteBtn{"Mute Monitor"};
    juce::TextButton qaDuckingToggleBtn{"Toggle Ducking"};
    juce::TextButton qaResetPeakBtn{"Reset Peak"};
    juce::TextButton qaPlaybackToggleBtn{"▶ Playback"};

    // Macro Cards Grid
    struct MacroCardUI
    {
        juce::GroupComponent cardBox;
        juce::Label nameLabel;
        juce::Label descLabel;
        juce::Slider valueSlider; // Rotary knob
        juce::Label assignmentCountLabel;
        juce::TextButton editBtn{"EDIT"};
        juce::TextButton resetBtn{"RESET"};
    };
    std::vector<std::unique_ptr<MacroCardUI>> cardUIs;

    // Macro Assignment Inspector / Editor
    juce::GroupComponent inspectorGroup{"inspectorGroup", "MACRO ASSIGNMENT INSPECTOR"};
    juce::Label inspectorTitleLabel;
    juce::ListBox assignmentListBox;

    class AssignmentListModel : public juce::ListBoxModel
    {
    public:
        AssignmentListModel(MacroControlComponent& o) : owner(o) {}
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    private:
        MacroControlComponent& owner;
    };
    std::unique_ptr<AssignmentListModel> listModel;
    int selectedAssignmentIndex = -1;

    // Assignment Edit Controls
    juce::Label targetParamLabel{"Target Parameter:"};
    juce::ComboBox targetParamCombo;
    juce::Label minValLabel{"Min Value:"};
    juce::Slider minValSlider;
    juce::Label maxValLabel{"Max Value:"};
    juce::Slider maxValSlider;
    juce::Label curveLabel{"Curve:"};
    juce::ComboBox curveCombo;
    juce::ToggleButton invertToggle{"Invert Mapping"};
    juce::TextButton addAssignmentBtn{"+ Add Assignment"};
    juce::TextButton removeAssignmentBtn{"- Remove Selected"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroControlComponent)
};

} // namespace LiveBus
