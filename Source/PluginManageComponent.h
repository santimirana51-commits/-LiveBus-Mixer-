#pragma once
#include <JuceHeader.h>
#include "VST3HostEngine.h"

namespace LiveBus
{

/**
 * Manage Dialog for VST3 search directories, scanner settings, and cache maintenance.
 */
class PluginManageComponent : public juce::Component,
                              public juce::ListBoxModel
{
public:
    PluginManageComponent(VST3HostEngine& engine, std::function<void()> onClose = nullptr);
    ~PluginManageComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel for paths
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    std::function<void()> onCloseRequested;

private:
    void refreshPathList();

    VST3HostEngine& hostEngine;
    juce::StringArray currentPaths;

    juce::Label headerTitleLabel{"title", "MANAGE PLUG-IN DIRECTORIES & SCANNER"};
    juce::TextButton closeBtn{"X"};

    // Search Directories Panel
    juce::GroupComponent pathsGroup{"pathsGroup", "Configured VST3 Search Paths"};
    juce::ListBox pathsListBox;
    juce::TextButton addPathBtn{"+ Add Path..."};
    juce::TextButton removePathBtn{"- Remove Path"};
    juce::ToggleButton scanStandardToggle{"Include Standard Windows VST3 Folder (Common Files\\VST3)"};

    // Maintenance Panel
    juce::GroupComponent maintenanceGroup{"maintGroup", "Scanner Cache & Troubleshooting"};
    juce::TextButton fullRescanBtn{"Full Rescan Plug-ins"};
    juce::TextButton clearCacheBtn{"Clear Plug-in Cache"};
    juce::Label statusSummaryLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManageComponent)
};

} // namespace LiveBus
