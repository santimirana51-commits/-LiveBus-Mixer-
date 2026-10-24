#include "PresetManagerComponent.h"

namespace LiveBus
{

PresetManagerComponent::PresetManagerComponent(PresetManager& pm)
    : presetManager(pm)
{
    // Category combo
    categoryCombo.addItem("Vocal DSP Rack", 1);
    categoryCombo.addItem("8-Band Parametric EQ", 2);
    categoryCombo.addItem("Channel Strip", 3);
    categoryCombo.addItem("Bus Master DSP", 4);
    categoryCombo.setSelectedId(1, juce::dontSendNotification);
    categoryCombo.onChange = [this] {
        updateTargetOptions();
        refreshList();
    };
    addAndMakeVisible(categoryCombo);

    // Target combo
    addAndMakeVisible(targetCombo);
    updateTargetOptions();

    // Search box
    searchBox.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
    searchBox.onTextChange = [this] { updateFilteredList(); };
    addAndMakeVisible(searchBox);

    // ListBox
    presetListBox.setModel(this);
    presetListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff16191f));
    presetListBox.setRowHeight(38);
    addAndMakeVisible(presetListBox);

    // Description
    descriptionLabel.setFont(juce::FontOptions(12.0f, juce::Font::italic));
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0aab8));
    addAndMakeVisible(descriptionLabel);

    // Status label
    statusLabel.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d2ff));
    addAndMakeVisible(statusLabel);

    // Action buttons
    loadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff145a32));
    loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadBtn.onClick = [this] {
        int row = presetListBox.getSelectedRow();
        if (row >= 0 && row < (int)filteredPresets.size())
        {
            if (onLoadPresetToTarget)
                onLoadPresetToTarget(getSelectedCategory(), getSelectedTargetIndex(), filteredPresets[(size_t)row]);
            statusLabel.setText("Loaded preset: " + filteredPresets[(size_t)row].name, juce::dontSendNotification);
        }
    };
    addAndMakeVisible(loadBtn);

    saveBtn.onClick = [this] {
        int row = presetListBox.getSelectedRow();
        if (row >= 0 && row < (int)filteredPresets.size())
        {
            const auto& item = filteredPresets[(size_t)row];
            if (item.isFactory)
            {
                juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Factory Preset",
                    "Factory presets cannot be overwritten. Please use 'Save As...' to create a user preset.", "OK");
                return;
            }
            juce::String name = item.name;
            if (onSavePresetFromTarget)
                onSavePresetFromTarget(getSelectedCategory(), getSelectedTargetIndex(), name);
            refreshList();
            statusLabel.setText("Saved preset: " + name, juce::dontSendNotification);
        }
    };
    addAndMakeVisible(saveBtn);

    saveAsBtn.onClick = [this] {
        juce::AlertWindow alert("Save Preset As...", "Enter preset name:", juce::AlertWindow::QuestionIcon);
        alert.addTextEditor("name", "NewPreset");
        alert.addButton("Save", 1);
        alert.addButton("Cancel", 0);
        if (alert.runModalLoop() == 1)
        {
            auto name = alert.getTextEditorContents("name").trim();
            if (name.isNotEmpty())
            {
                if (onSavePresetFromTarget)
                    onSavePresetFromTarget(getSelectedCategory(), getSelectedTargetIndex(), name);
                refreshList();
                statusLabel.setText("Saved preset as: " + name, juce::dontSendNotification);
            }
        }
    };
    addAndMakeVisible(saveAsBtn);

    duplicateBtn.onClick = [this] {
        int row = presetListBox.getSelectedRow();
        if (row >= 0 && row < (int)filteredPresets.size())
        {
            auto item = filteredPresets[(size_t)row];
            auto newName = item.name + "_Copy";
            if (presetManager.duplicateUserPreset(item, newName))
            {
                refreshList();
                statusLabel.setText("Duplicated preset: " + newName, juce::dontSendNotification);
            }
        }
    };
    addAndMakeVisible(duplicateBtn);

    renameBtn.onClick = [this] {
        int row = presetListBox.getSelectedRow();
        if (row >= 0 && row < (int)filteredPresets.size())
        {
            const auto& item = filteredPresets[(size_t)row];
            if (item.isFactory)
            {
                juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Factory Preset",
                    "Factory presets cannot be renamed.", "OK");
                return;
            }
            juce::AlertWindow alert("Rename Preset", "Enter new name:", juce::AlertWindow::QuestionIcon);
            alert.addTextEditor("name", item.name);
            alert.addButton("Rename", 1);
            alert.addButton("Cancel", 0);
            if (alert.runModalLoop() == 1)
            {
                auto newName = alert.getTextEditorContents("name").trim();
                if (newName.isNotEmpty() && presetManager.renameUserPreset(item, newName))
                {
                    refreshList();
                    statusLabel.setText("Renamed preset to: " + newName, juce::dontSendNotification);
                }
            }
        }
    };
    addAndMakeVisible(renameBtn);

    deleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a1b20));
    deleteBtn.onClick = [this] {
        int row = presetListBox.getSelectedRow();
        if (row >= 0 && row < (int)filteredPresets.size())
        {
            const auto& item = filteredPresets[(size_t)row];
            if (item.isFactory)
            {
                juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Factory Preset",
                    "Factory presets cannot be deleted.", "OK");
                return;
            }
            if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon, "Delete Preset",
                "Delete preset '" + item.name + "'?"))
            {
                if (presetManager.deleteUserPreset(item))
                {
                    refreshList();
                    statusLabel.setText("Deleted preset: " + item.name, juce::dontSendNotification);
                }
            }
        }
    };
    addAndMakeVisible(deleteBtn);

    importBtn.onClick = [this] {
        juce::FileChooser fc("Import Preset XML", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
        if (fc.browseForFileToOpen())
        {
            juce::String outName, outErr;
            if (presetManager.importPresetFromFile(getSelectedCategory(), fc.getResult(), outName, outErr))
            {
                refreshList();
                statusLabel.setText("Imported preset: " + outName, juce::dontSendNotification);
            }
            else
            {
                juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Import Error", outErr, "OK");
            }
        }
    };
    addAndMakeVisible(importBtn);

    exportBtn.onClick = [this] {
        int row = presetListBox.getSelectedRow();
        if (row >= 0 && row < (int)filteredPresets.size())
        {
            const auto& item = filteredPresets[(size_t)row];
            juce::FileChooser fc("Export Preset XML", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(item.name + ".xml"), "*.xml");
            if (fc.browseForFileToSave(true))
            {
                if (presetManager.exportPresetToFile(item, fc.getResult()))
                {
                    statusLabel.setText("Exported preset to: " + fc.getResult().getFileName(), juce::dontSendNotification);
                }
            }
        }
    };
    addAndMakeVisible(exportBtn);

    refreshList();
}

PresetCategory PresetManagerComponent::getSelectedCategory() const
{
    return (PresetCategory)(categoryCombo.getSelectedId() - 1);
}

int PresetManagerComponent::getSelectedTargetIndex() const
{
    return targetCombo.getSelectedId() - 1;
}

void PresetManagerComponent::updateTargetOptions()
{
    targetCombo.clear();
    auto cat = getSelectedCategory();
    if (cat == PresetCategory::VocalDSP || cat == PresetCategory::EightBandEQ)
    {
        for (int channel = 0; channel < maxInputs; ++channel)
        {
            const auto name = channel == 0 ? "Input 1 - Mic"
                : channel == 1 ? "Input 2 - Guitar"
                : channel == 2 ? "Input 3 - Music L"
                : channel == 3 ? "Input 4 - Music R"
                : "Input " + juce::String(channel + 1);
            targetCombo.addItem(name, channel + 1);
        }
        targetCombo.addItem("Stream Bus (Master)", maxInputs + 1);
        targetCombo.addItem("Monitor Bus", maxInputs + 2);
        targetCombo.setSelectedId(1, juce::dontSendNotification);
    }
    else if (cat == PresetCategory::ChannelStrip)
    {
        for (int channel = 0; channel < maxInputs; ++channel)
        {
            const auto name = channel == 0 ? "Input 1 - Mic"
                : channel == 1 ? "Input 2 - Guitar"
                : channel == 2 ? "Input 3 - Music L"
                : channel == 3 ? "Input 4 - Music R"
                : "Input " + juce::String(channel + 1);
            targetCombo.addItem(name, channel + 1);
        }
        targetCombo.setSelectedId(1, juce::dontSendNotification);
    }
    else if (cat == PresetCategory::BusDSP)
    {
        targetCombo.addItem("Stream Bus (Master)", 1);
        targetCombo.addItem("Monitor Bus", 2);
        targetCombo.setSelectedId(1, juce::dontSendNotification);
    }
}

void PresetManagerComponent::refreshList()
{
    currentPresets = presetManager.getPresetsForCategory(getSelectedCategory());
    updateFilteredList();
}

void PresetManagerComponent::updateFilteredList()
{
    auto query = searchBox.getText().trim().toLowerCase();
    filteredPresets.clear();

    for (const auto& item : currentPresets)
    {
        if (query.isEmpty() || item.name.toLowerCase().contains(query) || item.description.toLowerCase().contains(query))
        {
            filteredPresets.push_back(item);
        }
    }

    presetListBox.updateContent();
    presetListBox.repaint();

    if (!filteredPresets.empty())
    {
        presetListBox.selectRow(0);
        descriptionLabel.setText(filteredPresets[0].description, juce::dontSendNotification);
    }
    else
    {
        descriptionLabel.setText("No presets match query.", juce::dontSendNotification);
    }
}

int PresetManagerComponent::getNumRows()
{
    return (int)filteredPresets.size();
}

void PresetManagerComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredPresets.size()) return;
    const auto& item = filteredPresets[(size_t)rowNumber];

    if (rowIsSelected)
    {
        g.setColour(juce::Colour(0xff0095c8));
        g.fillRoundedRectangle(2.0f, 2.0f, (float)width - 4.0f, (float)height - 4.0f, 4.0f);
    }
    else if (rowNumber % 2 == 0)
    {
        g.setColour(juce::Colour(0xff1d2027));
        g.fillRoundedRectangle(2.0f, 2.0f, (float)width - 4.0f, (float)height - 4.0f, 4.0f);
    }

    // Badge
    juce::Rectangle<float> badgeRect(10.0f, (float)(height - 18) * 0.5f, 64.0f, 18.0f);
    if (item.isFactory)
    {
        g.setColour(juce::Colour(0xff2b4c7e));
        g.fillRoundedRectangle(badgeRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("FACTORY", badgeRect, juce::Justification::centred, false);
    }
    else
    {
        g.setColour(juce::Colour(0xff1f593b));
        g.fillRoundedRectangle(badgeRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("USER", badgeRect, juce::Justification::centred, false);
    }

    // Preset Name
    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffe1e6ed));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(item.name, 84, 0, width - 260, height, juce::Justification::centredLeft, true);

    // Short summary text
    g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.8f) : juce::Colour(0xff8a94a6));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText(item.description, width - 240, 0, 230, height, juce::Justification::centredRight, true);
}

void PresetManagerComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < (int)filteredPresets.size())
    {
        descriptionLabel.setText(filteredPresets[(size_t)lastRowSelected].description, juce::dontSendNotification);
    }
}

void PresetManagerComponent::resized()
{
    auto b = getLocalBounds().reduced(16);

    int topY = b.getY();
    categoryCombo.setBounds(b.getX(), topY, 220, 28);
    targetCombo.setBounds(categoryCombo.getRight() + 10, topY, 220, 28);
    searchBox.setBounds(targetCombo.getRight() + 10, topY, b.getRight() - (targetCombo.getRight() + 10), 28);

    int actionColW = 140;
    int listW = b.getWidth() - actionColW - 16;
    int listH = b.getHeight() - 110;

    presetListBox.setBounds(b.getX(), topY + 38, listW, listH);

    // Action buttons column
    int btnX = presetListBox.getRight() + 16;
    int btnY = topY + 38;
    int btnH = 28;
    int spacing = 6;

    loadBtn.setBounds(btnX, btnY, actionColW, 34); btnY += 40;
    saveBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
    saveAsBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
    duplicateBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
    renameBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
    deleteBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing + 6;
    importBtn.setBounds(btnX, btnY, actionColW, btnH); btnY += btnH + spacing;
    exportBtn.setBounds(btnX, btnY, actionColW, btnH);

    // Bottom description and status
    descriptionLabel.setBounds(b.getX(), presetListBox.getBottom() + 6, listW, 20);
    statusLabel.setBounds(b.getX(), descriptionLabel.getBottom() + 2, b.getWidth(), 20);
}

void PresetManagerComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff181a1f));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xff2d323b));
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);

    // Notice banner
    g.setColour(juce::Colour(0xff121418));
    g.fillRect(16, getHeight() - 28, getWidth() - 32, 20);
    g.setColour(juce::Colour(0xff6e7687));
    g.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    g.drawText("Scope: Presets apply strictly to the selected processor. Routing matrix, device settings, and other channels are preserved.",
               24, getHeight() - 28, getWidth() - 48, 20, juce::Justification::centredLeft, true);
}

} // namespace LiveBus
