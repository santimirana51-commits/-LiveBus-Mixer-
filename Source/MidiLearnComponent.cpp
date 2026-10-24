#include "MidiLearnComponent.h"

namespace LiveBus
{

// ==============================================================================
// MidiMappingRowComponent Implementation
// ==============================================================================
MidiMappingRowComponent::MidiMappingRowComponent(MidiManager& manager,
                                                 MidiParameterRegistry& registry,
                                                 int mappingIndex,
                                                 bool showAdvanced)
    : midiManager(manager),
      paramRegistry(registry),
      index(mappingIndex),
      advancedMode(showAdvanced)
{
    // 1. Type ComboBox
    typeCombo.addItem("CC", 1);
    typeCombo.addItem("Note", 2);
    typeCombo.onChange = [this]() {
        if (index >= 0 && index < midiManager.getNumMappings())
        {
            auto& m = midiManager.getMapping(index);
            m.messageType = (typeCombo.getSelectedId() == 2) ? MidiMessageType::Note : MidiMessageType::CC;
            updateFromMapping();
        }
    };
    addAndMakeVisible(typeCombo);

    // 2. Channel ComboBox
    channelCombo.addItem("All", 1);
    for (int ch = 1; ch <= 16; ++ch)
        channelCombo.addItem(juce::String(ch), ch + 1);
    channelCombo.onChange = [this]() {
        if (index >= 0 && index < midiManager.getNumMappings())
        {
            auto& m = midiManager.getMapping(index);
            m.midiChannel = channelCombo.getSelectedId() - 1;
        }
    };
    addAndMakeVisible(channelCombo);

    // 3. Part / Scope Label
    partScopeLabel.setFont(juce::FontOptions(12.0f));
    partScopeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    partScopeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff161e2e));
    partScopeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(partScopeLabel);

    // 4. Controller Number Label
    controllerNumberLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    controllerNumberLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    controllerNumberLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff161e2e));
    controllerNumberLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(controllerNumberLabel);

    // 5. Mode ComboBox
    modeCombo.addItem("Absolute", 1);
    modeCombo.addItem("Relative Signed", 2);
    modeCombo.addItem("Relative 2's Compl", 3);
    modeCombo.addItem("Relative Bin Offset", 4);
    modeCombo.onChange = [this]() {
        if (index >= 0 && index < midiManager.getNumMappings())
        {
            auto& m = midiManager.getMapping(index);
            m.mode = (MidiMode)(modeCombo.getSelectedId() - 1);
        }
    };
    addAndMakeVisible(modeCombo);

    // 6. Parameter ComboBox
    const auto& allParamIds = paramRegistry.getAllParameterIds();
    for (int p = 0; p < (int)allParamIds.size(); ++p)
    {
        const auto* desc = paramRegistry.getDescriptor(allParamIds[(size_t)p]);
        juce::String displayName = desc ? (desc->scope + " — " + desc->name) : allParamIds[(size_t)p];
        parameterCombo.addItem(displayName, p + 1);
    }
    parameterCombo.onChange = [this, allParamIds]() {
        int sel = parameterCombo.getSelectedId() - 1;
        if (sel >= 0 && sel < (int)allParamIds.size() && index >= 0 && index < midiManager.getNumMappings())
        {
            auto& m = midiManager.getMapping(index);
            m.targetParameterId = allParamIds[(size_t)sel];
            const auto* desc = paramRegistry.getDescriptor(m.targetParameterId);
            if (desc != nullptr)
            {
                m.targetScope = desc->scope;
                m.targetParamName = desc->name;
                m.minValue = desc->minValue;
                m.maxValue = desc->maxValue;
                m.defaultValue = desc->defaultValue;
                m.skewFactor = desc->skewFactor;
            }
            updateFromMapping();
        }
    };
    addAndMakeVisible(parameterCombo);

    // 7. Learn Button
    learnBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    learnBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    learnBtn.onClick = [this]() {
        if (onLearnClicked) onLearnClicked(index);
    };
    addAndMakeVisible(learnBtn);

    // 8. Remove Button
    removeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    removeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffef4444));
    removeBtn.onClick = [this]() {
        if (onRemoveClicked) onRemoveClicked(index);
    };
    addAndMakeVisible(removeBtn);

    // 9. Advanced Toggles
    invertToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    invertToggle.onClick = [this]() {
        if (index >= 0 && index < midiManager.getNumMappings())
            midiManager.getMapping(index).invert = invertToggle.getToggleState();
    };
    addChildComponent(invertToggle);

    pickupToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    pickupToggle.onClick = [this]() {
        if (index >= 0 && index < midiManager.getNumMappings())
            midiManager.getMapping(index).pickup = pickupToggle.getToggleState();
    };
    addChildComponent(pickupToggle);

    updateFromMapping();
}

void MidiMappingRowComponent::setShowAdvanced(bool show)
{
    advancedMode = show;
    invertToggle.setVisible(show);
    pickupToggle.setVisible(show);
    resized();
    repaint();
}

void MidiMappingRowComponent::updateFromMapping()
{
    if (index < 0 || index >= midiManager.getNumMappings()) return;
    const auto& m = midiManager.getMapping(index);

    typeCombo.setSelectedId((m.messageType == MidiMessageType::Note) ? 2 : 1, juce::dontSendNotification);
    channelCombo.setSelectedId(m.midiChannel + 1, juce::dontSendNotification);

    partScopeLabel.setText(m.targetScope.isNotEmpty() ? m.targetScope : "Auto", juce::dontSendNotification);
    controllerNumberLabel.setText(juce::String(m.controllerOrNoteNumber), juce::dontSendNotification);

    modeCombo.setSelectedId((int)m.mode + 1, juce::dontSendNotification);

    // Find parameter index
    const auto& allParamIds = paramRegistry.getAllParameterIds();
    for (int p = 0; p < (int)allParamIds.size(); ++p)
    {
        if (allParamIds[(size_t)p] == m.targetParameterId)
        {
            parameterCombo.setSelectedId(p + 1, juce::dontSendNotification);
            break;
        }
    }

    invertToggle.setToggleState(m.invert, juce::dontSendNotification);
    pickupToggle.setToggleState(m.pickup, juce::dontSendNotification);

    if (midiManager.isLearning() && midiManager.getLearningMappingId() == m.id)
    {
        learnBtn.setButtonText("LEARNING...");
        learnBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c)); // Pulsing Orange
        learnBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        learnBtn.setButtonText("Learn");
        learnBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        learnBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    }
}

void MidiMappingRowComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour((index % 2 == 0) ? 0x08ffffff : 0x03ffffff));
    g.fillRoundedRectangle(b.reduced(1.0f), 4.0f);

    if (midiManager.isLearning() && index < midiManager.getNumMappings() &&
        midiManager.getLearningMappingId() == midiManager.getMapping(index).id)
    {
        g.setColour(juce::Colour(0xfff59e0b)); // Amber glowing border
        g.drawRoundedRectangle(b.reduced(1.0f), 4.0f, 1.5f);
    }
}

void MidiMappingRowComponent::resized()
{
    auto b = getLocalBounds().reduced(2);
    int y = b.getY() + 3;
    int h = 26;

    if (!advancedMode)
    {
        typeCombo.setBounds(b.getX() + 4, y, 64, h);
        channelCombo.setBounds(b.getX() + 74, y, 68, h);
        partScopeLabel.setBounds(b.getX() + 148, y, 80, h);
        controllerNumberLabel.setBounds(b.getX() + 234, y, 54, h);
        modeCombo.setBounds(b.getX() + 294, y, 126, h);
        parameterCombo.setBounds(b.getX() + 426, y, b.getWidth() - 584, h);

        learnBtn.setBounds(b.getRight() - 150, y, 68, h);
        removeBtn.setBounds(b.getRight() - 76, y, 70, h);
    }
    else
    {
        typeCombo.setBounds(b.getX() + 4, y, 54, h);
        channelCombo.setBounds(b.getX() + 62, y, 56, h);
        partScopeLabel.setBounds(b.getX() + 122, y, 70, h);
        controllerNumberLabel.setBounds(b.getX() + 196, y, 48, h);
        modeCombo.setBounds(b.getX() + 248, y, 110, h);

        invertToggle.setBounds(b.getX() + 362, y, 58, h);
        pickupToggle.setBounds(b.getX() + 424, y, 62, h);

        parameterCombo.setBounds(b.getX() + 490, y, b.getWidth() - 646, h);
        learnBtn.setBounds(b.getRight() - 150, y, 68, h);
        removeBtn.setBounds(b.getRight() - 76, y, 70, h);
    }
}

// ==============================================================================
// MidiLearnComponent (Full Console View)
// ==============================================================================
MidiLearnComponent::MidiLearnComponent(MidiManager& manager, MidiParameterRegistry& registry)
    : midiManager(manager),
      parameterRegistry(registry)
{
    midiManager.addChangeListener(this);

    // 1. Top Header Title & Close
    titleLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    closeBtn.onClick = [this]() {
        if (onCloseRequested) onCloseRequested();
    };
    addAndMakeVisible(closeBtn);

    // 2. Preset Toolbar
    presetLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(presetLabel);

    presetCombo.onChange = [this]() {
        auto selName = presetCombo.getText();
        if (selName.isNotEmpty() && selName != midiManager.getCurrentPresetName())
        {
            midiManager.loadPreset(selName);
            refreshMappingTable();
        }
    };
    addAndMakeVisible(presetCombo);

    createPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    createPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff1f5f9));
    createPresetBtn.onClick = [this]() {
        juce::String newName = "User Preset " + juce::String(juce::Random::getSystemRandom().nextInt({10, 99}));
        midiManager.savePreset(newName);
        updatePresetList();
    };
    addAndMakeVisible(createPresetBtn);

    savePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    savePresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff1f5f9));
    savePresetBtn.onClick = [this]() {
        midiManager.savePreset(midiManager.getCurrentPresetName());
        updatePresetList();
    };
    addAndMakeVisible(savePresetBtn);

    deletePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    deletePresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffef4444));
    deletePresetBtn.onClick = [this]() {
        auto cur = midiManager.getCurrentPresetName();
        if (!midiManager.isFactoryPreset(cur))
        {
            midiManager.deletePreset(cur);
            midiManager.loadDefaultFactoryPreset();
            updatePresetList();
            refreshMappingTable();
        }
    };
    addAndMakeVisible(deletePresetBtn);

    browsePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    browsePresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff1f5f9));
    browsePresetBtn.onClick = [this]() {
        // Browse/Import XML
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::FileChooser fc("Import MIDI Preset", docs, "*.xml");
        if (fc.browseForFileToOpen())
        {
            midiManager.importPreset(fc.getResult());
            updatePresetList();
            refreshMappingTable();
        }
    };
    addAndMakeVisible(browsePresetBtn);

    exportPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    exportPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff1f5f9));
    exportPresetBtn.onClick = [this]() {
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::FileChooser fc("Export MIDI Preset", docs.getChildFile(midiManager.getCurrentPresetName() + ".xml"), "*.xml");
        if (fc.browseForFileToSave(true))
        {
            midiManager.exportPreset(fc.getResult());
        }
    };
    addAndMakeVisible(exportPresetBtn);

    // 3. Left Navigation Buttons (matching screenshot)
    settingsNavLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    settingsNavLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    settingsNavLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(settingsNavLabel);

    const char* tabNames[] = { "Skin", "GUI", "MIDI", "MIDI Learn", "DSP & Audio", "DSP Bridge" };
    for (int i = 0; i < 6; ++i)
    {
        subTabButtons[(size_t)i].setButtonText(tabNames[i]);
        subTabButtons[(size_t)i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161e2e));
        subTabButtons[(size_t)i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        subTabButtons[(size_t)i].onClick = [this, i]() {
            selectSubTab((SubTab)i);
        };
        addAndMakeVisible(subTabButtons[(size_t)i]);
    }

    enableAdvancedToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    enableAdvancedToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));
    enableAdvancedToggle.onClick = [this]() {
        advancedOptionsEnabled = enableAdvancedToggle.getToggleState();
        refreshMappingTable();
    };
    addAndMakeVisible(enableAdvancedToggle);

    // 4. Mappings Center Area
    mappingsHeader.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    mappingsHeader.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    mappingsHeader.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mappingsHeader);

    auto setupHeaderLabel = [this](juce::Label& lbl) {
        lbl.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        lbl.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);
    };

    setupHeaderLabel(colType);
    setupHeaderLabel(colChannel);
    setupHeaderLabel(colPart);
    setupHeaderLabel(colController);
    setupHeaderLabel(colMode);
    setupHeaderLabel(colParameter);
    setupHeaderLabel(colActions);

    tableContent = std::make_unique<juce::Component>();
    tableViewport.setViewedComponent(tableContent.get(), false);
    tableViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(tableViewport);

    addMappingBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    addMappingBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addMappingBtn.onClick = [this]() {
        MidiMappingEntry newMap;
        newMap.controllerOrNoteNumber = 1;
        newMap.targetParameterId = "ch.1.gain";
        newMap.targetScope = "Input 1";
        newMap.targetParamName = "Ch 1 Gain";
        newMap.minValue = -60.0f;
        newMap.maxValue = 12.0f;
        midiManager.addMapping(newMap);
        refreshMappingTable();
    };
    addAndMakeVisible(addMappingBtn);

    clearAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    clearAllBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffef4444));
    clearAllBtn.onClick = [this]() {
        midiManager.clearAllMappings();
        refreshMappingTable();
    };
    addAndMakeVisible(clearAllBtn);

    // 5. Bottom Sections (MIDI Input & Feedback)
    midiInputHeader.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    midiInputHeader.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(midiInputHeader);

    learnFromLabel.setFont(juce::FontOptions(13.0f));
    learnFromLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(learnFromLabel);

    learnHostToggle.setToggleState(midiManager.isLearnFromHost(), juce::dontSendNotification);
    learnHostToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    learnHostToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));
    learnHostToggle.onClick = [this]() { midiManager.setLearnFromHost(learnHostToggle.getToggleState()); };
    addAndMakeVisible(learnHostToggle);

    learnPhysicalToggle.setToggleState(midiManager.isLearnFromPhysical(), juce::dontSendNotification);
    learnPhysicalToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    learnPhysicalToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));
    learnPhysicalToggle.onClick = [this]() { midiManager.setLearnFromPhysical(learnPhysicalToggle.getToggleState()); };
    addAndMakeVisible(learnPhysicalToggle);

    midiFeedbackHeader.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    midiFeedbackHeader.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(midiFeedbackHeader);

    sendFeedbackLabel.setFont(juce::FontOptions(13.0f));
    sendFeedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(sendFeedbackLabel);

    feedbackHostToggle.setToggleState(midiManager.isSendFeedbackToHost(), juce::dontSendNotification);
    feedbackHostToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    feedbackHostToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));
    feedbackHostToggle.onClick = [this]() { midiManager.setSendFeedbackToHost(feedbackHostToggle.getToggleState()); };
    addAndMakeVisible(feedbackHostToggle);

    feedbackPhysicalToggle.setToggleState(midiManager.isSendFeedbackToPhysical(), juce::dontSendNotification);
    feedbackPhysicalToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    feedbackPhysicalToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00f5d4));
    feedbackPhysicalToggle.onClick = [this]() { midiManager.setSendFeedbackToPhysical(feedbackPhysicalToggle.getToggleState()); };
    addAndMakeVisible(feedbackPhysicalToggle);

    // 6. SubTab 2 (Hardware Devices & Monitor)
    midiDevicesHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    midiDevicesHeader.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addChildComponent(midiDevicesHeader);

    midiDeviceSelector.onChange = [this]() {
        auto dev = midiDeviceSelector.getText();
        if (dev.isNotEmpty())
            midiManager.openInputDevice(dev);
    };
    addChildComponent(midiDeviceSelector);

    refreshDevicesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    refreshDevicesBtn.onClick = [this]() {
        midiDeviceSelector.clear(juce::dontSendNotification);
        midiDeviceSelector.addItem("None", 1);
        auto devs = midiManager.getAvailableInputDevices();
        for (int i = 0; i < devs.size(); ++i)
            midiDeviceSelector.addItem(devs[i], i + 2);

        auto cur = midiManager.getCurrentDeviceName();
        midiDeviceSelector.setText(cur, juce::dontSendNotification);
    };
    addChildComponent(refreshDevicesBtn);

    midiActivityHeader.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    midiActivityHeader.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addChildComponent(midiActivityHeader);

    midiActivityLog.setMultiLine(true);
    midiActivityLog.setReadOnly(true);
    midiActivityLog.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    midiActivityLog.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0a0e17));
    midiActivityLog.setColour(juce::TextEditor::textColourId, juce::Colour(0xff38bdf8));
    addChildComponent(midiActivityLog);

    clearActivityBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    clearActivityBtn.onClick = [this]() {
        midiActivityLog.clear();
    };
    addChildComponent(clearActivityBtn);

    // 7. SubTab 3 (DSP & Audio)
    dspMappingHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    dspMappingHeader.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addChildComponent(dspMappingHeader);

    dspHelpEditor.setMultiLine(true);
    dspHelpEditor.setReadOnly(true);
    dspHelpEditor.setFont(juce::FontOptions(12.0f));
    dspHelpEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121824));
    dspHelpEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xff94a3b8));
    dspHelpEditor.setText("Quick DSP & Mixer Mapping Templates:\n\n"
                          "Use the one-click templates below to quickly map your hardware MIDI controller "
                          "to LiveBus Mixer channels, master buses, and dynamic DSP processors.\n"
                          "All mappings can be customized further in the MIDI Learn tab.");
    addChildComponent(dspHelpEditor);

    mapAllFadersBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    mapAllFadersBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00f5d4));
    mapAllFadersBtn.onClick = [this]() {
        midiManager.loadPreset("Default Mixer Faders");
        selectSubTab(SubTab::MIDILearn);
    };
    addChildComponent(mapAllFadersBtn);

    mapNanoKontrolBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    mapNanoKontrolBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    mapNanoKontrolBtn.onClick = [this]() {
        midiManager.loadPreset("Korg nanoKONTROL2");
        selectSubTab(SubTab::MIDILearn);
    };
    addChildComponent(mapNanoKontrolBtn);

    mapRelativeRotaryBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    mapRelativeRotaryBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffeab308));
    mapRelativeRotaryBtn.onClick = [this]() {
        midiManager.loadPreset("Rotary Encoders Relative");
        selectSubTab(SubTab::MIDILearn);
    };
    addChildComponent(mapRelativeRotaryBtn);

    // 8. SubTab 4 (DSP Bridge)
    dspBridgeHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    dspBridgeHeader.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addChildComponent(dspBridgeHeader);

    dspBridgeInfo.setMultiLine(true);
    dspBridgeInfo.setReadOnly(true);
    dspBridgeInfo.setFont(juce::FontOptions(12.0f));
    dspBridgeInfo.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121824));
    dspBridgeInfo.setColour(juce::TextEditor::textColourId, juce::Colour(0xff94a3b8));
    dspBridgeInfo.setText("LiveBus VST3 Plugin Host MIDI Bridge:\n\n"
                          "- Incoming MIDI events are automatically buffered into real-time circular FIFOs.\n"
                          "- Out-of-process VST3 plugins running inside LiveBusPluginHost.exe receive "
                          "sample-accurate MIDI events via Windows Named Shared Memory.\n"
                          "- Hardware controllers can simultaneously drive LiveBus DSP channel strips "
                          "and hosted VST3 synthesizers or guitar effect processors.");
    addChildComponent(dspBridgeInfo);

    // 9. SubTabs 0 & 1 (Skin & GUI)
    skinHeader.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    skinHeader.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    skinHeader.setText("Console Theme: Charcoal Broadcast Dark (#0d1117 / #12161f)\n"
                       "Accents: Cyan (#00f5d4), Blue (#3b82f6), Amber (#f59e0b), Coral (#ef4444)", juce::dontSendNotification);
    addChildComponent(skinHeader);

    guiHeader.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    guiHeader.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    guiHeader.setText("Resolution: Native vector rendering | 60 FPS Smooth Meters | Anti-aliasing active", juce::dontSendNotification);
    addChildComponent(guiHeader);

    // 10. Learn Active Banner
    learnBanner.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    learnBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0xeeea580c)); // Glowing Orange
    learnBanner.setColour(juce::Label::textColourId, juce::Colours::white);
    learnBanner.setJustificationType(juce::Justification::centred);
    addChildComponent(learnBanner);

    cancelLearnBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    cancelLearnBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    cancelLearnBtn.onClick = [this]() {
        midiManager.cancelLearn();
        refreshMappingTable();
    };
    addChildComponent(cancelLearnBtn);

    // 11. Warning Banner
    warningBanner.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    warningBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0xeeeab308)); // Amber
    warningBanner.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
    warningBanner.setJustificationType(juce::Justification::centred);
    addChildComponent(warningBanner);

    // Callbacks from manager
    midiManager.onLearnCompleted = [this](const MidiMappingEntry&) {
        refreshMappingTable();
    };
    midiManager.onDuplicateWarning = [this](const juce::String& msg) {
        warningBanner.setText(msg, juce::dontSendNotification);
        warningBanner.setVisible(true);
        warningDismissTimer = 100; // ~4 seconds
        resized();
    };
    midiManager.onMappingsChanged = [this]() {
        refreshMappingTable();
    };

    updatePresetList();
    selectSubTab(SubTab::MIDILearn);
    refreshMappingTable();
    startTimer(40); // 25 Hz update loop
}

MidiLearnComponent::~MidiLearnComponent()
{
    stopTimer();
    midiManager.removeChangeListener(this);
}

void MidiLearnComponent::selectSubTab(SubTab tab)
{
    currentSubTab = tab;

    for (int i = 0; i < 6; ++i)
    {
        bool isActive = (i == (int)currentSubTab);
        subTabButtons[(size_t)i].setColour(juce::TextButton::buttonColourId,
            isActive ? juce::Colour(0xff0284c7) : juce::Colour(0xff161e2e));
        subTabButtons[(size_t)i].setColour(juce::TextButton::textColourOffId,
            isActive ? juce::Colours::white : juce::Colour(0xff94a3b8));
    }

    updateSubTabVisibility();
    resized();
}

void MidiLearnComponent::updateSubTabVisibility()
{
    bool isLearn = (currentSubTab == SubTab::MIDILearn);
    mappingsHeader.setVisible(isLearn);
    colType.setVisible(isLearn);
    colChannel.setVisible(isLearn);
    colPart.setVisible(isLearn);
    colController.setVisible(isLearn);
    colMode.setVisible(isLearn);
    colParameter.setVisible(isLearn);
    colActions.setVisible(isLearn);
    tableViewport.setVisible(isLearn);
    addMappingBtn.setVisible(isLearn);
    clearAllBtn.setVisible(isLearn);
    midiInputHeader.setVisible(isLearn);
    learnFromLabel.setVisible(isLearn);
    learnHostToggle.setVisible(isLearn);
    learnPhysicalToggle.setVisible(isLearn);
    midiFeedbackHeader.setVisible(isLearn);
    sendFeedbackLabel.setVisible(isLearn);
    feedbackHostToggle.setVisible(isLearn);
    feedbackPhysicalToggle.setVisible(isLearn);

    bool isMidiDev = (currentSubTab == SubTab::MIDI);
    midiDevicesHeader.setVisible(isMidiDev);
    midiDeviceSelector.setVisible(isMidiDev);
    refreshDevicesBtn.setVisible(isMidiDev);
    midiActivityHeader.setVisible(isMidiDev);
    midiActivityLog.setVisible(isMidiDev);
    clearActivityBtn.setVisible(isMidiDev);

    bool isDSP = (currentSubTab == SubTab::DSPAudio);
    dspMappingHeader.setVisible(isDSP);
    dspHelpEditor.setVisible(isDSP);
    mapAllFadersBtn.setVisible(isDSP);
    mapNanoKontrolBtn.setVisible(isDSP);
    mapRelativeRotaryBtn.setVisible(isDSP);

    bool isBridge = (currentSubTab == SubTab::DSPBridge);
    dspBridgeHeader.setVisible(isBridge);
    dspBridgeInfo.setVisible(isBridge);

    bool isSkin = (currentSubTab == SubTab::Skin);
    skinHeader.setVisible(isSkin);

    bool isGUI = (currentSubTab == SubTab::GUI);
    guiHeader.setVisible(isGUI);
}

void MidiLearnComponent::updatePresetList()
{
    presetCombo.clear(juce::dontSendNotification);
    auto presets = midiManager.getAvailablePresets();
    for (int i = 0; i < presets.size(); ++i)
        presetCombo.addItem(presets[i], i + 1);

    presetCombo.setText(midiManager.getCurrentPresetName(), juce::dontSendNotification);
}

void MidiLearnComponent::refreshMappingTable()
{
    rowComponents.clear();
    int count = midiManager.getNumMappings();

    for (int i = 0; i < count; ++i)
    {
        auto row = std::make_unique<MidiMappingRowComponent>(midiManager, parameterRegistry, i, advancedOptionsEnabled);
        row->onRemoveClicked = [this](int idx) {
            midiManager.removeMapping(idx);
            refreshMappingTable();
        };
        row->onLearnClicked = [this](int idx) {
            if (idx >= 0 && idx < midiManager.getNumMappings())
            {
                const auto& m = midiManager.getMapping(idx);
                midiManager.startLearn(m.id);
                refreshMappingTable();
            }
        };
        tableContent->addAndMakeVisible(row.get());
        rowComponents.push_back(std::move(row));
    }

    layoutMappingsTable();
}

void MidiLearnComponent::layoutMappingsTable()
{
    int rowH = 32;
    int totalH = (int)rowComponents.size() * rowH;
    int tableW = tableViewport.getWidth() - 16;
    if (tableW < 600) tableW = 600;

    tableContent->setBounds(0, 0, tableW, juce::jmax(totalH, tableViewport.getHeight()));

    for (int i = 0; i < (int)rowComponents.size(); ++i)
    {
        rowComponents[(size_t)i]->setBounds(0, i * rowH, tableW, rowH);
        rowComponents[(size_t)i]->updateFromMapping();
    }
}

void MidiLearnComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updatePresetList();
    refreshMappingTable();
    repaint();
}

void MidiLearnComponent::timerCallback()
{
    // 1. Process real-time queued MIDI messages and dispatch
    midiManager.processQueuedEvents();

    // 2. Manage Learn banner countdown
    if (midiManager.isLearning())
    {
        midiManager.tickLearnTimer(40);
        int remSec = (midiManager.getLearnTimeoutRemainingMs() + 999) / 1000;
        learnBanner.setText("WAITING FOR MIDI... Move any fader/knob or press a button on your controller. (" +
                            juce::String(remSec) + "s remaining)", juce::dontSendNotification);
        learnBanner.setVisible(true);
        cancelLearnBtn.setVisible(true);
    }
    else
    {
        learnBanner.setVisible(false);
        cancelLearnBtn.setVisible(false);
    }

    // 3. Manage warning dismiss
    if (warningDismissTimer > 0)
    {
        if (--warningDismissTimer <= 0)
            warningBanner.setVisible(false);
    }

    // 4. Update activity log on MIDI sub-tab
    if (currentSubTab == SubTab::MIDI && midiManager.hasRecentActivity())
    {
        midiManager.clearActivity();
        auto history = midiManager.getRecentEvents();
        juce::String text;
        for (auto it = history.rbegin(); it != history.rend(); ++it)
        {
            text += it->timestamp.formatted("[%H:%M:%S] ") + it->description + "\n";
        }
        midiActivityLog.setText(text);
    }
}

void MidiLearnComponent::paint(juce::Graphics& g)
{
    // Charcoal dark background
    g.fillAll(juce::Colour(0xff0d1117));

    // Outer Main Card Panel
    g.setColour(juce::Colour(0xff161b24));
    g.fillRoundedRectangle(10.0f, 10.0f, (float)getWidth() - 20.0f, (float)getHeight() - 20.0f, 8.0f);
    g.setColour(juce::Colour(0xff2a3447));
    g.drawRoundedRectangle(10.0f, 10.0f, (float)getWidth() - 20.0f, (float)getHeight() - 20.0f, 8.0f, 1.0f);

    // Left Navigation Sidebar Background
    g.setColour(juce::Colour(0xff111620));
    g.fillRoundedRectangle(18.0f, 54.0f, 180.0f, (float)getHeight() - 72.0f, 6.0f);
    g.setColour(juce::Colour(0xff222c3d));
    g.drawRoundedRectangle(18.0f, 54.0f, 180.0f, (float)getHeight() - 72.0f, 6.0f, 1.0f);

    // Top Header Divider Line
    g.setColour(juce::Colour(0xff243044));
    g.drawHorizontalLine(48, 18.0f, (float)getWidth() - 18.0f);
}

void MidiLearnComponent::resized()
{
    // Top Bar
    closeBtn.setBounds(24, 16, 32, 24);
    titleLabel.setBounds(getWidth() / 2 - 120, 12, 240, 28);

    // Left Navigation Sidebar
    settingsNavLabel.setBounds(22, 64, 172, 24);
    int navY = 96;
    for (int i = 0; i < 6; ++i)
    {
        subTabButtons[(size_t)i].setBounds(28, navY, 160, 32);
        navY += 38;
    }
    enableAdvancedToggle.setBounds(24, getHeight() - 52, 172, 24);

    // Center Top: Preset Toolbar
    int contentX = 210;
    int contentW = getWidth() - contentX - 24;

    presetLabel.setBounds(contentX + 10, 60, 54, 26);
    presetCombo.setBounds(contentX + 68, 60, 220, 26);
    createPresetBtn.setBounds(contentX + 296, 60, 64, 26);
    savePresetBtn.setBounds(contentX + 366, 60, 56, 26);
    deletePresetBtn.setBounds(contentX + 428, 60, 64, 26);
    browsePresetBtn.setBounds(contentX + 498, 60, 78, 26);
    exportPresetBtn.setBounds(contentX + 582, 60, 78, 26);

    // SubTab 1: MIDI Learn
    if (currentSubTab == SubTab::MIDILearn)
    {
        mappingsHeader.setBounds(contentX, 94, contentW, 26);

        // Column headers
        int colY = 124;
        if (!advancedOptionsEnabled)
        {
            colType.setBounds(contentX + 8, colY, 64, 18);
            colChannel.setBounds(contentX + 78, colY, 68, 18);
            colPart.setBounds(contentX + 152, colY, 80, 18);
            colController.setBounds(contentX + 238, colY, 54, 18);
            colMode.setBounds(contentX + 298, colY, 126, 18);
            colParameter.setBounds(contentX + 430, colY, contentW - 584, 18);
            colActions.setBounds(contentX + contentW - 146, colY, 140, 18);
        }
        else
        {
            colType.setBounds(contentX + 8, colY, 54, 18);
            colChannel.setBounds(contentX + 66, colY, 56, 18);
            colPart.setBounds(contentX + 126, colY, 70, 18);
            colController.setBounds(contentX + 200, colY, 48, 18);
            colMode.setBounds(contentX + 252, colY, 110, 18);
            colParameter.setBounds(contentX + 494, colY, contentW - 646, 18);
            colActions.setBounds(contentX + contentW - 146, colY, 140, 18);
        }

        int tableH = getHeight() - 320;
        tableViewport.setBounds(contentX + 4, colY + 22, contentW - 8, tableH);
        layoutMappingsTable();

        addMappingBtn.setBounds(contentX + 10, colY + 26 + tableH, 120, 28);
        clearAllBtn.setBounds(contentX + 138, colY + 26 + tableH, 90, 28);

        // Bottom Sections: MIDI Input and MIDI Feedback
        int botSectionY = getHeight() - 130;
        midiInputHeader.setBounds(contentX + 10, botSectionY, 200, 22);
        learnFromLabel.setBounds(contentX + 10, botSectionY + 24, 90, 24);
        learnHostToggle.setBounds(contentX + 104, botSectionY + 24, 68, 24);
        learnPhysicalToggle.setBounds(contentX + 178, botSectionY + 24, 88, 24);

        int fbSectionX = contentX + 320;
        midiFeedbackHeader.setBounds(fbSectionX, botSectionY, 200, 22);
        sendFeedbackLabel.setBounds(fbSectionX, botSectionY + 24, 130, 24);
        feedbackHostToggle.setBounds(fbSectionX + 136, botSectionY + 24, 68, 24);
        feedbackPhysicalToggle.setBounds(fbSectionX + 210, botSectionY + 24, 88, 24);
    }
    else if (currentSubTab == SubTab::MIDI)
    {
        midiDevicesHeader.setBounds(contentX + 10, 100, 300, 24);
        midiDeviceSelector.setBounds(contentX + 10, 130, 340, 28);
        refreshDevicesBtn.setBounds(contentX + 360, 130, 130, 28);

        midiActivityHeader.setBounds(contentX + 10, 174, 300, 24);
        midiActivityLog.setBounds(contentX + 10, 202, contentW - 20, getHeight() - 250);
        clearActivityBtn.setBounds(contentX + contentW - 110, 172, 90, 24);
    }
    else if (currentSubTab == SubTab::DSPAudio)
    {
        dspMappingHeader.setBounds(contentX + 10, 100, 340, 24);
        dspHelpEditor.setBounds(contentX + 10, 130, contentW - 20, 100);
        mapAllFadersBtn.setBounds(contentX + 10, 244, 240, 34);
        mapNanoKontrolBtn.setBounds(contentX + 10, 288, 240, 34);
        mapRelativeRotaryBtn.setBounds(contentX + 10, 332, 240, 34);
    }
    else if (currentSubTab == SubTab::DSPBridge)
    {
        dspBridgeHeader.setBounds(contentX + 10, 100, 340, 24);
        dspBridgeInfo.setBounds(contentX + 10, 130, contentW - 20, 200);
    }
    else if (currentSubTab == SubTab::Skin)
    {
        skinHeader.setBounds(contentX + 10, 120, contentW - 20, 120);
    }
    else if (currentSubTab == SubTab::GUI)
    {
        guiHeader.setBounds(contentX + 10, 120, contentW - 20, 120);
    }

    // Overlay Banners
    learnBanner.setBounds(contentX + 20, 12, contentW - 140, 32);
    cancelLearnBtn.setBounds(contentX + contentW - 110, 14, 100, 28);

    warningBanner.setBounds(contentX + 40, getHeight() - 44, contentW - 80, 28);
}

} // namespace LiveBus
