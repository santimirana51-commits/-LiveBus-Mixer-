#pragma once
#include <JuceHeader.h>
#include "OBSIntegrationService.h"
#include "OBSWebSocketClient.h"
#include "OBSGuideComponent.h"
#include "RoutingMatrix.h"
#include "LevelMeterProcessor.h"
#include "AudioDeviceService.h"

namespace LiveBus
{

/**
 * Custom LookAndFeel for OBS Studio style vertical fader sliders.
 */
class OBSFaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OBSFaderLookAndFeel();
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override;
};

/**
 * Single Channel Strip component styled exactly after the OBS Studio Audio Mixer dock.
 */
class OBSChannelStrip : public juce::Component
{
public:
    OBSChannelStrip(const juce::String& name, const juce::String& badgeText, juce::Colour badgeCol);
    ~OBSChannelStrip() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setLevelMeter(float peakL, float peakR, float rmsL, float rmsR);
    void setGainDb(float gainDb);
    void setChannelName(const juce::String& name);
    float getGainDb() const;

    juce::Slider fader;
    juce::TextButton muteBtn;
    juce::TextButton monitorBtn;
    juce::TextButton gearBtn;

    std::function<void(float newGainLinear)> onGainChanged;
    std::function<void(bool isMuted)> onMuteToggled;

private:
    juce::String channelName;
    juce::String badge;
    juce::Colour badgeColour;
    juce::Label nameLabel;
    juce::Label dbReadoutLabel;

    float currentPeakL = 0.0f;
    float currentPeakR = 0.0f;
    float peakHoldL = 0.0f;
    float peakHoldR = 0.0f;
    int peakHoldFrames = 0;

    void drawOBSMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float peakVal, float peakHoldVal);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OBSChannelStrip)
};

/**
 * Professional OBS Studio Integration & Audio Mixer Dock Component.
 */
class OBSMixerComponent : public juce::Component,
                          public juce::Timer
{
public:
    OBSMixerComponent(OBSIntegrationService& service,
                      RoutingMatrix& routing,
                      LevelMeterProcessor* chMeters,
                      LevelMeterProcessor* busMeters);
    ~OBSMixerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void refreshDevices();

    // Callbacks to MainComponent
    std::function<void(int channelIndex, float gainLinear)> onChannelGainChanged;
    std::function<void(int channelIndex, bool muted)> onChannelMuteChanged;
    std::function<void(int busIndex, float gainLinear)> onBusGainChanged;
    std::function<void(int busIndex, bool muted)> onBusMuteChanged;

private:
    OBSIntegrationService& obsService;
    RoutingMatrix& routingMatrix;
    LevelMeterProcessor* channelLevelMeters = nullptr;
    LevelMeterProcessor* busLevelMeters = nullptr;

    OBSFaderLookAndFeel obsLookAndFeel;
    OBSWebSocketClient webSocketClient;

    // Top Control / Status Bar
    juce::Label titleLabel{"title", "OBS Studio Integration"};
    juce::Label statusPill;
    juce::Label deviceLabel{"dev", "Endpoint Output:"};
    juce::ComboBox streamDeviceCombo;
    juce::ComboBox endpointBusCombo;
    juce::Label routingModeLabel{"mode", "Routing Mode:"};
    juce::ComboBox routingModeCombo;

    juce::Label audioSpecsPill;
    juce::TextButton testSignalBtn{"Send Test Tone to OBS"};
    juce::TextButton openGuideBtn{"Open OBS Setup Guide"};
    juce::TextButton webSocketBtn{"OBS WebSocket"};

    // 5 OBS-style vertical channel strips
    std::unique_ptr<OBSChannelStrip> streamStrip;
    std::unique_ptr<OBSChannelStrip> micStrip;
    std::unique_ptr<OBSChannelStrip> guitarStrip;
    std::unique_ptr<OBSChannelStrip> musicStrip;
    std::unique_ptr<OBSChannelStrip> monitorStrip;
    std::unique_ptr<OBSChannelStrip> endpointOutputStrip;
    juce::StringArray outputDeviceNames;
    bool updatingOutputDevices = false;

    // Overlay Guide Dialog
    std::unique_ptr<OBSGuideComponent> guideComponent;
    bool showGuideOverlay = false;

    // WebSocket Configuration Dialog (Modal/Overlay)
    bool showWebSocketOverlay = false;
    juce::TextEditor wsHostEditor;
    juce::TextEditor wsPortEditor;
    juce::TextEditor wsPassEditor;
    juce::TextButton wsConnectBtn{"Connect WebSocket"};
    juce::TextButton wsCloseBtn{"Close"};
    juce::Label wsStatusLabel;

    void updateUIFromState();
    void updateAudioSpecsLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OBSMixerComponent)
};

} // namespace LiveBus
