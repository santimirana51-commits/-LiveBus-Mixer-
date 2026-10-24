#pragma once
#include <JuceHeader.h>
#include "AudioDeviceService.h"

namespace LiveBus
{

/**
 * Vertical Segmented LED Input Meter Component.
 * Emulates the segmented LED ladder next to the Audio Input Device dropdown in the reference design.
 */
class SegmentedInputMeterComponent : public juce::Component
{
public:
    SegmentedInputMeterComponent();
    void setLevelDb(float peakDb, float rmsDb);

    void paint(juce::Graphics& g) override;

private:
    float currentPeakDb = -100.0f;
    float currentRmsDb = -100.0f;
    static constexpr int numSegments = 8;
};

/**
 * Custom Channel Checkbox List Component.
 * Renders scrollable recessed panel with checkboxes for each hardware audio channel.
 */
class ChannelListBoxComponent : public juce::Component
{
public:
    ChannelListBoxComponent(bool isOutput, AudioDeviceService& devService);

    void refreshChannels();
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    bool isOutputList;
    AudioDeviceService& service;

    juce::Viewport viewport;
    juce::Component contentContainer;
    juce::OwnedArray<juce::ToggleButton> channelCheckboxes;
};

/**
 * Professional Audio Settings Component.
 *
 * Implements the exact dark hardware layout from the reference image:
 * - Audio device type dropdown
 * - Audio Output Device dropdown + Test button
 * - Audio Input Device dropdown + live segmented LED input meter
 * - Audio Output Channels checkbox list (in recessed dark box)
 * - Audio Input Channels checkbox list (in recessed dark box)
 * - Sample Rate dropdown
 * - Audio Buffer Size dropdown (samples + buffer duration ms)
 * - Real-time stability status badge, diagnostics, and retry controls
 */
class AudioSettingsComponent : public juce::Component,
                               public juce::Timer,
                               public AudioDeviceServiceListener
{
public:
    explicit AudioSettingsComponent(AudioDeviceService& devService);
    ~AudioSettingsComponent() override;

    void updateUIFromDevice();
    void setCloseButtonVisible(bool visible);
    std::function<void()> onCloseClicked;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // AudioDeviceServiceListener callbacks
    void onDeviceStateChanged(DeviceState newState, const juce::String& message) override;
    void onDeviceSetupChanged() override;

private:
    void timerCallback() override;

    AudioDeviceService& service;
    bool isUpdatingUI = false;

    // Header
    juce::Label titleLabel{"title", "Audio Settings"};
    juce::TextButton closeBtn{"\xc3\x97"}; // Unicode multiplication sign for 'x'

    // Form Row Labels
    juce::Label deviceTypeLabel{"devTypeLbl", "Audio device type:"};
    juce::Label outputDeviceLabel{"outDevLbl", "Audio Output Device:"};
    juce::Label inputDeviceLabel{"inDevLbl", "Audio Input Device:"};
    juce::Label outputChannelsLabel{"outChLbl", "Audio Output Channels:"};
    juce::Label inputChannelsLabel{"inChLbl", "Audio Input Channels:"};
    juce::Label sampleRateLabel{"srLbl", "Sample Rate:"};
    juce::Label bufferSizeLabel{"bufLbl", "Audio Buffer Size:"};

    // Form Controls
    juce::ComboBox deviceTypeCombo;
    juce::ComboBox outputDeviceCombo;
    juce::TextButton testToneBtn{"Test"};

    juce::ComboBox inputDeviceCombo;
    SegmentedInputMeterComponent inputMeter;

    std::unique_ptr<ChannelListBoxComponent> outputChannelsBox;
    std::unique_ptr<ChannelListBoxComponent> inputChannelsBox;

    juce::ComboBox sampleRateCombo;
    juce::ComboBox bufferSizeCombo;

    // Latency & Diagnostics Footer
    juce::Label latencyNoteLabel;
    juce::Label statusBadgeLabel;
    juce::Label statusDetailsLabel;
    juce::TextButton retryBtn{"Retry Reconnection"};
    juce::TextButton asioControlBtn{"Open ASIO Control Panel"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsComponent)
};

/**
 * Modal Dialog window to pop up Audio Settings from any tab or header.
 */
class AudioSettingsDialog : public juce::DialogWindow
{
public:
    explicit AudioSettingsDialog(AudioDeviceService& devService);
    void closeButtonPressed() override;
};

} // namespace LiveBus
