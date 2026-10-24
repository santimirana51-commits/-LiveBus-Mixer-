#pragma once
#include <JuceHeader.h>
#include "SceneManager.h"
#include "PresetManager.h"
#include "RecallFilterComponent.h"
#include "PresetManagerComponent.h"
#include "CopyToolsComponent.h"

namespace LiveBus
{

class ScenesConsoleComponent : public juce::Component,
                               public juce::TableListBoxModel
{
public:
    ScenesConsoleComponent(SceneManager& sm, PresetManager& pm);
    ~ScenesConsoleComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TableListBoxModel for Scene Manager
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void refreshScenes();
    void setActiveScene(const juce::String& name);
    void setUnsavedChanges(bool hasChanges);

    // Callbacks to MainComponent
    std::function<void(const juce::String& sceneName, const RecallFilter& filter)> onRecallSceneWithFilter;
    std::function<void(const juce::String& sceneName, const juce::String& desc)> onSaveScene;
    std::function<void(const juce::String& sceneName)> onDeleteScene;
    std::function<void(const juce::String& oldName, const juce::String& newName)> onRenameScene;
    std::function<void(const juce::String& srcName, const juce::String& destName)> onDuplicateScene;
    std::function<void(const juce::File& file)> onImportScene;
    std::function<void(const juce::String& sceneName, const juce::File& destFile)> onExportScene;
    std::function<void()> onUpdateActiveScene;

    // Direct access to sub-components
    RecallFilterComponent& getRecallFilterComponent() { return recallFilterComponent; }
    PresetManagerComponent& getPresetManagerComponent() { return presetManagerComponent; }
    CopyToolsComponent& getCopyToolsComponent() { return copyToolsComponent; }

    enum ConsoleSubTab
    {
        SubTabSceneManager = 0,
        SubTabRecallFilter,
        SubTabPresets,
        SubTabCopyTools,
        NumSubTabs
    };

    void selectSubTab(ConsoleSubTab tab);

private:
    SceneManager& sceneManager;
    PresetManager& presetManager;

    ConsoleSubTab currentSubTab = SubTabSceneManager;
    std::array<juce::TextButton, NumSubTabs> subTabButtons;

    // Scene Manager sub-view controls
    juce::TableListBox sceneTable;
    juce::TextButton recallBtn{ "Recall Scene..." };
    juce::TextButton newSceneBtn{ "New Scene" };
    juce::TextButton saveBtn{ "Save" };
    juce::TextButton saveAsBtn{ "Save As..." };
    juce::TextButton duplicateBtn{ "Duplicate" };
    juce::TextButton renameBtn{ "Rename" };
    juce::TextButton deleteBtn{ "Delete" };
    juce::TextButton importBtn{ "Import..." };
    juce::TextButton exportBtn{ "Export..." };
    juce::TextButton updateActiveBtn{ "Update Active" };

    juce::Label activeSceneBadge;
    juce::Label unsavedBadge;
    juce::Label sceneDetailsLabel;

    std::vector<SceneSummaryInfo> sceneSummaries;
    juce::String activeSceneName{ "Default" };
    bool unsavedChanges = false;

    // Embedded Components
    RecallFilterComponent recallFilterComponent;
    PresetManagerComponent presetManagerComponent;
    CopyToolsComponent copyToolsComponent;

    void updateVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScenesConsoleComponent)
};

} // namespace LiveBus
