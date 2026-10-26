#pragma once
#include <JuceHeader.h>
#include "RoutingMatrix.h"
#include "ChannelStripDSP.h"
#include "EightBandEQComponent.h"
#include "VocalDSPRackComponent.h"
#include "PluginHostClient.h"
#include "DeviceSupervisor.h"
#include "SceneManager.h"
#include "AudioFilePlayer.h"
#include "MidiEngine.h"
#include "PresetZipManager.h"
#include "PuzuDSPComponent.h"
#include "LevelMeterProcessor.h"
#include "LevelMeterComponents.h"
#include "AudioDeviceService.h"
#include "AudioSettingsComponent.h"
#include "PresetManager.h"
#include "RecallFilter.h"
#include "ScenesConsoleComponent.h"
#include "OBSIntegrationService.h"
#include "OBSMixerComponent.h"
#include "MidiMappingModel.h"
#include "MidiParameterRegistry.h"
#include "MidiManager.h"
#include "RoutingMatrixComponent.h"
#include "MidiLearnComponent.h"
#include "AudioPlaybackModel.h"
#include "WaveformThumbnailCache.h"
#include "TestSignalGenerator.h"
#include "PlaybackComponent.h"
#include "PluginLibraryModel.h"
#include "VST3HostEngine.h"
#include "EffectsConsoleComponent.h"
#include "PerformanceDiagnosticsModel.h"
#include "AudioPerformanceMonitor.h"
#include "DiagnosticsLogManager.h"
#include "PerformanceDiagnosticsComponent.h"
#include "AutoDucker.h"
#include "DuckingComponent.h"
#include "AudioSafetyProcessor.h"
#include "AudioSafetyComponent.h"
#include "MacroEngine.h"
#include "MacroControlComponent.h"
#include "YouTubeAudioCaptureService.h"
#include "YouTubePlayerComponent.h"
#include "AudioDelayCompensator.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// ==============================================================================
// Professional Broadcast Console Look & Feel
// ==============================================================================
class BroadcastConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BroadcastConsoleLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0d1117));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00f5d4));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0d1117));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161b22));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff30363d));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0f6fc));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff161b22));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff30363d));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xfff0f6fc));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a2333));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff2d3b55));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Background track
        g.setColour(juce::Colour(0xff1a2333));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff2a3447));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Active Arc
        juce::Path arcPath;
        arcPath.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
        auto accentColor = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (accentColor.isTransparent())
            accentColor = juce::Colour(0xff00f5d4);
        g.setColour(accentColor);
        g.strokePath(arcPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pointer Needle / Dot
        auto pointerLen = radius * 0.65f;
        auto px = center.x + pointerLen * std::cos(angle - juce::MathConstants<float>::halfPi);
        auto py = center.y + pointerLen * std::sin(angle - juce::MathConstants<float>::halfPi);

        g.setColour(juce::Colour(0xffffffff));
        g.fillEllipse(px - 2.5f, py - 2.5f, 5.0f, 5.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        bool isOn = button.getToggleState();
        auto text = button.getButtonText();

        if (text.isEmpty() || text == "On" || text == "Enable")
        {
            float switchW = 32.0f;
            float switchH = 18.0f;
            float switchX = bounds.getX() + (bounds.getWidth() - switchW) * 0.5f;
            float switchY = bounds.getY() + (bounds.getHeight() - switchH) * 0.5f;

            juce::Colour onCol = button.findColour(juce::TextButton::buttonOnColourId);
            if (onCol.isTransparent()) onCol = juce::Colour(0xff2563eb);

            g.setColour(isOn ? onCol : juce::Colour(0xff222a3a));
            g.fillRoundedRectangle(switchX, switchY, switchW, switchH, switchH * 0.5f);

            g.setColour(isOn ? onCol.brighter(0.2f) : juce::Colour(0xff334155));
            g.drawRoundedRectangle(switchX, switchY, switchW, switchH, switchH * 0.5f, 1.2f);

            float thumbRadius = (switchH - 4.0f) * 0.5f;
            float thumbX = isOn ? (switchX + switchW - thumbRadius * 2.0f - 2.0f) : (switchX + 2.0f);
            float thumbY = switchY + 2.0f;

            g.setColour(juce::Colours::white);
            g.fillEllipse(thumbX, thumbY, thumbRadius * 2.0f, thumbRadius * 2.0f);
        }
        else
        {
            float boxSize = 13.0f;
            float boxX = bounds.getX();
            float boxY = bounds.getY() + (bounds.getHeight() - boxSize) * 0.5f;

            g.setColour(juce::Colour(0xff161e2b));
            g.fillRoundedRectangle(boxX, boxY, boxSize, boxSize, 3.0f);
            g.setColour(isOn ? juce::Colour(0xff2563eb) : juce::Colour(0xff3b485d));
            g.drawRoundedRectangle(boxX, boxY, boxSize, boxSize, 3.0f, 1.2f);

            if (isOn)
            {
                g.setColour(juce::Colour(0xff2563eb));
                g.fillRoundedRectangle(boxX + 2.5f, boxY + 2.5f, boxSize - 5.0f, boxSize - 5.0f, 2.0f);
            }

            g.setColour(juce::Colour(0xffcbd5e1));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(text, (int)(boxX + boxSize + 6.0f), (int)bounds.getY(),
                       (int)(bounds.getWidth() - boxSize - 6.0f), (int)bounds.getHeight(),
                       juce::Justification::centredLeft, true);
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        auto isVertical = slider.isVertical();
        auto thumbColor = slider.findColour(juce::Slider::thumbColourId);
        if (thumbColor.isTransparent())
            thumbColor = juce::Colour(0xffe2e8f0);

        if (isVertical)
        {
            auto trackX = x + width * 0.5f;
            // Track rail
            g.setColour(juce::Colour(0xff161d2b));
            g.fillRoundedRectangle(trackX - 3.0f, (float)y, 6.0f, (float)height, 3.0f);
            g.setColour(juce::Colour(0xff2a3447));
            g.drawRoundedRectangle(trackX - 3.0f, (float)y, 6.0f, (float)height, 3.0f, 1.0f);

            // Active fill below thumb
            g.setColour(thumbColor.withAlpha(0.6f));
            g.fillRoundedRectangle(trackX - 2.0f, sliderPos, 4.0f, (float)(y + height) - sliderPos, 2.0f);

            // Thumb cap
            auto thumbW = (float)width - 6.0f;
            auto thumbH = 14.0f;
            auto thumbRect = juce::Rectangle<float>(trackX - thumbW * 0.5f, sliderPos - thumbH * 0.5f, thumbW, thumbH);
            g.setColour(juce::Colour(0xff1e2638));
            g.fillRoundedRectangle(thumbRect, 3.0f);
            g.setColour(thumbColor);
            g.drawRoundedRectangle(thumbRect, 3.0f, 1.5f);
            // Center notch
            g.drawLine(thumbRect.getX() + 3.0f, sliderPos, thumbRect.getRight() - 3.0f, sliderPos, 2.0f);
        }
        else
        {
            auto trackY = y + height * 0.5f;
            g.setColour(juce::Colour(0xff161d2b));
            g.fillRoundedRectangle((float)x, trackY - 3.0f, (float)width, 6.0f, 3.0f);
            g.setColour(thumbColor);
            g.fillRoundedRectangle((float)x, trackY - 2.0f, sliderPos - (float)x, 4.0f, 2.0f);

            auto thumbW = 12.0f;
            auto thumbH = (float)height - 6.0f;
            auto thumbRect = juce::Rectangle<float>(sliderPos - thumbW * 0.5f, trackY - thumbH * 0.5f, thumbW, thumbH);
            g.setColour(juce::Colour(0xff1e2638));
            g.fillRoundedRectangle(thumbRect, 3.0f);
            g.setColour(thumbColor);
            g.drawRoundedRectangle(thumbRect, 3.0f, 1.5f);
        }
    }
};

// ==============================================================================
// MainComponent Class
// ==============================================================================
class FloatingFxRackWindow final : public juce::DocumentWindow
{
public:
    FloatingFxRackWindow()
        : juce::DocumentWindow("LiveBus FX Rack", juce::Colour(0xff0d1117), juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(720, 440, 1800, 1200);
        setContentComponentSize(900, 640);
    }

    void closeButtonPressed() override { setVisible(false); }
};

class FxStripButton final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    std::function<void()> onContextMenu;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown())
        {
            if (onContextMenu)
                onContextMenu();
            return;
        }

        juce::TextButton::mouseDown(event);
    }
};

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::Button::Listener,
                            private juce::Timer,
                            private juce::ChangeListener,
                            public LiveBus::AudioDeviceServiceListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void captureTabSnapshots(const juce::File& outputDir);

    // LiveBus::AudioDeviceServiceListener
    void onDeviceStateChanged(DeviceState newState, const juce::String& message) override;
    void onDeviceSetupChanged() override;
    void refreshChannelInputSourceCombos();

private:
    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* in, int nIn, float* const* out, int nOut, int n,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

    // Listeners and timers
    void buttonClicked(juce::Button*) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    // Internal actions
    void showAudioSettings();
    void scanVST3Plugins();
    void refreshPluginList();
    void addSelectedPlugin();
    void removePlugin();
    void togglePluginBypass();

    void refreshSceneList();
    void onNewScene();
    void onSaveScene();
    void onSaveSceneAs();
    void onLoadScene(const juce::String& sceneName);
    void onDuplicateScene();
    void onRenameScene();
    void onDeleteScene();
    void initializeMidiParameterRegistry();

    void handleAddChannel(juce::Component* menuTarget = nullptr);
    void handleRemoveChannel();
    void chooseStreamRecordingFile();
    bool startStreamRecording(const juce::File& file);
    void stopStreamRecording();
    void updateLatencyCompensation();
    void updateChannelUIVisibility();
    void syncSharedUIFromModel();

    void selectDSPTarget(int targetIndex);
    void openFloatingFxRack(int targetIndex);
    void showFxChainMenu(int targetIndex, juce::Component& targetComponent);
    void updateFxStripIndicators();
    void applyDSPControls(int targetIndex);
    void updateDSPControlsFromTarget(int targetIndex);
    BusDSPProcessor& getProcessorForTarget(int targetIndex);

    void syncChannelRouteUiState(int channelIndex);
    void applySceneToUI(const SceneData& data);
    void captureSceneFromUI(SceneData& data);
    void applySceneWithFilter(const SceneData& data, const LiveBus::RecallFilter& filter);
    void copyEQ(int srcTarget, int destTarget);
    void copyVocalDSP(int srcTarget, int destTarget);
    void copyChannelStrip(int srcCh, int destCh, bool includeSends);
    void resetTargetDefaults(int targetIndex);
    void loadPresetToTarget(LiveBus::PresetCategory cat, int targetIndex, const LiveBus::PresetItem& item);
    void savePresetFromTarget(LiveBus::PresetCategory cat, int targetIndex, const juce::String& name);
    void markUnsavedChanges();

    void log(const juce::String& message);

    // Look and feel
    BroadcastConsoleLookAndFeel consoleLookAndFeel;

    // Audio Engine Core
    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    RoutingMatrix routingMatrix;
    std::array<BusDSPProcessor, maxInputs> channelDSPs;
    std::array<BusDSPProcessor, numBuses> busProcessors;
    std::array<LiveBus::AudioDelayCompensator, maxInputs> channelLatencyCompensators;
    std::array<LiveBus::AudioDelayCompensator, numBuses> busLatencyCompensators;
    LiveBus::AudioDelayCompensator hardwareDirectLatencyCompensator;
    std::atomic<bool> latencyCompensationOutOfRange{false};
    bool latencyCompensationWarningLogged = false;
    std::array<juce::AudioBuffer<float>, maxInputs> channelBuffers;
    PluginHostClient pluginHostClient;
    std::unique_ptr<LiveBus::AudioDeviceService> audioDeviceService;
    std::unique_ptr<LiveBus::AudioSettingsComponent> audioSettingsComponent;
    SceneManager sceneManager;
    LiveBus::PresetManager presetManager;
    std::unique_ptr<LiveBus::ScenesConsoleComponent> scenesConsoleComponent;
    juce::String activeSceneName{ "Default" };
    bool hasUnsavedChanges = false;
    LiveBus::AudioFilePlayer audioFilePlayer;
    LiveBus::TestSignalGenerator testSignalGenerator;
    std::unique_ptr<LiveBus::PlaybackComponent> playbackComponent;
    MidiEngine midiEngine;
    PresetZipManager presetZipManager;

    juce::AudioBuffer<float> streamBuffer;
    juce::AudioBuffer<float> monitorBuffer;

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
    std::atomic<bool> isAudioRunning{false};
    bool isUpdatingUI = false;
    int selectedTarget = 0; // 0..maxInputs-1 = Input Channels, maxInputs = Stream Bus, maxInputs+1 = Monitor Bus
    // Array VST Target Index per channel (16 channel + 2 bus)
// Index 0-15: Channel 1-16
// Index 16: Stream Bus
// Index 17: Monitor Bus
std::atomic<int> vstTargetIndex[maxInputs + 2]; // Array ukuran 18 (0-17)

    // Test tone generator
    std::atomic<bool> testToneEnabled{false};
    float testTonePhase = 0.0f;

    // Navigation Tab state
    enum NavTab
    {
        TabMixer = 0,
        TabRouting,
        TabDSP,
        TabEffects,
        TabDucking,
        TabSafety,
        TabMacros,
        TabPlayback,
        TabScenes,
        TabDevices,
        TabMIDI,
        TabSettings,
        TabDiagnostics,
        TabOBS,
        TabYouTube,
        NumTabs
    };
    int currentTab = TabMixer;

    // =========================================================================
    // 1. Top Header Bar
    // =========================================================================
    juce::Label brandLabel;
    juce::Label subBrandLabel;
    juce::Label audioStatusBadge;
    juce::Label devicePillLabel;
    juce::Label srPillLabel;
    juce::Label bufferPillLabel;
    juce::Label cpuPillLabel;
    float currentCpuUsage = 0.032f;
    juce::TextButton headerEmergencyMuteBtn{"PANIC MUTE"};

    // Top Header Quick Actions (matching reference design)
    juce::TextButton topScenesBtn{"Scenes"};
    juce::TextButton topPresetsBtn{"Presets"};
    juce::TextButton topMidiBtn{"MIDI"};
    juce::TextButton topObsBtn{"OBS"};
    juce::TextButton topSettingsBtn{"Settings"};

    // Navigation Tabs (Left Sidebar)
    std::array<juce::TextButton, NumTabs> navTabButtons;

    // =========================================================================
    // 2. Mixer Page Components
    // =========================================================================
    // Area 1: Input Channels (1..4, expandable to 16)
    struct InputChannelUI
    {
        juce::TextButton channelColorButton;
        juce::Colour accentColour;
        juce::Label iconLabel;
        juce::TextEditor nameEditor;
        juce::Label inputSourceHeaderLabel{"Input Source"};
        juce::ComboBox inputSourceCombo;
        juce::Slider faderSlider;
        juce::Label faderValueLabel;
        juce::Slider gainSlider; // Rotary Gain
        juce::Label gainValueLabel;
        juce::Slider panSlider;
        juce::Label panValueLabel;
        juce::TextButton muteBtn{"M"};
        juce::TextButton soloBtn{"S"};
        juce::TextButton linkBtn{"Link"};
        juce::ComboBox outputRouteCombo;
        juce::TextButton bottomPillBtn;
        FxStripButton dspSelectBtn{"FX"};
        juce::ToggleButton streamRouteToggle{"Send to Stream"};
        juce::Slider streamSendKnob;
        juce::Label streamSendValue;
        juce::ToggleButton monitorRouteToggle{"Send to Monitor"};
        juce::Slider monitorSendKnob;
        juce::Label monitorSendValue;
    };
    std::array<InputChannelUI, maxInputs> inputUIs;
    juce::TextButton addChannelBtn{"+ Add Ch"};
    juce::TextButton removeChannelBtn{"- Del Ch"};
    juce::TextButton previousChannelPageBtn{"< Ch"};
    juce::TextButton nextChannelPageBtn{"Ch >"};
    juce::Label channelPageLabel;
    juce::Slider channelScrollSlider;
    int channelScrollPosition = 0;
    float mixerTopPanelProportion = 0.54f;
    bool resizingMixerPanels = false;

    // Area 2: Master Buses (Stream & Monitor)
    struct MasterBusUI
    {
        juce::Label headerLabel;
        juce::Label iconLabel;
        juce::Slider masterFader;
        juce::Label faderValueLabel;
        juce::TextButton muteBtn{"M"};
        juce::TextButton linkBtn{"Link"};
        juce::Label outputTagLabel;
        FxStripButton dspSelectBtn{"Bus DSP"};
        juce::TextButton bottomPillBtn;
    };
    std::array<MasterBusUI, numBuses> masterUIs;

    // Professional Audio Level Metering Engines (Thread-Safe DSP)
    std::array<LiveBus::LevelMeterProcessor, maxInputs> channelLevelMeters;
    std::array<LiveBus::LevelMeterProcessor, numBuses> busLevelMeters;

    // Professional Audio Metering UI Components
    std::array<std::unique_ptr<LiveBus::DigitalPeakMeterComponent>, maxInputs> channelPeakMeters;
    std::unique_ptr<LiveBus::DigitalPeakMeterComponent> streamBusPeakMeter;
    std::unique_ptr<LiveBus::DigitalPeakMeterComponent> monitorBusPeakMeter;
    std::unique_ptr<LiveBus::MasterMeterPanel> masterMeterStation;

    // Area 3: Right DSP Rack (Channel 1 - DSP & Effects)
    juce::Label dspRackHeaderLabel{"Channel 1 - DSP & Effects"};
    juce::ToggleButton rackPowerToggle{"On"};
    juce::TextButton bypassAllBtn{"Bypass All"};

    // HPF
    juce::ToggleButton hpfToggle{"High-Pass"};
    juce::Slider hpfFreqSlider;
    juce::ComboBox hpfSlopeCombo;

    // Noise Gate
    juce::ToggleButton gateToggle{"Noise Gate"};
    juce::Slider gateThreshSlider;
    juce::Slider gateAttackSlider;
    juce::Slider gateHoldSlider;
    juce::Slider gateReleaseSlider;
    juce::Slider gateRatioSlider;

    // Compressor
    juce::ToggleButton compToggle{"Compressor"};
    juce::Slider compThreshSlider;
    juce::Slider compRatioSlider;
    juce::Slider compAttackSlider;
    juce::Slider compReleaseSlider;
    juce::Slider compMakeupSlider;

    // Limiter
    juce::ToggleButton limiterToggle{"Limiter"};
    juce::Slider limiterCeilSlider;
    juce::Slider limiterReleaseSlider;

    // Reverb
    juce::ToggleButton reverbToggle{"Reverb"};
    juce::Slider reverbRoomSlider;
    juce::Slider reverbDampingSlider;
    juce::Slider reverbWetSlider;
    juce::Slider reverbDrySlider;

    // Delay
    juce::ToggleButton delayToggle{"Delay"};
    juce::Slider delayTimeSlider;
    juce::TextButton delayTempoBtn{"1/8 𝅘𝅥𝅯"};
    juce::Slider delayFeedbackSlider;
    juce::Slider delayWetSlider;
    juce::Slider delayDrySlider;

    // Legacy EQ display support for standalone DSP page
    juce::TextButton streamBusDSPTab{"Stream Bus DSP"};
    juce::TextButton monitorBusDSPTab{"Monitor Bus DSP"};
    std::array<juce::TextButton, 6> dspSubTabs;
    int currentDSPSubTab = 0;
    juce::ToggleButton enableEQToggle{"Enable EQ"};

    class EQCurveDisplay : public juce::Component
    {
    public:
        EQCurveDisplay(MainComponent& o) : owner(o) {}
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
    private:
        MainComponent& owner;
        int draggedBand = -1;
    };
    std::unique_ptr<EQCurveDisplay> eqCurveDisplay;

    juce::Slider eqLowFreqSlider;
    juce::Slider eqLowGainSlider;
    juce::Slider eqLowQSlider;
    juce::Slider eqMid1FreqSlider;
    juce::Slider eqMid1GainSlider;
    juce::Slider eqMid1QSlider;
    juce::Slider eqMid2FreqSlider;
    juce::Slider eqMid2GainSlider;
    juce::Slider eqMid2QSlider;
    juce::Slider eqHighFreqSlider;
    juce::Slider eqHighGainSlider;
    juce::Slider eqHighQSlider;

    // =========================================================================
    // Bottom Tier Components (3 Side-by-Side Cards)
    // =========================================================================
    // Card 1: Input / Output Routing Matrix
    juce::Label routingMatrixHeader{"Input / Output Routing Matrix"};
    juce::TextButton matrixResetBtn{"Reset"};
    juce::ComboBox matrixPresetCombo;
    std::array<juce::Label, 4> matrixChannelPills;
    std::array<juce::ComboBox, 4> matrixSourceCombos;
    std::array<juce::ComboBox, 4> matrixStereoCombos;
    std::array<juce::ToggleButton, 4> matrixStreamToggles;
    std::array<juce::Slider, 4> matrixStreamSliders;
    std::array<juce::Label, 4> matrixStreamLabels;
    std::array<juce::ToggleButton, 4> matrixMonitorToggles;
    std::array<juce::Slider, 4> matrixMonitorSliders;
    std::array<juce::Label, 4> matrixMonitorLabels;
    std::array<juce::Slider, 4> matrixPanSliders;
    std::array<juce::Label, 4> matrixPanLabels;

    // Card 2: Auto Ducking
    juce::Label duckingHeaderLabel{"Auto Ducking"};
    juce::ToggleButton duckingEnableToggle{"On"};
    juce::ComboBox duckingPresetCombo;
    juce::Label duckingTriggerLabel{"Trigger Source"};
    juce::ComboBox duckingTriggerCombo;
    juce::Slider duckingThresholdSlider;
    juce::Label duckingThresholdLabel;
    juce::Slider duckingDepthSlider;
    juce::Slider duckingAttackSlider;
    juce::Slider duckingHoldSlider;
    juce::Slider duckingReleaseSlider;
    juce::ToggleButton duckingTargetCh3Check{"Music Left (CH 3)"};
    juce::ToggleButton duckingTargetCh4Check{"Music Right (CH 4)"};
    juce::ToggleButton duckingApplyStreamCheck{"Stream Bus"};
    juce::ToggleButton duckingApplyMonitorCheck{"Monitor Bus"};
    juce::Label duckingInputLevelLabel{"-12.3 dB"};
    juce::Label duckingGRLabel{"-11.6 dB"};
    std::unique_ptr<LiveBus::DigitalPeakMeterComponent> duckingInputMeter;

    // Card 3: Audio Safety
    juce::Label safetyHeaderLabel{"Audio Safety"};
    juce::ToggleButton safetyStreamLimiterToggle{"On"};
    juce::Slider safetyStreamCeilingSlider;
    juce::Slider safetyStreamReleaseSlider;
    juce::ToggleButton safetyMonitorLimiterToggle{"On"};
    juce::Slider safetyMonitorCeilingSlider;
    juce::Slider safetyMonitorReleaseSlider;
    juce::Label safetyClipWarningLabel{"Clipping Warning"};
    juce::TextButton safetyResetPeakBtn{"Reset Peak"};
    juce::Label safetyStreamClipLabel{"Stream: -0.3 dB"};
    juce::Label safetyMonitorPeakHoldLabel{"Monitor: -1.2 dB"};
    juce::Label safetyRoutingCheckBadge{"No feedback risk detected"};
    juce::TextButton safetyRoutingSettingsBtn{"Settings"};
    juce::TextButton safetyEmergencyMuteBtn{"EMERGENCY MUTE"};
    juce::ToggleButton safetyMuteStreamRadio{"Stream Bus"};
    juce::ToggleButton safetyMuteMonitorRadio{"Monitor Bus"};
    juce::ToggleButton safetyMuteBothRadio{"Both"};

    // Legacy Routing Matrix controls kept for backward compatibility
    std::array<juce::Label, maxInputs> routingChannelNames;
    std::array<juce::ToggleButton, maxInputs> routingStreamChecks;
    std::array<juce::ToggleButton, maxInputs> routingMonitorChecks;
    juce::TextButton allStreamBtn{"All Stream"};
    juce::TextButton allMonitorBtn{"All Monitor"};
    juce::TextButton clearAllRouteBtn{"Clear All"};

    // Area 5: VST3 Effects Rack
    juce::Label vstRackHeader{"VST3 Effects Rack (Stream Bus)"};
    juce::ComboBox vstTargetCombo;
    juce::TextButton scanPluginsBtn{"Scan VST3..."};
    juce::TextButton addPluginBtn{"Add Plugin"};
    juce::TextButton bypassAllPluginsBtn{"Bypass All"};

    struct PluginSlotRow
    {
        juce::Label dragHandle{"="};
        juce::Label indexLabel{"1"};
        juce::Label nameLabel{"No Plugin Loaded"};
        juce::Label categoryLabel{"Audio Effect"};
        juce::Label statusBadge{"Inactive"};
        juce::TextButton configBtn{"EDIT"};
        juce::TextButton bypassBtn{"BYP"};
    };
    std::array<PluginSlotRow, 3> pluginSlotRows;

    // Area 6: Scene Manager
    juce::Label sceneManagerHeader{"Scene Manager"};
    juce::ComboBox sceneCombo;
    juce::TextButton saveSceneBtn{"Save"};
    juce::TextButton saveSceneAsBtn{"Save As..."};
    juce::ListBox sceneListBox;
    class SceneListModel : public juce::ListBoxModel
    {
    public:
        SceneListModel(MainComponent& o) : owner(o) {}
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    private:
        MainComponent& owner;
    };
    std::unique_ptr<SceneListModel> sceneListModel;
    juce::StringArray availableScenes;
    juce::TextButton dupSceneBtn{"Duplicate"};
    juce::TextButton renameSceneBtn{"Rename"};
    juce::TextButton deleteSceneBtn{"Delete"};

    // Area 7: Application Log Box
    juce::Label logHeaderLabel{"Log / Status"};
    juce::TextButton clearLogBtn{"Clear Log"};
    juce::TextButton copyLogBtn{"Copy Log"};
    juce::TextEditor logBox;
    juce::String lastLoggedMessage;
    int duplicateLogCount = 0;

    // Area 8: Device Settings Quick Bar
    juce::Label deviceSettingsHeader{"Device Settings"};
    juce::ComboBox inputDeviceCombo;
    juce::ComboBox outputDeviceCombo;
    juce::ComboBox sampleRateCombo;
    juce::ComboBox bufferSizeCombo;
    juce::ComboBox streamOutputMapCombo;
    juce::ComboBox monitorOutputMapCombo;
    juce::TextButton testToneBtn{"Test Tone (440Hz)"};

    // =========================================================================
    // Dedicated Full Pages for Tabs 1..7
    // =========================================================================
    // DSP Full Page
    juce::Label dspPageTitle{"DSP Studio — Precision Audio Processing"};
    juce::ComboBox dspPageTargetCombo;
    juce::TextButton dspPuzuEQToggleBtn{"Switch to Puzu 15-Band EQ"};
    bool showPuzu15BandEQ = false;
    juce::TextButton dspViewSwitchBtn{"Switch to Standalone 8-Band EQ"};
    bool showVocalRack = true;
    std::unique_ptr<LiveBus::VocalDSPRackComponent> vocalDSPRackComponent;
    std::unique_ptr<LiveBus::EightBandEQComponent> eightBandEQComponent;
    std::unique_ptr<PuzuDSPComponent> puzuDSPComponent;
    std::array<juce::Slider, 15> puzuBandSliders;
    std::array<juce::Label, 15> puzuBandLabels;

    // Effects Full Page
    juce::Label effectsPageTitle{"VST3 Plugin Host — Isolated Sandbox"};
    juce::TextEditor vstPathEditor;
    juce::TextButton browseVSTPathBtn{"Scan Installed VST3 Plugins"};
    juce::Label helperHostStatusLabel{"Helper: LiveBusPluginHost.exe [Running PID: Active]"};
    juce::TextButton testWatchdogRecoveryBtn{"Simulate Plugin Crash & Verify Audio Fallback"};
    juce::TextEditor discoveredPluginsEditor;
    std::unique_ptr<LiveBus::VST3HostEngine> vst3HostEngine;
    std::unique_ptr<LiveBus::EffectsConsoleComponent> effectsConsoleComponent;
    std::unique_ptr<LiveBus::EffectsConsoleComponent> floatingFxRackComponent;
    std::unique_ptr<FloatingFxRackWindow> floatingFxRackWindow;
    int fxStatusRefreshTicks = 0;

    // Scenes Full Page
    juce::Label scenesPageTitle{"Scene Manager & Preset Cloud"};
    juce::TextButton exportZipBtn{"Export Presets to ZIP..."};
    juce::TextButton importZipBtn{"Import Presets from ZIP..."};
    juce::TextEditor sceneXmlPreview;

    // Devices Full Page
    juce::Label devicesPageTitle{"Audio Device Supervisor & Latency Diagnostics"};
    juce::Label deviceStatusDetailedLabel;
    juce::TextButton restartDeviceBtn{"Restart Audio Driver"};
    juce::TextButton asioControlPanelBtn{"Open ASIO Control Panel"};
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioDeviceSelector;

    // MIDI Full Page
    juce::Label midiPageTitle{"MIDI Controller & Hardware Mapping"};
    juce::ComboBox midiInputCombo;
    juce::TextEditor midiActivityLog;

    // Playback Full Page
    juce::Label playbackPageTitle{"BGM Player — Audio File Playback"};
    juce::TextButton openAudioFileBtn{"Open Audio File (MP3 / WAV)..."};
    juce::TextButton playAudioFileBtn{"▶ Play"};
    juce::TextButton pauseAudioFileBtn{"⏸ Pause"};
    juce::TextButton stopAudioFileBtn{"⏹ Stop"};
    juce::ToggleButton loopAudioFileToggle{"Loop"};
    juce::Slider audioFileGainSlider;
    juce::Label audioFileGainValueLabel;
    juce::Slider playbackPositionSlider;
    juce::Label playbackStatusLabel;
    juce::ToggleButton playbackRouteStream{"Route BGM to Stream Bus"};
    juce::ToggleButton playbackRouteMonitor{"Route BGM to Monitor Bus"};
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Settings Full Page
    juce::Label settingsPageTitle{"Application Settings & System Preferences"};
    juce::TextButton openAudioSettingsBtn{"Configure Audio Hardware Settings..."};
    juce::ToggleButton darkThemeToggle{"Charcoal Dark Theme (Default)"};
    juce::ToggleButton autoSaveSceneToggle{"Auto-Save Scene on Exit"};
    juce::ToggleButton peakHoldToggle{"Enable Peak Hold VU Meters"};
    juce::TextButton resetDefaultsBtn{"Reset All Mixer Defaults"};
    juce::TextEditor aboutBoxEditor;

    // OBS Studio Integration
    std::unique_ptr<LiveBus::OBSIntegrationService> obsIntegrationService;
    std::unique_ptr<LiveBus::OBSMixerComponent> obsMixerComponent;

    // Professional MIDI Controller & MIDI Learn
    LiveBus::MidiParameterRegistry midiParameterRegistry;
    std::unique_ptr<LiveBus::MidiManager> midiManager;
    std::unique_ptr<LiveBus::MidiLearnComponent> midiLearnComponent;

    // Professional Performance & Diagnostics System
    std::unique_ptr<LiveBus::AudioPerformanceMonitor> audioPerformanceMonitor;
    std::unique_ptr<LiveBus::DiagnosticsLogManager> diagnosticsLogManager;
    std::unique_ptr<LiveBus::PerformanceDiagnosticsComponent> performanceDiagnosticsComponent;

    // Professional Broadcast Auto Ducking, Audio Safety & Macro Engine
    LiveBus::AutoDucker autoDucker;
    LiveBus::AudioSafetyProcessor audioSafetyProcessor;
    LiveBus::MacroEngine macroEngine;

    std::unique_ptr<LiveBus::DuckingComponent> duckingComponent;
    std::unique_ptr<LiveBus::AudioSafetyComponent> audioSafetyComponent;
    std::unique_ptr<LiveBus::MacroControlComponent> macroControlComponent;
    std::unique_ptr<LiveBus::RoutingMatrixComponent> routingMatrixComponent;
    juce::TimeSliceThread streamRecorderThread{"Stream recorder writer"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> streamRecorder;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeStreamRecorder{nullptr};
    std::atomic<unsigned int> streamRecorderReaders{0};
    std::atomic<std::uint64_t> droppedRecordingBlocks{0};
    std::atomic<bool> recordingFileChooserOpen{false};
    std::unique_ptr<juce::FileChooser> recordingFileChooser;
    juce::File streamRecordingFile;

    // Embedded YouTube Player & Audio Capture Bridge
    LiveBus::YouTubeAudioCaptureService youtubeAudioService;
    std::unique_ptr<LiveBus::YouTubePlayerComponent> youtubePlayerComponent;

    // Dedicated preallocated audio thread buffers for per-channel routing
    juce::AudioBuffer<float> routingPlaybackBuffer;
    juce::AudioBuffer<float> routingTestSignalBuffer;
    juce::AudioBuffer<float> routingYouTubeBuffer;
    juce::AudioBuffer<float> routingHwDirectBuffer; // Preallocated 8-channel buffer for direct hardware sends

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
