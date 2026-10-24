#include "YouTubePlayerComponent.h"

#include <cstring>
#include <cstdio>
#include <cmath>

namespace LiveBus
{
namespace
{
bool isYouTubeHost(const juce::String& host)
{
    return host.equalsIgnoreCase("youtube.com")
        || host.equalsIgnoreCase("www.youtube.com")
        || host.equalsIgnoreCase("m.youtube.com")
        || host.equalsIgnoreCase("music.youtube.com")
        || host.equalsIgnoreCase("youtube-nocookie.com")
        || host.equalsIgnoreCase("www.youtube-nocookie.com")
        || host.equalsIgnoreCase("youtu.be");
}

bool isAllowedYouTubeNavigation(const juce::String& value)
{
    const juce::URL url(value);
    return url.isWellFormed()
        && url.getScheme().equalsIgnoreCase("https")
        && (url.getPort() == 0 || url.getPort() == 443)
        && isYouTubeHost(url.getDomain());
}

bool isVideoId(const juce::String& id)
{
    if (id.length() != 11)
        return false;

    for (auto character : id)
    {
        const bool valid = (character >= 'a' && character <= 'z')
                        || (character >= 'A' && character <= 'Z')
                        || (character >= '0' && character <= '9')
                        || character == '_' || character == '-';
        if (!valid)
            return false;
    }
    return true;
}

#if JUCE_WEB_BROWSER
std::optional<juce::WebBrowserComponent::Resource> getPlayerPageResource(const juce::String& path)
{
    if (path != "/")
        return std::nullopt;

    static constexpr auto html = R"html(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
html,body,#player{width:100%;height:100%;margin:0;background:#090d13;overflow:hidden}
body{font-family:Segoe UI,Arial,sans-serif;color:#cbd5e1}
</style></head><body><div id="player"></div>
<script>
(() => {
  let player = null;
  let apiReady = false;
  let videoId = "";
  let playRequested = false;
  let state = "loading";
  let errorCode = 0;
  let errorMessage = "";
  let autoplayBlocked = false;
  let volume = 85;
  let muted = false;

  const setError = (code, message) => {
    errorCode = code;
    errorMessage = message;
    state = "error";
  };

  window.liveBus = {
    status() {
      if (!player)
        return { state, apiReady, loaded: false, errorCode, errorMessage, autoplayBlocked };
      return {
        state, apiReady, loaded: apiReady && videoId.length > 0,
        errorCode, errorMessage, autoplayBlocked,
        currentTime: apiReady && videoId ? player.getCurrentTime() : 0,
        duration: apiReady && videoId ? player.getDuration() : 0,
        volume: apiReady ? player.getVolume() : volume,
        muted: apiReady ? player.isMuted() : muted
      };
    },
    load(id) {
      videoId = id;
      playRequested = false;
      errorCode = 0;
      errorMessage = "";
      autoplayBlocked = false;
      state = "loading";
      if (apiReady)
        player.cueVideoById({ videoId: id });
    },
    play() {
      if (videoId) {
        playRequested = true;
      }
      if (apiReady && videoId) {
        state = "buffering";
        player.playVideo();
      }
    },
    pause() {
      playRequested = false;
      if (apiReady && videoId)
        player.pauseVideo();
    },
    stop() {
      playRequested = false;
      if (apiReady && videoId)
        player.stopVideo();
    },
    seek(seconds) {
      if (apiReady && videoId && player.getDuration() > 0)
        player.seekTo(seconds, true);
    },
    setVolume(value) {
      volume = Math.max(0, Math.min(100, Math.round(value)));
      if (apiReady)
        player.setVolume(volume);
    },
    setMuted(value) {
      muted = !!value;
      if (apiReady) {
        if (muted) player.mute();
        else player.unMute();
      }
    }
  };

  window.onYouTubeIframeAPIReady = () => {
    player = new YT.Player("player", {
      host: "https://www.youtube-nocookie.com",
      width: "100%",
      height: "100%",
      playerVars: {
        enablejsapi: 1,
        origin: window.location.origin,
        autoplay: 0,
        controls: 1,
        rel: 0
      },
      events: {
        onReady() {
          apiReady = true;
          state = videoId ? "loading" : "ready";
          player.setVolume(volume);
          if (muted) player.mute();
          if (videoId) {
            if (playRequested) player.loadVideoById({ videoId });
            else player.cueVideoById({ videoId });
          }
          playRequested = false;
        },
        onStateChange(event) {
          const states = {
            [-1]: videoId ? "ready" : "idle",
            [0]: "ended",
            [1]: "playing",
            [2]: "paused",
            [3]: "buffering",
            [5]: "ready"
          };
          state = states[event.data] || "unknown";
        },
        onError(event) {
          const errors = {
            2: "Invalid video request.",
            5: "The video cannot be played in the embedded player.",
            100: "Video unavailable, deleted, or private.",
            101: "The video owner does not allow embedded playback.",
            150: "The video owner does not allow embedded playback.",
            153: "YouTube could not verify the embedded player client."
          };
          setError(event.data, errors[event.data] || "YouTube player error.");
        },
        onAutoplayBlocked() {
          autoplayBlocked = true;
          errorMessage = "Playback was blocked; press Play to start it.";
          state = "ready";
        }
      }
    });
  };

  const apiTimeout = window.setTimeout(() => {
    if (!apiReady)
      setError(0, "YouTube player API did not load. Check the network connection.");
  }, 20000);
  const script = document.createElement("script");
  script.src = "https://www.youtube.com/iframe_api";
  script.onerror = () => {
    window.clearTimeout(apiTimeout);
    setError(0, "Could not download the YouTube player API. Check the network connection.");
  };
  document.head.appendChild(script);
})();
</script></body></html>)html";

    juce::WebBrowserComponent::Resource resource;
    resource.mimeType = "text/html";
    const auto* bytes = reinterpret_cast<const std::byte*>(html);
    resource.data.assign(bytes, bytes + std::strlen(html));
    return resource;
}

class YouTubeWebView final : public juce::WebBrowserComponent
{
public:
    explicit YouTubeWebView(const Options& options) : WebBrowserComponent(options) {}

    std::function<void()> onPlayerPageFinished;
    std::function<void(const juce::String&)> onYouTubePageFinished;
    std::function<void(const juce::String&)> onNetworkError;

    bool pageAboutToLoad(const juce::String& url) override
    {
        return url.startsWith(getResourceProviderRoot()) || isAllowedYouTubeNavigation(url);
    }

    void pageFinishedLoading(const juce::String& url) override
    {
        if (url.startsWith(getResourceProviderRoot()) && onPlayerPageFinished)
            onPlayerPageFinished();
        else if (isAllowedYouTubeNavigation(url) && onYouTubePageFinished)
            onYouTubePageFinished(url);
    }

    bool pageLoadHadNetworkError(const juce::String& error) override
    {
        if (onNetworkError)
            onNetworkError(error);
        return false;
    }

    void newWindowAttemptingToLoad(const juce::String& url) override
    {
        const juce::URL target(url);
        if (target.getScheme().equalsIgnoreCase("https") && isYouTubeHost(target.getDomain()))
            target.launchInDefaultBrowser();
    }
};
#endif
}

YouTubePlayerComponent::YouTubePlayerComponent(YouTubeAudioCaptureService& captureService)
    : audioCaptureService(captureService)
{
    headerTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(headerTitle);

    subHeaderLabel.setText("Official YouTube player • Capture uses the selected input endpoint",
                           juce::dontSendNotification);
    subHeaderLabel.setFont(juce::FontOptions(11.0f));
    subHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(subHeaderLabel);

    urlLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    urlLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(urlLabel);

    urlEditor.setText("https://www.youtube.com/");
    urlEditor.setFont(juce::FontOptions(12.0f));
    urlEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff161e2b));
    urlEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xfff8fafc));
    urlEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2563eb));
    addAndMakeVisible(urlEditor);

    loadBtn.addListener(this);
    loadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(loadBtn);

    openExternalBtn.addListener(this);
    openExternalBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    openExternalBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(openExternalBtn);

    presetCombo.addItem("Preset: Lofi Girl 24/7 Radio", 1);
    presetCombo.addItem("Preset: Synthwave Chill Beats", 2);
    presetCombo.addItem("Preset: Audio Calibration Broadcast Stream", 3);
    presetCombo.addItem("Preset: Royalty Free Stream Music", 4);
    presetCombo.setSelectedId(0, juce::dontSendNotification);
    presetCombo.onChange = [this]() {
        switch (presetCombo.getSelectedId())
        {
            case 1: urlEditor.setText("https://www.youtube.com/watch?v=jfKfPfyJRdk"); break;
            case 2: urlEditor.setText("https://www.youtube.com/watch?v=4xDzrJKXOOY"); break;
            case 3: urlEditor.setText("https://www.youtube.com/watch?v=b4bKzz4A4j4"); break;
            case 4: urlEditor.setText("https://www.youtube.com/watch?v=DWcJFNfaw9c"); break;
            default: return;
        }
        loadVideoUrl(urlEditor.getText());
    };
    addAndMakeVisible(presetCombo);

    browserPlaceholder.setText("Embedded YouTube player is starting…",
                               juce::dontSendNotification);
    browserPlaceholder.setFont(juce::FontOptions(13.0f));
    browserPlaceholder.setJustificationType(juce::Justification::centred);
    browserPlaceholder.setColour(juce::Label::backgroundColourId, juce::Colour(0xff090d13));
    browserPlaceholder.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    browserPlaceholder.setColour(juce::Label::outlineColourId, juce::Colour(0xff1e2638));
    addAndMakeVisible(browserPlaceholder);

#if JUCE_WEB_BROWSER
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider(getPlayerPageResource);

#if JUCE_WINDOWS
    auto userDataFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("LiveBusMixer").getChildFile("WebView2");
    if (userDataFolder.createDirectory().failed())
    {
        browserPlaceholder.setText("Could not create the WebView2 profile folder.\n"
                                   "Use Open in Browser to play YouTube.",
                                   juce::dontSendNotification);
    }
    else
    {
        options = options.withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}.withUserDataFolder(userDataFolder));
    }
#endif

    if (juce::WebBrowserComponent::areOptionsSupported(options)
#if JUCE_WINDOWS
        && userDataFolder.isDirectory()
#endif
       )
    {
        auto view = std::make_unique<YouTubeWebView>(options);
        juce::Component::SafePointer<YouTubePlayerComponent> safeThis(this);
        view->onPlayerPageFinished = [safeThis]() {
            if (safeThis != nullptr)
                safeThis->handleBrowserPageFinished();
        };
        view->onYouTubePageFinished = [safeThis](const juce::String& url) {
            if (safeThis != nullptr)
                safeThis->handleYouTubeSitePageFinished(url);
        };
        view->onNetworkError = [safeThis](const juce::String& error) {
            if (safeThis != nullptr)
                safeThis->handleBrowserNetworkError(error);
        };
        webView = std::move(view);
        addAndMakeVisible(webView.get());
        browserPlaceholder.toFront(false);
        webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    }
    else
    {
        browserPlaceholder.setText("WebView2 is unavailable.\n"
                                   "Install the WebView2 Runtime or use Open in Browser.",
                                   juce::dontSendNotification);
    }
#else
    browserPlaceholder.setText("Embedded browser support is unavailable.\n"
                               "Use Open in Browser to play YouTube.",
                               juce::dontSendNotification);
#endif

    playBtn.addListener(this);
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff16a34a));
    playBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(playBtn);

    pauseBtn.addListener(this);
    pauseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
    pauseBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0f172a));
    addAndMakeVisible(pauseBtn);

    stopBtn.addListener(this);
    stopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
    stopBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(stopBtn);

    positionSlider.setRange(0.0, 1.0, 0.1);
    positionSlider.setValue(0.0, juce::dontSendNotification);
    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setEnabled(false);
    positionSlider.addListener(this);
    addAndMakeVisible(positionSlider);

    positionLabel.setFont(juce::FontOptions(11.0f));
    positionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    positionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(positionLabel);

    volumeLabel.setFont(juce::FontOptions(11.0f));
    volumeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(volumeLabel);

    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.85, juce::dontSendNotification);
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    muteBtn.addListener(this);
    muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
    muteBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(muteBtn);

    captureGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff1e2638));
    captureGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(captureGroup);

    captureModeLabel.setFont(juce::FontOptions(11.0f));
    captureModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(captureModeLabel);

    captureModeCombo.addItem("Audio capture off (recommended)", 1);
    captureModeCombo.addItem("Windows input / virtual cable", 2);
    captureModeCombo.addItem("Synthetic test tone (not YouTube)", 3);
    captureModeCombo.setSelectedId(1, juce::dontSendNotification);
    captureModeCombo.onChange = [this]() {
        switch (captureModeCombo.getSelectedId())
        {
            case 1:
                audioCaptureService.setCaptureMode(YouTubeCaptureMode::Off);
                break;
            case 2:
            {
                const auto name = captureDeviceCombo.getText();
                audioCaptureService.startDeviceCapture(name.startsWith("<<") ? juce::String{} : name);
                break;
            }
            case 3:
                audioCaptureService.setCaptureMode(YouTubeCaptureMode::SimulatedTestStream);
                break;
            default:
                break;
        }
        updateStatusBadges();
    };
    addAndMakeVisible(captureModeCombo);

    captureDeviceLabel.setFont(juce::FontOptions(11.0f));
    captureDeviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(captureDeviceLabel);

    captureDeviceCombo.onChange = [this]() {
        if (captureModeCombo.getSelectedId() == 2)
        {
            const auto name = captureDeviceCombo.getText();
            audioCaptureService.startDeviceCapture(name.startsWith("<<") ? juce::String{} : name);
        }
        updateStatusBadges();
    };
    captureDeviceCombo.setTooltip(
        "This list contains Windows recording/input devices, not apps. "
        "For YouTube, route browser audio to a virtual cable and select its recording endpoint.");
    addAndMakeVisible(captureDeviceCombo);

    refreshDevicesBtn.addListener(this);
    refreshDevicesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    refreshDevicesBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(refreshDevicesBtn);

    captureNotice.setFont(juce::FontOptions(10.0f));
    captureNotice.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    captureNotice.setTooltip(
        "The embedded browser is not captured directly. In Windows Volume Mixer, route the app's "
        "output to a virtual cable (for example CABLE Input), then select its recording side "
        "(for example CABLE Output) above.");
    addAndMakeVisible(captureNotice);

    auto setupBadge = [this](juce::Label& badge, const juce::Colour& bg, const juce::Colour& fg) {
        badge.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        badge.setJustificationType(juce::Justification::centred);
        badge.setColour(juce::Label::backgroundColourId, bg);
        badge.setColour(juce::Label::textColourId, fg);
        badge.setColour(juce::Label::outlineColourId, fg.withAlpha(0.3f));
        addAndMakeVisible(badge);
    };
    setupBadge(statusBadgePlayer, juce::Colour(0x2264748b), juce::Colour(0xff94a3b8));
    setupBadge(statusBadgeCapture, juce::Colour(0x2264748b), juce::Colour(0xff94a3b8));
    setupBadge(statusBadgeSignal, juce::Colour(0x2264748b), juce::Colour(0xff94a3b8));
    setupBadge(streamBusBadge, juce::Colour(0x228b5cf6), juce::Colour(0xffc084fc));
    setupBadge(monitorBusBadge, juce::Colour(0x22f59e0b), juce::Colour(0xfffbbf24));
    streamBusBadge.setText("STREAM BUS: ROUTABLE", juce::dontSendNotification);
    monitorBusBadge.setText("MONITOR BUS: ROUTABLE", juce::dontSendNotification);

    audioCaptureService.setCaptureMode(YouTubeCaptureMode::Off);
    refreshCaptureDevices();
    updateStatusBadges();
    startTimerHz(30);
}

YouTubePlayerComponent::~YouTubePlayerComponent()
{
    stopTimer();
#if JUCE_WEB_BROWSER
    webView.reset();
#endif
}

void YouTubePlayerComponent::refreshCaptureDevices()
{
    const auto previousDevice = captureDeviceCombo.getText();
    captureDeviceCombo.clear(juce::dontSendNotification);

    juce::AudioDeviceManager deviceManager;
    juce::OwnedArray<juce::AudioIODeviceType> deviceTypes;
    deviceManager.createAudioDeviceTypes(deviceTypes);

    juce::StringArray devices;
    for (auto* type : deviceTypes)
    {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames(true))
            devices.addIfNotAlreadyThere(name);
    }

    if (devices.isEmpty())
    {
        captureDeviceCombo.addItem("<< No input endpoints found >>", 1);
        captureDeviceCombo.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    int id = 1;
    int previousId = 0;
    for (const auto& name : devices)
    {
        captureDeviceCombo.addItem(name, id);
        if (name == previousDevice)
            previousId = id;
        ++id;
    }
    captureDeviceCombo.setSelectedId(previousId != 0 ? previousId : 1, juce::dontSendNotification);
}

juce::String YouTubePlayerComponent::extractVideoId(const juce::String& input)
{
    const auto value = input.trim();
    if (isVideoId(value))
        return value;

    const juce::URL url(value);
    if (!url.isWellFormed() || !url.getScheme().equalsIgnoreCase("https")
        || (url.getPort() != 0 && url.getPort() != 443)
        || !isYouTubeHost(url.getDomain()))
        return {};

    juce::StringArray pathParts;
    pathParts.addTokens(url.getSubPath(false), "/", "");
    pathParts.trim();
    pathParts.removeEmptyStrings();

    juce::String id;
    const auto host = url.getDomain();
    if (host.equalsIgnoreCase("youtu.be"))
    {
        if (pathParts.size() != 1)
            return {};
        id = pathParts[0];
    }
    else if (pathParts.size() == 1 && pathParts[0] == "watch")
    {
        const auto& names = url.getParameterNames();
        const auto& values = url.getParameterValues();
        for (int i = 0; i < names.size(); ++i)
        {
            if (names[i] == "v")
            {
                id = values[i];
                break;
            }
        }
    }
    else if (pathParts.size() == 2
             && (pathParts[0] == "embed" || pathParts[0] == "shorts" || pathParts[0] == "live"))
    {
        id = pathParts[1];
    }

    return isVideoId(id) ? id : juce::String{};
}

bool YouTubePlayerComponent::isValidYouTubeUrl(const juce::String& url)
{
    return extractVideoId(url).isNotEmpty();
}

bool YouTubePlayerComponent::isYouTubeHomeUrl(const juce::String& value)
{
    if (!isAllowedYouTubeNavigation(value))
        return false;

    const juce::URL url(value);
    auto path = url.getSubPath(false);
    while (path.startsWithChar('/'))
        path = path.substring(1);
    return path.isEmpty();
}

void YouTubePlayerComponent::loadVideoUrl(const juce::String& url)
{
    if (isYouTubeHomeUrl(url))
    {
        currentUrl = "https://www.youtube.com/";
        currentVideoId.clear();
        playerIsLoaded = false;
        playerIsPlaying = false;
        playerIsPaused = false;
        playerIsLoading = true;
        playerHasError = false;
        playWhenReady = false;
        browserPageLoaded = false;
        browserShowingYouTubeSite = true;
        playerStatusMessage = "Loading YouTube home…";
        currentPositionSeconds = 0.0;
        durationSeconds = 0.0;
        positionSlider.setValue(0.0, juce::dontSendNotification);
        positionSlider.setEnabled(false);

#if JUCE_WEB_BROWSER
        if (webView != nullptr)
        {
            browserPlaceholder.setText("Loading YouTube home…", juce::dontSendNotification);
            browserPlaceholder.setVisible(true);
            browserPlaceholder.toFront(false);
            webView->goToURL(currentUrl);
        }
        else
#endif
        {
            playerIsLoading = false;
            playerHasError = true;
            playerStatusMessage = "Embedded browser is unavailable. Use Open in Browser.";
            browserPlaceholder.setText(playerStatusMessage, juce::dontSendNotification);
            browserPlaceholder.setVisible(true);
            browserPlaceholder.toFront(false);
        }
        updateStatusBadges();
        return;
    }

    const auto videoId = extractVideoId(url);
    if (videoId.isEmpty())
    {
        playerHasError = true;
        playerIsLoaded = false;
        playerIsPlaying = false;
        playerIsPaused = false;
        playerIsLoading = false;
        playerStatusMessage = "Enter a valid HTTPS YouTube video URL or 11-character video ID.";
        updateStatusBadges();
        return;
    }

    currentUrl = url.trim();
    currentVideoId = videoId;
    currentPositionSeconds = 0.0;
    durationSeconds = 0.0;
    playerIsLoaded = false;
    playerIsPlaying = false;
    playerIsPaused = false;
    playerIsLoading = true;
    playerHasError = false;
    playWhenReady = false;
    playerStatusMessage = "Waiting for the official YouTube player…";
    positionSlider.setRange(0.0, 1.0, 0.1);
    positionSlider.setValue(0.0, juce::dontSendNotification);
    positionSlider.setEnabled(false);

#if JUCE_WEB_BROWSER
    if (webView == nullptr)
    {
        playerIsLoading = false;
        playerHasError = true;
        playerStatusMessage = "WebView2 is unavailable. Use Open in Browser to play YouTube.";
        browserPlaceholder.setText(playerStatusMessage, juce::dontSendNotification);
        browserPlaceholder.setVisible(true);
        browserPlaceholder.toFront(false);
        updateStatusBadges();
        return;
    }
#else
    playerIsLoading = false;
    playerHasError = true;
    playerStatusMessage = "Embedded browser support is unavailable. Use Open in Browser.";
    browserPlaceholder.setText(playerStatusMessage, juce::dontSendNotification);
    browserPlaceholder.setVisible(true);
    browserPlaceholder.toFront(false);
    updateStatusBadges();
    return;
#endif

    if (browserPageLoaded)
    {
        browserShowingYouTubeSite = false;
        browserPlaceholder.setVisible(false);
        executePlayerScript("window.liveBus.load('" + videoId + "')");
    }
    else
    {
        browserShowingYouTubeSite = false;
        browserPlaceholder.setVisible(true);
        browserPlaceholder.setText("Loading the official embedded player…",
                                  juce::dontSendNotification);
        browserPlaceholder.toFront(false);
#if JUCE_WEB_BROWSER
        if (webView != nullptr)
            webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
#endif
    }
    updateStatusBadges();
}

void YouTubePlayerComponent::play()
{
    if (currentVideoId.isEmpty())
    {
        loadVideoUrl(urlEditor.getText());
        if (currentVideoId.isEmpty())
            return;
    }
    playWhenReady = true;
    executePlayerScript("window.liveBus.play()");
    playerIsLoading = true;
    playerStatusMessage = "Waiting for YouTube playback…";
    updateStatusBadges();
}

void YouTubePlayerComponent::pause()
{
    executePlayerScript("window.liveBus.pause()");
}

void YouTubePlayerComponent::stop()
{
    executePlayerScript("window.liveBus.stop()");
    currentPositionSeconds = 0.0;
    positionSlider.setValue(0.0, juce::dontSendNotification);
}

void YouTubePlayerComponent::seekTo(double seconds)
{
    if (!playerIsLoaded || durationSeconds <= 0.0)
        return;

    const auto bounded = juce::jlimit(0.0, durationSeconds, seconds);
    executePlayerScript("window.liveBus.seek(" + juce::String(bounded, 3) + ")");
}

void YouTubePlayerComponent::setPlayerVolume(float volumeLinear)
{
    const auto volume = juce::jlimit(0.0f, 1.0f, volumeLinear);
    volumeSlider.setValue(volume, juce::dontSendNotification);
    executePlayerScript("window.liveBus.setVolume(" + juce::String(juce::roundToInt(volume * 100.0f)) + ")");
}

void YouTubePlayerComponent::setMuted(bool mute)
{
    isMuted = mute;
    executePlayerScript(juce::String("window.liveBus.setMuted(") + (mute ? "true)" : "false)"));
    muteBtn.setColour(juce::TextButton::buttonColourId,
                      mute ? juce::Colour(0xffdc2626) : juce::Colour(0xff334155));
    muteBtn.setButtonText(mute ? "MUTED" : "Mute");
}

void YouTubePlayerComponent::buttonClicked(juce::Button* button)
{
    if (button == &loadBtn)
    {
        loadVideoUrl(urlEditor.getText());
    }
    else if (button == &openExternalBtn)
    {
        const auto enteredUrl = urlEditor.getText().trim();
        if (isYouTubeHomeUrl(enteredUrl))
        {
            if (!juce::URL("https://www.youtube.com/").launchInDefaultBrowser())
            {
                playerHasError = true;
                playerStatusMessage = "Could not open the system's default browser.";
                updateStatusBadges();
            }
            return;
        }

        const auto id = extractVideoId(urlEditor.getText());
        if (id.isEmpty())
        {
            playerHasError = true;
            playerStatusMessage = "Enter a valid YouTube URL before opening the browser.";
            updateStatusBadges();
            return;
        }
        if (!juce::URL("https://www.youtube.com/watch?v=" + id).launchInDefaultBrowser())
        {
            playerHasError = true;
            playerStatusMessage = "Could not open the system's default browser.";
            updateStatusBadges();
        }
    }
    else if (button == &playBtn)
    {
        play();
    }
    else if (button == &pauseBtn)
    {
        pause();
    }
    else if (button == &stopBtn)
    {
        stop();
    }
    else if (button == &muteBtn)
    {
        setMuted(!isMuted);
    }
    else if (button == &refreshDevicesBtn)
    {
        refreshCaptureDevices();
    }
}

void YouTubePlayerComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &positionSlider)
    {
        currentPositionSeconds = positionSlider.getValue();
        seekTo(currentPositionSeconds);
    }
    else if (slider == &volumeSlider)
    {
        setPlayerVolume(static_cast<float>(volumeSlider.getValue()));
    }
}

void YouTubePlayerComponent::executePlayerScript(const juce::String& script)
{
#if JUCE_WEB_BROWSER
    if (webView != nullptr && browserPageLoaded)
        webView->evaluateJavascript(script);
#else
    juce::ignoreUnused(script);
#endif
}

void YouTubePlayerComponent::handleBrowserPageFinished()
{
    browserPageLoaded = true;
    browserShowingYouTubeSite = false;
    playerIsLoading = false;
    playerHasError = false;
    playBtn.setEnabled(true);
    pauseBtn.setEnabled(true);
    stopBtn.setEnabled(true);
    volumeSlider.setEnabled(true);
    muteBtn.setEnabled(true);
    browserPlaceholder.setVisible(false);
    setPlayerVolume(static_cast<float>(volumeSlider.getValue()));
    setMuted(isMuted);
    if (currentVideoId.isNotEmpty())
    {
        executePlayerScript("window.liveBus.load('" + currentVideoId + "')");
        if (playWhenReady)
            executePlayerScript("window.liveBus.play()");
    }
    playWhenReady = false;
    if (currentVideoId.isEmpty() && currentUrl.isEmpty())
    {
        browserPageLoaded = false;
        browserShowingYouTubeSite = true;
        currentUrl = "https://www.youtube.com/";
        urlEditor.setText(currentUrl, juce::dontSendNotification);
        playerStatusMessage = "Loading YouTube home…";
        browserPlaceholder.setText(playerStatusMessage, juce::dontSendNotification);
        browserPlaceholder.setVisible(true);
        browserPlaceholder.toFront(false);
#if JUCE_WEB_BROWSER
        webView->goToURL("https://www.youtube.com/");
#endif
    }
    updateStatusBadges();
}

void YouTubePlayerComponent::handleYouTubeSitePageFinished(const juce::String& url)
{
    browserPageLoaded = false;
    browserShowingYouTubeSite = true;
    playerIsLoaded = false;
    playerIsPlaying = false;
    playerIsPaused = false;
    playerIsLoading = false;
    playerHasError = false;
    currentUrl = url;
    urlEditor.setText(url, juce::dontSendNotification);
    currentVideoId.clear();
    currentPositionSeconds = 0.0;
    durationSeconds = 0.0;
    positionSlider.setValue(0.0, juce::dontSendNotification);
    positionSlider.setEnabled(false);
    playBtn.setEnabled(false);
    pauseBtn.setEnabled(false);
    stopBtn.setEnabled(false);
    volumeSlider.setEnabled(false);
    muteBtn.setEnabled(false);
    positionLabel.setText("YouTube Home", juce::dontSendNotification);
    browserPlaceholder.setVisible(false);
    playerStatusMessage = isYouTubeHomeUrl(url)
        ? "YouTube home"
        : "Browsing YouTube; load a video URL to use mixer transport controls.";
    updateStatusBadges();
}

void YouTubePlayerComponent::handleBrowserNetworkError(const juce::String& error)
{
    browserPageLoaded = false;
    browserShowingYouTubeSite = false;
    playerHasError = true;
    playerIsLoading = false;
    playerStatusMessage = "Embedded browser failed to load: " + error;
    browserPlaceholder.setText(playerStatusMessage + "\nUse Open in Browser to play YouTube.",
                               juce::dontSendNotification);
    browserPlaceholder.setVisible(true);
    browserPlaceholder.toFront(false);
    updateStatusBadges();
}

void YouTubePlayerComponent::pollPlayerStatus()
{
#if JUCE_WEB_BROWSER
    if (webView == nullptr || !browserPageLoaded)
        return;

    juce::Component::SafePointer<YouTubePlayerComponent> safeThis(this);
    webView->evaluateJavascript(
        "JSON.stringify(window.liveBus ? window.liveBus.status() : {state:'loading'})",
        [safeThis](juce::WebBrowserComponent::EvaluationResult evaluation) {
            if (safeThis == nullptr)
                return;
            const auto* result = evaluation.getResult();
            if (result == nullptr)
            {
                const auto* error = evaluation.getError();
                if (error != nullptr)
                    safeThis->handleBrowserNetworkError("Player status failed: " + error->message);
                return;
            }
            const auto parsed = juce::JSON::parse(result->toString());
            if (!parsed.isVoid() && !parsed.isUndefined())
                safeThis->handlePlayerStatus(parsed);
        });
#endif
}

void YouTubePlayerComponent::handlePlayerStatus(const juce::var& status)
{
    const auto state = status.getProperty("state", {}).toString();
    playerIsLoaded = static_cast<bool>(status.getProperty("loaded", false));
    playerIsPlaying = state == "playing";
    playerIsPaused = state == "paused";
    playerIsLoading = state == "loading" || state == "buffering";
    playerHasError = state == "error";

    const auto errorMessage = status.getProperty("errorMessage", {}).toString();
    const bool autoplayBlocked = static_cast<bool>(status.getProperty("autoplayBlocked", false));
    if (errorMessage.isNotEmpty())
        playerStatusMessage = errorMessage;
    else if (playerHasError)
        playerStatusMessage = "YouTube player reported an error.";
    else
        playerStatusMessage.clear();

    const auto position = static_cast<double>(status.getProperty("currentTime", 0.0));
    const auto duration = static_cast<double>(status.getProperty("duration", 0.0));
    currentPositionSeconds = std::isfinite(position) ? juce::jmax(0.0, position) : 0.0;
    durationSeconds = std::isfinite(duration) ? juce::jmax(0.0, duration) : 0.0;

    if (durationSeconds > 0.0)
    {
        positionSlider.setRange(0.0, durationSeconds, 0.1);
        positionSlider.setEnabled(playerIsLoaded);
        positionSlider.setValue(juce::jlimit(0.0, durationSeconds, currentPositionSeconds),
                               juce::dontSendNotification);
    }
    else
    {
        positionSlider.setEnabled(false);
    }

    const auto actualVolume = static_cast<double>(status.getProperty("volume", volumeSlider.getValue() * 100.0));
    volumeSlider.setValue(juce::jlimit(0.0, 1.0, actualVolume / 100.0), juce::dontSendNotification);
    isMuted = static_cast<bool>(status.getProperty("muted", isMuted));
    muteBtn.setButtonText(isMuted ? "MUTED" : "Mute");
    muteBtn.setColour(juce::TextButton::buttonColourId,
                      isMuted ? juce::Colour(0xffdc2626) : juce::Colour(0xff334155));

    if (autoplayBlocked)
        playerStatusMessage = "Autoplay blocked; press Play.";
    if (playerHasError)
    {
        browserPlaceholder.setText(playerStatusMessage + "\nUse Open in Browser to play YouTube.",
                                   juce::dontSendNotification);
        browserPlaceholder.setVisible(true);
        browserPlaceholder.toFront(false);
    }
    else if (browserPageLoaded)
    {
        browserPlaceholder.setVisible(false);
    }
    updateStatusBadges();
}

void YouTubePlayerComponent::timerCallback()
{
    if (++ticksSinceStatusPoll >= 8)
    {
        ticksSinceStatusPoll = 0;
        pollPlayerStatus();
    }

    const auto minutes = static_cast<int>(currentPositionSeconds / 60.0);
    const auto seconds = static_cast<int>(currentPositionSeconds) % 60;
    char timeText[48];
    if (durationSeconds > 0.0)
    {
        const auto durationMinutes = static_cast<int>(durationSeconds / 60.0);
        const auto durationRemaining = static_cast<int>(durationSeconds) % 60;
        std::snprintf(timeText, sizeof(timeText), "%02d:%02d / %02d:%02d",
                      minutes, seconds, durationMinutes, durationRemaining);
    }
    else
    {
        std::snprintf(timeText, sizeof(timeText), "%02d:%02d / %s",
                      minutes, seconds, playerIsLoaded ? "LIVE" : "--:--");
    }
    positionLabel.setText(timeText, juce::dontSendNotification);

    currentPeakL = audioCaptureService.getPeakLevelL();
    currentPeakR = audioCaptureService.getPeakLevelR();
    updateStatusBadges();
    repaint();
}

void YouTubePlayerComponent::updateStatusBadges()
{
    if (playerHasError)
    {
        statusBadgePlayer.setText("PLAYER: ERROR", juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x33ef4444));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
        statusBadgePlayer.setTooltip(playerStatusMessage);
    }
    else if (browserShowingYouTubeSite)
    {
        statusBadgePlayer.setText(isYouTubeHomeUrl(currentUrl) ? "YOUTUBE: HOME" : "YOUTUBE: WEB",
                                  juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x222563eb));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
        statusBadgePlayer.setTooltip(playerStatusMessage);
    }
    else if (playerIsPlaying)
    {
        statusBadgePlayer.setText("PLAYER: PLAYING", juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xff34d399));
        statusBadgePlayer.setTooltip(currentVideoId);
    }
    else if (playerIsPaused)
    {
        statusBadgePlayer.setText("PLAYER: PAUSED", juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x22f59e0b));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
        statusBadgePlayer.setTooltip(playerStatusMessage);
    }
    else if (playerStatusMessage.startsWith("Autoplay blocked"))
    {
        statusBadgePlayer.setText("PLAYER: PLAY REQUIRED", juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x22f59e0b));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
        statusBadgePlayer.setTooltip(playerStatusMessage);
    }
    else if (playerIsLoading)
    {
        statusBadgePlayer.setText("PLAYER: LOADING", juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x222563eb));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
        statusBadgePlayer.setTooltip(playerStatusMessage);
    }
    else
    {
        statusBadgePlayer.setText(playerIsLoaded ? "PLAYER: READY" : "PLAYER: STANDBY",
                                  juce::dontSendNotification);
        statusBadgePlayer.setColour(juce::Label::backgroundColourId, juce::Colour(0x2264748b));
        statusBadgePlayer.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        statusBadgePlayer.setTooltip(playerStatusMessage);
    }

    const auto mode = audioCaptureService.getCaptureMode();
    if (mode == YouTubeCaptureMode::Off)
    {
        statusBadgeCapture.setText("CAPTURE: OFF", juce::dontSendNotification);
        statusBadgeCapture.setColour(juce::Label::backgroundColourId, juce::Colour(0x2264748b));
        statusBadgeCapture.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    }
    else if (mode == YouTubeCaptureMode::SimulatedTestStream)
    {
        const bool active = audioCaptureService.isSimulatedStreamEnabled();
        statusBadgeCapture.setText(active ? "CAPTURE: TEST TONE" : "CAPTURE: OFF",
                                   juce::dontSendNotification);
        statusBadgeCapture.setColour(juce::Label::backgroundColourId,
                                     active ? juce::Colour(0x220284c7) : juce::Colour(0x2264748b));
        statusBadgeCapture.setColour(juce::Label::textColourId,
                                     active ? juce::Colour(0xff38bdf8) : juce::Colour(0xff94a3b8));
    }
    else
    {
        const bool running = audioCaptureService.isCaptureActive();
        statusBadgeCapture.setText(running ? "INPUT: CONNECTED" : "INPUT: ERROR",
                                   juce::dontSendNotification);
        statusBadgeCapture.setColour(juce::Label::backgroundColourId,
                                     running ? juce::Colour(0x2210b981) : juce::Colour(0x22ef4444));
        statusBadgeCapture.setColour(juce::Label::textColourId,
                                     running ? juce::Colour(0xff34d399) : juce::Colour(0xfff87171));
        statusBadgeCapture.setTooltip(audioCaptureService.getStatusDescription());
    }

    const bool signal = audioCaptureService.isSignalDetected();
    statusBadgeSignal.setText(signal ? "SIGNAL: DETECTED" : "SIGNAL: SILENT",
                              juce::dontSendNotification);
    statusBadgeSignal.setColour(juce::Label::backgroundColourId,
                                signal ? juce::Colour(0x2210b981) : juce::Colour(0x22334155));
    statusBadgeSignal.setColour(juce::Label::textColourId,
                                signal ? juce::Colour(0xff34d399) : juce::Colour(0xff64748b));
}

void YouTubePlayerComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff0d1117));
    graphics.setColour(juce::Colour(0xff1e2638));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 6.0f, 1.0f);

    const int meterX = getWidth() - 36;
    const int meterY = getHeight() - 110;
    constexpr int meterWidth = 12;
    constexpr int meterHeight = 92;
    graphics.setColour(juce::Colour(0xff161e2b));
    graphics.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY),
                                  static_cast<float>(meterWidth), static_cast<float>(meterHeight), 2.0f);
    graphics.fillRoundedRectangle(static_cast<float>(meterX + 16), static_cast<float>(meterY),
                                  static_cast<float>(meterWidth), static_cast<float>(meterHeight), 2.0f);

    const auto drawMeter = [&graphics, meterY, meterWidth, meterHeight](int x, float value) {
        const auto level = juce::jlimit(0.0f, 1.0f, value);
        const auto colour = level > 0.95f ? juce::Colour(0xffef4444)
                          : level > 0.70f ? juce::Colour(0xfff59e0b)
                                          : juce::Colour(0xff10b981);
        graphics.setColour(colour);
        graphics.fillRoundedRectangle(static_cast<float>(x),
            static_cast<float>(meterY + meterHeight * (1.0f - level)),
            static_cast<float>(meterWidth), static_cast<float>(meterHeight * level), 2.0f);
    };
    drawMeter(meterX, currentPeakL);
    drawMeter(meterX + 16, currentPeakR);
    graphics.setColour(juce::Colour(0xff64748b));
    graphics.setFont(juce::FontOptions(9.0f));
    graphics.drawText("VU", meterX, meterY - 14, 28, 12, juce::Justification::centred);
}

void YouTubePlayerComponent::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    headerTitle.setBounds(bounds.getX(), bounds.getY(), bounds.getWidth() - 360, 20);
    subHeaderLabel.setBounds(bounds.getX(), bounds.getY() + 18, bounds.getWidth() - 360, 16);

    constexpr int badgeWidth = 110;
    constexpr int badgeHeight = 22;
    const int badgeX = bounds.getRight() - (badgeWidth * 3 + 12);
    statusBadgePlayer.setBounds(badgeX, bounds.getY() + 6, badgeWidth, badgeHeight);
    statusBadgeCapture.setBounds(badgeX + badgeWidth + 6, bounds.getY() + 6, badgeWidth, badgeHeight);
    statusBadgeSignal.setBounds(badgeX + (badgeWidth + 6) * 2, bounds.getY() + 6, badgeWidth, badgeHeight);

    const int urlY = bounds.getY() + 42;
    urlLabel.setBounds(bounds.getX(), urlY + 4, 150, 20);
    const int urlEditWidth = bounds.getWidth() - 605;
    urlEditor.setBounds(bounds.getX() + 155, urlY, urlEditWidth, 26);
    loadBtn.setBounds(bounds.getX() + 160 + urlEditWidth, urlY, 120, 26);
    presetCombo.setBounds(bounds.getX() + 285 + urlEditWidth, urlY, 190, 26);
    openExternalBtn.setBounds(bounds.getX() + 475 + urlEditWidth, urlY, 125, 26);

    constexpr int bottomHeight = 120;
    const int bottomY = bounds.getBottom() - bottomHeight;
    const int viewY = urlY + 34;
    const int viewHeight = bottomY - viewY - 8;
    const auto viewBounds = juce::Rectangle<int>(bounds.getX(), viewY, bounds.getWidth(), viewHeight);
#if JUCE_WEB_BROWSER
    if (webView != nullptr)
        webView->setBounds(viewBounds);
#endif
    browserPlaceholder.setBounds(viewBounds);

    playBtn.setBounds(bounds.getX(), bottomY, 65, 26);
    pauseBtn.setBounds(bounds.getX() + 70, bottomY, 70, 26);
    stopBtn.setBounds(bounds.getX() + 145, bottomY, 65, 26);
    const int positionWidth = bounds.getWidth() - 560;
    positionSlider.setBounds(bounds.getX() + 220, bottomY + 2, positionWidth, 22);
    positionLabel.setBounds(bounds.getX() + 225 + positionWidth, bottomY + 2, 95, 22);
    volumeLabel.setBounds(bounds.getX() + 325 + positionWidth, bottomY + 2, 65, 22);
    volumeSlider.setBounds(bounds.getX() + 390 + positionWidth, bottomY + 2, 90, 22);
    muteBtn.setBounds(bounds.getX() + 485 + positionWidth, bottomY, 65, 26);

    const int captureY = bottomY + 32;
    captureGroup.setBounds(bounds.getX(), captureY, bounds.getWidth() - 48, 86);
    const int inputY = captureY + 22;
    captureModeLabel.setBounds(bounds.getX() + 12, inputY, 75, 22);
    captureModeCombo.setBounds(bounds.getX() + 90, inputY, 260, 24);
    captureDeviceLabel.setBounds(bounds.getX() + 365, inputY, 100, 22);
    captureDeviceCombo.setBounds(bounds.getX() + 470, inputY, 280, 24);
    refreshDevicesBtn.setBounds(bounds.getX() + 755, inputY, 55, 24);

    const int routeY = captureY + 54;
    streamBusBadge.setBounds(bounds.getX() + 12, routeY, 150, 22);
    monitorBusBadge.setBounds(bounds.getX() + 170, routeY, 150, 22);
    captureNotice.setBounds(bounds.getX() + 340, routeY, bounds.getWidth() - 390, 22);
}

} // namespace LiveBus
