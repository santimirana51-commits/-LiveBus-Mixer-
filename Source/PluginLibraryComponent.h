#pragma once
#include <JuceHeader.h>
#include "VST3HostEngine.h"
#include "PluginManageComponent.h"

namespace LiveBus
{

/**
 * Professional Plug-in Library component matching the reference design.
 * Features full metadata columns, search/filter, favorites, defaults, scan, and manage dialog.
 */
class PluginLibraryComponent : public juce::Component,
                               public juce::TableListBoxModel,
                               public juce::ChangeListener,
                               public juce::Timer
{
public:
    PluginLibraryComponent(VST3HostEngine& engine);
    ~PluginLibraryComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;

    // Callbacks
    std::function<void(const juce::PluginDescription& desc)> onPluginSelectedToLoad;
    std::function<void()> onCloseRequested;

    // Target selection for loading
    void setActiveTarget(int targetIndex) { activeTargetIndex = targetIndex; }
    int getActiveTarget() const { return activeTargetIndex; }
    void setIsolatedModeEnabled(bool enabled) { isolatedModeEnabled = enabled; }

private:
    void rebuildFilteredList();
    void loadSelectedPlugin();

    VST3HostEngine& hostEngine;
    int activeTargetIndex{4}; // Default to Stream Bus (4)
    bool isolatedModeEnabled{false};

    std::vector<PluginInfoItem> allPlugins;
    std::vector<PluginInfoItem> filteredPlugins;

    // Header elements
    juce::Label headerTitleLabel;
    juce::TextButton closeBtn{"X"};
    juce::Label subTitleInstructionLabel;

    // Search & Filter controls
    juce::TextEditor searchBox;
    juce::ComboBox categoryFilterCombo;
    juce::ComboBox manufacturerFilterCombo;
    juce::ToggleButton favoritesOnlyToggle{"Favorites Only"};

    // Table
    enum Columns
    {
        ColDefault = 1,
        ColFavorite = 2,
        ColName = 3,
        ColFormat = 4,
        ColCategory = 5,
        ColManufacturer = 6,
        ColProfile = 7
    };
    juce::TableListBox table;

    // Bottom Action Buttons
    juce::TextButton loadPluginBtn{"LOAD PLUG-IN"};
    juce::TextButton scanPluginsBtn{"SCAN PLUG-INS"};
    juce::TextButton manageBtn{"MANAGE"};
    juce::TextButton backBtn{"BACK"};

    // Scanning progress overlay
    juce::ProgressBar scanProgressBar;
    double scanProgressValue{0.0};
    juce::Label scanStatusLabel;
    juce::TextButton cancelScanBtn{"Cancel Scan"};

    // Manage Dialog sub-component
    std::unique_ptr<PluginManageComponent> manageModal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLibraryComponent)
};

} // namespace LiveBus
