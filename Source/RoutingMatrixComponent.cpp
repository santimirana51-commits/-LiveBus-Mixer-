#include "RoutingMatrixComponent.h"
#include "RoutingGraphRouting.h"
#include <algorithm>
#include <cmath>

namespace LiveBus
{

namespace
{
class RoutingFlowDiagram final : public juce::Component, private juce::Timer
{
public:
    RoutingFlowDiagram(RoutingMatrix& routingMatrix, std::function<bool(int)> busMutedState)
        : matrix(routingMatrix), getBusMuted(std::move(busMutedState))
    {
        startTimerHz(15);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));
        const auto bounds = getLocalBounds().toFloat().reduced(16.0f, 10.0f);
        const float streamX = bounds.getX() + bounds.getWidth() * 0.66f;
        const float monitorX = bounds.getX() + bounds.getWidth() * 0.84f;
        const float nodeWidth = juce::jmin(250.0f, bounds.getWidth() * 0.30f);
        const float rowStart = bounds.getY() + 72.0f;
        const float rowHeight = 27.0f;
        const int activeChannels = matrix.getNumActiveChannels();
        const float rowEnd = rowStart + juce::jmax(0, activeChannels - 1) * rowHeight + 12.0f;
        bool anySolo = false;
        for (int channelIndex = 0; channelIndex < activeChannels; ++channelIndex)
            anySolo = anySolo || matrix.getChannel(channelIndex).solo.load();

        const bool streamMuted = getBusMuted && getBusMuted(0);
        const bool monitorMuted = getBusMuted && getBusMuted(1);
        drawBus(g, streamX, bounds.getY(), "STREAM BUS", "OBS / Stream Output",
                juce::Colour(0xffa855f7), streamMuted);
        drawBus(g, monitorX, bounds.getY(), "MONITOR BUS", "Headphones / Monitor Output",
                juce::Colour(0xff06b6d4), monitorMuted);

        g.setColour(juce::Colour(0xff334155));
        g.drawVerticalLine(juce::roundToInt(streamX), rowStart - 8.0f, rowEnd);
        g.drawVerticalLine(juce::roundToInt(monitorX), rowStart - 8.0f, rowEnd);

        for (int channelIndex = 0; channelIndex < activeChannels; ++channelIndex)
        {
            const auto& channel = matrix.getChannel(channelIndex);
            const float centerY = rowStart + channelIndex * rowHeight;
            const float nodeY = centerY - 10.0f;
            const bool enabled = channel.isEnabled.load();
            const bool muted = channel.mute.load();
            const bool soloed = channel.solo.load();
            const bool soloGated = anySolo && !soloed;
            const bool sourceAvailable = channel.isSourceAvailable.load();
            const bool sourceConnected =
                channel.inputSource.load() != ChannelInputSource::Disabled && sourceAvailable;
            const float meter = juce::jlimit(0.0f, 1.0f, channel.inputMeter.load());
            const bool routeCanPassAudio = enabled && !muted && !soloGated && sourceConnected;

            auto nodeBounds = juce::Rectangle<float>(bounds.getX(), nodeY, nodeWidth, 20.0f);
            g.setColour(enabled ? juce::Colour(0xff172131) : juce::Colour(0xff241b20));
            g.fillRoundedRectangle(nodeBounds, 4.0f);
            g.setColour(enabled ? juce::Colour(0xff3b82f6) : juce::Colour(0xff713f46));
            g.drawRoundedRectangle(nodeBounds, 4.0f, 1.0f);
            g.setColour(routeCanPassAudio ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff9f7b80));
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText("CH " + juce::String(channelIndex + 1) + "  " + channel.name,
                       nodeBounds.withTrimmedLeft(7.0f).withTrimmedRight(86.0f),
                       juce::Justification::centredLeft, true);

            const auto stateText = muted ? "MUTE"
                : (soloGated ? "CUT" : (soloed ? "SOLO"
                    : (!enabled || !sourceConnected ? "OFF" : juce::String())));
            if (stateText.isNotEmpty())
            {
                const auto stateColour = muted ? juce::Colour(0xffef4444)
                    : (soloed ? juce::Colour(0xffeab308) : juce::Colour(0xff9f7b80));
                g.setColour(stateColour);
                g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                g.drawText(stateText, nodeBounds.withTrimmedLeft(nodeWidth - 83.0f)
                                                   .withTrimmedRight(44.0f),
                           juce::Justification::centred);
            }

            const float meterX = nodeBounds.getRight() - 42.0f;
            g.setColour(juce::Colour(0xff0b1018));
            g.fillRoundedRectangle(meterX, centerY - 3.0f, 34.0f, 6.0f, 3.0f);
            if (enabled && sourceConnected)
            {
                g.setColour(meter > 0.85f ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));
                g.fillRoundedRectangle(meterX, centerY - 3.0f, 34.0f * meter, 6.0f, 3.0f);
            }

            drawRoute(g, nodeBounds.getRight(), streamX, centerY,
                      channel.routeEnable[0].load(), routeCanPassAudio && !streamMuted,
                      channel.sendLevel[0].load(), juce::Colour(0xffa855f7));
            drawRoute(g, nodeBounds.getRight(), monitorX, centerY,
                      channel.routeEnable[1].load(), routeCanPassAudio && !monitorMuted,
                      channel.sendLevel[1].load(), juce::Colour(0xff06b6d4));
        }
    }

    void resized() override {}

private:
    static void drawBus(juce::Graphics& g, float centerX, float y,
                        const juce::String& title, const juce::String& output,
                        juce::Colour colour, bool muted)
    {
        auto bus = juce::Rectangle<float>(centerX - 82.0f, y + 18.0f, 164.0f, 44.0f);
        g.setColour(muted ? juce::Colour(0xff241b20) : colour.withAlpha(0.18f));
        g.fillRoundedRectangle(bus, 6.0f);
        g.setColour(muted ? juce::Colour(0xffef4444) : colour);
        g.drawRoundedRectangle(bus, 6.0f, 1.3f);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(title + (muted ? "  MUTE" : ""), bus.withTrimmedTop(4.0f).withHeight(17.0f),
                   juce::Justification::centred);
        g.setFont(juce::FontOptions(10.0f));
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawText(output, bus.withTrimmedTop(22.0f),
                   juce::Justification::centred);
    }

    static void drawRoute(juce::Graphics& g, float sourceX, float busX, float centerY,
                          bool routed, bool canPassAudio, float sendLevel,
                          juce::Colour colour)
    {
        const bool active = routed && canPassAudio;
        g.setColour(active ? colour.withAlpha(juce::jlimit(0.3f, 1.0f, sendLevel))
                           : juce::Colour(0xff303846));
        g.drawLine(sourceX + 4.0f, centerY, busX, centerY, active ? 2.0f : 1.0f);
        const float labelCenterX = (sourceX + 4.0f + busX) * 0.5f;
        const auto levelText = routed && sendLevel > 0.000001f
            ? juce::String(juce::Decibels::gainToDecibels(sendLevel), 0) + " dB"
            : (routed ? "-inf" : "OFF");
        auto labelBounds = juce::Rectangle<float>(labelCenterX - 22.0f, centerY - 8.0f, 44.0f, 15.0f);
        g.setColour(juce::Colour(0xff0d1117));
        g.fillRoundedRectangle(labelBounds, 3.0f);
        g.setColour(active ? colour : juce::Colour(0xff64748b));
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText(levelText, labelBounds, juce::Justification::centred);
        if (active)
        {
            g.fillEllipse(busX - 3.0f, centerY - 3.0f, 6.0f, 6.0f);
        }
    }

    void timerCallback() override { repaint(); }

    RoutingMatrix& matrix;
    std::function<bool(int)> getBusMuted;
};

class SignalFlowOverviewComponent final : public juce::Component
{
public:
    SignalFlowOverviewComponent(RoutingMatrix& matrix, std::function<bool(int)> busMutedProvider)
        : diagram(matrix, std::move(busMutedProvider))
    {
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&diagram, false);
        viewport.setScrollBarsShown(true, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));
        g.setColour(juce::Colour(0xffe2e8f0));
        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        g.drawText("LIVE INPUT ROUTING", 16, 8, getWidth() - 32, 22,
                   juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Live sends to Stream and Monitor. Dim paths are muted, solo-gated, unavailable, or disabled; green bars show input level.",
                   16, 31, getWidth() - 32, 18, juce::Justification::centredLeft);
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds().withTrimmedTop(54));
        diagram.setSize(juce::jmax(500, viewport.getWidth() - 16),
                        84 + maxInputs * 27);
    }

private:
    juce::Viewport viewport;
    RoutingFlowDiagram diagram;
};
}

class SignalFlowWindow final : public juce::DocumentWindow
{
public:
    SignalFlowWindow(RoutingMatrix& matrix, std::function<bool(int)> busMutedProvider)
        : juce::DocumentWindow("LiveBus Signal Flow", juce::Colour(0xff0d1117),
                               juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(720, 440, 1800, 1200);
        setContentOwned(new SignalFlowOverviewComponent(matrix, std::move(busMutedProvider)), true);
        setContentComponentSize(980, 640);
        centreWithSize(980, 640);
    }

    void closeButtonPressed() override { setVisible(false); }
};

RoutingMatrixComponent::RoutingMatrixComponent(RoutingMatrix& matrixRef, AudioDeviceService* deviceService,
                                               std::function<bool(int)> busMutedState)
    : matrix(matrixRef), audioDeviceService(deviceService),
      busMutedProvider(std::move(busMutedState))
{
    // 1. Header & Actions
    headerTitle.setText("AUDIO ROUTING MATRIX & PATCHBAY", juce::dontSendNotification);
    headerTitle.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4)); // Cyan
    addAndMakeVisible(headerTitle);

    headerSubtitle.setText("Channel sources, sends, and output routing", juce::dontSendNotification);
    headerSubtitle.setFont(juce::FontOptions(11.0f));
    headerSubtitle.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(headerSubtitle);

    warningBanner.setText("✓ Sources and buses valid", juce::dontSendNotification);
    warningBanner.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    warningBanner.setColour(juce::Label::textColourId, juce::Colour(0xff10b981)); // Emerald
    warningBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    warningBanner.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(warningBanner);

    presetLabel.setFont(juce::FontOptions(11.0f));
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(presetLabel);

    auto presetNames = RoutingMatrix::getPresetNames();
    for (size_t i = 0; i < presetNames.size(); ++i)
        presetCombo.addItem(presetNames[i], (int)i + 1);
    presetCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(presetCombo);

    applyPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    applyPresetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00f5d4));
    applyPresetBtn.addListener(this);
    addAndMakeVisible(applyPresetBtn);

    resetRoutesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    resetRoutesBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    resetRoutesBtn.setButtonText("Reset");
    resetRoutesBtn.setTooltip("Reset All Routes");
    resetRoutesBtn.addListener(this);
    addAndMakeVisible(resetRoutesBtn);

    copyToAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    copyToAllBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffb703));
    copyToAllBtn.setButtonText("Copy to All");
    copyToAllBtn.addListener(this);
    addAndMakeVisible(copyToAllBtn);

    signalFlowBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff164e63));
    signalFlowBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff67e8f9));
    signalFlowBtn.addListener(this);
    addAndMakeVisible(signalFlowBtn);

    advancedRoutingBtn.setClickingTogglesState(true);
    advancedRoutingBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    advancedRoutingBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff164e63));
    advancedRoutingBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff67e8f9));
    advancedRoutingBtn.setTooltip("Show or hide per-bus send, AUX, and direct hardware output columns");
    advancedRoutingBtn.addListener(this);
    addAndMakeVisible(advancedRoutingBtn);

    // 2. Main Matrix Viewport
    matrixTableContainer = std::make_unique<juce::Component>();
    matrixViewport.setViewedComponent(matrixTableContainer.get(), false);
    matrixViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(matrixViewport);

    // 3. Inspector Panel
    inspectorGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff2d3340));
    inspectorGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff00f5d4));
    inspectorGroup.setText("CHANNEL ROUTING DETAIL");
    inspectorGroup.setTextLabelPosition(juce::Justification::left);
    addAndMakeVisible(inspectorGroup);

    inspectorChTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    inspectorChTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(inspectorChTitle);

    for (int i = 0; i < maxInputs; ++i)
    {
        channelSelectorTabs[i].setButtonText("CH" + juce::String(i + 1));
        channelSelectorTabs[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2638));
        channelSelectorTabs[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        channelSelectorTabs[i].addListener(this);
        addAndMakeVisible(channelSelectorTabs[i]);
    }

    // Inspector - Section 1: Input Group
    inputGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff2d3340));
    inputGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(inputGroup);

    inputSourceTypeLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(inputSourceTypeLabel);

    inspectorSourceTypeCombo.addItem("Physical Hardware Input", 1);
    inspectorSourceTypeCombo.addItem("Audio Playback Player", 2);
    inspectorSourceTypeCombo.addItem("Internal Test Signal", 3);
    inspectorSourceTypeCombo.addItem("None / Disabled", 4);
    inspectorSourceTypeCombo.addItem("YouTube Player", 5);
    inspectorSourceTypeCombo.addListener(this);
    addAndMakeVisible(inspectorSourceTypeCombo);

    hardwarePortLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(hardwarePortLabel);

    for (int p = 0; p < numHardwareInputsAvailable; ++p)
    {
        const auto name = p < hardwareInputChannelNames.size() && hardwareInputChannelNames[p].isNotEmpty()
            ? hardwareInputChannelNames[p] : "Hardware Port " + juce::String(p + 1);
        inspectorHardwarePortCombo.addItem(name, p + 1);
    }
    if (numHardwareInputsAvailable == 0)
        inspectorHardwarePortCombo.addItem("<< No Hardware Inputs >>", 1);
    inspectorHardwarePortCombo.addListener(this);
    addAndMakeVisible(inspectorHardwarePortCombo);

    stereoModeLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(stereoModeLabel);

    inspectorStereoModeCombo.addItem("Mono (Center / Pan)", 1);
    inspectorStereoModeCombo.addItem("Stereo Pair (L/R Link)", 2);
    inspectorStereoModeCombo.addItem("Dual Mono Duplicate", 3);
    inspectorStereoModeCombo.addListener(this);
    addAndMakeVisible(inspectorStereoModeCombo);

    channelEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff00f5d4));
    channelEnableToggle.addListener(this);
    addAndMakeVisible(channelEnableToggle);

    duplicateWarningLabel.setFont(juce::FontOptions(10.0f));
    duplicateWarningLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb703)); // Amber
    addAndMakeVisible(duplicateWarningLabel);

    // Inspector - Section 2: Send Group
    sendGroup.setText("2. OUTPUT ROUTING & SENDS");
    sendGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff2d3340));
    sendGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(sendGroup);

    streamSendEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff10b981));
    streamSendEnableToggle.addListener(this);
    addAndMakeVisible(streamSendEnableToggle);

    streamPrePostCombo.addItem("Post-Fader Send", 1);
    streamPrePostCombo.addItem("Pre-Fader Send", 2);
    streamPrePostCombo.addListener(this);
    addAndMakeVisible(streamPrePostCombo);

    streamSendSlider.setRange(-60.0, 12.0, 0.1);
    streamSendSlider.setValue(0.0);
    streamSendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    streamSendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    streamSendSlider.addListener(this);
    addAndMakeVisible(streamSendSlider);

    streamSendDbLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    streamSendDbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    addAndMakeVisible(streamSendDbLabel);

    monitorSendEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff00f5d4));
    monitorSendEnableToggle.addListener(this);
    addAndMakeVisible(monitorSendEnableToggle);

    monitorPrePostCombo.addItem("Post-Fader Send", 1);
    monitorPrePostCombo.addItem("Pre-Fader Send", 2);
    monitorPrePostCombo.addListener(this);
    addAndMakeVisible(monitorPrePostCombo);

    monitorSendSlider.setRange(-60.0, 12.0, 0.1);
    monitorSendSlider.setValue(0.0);
    monitorSendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    monitorSendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    monitorSendSlider.addListener(this);
    addAndMakeVisible(monitorSendSlider);

    monitorSendDbLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    monitorSendDbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(monitorSendDbLabel);

    // Direct Hardware Output Routing Controls
    hwDirectLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    hwDirectLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb703));
    addAndMakeVisible(hwDirectLabel);

    for (int h = 0; h < 4; ++h)
    {
        inspectorHwEnableToggles[(size_t)h].setButtonText("HW " + juce::String(h + 1));
        inspectorHwEnableToggles[(size_t)h].setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffb703));
        inspectorHwEnableToggles[(size_t)h].addListener(this);
        addAndMakeVisible(inspectorHwEnableToggles[(size_t)h]);

        inspectorHwSendSliders[(size_t)h].setRange(-60.0, 12.0, 0.1);
        inspectorHwSendSliders[(size_t)h].setValue(0.0);
        inspectorHwSendSliders[(size_t)h].setSliderStyle(juce::Slider::LinearHorizontal);
        inspectorHwSendSliders[(size_t)h].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        inspectorHwSendSliders[(size_t)h].addListener(this);
        addAndMakeVisible(inspectorHwSendSliders[(size_t)h]);

        inspectorHwSendLabels[(size_t)h].setFont(juce::FontOptions(10.0f));
        inspectorHwSendLabels[(size_t)h].setColour(juce::Label::textColourId, juce::Colour(0xffffb703));
        inspectorHwSendLabels[(size_t)h].setText("0.0 dB", juce::dontSendNotification);
        addAndMakeVisible(inspectorHwSendLabels[(size_t)h]);
    }

    // Inspector - Section 3: Pan, Gain & Status
    panGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff2d3340));
    panGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(panGroup);

    panLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(panLabel);

    panKnob.setRange(-1.0, 1.0, 0.01);
    panKnob.setValue(0.0);
    panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panKnob.addListener(this);
    addAndMakeVisible(panKnob);

    panValueLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    panValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    panValueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(panValueLabel);

    gainLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(gainLabel);

    channelGainSlider.setRange(0.0, 2.0, 0.01);
    channelGainSlider.setValue(0.707);
    channelGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    channelGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    channelGainSlider.addListener(this);
    addAndMakeVisible(channelGainSlider);

    gainValueLabel.setFont(juce::FontOptions(11.0f));
    gainValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(gainValueLabel);

    muteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    muteBtn.addListener(this);
    addAndMakeVisible(muteBtn);

    soloBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffb703));
    soloBtn.addListener(this);
    addAndMakeVisible(soloBtn);

    statusDetailLabel.setFont(juce::FontOptions(11.0f));
    statusDetailLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(statusDetailLabel);

    // Section 2: Submix Group Bus assignment
    submixGroupLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(submixGroupLabel);

    inspectorSubmixCombo.addItem("Direct to Master (Stream/Monitor)", 1);
    inspectorSubmixCombo.addItem("Submix 1 (Vocals)", 2);
    inspectorSubmixCombo.addItem("Submix 2 (BGM / Music)", 3);
    inspectorSubmixCombo.addItem("Submix 3 (Instruments)", 4);
    inspectorSubmixCombo.addItem("Submix 4 (Custom Group)", 5);
    inspectorSubmixCombo.addListener(this);
    addAndMakeVisible(inspectorSubmixCombo);

    // Section 2: Aux 1 Send (Reverb)
    aux1SendEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffa78bfa));
    aux1SendEnableToggle.addListener(this);
    addAndMakeVisible(aux1SendEnableToggle);

    aux1PrePostCombo.addItem("Post-Fader Send", 1);
    aux1PrePostCombo.addItem("Pre-Fader Send", 2);
    aux1PrePostCombo.addListener(this);
    addAndMakeVisible(aux1PrePostCombo);

    aux1SendSlider.setRange(0.0, 2.0, 0.01);
    aux1SendSlider.setValue(0.0);
    aux1SendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    aux1SendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    aux1SendSlider.addListener(this);
    addAndMakeVisible(aux1SendSlider);

    aux1SendDbLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    aux1SendDbLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa78bfa));
    aux1SendDbLabel.setText("-inf dB", juce::dontSendNotification);
    addAndMakeVisible(aux1SendDbLabel);

    // Section 2: Aux 2 Send (Delay)
    aux2SendEnableToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff38bdf8));
    aux2SendEnableToggle.addListener(this);
    addAndMakeVisible(aux2SendEnableToggle);

    aux2PrePostCombo.addItem("Post-Fader Send", 1);
    aux2PrePostCombo.addItem("Pre-Fader Send", 2);
    aux2PrePostCombo.addListener(this);
    addAndMakeVisible(aux2PrePostCombo);

    aux2SendSlider.setRange(0.0, 2.0, 0.01);
    aux2SendSlider.setValue(0.0);
    aux2SendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    aux2SendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    aux2SendSlider.addListener(this);
    addAndMakeVisible(aux2SendSlider);

    aux2SendDbLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    aux2SendDbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    aux2SendDbLabel.setText("-inf dB", juce::dontSendNotification);
    addAndMakeVisible(aux2SendDbLabel);

    // Section 4: Aux Return & Submix Bus Masters
    auxSubmixGroup.setText("4. AUX / SUBMIX MASTERS");
    auxSubmixGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff2d3340));
    auxSubmixGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xffffb703));
    addAndMakeVisible(auxSubmixGroup);

    // Aux 1 Return Master
    aux1ReturnLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    aux1ReturnLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa78bfa));
    addAndMakeVisible(aux1ReturnLabel);

    aux1ReturnSlider.setRange(0.0, 2.0, 0.01);
    aux1ReturnSlider.setValue(0.707);
    aux1ReturnSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    aux1ReturnSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    aux1ReturnSlider.addListener(this);
    addAndMakeVisible(aux1ReturnSlider);

    aux1ReturnDbLabel.setFont(juce::FontOptions(10.0f));
    aux1ReturnDbLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa78bfa));
    aux1ReturnDbLabel.setText("-3.0 dB", juce::dontSendNotification);
    addAndMakeVisible(aux1ReturnDbLabel);

    aux1RoomSizeSlider.setRange(0.0, 1.0, 0.01);
    aux1RoomSizeSlider.setValue(0.5);
    aux1RoomSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    aux1RoomSizeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    aux1RoomSizeSlider.addListener(this);
    addAndMakeVisible(aux1RoomSizeSlider);

    aux1MuteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    aux1MuteBtn.addListener(this);
    addAndMakeVisible(aux1MuteBtn);

    // Aux 2 Return Master
    aux2ReturnLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    aux2ReturnLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(aux2ReturnLabel);

    aux2ReturnSlider.setRange(0.0, 2.0, 0.01);
    aux2ReturnSlider.setValue(0.707);
    aux2ReturnSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    aux2ReturnSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    aux2ReturnSlider.addListener(this);
    addAndMakeVisible(aux2ReturnSlider);

    aux2ReturnDbLabel.setFont(juce::FontOptions(10.0f));
    aux2ReturnDbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    aux2ReturnDbLabel.setText("-3.0 dB", juce::dontSendNotification);
    addAndMakeVisible(aux2ReturnDbLabel);

    aux2DelayTimeSlider.setRange(0.05, 1.0, 0.01);
    aux2DelayTimeSlider.setValue(0.35);
    aux2DelayTimeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    aux2DelayTimeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    aux2DelayTimeSlider.addListener(this);
    addAndMakeVisible(aux2DelayTimeSlider);

    aux2MuteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    aux2MuteBtn.addListener(this);
    addAndMakeVisible(aux2MuteBtn);

    // Submix 1 Master
    submix1Label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    submix1Label.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    addAndMakeVisible(submix1Label);

    submix1FaderSlider.setRange(0.0, 2.0, 0.01);
    submix1FaderSlider.setValue(1.0);
    submix1FaderSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    submix1FaderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    submix1FaderSlider.addListener(this);
    addAndMakeVisible(submix1FaderSlider);

    submix1DbLabel.setFont(juce::FontOptions(10.0f));
    submix1DbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    submix1DbLabel.setText("0.0 dB", juce::dontSendNotification);
    addAndMakeVisible(submix1DbLabel);

    submix1MuteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    submix1MuteBtn.addListener(this);
    addAndMakeVisible(submix1MuteBtn);

    // Submix 2 Master
    submix2Label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    submix2Label.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
    addAndMakeVisible(submix2Label);

    submix2FaderSlider.setRange(0.0, 2.0, 0.01);
    submix2FaderSlider.setValue(1.0);
    submix2FaderSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    submix2FaderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    submix2FaderSlider.addListener(this);
    addAndMakeVisible(submix2FaderSlider);

    submix2DbLabel.setFont(juce::FontOptions(10.0f));
    submix2DbLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
    submix2DbLabel.setText("0.0 dB", juce::dontSendNotification);
    addAndMakeVisible(submix2DbLabel);

    submix2MuteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    submix2MuteBtn.addListener(this);
    addAndMakeVisible(submix2MuteBtn);

    refreshFromMatrix();
    setSelectedChannel(0);
    signalFlowBtn.setClickingTogglesState(true);
    signalFlowBtn.setTooltip("Show or hide the interactive routing graph");

    startTimerHz(30);
}

RoutingMatrixComponent::~RoutingMatrixComponent()
{
    stopTimer();
    signalFlowWindow.reset();
}

void RoutingMatrixComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));

    // Matrix Table Header Background
    auto headerBar = matrixViewport.getBounds()
                         .withY(matrixViewport.getY() - 26)
                         .withHeight(26);
    g.setColour(juce::Colour(0xff161f2e));
    g.fillRect(headerBar);
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRect(headerBar, 1);

    // Matrix Column Headers
    g.saveState();
    g.reduceClipRegion(headerBar);
    const int x = headerBar.getX() - matrixViewport.getViewPositionX();
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

    g.drawText("CH", x + 5, headerBar.getY(), 35, headerBar.getHeight(), juce::Justification::centredLeft);
    g.drawText("CHANNEL NAME", x + 45, headerBar.getY(), 125, headerBar.getHeight(), juce::Justification::centredLeft);
    g.drawText("INPUT SOURCE", x + 175, headerBar.getY(), 160, headerBar.getHeight(), juce::Justification::centredLeft);
    if (advancedRoutingVisible)
    {
        g.drawText("STEREO MODE", x + 340, headerBar.getY(), 95, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("SUBMIX", x + 440, headerBar.getY(), 95, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("STATUS", x + 540, headerBar.getY(), 70, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("IN VU", x + 615, headerBar.getY(), 40, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("STREAM (SEND)", x + 660, headerBar.getY(), 140, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("MONITOR (SEND)", x + 805, headerBar.getY(), 140, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("AUX 1 (REV)", x + 950, headerBar.getY(), 115, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("AUX 2 (DLY)", x + 1070, headerBar.getY(), 115, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("HW 1", x + 1190, headerBar.getY(), 42, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("HW 2", x + 1235, headerBar.getY(), 42, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("HW 3", x + 1280, headerBar.getY(), 42, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("HW 4", x + 1325, headerBar.getY(), 42, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("PAN", x + 1370, headerBar.getY(), 55, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("ACTIONS", x + 1430, headerBar.getY(), 70, headerBar.getHeight(), juce::Justification::centred);
    }
    else
    {
        g.drawText("OUTPUT", x + 340, headerBar.getY(), 145, headerBar.getHeight(), juce::Justification::centredLeft);
        g.drawText("LEVEL", x + 490, headerBar.getY(), 115, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("PAN", x + 610, headerBar.getY(), 70, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("MUTE", x + 685, headerBar.getY(), 50, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("SOLO", x + 740, headerBar.getY(), 50, headerBar.getHeight(), juce::Justification::centred);
        g.drawText("DETAILS", x + 795, headerBar.getY(), 70, headerBar.getHeight(), juce::Justification::centred);
    }
    g.restoreState();
}

void RoutingMatrixComponent::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Top Header
    auto topArea = area.removeFromTop(36);
    auto titleArea = topArea.removeFromLeft(480);
    auto titleTextArea = titleArea.removeFromLeft(titleArea.getWidth() - 210);
    headerTitle.setBounds(titleTextArea.removeFromTop(20));
    headerSubtitle.setBounds(titleTextArea);
    advancedRoutingBtn.setBounds(titleArea.getX(), topArea.getY() + 5, 112, 26);
    signalFlowBtn.setBounds(titleArea.getX() + 118, topArea.getY() + 5, 92, 26);

    auto actionsArea = topArea;
    warningBanner.setBounds(actionsArea.removeFromLeft(175).reduced(2));
    actionsArea.removeFromLeft(5);
    presetLabel.setBounds(actionsArea.removeFromLeft(45).withHeight(26));
    presetCombo.setBounds(actionsArea.removeFromLeft(160).withHeight(26));
    actionsArea.removeFromLeft(5);
    applyPresetBtn.setBounds(actionsArea.removeFromLeft(88).withHeight(26));
    actionsArea.removeFromLeft(5);
    copyToAllBtn.setBounds(actionsArea.removeFromLeft(118).withHeight(26));
    actionsArea.removeFromLeft(5);
    resetRoutesBtn.setBounds(actionsArea.removeFromLeft(90).withHeight(26));

    area.removeFromTop(6);

    // Main Matrix Viewport Area (top half)
    auto matrixArea = area.removeFromTop(185);
    matrixViewport.setBounds(matrixArea.withTrimmedTop(26));
    if (routingGraph != nullptr)
    {
        routingGraph->setBounds(getLocalBounds().reduced(8).withTrimmedTop(42));
        if (routingGraph->isVisible())
            routingGraph->toFront(false);
    }

    int rowH = 34;
    int totalRows = (int)rowUIs.size();
    matrixTableContainer->setBounds(0, 0,
        juce::jmax(advancedRoutingVisible ? 1510 : 850, matrixViewport.getWidth() - 16),
        totalRows * rowH);

    for (int i = 0; i < totalRows; ++i)
    {
        auto& row = *rowUIs[(size_t)i];
        int y = i * rowH;
        int x = 2;

        row.chNumLabel.setBounds(x, y + 4, 38, 24);
        x += 42;
        row.nameEditor.setBounds(x, y + 4, advancedRoutingVisible ? 125 : 120, 24);
        x += advancedRoutingVisible ? 130 : 125;
        row.inputSourceCombo.setBounds(x, y + 4, advancedRoutingVisible ? 160 : 170, 24);
        x += advancedRoutingVisible ? 165 : 175;

        if (advancedRoutingVisible)
        {
            row.stereoModeCombo.setBounds(x, y + 4, 95, 24);
            x += 100;
            row.submixGroupCombo.setBounds(x, y + 4, 95, 24);
            x += 100;
            row.statusBadge.setBounds(x, y + 4, 70, 24);
            x += 75 + 45;
            row.streamEnableBtn.setBounds(x, y + 4, 60, 24);
            x += 65;
            row.streamSendSlider.setBounds(x, y + 4, 75, 24);
            x += 80;
            row.monitorEnableBtn.setBounds(x, y + 4, 60, 24);
            x += 65;
            row.monitorSendSlider.setBounds(x, y + 4, 75, 24);
            x += 80;
            row.aux1EnableBtn.setBounds(x, y + 4, 50, 24);
            x += 55;
            row.aux1SendSlider.setBounds(x, y + 4, 60, 24);
            x += 65;
            row.aux2EnableBtn.setBounds(x, y + 4, 50, 24);
            x += 55;
            row.aux2SendSlider.setBounds(x, y + 4, 60, 24);
            x += 65;
            for (int h = 0; h < 4; ++h)
            {
                row.hwEnableBtns[(size_t)h].setBounds(x, y + 4, 42, 24);
                x += 45;
            }
            row.panSlider.setBounds(x, y + 4, 55, 24);
            x += 60;
            row.inspectBtn.setBounds(x, y + 4, 70, 24);
        }
        else
        {
            row.outputRouteCombo.setBounds(x, y + 4, 150, 24);
            x += 155;
            row.gainSlider.setBounds(x, y + 4, 112, 24);
            x += 117;
            row.panSlider.setBounds(x, y + 4, 70, 24);
            x += 75;
            row.muteBtn.setBounds(x, y + 4, 48, 24);
            x += 53;
            row.soloBtn.setBounds(x, y + 4, 48, 24);
            x += 53;
            row.inspectBtn.setBounds(x, y + 4, 70, 24);
        }
    }

    area.removeFromTop(6);

    // Lower Half: Channel Detail Inspector
    inspectorGroup.setBounds(area);
    auto inspArea = area.reduced(8);
    inspArea.removeFromTop(16); // group header offset

    auto tabRow = inspArea.removeFromTop(26);
    inspectorChTitle.setBounds(tabRow.removeFromLeft(320));
    tabRow.removeFromLeft(10);
    int tabW = juce::jmax(34, (tabRow.getWidth() - (maxInputs - 1) * 2) / maxInputs);
    for (int i = 0; i < maxInputs; ++i)
    {
        channelSelectorTabs[i].setBounds(tabRow.removeFromLeft(tabW));
        tabRow.removeFromLeft(2);
    }

    inspArea.removeFromTop(6);

    // 4 Columns inside Inspector
    int colW = (inspArea.getWidth() - 24) / 4;

    // Column 1: Input Group
    auto col1 = inspArea.removeFromLeft(colW);
    inputGroup.setBounds(col1);
    auto c1 = col1.reduced(10);
    c1.removeFromTop(18);

    inputSourceTypeLabel.setBounds(c1.removeFromTop(16));
    inspectorSourceTypeCombo.setBounds(c1.removeFromTop(24));
    c1.removeFromTop(6);

    hardwarePortLabel.setBounds(c1.removeFromTop(16));
    inspectorHardwarePortCombo.setBounds(c1.removeFromTop(24));
    c1.removeFromTop(6);

    stereoModeLabel.setBounds(c1.removeFromTop(16));
    inspectorStereoModeCombo.setBounds(c1.removeFromTop(24));
    c1.removeFromTop(8);

    channelEnableToggle.setBounds(c1.removeFromTop(24));
    duplicateWarningLabel.setBounds(c1.removeFromTop(20));

    inspArea.removeFromLeft(8);

    // Column 2: Send Group (Stream, Monitor, Submix, Aux, HW direct)
    auto col2 = inspArea.removeFromLeft(colW);
    sendGroup.setBounds(col2);
    auto c2 = col2.reduced(10);
    c2.removeFromTop(18);

    // Submix assignment in Section 2
    auto smRow = c2.removeFromTop(22);
    submixGroupLabel.setBounds(smRow.removeFromLeft(75));
    inspectorSubmixCombo.setBounds(smRow);
    c2.removeFromTop(4);

    auto strRow = c2.removeFromTop(22);
    streamSendEnableToggle.setBounds(strRow.removeFromLeft(140));
    streamPrePostCombo.setBounds(strRow.removeFromRight(90));

    auto strSliderRow = c2.removeFromTop(22);
    streamSendSlider.setBounds(strSliderRow.removeFromLeft(strSliderRow.getWidth() - 55));
    streamSendDbLabel.setBounds(strSliderRow);
    c2.removeFromTop(4);

    auto monRow = c2.removeFromTop(22);
    monitorSendEnableToggle.setBounds(monRow.removeFromLeft(150));
    monitorPrePostCombo.setBounds(monRow.removeFromRight(90));

    auto monSliderRow = c2.removeFromTop(22);
    monitorSendSlider.setBounds(monSliderRow.removeFromLeft(monSliderRow.getWidth() - 55));
    monitorSendDbLabel.setBounds(monSliderRow);
    c2.removeFromTop(4);

    // Aux 1 Send (Reverb)
    auto a1Row = c2.removeFromTop(22);
    aux1SendEnableToggle.setBounds(a1Row.removeFromLeft(140));
    aux1PrePostCombo.setBounds(a1Row.removeFromRight(90));

    auto a1SliderRow = c2.removeFromTop(22);
    aux1SendSlider.setBounds(a1SliderRow.removeFromLeft(a1SliderRow.getWidth() - 55));
    aux1SendDbLabel.setBounds(a1SliderRow);
    c2.removeFromTop(4);

    // Aux 2 Send (Delay)
    auto a2Row = c2.removeFromTop(22);
    aux2SendEnableToggle.setBounds(a2Row.removeFromLeft(140));
    aux2PrePostCombo.setBounds(a2Row.removeFromRight(90));

    auto a2SliderRow = c2.removeFromTop(22);
    aux2SendSlider.setBounds(a2SliderRow.removeFromLeft(a2SliderRow.getWidth() - 55));
    aux2SendDbLabel.setBounds(a2SliderRow);
    c2.removeFromTop(4);

    // Direct Hardware Output Rows
    hwDirectLabel.setBounds(c2.removeFromTop(16));
    for (int h = 0; h < 4; ++h)
    {
        auto hwRow = c2.removeFromTop(20);
        inspectorHwEnableToggles[(size_t)h].setBounds(hwRow.removeFromLeft(60));
        inspectorHwSendSliders[(size_t)h].setBounds(hwRow.removeFromLeft(hwRow.getWidth() - 45));
        inspectorHwSendLabels[(size_t)h].setBounds(hwRow);
        c2.removeFromTop(2);
    }

    inspArea.removeFromLeft(8);

    // Column 3: Pan, Gain & Status
    auto col3 = inspArea.removeFromLeft(colW);
    panGroup.setBounds(col3);
    auto c3 = col3.reduced(10);
    c3.removeFromTop(18);

    auto panRow = c3.removeFromTop(50);
    panLabel.setBounds(panRow.removeFromLeft(75).withHeight(20));
    panKnob.setBounds(panRow.removeFromLeft(50));
    panValueLabel.setBounds(panRow.removeFromLeft(60).withHeight(20));

    c3.removeFromTop(6);
    auto gainRow = c3.removeFromTop(24);
    gainLabel.setBounds(gainRow.removeFromLeft(75));
    channelGainSlider.setBounds(gainRow.removeFromLeft(gainRow.getWidth() - 60));
    gainValueLabel.setBounds(gainRow);

    c3.removeFromTop(8);
    auto btnRow = c3.removeFromTop(24);
    muteBtn.setBounds(btnRow.removeFromLeft(80));
    btnRow.removeFromLeft(10);
    soloBtn.setBounds(btnRow.removeFromLeft(80));

    c3.removeFromTop(6);
    statusDetailLabel.setBounds(c3.removeFromTop(20));

    inspArea.removeFromLeft(8);

    // Column 4: Section 4 - Aux Return & Submix Bus Masters
    auto col4 = inspArea;
    auxSubmixGroup.setBounds(col4);
    auto c4 = col4.reduced(10);
    c4.removeFromTop(18);

    // Aux 1 Master (Reverb)
    auto a1MasterRow = c4.removeFromTop(22);
    aux1ReturnLabel.setBounds(a1MasterRow.removeFromLeft(90));
    aux1MuteBtn.setBounds(a1MasterRow.removeFromRight(55));
    auto a1FaderRow = c4.removeFromTop(22);
    aux1ReturnSlider.setBounds(a1FaderRow.removeFromLeft(a1FaderRow.getWidth() - 50));
    aux1ReturnDbLabel.setBounds(a1FaderRow);
    auto a1RoomRow = c4.removeFromTop(20);
    aux1RoomSizeSlider.setBounds(a1RoomRow);
    c4.removeFromTop(6);

    // Aux 2 Master (Delay)
    auto a2MasterRow = c4.removeFromTop(22);
    aux2ReturnLabel.setBounds(a2MasterRow.removeFromLeft(90));
    aux2MuteBtn.setBounds(a2MasterRow.removeFromRight(55));
    auto a2FaderRow = c4.removeFromTop(22);
    aux2ReturnSlider.setBounds(a2FaderRow.removeFromLeft(a2FaderRow.getWidth() - 50));
    aux2ReturnDbLabel.setBounds(a2FaderRow);
    auto a2TimeRow = c4.removeFromTop(20);
    aux2DelayTimeSlider.setBounds(a2TimeRow);
    c4.removeFromTop(6);

    // Submix 1 Master (Vocals)
    auto s1Row = c4.removeFromTop(22);
    submix1Label.setBounds(s1Row.removeFromLeft(90));
    submix1MuteBtn.setBounds(s1Row.removeFromRight(55));
    auto s1FaderRow = c4.removeFromTop(22);
    submix1FaderSlider.setBounds(s1FaderRow.removeFromLeft(s1FaderRow.getWidth() - 50));
    submix1DbLabel.setBounds(s1FaderRow);
    c4.removeFromTop(6);

    // Submix 2 Master (BGM)
    auto s2Row = c4.removeFromTop(22);
    submix2Label.setBounds(s2Row.removeFromLeft(90));
    submix2MuteBtn.setBounds(s2Row.removeFromRight(55));
    auto s2FaderRow = c4.removeFromTop(22);
    submix2FaderSlider.setBounds(s2FaderRow.removeFromLeft(s2FaderRow.getWidth() - 50));
    submix2DbLabel.setBounds(s2FaderRow);
}

void RoutingMatrixComponent::refreshFromMatrix()
{
    int numActive = matrix.getNumActiveChannels();
    selectedChannelIndex = juce::jlimit(0, juce::jmax(0, numActive - 1), selectedChannelIndex);
    rowUIs.clear();

    for (int i = 0; i < numActive; ++i)
        buildMatrixRow(i);

    // Refresh Section 4 Masters
    const auto& aux1 = matrix.getAuxBus(0);
    aux1ReturnSlider.setValue(aux1.returnLevel.load(), juce::dontSendNotification);
    float a1Db = aux1.returnLevel.load() > 0.0001f ? 20.0f * std::log10(aux1.returnLevel.load()) : -60.0f;
    aux1ReturnDbLabel.setText(juce::String(a1Db, 1) + " dB", juce::dontSendNotification);
    aux1RoomSizeSlider.setValue(aux1.fxParam1.load(), juce::dontSendNotification);
    aux1MuteBtn.setToggleState(aux1.mute.load(), juce::dontSendNotification);

    const auto& aux2 = matrix.getAuxBus(1);
    aux2ReturnSlider.setValue(aux2.returnLevel.load(), juce::dontSendNotification);
    float a2Db = aux2.returnLevel.load() > 0.0001f ? 20.0f * std::log10(aux2.returnLevel.load()) : -60.0f;
    aux2ReturnDbLabel.setText(juce::String(a2Db, 1) + " dB", juce::dontSendNotification);
    aux2DelayTimeSlider.setValue(aux2.fxParam1.load(), juce::dontSendNotification);
    aux2MuteBtn.setToggleState(aux2.mute.load(), juce::dontSendNotification);

    const auto& sub1 = matrix.getSubmixBus(0);
    submix1FaderSlider.setValue(sub1.gain.load(), juce::dontSendNotification);
    float s1Db = sub1.gain.load() > 0.0001f ? 20.0f * std::log10(sub1.gain.load()) : -60.0f;
    submix1DbLabel.setText(juce::String(s1Db, 1) + " dB", juce::dontSendNotification);
    submix1MuteBtn.setToggleState(sub1.mute.load(), juce::dontSendNotification);

    const auto& sub2 = matrix.getSubmixBus(1);
    submix2FaderSlider.setValue(sub2.gain.load(), juce::dontSendNotification);
    float s2Db = sub2.gain.load() > 0.0001f ? 20.0f * std::log10(sub2.gain.load()) : -60.0f;
    submix2DbLabel.setText(juce::String(s2Db, 1) + " dB", juce::dontSendNotification);
    submix2MuteBtn.setToggleState(sub2.mute.load(), juce::dontSendNotification);

    resized();
    updateInspectorForSelectedChannel();
}

void RoutingMatrixComponent::syncFromMatrix()
{
    const int activeChannels = matrix.getNumActiveChannels();
    if (activeChannels != (int)rowUIs.size())
    {
        refreshFromMatrix();
        return;
    }

    for (int i = 0; i < activeChannels; ++i)
    {
        auto& state = matrix.getChannel(i);
        auto& row = *rowUIs[(size_t)i];

        if (!row.nameEditor.hasKeyboardFocus(false) && row.nameEditor.getText() != state.name)
            row.nameEditor.setText(state.name, false);

        int sourceId = 9;
        switch (state.inputSource.load())
        {
            case ChannelInputSource::Hardware:
                sourceId = state.isSourceAvailable.load()
                    ? 101 + state.hardwareInputChannel.load()
                    : 500 + state.hardwareInputChannel.load();
                break;
            case ChannelInputSource::Playback:
                sourceId = 5 + state.playbackChannel.load();
                break;
            case ChannelInputSource::TestSignal:
                sourceId = 8;
                break;
            case ChannelInputSource::YouTube:
                sourceId = 10;
                break;
            case ChannelInputSource::Disabled:
            default:
                sourceId = 9;
                break;
        }

        if (sourceId >= 500 && row.inputSourceCombo.indexOfItemId(sourceId) < 0)
            row.inputSourceCombo.addItem("Hardware In " +
                juce::String(state.hardwareInputChannel.load() + 1) + " (Unavailable)", sourceId);
        if (!row.inputSourceCombo.isPopupActive() && row.inputSourceCombo.getSelectedId() != sourceId)
            row.inputSourceCombo.setSelectedId(sourceId, juce::dontSendNotification);
        if (!row.stereoModeCombo.isPopupActive())
            row.stereoModeCombo.setSelectedId((int)state.stereoMode.load() + 1, juce::dontSendNotification);
        if (!row.submixGroupCombo.isPopupActive())
            row.submixGroupCombo.setSelectedId(state.submixGroup.load() + 2, juce::dontSendNotification);
        if (!row.outputRouteCombo.isPopupActive())
        {
            const int submix = state.submixGroup.load();
            const int routeId = submix >= 0 && submix < maxSubmixBuses
                ? 5 + submix
                : (state.routeEnable[0].load()
                    ? (state.routeEnable[1].load() ? 1 : 2)
                    : (state.routeEnable[1].load() ? 3 : 4));
            row.outputRouteCombo.setSelectedId(routeId, juce::dontSendNotification);
        }
        if (!row.gainSlider.isMouseButtonDown())
            row.gainSlider.setValue(juce::Decibels::gainToDecibels(state.gain.load(), -60.0f),
                                    juce::dontSendNotification);
        row.muteBtn.setToggleState(state.mute.load(), juce::dontSendNotification);
        row.soloBtn.setToggleState(state.solo.load(), juce::dontSendNotification);
        row.streamEnableBtn.setToggleState(state.routeEnable[0].load(), juce::dontSendNotification);
        row.monitorEnableBtn.setToggleState(state.routeEnable[1].load(), juce::dontSendNotification);
        row.aux1EnableBtn.setToggleState(state.auxRouteEnable[0].load(), juce::dontSendNotification);
        row.aux2EnableBtn.setToggleState(state.auxRouteEnable[1].load(), juce::dontSendNotification);

        if (!row.streamSendSlider.isMouseButtonDown())
            row.streamSendSlider.setValue(state.sendLevel[0].load(), juce::dontSendNotification);
        if (!row.monitorSendSlider.isMouseButtonDown())
            row.monitorSendSlider.setValue(state.sendLevel[1].load(), juce::dontSendNotification);
        if (!row.aux1SendSlider.isMouseButtonDown())
            row.aux1SendSlider.setValue(state.auxSendLevel[0].load(), juce::dontSendNotification);
        if (!row.aux2SendSlider.isMouseButtonDown())
            row.aux2SendSlider.setValue(state.auxSendLevel[1].load(), juce::dontSendNotification);
        if (!row.panSlider.isMouseButtonDown())
            row.panSlider.setValue(state.pan.load(), juce::dontSendNotification);

        for (int h = 0; h < 4; ++h)
            row.hwEnableBtns[(size_t)h].setToggleState(state.hwRouteEnable[(size_t)h].load(), juce::dontSendNotification);
    }

    const bool inspectorIsBeingEdited =
        streamSendSlider.isMouseButtonDown() || monitorSendSlider.isMouseButtonDown() ||
        aux1SendSlider.isMouseButtonDown() || aux2SendSlider.isMouseButtonDown() ||
        panKnob.isMouseButtonDown() || std::any_of(inspectorHwSendSliders.begin(), inspectorHwSendSliders.end(),
            [](const juce::Slider& slider) { return slider.isMouseButtonDown(); }) ||
        inspectorSourceTypeCombo.isPopupActive() || inspectorHardwarePortCombo.isPopupActive() ||
        inspectorStereoModeCombo.isPopupActive() || inspectorSubmixCombo.isPopupActive() ||
        streamPrePostCombo.isPopupActive() || monitorPrePostCombo.isPopupActive() ||
        aux1PrePostCombo.isPopupActive() || aux2PrePostCombo.isPopupActive();
    if (!inspectorIsBeingEdited)
        updateInspectorForSelectedChannel();
}

void RoutingMatrixComponent::buildMatrixRow(int ch)
{
    auto row = std::make_unique<MatrixRowUI>();
    auto& chState = matrix.getChannel(ch);

    row->chNumLabel.setText("CH " + juce::String(ch + 1), juce::dontSendNotification);
    row->chNumLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    row->chNumLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    matrixTableContainer->addAndMakeVisible(row->chNumLabel);

    row->nameEditor.setText(chState.name, false);
    row->nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff161f2e));
    row->nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    row->nameEditor.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->nameEditor);

    // Input Source combo
    for (int p = 0; p < numHardwareInputsAvailable; ++p)
    {
        const auto name = p < hardwareInputChannelNames.size() && hardwareInputChannelNames[p].isNotEmpty()
            ? hardwareInputChannelNames[p] : "Hardware In " + juce::String(p + 1);
        row->inputSourceCombo.addItem(name, 101 + p);
    }
    if (numHardwareInputsAvailable == 0)
        row->inputSourceCombo.addItem("<< No Hardware Inputs >>", 100);
    row->inputSourceCombo.addItem("Playback Player (L)", 5);
    row->inputSourceCombo.addItem("Playback Player (R)", 6);
    row->inputSourceCombo.addItem("Playback Player (Sum)", 7);
    row->inputSourceCombo.addItem("Test Signal", 8);
    row->inputSourceCombo.addItem("None (Disabled)", 9);
    row->inputSourceCombo.addItem("YouTube Player", 10);

    auto src = chState.inputSource.load();
    if (src == ChannelInputSource::Hardware)
    {
        const int hwChannel = chState.hardwareInputChannel.load();
        const int sourceId = chState.isSourceAvailable.load() ? 101 + hwChannel : 500 + hwChannel;
        if (!chState.isSourceAvailable.load())
            row->inputSourceCombo.addItem("Hardware In " + juce::String(hwChannel + 1) + " (Unavailable)", sourceId);
        row->inputSourceCombo.setSelectedId(sourceId, juce::dontSendNotification);
    }
    else if (src == ChannelInputSource::Playback)
        row->inputSourceCombo.setSelectedId(5 + chState.playbackChannel.load(), juce::dontSendNotification);
    else if (src == ChannelInputSource::TestSignal)
        row->inputSourceCombo.setSelectedId(8, juce::dontSendNotification);
    else
        row->inputSourceCombo.setSelectedId(9, juce::dontSendNotification);

    row->inputSourceCombo.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->inputSourceCombo);

    row->outputRouteCombo.addItem("Stream + Monitor", 1);
    row->outputRouteCombo.addItem("Stream only", 2);
    row->outputRouteCombo.addItem("Monitor only", 3);
    row->outputRouteCombo.addItem("Off", 4);
    row->outputRouteCombo.addSeparator();
    row->outputRouteCombo.addItem("Sub 1 Vocals", 5);
    row->outputRouteCombo.addItem("Sub 2 BGM", 6);
    row->outputRouteCombo.addItem("Sub 3 Inst", 7);
    row->outputRouteCombo.addItem("Sub 4 Aux", 8);
    const int submix = chState.submixGroup.load();
    const int outputRouteId = submix >= 0 && submix < maxSubmixBuses
        ? 5 + submix
        : (chState.routeEnable[0].load()
            ? (chState.routeEnable[1].load() ? 1 : 2)
            : (chState.routeEnable[1].load() ? 3 : 4));
    row->outputRouteCombo.setSelectedId(outputRouteId, juce::dontSendNotification);
    row->outputRouteCombo.setTooltip("Choose the direct output bus or a submix");
    row->outputRouteCombo.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->outputRouteCombo);

    // Stereo Mode combo
    row->stereoModeCombo.addItem("Mono", 1);
    row->stereoModeCombo.addItem("Stereo Pair", 2);
    row->stereoModeCombo.addItem("Dual Mono", 3);
    row->stereoModeCombo.setSelectedId((int)chState.stereoMode.load() + 1, juce::dontSendNotification);
    row->stereoModeCombo.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->stereoModeCombo);

    // Submix Group Combo
    row->submixGroupCombo.addItem("Direct Master", 1);
    row->submixGroupCombo.addItem("Sub 1 (Vocals)", 2);
    row->submixGroupCombo.addItem("Sub 2 (BGM)", 3);
    row->submixGroupCombo.addItem("Sub 3 (Inst)", 4);
    row->submixGroupCombo.addItem("Sub 4 (Aux)", 5);
    row->submixGroupCombo.setSelectedId(chState.submixGroup.load() + 2, juce::dontSendNotification);
    row->submixGroupCombo.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->submixGroupCombo);

    // Status Badge
    row->statusBadge.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    row->statusBadge.setJustificationType(juce::Justification::centred);
    matrixTableContainer->addAndMakeVisible(row->statusBadge);

    // Stream send
    row->streamEnableBtn.setToggleState(chState.routeEnable[0].load(), juce::dontSendNotification);
    row->streamEnableBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->streamEnableBtn);

    row->streamSendSlider.setRange(0.0, 2.0, 0.01);
    row->streamSendSlider.setValue(chState.sendLevel[0].load(), juce::dontSendNotification);
    row->streamSendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    row->streamSendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row->streamSendSlider.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->streamSendSlider);

    // Monitor send
    row->monitorEnableBtn.setToggleState(chState.routeEnable[1].load(), juce::dontSendNotification);
    row->monitorEnableBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->monitorEnableBtn);

    row->monitorSendSlider.setRange(0.0, 2.0, 0.01);
    row->monitorSendSlider.setValue(chState.sendLevel[1].load(), juce::dontSendNotification);
    row->monitorSendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    row->monitorSendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row->monitorSendSlider.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->monitorSendSlider);

    // Aux 1 Send (Reverb)
    row->aux1EnableBtn.setToggleState(chState.auxRouteEnable[0].load(), juce::dontSendNotification);
    row->aux1EnableBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffa78bfa));
    row->aux1EnableBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->aux1EnableBtn);

    row->aux1SendSlider.setRange(0.0, 2.0, 0.01);
    row->aux1SendSlider.setValue(chState.auxSendLevel[0].load(), juce::dontSendNotification);
    row->aux1SendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    row->aux1SendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row->aux1SendSlider.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->aux1SendSlider);

    // Aux 2 Send (Delay)
    row->aux2EnableBtn.setToggleState(chState.auxRouteEnable[1].load(), juce::dontSendNotification);
    row->aux2EnableBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff38bdf8));
    row->aux2EnableBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->aux2EnableBtn);

    row->aux2SendSlider.setRange(0.0, 2.0, 0.01);
    row->aux2SendSlider.setValue(chState.auxSendLevel[1].load(), juce::dontSendNotification);
    row->aux2SendSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    row->aux2SendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row->aux2SendSlider.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->aux2SendSlider);

    // Direct Hardware Output Buttons (HW 1..4)
    for (int h = 0; h < 4; ++h)
    {
        row->hwEnableBtns[(size_t)h].setButtonText(juce::String(h + 1));
        row->hwEnableBtns[(size_t)h].setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffb703));
        row->hwEnableBtns[(size_t)h].setToggleState(chState.hwRouteEnable[(size_t)h].load(), juce::dontSendNotification);
        row->hwEnableBtns[(size_t)h].addListener(this);
        matrixTableContainer->addAndMakeVisible(row->hwEnableBtns[(size_t)h]);
    }

    // Pan
    row->panSlider.setRange(-1.0, 1.0, 0.01);
    row->panSlider.setValue(chState.pan.load(), juce::dontSendNotification);
    row->panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    row->panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row->panSlider.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->panSlider);

    row->gainSlider.setRange(-60.0, 6.0, 0.1);
    row->gainSlider.setValue(juce::Decibels::gainToDecibels(chState.gain.load(), -60.0f),
                             juce::dontSendNotification);
    row->gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    row->gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    row->gainSlider.setTooltip("Channel level");
    row->gainSlider.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->gainSlider);

    row->muteBtn.setClickingTogglesState(true);
    row->muteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    row->muteBtn.setTooltip("Mute channel");
    row->muteBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->muteBtn);

    row->soloBtn.setClickingTogglesState(true);
    row->soloBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffffb703));
    row->soloBtn.setTooltip("Solo channel");
    row->soloBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->soloBtn);

    // Inspect button
    row->inspectBtn.setButtonText("Details");
    row->inspectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    row->inspectBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00f5d4));
    row->inspectBtn.addListener(this);
    matrixTableContainer->addAndMakeVisible(row->inspectBtn);

    rowUIs.push_back(std::move(row));
    setAdvancedRoutingVisible(advancedRoutingVisible);
}

void RoutingMatrixComponent::setSelectedChannel(int ch)
{
    selectedChannelIndex = juce::jlimit(0, matrix.getNumActiveChannels() - 1, ch);

    for (int i = 0; i < maxInputs; ++i)
    {
        channelSelectorTabs[i].setEnabled(i < matrix.getNumActiveChannels());
        if (i == selectedChannelIndex)
        {
            channelSelectorTabs[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f5d4));
            channelSelectorTabs[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0d1117));
        }
        else
        {
            channelSelectorTabs[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2638));
            channelSelectorTabs[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        }
    }

    updateInspectorForSelectedChannel();
}

void RoutingMatrixComponent::setAdvancedRoutingVisible(bool shouldBeVisible)
{
    advancedRoutingVisible = shouldBeVisible;
    advancedRoutingBtn.setToggleState(advancedRoutingVisible, juce::dontSendNotification);
    matrixViewport.setScrollBarsShown(true, advancedRoutingVisible);

    for (auto& row : rowUIs)
    {
        row->outputRouteCombo.setVisible(!advancedRoutingVisible);
        row->gainSlider.setVisible(!advancedRoutingVisible);
        row->muteBtn.setVisible(!advancedRoutingVisible);
        row->soloBtn.setVisible(!advancedRoutingVisible);

        row->stereoModeCombo.setVisible(advancedRoutingVisible);
        row->submixGroupCombo.setVisible(advancedRoutingVisible);
        row->statusBadge.setVisible(advancedRoutingVisible);
        row->streamEnableBtn.setVisible(advancedRoutingVisible);
        row->streamSendSlider.setVisible(advancedRoutingVisible);
        row->monitorEnableBtn.setVisible(advancedRoutingVisible);
        row->monitorSendSlider.setVisible(advancedRoutingVisible);
        row->aux1EnableBtn.setVisible(advancedRoutingVisible);
        row->aux1SendSlider.setVisible(advancedRoutingVisible);
        row->aux2EnableBtn.setVisible(advancedRoutingVisible);
        row->aux2SendSlider.setVisible(advancedRoutingVisible);
        for (auto& hardwareButton : row->hwEnableBtns)
            hardwareButton.setVisible(advancedRoutingVisible);
    }

    repaint();
    resized();
}

void RoutingMatrixComponent::updateInspectorForSelectedChannel()
{
    auto& chState = matrix.getChannel(selectedChannelIndex);

    inspectorChTitle.setText("CHANNEL " + juce::String(selectedChannelIndex + 1) + ": " + chState.name.toUpperCase(), juce::dontSendNotification);

    // 1. Input Source
    auto src = chState.inputSource.load();
    if (src == ChannelInputSource::Hardware) inspectorSourceTypeCombo.setSelectedId(1, juce::dontSendNotification);
    else if (src == ChannelInputSource::Playback) inspectorSourceTypeCombo.setSelectedId(2, juce::dontSendNotification);
    else if (src == ChannelInputSource::TestSignal) inspectorSourceTypeCombo.setSelectedId(3, juce::dontSendNotification);
    else if (src == ChannelInputSource::YouTube) inspectorSourceTypeCombo.setSelectedId(5, juce::dontSendNotification);
    else inspectorSourceTypeCombo.setSelectedId(4, juce::dontSendNotification);

    const int hardwarePortId = chState.hardwareInputChannel.load() + 1;
    if (inspectorHardwarePortCombo.indexOfItemId(hardwarePortId) < 0)
        inspectorHardwarePortCombo.addItem("Hardware Port " + juce::String(hardwarePortId) + " (Unavailable)",
                                           hardwarePortId);
    inspectorHardwarePortCombo.setSelectedId(hardwarePortId, juce::dontSendNotification);
    inspectorStereoModeCombo.setSelectedId((int)chState.stereoMode.load() + 1, juce::dontSendNotification);
    channelEnableToggle.setToggleState(chState.isEnabled.load(), juce::dontSendNotification);

    if (chState.duplicateWarning.load())
        duplicateWarningLabel.setText("⚠️ Warning: Hardware port shared with another channel", juce::dontSendNotification);
    else if (!chState.isSourceAvailable.load())
        duplicateWarningLabel.setText("❌ Warning: Hardware port unavailable on active device", juce::dontSendNotification);
    else
        duplicateWarningLabel.setText("✓ Input source port mapped & valid", juce::dontSendNotification);

    // 2. Sends, Submix & Aux
    inspectorSubmixCombo.setSelectedId(chState.submixGroup.load() + 2, juce::dontSendNotification);

    streamSendEnableToggle.setToggleState(chState.routeEnable[0].load(), juce::dontSendNotification);
    streamPrePostCombo.setSelectedId(chState.preFaderSendStream.load() ? 2 : 1, juce::dontSendNotification);
    float sGain = chState.sendLevel[0].load();
    float sDb = sGain > 0.0001f ? 20.0f * std::log10(sGain) : -60.0f;
    streamSendSlider.setValue(sGain, juce::dontSendNotification);
    streamSendDbLabel.setText(juce::String(sDb, 1) + " dB", juce::dontSendNotification);

    monitorSendEnableToggle.setToggleState(chState.routeEnable[1].load(), juce::dontSendNotification);
    monitorPrePostCombo.setSelectedId(chState.preFaderSendMonitor.load() ? 2 : 1, juce::dontSendNotification);
    float mGain = chState.sendLevel[1].load();
    float mDb = mGain > 0.0001f ? 20.0f * std::log10(mGain) : -60.0f;
    monitorSendSlider.setValue(mGain, juce::dontSendNotification);
    monitorSendDbLabel.setText(juce::String(mDb, 1) + " dB", juce::dontSendNotification);

    // Aux 1 Send (Reverb)
    aux1SendEnableToggle.setToggleState(chState.auxRouteEnable[0].load(), juce::dontSendNotification);
    aux1PrePostCombo.setSelectedId(chState.preFaderAuxSend[0].load() ? 2 : 1, juce::dontSendNotification);
    float a1Gain = chState.auxSendLevel[0].load();
    float a1Db = a1Gain > 0.0001f ? 20.0f * std::log10(a1Gain) : -60.0f;
    aux1SendSlider.setValue(a1Gain, juce::dontSendNotification);
    aux1SendDbLabel.setText(juce::String(a1Db, 1) + " dB", juce::dontSendNotification);

    // Aux 2 Send (Delay)
    aux2SendEnableToggle.setToggleState(chState.auxRouteEnable[1].load(), juce::dontSendNotification);
    aux2PrePostCombo.setSelectedId(chState.preFaderAuxSend[1].load() ? 2 : 1, juce::dontSendNotification);
    float a2Gain = chState.auxSendLevel[1].load();
    float a2Db = a2Gain > 0.0001f ? 20.0f * std::log10(a2Gain) : -60.0f;
    aux2SendSlider.setValue(a2Gain, juce::dontSendNotification);
    aux2SendDbLabel.setText(juce::String(a2Db, 1) + " dB", juce::dontSendNotification);

    // Direct Hardware Output distribution controls update
    for (int h = 0; h < 4; ++h)
    {
        bool avail = matrix.isHardwareOutputAvailable(h);
        inspectorHwEnableToggles[(size_t)h].setEnabled(avail);
        inspectorHwSendSliders[(size_t)h].setEnabled(avail);
        inspectorHwEnableToggles[(size_t)h].setToggleState(chState.hwRouteEnable[(size_t)h].load(), juce::dontSendNotification);
        float hGain = chState.hwSendLevel[(size_t)h].load();
        inspectorHwSendSliders[(size_t)h].setValue(hGain, juce::dontSendNotification);
        float hDb = hGain > 0.0001f ? 20.0f * std::log10(hGain) : -60.0f;
        inspectorHwSendLabels[(size_t)h].setText(avail ? (juce::String(hDb, 1) + " dB") : "N/A", juce::dontSendNotification);
    }

    // 3. Pan, Gain & Status
    panKnob.setValue(chState.pan.load(), juce::dontSendNotification);
    float p = chState.pan.load();
    if (std::abs(p) < 0.05f) panValueLabel.setText("C (0%)", juce::dontSendNotification);
    else if (p < 0.0f) panValueLabel.setText("L " + juce::String((int)(-p * 100.0f)) + "%", juce::dontSendNotification);
    else panValueLabel.setText("R " + juce::String((int)(p * 100.0f)) + "%", juce::dontSendNotification);

    channelGainSlider.setValue(chState.gain.load(), juce::dontSendNotification);
    float gDb = 20.0f * std::log10(juce::jmax(0.0001f, chState.gain.load()));
    gainValueLabel.setText(juce::String(gDb, 1) + " dB", juce::dontSendNotification);

    muteBtn.setToggleState(chState.mute.load(), juce::dontSendNotification);
    soloBtn.setToggleState(chState.solo.load(), juce::dontSendNotification);

    juce::String submixName = (chState.submixGroup.load() >= 0)
        ? matrix.getSubmixBus(chState.submixGroup.load()).name
        : "Direct Master";

    statusDetailLabel.setText("Submix: " + submixName +
                              " | Stream (" + juce::String(chState.routeEnable[0].load() ? "ON" : "OFF") +
                              ") | Monitor (" + juce::String(chState.routeEnable[1].load() ? "ON" : "OFF") +
                              ") | Aux1 (" + juce::String(chState.auxRouteEnable[0].load() ? "ON" : "OFF") +
                              ") | Aux2 (" + juce::String(chState.auxRouteEnable[1].load() ? "ON" : "OFF") + ")", juce::dontSendNotification);
}

void RoutingMatrixComponent::createRoutingGraph()
{
    if (routingGraph != nullptr)
        return;

    routingGraph = std::make_unique<RoutingGraphComponent>(
        matrix,
        [this](int channelIndex)
        {
            setSelectedChannel(channelIndex);
            if (onChannelSelected)
                onChannelSelected(channelIndex);
        },
        [this]
        {
            if (signalFlowWindow == nullptr)
                signalFlowWindow = std::make_unique<SignalFlowWindow>(matrix, busMutedProvider);
            signalFlowWindow->setVisible(true);
            signalFlowWindow->toFront(true);
        },
        [this](juce::Component* menuTarget)
        {
            if (onAddInputNodeRequested)
                onAddInputNodeRequested(menuTarget);
        },
        [this](int targetIndex)
        {
            if (onDSPNodeSelected)
                onDSPNodeSelected(targetIndex);
        },
        [this](int targetIndex)
        {
            if (onVSTChainSelected)
                onVSTChainSelected(targetIndex);
        },
        vstChainPluginsProvider);
    routingGraph->setMasterBusControlCallbacks(masterBusControlCallbacks);
    routingGraph->setDSPControlCallbacks(dspControlCallbacks);
    routingGraph->setPaletteActionCallback([this](RoutingGraphPaletteAction action)
    {
        if (onPaletteActionRequested)
            onPaletteActionRequested(action);
    });
    routingGraph->setBounds(getLocalBounds().reduced(8).withTrimmedTop(42));
    addAndMakeVisible(*routingGraph);
    if (pendingRoutingGraphLayoutXml.isNotEmpty() &&
        !routingGraph->restoreLayout(pendingRoutingGraphLayoutXml))
        pendingRoutingGraphLayoutXml.clear();
    routingGraph->toFront(false);
}

void RoutingMatrixComponent::focusRoutingGraphNode(const juce::String& typeId, int index)
{
    if (routingGraph != nullptr)
        routingGraph->focusNode(typeId, index);
}

void RoutingMatrixComponent::refreshRoutingGraph()
{
    if (routingGraph != nullptr)
        routingGraph->refresh();
}

void RoutingMatrixComponent::showRoutingGraph()
{
    signalFlowBtn.setToggleState(true, juce::dontSendNotification);
    createRoutingGraph();
    routingGraph->setVisible(true);
    routingGraph->refresh();
    routingGraph->toFront(false);
}

juce::String RoutingMatrixComponent::getRoutingGraphLayoutXml() const
{
    if (routingGraph != nullptr)
        return routingGraph->serializeLayout();
    if (pendingRoutingGraphLayoutXml.isEmpty())
        return {};

    auto layout = juce::XmlDocument::parse(pendingRoutingGraphLayoutXml);
    if (layout == nullptr)
        return pendingRoutingGraphLayoutXml;

    layout->setAttribute("version", 2);
    if (auto* existingConnections = layout->getChildByName("CONNECTIONS"))
        layout->removeChildElement(existingConnections, true);
    layout->addChildElement(serializeRoutingGraphConnections(matrix).release());
    return layout->toString();
}

void RoutingMatrixComponent::setRoutingGraphLayoutXml(const juce::String& xml)
{
    pendingRoutingGraphLayoutXml = xml;
    if (routingGraph != nullptr && !routingGraph->restoreLayout(xml))
        pendingRoutingGraphLayoutXml.clear();
}

void RoutingMatrixComponent::buttonClicked(juce::Button* b)
{
    if (b == &advancedRoutingBtn)
    {
        setAdvancedRoutingVisible(advancedRoutingBtn.getToggleState());
        return;
    }

    if (b == &signalFlowBtn)
    {
        if (signalFlowBtn.getToggleState())
        {
            createRoutingGraph();
            routingGraph->setVisible(true);
            routingGraph->refresh();
            routingGraph->toFront(false);
        }
        else if (routingGraph != nullptr)
            routingGraph->setVisible(false);
        return;
    }

    if (b == &applyPresetBtn)
    {
        applySelectedPreset();
        return;
    }
    if (b == &resetRoutesBtn)
    {
        resetAllRoutingToDefaults();
        return;
    }
    if (b == &copyToAllBtn)
    {
        copySelectedChannelRoutingToAll();
        return;
    }

    // Channel selector tabs in inspector
    for (int i = 0; i < maxInputs; ++i)
    {
        if (b == &channelSelectorTabs[i])
        {
            setSelectedChannel(i);
            if (onChannelSelected)
                onChannelSelected(selectedChannelIndex);
            return;
        }
    }

    // Inspect buttons in matrix table
    for (size_t i = 0; i < rowUIs.size(); ++i)
    {
        if (b == &rowUIs[i]->inspectBtn)
        {
            setSelectedChannel((int)i);
            if (!advancedRoutingVisible)
                setAdvancedRoutingVisible(true);
            if (onChannelSelected)
                onChannelSelected(selectedChannelIndex);
            return;
        }
        if (b == &rowUIs[i]->muteBtn)
        {
            auto& state = matrix.getChannel((int)i);
            state.mute.store(rowUIs[i]->muteBtn.getToggleState());
            if ((int)i == selectedChannelIndex)
                muteBtn.setToggleState(rowUIs[i]->muteBtn.getToggleState(), juce::dontSendNotification);
            return;
        }
        if (b == &rowUIs[i]->soloBtn)
        {
            auto& state = matrix.getChannel((int)i);
            state.solo.store(rowUIs[i]->soloBtn.getToggleState());
            if ((int)i == selectedChannelIndex)
                soloBtn.setToggleState(rowUIs[i]->soloBtn.getToggleState(), juce::dontSendNotification);
            return;
        }
        if (b == &rowUIs[i]->streamEnableBtn)
        {
            matrix.getChannel((int)i).routeEnable[0].store(rowUIs[i]->streamEnableBtn.getToggleState());
            if ((int)i == selectedChannelIndex)
                streamSendEnableToggle.setToggleState(rowUIs[i]->streamEnableBtn.getToggleState(), juce::dontSendNotification);
            return;
        }
        if (b == &rowUIs[i]->monitorEnableBtn)
        {
            matrix.getChannel((int)i).routeEnable[1].store(rowUIs[i]->monitorEnableBtn.getToggleState());
            if ((int)i == selectedChannelIndex)
                monitorSendEnableToggle.setToggleState(rowUIs[i]->monitorEnableBtn.getToggleState(), juce::dontSendNotification);
            return;
        }
        if (b == &rowUIs[i]->aux1EnableBtn)
        {
            matrix.getChannel((int)i).auxRouteEnable[0].store(rowUIs[i]->aux1EnableBtn.getToggleState());
            if ((int)i == selectedChannelIndex)
                aux1SendEnableToggle.setToggleState(rowUIs[i]->aux1EnableBtn.getToggleState(), juce::dontSendNotification);
            return;
        }
        if (b == &rowUIs[i]->aux2EnableBtn)
        {
            matrix.getChannel((int)i).auxRouteEnable[1].store(rowUIs[i]->aux2EnableBtn.getToggleState());
            if ((int)i == selectedChannelIndex)
                aux2SendEnableToggle.setToggleState(rowUIs[i]->aux2EnableBtn.getToggleState(), juce::dontSendNotification);
            return;
        }
        for (int h = 0; h < 4; ++h)
        {
            if (b == &rowUIs[i]->hwEnableBtns[(size_t)h])
            {
                bool st = rowUIs[i]->hwEnableBtns[(size_t)h].getToggleState();
                matrix.getChannel((int)i).hwRouteEnable[(size_t)h].store(st);
                if ((int)i == selectedChannelIndex)
                    inspectorHwEnableToggles[(size_t)h].setToggleState(st, juce::dontSendNotification);
                return;
            }
        }
    }

    // Inspector buttons
    auto& chState = matrix.getChannel(selectedChannelIndex);
    if (b == &channelEnableToggle)
    {
        chState.isEnabled.store(channelEnableToggle.getToggleState());
    }
    else if (b == &streamSendEnableToggle)
    {
        chState.routeEnable[0].store(streamSendEnableToggle.getToggleState());
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->streamEnableBtn.setToggleState(streamSendEnableToggle.getToggleState(), juce::dontSendNotification);
    }
    else if (b == &monitorSendEnableToggle)
    {
        chState.routeEnable[1].store(monitorSendEnableToggle.getToggleState());
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->monitorEnableBtn.setToggleState(monitorSendEnableToggle.getToggleState(), juce::dontSendNotification);
    }
    else if (b == &aux1SendEnableToggle)
    {
        chState.auxRouteEnable[0].store(aux1SendEnableToggle.getToggleState());
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->aux1EnableBtn.setToggleState(aux1SendEnableToggle.getToggleState(), juce::dontSendNotification);
    }
    else if (b == &aux2SendEnableToggle)
    {
        chState.auxRouteEnable[1].store(aux2SendEnableToggle.getToggleState());
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->aux2EnableBtn.setToggleState(aux2SendEnableToggle.getToggleState(), juce::dontSendNotification);
    }
    else if (b == &muteBtn)
    {
        chState.mute.store(muteBtn.getToggleState());
    }
    else if (b == &soloBtn)
    {
        chState.solo.store(soloBtn.getToggleState());
    }
    else if (b == &aux1MuteBtn)
    {
        matrix.getAuxBus(0).mute.store(aux1MuteBtn.getToggleState());
    }
    else if (b == &aux2MuteBtn)
    {
        matrix.getAuxBus(1).mute.store(aux2MuteBtn.getToggleState());
    }
    else if (b == &submix1MuteBtn)
    {
        matrix.getSubmixBus(0).mute.store(submix1MuteBtn.getToggleState());
    }
    else if (b == &submix2MuteBtn)
    {
        matrix.getSubmixBus(1).mute.store(submix2MuteBtn.getToggleState());
    }
    else
    {
        for (int h = 0; h < 4; ++h)
        {
            if (b == &inspectorHwEnableToggles[(size_t)h])
            {
                bool st = inspectorHwEnableToggles[(size_t)h].getToggleState();
                chState.hwRouteEnable[(size_t)h].store(st);
                if (selectedChannelIndex < (int)rowUIs.size())
                    rowUIs[(size_t)selectedChannelIndex]->hwEnableBtns[(size_t)h].setToggleState(st, juce::dontSendNotification);
                return;
            }
        }
    }
}

void RoutingMatrixComponent::sliderValueChanged(juce::Slider* s)
{
    // Matrix table sliders
    for (size_t i = 0; i < rowUIs.size(); ++i)
    {
        if (s == &rowUIs[i]->gainSlider)
        {
            const float gain = juce::Decibels::decibelsToGain((float)s->getValue());
            matrix.getChannel((int)i).gain.store(gain);
            if ((int)i == selectedChannelIndex)
            {
                channelGainSlider.setValue(gain, juce::dontSendNotification);
                gainValueLabel.setText(juce::String(s->getValue(), 1) + " dB",
                                       juce::dontSendNotification);
            }
            return;
        }

        if (s == &rowUIs[i]->streamSendSlider)
        {
            matrix.getChannel((int)i).sendLevel[0].store((float)s->getValue());
            if ((int)i == selectedChannelIndex)
            {
                streamSendSlider.setValue(s->getValue(), juce::dontSendNotification);
                float db = s->getValue() > 0.0001 ? 20.0f * std::log10((float)s->getValue()) : -60.0f;
                streamSendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
            }
            return;
        }
        if (s == &rowUIs[i]->monitorSendSlider)
        {
            matrix.getChannel((int)i).sendLevel[1].store((float)s->getValue());
            if ((int)i == selectedChannelIndex)
            {
                monitorSendSlider.setValue(s->getValue(), juce::dontSendNotification);
                float db = s->getValue() > 0.0001 ? 20.0f * std::log10((float)s->getValue()) : -60.0f;
                monitorSendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
            }
            return;
        }
        if (s == &rowUIs[i]->aux1SendSlider)
        {
            matrix.getChannel((int)i).auxSendLevel[0].store((float)s->getValue());
            if ((int)i == selectedChannelIndex)
            {
                aux1SendSlider.setValue(s->getValue(), juce::dontSendNotification);
                float db = s->getValue() > 0.0001 ? 20.0f * std::log10((float)s->getValue()) : -60.0f;
                aux1SendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
            }
            return;
        }
        if (s == &rowUIs[i]->aux2SendSlider)
        {
            matrix.getChannel((int)i).auxSendLevel[1].store((float)s->getValue());
            if ((int)i == selectedChannelIndex)
            {
                aux2SendSlider.setValue(s->getValue(), juce::dontSendNotification);
                float db = s->getValue() > 0.0001 ? 20.0f * std::log10((float)s->getValue()) : -60.0f;
                aux2SendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
            }
            return;
        }
        if (s == &rowUIs[i]->panSlider)
        {
            matrix.getChannel((int)i).pan.store((float)s->getValue());
            if ((int)i == selectedChannelIndex)
            {
                panKnob.setValue(s->getValue(), juce::dontSendNotification);
                float p = (float)s->getValue();
                if (std::abs(p) < 0.05f) panValueLabel.setText("C (0%)", juce::dontSendNotification);
                else if (p < 0.0f) panValueLabel.setText("L " + juce::String((int)(-p * 100.0f)) + "%", juce::dontSendNotification);
                else panValueLabel.setText("R " + juce::String((int)(p * 100.0f)) + "%", juce::dontSendNotification);
            }
            return;
        }
    }

    // Section 4 Masters
    if (s == &aux1ReturnSlider)
    {
        float val = (float)s->getValue();
        matrix.getAuxBus(0).returnLevel.store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        aux1ReturnDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        return;
    }
    if (s == &aux1RoomSizeSlider)
    {
        matrix.getAuxBus(0).fxParam1.store((float)s->getValue());
        return;
    }
    if (s == &aux2ReturnSlider)
    {
        float val = (float)s->getValue();
        matrix.getAuxBus(1).returnLevel.store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        aux2ReturnDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        return;
    }
    if (s == &aux2DelayTimeSlider)
    {
        matrix.getAuxBus(1).fxParam1.store((float)s->getValue());
        return;
    }
    if (s == &submix1FaderSlider)
    {
        float val = (float)s->getValue();
        matrix.getSubmixBus(0).gain.store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        submix1DbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        return;
    }
    if (s == &submix2FaderSlider)
    {
        float val = (float)s->getValue();
        matrix.getSubmixBus(1).gain.store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        submix2DbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        return;
    }

    // Inspector sliders
    auto& chState = matrix.getChannel(selectedChannelIndex);
    if (s == &streamSendSlider)
    {
        float val = (float)s->getValue();
        chState.sendLevel[0].store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        streamSendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->streamSendSlider.setValue(val, juce::dontSendNotification);
    }
    else if (s == &monitorSendSlider)
    {
        float val = (float)s->getValue();
        chState.sendLevel[1].store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        monitorSendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->monitorSendSlider.setValue(val, juce::dontSendNotification);
    }
    else if (s == &aux1SendSlider)
    {
        float val = (float)s->getValue();
        chState.auxSendLevel[0].store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        aux1SendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->aux1SendSlider.setValue(val, juce::dontSendNotification);
    }
    else if (s == &aux2SendSlider)
    {
        float val = (float)s->getValue();
        chState.auxSendLevel[1].store(val);
        float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
        aux2SendDbLabel.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->aux2SendSlider.setValue(val, juce::dontSendNotification);
    }
    else if (s == &panKnob)
    {
        float val = (float)s->getValue();
        chState.pan.store(val);
        if (std::abs(val) < 0.05f) panValueLabel.setText("C (0%)", juce::dontSendNotification);
        else if (val < 0.0f) panValueLabel.setText("L " + juce::String((int)(-val * 100.0f)) + "%", juce::dontSendNotification);
        else panValueLabel.setText("R " + juce::String((int)(val * 100.0f)) + "%", juce::dontSendNotification);

        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->panSlider.setValue(val, juce::dontSendNotification);
    }
    else if (s == &channelGainSlider)
    {
        float val = (float)s->getValue();
        chState.gain.store(val);
        float gDb = 20.0f * std::log10(juce::jmax(0.0001f, val));
        gainValueLabel.setText(juce::String(gDb, 1) + " dB", juce::dontSendNotification);
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->gainSlider.setValue(
                juce::Decibels::gainToDecibels(val, -60.0f), juce::dontSendNotification);
    }
    else
    {
        for (int h = 0; h < 4; ++h)
        {
            if (s == &inspectorHwSendSliders[(size_t)h])
            {
                float val = (float)s->getValue();
                chState.hwSendLevel[(size_t)h].store(val);
                float db = val > 0.0001f ? 20.0f * std::log10(val) : -60.0f;
                inspectorHwSendLabels[(size_t)h].setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
                return;
            }
        }
    }
}

void RoutingMatrixComponent::comboBoxChanged(juce::ComboBox* c)
{
    // Matrix table row combos
    for (size_t i = 0; i < rowUIs.size(); ++i)
    {
        if (c == &rowUIs[i]->outputRouteCombo)
        {
            auto& channel = matrix.getChannel((int)i);
            const int selectedId = c->getSelectedId();
            if (selectedId >= 1 && selectedId <= 4)
            {
                const bool toStream = selectedId == 1 || selectedId == 2;
                const bool toMonitor = selectedId == 1 || selectedId == 3;
                channel.submixGroup.store(-1);
                channel.routeEnable[0].store(toStream);
                channel.routeEnable[1].store(toMonitor);
                rowUIs[i]->streamEnableBtn.setToggleState(toStream, juce::dontSendNotification);
                rowUIs[i]->monitorEnableBtn.setToggleState(toMonitor, juce::dontSendNotification);
            }
            else if (selectedId >= 5 && selectedId <= 8)
            {
                channel.submixGroup.store(selectedId - 5);
                rowUIs[i]->submixGroupCombo.setSelectedId(selectedId - 3, juce::dontSendNotification);
            }

            if ((int)i == selectedChannelIndex)
                updateInspectorForSelectedChannel();
            return;
        }

        if (c == &rowUIs[i]->inputSourceCombo)
        {
            auto& ch = matrix.getChannel((int)i);
            int id = c->getSelectedId();
            if (id >= 101 && id < 500)
            {
                ch.inputSource.store(ChannelInputSource::Hardware);
                ch.hardwareInputChannel.store(id - 101);
            }
            else if (id >= 500)
            {
                ch.inputSource.store(ChannelInputSource::Hardware);
                ch.hardwareInputChannel.store(id - 500);
            }
            else if (id >= 1 && id <= 4)
            {
                ch.inputSource.store(ChannelInputSource::Hardware);
                ch.hardwareInputChannel.store(id - 1);
            }
            else if (id >= 5 && id <= 7)
            {
                ch.inputSource.store(ChannelInputSource::Playback);
                ch.playbackChannel.store(id - 5);
            }
            else if (id == 8)
            {
                ch.inputSource.store(ChannelInputSource::TestSignal);
            }
            else if (id == 10)
            {
                ch.inputSource.store(ChannelInputSource::YouTube);
            }
            else
            {
                ch.inputSource.store(ChannelInputSource::Disabled);
            }
            matrix.validateAvailableInputs(matrix.getNumAvailableHardwareInputs());
            matrix.updateDuplicateWarnings();
            if ((int)i == selectedChannelIndex)
                updateInspectorForSelectedChannel();
            return;
        }
        if (c == &rowUIs[i]->stereoModeCombo)
        {
            matrix.setChannelStereoMode((int)i, (ChannelStereoMode)(c->getSelectedId() - 1));
            if ((int)i == selectedChannelIndex)
                inspectorStereoModeCombo.setSelectedId(c->getSelectedId(), juce::dontSendNotification);
            return;
        }
        if (c == &rowUIs[i]->submixGroupCombo)
        {
            auto& ch = matrix.getChannel((int)i);
            ch.submixGroup.store(c->getSelectedId() - 2);
            if ((int)i == selectedChannelIndex)
                inspectorSubmixCombo.setSelectedId(c->getSelectedId(), juce::dontSendNotification);
            return;
        }
    }

    // Inspector combos
    auto& chState = matrix.getChannel(selectedChannelIndex);
    if (c == &inspectorSourceTypeCombo)
    {
        int typeId = c->getSelectedId();
        if (typeId == 1) chState.inputSource.store(ChannelInputSource::Hardware);
        else if (typeId == 2) chState.inputSource.store(ChannelInputSource::Playback);
        else if (typeId == 3) chState.inputSource.store(ChannelInputSource::TestSignal);
        else if (typeId == 5) chState.inputSource.store(ChannelInputSource::YouTube);
        else chState.inputSource.store(ChannelInputSource::Disabled);
        matrix.validateAvailableInputs(matrix.getNumAvailableHardwareInputs());
        matrix.updateDuplicateWarnings();
    }
    else if (c == &inspectorHardwarePortCombo)
    {
        chState.hardwareInputChannel.store(c->getSelectedId() - 1);
        chState.inputSource.store(ChannelInputSource::Hardware);
        matrix.validateAvailableInputs(matrix.getNumAvailableHardwareInputs());
        matrix.updateDuplicateWarnings();
    }
    else if (c == &inspectorStereoModeCombo)
    {
        matrix.setChannelStereoMode(selectedChannelIndex, (ChannelStereoMode)(c->getSelectedId() - 1));
    }
    else if (c == &inspectorSubmixCombo)
    {
        int sm = c->getSelectedId() - 2;
        chState.submixGroup.store(sm);
        if (selectedChannelIndex < (int)rowUIs.size())
            rowUIs[(size_t)selectedChannelIndex]->submixGroupCombo.setSelectedId(c->getSelectedId(), juce::dontSendNotification);
    }
    else if (c == &streamPrePostCombo)
    {
        chState.preFaderSendStream.store(c->getSelectedId() == 2);
    }
    else if (c == &monitorPrePostCombo)
    {
        chState.preFaderSendMonitor.store(c->getSelectedId() == 2);
    }
    else if (c == &aux1PrePostCombo)
    {
        chState.preFaderAuxSend[0].store(c->getSelectedId() == 2);
    }
    else if (c == &aux2PrePostCombo)
    {
        chState.preFaderAuxSend[1].store(c->getSelectedId() == 2);
    }
}

void RoutingMatrixComponent::textEditorFocusLost(juce::TextEditor& ed)
{
    for (size_t i = 0; i < rowUIs.size(); ++i)
    {
        if (&ed == &rowUIs[i]->nameEditor)
        {
            matrix.getChannel((int)i).name = ed.getText();
            if ((int)i == selectedChannelIndex)
                inspectorChTitle.setText("CHANNEL " + juce::String((int)i + 1) + ": " + ed.getText().toUpperCase(), juce::dontSendNotification);
            break;
        }
    }
}

void RoutingMatrixComponent::textEditorReturnKeyPressed(juce::TextEditor& ed)
{
    textEditorFocusLost(ed);
}

void RoutingMatrixComponent::applySelectedPreset()
{
    int presetIdx = presetCombo.getSelectedId() - 1;
    if (presetIdx >= 0)
    {
        matrix.applyRoutingPreset(presetIdx);
        refreshFromMatrix();
    }
}

void RoutingMatrixComponent::copySelectedChannelRoutingToAll()
{
    const auto& src = matrix.getChannel(selectedChannelIndex);
    int numActive = matrix.getNumActiveChannels();

    for (int i = 0; i < numActive; ++i)
    {
        if (i == selectedChannelIndex) continue;
        auto& dest = matrix.getChannel(i);
        dest.submixGroup.store(src.submixGroup.load());

        dest.routeEnable[0].store(src.routeEnable[0].load());
        dest.sendLevel[0].store(src.sendLevel[0].load());
        dest.preFaderSendStream.store(src.preFaderSendStream.load());

        dest.routeEnable[1].store(src.routeEnable[1].load());
        dest.sendLevel[1].store(src.sendLevel[1].load());
        dest.preFaderSendMonitor.store(src.preFaderSendMonitor.load());

        for (int a = 0; a < 4; ++a)
        {
            dest.auxRouteEnable[(size_t)a].store(src.auxRouteEnable[(size_t)a].load());
            dest.auxSendLevel[(size_t)a].store(src.auxSendLevel[(size_t)a].load());
            dest.preFaderAuxSend[(size_t)a].store(src.preFaderAuxSend[(size_t)a].load());
        }

        for (int h = 0; h < 4; ++h)
        {
            dest.hwRouteEnable[(size_t)h].store(src.hwRouteEnable[(size_t)h].load());
            dest.hwSendLevel[(size_t)h].store(src.hwSendLevel[(size_t)h].load());
            dest.preFaderHwSend[(size_t)h].store(src.preFaderHwSend[(size_t)h].load());
        }
    }
    refreshFromMatrix();
}

void RoutingMatrixComponent::resetAllRoutingToDefaults()
{
    matrix.applyRoutingPreset(0);
    refreshFromMatrix();
}

void RoutingMatrixComponent::updateAvailableHardwareInputs(int count, const juce::StringArray& inputNames)
{
    numHardwareInputsAvailable = juce::jmax(0, count);
    hardwareInputChannelNames = inputNames;
    matrix.validateAvailableInputs(count);

    inspectorHardwarePortCombo.clear(juce::dontSendNotification);
    for (int p = 0; p < numHardwareInputsAvailable; ++p)
    {
        const auto name = p < hardwareInputChannelNames.size() && hardwareInputChannelNames[p].isNotEmpty()
            ? hardwareInputChannelNames[p] : "Hardware Port " + juce::String(p + 1);
        inspectorHardwarePortCombo.addItem(name, p + 1);
    }
    if (numHardwareInputsAvailable == 0)
        inspectorHardwarePortCombo.addItem("<< No Hardware Inputs >>", 1);
    refreshFromMatrix();
}

void RoutingMatrixComponent::updateAvailableHardwareOutputs(int count, const juce::StringArray& outputNames)
{
    numHardwareOutputsAvailable = juce::jmax(0, count);
    hardwareOutputChannelNames = outputNames;
    matrix.validateAvailableOutputs(count);

    // Refresh UI to update enabled state of HW output buttons/sliders
    for (size_t i = 0; i < rowUIs.size(); ++i)
    {
        for (int h = 0; h < 4; ++h)
        {
            bool avail = matrix.isHardwareOutputAvailable(h);
            rowUIs[i]->hwEnableBtns[(size_t)h].setEnabled(avail);
            if (!avail)
                rowUIs[i]->hwEnableBtns[(size_t)h].setButtonText("N/A");
            else
                rowUIs[i]->hwEnableBtns[(size_t)h].setButtonText(rowUIs[i]->hwEnableBtns[(size_t)h].getToggleState() ? "ON" : "OFF");
        }
    }

    for (int h = 0; h < 4; ++h)
    {
        bool avail = matrix.isHardwareOutputAvailable(h);
        inspectorHwEnableToggles[(size_t)h].setEnabled(avail);
        inspectorHwSendSliders[(size_t)h].setEnabled(avail);
        if (!avail)
            inspectorHwSendLabels[(size_t)h].setText("N/A", juce::dontSendNotification);
    }
}

void RoutingMatrixComponent::timerCallback()
{
    syncFromMatrix();
    if (routingGraph != nullptr && routingGraph->isVisible())
        routingGraph->refresh();

    // Update global warning banner
    if (matrix.hasAnyDuplicateWarning())
    {
        warningBanner.setText("⚠ Duplicate hardware input", juce::dontSendNotification);
        warningBanner.setColour(juce::Label::textColourId, juce::Colour(0xffffb703));
        warningBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ffb703));
    }
    else
    {
        warningBanner.setText("✓ Sources and buses valid", juce::dontSendNotification);
        warningBanner.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        warningBanner.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    }

    // Update status badges on rows
    int active = matrix.getNumActiveChannels();
    for (int i = 0; i < active && i < (int)rowUIs.size(); ++i)
    {
        auto& ch = matrix.getChannel(i);
        auto& row = *rowUIs[(size_t)i];

        // Ensure HW button text reflects current toggle and availability state
        for (int h = 0; h < 4; ++h)
        {
            bool avail = matrix.isHardwareOutputAvailable(h);
            row.hwEnableBtns[(size_t)h].setEnabled(avail);
            if (!avail)
                row.hwEnableBtns[(size_t)h].setButtonText("N/A");
            else
                row.hwEnableBtns[(size_t)h].setButtonText(row.hwEnableBtns[(size_t)h].getToggleState() ? "ON" : "OFF");
        }

        if (!ch.isEnabled.load())
        {
            row.statusBadge.setText("DISABLED", juce::dontSendNotification);
            row.statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
            row.statusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x1194a3b8));
        }
        else if (ch.duplicateWarning.load())
        {
            row.statusBadge.setText("SHARED", juce::dontSendNotification);
            row.statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffffb703));
            row.statusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ffb703));
        }
        else if (!ch.isSourceAvailable.load())
        {
            row.statusBadge.setText("UNAVAIL", juce::dontSendNotification);
            row.statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
            row.statusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ef4444));
        }
        else
        {
            row.statusBadge.setText("ACTIVE", juce::dontSendNotification);
            row.statusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
            row.statusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
        }
    }
}

} // namespace LiveBus
