#pragma once
#include <JuceHeader.h>
#include "YouTubeAudioCaptureService.h"

namespace LiveBus
{

/**
 * YouTube Player Component.
 *
 * Responsibilities:
 * - URL input with standard YouTube format validation (youtube.com/watch?v=, youtu.be/, embed/).
 * - Embedded web browser component (supports Windows WebView2 / standard embedded browser).
 * - Full transport controls: Load/Open, Play, Pause, Stop, Seek, Volume, Mute.
 * - Legitimate platform compliance: Uses official YouTube IFrame Embed Player API
 *   (youtube-nocookie.com/embed/{id}?enablejsapi=1). Never bypasses ads, scrapes,
 *   or violates terms of service.
 * - Hardware Audio Capture Controls: input device selector for loopback / virtual audio cable,
 *   live VU peak meters, connection indicators, and stream diagnostic badges.
 * - Clear separation of player status vs. real audio signal detected status.
 */
class YouTubePlayerComponent : public juce::Component,
                               public juce::Button::Listener,
                               public juce::Slider::Listener,
                               private juce::Timer
{
public:
    YouTubePlayerComponent(YouTubeAudioCaptureService& captureService);
    ~YouTubePlayerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    // URL Parsing and Video ID extraction
    static juce::String extractVideoId(const juce::String& url);
    static bool isValidYouTubeUrl(const juce::String& url);
    static bool isYouTubeHomeUrl(const juce::String& url);

    void loadVideoUrl(const juce::String& url);
    void play();
    void pause();
    void stop();
    void seekTo(double seconds);
    void setPlayerVolume(float volumeLinear);
    void setMuted(bool mute);

    juce::String getCurrentVideoId() const { return currentVideoId; }
    juce::String getCurrentVideoUrl() const { return currentUrl; }
    bool isPlaying() const noexcept { return playerIsPlaying; }
    bool isPaused() const noexcept { return playerIsPaused; }
    bool isLoaded() const noexcept { return playerIsLoaded; }

private:
    void timerCallback() override;
    void refreshCaptureDevices();
    void updateStatusBadges();
    void pollPlayerStatus();
    void handlePlayerStatus(const juce::var& status);
    void executePlayerScript(const juce::String& script);
    void handleBrowserPageFinished();
    void handleYouTubeSitePageFinished(const juce::String& url);
    void handleBrowserNetworkError(const juce::String& error);

    YouTubeAudioCaptureService& audioCaptureService;

    // Header & Info
    juce::Label headerTitle{"YouTube Player — Studio Broadcast Integration"};
    juce::Label subHeaderLabel{"Official IFrame Embed Player • Real-Time Audio Capture & Mixer Routing"};

    // URL input & presets
    juce::Label urlLabel{"Video / Live Stream URL:"};
    juce::TextEditor urlEditor;
    juce::TextButton loadBtn{"Load Video / Home"};
    juce::TextButton openExternalBtn{"Open in Browser"};
    juce::ComboBox presetCombo;

    // Web Browser Component for Embedded YouTube View
#if JUCE_WEB_BROWSER
    std::unique_ptr<juce::WebBrowserComponent> webView;
#endif

    // Fallback/Placeholder UI when WebView is initializing or on systems without WebView2
    juce::Label browserPlaceholder;

    // Transport Bar
    juce::TextButton playBtn{"PLAY"};
    juce::TextButton pauseBtn{"PAUSE"};
    juce::TextButton stopBtn{"STOP"};
    juce::Slider positionSlider;
    juce::Label positionLabel{"00:00 / 00:00"};

    juce::Label volumeLabel{"Player Vol:"};
    juce::Slider volumeSlider;
    juce::TextButton muteBtn{"Mute"};

    // Audio Capture Bridge Configuration
    juce::GroupComponent captureGroup{"captureGroup", "YouTube Audio Capture (via Windows input)"};
    juce::Label captureModeLabel{"Bridge Mode:"};
    juce::ComboBox captureModeCombo;
    juce::Label captureDeviceLabel{"Input Device:"};
    juce::ComboBox captureDeviceCombo;
    juce::TextButton refreshDevicesBtn{"Scan"};
    juce::Label captureNotice{
        "YouTube is not listed as an input. Route browser audio to a virtual cable, then select its input here."};

    // Telemetry and Status Badges
    juce::Label statusBadgePlayer{"PLAYER: IDLE"};
    juce::Label statusBadgeCapture{"BRIDGE: STANDBY"};
    juce::Label statusBadgeSignal{"SIGNAL: SILENT"};
    juce::Label streamBusBadge{"STREAM: ACTIVE"};
    juce::Label monitorBusBadge{"MONITOR: ACTIVE"};

    // Audio Meter
    float currentPeakL = 0.0f;
    float currentPeakR = 0.0f;

    // Player state
    juce::String currentUrl;
    juce::String currentVideoId;
    bool playerIsLoaded = false;
    bool playerIsPlaying = false;
    bool playerIsPaused = false;
    bool playerIsLoading = false;
    bool playerHasError = false;
    bool browserPageLoaded = false;
    bool browserShowingYouTubeSite = false;
    bool playWhenReady = false;
    bool isMuted = false;
    double currentPositionSeconds = 0.0;
    double durationSeconds = 0.0;
    int ticksSinceStatusPoll = 0;
    juce::String playerStatusMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouTubePlayerComponent)
};

} // namespace LiveBus
