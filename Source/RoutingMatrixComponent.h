#pragma once
#include <JuceHeader.h>
#include "RoutingMatrix.h"
#include "AudioDeviceService.h"
#include "RoutingGraphComponent.h"
#include <vector>
#include <memory>

namespace LiveBus
{

class SignalFlowWindow;

class RoutingMatrixComponent final : public juce::Component,
                                     public juce::Button::Listener,
                                     public juce::Slider::Listener,
                                     public juce::ComboBox::Listener,
                                     public juce::TextEditor::Listener,
                                     private juce::Timer
{
public:
    RoutingMatrixComponent(RoutingMatrix& matrixRef, AudioDeviceService* deviceService = nullptr,
                           std::function<bool(int)> busMutedProvider = {});
    ~RoutingMatrixComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void comboBoxChanged(juce::ComboBox* c) override;
    void textEditorFocusLost(juce::TextEditor& ed) override;
    void textEditorReturnKeyPressed(juce::TextEditor& ed) override;

    void setSelectedChannel(int channelIndex);
    int getSelectedChannel() const noexcept { return selectedChannelIndex; }

    void refreshFromMatrix();
    void syncFromMatrix();
    void updateUIFromMatrix() { refreshFromMatrix(); }
    void showRoutingGraph();
    void refreshRoutingGraph();
    void focusRoutingGraphNode(const juce::String& typeId, int index);
    void updateAvailableHardwareInputs(int count, const juce::StringArray& inputNames);
    void updateAvailableHardwareOutputs(int count, const juce::StringArray& outputNames);
    juce::String getRoutingGraphLayoutXml() const;
    void setRoutingGraphLayoutXml(const juce::String& xml);
    std::function<void(int)> onChannelSelected;
    std::function<void(juce::Component*)> onAddInputNodeRequested;
    std::function<void(RoutingGraphPaletteAction)> onPaletteActionRequested;
    std::function<void(int)> onDSPNodeSelected;
    std::function<void(int)> onVSTChainSelected;
    RoutingGraphComponent::VSTChainPluginsProvider vstChainPluginsProvider;
    RoutingGraphComponent::MasterBusControlCallbacks masterBusControlCallbacks;
    RoutingGraphComponent::DSPControlCallbacks dspControlCallbacks;

private:
    void timerCallback() override;
    void buildMatrixRow(int ch);
    void updateInspectorForSelectedChannel();
    void setAdvancedRoutingVisible(bool shouldBeVisible);
    void createRoutingGraph();
    void applySelectedPreset();
    void copySelectedChannelRoutingToAll();
    void resetAllRoutingToDefaults();

    RoutingMatrix& matrix;
    AudioDeviceService* audioDeviceService = nullptr;
    std::function<bool(int)> busMutedProvider;
    int selectedChannelIndex = 0;
    int numHardwareInputsAvailable = 2;
    juce::StringArray hardwareInputChannelNames;
    int numHardwareOutputsAvailable = 2;
    juce::StringArray hardwareOutputChannelNames;

    // Top Header & Action Bar
    juce::Label headerTitle;
    juce::Label headerSubtitle;
    juce::Label warningBanner;

    juce::Label presetLabel{"Preset:"};
    juce::ComboBox presetCombo;
    juce::TextButton applyPresetBtn{"Apply Preset"};
    juce::TextButton resetRoutesBtn{"Reset All Routes"};
    juce::TextButton copyToAllBtn{"Copy Routing to All"};
    juce::TextButton signalFlowBtn{"Node Graph"};
    juce::TextButton advancedRoutingBtn{"Advanced Sends"};
    std::unique_ptr<SignalFlowWindow> signalFlowWindow;
    std::unique_ptr<RoutingGraphComponent> routingGraph;
    juce::String pendingRoutingGraphLayoutXml;
    bool advancedRoutingVisible = false;

    // Main Matrix Table
    juce::Viewport matrixViewport;
    std::unique_ptr<juce::Component> matrixTableContainer;

    struct MatrixRowUI
    {
        juce::Label chNumLabel;
        juce::TextEditor nameEditor;
        juce::ComboBox inputSourceCombo;
        juce::ComboBox outputRouteCombo;
        juce::ComboBox stereoModeCombo;
        juce::ComboBox submixGroupCombo;
        juce::Label statusBadge;
        juce::ToggleButton streamEnableBtn{"Stream"};
        juce::Slider streamSendSlider;
        juce::ToggleButton monitorEnableBtn{"Monitor"};
        juce::Slider monitorSendSlider;
        juce::ToggleButton aux1EnableBtn{"Rev"};
        juce::Slider aux1SendSlider;
        juce::ToggleButton aux2EnableBtn{"Dly"};
        juce::Slider aux2SendSlider;
        // Direct Hardware Outputs (HW 1, HW 2, HW 3, HW 4)
        std::array<juce::ToggleButton, 4> hwEnableBtns;
        juce::Slider panSlider;
        juce::Slider gainSlider;
        juce::ToggleButton muteBtn{"M"};
        juce::ToggleButton soloBtn{"S"};
        juce::TextButton inspectBtn{"Inspect"};
        float inMeterLevel = 0.0f;
    };
    std::vector<std::unique_ptr<MatrixRowUI>> rowUIs;

    // Channel Routing Inspector Panel (Lower Section)
    juce::GroupComponent inspectorGroup{"inspectorGroup", "CHANNEL ROUTING DETAIL INSPECTOR"};
    juce::Label inspectorChTitle;
    std::array<juce::TextButton, maxInputs> channelSelectorTabs;

    // Inspector - Section 1: Input Source
    juce::GroupComponent inputGroup{"inputGroup", "1. INPUT SOURCE & HARDWARE MAPPING"};
    juce::Label inputSourceTypeLabel{"Source Type:"};
    juce::ComboBox inspectorSourceTypeCombo;
    juce::Label hardwarePortLabel{"Physical Hardware Port:"};
    juce::ComboBox inspectorHardwarePortCombo;
    juce::Label stereoModeLabel{"Stereo Mode:"};
    juce::ComboBox inspectorStereoModeCombo;
    juce::Label duplicateWarningLabel;
    juce::ToggleButton channelEnableToggle{"Input Channel Active"};

    // Inspector - Section 2: Output Bus Sends, Submix & Aux Sends
    juce::GroupComponent sendGroup{"sendGroup", "2. OUTPUT BUS DISTRIBUTION, SUBMIX & AUX SENDS"};
    juce::Label submixGroupLabel{"Submix Bus:"};
    juce::ComboBox inspectorSubmixCombo;

    juce::ToggleButton streamSendEnableToggle{"Send to Stream Bus (OBS)"};
    juce::ComboBox streamPrePostCombo;
    juce::Slider streamSendSlider;
    juce::Label streamSendDbLabel;

    juce::ToggleButton monitorSendEnableToggle{"Send to Monitor Bus (Headphones)"};
    juce::ComboBox monitorPrePostCombo;
    juce::Slider monitorSendSlider;
    juce::Label monitorSendDbLabel;

    // Aux Sends (FX Tracks)
    juce::ToggleButton aux1SendEnableToggle{"Aux 1 Send (Reverb)"};
    juce::ComboBox aux1PrePostCombo;
    juce::Slider aux1SendSlider;
    juce::Label aux1SendDbLabel;

    juce::ToggleButton aux2SendEnableToggle{"Aux 2 Send (Delay)"};
    juce::ComboBox aux2PrePostCombo;
    juce::Slider aux2SendSlider;
    juce::Label aux2SendDbLabel;

    // Direct Hardware Output Distribution in Section 2
    juce::Label hwDirectLabel{"Direct Hardware Outputs:"};
    std::array<juce::ToggleButton, 4> inspectorHwEnableToggles;
    std::array<juce::Slider, 4> inspectorHwSendSliders;
    std::array<juce::Label, 4> inspectorHwSendLabels;

    // Inspector - Section 3: Pan, Gain & Status
    juce::GroupComponent panGroup{"panGroup", "3. PAN, GAIN & STATUS"};
    juce::Label panLabel{"Pan / Balance:"};
    juce::Slider panKnob;
    juce::Label panValueLabel;
    juce::Label gainLabel{"Channel Fader:"};
    juce::Slider channelGainSlider;
    juce::Label gainValueLabel;
    juce::ToggleButton muteBtn{"MUTE"};
    juce::ToggleButton soloBtn{"SOLO"};
    juce::Label statusDetailLabel;

    // Section 4: Aux Return & Submix Bus Masters (DAW Style FX Returns & Group Faders)
    juce::GroupComponent auxSubmixGroup{"auxSubmixGroup", "4. AUX RETURN & SUBMIX BUS MASTERS (FX TRACKS & GROUP FADERS)"};
    juce::Label aux1ReturnLabel{"Aux 1 Reverb:"};
    juce::Slider aux1ReturnSlider;
    juce::Label aux1ReturnDbLabel;
    juce::Slider aux1RoomSizeSlider;
    juce::ToggleButton aux1MuteBtn{"Mute"};

    juce::Label aux2ReturnLabel{"Aux 2 Delay:"};
    juce::Slider aux2ReturnSlider;
    juce::Label aux2ReturnDbLabel;
    juce::Slider aux2DelayTimeSlider;
    juce::ToggleButton aux2MuteBtn{"Mute"};

    juce::Label submix1Label{"Sub 1 (Vocals):"};
    juce::Slider submix1FaderSlider;
    juce::Label submix1DbLabel;
    juce::ToggleButton submix1MuteBtn{"Mute"};

    juce::Label submix2Label{"Sub 2 (BGM / Music):"};
    juce::Slider submix2FaderSlider;
    juce::Label submix2DbLabel;
    juce::ToggleButton submix2MuteBtn{"Mute"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingMatrixComponent)
};

} // namespace LiveBus
