#pragma once
#include <JuceHeader.h>
#include "AudioFilePlayer.h"
#include "TestSignalGenerator.h"

namespace LiveBus
{

/**
 * Professional Streaming Audio File Player and Test Signal Generator Console.
 * Implements the full UI layout matching the reference design.
 */
class PlaybackComponent : public juce::Component,
                          public juce::ChangeListener,
                          public juce::Timer
{
public:
    PlaybackComponent(AudioFilePlayer& player, TestSignalGenerator& generator);
    ~PlaybackComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    // View mode: Audio File Player vs Test Signal Generator
    void setShowTestSignal(bool showSignal);
    bool isShowingTestSignal() const { return showingTestSignal; }

    enum SubTab { SubTabFileList = 0, SubTabMarkerList = 1 };
    void setSubTab(SubTab tab);
    SubTab getSubTab() const { return activeSubTab; }

private:
    AudioFilePlayer& audioPlayer;
    TestSignalGenerator& signalGen;

    bool showingTestSignal{false};

    // Header Controls
    juce::Label headerTitleLabel;
    juce::TextButton bypassBtn{"Bypass"};
    juce::TextButton modeSwitchBtn{"[Test Signal Generator]"};
    juce::TextButton optionsMenuBtn{"..."};

    // =========================================================================
    // Waveform & Timeline Viewport
    // =========================================================================
    class WaveformViewport : public juce::Component
    {
    public:
        WaveformViewport(AudioFilePlayer& player);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        void zoomIn();
        void zoomOut();
        void zoomFit();

        double viewStartTime{0.0};
        double viewEndTime{10.0};
        float zoomFactor{1.0f};

    private:
        AudioFilePlayer& player;
        enum DragTarget { None, Playhead, LoopStart, LoopEnd, OverviewSeek };
        DragTarget currentDrag{None};
    };

    WaveformViewport waveformView;

    // Timeline Bar & Waveform Tools
    juce::Label timeDisplayCurrent;
    juce::Label timeDisplayTotal;
    juce::TextButton zoomInBtn{"+"};
    juce::TextButton zoomOutBtn{"-"};
    juce::TextButton zoomFitBtn{"< >"};
    juce::TextButton addMarkerBtn{"+ Flag"};
    juce::Label currentTrackNameLabel;

    // =========================================================================
    // Lower Left Panel: FILE LIST VIEW vs MARKER LIST VIEW
    // =========================================================================
    SubTab activeSubTab{SubTabFileList};

    juce::TextButton fileListTabBtn{"FILE LIST VIEW"};
    juce::TextButton markerListTabBtn{"MARKER LIST VIEW"};

    // File list toolbar & Table
    juce::TextButton addFilesBtn{"+ Add Files"};
    juce::TextButton shuffleBtn{"Shuffle"};
    juce::TextButton clearPlaylistBtn{"Clear"};
    juce::TextButton savePlaylistBtn{"Save PL"};
    juce::TextButton loadPlaylistBtn{"Load PL"};
    juce::ToggleButton autoNextToggle{"Auto Next"};

    juce::ListBox playlistListBox;
    class PlaylistModel;
    std::unique_ptr<PlaylistModel> playlistModel;

    // Marker list toolbar & Table
    juce::TextButton addMarkerFromListBtn{"+ Add Marker"};
    juce::TextButton clearAllMarkersBtn{"Clear Markers"};
    juce::ListBox markerListBox;
    class MarkerModel;
    std::unique_ptr<MarkerModel> markerModel;

    // =========================================================================
    // Lower Right Panel: Transport, Speed/Pitch, Meters & Master Fader
    // =========================================================================
    // Transport Buttons
    juce::TextButton prevTrackBtn{"|<<"};
    juce::TextButton playPauseBtn{"PLAY"};
    juce::TextButton nextTrackBtn{">>|"};
    juce::TextButton rewindBtn{"<<"};
    juce::TextButton fastFwdBtn{">>"};
    juce::ToggleButton loopBtn{"Loop"};
    juce::ToggleButton muteBtn{"Mute"};
    juce::TextButton jumpLoopStartBtn{"|<"};
    juce::TextButton stopBtn{"STOP"};
    juce::TextButton jumpLoopEndBtn{">|"};
    juce::ToggleButton routeStreamToggle{"To Stream"};
    juce::ToggleButton routeMonitorToggle{"To Monitor"};

    // Speed & Pitch Rotary Dials
    juce::Slider speedKnob;
    juce::Label speedLabel;
    juce::Slider pitchKnob;
    juce::Label pitchLabel;
    juce::ToggleButton pitchLockToggle{"Lock"};

    // Stereo Output Level Meter
    float meterL{0.0f};
    float meterR{0.0f};
    juce::Slider masterGainSlider;
    juce::Label masterGainLabel;

    // =========================================================================
    // Test Signal Generator Panel Components
    // =========================================================================
    juce::GroupComponent testSignalGroup{"testGroup", "Audio Test Signal Generator & System Calibration"};
    juce::ComboBox signalTypeCombo;
    juce::Slider signalFreqSlider;
    juce::Label signalFreqLabel;
    juce::Slider signalLevelSlider;
    juce::Label signalLevelLabel;
    juce::ComboBox signalDestCombo;
    juce::ComboBox signalChannelCombo;
    juce::ToggleButton signalContinuousToggle{"Continuous Signal"};
    juce::TextButton signalBurst1sBtn{"Burst 1s"};
    juce::TextButton signalBurst5sBtn{"Burst 5s"};
    juce::TextButton signalStartStopBtn{"START TEST SIGNAL"};
    juce::Label signalWarningLabel;
    float testMeterL{0.0f};
    float testMeterR{0.0f};

    void updateUIFromState();
    void formatTimeText(double currentSecs, double totalSecs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaybackComponent)
};

} // namespace LiveBus
