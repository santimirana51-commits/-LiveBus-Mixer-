#include "EffectsConsoleComponent.h"
#include "RNNoiseVST3Plugin.h"

namespace LiveBus
{

EffectsConsoleComponent::EffectsConsoleComponent(VST3HostEngine& engine, PluginHostClient& isolatedClient)
    : hostEngine(engine), outOfProcessClient(isolatedClient)
{
    setOpaque(true);

    hostEngine.addChangeListener(this);

    titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(titleLabel);

    targetCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1f2937));
    targetCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    for (int i = 0; i < VST3HostEngine::NumTargets; ++i)
        targetCombo.addItem(VST3HostEngine::getTargetName(i), i + 1);
    targetCombo.onChange = [this]() {
        const int targetIndex = targetCombo.getSelectedId() - 1;
        if (targetIndex < 0 || targetIndex >= VST3HostEngine::NumTargets)
            return;
        selectTarget(targetIndex);
        if (onTargetSelected)
            onTargetSelected(targetIndex);
    };
    addAndMakeVisible(targetCombo);

    // Mode Toggle
    isolatedModeToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff60a5fa));
    isolatedModeToggle.setToggleState(false, juce::dontSendNotification);
    isolatedModeToggle.onClick = [this]() {
        if (libraryComponent)
            libraryComponent->setIsolatedModeEnabled(isolatedModeToggle.getToggleState());
    };
    addAndMakeVisible(isolatedModeToggle);

    // Top Controls
    openLibraryBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7)); // Cyan
    openLibraryBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openLibraryBtn.onClick = [this]() { showPluginLibrary(true); };
    addAndMakeVisible(openLibraryBtn);

    insertPluginBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb)); // Blue
    insertPluginBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    insertPluginBtn.onClick = [this]() { showPluginLibrary(true); };
    addAndMakeVisible(insertPluginBtn);

    bypassAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
    bypassAllBtn.onClick = [this]() {
        int count = hostEngine.getNumPluginsInTarget(currentTargetIndex);
        if (count > 0)
        {
            bool anyEnabled = false;
            for (int s = 0; s < count; ++s)
            {
                if (!hostEngine.isSlotBypassed(currentTargetIndex, s))
                {
                    anyEnabled = true;
                    break;
                }
            }
            for (int s = 0; s < count; ++s)
                hostEngine.setSlotBypassed(currentTargetIndex, s, anyEnabled);

            rebuildSlotViews();
        }
    };
    addAndMakeVisible(bypassAllBtn);

    clearChainBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
    clearChainBtn.onClick = [this]() {
        hostEngine.clearTargetChain(currentTargetIndex);
        rebuildSlotViews();
    };
    addAndMakeVisible(clearChainBtn);

    testWatchdogBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd97706)); // Amber
    testWatchdogBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    testWatchdogBtn.onClick = [this]() {
        if (!hostEngine.simulateTargetIsolatedCrash(currentTargetIndex))
            outOfProcessClient.simulateCrash();
    };
    addAndMakeVisible(testWatchdogBtn);

    // Status Telemetry
    statusTelemetryLabel.setFont(juce::FontOptions(12.0f));
    statusTelemetryLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ca3af));
    addAndMakeVisible(statusTelemetryLabel);

    latencyTelemetryLabel.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    latencyTelemetryLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(latencyTelemetryLabel);

    // Chain Viewport
    chainViewport.setViewedComponent(&chainHolder, false);
    chainViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(chainViewport);

    // Plug-in Library Overlay
    libraryComponent = std::make_unique<PluginLibraryComponent>(hostEngine);
    libraryComponent->setIsolatedModeEnabled(isolatedModeToggle.getToggleState());
    libraryComponent->onCloseRequested = [this]() { showPluginLibrary(false); };
    libraryComponent->onPluginSelectedToLoad = [this](const juce::PluginDescription& /*desc*/) {
        rebuildSlotViews();
    };
    addChildComponent(libraryComponent.get());

    selectTarget(maxInputs); // Default to Stream Bus Master
    startTimerHz(15);
}

EffectsConsoleComponent::~EffectsConsoleComponent()
{
    stopTimer();
    hostEngine.removeChangeListener(this);
}

void EffectsConsoleComponent::selectTarget(int targetIndex)
{
    if (targetIndex < 0 || targetIndex >= VST3HostEngine::NumTargets)
        return;

    currentTargetIndex = targetIndex;
    if (targetCombo.getSelectedId() != targetIndex + 1)
        targetCombo.setSelectedId(targetIndex + 1, juce::dontSendNotification);

    if (libraryComponent)
        libraryComponent->setActiveTarget(currentTargetIndex);

    rebuildSlotViews();
}

void EffectsConsoleComponent::showPluginLibrary(bool show)
{
    showingLibrary = show;
    if (libraryComponent)
    {
        libraryComponent->setVisible(show);
        if (show)
        {
            libraryComponent->setActiveTarget(currentTargetIndex);
            libraryComponent->setIsolatedModeEnabled(isolatedModeToggle.getToggleState());
            libraryComponent->toFront(true);
        }
    }
    resized();
    repaint();
}

void EffectsConsoleComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    rebuildSlotViews();
}

void EffectsConsoleComponent::timerCallback()
{
    hostEngine.checkIsolatedClientsHealthAndRecover();

    // Update Telemetry
    juce::String stateStr;
    auto state = hostEngine.getTargetIsolatedState(currentTargetIndex);
    switch (state)
    {
        case PluginHostClient::PluginState::HelperCrashed:
            stateStr = "Helper Crashed — Watchdog recovering";
            break;
        case PluginHostClient::PluginState::HelperRestarting:
            stateStr = "Helper Restarting...";
            break;
        case PluginHostClient::PluginState::ActiveProcessing:
            stateStr = "Active (Isolated Worker Ready)";
            break;
        default:
            stateStr = isolatedModeToggle.getToggleState() ? "Out-of-Process Mode Enabled" : "In-Process Hosting Active (0ms IPC)";
            break;
    }

    int totalLat = hostEngine.getTargetTotalLatency(currentTargetIndex);
    const double sampleRate = hostEngine.getCurrentSampleRate();
    const double latMs = sampleRate > 0.0 ? (double)totalLat * 1000.0 / sampleRate : 0.0;
    latencyTelemetryLabel.setText("Chain Latency: " + juce::String(totalLat) + " smp (" + juce::String(latMs, 1) + " ms)",
                                  juce::dontSendNotification);

    statusTelemetryLabel.setText("Host Status: " + stateStr + " | Inserts in Chain: " + juce::String(hostEngine.getNumPluginsInTarget(currentTargetIndex)),
                                 juce::dontSendNotification);
}

void EffectsConsoleComponent::rebuildSlotViews()
{
    slotRows.clear();
    chainHolder.removeAllChildren();

    int numSlots = hostEngine.getNumPluginsInTarget(currentTargetIndex);
    int rowH = 46;

    for (int i = 0; i < numSlots; ++i)
    {
        auto row = std::make_unique<SlotRowComponent>(*this, i);
        chainHolder.addAndMakeVisible(row.get());
        row->setBounds(0, i * rowH, chainHolder.getWidth(), rowH - 4);
        slotRows.push_back(std::move(row));
    }

    chainHolder.setSize(chainViewport.getWidth() - 16, juce::jmax(200, numSlots * rowH + 20));
    chainViewport.repaint();
}

void EffectsConsoleComponent::openEditorForSlot(int slotIndex)
{
    auto slot = hostEngine.getSlotInfo(currentTargetIndex, slotIndex);
    if (auto* inst = hostEngine.getPluginInstance(currentTargetIndex, slotIndex))
    {
        PluginEditorWindow::openWindowFor(*inst, slot.pluginName + " — " + VST3HostEngine::getTargetName(currentTargetIndex));
        return;
    }

    // If running in isolated mode or bundled plugin mode, open the rich dedicated editor
    if (slot.pluginName.containsIgnoreCase("RNNoise") || slot.fileOrIdentifier.containsIgnoreCase("RNNoise"))
    {
        if (!internalRNNoisePlugin)
        {
            internalRNNoisePlugin = std::make_unique<RNNoiseVST3Plugin>();
            internalRNNoisePlugin->prepareToPlay(hostEngine.getCurrentSampleRate(), 512);
        }
        PluginEditorWindow::openWindowFor(*internalRNNoisePlugin, slot.pluginName + " (Isolated Editor) — " + VST3HostEngine::getTargetName(currentTargetIndex));
        return;
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                           slot.pluginName,
                                           "Plug-in instance is active in chain (" + slot.fileOrIdentifier + ").\n"
                                           "Isolated worker process is running.");
}

void EffectsConsoleComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));

    // Top control banner
    g.setColour(juce::Colour(0xff161b22));
    g.fillRect(10, 10, getWidth() - 20, 96);
    g.setColour(juce::Colour(0xff30363d));
    g.drawRect(10, 10, getWidth() - 20, 96, 1);

    // Empty state message if no slots loaded
    if (hostEngine.getNumPluginsInTarget(currentTargetIndex) == 0 && !showingLibrary)
    {
        g.setColour(juce::Colour(0xff6b7280));
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("No plug-in inserts loaded in " + VST3HostEngine::getTargetName(currentTargetIndex) + ".\n"
                   "Click [+ Insert Plug-in...] or [Open Plug-in Library] to add VST3 plug-ins.",
                   chainViewport.getBounds(), juce::Justification::centred);
    }
}

void EffectsConsoleComponent::resized()
{
    if (showingLibrary && libraryComponent != nullptr)
    {
        libraryComponent->setBounds(10, 10, getWidth() - 20, getHeight() - 20);
        return;
    }

    titleLabel.setBounds(20, 16, 400, 24);
    openLibraryBtn.setBounds(getWidth() - 210, 16, 190, 28);

    targetCombo.setBounds(20, 46, 300, 26);

    // Action Row (Row 3)
    insertPluginBtn.setBounds(20, 76, 150, 26);
    bypassAllBtn.setBounds(176, 76, 140, 26);
    clearChainBtn.setBounds(322, 76, 110, 26);
    isolatedModeToggle.setBounds(440, 76, 280, 26);
    testWatchdogBtn.setBounds(getWidth() - 210, 76, 190, 26);

    // Status bar (Row 4)
    statusTelemetryLabel.setBounds(20, 110, getWidth() - 320, 22);
    latencyTelemetryLabel.setBounds(getWidth() - 290, 110, 270, 22);

    // Chain Viewport (Row 5)
    int viewY = 136;
    chainViewport.setBounds(10, viewY, getWidth() - 20, getHeight() - viewY - 10);
    chainHolder.setSize(chainViewport.getWidth() - 16, chainHolder.getHeight());

    for (int i = 0; i < (int)slotRows.size(); ++i)
    {
        slotRows[i]->setBounds(0, i * 46, chainHolder.getWidth(), 42);
    }
}

// =============================================================================
// SlotRowComponent Implementation
// =============================================================================

EffectsConsoleComponent::SlotRowComponent::SlotRowComponent(EffectsConsoleComponent& owner, int slotIdx)
    : parent(owner), slotIndex(slotIdx)
{
    auto slot = parent.hostEngine.getSlotInfo(parent.currentTargetIndex, slotIndex);

    slotNumLabel.setText(juce::String(slotIndex + 1), juce::dontSendNotification);
    slotNumLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    slotNumLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(slotNumLabel);

    pluginNameLabel.setText(slot.pluginName, juce::dontSendNotification);
    pluginNameLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    pluginNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pluginNameLabel);

    latencyLabel.setText("Latency: " + juce::String(slot.latencySamples) + " smp", juce::dontSendNotification);
    latencyLabel.setFont(juce::FontOptions(11.0f));
    latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ca3af));
    addAndMakeVisible(latencyLabel);

    bypassToggle.setToggleState(slot.isBypassed, juce::dontSendNotification);
    bypassToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff59e0b)); // Amber
    bypassToggle.onClick = [this]() {
        parent.hostEngine.setSlotBypassed(parent.currentTargetIndex, slotIndex, bypassToggle.getToggleState());
        repaint();
    };
    addAndMakeVisible(bypassToggle);

    openEditorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb)); // Blue
    openEditorBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openEditorBtn.onClick = [this]() {
        parent.openEditorForSlot(slotIndex);
    };
    addAndMakeVisible(openEditorBtn);

    moveUpBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
    moveUpBtn.onClick = [this]() {
        if (slotIndex > 0)
            parent.hostEngine.movePluginInTarget(parent.currentTargetIndex, slotIndex, slotIndex - 1);
    };
    addAndMakeVisible(moveUpBtn);

    moveDownBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
    moveDownBtn.onClick = [this]() {
        int count = parent.hostEngine.getNumPluginsInTarget(parent.currentTargetIndex);
        if (slotIndex < count - 1)
            parent.hostEngine.movePluginInTarget(parent.currentTargetIndex, slotIndex, slotIndex + 1);
    };
    addAndMakeVisible(moveDownBtn);

    removeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626)); // Red
    removeBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    removeBtn.onClick = [this]() {
        parent.hostEngine.removePluginFromTarget(parent.currentTargetIndex, slotIndex);
    };
    addAndMakeVisible(removeBtn);
}

void EffectsConsoleComponent::SlotRowComponent::paint(juce::Graphics& g)
{
    bool bypassed = parent.hostEngine.isSlotBypassed(parent.currentTargetIndex, slotIndex);

    g.fillAll(bypassed ? juce::Colour(0xff18181b) : juce::Colour(0xff1f293d));
    g.setColour(bypassed ? juce::Colour(0xff71717a) : juce::Colour(0xff3b82f6));
    g.drawRect(getLocalBounds(), 1);

    if (bypassed)
    {
        g.setColour(juce::Colour(0x66f59e0b));
        g.fillRect(0, 0, 4, getHeight());
    }
}

void EffectsConsoleComponent::SlotRowComponent::resized()
{
    slotNumLabel.setBounds(8, 6, 24, 28);
    pluginNameLabel.setBounds(38, 6, 240, 28);
    latencyLabel.setBounds(290, 6, 120, 28);
    bypassToggle.setBounds(420, 8, 80, 26);
    openEditorBtn.setBounds(510, 8, 90, 26);
    moveUpBtn.setBounds(getWidth() - 110, 8, 28, 26);
    moveDownBtn.setBounds(getWidth() - 78, 8, 28, 26);
    removeBtn.setBounds(getWidth() - 40, 8, 28, 26);
}

} // namespace LiveBus
