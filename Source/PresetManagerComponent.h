#pragma once
#include <JuceHeader.h>
#include "PresetManager.h"
#include "EightBandEQ.h"
#include "VocalDSPProcessor.h"
#include "RoutingMatrix.h"

namespace LiveBus
{

class PresetManagerComponent : public juce::Component,
                               public juce::ListBoxModel
{
public:
    PresetManagerComponent(PresetManager& pm);
    ~PresetManagerComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void refreshList();

    // Callbacks to apply/capture presets from the live mixer engine
    std::function<void(PresetCategory, int targetIndex, const PresetItem&)> onLoadPresetToTarget;
    std::function<void(PresetCategory, int targetIndex, juce::String& outName)> onSavePresetFromTarget;

    PresetCategory getSelectedCategory() const;
    int getSelectedTargetIndex() const;

private:
    PresetManager& presetManager;

    // Controls
    juce::ComboBox categoryCombo;
    juce::ComboBox targetCombo;
    juce::ListBox presetListBox;
    juce::TextEditor searchBox;
    juce::Label descriptionLabel;

    juce::TextButton loadBtn{ "Load Preset" };
    juce::TextButton saveBtn{ "Save" };
    juce::TextButton saveAsBtn{ "Save As..." };
    juce::TextButton duplicateBtn{ "Duplicate" };
    juce::TextButton renameBtn{ "Rename" };
    juce::TextButton deleteBtn{ "Delete" };
    juce::TextButton importBtn{ "Import..." };
    juce::TextButton exportBtn{ "Export..." };

    juce::Label statusLabel;

    std::vector<PresetItem> currentPresets;
    std::vector<PresetItem> filteredPresets;

    void updateFilteredList();
    void updateTargetOptions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManagerComponent)
};

} // namespace LiveBus
