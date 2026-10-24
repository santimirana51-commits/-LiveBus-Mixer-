#include "PluginLibraryComponent.h"

namespace LiveBus
{

PluginLibraryComponent::PluginLibraryComponent(VST3HostEngine& engine)
    : hostEngine(engine), scanProgressBar(scanProgressValue)
{
    setOpaque(true);

    hostEngine.addChangeListener(this);

    // 1. Header
    headerTitleLabel.setText("PLUG-IN LIBRARY", juce::dontSendNotification);
    headerTitleLabel.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    headerTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f6fc));
    addAndMakeVisible(headerTitleLabel);

    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn.onClick = [this]() {
        if (onCloseRequested)
            onCloseRequested();
    };
    addAndMakeVisible(closeBtn);

    subTitleInstructionLabel.setText("Use the Def Column to set your default plug-in. Click the Star icon to add a plug-in to your favourites.",
                                     juce::dontSendNotification);
    subTitleInstructionLabel.setFont(juce::FontOptions(12.0f));
    subTitleInstructionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ca3af));
    addAndMakeVisible(subTitleInstructionLabel);

    // 2. Search & Filter Bar
    searchBox.setTextToShowWhenEmpty("Search plug-ins...", juce::Colour(0xff6b7280));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111827));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff374151));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.onTextChange = [this]() { rebuildFilteredList(); };
    addAndMakeVisible(searchBox);

    categoryFilterCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111827));
    categoryFilterCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff374151));
    categoryFilterCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe5e7eb));
    categoryFilterCombo.addItem("All Categories", 1);
    categoryFilterCombo.setSelectedId(1, juce::dontSendNotification);
    categoryFilterCombo.onChange = [this]() { rebuildFilteredList(); };
    addAndMakeVisible(categoryFilterCombo);

    manufacturerFilterCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff111827));
    manufacturerFilterCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff374151));
    manufacturerFilterCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe5e7eb));
    manufacturerFilterCombo.addItem("All Manufacturers", 1);
    manufacturerFilterCombo.setSelectedId(1, juce::dontSendNotification);
    manufacturerFilterCombo.onChange = [this]() { rebuildFilteredList(); };
    addAndMakeVisible(manufacturerFilterCombo);

    favoritesOnlyToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfffbbf24));
    favoritesOnlyToggle.onClick = [this]() { rebuildFilteredList(); };
    addAndMakeVisible(favoritesOnlyToggle);

    // 3. Table Setup
    table.setModel(this);
    table.setColour(juce::TableListBox::backgroundColourId, juce::Colour(0xff0d1117));
    table.setColour(juce::TableListBox::outlineColourId, juce::Colour(0xff1f2937));
    table.setRowHeight(28);
    table.setMultipleSelectionEnabled(false);

    auto& header = table.getHeader();
    header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff161b22));
    header.setColour(juce::TableHeaderComponent::textColourId, juce::Colour(0xffe5e7eb));
    header.setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour(0xff30363d));

    header.addColumn("Def", ColDefault, 44, 30, 50, juce::TableHeaderComponent::notSortable);
    header.addColumn(juce::CharPointer_UTF8("\xe2\x98\x85"), ColFavorite, 34, 25, 45, juce::TableHeaderComponent::notSortable);
    header.addColumn("Name", ColName, 260, 150, 450);
    header.addColumn("Format", ColFormat, 65, 50, 90);
    header.addColumn("Category", ColCategory, 180, 100, 300);
    header.addColumn("Manufacturer", ColManufacturer, 160, 100, 250);
    header.addColumn("Profile", ColProfile, 80, 50, 120);

    addAndMakeVisible(table);

    // 4. Bottom Buttons
    loadPluginBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
    loadPluginBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadPluginBtn.onClick = [this]() { loadSelectedPlugin(); };
    addAndMakeVisible(loadPluginBtn);

    scanPluginsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7)); // Cyan/Teal
    scanPluginsBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    scanPluginsBtn.onClick = [this]() { hostEngine.startScan(false); };
    addAndMakeVisible(scanPluginsBtn);

    manageBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
    manageBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    manageBtn.onClick = [this]() {
        manageModal = std::make_unique<PluginManageComponent>(hostEngine, [this]() {
            manageModal.reset();
            rebuildFilteredList();
            resized();
        });
        addAndMakeVisible(manageModal.get());
        resized();
    };
    addAndMakeVisible(manageBtn);

    backBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
    backBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    backBtn.onClick = [this]() {
        if (onCloseRequested)
            onCloseRequested();
    };
    addAndMakeVisible(backBtn);

    // 5. Scan Progress
    scanStatusLabel.setFont(juce::FontOptions(11.0f));
    scanStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addChildComponent(scanStatusLabel);
    addChildComponent(scanProgressBar);

    cancelScanBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
    cancelScanBtn.onClick = [this]() { hostEngine.cancelScan(); };
    addChildComponent(cancelScanBtn);

    rebuildFilteredList();
    startTimerHz(15); // Progress bar refresh
}

PluginLibraryComponent::~PluginLibraryComponent()
{
    stopTimer();
    hostEngine.removeChangeListener(this);
}

void PluginLibraryComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    rebuildFilteredList();
}

void PluginLibraryComponent::timerCallback()
{
    bool scanning = hostEngine.isScanning();
    scanProgressBar.setVisible(scanning);
    scanStatusLabel.setVisible(scanning);
    cancelScanBtn.setVisible(scanning);

    if (scanning)
    {
        scanProgressValue = (double)hostEngine.getScanProgress();
        scanStatusLabel.setText("Scanning: " + hostEngine.getScanCurrentFile(), juce::dontSendNotification);
        repaint();
    }
}

void PluginLibraryComponent::rebuildFilteredList()
{
    allPlugins = hostEngine.getLibraryPlugins();

    juce::String filterText = searchBox.getText().trim().toLowerCase();
    juce::String selectedCategory = (categoryFilterCombo.getSelectedId() > 1) ? categoryFilterCombo.getText() : "";
    juce::String selectedMfg = (manufacturerFilterCombo.getSelectedId() > 1) ? manufacturerFilterCombo.getText() : "";
    bool favOnly = favoritesOnlyToggle.getToggleState();

    filteredPlugins.clear();

    juce::StringArray uniqueCategories;
    juce::StringArray uniqueManufacturers;

    for (const auto& item : allPlugins)
    {
        uniqueCategories.addIfNotAlreadyThere(item.getCategory());
        uniqueManufacturers.addIfNotAlreadyThere(item.getManufacturer());

        if (favOnly && !item.isFavorite)
            continue;

        if (selectedCategory.isNotEmpty() && item.getCategory() != selectedCategory)
            continue;

        if (selectedMfg.isNotEmpty() && item.getManufacturer() != selectedMfg)
            continue;

        if (filterText.isNotEmpty())
        {
            if (!item.getDisplayName().toLowerCase().contains(filterText) &&
                !item.getCategory().toLowerCase().contains(filterText) &&
                !item.getManufacturer().toLowerCase().contains(filterText))
            {
                continue;
            }
        }

        filteredPlugins.push_back(item);
    }

    // Refresh Combos if empty
    if (categoryFilterCombo.getNumItems() <= 1)
    {
        uniqueCategories.sort(true);
        for (int i = 0; i < uniqueCategories.size(); ++i)
            categoryFilterCombo.addItem(uniqueCategories[i], i + 2);
    }
    if (manufacturerFilterCombo.getNumItems() <= 1)
    {
        uniqueManufacturers.sort(true);
        for (int i = 0; i < uniqueManufacturers.size(); ++i)
            manufacturerFilterCombo.addItem(uniqueManufacturers[i], i + 2);
    }

    table.updateContent();
    table.repaint();

    // Default select row 0 or default plugin if nothing selected
    if (table.getSelectedRow() < 0 && !filteredPlugins.empty())
    {
        for (int i = 0; i < (int)filteredPlugins.size(); ++i)
        {
            if (filteredPlugins[i].isDefault)
            {
                table.selectRow(i);
                break;
            }
        }
        if (table.getSelectedRow() < 0)
            table.selectRow(0);
    }
}

void PluginLibraryComponent::loadSelectedPlugin()
{
    int row = table.getSelectedRow();
    if (row >= 0 && row < (int)filteredPlugins.size())
    {
        const auto& item = filteredPlugins[row];
        hostEngine.loadPluginToTarget(activeTargetIndex, item.description, false);

        if (onPluginSelectedToLoad)
            onPluginSelectedToLoad(item.description);

        if (onCloseRequested)
            onCloseRequested();
    }
}

void PluginLibraryComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));

    // Outer border
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);

    // Top Header bar
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(0, 0, getWidth(), 42);

    // List icon glyph
    g.setColour(juce::Colour(0xff00f5d4));
    float iconX = 14.0f;
    float iconY = 14.0f;
    g.fillRect(iconX, iconY, 14.0f, 2.0f);
    g.fillRect(iconX, iconY + 5.0f, 14.0f, 2.0f);
    g.fillRect(iconX, iconY + 10.0f, 14.0f, 2.0f);

    // Bottom action bar
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(0, getHeight() - 52, getWidth(), 52);
    g.setColour(juce::Colour(0xff30363d));
    g.drawLine(0.0f, (float)(getHeight() - 52), (float)getWidth(), (float)(getHeight() - 52), 1.0f);
}

void PluginLibraryComponent::resized()
{
    if (manageModal != nullptr)
    {
        manageModal->setBounds(getLocalBounds().reduced(40, 30));
        return;
    }

    headerTitleLabel.setBounds(38, 9, 250, 24);
    closeBtn.setBounds(getWidth() - 34, 9, 24, 24);

    subTitleInstructionLabel.setBounds(16, 44, getWidth() - 32, 20);

    // Search & Filter controls
    int filterY = 68;
    searchBox.setBounds(16, filterY, 220, 26);
    categoryFilterCombo.setBounds(244, filterY, 180, 26);
    manufacturerFilterCombo.setBounds(432, filterY, 180, 26);
    favoritesOnlyToggle.setBounds(620, filterY, 130, 26);

    // Scan progress row
    int tableTop = filterY + 34;
    if (hostEngine.isScanning())
    {
        scanStatusLabel.setBounds(16, tableTop, 350, 20);
        scanProgressBar.setBounds(370, tableTop, 220, 20);
        cancelScanBtn.setBounds(600, tableTop, 90, 20);
        tableTop += 26;
    }

    // Table
    int tableBottom = getHeight() - 58;
    table.setBounds(16, tableTop, getWidth() - 32, tableBottom - tableTop);

    // Bottom action toolbar
    int btnY = getHeight() - 44;
    loadPluginBtn.setBounds(16, btnY, 130, 34);
    scanPluginsBtn.setBounds(154, btnY, 130, 34);
    manageBtn.setBounds(292, btnY, 110, 34);
    backBtn.setBounds(getWidth() - 126, btnY, 110, 34);
}

// =============================================================================
// TableListBoxModel Implementation
// =============================================================================

int PluginLibraryComponent::getNumRows()
{
    return (int)filteredPlugins.size();
}

void PluginLibraryComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        // Selected highlight: Solid dark grey background like Harrison MPC Compressor row in screenshot
        g.fillAll(juce::Colour(0xff374151));
    }
    else
    {
        // Alternating dark background
        if (rowNumber % 2 == 1)
            g.fillAll(juce::Colour(0xff111827));
        else
            g.fillAll(juce::Colour(0xff0d1117));
    }
}

void PluginLibraryComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredPlugins.size())
        return;

    const auto& item = filteredPlugins[rowNumber];

    g.setFont(juce::FontOptions(13.0f));

    switch (columnId)
    {
        case ColDefault:
        {
            // Radio/bullet icon for default plugin
            float cx = (float)width * 0.5f;
            float cy = (float)height * 0.5f;
            g.setColour(item.isDefault ? juce::Colours::white : juce::Colour(0xff4b5563));
            g.drawEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.5f);
            if (item.isDefault)
            {
                g.setColour(juce::Colour(0xff00f5d4));
                g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
            }
            break;
        }

        case ColFavorite:
        {
            // Star icon
            g.setFont(juce::FontOptions(15.0f));
            if (item.isFavorite)
            {
                g.setColour(juce::Colour(0xfffbbf24)); // Solid Gold
                g.drawText(juce::CharPointer_UTF8("\xe2\x98\x85"), 0, 0, width, height, juce::Justification::centred);
            }
            else
            {
                g.setColour(juce::Colour(0xff6b7280)); // Hollow Grey
                g.drawText(juce::CharPointer_UTF8("\xe2\x98\x86"), 0, 0, width, height, juce::Justification::centred);
            }
            break;
        }

        case ColName:
        {
            g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xfff0f6fc));
            g.setFont(rowIsSelected ? juce::FontOptions(13.0f).withStyle("Bold") : juce::FontOptions(13.0f));
            g.drawText(item.getDisplayName(), 6, 0, width - 10, height, juce::Justification::centredLeft, true);
            break;
        }

        case ColFormat:
        {
            g.setColour(juce::Colour(0xff00e5ff)); // Cyan Format badge
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText(item.getFormat(), 0, 0, width, height, juce::Justification::centred);
            break;
        }

        case ColCategory:
        {
            g.setColour(rowIsSelected ? juce::Colour(0xffe5e7eb) : juce::Colour(0xff9ca3af));
            g.drawText(item.getCategory(), 6, 0, width - 10, height, juce::Justification::centredLeft, true);
            break;
        }

        case ColManufacturer:
        {
            g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffd1d5db));
            g.drawText(item.getManufacturer(), 6, 0, width - 10, height, juce::Justification::centredLeft, true);
            break;
        }

        case ColProfile:
        {
            g.setColour(item.profile == "User" ? juce::Colour(0xff00f5d4) : juce::Colour(0xff6b7280));
            g.drawText(item.profile, 6, 0, width - 10, height, juce::Justification::centredLeft, true);
            break;
        }
    }
}

void PluginLibraryComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredPlugins.size())
        return;

    const auto& item = filteredPlugins[rowNumber];

    if (columnId == ColFavorite)
    {
        // Toggle Favorite
        hostEngine.setFavorite(item.description.fileOrIdentifier, !item.isFavorite);
        rebuildFilteredList();
    }
    else if (columnId == ColDefault)
    {
        // Set Default
        hostEngine.setDefaultPlugin(item.description.fileOrIdentifier);
        rebuildFilteredList();
    }
}

void PluginLibraryComponent::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
    if (rowNumber >= 0 && rowNumber < (int)filteredPlugins.size())
    {
        table.selectRow(rowNumber);
        loadSelectedPlugin();
    }
}

} // namespace LiveBus
