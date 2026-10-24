#include "PluginManageComponent.h"

namespace LiveBus
{

PluginManageComponent::PluginManageComponent(VST3HostEngine& engine, std::function<void()> onClose)
    : hostEngine(engine), onCloseRequested(std::move(onClose))
{
    setOpaque(true);

    headerTitleLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    headerTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(headerTitleLabel);

    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn.onClick = [this]() {
        if (onCloseRequested)
            onCloseRequested();
    };
    addAndMakeVisible(closeBtn);

    // Paths Group
    pathsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff3b82f6));
    pathsGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff60a5fa));
    addAndMakeVisible(pathsGroup);

    pathsListBox.setModel(this);
    pathsListBox.setRowHeight(24);
    pathsListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff111827));
    pathsListBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff374151));
    addAndMakeVisible(pathsListBox);

    addPathBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    addPathBtn.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>("Select VST3 Plug-in Folder",
                                                          juce::File::getSpecialLocation(juce::File::userHomeDirectory));
        if (fileChooser->browseForDirectory())
        {
            auto dir = fileChooser->getResult();
            if (dir.isDirectory())
            {
                auto settings = hostEngine.getScanSettings();
                settings.customScanPaths.addIfNotAlreadyThere(dir.getFullPathName());
                hostEngine.setScanSettings(settings);
                refreshPathList();
            }
        }
    };
    addAndMakeVisible(addPathBtn);

    removePathBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
    removePathBtn.onClick = [this]() {
        int selected = pathsListBox.getSelectedRow();
        if (selected >= 0 && selected < currentPaths.size())
        {
            auto settings = hostEngine.getScanSettings();
            settings.customScanPaths.removeString(currentPaths[selected]);
            hostEngine.setScanSettings(settings);
            refreshPathList();
        }
    };
    addAndMakeVisible(removePathBtn);

    auto settings = hostEngine.getScanSettings();
    scanStandardToggle.setToggleState(settings.scanStandardVst3, juce::dontSendNotification);
    scanStandardToggle.onClick = [this]() {
        auto s = hostEngine.getScanSettings();
        s.scanStandardVst3 = scanStandardToggle.getToggleState();
        hostEngine.setScanSettings(s);
    };
    addAndMakeVisible(scanStandardToggle);

    // Maintenance Group
    maintenanceGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xfff59e0b));
    maintenanceGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xfffbbf24));
    addAndMakeVisible(maintenanceGroup);

    fullRescanBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd97706));
    fullRescanBtn.onClick = [this]() {
        hostEngine.startScan(true);
        if (onCloseRequested)
            onCloseRequested();
    };
    addAndMakeVisible(fullRescanBtn);

    clearCacheBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
    clearCacheBtn.onClick = [this]() {
        hostEngine.clearLibraryCache();
        refreshPathList();
    };
    addAndMakeVisible(clearCacheBtn);

    statusSummaryLabel.setText("Total Library Plug-ins: " + juce::String((int)hostEngine.getLibraryPlugins().size()),
                               juce::dontSendNotification);
    statusSummaryLabel.setFont(juce::FontOptions(12.0f));
    statusSummaryLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(statusSummaryLabel);

    refreshPathList();
}

void PluginManageComponent::refreshPathList()
{
    currentPaths = hostEngine.getScanSettings().customScanPaths;
    pathsListBox.updateContent();
    pathsListBox.repaint();
    statusSummaryLabel.setText("Total Library Plug-ins: " + juce::String((int)hostEngine.getLibraryPlugins().size()),
                               juce::dontSendNotification);
}

void PluginManageComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));

    // Outer border
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(getLocalBounds(), 1);

    // Header bar
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(0, 0, getWidth(), 40);
    g.setColour(juce::Colour(0xff30363d));
    g.drawLine(0.0f, 40.0f, (float)getWidth(), 40.0f, 1.0f);
}

void PluginManageComponent::resized()
{
    headerTitleLabel.setBounds(16, 8, getWidth() - 70, 24);
    closeBtn.setBounds(getWidth() - 36, 8, 24, 24);

    int contentY = 50;
    int groupW = getWidth() - 32;

    pathsGroup.setBounds(16, contentY, groupW, 220);
    pathsListBox.setBounds(28, contentY + 28, groupW - 24, 110);
    addPathBtn.setBounds(28, contentY + 146, 130, 28);
    removePathBtn.setBounds(168, contentY + 146, 130, 28);
    scanStandardToggle.setBounds(28, contentY + 180, groupW - 24, 26);

    int maintY = contentY + 234;
    maintenanceGroup.setBounds(16, maintY, groupW, 110);
    fullRescanBtn.setBounds(28, maintY + 28, 170, 32);
    clearCacheBtn.setBounds(210, maintY + 28, 150, 32);
    statusSummaryLabel.setBounds(28, maintY + 68, groupW - 24, 24);
}

int PluginManageComponent::getNumRows()
{
    return currentPaths.size();
}

void PluginManageComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= currentPaths.size())
        return;

    if (rowIsSelected)
    {
        g.fillAll(juce::Colour(0x333b82f6));
        g.setColour(juce::Colour(0xff3b82f6));
        g.drawRect(0, 0, width, height, 1);
    }

    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colour(0xfff0f6fc));
    g.drawText(currentPaths[rowNumber], 8, 0, width - 16, height, juce::Justification::centredLeft, true);
}

} // namespace LiveBus
