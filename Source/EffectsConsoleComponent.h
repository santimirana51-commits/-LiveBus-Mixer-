#pragma once
#include <JuceHeader.h>
#include "VST3HostEngine.h"
#include "PluginLibraryComponent.h"
#include "PluginEditorWindow.h"

class RNNoiseVST3Plugin;

namespace LiveBus
{

/**
 * Professional VST3 Effects Console Component for TabEffects.
 * Provides target selection (Inputs 1..16, Stream Bus, Monitor Bus),
 * chain slot management (Bypass, GUI Editor, Reorder, Remove),
 * Mode toggle (In-Process vs Out-of-Process Isolation), and Plug-in Library integration.
 */
class EffectsConsoleComponent : public juce::Component,
                                public juce::ChangeListener,
                                public juce::Timer
{
public:
    EffectsConsoleComponent(VST3HostEngine& engine, PluginHostClient& isolatedClient);
    ~EffectsConsoleComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    void selectTarget(int targetIndex);
    int getSelectedTarget() const { return currentTargetIndex; }
    std::function<void(int)> onTargetSelected;

    void showPluginLibrary(bool show);
    bool isShowingPluginLibrary() const { return showingLibrary; }

    PluginLibraryComponent* getLibraryComponent() { return libraryComponent.get(); }

private:
    void rebuildSlotViews();
    void openEditorForSlot(int slotIndex);

    VST3HostEngine& hostEngine;
    PluginHostClient& outOfProcessClient;
    int currentTargetIndex{4}; // Default to Stream Bus (4)
    bool showingLibrary{false};

    // Header & Target Navigation
    juce::Label titleLabel{"title", "VST3 Effects Host & Plugin Insert Rack"};
    juce::ComboBox targetCombo;

    // Mode Selector & Global Controls
    juce::ToggleButton isolatedModeToggle{"Out-of-Process Isolation (Crash-Resilient)"};
    juce::TextButton openLibraryBtn{"[Open Plug-in Library]"};
    juce::TextButton insertPluginBtn{"+ Insert Plug-in..."};
    juce::TextButton bypassAllBtn{"Bypass Target Chain"};
    juce::TextButton clearChainBtn{"Clear Chain"};
    juce::TextButton testWatchdogBtn{"Test Watchdog Recovery"};

    // Telemetry & Status
    juce::Label statusTelemetryLabel;
    juce::Label latencyTelemetryLabel;

    // Slot UI Row components
    struct SlotRowComponent : public juce::Component
    {
        SlotRowComponent(EffectsConsoleComponent& owner, int slotIdx);
        void paint(juce::Graphics& g) override;
        void resized() override;

        EffectsConsoleComponent& parent;
        int slotIndex{0};

        juce::Label slotNumLabel;
        juce::Label pluginNameLabel;
        juce::Label latencyLabel;
        juce::ToggleButton bypassToggle{"Bypass"};
        juce::TextButton openEditorBtn{"Open GUI"};
        juce::TextButton moveUpBtn{"^"};
        juce::TextButton moveDownBtn{"v"};
        juce::TextButton removeBtn{"X"};
    };

    juce::Viewport chainViewport;
    juce::Component chainHolder;
    std::vector<std::unique_ptr<SlotRowComponent>> slotRows;

    // Plug-in Library Overlay View
    std::unique_ptr<PluginLibraryComponent> libraryComponent;

    // Direct bundled plugin processor instance for editing when running in out-of-process isolation
    std::unique_ptr<RNNoiseVST3Plugin> internalRNNoisePlugin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsConsoleComponent)
};

} // namespace LiveBus
