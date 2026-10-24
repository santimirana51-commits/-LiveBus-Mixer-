#pragma once
#include <JuceHeader.h>
#include "RoutingMatrix.h"
#include "ChannelStripDSP.h"
#include "PluginHostClient.h"
#include "SceneManager.h"
#include <array>
#include <functional>

class LV1MixerComponent final : public juce::Component,
                                public juce::Slider::Listener,
                                public juce::Button::Listener,
                                public juce::Timer
{
public:
    LV1MixerComponent(RoutingMatrix& matrix,
                      std::array<BusDSPProcessor, numBuses>& dspProcessors,
                      PluginHostClient& pluginClient,
                      SceneManager& scenes);
    ~LV1MixerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;

    // Callbacks to parent
    std::function<void()> onOpenDSPStudio;
    std::function<void()> onOpenAudioSettings;
    std::function<void()> onOpenPluginScanner;
    std::function<void()> onSaveScene;
    std::function<void()> onLoadScene;

    void updateUIFromState();

private:
    RoutingMatrix& routingMatrix;
    std::array<BusDSPProcessor, numBuses>& busProcessors;
    PluginHostClient& pluginHostClient;
    SceneManager& sceneManager;

    bool isUpdatingUI = false;
    int selectedChannel = 0; // 0..15

    // Tap tempo variables
    juce::int64 lastTapTimeMs = 0;
    int currentBPM = 120;

    // 1. Top Header Components
    juce::TextButton chSelectBadge{"Channel 1"};
    juce::TextButton chPrevBtn{"◄"};
    juce::TextButton chNextBtn{"►"};
    juce::TextButton presetBtn{"PRESET* ▼"};
    juce::TextButton clrSoloBtn{"CLR SOLO"};
    juce::TextButton talkBtn{"TALK"};
    juce::TextButton tapBpmBtn{"120 BPM TAP"};

    // Screen Tabs
    juce::TextButton mixer1TabBtn{"MIXER 1"};
    juce::TextButton mixer2TabBtn{"MIXER 2"};
    juce::TextButton channelTabBtn{"CHANNEL"};
    juce::TextButton showTabBtn{"SHOW"};
    juce::TextButton patchTabBtn{"PATCH"};
    juce::TextButton setupTabBtn{"SETUP"};

    // Scene & Status header
    juce::ComboBox sceneSelector;
    juce::Label serverIoLabel;
    juce::Label clockLabel;
    juce::Label brandLabel;

    // 2. 16 Channel Strips
    struct LV1ChannelStrip
    {
        juce::Label sourceLabel;     // "1 Mic 1"
        juce::TextButton filterBtn;  // "FILTERS"
        juce::Component dynPreview;  // Mini compression curve
        juce::Component eqPreview;   // Mini EQ curve
        juce::TextButton idBadge;    // "Chn 1"
        juce::Slider panKnob;        // Rotary pan dial
        juce::TextButton muteBtn;    // "MUTE"
        juce::Label dbReadout;       // "-3.8 dB"
        juce::Slider fader;          // Long-throw fader
        juce::TextButton cueBtn;     // "1" (Listen/Solo)
        float currentMeterVal = 0.0f;
    };
    std::array<LV1ChannelStrip, maxInputs> channelStrips;

    // 3. Master Section
    juce::Label masterTag;
    juce::Slider masterPanKnob;
    juce::TextButton masterMuteBtn;
    juce::Label masterDbReadout;
    juce::Slider masterFader;
    juce::TextButton masterCueBtn;
    float masterMeterL = 0.0f;
    float masterMeterR = 0.0f;

    // 4. Right Sidebar: MODES, LAYERS, MUTE GRP
    std::array<juce::TextButton, 8> modeBtns;
    std::array<juce::TextButton, 6> layerBtns;
    std::array<juce::TextButton, 8> muteGrpBtns;

    // 5. Left Sidebar: USER Buttons
    std::array<juce::TextButton, 16> userBtns;

    void drawFaderScale(juce::Graphics& g, int x, int y, int h);
    void drawLedMeter(juce::Graphics& g, int x, int y, int w, int h, float level);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LV1MixerComponent)
};
