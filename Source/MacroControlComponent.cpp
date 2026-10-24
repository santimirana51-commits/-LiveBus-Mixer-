#include "MacroControlComponent.h"

namespace LiveBus
{

int MacroControlComponent::AssignmentListModel::getNumRows()
{
    auto m = owner.engine.getMacro(owner.selectedMacroIndex);
    return m ? (int)m->assignments.size() : 0;
}

void MacroControlComponent::AssignmentListModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto m = owner.engine.getMacro(owner.selectedMacroIndex);
    if (!m || rowNumber < 0 || rowNumber >= (int)m->assignments.size()) return;

    if (rowIsSelected)
    {
        g.setColour(juce::Colour(0xff1d2b3a));
        g.fillRoundedRectangle(2.0f, 1.0f, (float)width - 4.0f, (float)height - 2.0f, 4.0f);
        g.setColour(juce::Colour(0xff00f5d4));
        g.drawRoundedRectangle(2.0f, 1.0f, (float)width - 4.0f, (float)height - 2.0f, 4.0f, 1.0f);
    }

    const auto& a = m->assignments[(size_t)rowNumber];
    juce::String txt = a.targetParameterId + "  [" + juce::String(a.minVal, 1) + " .. " + juce::String(a.maxVal, 1) + "]";
    if (a.invert) txt += " (Inverted)";

    g.setFont(juce::FontOptions(12.0f, rowIsSelected ? juce::Font::bold : juce::Font::plain));
    g.setColour(rowIsSelected ? juce::Colour(0xff00f5d4) : juce::Colour(0xffe2e8f0));
    g.drawText(txt, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
}

void MacroControlComponent::AssignmentListModel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    owner.selectedAssignmentIndex = row;
    owner.assignmentListBox.repaint();
}

MacroControlComponent::MacroControlComponent(MacroEngine& engineRef)
    : engine(engineRef)
{
    engine.addListener(this);

    // Header
    headerTitle.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour(0xfff0f6fc));
    addAndMakeVisible(headerTitle);

    // Quick Actions Bar
    addAndMakeVisible(quickActionsGroup);

    qaEmergencyMuteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
    qaEmergencyMuteBtn.addListener(this);
    addAndMakeVisible(qaEmergencyMuteBtn);

    qaStreamMuteBtn.addListener(this);
    addAndMakeVisible(qaStreamMuteBtn);

    qaMonitorMuteBtn.addListener(this);
    addAndMakeVisible(qaMonitorMuteBtn);

    qaDuckingToggleBtn.addListener(this);
    addAndMakeVisible(qaDuckingToggleBtn);

    qaResetPeakBtn.addListener(this);
    addAndMakeVisible(qaResetPeakBtn);

    qaPlaybackToggleBtn.addListener(this);
    addAndMakeVisible(qaPlaybackToggleBtn);

    // Build Macro Cards
    int count = engine.getNumMacros();
    for (int i = 0; i < count; ++i)
    {
        auto card = std::make_unique<MacroCardUI>();
        auto m = engine.getMacro(i);

        card->nameLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        card->nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
        card->nameLabel.setText(m ? m->name : "Macro", juce::dontSendNotification);
        addAndMakeVisible(card->nameLabel);

        card->descLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        card->descLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        card->descLabel.setText(m ? m->description : "", juce::dontSendNotification);
        addAndMakeVisible(card->descLabel);

        card->valueSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        card->valueSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
        card->valueSlider.setRange(0.0, 1.0, 0.01);
        card->valueSlider.setValue(m ? m->currentValue : 0.0, juce::dontSendNotification);
        card->valueSlider.addListener(this);
        addAndMakeVisible(card->valueSlider);

        int aCount = m ? (int)m->assignments.size() : 0;
        card->assignmentCountLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        card->assignmentCountLabel.setText("Targets: " + juce::String(aCount), juce::dontSendNotification);
        addAndMakeVisible(card->assignmentCountLabel);

        card->editBtn.addListener(this);
        addAndMakeVisible(card->editBtn);

        card->resetBtn.addListener(this);
        addAndMakeVisible(card->resetBtn);

        cardUIs.push_back(std::move(card));
    }

    // Inspector Group
    addAndMakeVisible(inspectorGroup);

    inspectorTitleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    inspectorTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(inspectorTitleLabel);

    listModel = std::make_unique<AssignmentListModel>(*this);
    assignmentListBox.setModel(listModel.get());
    assignmentListBox.setRowHeight(24);
    assignmentListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff161b22));
    addAndMakeVisible(assignmentListBox);

    addAndMakeVisible(targetParamLabel);

    // Populate registered parameters
    const auto& allParams = engine.getAllParameters();
    int pId = 1;
    for (const auto& p : allParams)
    {
        targetParamCombo.addItem("[" + p.group + "] " + p.displayName, pId++);
    }
    if (allParams.empty())
    {
        targetParamCombo.addItem("ch.0.gain (Channel 1 Gain)", 1);
        targetParamCombo.addItem("ducking.depth (Ducking Depth)", 2);
        targetParamCombo.addItem("safety.streamCeiling (Stream Ceiling)", 3);
    }
    targetParamCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(targetParamCombo);

    addAndMakeVisible(minValLabel);
    minValSlider.setRange(-60.0, 60.0, 0.5);
    minValSlider.setValue(0.0, juce::dontSendNotification);
    minValSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    minValSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible(minValSlider);

    addAndMakeVisible(maxValLabel);
    maxValSlider.setRange(-60.0, 60.0, 0.5);
    maxValSlider.setValue(1.0, juce::dontSendNotification);
    maxValSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    maxValSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible(maxValSlider);

    addAndMakeVisible(curveLabel);
    curveCombo.addItem("Linear", 1);
    curveCombo.addItem("Logarithmic", 2);
    curveCombo.addItem("Exponential", 3);
    curveCombo.addItem("Inverted", 4);
    curveCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(curveCombo);

    addAndMakeVisible(invertToggle);

    addAssignmentBtn.addListener(this);
    addAndMakeVisible(addAssignmentBtn);

    removeAssignmentBtn.addListener(this);
    addAndMakeVisible(removeAssignmentBtn);

    selectMacroForEditing(0);
}

MacroControlComponent::~MacroControlComponent()
{
    engine.removeListener(this);
}

void MacroControlComponent::selectMacroForEditing(int macroIndex)
{
    selectedMacroIndex = macroIndex;
    auto m = engine.getMacro(macroIndex);
    if (m)
    {
        inspectorTitleLabel.setText("Editing: " + m->name + " (ID " + juce::String(m->id) + ")", juce::dontSendNotification);
    }
    selectedAssignmentIndex = -1;
    assignmentListBox.updateContent();
    assignmentListBox.repaint();
}

void MacroControlComponent::refreshAssignmentList()
{
    assignmentListBox.updateContent();
    assignmentListBox.repaint();
    for (size_t i = 0; i < cardUIs.size(); ++i)
    {
        auto m = engine.getMacro((int)i);
        if (m)
        {
            cardUIs[i]->assignmentCountLabel.setText("Targets: " + juce::String((int)m->assignments.size()), juce::dontSendNotification);
        }
    }
}

void MacroControlComponent::buttonClicked(juce::Button* b)
{
    if (b == &qaEmergencyMuteBtn)
        engine.executeQuickAction(QuickActionType::EmergencyMuteAll);
    else if (b == &qaStreamMuteBtn)
        engine.executeQuickAction(QuickActionType::ToggleStreamMute);
    else if (b == &qaMonitorMuteBtn)
        engine.executeQuickAction(QuickActionType::ToggleMonitorMute);
    else if (b == &qaDuckingToggleBtn)
        engine.executeQuickAction(QuickActionType::ToggleDucking);
    else if (b == &qaResetPeakBtn)
        engine.executeQuickAction(QuickActionType::ResetPeakWarnings);
    else if (b == &qaPlaybackToggleBtn)
        engine.executeQuickAction(QuickActionType::TogglePlayback);
    else if (b == &addAssignmentBtn)
    {
        MacroAssignment a;
        auto text = targetParamCombo.getText();
        // Extract parameter key
        int closeBracket = text.indexOf("]");
        if (closeBracket > 0)
            a.targetParameterId = text.substring(closeBracket + 1).trim();
        else
            a.targetParameterId = text;

        a.minVal = (float)minValSlider.getValue();
        a.maxVal = (float)maxValSlider.getValue();
        a.curve = (MacroCurveType)(curveCombo.getSelectedId() - 1);
        a.invert = invertToggle.getToggleState();
        a.enabled = true;

        engine.addAssignment(selectedMacroIndex, a);
        refreshAssignmentList();
    }
    else if (b == &removeAssignmentBtn)
    {
        if (selectedAssignmentIndex >= 0)
        {
            engine.removeAssignment(selectedMacroIndex, selectedAssignmentIndex);
            selectedAssignmentIndex = -1;
            refreshAssignmentList();
        }
    }
    else
    {
        for (size_t i = 0; i < cardUIs.size(); ++i)
        {
            if (b == &cardUIs[i]->editBtn)
            {
                selectMacroForEditing((int)i);
                break;
            }
            else if (b == &cardUIs[i]->resetBtn)
            {
                engine.resetMacroToDefault((int)i);
                break;
            }
        }
    }
}

void MacroControlComponent::sliderValueChanged(juce::Slider* s)
{
    if (isInternalUpdate) return;

    for (size_t i = 0; i < cardUIs.size(); ++i)
    {
        if (s == &cardUIs[i]->valueSlider)
        {
            engine.setMacroValue((int)i, (float)s->getValue(), true);
            break;
        }
    }
}

void MacroControlComponent::comboBoxChanged(juce::ComboBox*)
{
}

void MacroControlComponent::onMacroValueChanged(int macroIndex, float newValue)
{
    if (macroIndex >= 0 && macroIndex < (int)cardUIs.size())
    {
        isInternalUpdate = true;
        cardUIs[(size_t)macroIndex]->valueSlider.setValue(newValue, juce::dontSendNotification);
        isInternalUpdate = false;
    }
}

void MacroControlComponent::onMacroStructureChanged()
{
    refreshAssignmentList();
}

void MacroControlComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));
}

void MacroControlComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Top Header & Quick Actions
    auto topArea = area.removeFromTop(75);
    headerTitle.setBounds(topArea.removeFromTop(24));

    quickActionsGroup.setBounds(topArea);
    auto qaInner = topArea.reduced(8, 16);
    int qaBtnW = (qaInner.getWidth() - 30) / 6;
    qaEmergencyMuteBtn.setBounds(qaInner.removeFromLeft(qaBtnW));
    qaInner.removeFromLeft(6);
    qaStreamMuteBtn.setBounds(qaInner.removeFromLeft(qaBtnW));
    qaInner.removeFromLeft(6);
    qaMonitorMuteBtn.setBounds(qaInner.removeFromLeft(qaBtnW));
    qaInner.removeFromLeft(6);
    qaDuckingToggleBtn.setBounds(qaInner.removeFromLeft(qaBtnW));
    qaInner.removeFromLeft(6);
    qaResetPeakBtn.setBounds(qaInner.removeFromLeft(qaBtnW));
    qaInner.removeFromLeft(6);
    qaPlaybackToggleBtn.setBounds(qaInner);

    area.removeFromTop(12);

    // Main split: Left 60% Macro Cards Grid, Right 40% Assignment Inspector
    int leftW = (int)(area.getWidth() * 0.58f);
    auto leftArea = area.removeFromLeft(leftW);
    area.removeFromLeft(14);
    auto rightArea = area;

    // Macro Cards Grid (2 rows x 2 columns)
    int numCards = (int)cardUIs.size();
    if (numCards > 0)
    {
        int cardW = (leftArea.getWidth() - 10) / 2;
        int cardH = (leftArea.getHeight() - 10) / 2;

        for (int i = 0; i < numCards; ++i)
        {
            int row = i / 2;
            int col = i % 2;
            auto cardBounds = juce::Rectangle<int>(leftArea.getX() + col * (cardW + 10),
                                                   leftArea.getY() + row * (cardH + 10),
                                                   cardW, cardH);

            auto inner = cardBounds.reduced(8);
            cardUIs[(size_t)i]->nameLabel.setBounds(inner.removeFromTop(20));
            cardUIs[(size_t)i]->descLabel.setBounds(inner.removeFromTop(32));

            auto btm = inner.removeFromBottom(26);
            cardUIs[(size_t)i]->editBtn.setBounds(btm.removeFromLeft(60));
            cardUIs[(size_t)i]->resetBtn.setBounds(btm.removeFromLeft(60).translated(6, 0));
            cardUIs[(size_t)i]->assignmentCountLabel.setBounds(btm.translated(12, 0));

            cardUIs[(size_t)i]->valueSlider.setBounds(inner.reduced(4));
        }
    }

    // Inspector
    inspectorGroup.setBounds(rightArea);
    auto inspInner = rightArea.reduced(14, 24);
    inspectorTitleLabel.setBounds(inspInner.removeFromTop(24));
    inspInner.removeFromTop(6);
    assignmentListBox.setBounds(inspInner.removeFromTop(120));
    inspInner.removeFromTop(8);

    targetParamLabel.setBounds(inspInner.removeFromTop(18));
    targetParamCombo.setBounds(inspInner.removeFromTop(26));
    inspInner.removeFromTop(6);

    auto rowMinMax = inspInner.removeFromTop(26);
    minValLabel.setBounds(rowMinMax.removeFromLeft(65));
    minValSlider.setBounds(rowMinMax.removeFromLeft(95));
    rowMinMax.removeFromLeft(8);
    maxValLabel.setBounds(rowMinMax.removeFromLeft(65));
    maxValSlider.setBounds(rowMinMax);

    inspInner.removeFromTop(6);
    auto rowCurve = inspInner.removeFromTop(26);
    curveLabel.setBounds(rowCurve.removeFromLeft(50));
    curveCombo.setBounds(rowCurve.removeFromLeft(120));
    rowCurve.removeFromLeft(10);
    invertToggle.setBounds(rowCurve);

    inspInner.removeFromTop(10);
    auto btnRow = inspInner.removeFromTop(30);
    addAssignmentBtn.setBounds(btnRow.removeFromLeft(130));
    btnRow.removeFromLeft(8);
    removeAssignmentBtn.setBounds(btnRow.removeFromLeft(130));
}

} // namespace LiveBus
