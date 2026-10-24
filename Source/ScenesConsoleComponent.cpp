#include "ScenesConsoleComponent.h"

namespace LiveBus
{

ScenesConsoleComponent::ScenesConsoleComponent(SceneManager& sm, PresetManager& pm)
    : sceneManager(sm),
      presetManager(pm),
      presetManagerComponent(pm)
{
    // Sub-tab buttons
    juce::String tabNames[NumSubTabs] = {
        "Scene Manager",
        "Recall Filter",
        "Preset Manager",
        "Copy & Tools"
    };

    for (int i = 0; i < NumSubTabs; ++i)
    {
        subTabButtons[i].setButtonText(tabNames[i]);
        subTabButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff22252c));
        subTabButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc2c8d2));
        subTabButtons[i].onClick = [this, i] { selectSubTab((ConsoleSubTab)i); };
        addAndMakeVisible(subTabButtons[i]);
    }

    // Active scene badges
    activeSceneBadge.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    activeSceneBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00d2ff));
    addAndMakeVisible(activeSceneBadge);

    unsavedBadge.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    unsavedBadge.setColour(juce::Label::textColourId, juce::Colour(0xffff9800));
    addAndMakeVisible(unsavedBadge);

    sceneDetailsLabel.setFont(juce::FontOptions(12.0f, juce::Font::italic));
    sceneDetailsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8892a0));
    addAndMakeVisible(sceneDetailsLabel);

    // Scene Table
    sceneTable.setModel(this);
    sceneTable.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff15171c));
    sceneTable.getHeader().addColumn("Status", 1, 90, 80, 120);
    sceneTable.getHeader().addColumn("Scene Name", 2, 220, 150, 350);
    sceneTable.getHeader().addColumn("Description", 3, 380, 200, 600);
    sceneTable.getHeader().addColumn("Last Modified", 4, 180, 140, 250);
    sceneTable.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff20242b));
    sceneTable.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colours::white);
    addAndMakeVisible(sceneTable);

    // Scene Manager Buttons
    recallBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff124d2d));
    recallBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    recallBtn.onClick = [this] {
        int row = sceneTable.getSelectedRow();
        if (row >= 0 && row < (int)sceneSummaries.size())
        {
            auto name = sceneSummaries[(size_t)row].name;
            recallFilterComponent.setSceneName(name);
            selectSubTab(SubTabRecallFilter);
        }
    };
    addAndMakeVisible(recallBtn);

    newSceneBtn.onClick = [this] {
        juce::AlertWindow alert("New Scene", "Enter scene name:", juce::AlertWindow::QuestionIcon);
        alert.addTextEditor("name", "NewScene");
        alert.addTextEditor("desc", "Default scene description");
        alert.addButton("Create", 1);
        alert.addButton("Cancel", 0);
        if (alert.runModalLoop() == 1)
        {
            auto name = alert.getTextEditorContents("name").trim();
            auto desc = alert.getTextEditorContents("desc").trim();
            if (name.isNotEmpty() && onSaveScene)
            {
                onSaveScene(name, desc);
                refreshScenes();
            }
        }
    };
    addAndMakeVisible(newSceneBtn);

    saveBtn.onClick = [this] {
        int row = sceneTable.getSelectedRow();
        juce::String name = activeSceneName;
        juce::String desc = "Snapshot";
        if (row >= 0 && row < (int)sceneSummaries.size())
        {
            name = sceneSummaries[(size_t)row].name;
            desc = sceneSummaries[(size_t)row].description;
        }
        if (onSaveScene)
        {
            onSaveScene(name, desc);
            refreshScenes();
        }
    };
    addAndMakeVisible(saveBtn);

    saveAsBtn.onClick = [this] {
        juce::AlertWindow alert("Save Scene As...", "Enter scene name and description:", juce::AlertWindow::QuestionIcon);
        alert.addTextEditor("name", activeSceneName + "_Copy");
        alert.addTextEditor("desc", "Snapshot of " + activeSceneName);
        alert.addButton("Save", 1);
        alert.addButton("Cancel", 0);
        if (alert.runModalLoop() == 1)
        {
            auto name = alert.getTextEditorContents("name").trim();
            auto desc = alert.getTextEditorContents("desc").trim();
            if (name.isNotEmpty() && onSaveScene)
            {
                onSaveScene(name, desc);
                refreshScenes();
            }
        }
    };
    addAndMakeVisible(saveAsBtn);

    duplicateBtn.onClick = [this] {
        int row = sceneTable.getSelectedRow();
        if (row >= 0 && row < (int)sceneSummaries.size())
        {
            auto src = sceneSummaries[(size_t)row].name;
            auto dest = src + "_Copy";
            if (onDuplicateScene)
            {
                onDuplicateScene(src, dest);
                refreshScenes();
            }
        }
    };
    addAndMakeVisible(duplicateBtn);

    renameBtn.onClick = [this] {
        int row = sceneTable.getSelectedRow();
        if (row >= 0 && row < (int)sceneSummaries.size())
        {
            auto oldName = sceneSummaries[(size_t)row].name;
            juce::AlertWindow alert("Rename Scene", "Enter new name for " + oldName + ":", juce::AlertWindow::QuestionIcon);
            alert.addTextEditor("name", oldName);
            alert.addButton("Rename", 1);
            alert.addButton("Cancel", 0);
            if (alert.runModalLoop() == 1)
            {
                auto newName = alert.getTextEditorContents("name").trim();
                if (newName.isNotEmpty() && newName != oldName && onRenameScene)
                {
                    onRenameScene(oldName, newName);
                    refreshScenes();
                }
            }
        }
    };
    addAndMakeVisible(renameBtn);

    deleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a1b20));
    deleteBtn.onClick = [this] {
        int row = sceneTable.getSelectedRow();
        if (row >= 0 && row < (int)sceneSummaries.size())
        {
            auto name = sceneSummaries[(size_t)row].name;
            if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Delete Scene",
                "Are you sure you want to delete scene '" + name + "'?"))
            {
                if (onDeleteScene)
                {
                    onDeleteScene(name);
                    refreshScenes();
                }
            }
        }
    };
    addAndMakeVisible(deleteBtn);

    importBtn.onClick = [this] {
        juce::FileChooser fc("Import Scene XML", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
        if (fc.browseForFileToOpen())
        {
            if (onImportScene)
            {
                onImportScene(fc.getResult());
                refreshScenes();
            }
        }
    };
    addAndMakeVisible(importBtn);

    exportBtn.onClick = [this] {
        int row = sceneTable.getSelectedRow();
        if (row >= 0 && row < (int)sceneSummaries.size())
        {
            auto name = sceneSummaries[(size_t)row].name;
            juce::FileChooser fc("Export Scene XML", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(name + ".xml"), "*.xml");
            if (fc.browseForFileToSave(true))
            {
                if (onExportScene)
                    onExportScene(name, fc.getResult());
            }
        }
    };
    addAndMakeVisible(exportBtn);

    updateActiveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff124d2d));
    updateActiveBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    updateActiveBtn.onClick = [this] {
        if (onUpdateActiveScene)
        {
            onUpdateActiveScene();
            refreshScenes();
        }
    };
    addAndMakeVisible(updateActiveBtn);

    // Embedded Components
    addAndMakeVisible(recallFilterComponent);
    addAndMakeVisible(presetManagerComponent);
    addAndMakeVisible(copyToolsComponent);

    // Recall Filter Wire-Up
    recallFilterComponent.onApply = [this](const RecallFilter& filter) {
        auto sceneName = recallFilterComponent.getSceneName();
        if (onRecallSceneWithFilter)
            onRecallSceneWithFilter(sceneName, filter);
        selectSubTab(SubTabSceneManager);
    };

    recallFilterComponent.onCancel = [this] {
        // Cancel does NOT alter live mixer state
        selectSubTab(SubTabSceneManager);
    };

    refreshScenes();
    selectSubTab(SubTabSceneManager);
}

void ScenesConsoleComponent::selectSubTab(ConsoleSubTab tab)
{
    currentSubTab = tab;
    for (int i = 0; i < NumSubTabs; ++i)
    {
        bool isSel = (i == (int)tab);
        subTabButtons[i].setColour(juce::TextButton::buttonColourId, isSel ? juce::Colour(0xff0095c8) : juce::Colour(0xff22252c));
        subTabButtons[i].setColour(juce::TextButton::textColourOffId, isSel ? juce::Colours::white : juce::Colour(0xffc2c8d2));
    }
    updateVisibility();
    resized();
    repaint();
}

void ScenesConsoleComponent::updateVisibility()
{
    bool isManager = (currentSubTab == SubTabSceneManager);
    sceneTable.setVisible(isManager);
    recallBtn.setVisible(isManager);
    newSceneBtn.setVisible(isManager);
    saveBtn.setVisible(isManager);
    saveAsBtn.setVisible(isManager);
    duplicateBtn.setVisible(isManager);
    renameBtn.setVisible(isManager);
    deleteBtn.setVisible(isManager);
    importBtn.setVisible(isManager);
    exportBtn.setVisible(isManager);
    updateActiveBtn.setVisible(isManager);
    activeSceneBadge.setVisible(isManager);
    unsavedBadge.setVisible(isManager);
    sceneDetailsLabel.setVisible(isManager);

    recallFilterComponent.setVisible(currentSubTab == SubTabRecallFilter);
    presetManagerComponent.setVisible(currentSubTab == SubTabPresets);
    copyToolsComponent.setVisible(currentSubTab == SubTabCopyTools);
}

void ScenesConsoleComponent::refreshScenes()
{
    sceneSummaries = sceneManager.getSceneSummaries();
    sceneTable.updateContent();
    sceneTable.repaint();
}

void ScenesConsoleComponent::setActiveScene(const juce::String& name)
{
    activeSceneName = name.isNotEmpty() ? name : "Default";
    activeSceneBadge.setText("Active Scene: " + activeSceneName, juce::dontSendNotification);
    copyToolsComponent.setActiveSceneName(activeSceneName);
    sceneTable.repaint();
}

void ScenesConsoleComponent::setUnsavedChanges(bool hasChanges)
{
    unsavedChanges = hasChanges;
    unsavedBadge.setText(hasChanges ? "[* UNSAVED CHANGES]" : "[SAVED]", juce::dontSendNotification);
    unsavedBadge.setColour(juce::Label::textColourId, hasChanges ? juce::Colour(0xffff9800) : juce::Colour(0xff4caf50));
}

int ScenesConsoleComponent::getNumRows()
{
    return (int)sceneSummaries.size();
}

void ScenesConsoleComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.setColour(juce::Colour(0xff007ba8));
        g.fillAll();
    }
    else if (rowNumber % 2 == 0)
    {
        g.setColour(juce::Colour(0xff1a1d24));
        g.fillAll();
    }
    else
    {
        g.setColour(juce::Colour(0xff15171c));
        g.fillAll();
    }
}

void ScenesConsoleComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)sceneSummaries.size()) return;
    const auto& s = sceneSummaries[(size_t)rowNumber];
    bool isActive = (s.name == activeSceneName);

    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffe1e6ed));

    if (columnId == 1) // Status
    {
        if (isActive)
        {
            auto badgeRect = juce::Rectangle<float>(6.0f, (float)(height - 18) * 0.5f, 68.0f, 18.0f);
            g.setColour(juce::Colour(0xff145a32));
            g.fillRoundedRectangle(badgeRect, 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText("ACTIVE", badgeRect, juce::Justification::centred, false);
        }
    }
    else if (columnId == 2) // Name
    {
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(s.name, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
    }
    else if (columnId == 3) // Description
    {
        g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
        g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.85f) : juce::Colour(0xff98a2b3));
        g.drawText(s.description, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
    }
    else if (columnId == 4) // Last Modified
    {
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.75f) : juce::Colour(0xff758091));
        g.drawText(s.timestampFormatted, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
    }
}

void ScenesConsoleComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < (int)sceneSummaries.size())
    {
        sceneDetailsLabel.setText(sceneSummaries[(size_t)lastRowSelected].description, juce::dontSendNotification);
    }
}

void ScenesConsoleComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff14161a));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xff2d323b));
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
}

void ScenesConsoleComponent::resized()
{
    auto b = getLocalBounds().reduced(12);

    // Top Sub-nav Bar
    int navX = b.getX();
    int navY = b.getY();
    int tabW = 150;
    int tabH = 30;

    for (int i = 0; i < NumSubTabs; ++i)
    {
        subTabButtons[i].setBounds(navX, navY, tabW, tabH);
        navX += tabW + 6;
    }

    // Active badges on the right
    activeSceneBadge.setBounds(b.getRight() - 340, navY, 200, tabH);
    unsavedBadge.setBounds(b.getRight() - 130, navY, 130, tabH);

    int contentY = navY + tabH + 12;
    int contentH = b.getHeight() - (tabH + 12);

    if (currentSubTab == SubTabSceneManager)
    {
        int actionColW = 140;
        int tableW = b.getWidth() - actionColW - 16;
        int tableH = contentH - 36;

        sceneTable.setBounds(b.getX(), contentY, tableW, tableH);
        sceneDetailsLabel.setBounds(b.getX(), sceneTable.getBottom() + 6, tableW, 24);

        int btnX = sceneTable.getRight() + 16;
        int btnY = contentY;
        int btnH = 28;
        int spacing = 6;

        recallBtn.setBounds(btnX, btnY, actionColW, 36); btnY += 42;
        updateActiveBtn.setBounds(btnX, btnY, actionColW, 32); btnY += 38;
        newSceneBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
        saveBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
        saveAsBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
        duplicateBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
        renameBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
        deleteBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing + 6;
        importBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
        exportBtn.setBounds(btnX, btnY, actionColW, btnH);
    }
    else if (currentSubTab == SubTabRecallFilter)
    {
        recallFilterComponent.setBounds(b.getX(), contentY, b.getWidth(), contentH);
    }
    else if (currentSubTab == SubTabPresets)
    {
        presetManagerComponent.setBounds(b.getX(), contentY, b.getWidth(), contentH);
    }
    else if (currentSubTab == SubTabCopyTools)
    {
        copyToolsComponent.setBounds(b.getX(), contentY, b.getWidth(), contentH);
    }
}

} // namespace LiveBus
