#include "CopyToolsComponent.h"

namespace LiveBus
{

CopyToolsComponent::CopyToolsComponent()
{
    // Update active scene
    activeSceneLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    activeSceneLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d2ff));
    addAndMakeVisible(activeSceneLabel);

    updateSceneBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff124d2d));
    updateSceneBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    updateSceneBtn.onClick = [this] {
        if (onUpdateActiveScene)
        {
            onUpdateActiveScene();
            statusLabel.setText("Active scene '" + activeSceneName + "' updated with current mixer state.", juce::dontSendNotification);
        }
    };
    addAndMakeVisible(updateSceneBtn);

    // 8-Band EQ
    populateTargetCombo(eqSourceCombo);
    populateTargetCombo(eqDestCombo);
    eqSourceCombo.setSelectedId(1);
    eqDestCombo.setSelectedId(2);
    addAndMakeVisible(eqSourceCombo);
    addAndMakeVisible(eqDestCombo);

    copyEQBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff25374d));
    copyEQBtn.onClick = [this] {
        int src = eqSourceCombo.getSelectedId() - 1;
        int dest = eqDestCombo.getSelectedId() - 1;
        if (src == dest)
        {
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Copy Target", "Source and destination cannot be identical.", "OK");
            return;
        }
        if (onCopyEQ)
        {
            onCopyEQ(src, dest);
            statusLabel.setText("Copied 8-Band EQ: " + eqSourceCombo.getText() + " -> " + eqDestCombo.getText(), juce::dontSendNotification);
        }
    };
    addAndMakeVisible(copyEQBtn);

    // Vocal DSP Rack
    populateTargetCombo(dspSourceCombo);
    populateTargetCombo(dspDestCombo);
    dspSourceCombo.setSelectedId(1);
    dspDestCombo.setSelectedId(2);
    addAndMakeVisible(dspSourceCombo);
    addAndMakeVisible(dspDestCombo);

    copyDSPBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff25374d));
    copyDSPBtn.onClick = [this] {
        int src = dspSourceCombo.getSelectedId() - 1;
        int dest = dspDestCombo.getSelectedId() - 1;
        if (src == dest)
        {
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Copy Target", "Source and destination cannot be identical.", "OK");
            return;
        }
        if (onCopyVocalDSP)
        {
            onCopyVocalDSP(src, dest);
            statusLabel.setText("Copied Vocal DSP Rack: " + dspSourceCombo.getText() + " -> " + dspDestCombo.getText(), juce::dontSendNotification);
        }
    };
    addAndMakeVisible(copyDSPBtn);

    // Channel Strip
    populateChannelCombo(chSourceCombo);
    populateChannelCombo(chDestCombo);
    chSourceCombo.setSelectedId(1); // Ch 1
    chDestCombo.setSelectedId(2);   // Ch 2
    addAndMakeVisible(chSourceCombo);
    addAndMakeVisible(chDestCombo);

    chIncludeSendsToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(chIncludeSendsToggle);

    copyChBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff25374d));
    copyChBtn.onClick = [this] {
        int src = chSourceCombo.getSelectedId() - 1;
        int dest = chDestCombo.getSelectedId() - 1;
        if (src == dest)
        {
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon, "Copy Target", "Source and destination cannot be identical.", "OK");
            return;
        }
        if (onCopyChannel)
        {
            onCopyChannel(src, dest, chIncludeSendsToggle.getToggleState());
            statusLabel.setText("Copied Channel Settings: " + chSourceCombo.getText() + " -> " + chDestCombo.getText(), juce::dontSendNotification);
        }
    };
    addAndMakeVisible(copyChBtn);

    // Reset Defaults
    populateTargetCombo(resetTargetCombo);
    resetTargetCombo.setSelectedId(1);
    addAndMakeVisible(resetTargetCombo);

    resetDefaultsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff521a1f));
    resetDefaultsBtn.onClick = [this] {
        int target = resetTargetCombo.getSelectedId() - 1;
        if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Reset Processor",
            "Reset all DSP parameters for '" + resetTargetCombo.getText() + "' to clean defaults?"))
        {
            if (onResetTargetDefaults)
            {
                onResetTargetDefaults(target);
                statusLabel.setText("Reset " + resetTargetCombo.getText() + " parameters to clean defaults.", juce::dontSendNotification);
            }
        }
    };
    addAndMakeVisible(resetDefaultsBtn);

    // Status label
    statusLabel.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d2ff));
    addAndMakeVisible(statusLabel);

    setActiveSceneName("Default");
}

void CopyToolsComponent::setActiveSceneName(const juce::String& name)
{
    activeSceneName = name.isNotEmpty() ? name : "Default";
    activeSceneLabel.setText("Active Scene: " + activeSceneName, juce::dontSendNotification);
}

void CopyToolsComponent::populateTargetCombo(juce::ComboBox& box)
{
    box.clear();
    for (int channel = 0; channel < maxInputs; ++channel)
    {
        const auto name = channel == 0 ? "Input 1 - Mic"
            : channel == 1 ? "Input 2 - Guitar"
            : channel == 2 ? "Input 3 - Music L"
            : channel == 3 ? "Input 4 - Music R"
            : "Input " + juce::String(channel + 1);
        box.addItem(name, channel + 1);
    }
    box.addItem("Stream Bus (Master)", maxInputs + 1);
    box.addItem("Monitor Bus", maxInputs + 2);
}

void CopyToolsComponent::populateChannelCombo(juce::ComboBox& box)
{
    box.clear();
    for (int channel = 0; channel < maxInputs; ++channel)
    {
        const auto name = channel == 0 ? "Input 1 - Mic"
            : channel == 1 ? "Input 2 - Guitar"
            : channel == 2 ? "Input 3 - Music L"
            : channel == 3 ? "Input 4 - Music R"
            : "Input " + juce::String(channel + 1);
        box.addItem(name, channel + 1);
    }
}

void CopyToolsComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff181a1f));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xff2d323b));
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);

    // Panel cards
    auto drawCard = [&](const juce::Rectangle<float>& r, const juce::String& title) {
        g.setColour(juce::Colour(0xff14161a));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(juce::Colour(0xff282c35));
        g.drawRoundedRectangle(r, 6.0f, 1.0f);

        g.setColour(juce::Colour(0xff00d2ff));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(title, r.getX() + 16, r.getY() + 8, r.getWidth() - 32, 20, juce::Justification::centredLeft, false);
    };

    int cardW = (getWidth() - 48) / 2;
    int cardH = 135;

    drawCard(juce::Rectangle<float>(16, 60, (float)cardW, (float)cardH), "Copy 8-Band Parametric EQ");
    drawCard(juce::Rectangle<float>((float)(32 + cardW), 60, (float)cardW, (float)cardH), "Copy Professional Vocal DSP Rack");
    drawCard(juce::Rectangle<float>(16, 210, (float)cardW, (float)cardH), "Copy Channel Strip Settings");
    drawCard(juce::Rectangle<float>((float)(32 + cardW), 210, (float)cardW, (float)cardH), "Reset Target Processor Defaults");
}

void CopyToolsComponent::resized()
{
    int topY = 16;
    activeSceneLabel.setBounds(20, topY, 320, 28);
    updateSceneBtn.setBounds(getWidth() - 250, topY, 230, 30);

    int cardW = (getWidth() - 48) / 2;
    int col1X = 32;
    int col2X = 48 + cardW;

    // Card 1: Copy EQ
    int y1 = 98;
    eqSourceCombo.setBounds(col1X, y1, 160, 26);
    eqDestCombo.setBounds(col1X + 170, y1, 160, 26);
    copyEQBtn.setBounds(col1X + 340, y1, 140, 26);

    // Card 2: Copy DSP
    int y2 = 98;
    dspSourceCombo.setBounds(col2X, y2, 160, 26);
    dspDestCombo.setBounds(col2X + 170, y2, 160, 26);
    copyDSPBtn.setBounds(col2X + 340, y2, 150, 26);

    // Card 3: Copy Channel
    int y3 = 248;
    chSourceCombo.setBounds(col1X, y3, 140, 26);
    chDestCombo.setBounds(col1X + 150, y3, 140, 26);
    chIncludeSendsToggle.setBounds(col1X, y3 + 34, 180, 24);
    copyChBtn.setBounds(col1X + 300, y3, 180, 26);

    // Card 4: Reset Target Defaults
    int y4 = 248;
    resetTargetCombo.setBounds(col2X, y4, 200, 26);
    resetDefaultsBtn.setBounds(col2X + 210, y4, 200, 26);

    // Status bar
    statusLabel.setBounds(24, getHeight() - 34, getWidth() - 48, 24);
}

} // namespace LiveBus
