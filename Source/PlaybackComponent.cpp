#include "PlaybackComponent.h"

namespace LiveBus
{

// =============================================================================
// Playlist Model for ListBox
// =============================================================================
class PlaybackComponent::PlaylistModel : public juce::ListBoxModel
{
public:
    PlaylistModel(AudioFilePlayer& p, PlaybackComponent& parent) : player(p), owner(parent) {}

    int getNumRows() override
    {
        return (int)player.getPlaylist().size();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        auto playlist = player.getPlaylist();
        if (rowNumber < 0 || rowNumber >= (int)playlist.size())
            return;

        const auto& item = playlist[rowNumber];
        bool isCurrent = (rowNumber == player.getCurrentTrackIndex());

        if (rowIsSelected || isCurrent)
        {
            g.fillAll(isCurrent ? juce::Colour(0x332563eb) : juce::Colour(0x223b82f6));
            g.setColour(juce::Colour(0xff3b82f6));
            g.drawRect(0, 0, width, height, 1);
        }

        g.setFont(juce::FontOptions(12.0f));
        g.setColour(isCurrent ? juce::Colour(0xff00e5ff) : juce::Colours::white);

        // Track index
        g.drawText(juce::String(rowNumber + 1), 5, 0, 24, height, juce::Justification::centredLeft);

        // Title
        g.drawText(item.title, 35, 0, width - 180, height, juce::Justification::centredLeft, true);

        // Duration
        g.setColour(juce::Colours::grey);
        g.drawText(item.getFormattedDuration(), width - 140, 0, 50, height, juce::Justification::centredRight);

        // Sample rate
        g.drawText(juce::String((int)(item.sampleRate / 1000.0)) + "k", width - 85, 0, 30, height, juce::Justification::centredRight);

        // Delete glyph
        g.setColour(juce::Colour(0xffef4444));
        g.drawText("[X]", width - 40, 0, 30, height, juce::Justification::centred);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        player.selectTrack(row);
        player.start();
        owner.updateUIFromState();
    }

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (e.x > owner.playlistListBox.getWidth() - 45)
        {
            player.removeTrackFromPlaylist(row);
            owner.playlistListBox.updateContent();
            owner.playlistListBox.repaint();
        }
        else
        {
            player.selectTrack(row);
            owner.updateUIFromState();
        }
    }

private:
    AudioFilePlayer& player;
    PlaybackComponent& owner;
};

// =============================================================================
// Marker Model for ListBox
// =============================================================================
class PlaybackComponent::MarkerModel : public juce::ListBoxModel
{
public:
    MarkerModel(AudioFilePlayer& p, PlaybackComponent& parent) : player(p), owner(parent) {}

    int getNumRows() override
    {
        return (int)player.getMarkers().size();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        auto markers = player.getMarkers();
        if (rowNumber < 0 || rowNumber >= (int)markers.size())
            return;

        const auto& m = markers[rowNumber];

        if (rowIsSelected)
        {
            g.fillAll(juce::Colour(0x33f59e0b));
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawRect(0, 0, width, height, 1);
        }

        g.setFont(juce::FontOptions(12.0f));
        g.setColour(juce::Colour(0xfff59e0b));

        // Flag icon
        g.drawText("P" + juce::String(m.id), 5, 0, 28, height, juce::Justification::centredLeft);

        // Marker name
        g.setColour(juce::Colours::white);
        g.drawText(m.name, 40, 0, width - 170, height, juce::Justification::centredLeft, true);

        // Time
        g.setColour(juce::Colour(0xff00e5ff));
        g.drawText(m.getFormattedTime(), width - 130, 0, 70, height, juce::Justification::centredRight);

        // Delete glyph
        g.setColour(juce::Colour(0xffef4444));
        g.drawText("[X]", width - 40, 0, 30, height, juce::Justification::centred);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        auto markers = player.getMarkers();
        if (row < 0 || row >= (int)markers.size()) return;

        if (e.x > owner.markerListBox.getWidth() - 45)
        {
            player.removeMarker(markers[row].id);
            owner.markerListBox.updateContent();
            owner.markerListBox.repaint();
        }
        else
        {
            player.setPosition(markers[row].positionSeconds);
            owner.updateUIFromState();
        }
    }

private:
    AudioFilePlayer& player;
    PlaybackComponent& owner;
};

// =============================================================================
// Waveform Viewport Implementation
// =============================================================================
PlaybackComponent::WaveformViewport::WaveformViewport(AudioFilePlayer& p) : player(p)
{
}

void PlaybackComponent::WaveformViewport::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (b.getHeight() < 40.0f) return;

    float overviewH = 36.0f;
    float rulerH = 20.0f;
    float detailH = b.getHeight() - overviewH - rulerH;

    auto overviewBounds = b.removeFromTop(overviewH);
    auto rulerBounds = b.removeFromTop(rulerH);
    auto detailBounds = b;

    double curPos = player.getCurrentPosition();
    double loopStart = player.getLoopStart();
    double loopEnd = player.getLoopEnd();
    bool loopActive = player.isLoopEnabled();
    auto markers = player.getMarkers();
    double totalDur = player.getLengthInSeconds();

    if (totalDur <= 0.0) totalDur = 10.0;
    if (viewEndTime <= viewStartTime)
    {
        viewStartTime = 0.0;
        viewEndTime = totalDur;
    }

    // 1. Draw Waveform Overview
    player.getWaveformCache().drawOverview(g, overviewBounds, loopStart, loopEnd, curPos, viewStartTime, viewEndTime, markers);

    // 2. Draw Ruler / Timeline
    g.fillAll(juce::Colour(0xff161c24));
    g.setColour(juce::Colour(0xff2d3748));
    g.drawHorizontalLine((int)rulerBounds.getBottom(), rulerBounds.getX(), rulerBounds.getRight());

    double viewSpan = viewEndTime - viewStartTime;
    if (viewSpan > 0.0)
    {
        // Interval calculation (1s, 5s, 10s, 30s)
        double interval = 5.0;
        if (viewSpan <= 5.0) interval = 1.0;
        else if (viewSpan <= 15.0) interval = 2.0;
        else if (viewSpan <= 60.0) interval = 5.0;
        else if (viewSpan <= 300.0) interval = 15.0;
        else interval = 30.0;

        double firstTick = std::ceil(viewStartTime / interval) * interval;
        g.setFont(juce::FontOptions(10.0f));
        g.setColour(juce::Colours::grey);

        for (double t = firstTick; t <= viewEndTime; t += interval)
        {
            float tx = rulerBounds.getX() + (float)((t - viewStartTime) / viewSpan) * rulerBounds.getWidth();
            g.drawVerticalLine((int)tx, rulerBounds.getBottom() - 5.0f, rulerBounds.getBottom());

            int totalSecs = (int)t;
            int mins = totalSecs / 60;
            int secs = totalSecs % 60;
            juce::String timeStr = juce::String::formatted("%02d:%02d", mins, secs);
            g.drawText(timeStr, (int)tx - 25, (int)rulerBounds.getY(), 50, (int)rulerH - 4, juce::Justification::centred, false);
        }
    }

    // 3. Draw Waveform Detail
    player.getWaveformCache().drawDetail(g, detailBounds, viewStartTime, viewEndTime, curPos, loopStart, loopEnd, loopActive, markers);
}

void PlaybackComponent::WaveformViewport::mouseDown(const juce::MouseEvent& e)
{
    float overviewH = 36.0f;
    float rulerH = 20.0f;
    double totalDur = player.getLengthInSeconds();
    if (totalDur <= 0.0) return;

    if (e.y <= overviewH)
    {
        // Click on Overview: seek directly
        float norm = juce::jlimit(0.0f, 1.0f, (float)e.x / (float)getWidth());
        player.setPosition(norm * totalDur);
        currentDrag = OverviewSeek;
        repaint();
    }
    else
    {
        // Click on Detail: check handles or seek
        double viewSpan = viewEndTime - viewStartTime;
        if (viewSpan <= 0.0) return;

        double clickedTime = viewStartTime + ((double)e.x / (double)getWidth()) * viewSpan;
        float curX = (float)((player.getCurrentPosition() - viewStartTime) / viewSpan) * (float)getWidth();
        float startX = (float)((player.getLoopStart() - viewStartTime) / viewSpan) * (float)getWidth();
        float endX = (float)((player.getLoopEnd() - viewStartTime) / viewSpan) * (float)getWidth();

        if (std::abs(e.x - startX) <= 8.0f)
            currentDrag = LoopStart;
        else if (std::abs(e.x - endX) <= 8.0f)
            currentDrag = LoopEnd;
        else
        {
            currentDrag = Playhead;
            player.setPosition(clickedTime);
        }
        repaint();
    }
}

void PlaybackComponent::WaveformViewport::mouseDrag(const juce::MouseEvent& e)
{
    double totalDur = player.getLengthInSeconds();
    if (totalDur <= 0.0) return;

    if (currentDrag == OverviewSeek)
    {
        float norm = juce::jlimit(0.0f, 1.0f, (float)e.x / (float)getWidth());
        player.setPosition(norm * totalDur);
        repaint();
    }
    else if (currentDrag == Playhead)
    {
        double viewSpan = viewEndTime - viewStartTime;
        double newTime = viewStartTime + ((double)e.x / (double)getWidth()) * viewSpan;
        player.setPosition(juce::jlimit(0.0, totalDur, newTime));
        repaint();
    }
    else if (currentDrag == LoopStart)
    {
        double viewSpan = viewEndTime - viewStartTime;
        double newStart = viewStartTime + ((double)e.x / (double)getWidth()) * viewSpan;
        player.setLoopRange(newStart, player.getLoopEnd());
        repaint();
    }
    else if (currentDrag == LoopEnd)
    {
        double viewSpan = viewEndTime - viewStartTime;
        double newEnd = viewStartTime + ((double)e.x / (double)getWidth()) * viewSpan;
        player.setLoopRange(player.getLoopStart(), newEnd);
        repaint();
    }
}

void PlaybackComponent::WaveformViewport::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY > 0.0f)
        zoomIn();
    else if (wheel.deltaY < 0.0f)
        zoomOut();
}

void PlaybackComponent::WaveformViewport::zoomIn()
{
    double span = viewEndTime - viewStartTime;
    if (span <= 1.0) return;

    double mid = (viewStartTime + viewEndTime) * 0.5;
    double newHalf = (span * 0.75) * 0.5;
    viewStartTime = juce::jmax(0.0, mid - newHalf);
    viewEndTime = viewStartTime + (newHalf * 2.0);
    repaint();
}

void PlaybackComponent::WaveformViewport::zoomOut()
{
    double totalDur = juce::jmax(10.0, player.getLengthInSeconds());
    double span = viewEndTime - viewStartTime;
    if (span >= totalDur) return;

    double mid = (viewStartTime + viewEndTime) * 0.5;
    double newHalf = (span * 1.33) * 0.5;
    viewStartTime = juce::jmax(0.0, mid - newHalf);
    viewEndTime = juce::jmin(totalDur, viewStartTime + (newHalf * 2.0));
    repaint();
}

void PlaybackComponent::WaveformViewport::zoomFit()
{
    viewStartTime = 0.0;
    viewEndTime = juce::jmax(1.0, player.getLengthInSeconds());
    repaint();
}

// =============================================================================
// PlaybackComponent Main Implementation
// =============================================================================
PlaybackComponent::PlaybackComponent(AudioFilePlayer& p, TestSignalGenerator& g)
    : audioPlayer(p), signalGen(g), waveformView(p)
{
    audioPlayer.addChangeListener(this);

    // 1. Header Controls
    headerTitleLabel.setText("Streaming Audio File Player", juce::dontSendNotification);
    headerTitleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    headerTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
    addAndMakeVisible(headerTitleLabel);

    bypassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    bypassBtn.onClick = [this]() {
        bool m = !audioPlayer.isMuted();
        audioPlayer.setMute(m);
        bypassBtn.setButtonText(m ? "Bypassed" : "Bypass");
    };
    addAndMakeVisible(bypassBtn);

    modeSwitchBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e3a8a));
    modeSwitchBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff60a5fa));
    modeSwitchBtn.onClick = [this]() {
        setShowTestSignal(!showingTestSignal);
    };
    addAndMakeVisible(modeSwitchBtn);

    optionsMenuBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    addAndMakeVisible(optionsMenuBtn);

    // 2. Waveform Viewport
    addAndMakeVisible(waveformView);

    // 3. Timeline Bar & Tools
    timeDisplayCurrent.setText("00:00.000", juce::dontSendNotification);
    timeDisplayCurrent.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    timeDisplayCurrent.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b)); // Amber Gold
    addAndMakeVisible(timeDisplayCurrent);

    timeDisplayTotal.setText("00:00.000", juce::dontSendNotification);
    timeDisplayTotal.setFont(juce::FontOptions(14.0f));
    timeDisplayTotal.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(timeDisplayTotal);

    zoomInBtn.onClick = [this]() { waveformView.zoomIn(); };
    zoomOutBtn.onClick = [this]() { waveformView.zoomOut(); };
    zoomFitBtn.onClick = [this]() { waveformView.zoomFit(); };
    addAndMakeVisible(zoomInBtn);
    addAndMakeVisible(zoomOutBtn);
    addAndMakeVisible(zoomFitBtn);

    addMarkerBtn.onClick = [this]() {
        audioPlayer.addMarker(audioPlayer.getCurrentPosition());
        if (markerModel) markerListBox.updateContent();
    };
    addAndMakeVisible(addMarkerBtn);

    currentTrackNameLabel.setText("No File Loaded", juce::dontSendNotification);
    currentTrackNameLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    currentTrackNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
    addAndMakeVisible(currentTrackNameLabel);

    // 4. Lower Left Panel (Tabs: FILE LIST VIEW vs MARKER LIST VIEW)
    fileListTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe11d48)); // Coral Red Active
    fileListTabBtn.onClick = [this]() {
        activeSubTab = SubTabFileList;
        fileListTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe11d48));
        markerListTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
        resized();
    };
    addAndMakeVisible(fileListTabBtn);

    markerListTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
    markerListTabBtn.onClick = [this]() {
        activeSubTab = SubTabMarkerList;
        markerListTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe11d48));
        fileListTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2937));
        resized();
    };
    addAndMakeVisible(markerListTabBtn);

    // File List Buttons
    addFilesBtn.onClick = [this]() {
        juce::FileChooser chooser("Select Audio Files to Play", juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                  "*.wav;*.mp3;*.flac;*.aiff;*.ogg");
        if (chooser.browseForMultipleFilesToOpen())
        {
            auto files = chooser.getResults();
            for (const auto& f : files)
                audioPlayer.addTrackToPlaylist(f);

            playlistListBox.updateContent();
            updateUIFromState();
        }
    };
    addAndMakeVisible(addFilesBtn);

    shuffleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    addAndMakeVisible(shuffleBtn);

    clearPlaylistBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    clearPlaylistBtn.onClick = [this]() {
        audioPlayer.clearPlaylist();
        playlistListBox.updateContent();
        updateUIFromState();
    };
    addAndMakeVisible(clearPlaylistBtn);

    savePlaylistBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    savePlaylistBtn.onClick = [this]() {
        juce::FileChooser chooser("Save Playlist XML", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
        if (chooser.browseForFileToSave(true))
            audioPlayer.savePlaylistXml(chooser.getResult());
    };
    addAndMakeVisible(savePlaylistBtn);

    loadPlaylistBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
    loadPlaylistBtn.onClick = [this]() {
        juce::FileChooser chooser("Load Playlist XML", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
        if (chooser.browseForFileToOpen())
        {
            audioPlayer.loadPlaylistXml(chooser.getResult());
            playlistListBox.updateContent();
            updateUIFromState();
        }
    };
    addAndMakeVisible(loadPlaylistBtn);

    autoNextToggle.setToggleState(audioPlayer.isAutoNextTrack(), juce::dontSendNotification);
    autoNextToggle.onClick = [this]() { audioPlayer.setAutoNextTrack(autoNextToggle.getToggleState()); };
    addAndMakeVisible(autoNextToggle);

    playlistModel = std::make_unique<PlaylistModel>(audioPlayer, *this);
    playlistListBox.setModel(playlistModel.get());
    playlistListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff12171e));
    addAndMakeVisible(playlistListBox);

    markerModel = std::make_unique<MarkerModel>(audioPlayer, *this);
    markerListBox.setModel(markerModel.get());
    markerListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff12171e));
    addChildComponent(markerListBox);

    // 5. Lower Right Panel: Transport, Speed/Pitch, Master Gain
    prevTrackBtn.onClick = [this]() { audioPlayer.prevTrack(); updateUIFromState(); };
    addAndMakeVisible(prevTrackBtn);

    playPauseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb)); // Bright Blue
    playPauseBtn.onClick = [this]() {
        if (audioPlayer.isPlaying()) audioPlayer.pause();
        else audioPlayer.start();
        updateUIFromState();
    };
    addAndMakeVisible(playPauseBtn);

    nextTrackBtn.onClick = [this]() { audioPlayer.nextTrack(); updateUIFromState(); };
    addAndMakeVisible(nextTrackBtn);

    rewindBtn.onClick = [this]() {
        audioPlayer.setPosition(juce::jmax(0.0, audioPlayer.getCurrentPosition() - 5.0));
    };
    addAndMakeVisible(rewindBtn);

    fastFwdBtn.onClick = [this]() {
        audioPlayer.setPosition(juce::jmin(audioPlayer.getLengthInSeconds(), audioPlayer.getCurrentPosition() + 5.0));
    };
    addAndMakeVisible(fastFwdBtn);

    loopBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff59e0b));
    loopBtn.setToggleState(audioPlayer.isLoopEnabled(), juce::dontSendNotification);
    loopBtn.onClick = [this]() {
        audioPlayer.setLoopEnabled(loopBtn.getToggleState());
        waveformView.repaint();
    };
    addAndMakeVisible(loopBtn);

    muteBtn.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffef4444));
    muteBtn.setToggleState(audioPlayer.isMuted(), juce::dontSendNotification);
    muteBtn.onClick = [this]() { audioPlayer.setMute(muteBtn.getToggleState()); };
    addAndMakeVisible(muteBtn);

    jumpLoopStartBtn.onClick = [this]() { audioPlayer.setPosition(audioPlayer.getLoopStart()); };
    addAndMakeVisible(jumpLoopStartBtn);

    stopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c)); // Bright Orange
    stopBtn.onClick = [this]() { audioPlayer.stop(); updateUIFromState(); };
    addAndMakeVisible(stopBtn);

    jumpLoopEndBtn.onClick = [this]() { audioPlayer.setPosition(audioPlayer.getLoopEnd()); };
    addAndMakeVisible(jumpLoopEndBtn);

    routeStreamToggle.setToggleState(audioPlayer.isRouteStream(), juce::dontSendNotification);
    routeStreamToggle.onClick = [this]() { audioPlayer.setRouteStream(routeStreamToggle.getToggleState()); };
    addAndMakeVisible(routeStreamToggle);

    routeMonitorToggle.setToggleState(audioPlayer.isRouteMonitor(), juce::dontSendNotification);
    routeMonitorToggle.onClick = [this]() { audioPlayer.setRouteMonitor(routeMonitorToggle.getToggleState()); };
    addAndMakeVisible(routeMonitorToggle);

    // Speed Knob
    speedKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    speedKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    speedKnob.setRange(50.0, 200.0, 1.0);
    speedKnob.setValue(100.0);
    speedKnob.setTextValueSuffix(" %");
    speedKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00e5ff));
    speedKnob.onValueChange = [this]() {
        float ratio = (float)(speedKnob.getValue() / 100.0);
        audioPlayer.setSpeedRatio(ratio);
        // Link pitch display honest reflection
        float semi = (ratio - 1.0f) * 12.0f;
        pitchKnob.setValue(semi, juce::dontSendNotification);
    };
    addAndMakeVisible(speedKnob);

    speedLabel.setText("Speed", juce::dontSendNotification);
    speedLabel.setFont(juce::FontOptions(11.0f));
    speedLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedLabel);

    // Pitch Knob
    pitchKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    pitchKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    pitchKnob.setRange(-12.0, 12.0, 0.1);
    pitchKnob.setValue(0.0);
    pitchKnob.setTextValueSuffix(" st");
    pitchKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xfff59e0b));
    addAndMakeVisible(pitchKnob);

    pitchLabel.setText("Pitch", juce::dontSendNotification);
    pitchLabel.setFont(juce::FontOptions(11.0f));
    pitchLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchLabel);

    pitchLockToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(pitchLockToggle);

    // Master Output Gain Fader
    masterGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    masterGainSlider.setRange(-60.0, 6.0, 0.1);
    masterGainSlider.setValue(0.0);
    masterGainSlider.setTextValueSuffix(" dB");
    masterGainSlider.onValueChange = [this]() {
        float db = (float)masterGainSlider.getValue();
        float lin = std::pow(10.0f, db / 20.0f);
        audioPlayer.setGain(lin);
    };
    addAndMakeVisible(masterGainSlider);

    masterGainLabel.setText("Output Level", juce::dontSendNotification);
    masterGainLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(masterGainLabel);

    // =========================================================================
    // Test Signal Generator Panel Components
    // =========================================================================
    testSignalGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff3b82f6));
    testSignalGroup.setColour(juce::GroupComponent::textColourId, juce::Colour(0xff60a5fa));
    addChildComponent(testSignalGroup);

    signalTypeCombo.addItem("Sine Wave (Harmonic Test)", 1);
    signalTypeCombo.addItem("White Noise (Full Bandwidth)", 2);
    signalTypeCombo.addItem("Pink Noise (Paul Kellet -3dB/Oct RTA)", 3);
    signalTypeCombo.addItem("Impulse / Click (1s Transient Test)", 4);
    signalTypeCombo.setSelectedId(1, juce::dontSendNotification);
    signalTypeCombo.onChange = [this]() {
        signalGen.setSignalType(static_cast<SignalType>(signalTypeCombo.getSelectedId() - 1));
    };
    addChildComponent(signalTypeCombo);

    signalFreqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    signalFreqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    signalFreqSlider.setRange(20.0, 20000.0, 1.0);
    signalFreqSlider.setSkewFactorFromMidPoint(1000.0);
    signalFreqSlider.setValue(440.0);
    signalFreqSlider.setTextValueSuffix(" Hz");
    signalFreqSlider.onValueChange = [this]() {
        signalGen.setFrequency(signalFreqSlider.getValue());
    };
    addChildComponent(signalFreqSlider);

    signalFreqLabel.setText("Frequency:", juce::dontSendNotification);
    addChildComponent(signalFreqLabel);

    signalLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    signalLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    signalLevelSlider.setRange(-60.0, 0.0, 0.5);
    signalLevelSlider.setValue(-24.0); // Safe -24 dBFS default
    signalLevelSlider.setTextValueSuffix(" dBFS");
    signalLevelSlider.onValueChange = [this]() {
        signalGen.setLevelDb((float)signalLevelSlider.getValue());
    };
    addChildComponent(signalLevelSlider);

    signalLevelLabel.setText("Output Level:", juce::dontSendNotification);
    addChildComponent(signalLevelLabel);

    signalDestCombo.addItem("Stream Bus (Broadcast to OBS)", 1);
    signalDestCombo.addItem("Monitor Bus (Headphones)", 2);
    signalDestCombo.addItem("Both Buses (Stream + Monitor)", 3);
    signalDestCombo.addItem("Input 1 (Mic - Internal DSP Injection)", 4);
    signalDestCombo.addItem("Input 2 (Guitar - Internal DSP Injection)", 5);
    signalDestCombo.addItem("Input 3 (Music L - Internal DSP Injection)", 6);
    signalDestCombo.addItem("Input 4 (Music R - Internal DSP Injection)", 7);
    signalDestCombo.setSelectedId(1, juce::dontSendNotification);
    signalDestCombo.onChange = [this]() {
        int id = signalDestCombo.getSelectedId();
        if (id == 1) signalGen.setDestination(SignalDestination::StreamBus);
        else if (id == 2) signalGen.setDestination(SignalDestination::MonitorBus);
        else if (id == 3) signalGen.setDestination(SignalDestination::BothBuses);
        else if (id == 4) signalGen.setDestination(SignalDestination::InputChannel1);
        else if (id == 5) signalGen.setDestination(SignalDestination::InputChannel2);
        else if (id == 6) signalGen.setDestination(SignalDestination::InputChannel3);
        else if (id == 7) signalGen.setDestination(SignalDestination::InputChannel4);
    };
    addChildComponent(signalDestCombo);

    signalChannelCombo.addItem("Stereo (Both Channels)", 1);
    signalChannelCombo.addItem("Left Channel Only", 2);
    signalChannelCombo.addItem("Right Channel Only", 3);
    signalChannelCombo.setSelectedId(1, juce::dontSendNotification);
    signalChannelCombo.onChange = [this]() {
        signalGen.setChannel(static_cast<SignalChannel>(signalChannelCombo.getSelectedId() - 1));
    };
    addChildComponent(signalChannelCombo);

    signalContinuousToggle.setToggleState(true, juce::dontSendNotification);
    signalContinuousToggle.onClick = [this]() {
        signalGen.setContinuous(signalContinuousToggle.getToggleState());
    };
    addChildComponent(signalContinuousToggle);

    signalBurst1sBtn.onClick = [this]() {
        signalContinuousToggle.setToggleState(false, juce::dontSendNotification);
        signalGen.setContinuous(false);
        signalGen.triggerBurst(1.0);
    };
    addChildComponent(signalBurst1sBtn);

    signalBurst5sBtn.onClick = [this]() {
        signalContinuousToggle.setToggleState(false, juce::dontSendNotification);
        signalGen.setContinuous(false);
        signalGen.triggerBurst(5.0);
    };
    addChildComponent(signalBurst5sBtn);

    signalStartStopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    signalStartStopBtn.onClick = [this]() {
        bool running = signalGen.isEnabled();
        signalGen.setEnabled(!running);
        signalStartStopBtn.setButtonText(!running ? "STOP TEST SIGNAL" : "START TEST SIGNAL");
        signalStartStopBtn.setColour(juce::TextButton::buttonColourId, !running ? juce::Colour(0xffef4444) : juce::Colour(0xff2563eb));
    };
    addChildComponent(signalStartStopBtn);

    signalWarningLabel.setText("Safe startup default: -24 dBFS. 15ms anti-click ramp applied on start/stop.", juce::dontSendNotification);
    signalWarningLabel.setFont(juce::FontOptions(11.0f));
    signalWarningLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addChildComponent(signalWarningLabel);

    updateUIFromState();
    playlistListBox.updateContent();
    markerListBox.updateContent();

    startTimerHz(30); // 30 FPS UI Refresh
}

PlaybackComponent::~PlaybackComponent()
{
    stopTimer();
    audioPlayer.removeChangeListener(this);
}

void PlaybackComponent::setShowTestSignal(bool showSignal)
{
    showingTestSignal = showSignal;
    modeSwitchBtn.setButtonText(showSignal ? "[Switch to Audio Player]" : "[Test Signal Generator]");
    modeSwitchBtn.setColour(juce::TextButton::buttonColourId, showSignal ? juce::Colour(0xffea580c) : juce::Colour(0xff1e3a8a));

    testSignalGroup.setVisible(showSignal);
    signalTypeCombo.setVisible(showSignal);
    signalFreqSlider.setVisible(showSignal);
    signalFreqLabel.setVisible(showSignal);
    signalLevelSlider.setVisible(showSignal);
    signalLevelLabel.setVisible(showSignal);
    signalDestCombo.setVisible(showSignal);
    signalChannelCombo.setVisible(showSignal);
    signalContinuousToggle.setVisible(showSignal);
    signalBurst1sBtn.setVisible(showSignal);
    signalBurst5sBtn.setVisible(showSignal);
    signalStartStopBtn.setVisible(showSignal);
    signalWarningLabel.setVisible(showSignal);

    resized();
    repaint();
}

void PlaybackComponent::setSubTab(SubTab tab)
{
    activeSubTab = tab;
    fileListTabBtn.setColour(juce::TextButton::buttonColourId, (tab == SubTabFileList) ? juce::Colour(0xffe11d48) : juce::Colour(0xff1f2937));
    markerListTabBtn.setColour(juce::TextButton::buttonColourId, (tab == SubTabMarkerList) ? juce::Colour(0xffe11d48) : juce::Colour(0xff1f2937));
    if (tab == SubTabMarkerList && markerModel)
        markerListBox.updateContent();
    else if (tab == SubTabFileList && playlistModel)
        playlistListBox.updateContent();
    resized();
    repaint();
}

void PlaybackComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    updateUIFromState();
}

void PlaybackComponent::updateUIFromState()
{
    playPauseBtn.setButtonText(audioPlayer.isPlaying() ? "PAUSE" : "PLAY");
    playPauseBtn.setColour(juce::TextButton::buttonColourId, audioPlayer.isPlaying() ? juce::Colour(0xff0284c7) : juce::Colour(0xff2563eb));

    loopBtn.setToggleState(audioPlayer.isLoopEnabled(), juce::dontSendNotification);
    muteBtn.setToggleState(audioPlayer.isMuted(), juce::dontSendNotification);

    currentTrackNameLabel.setText(audioPlayer.getLoadedFileName().isNotEmpty() ? audioPlayer.getLoadedFileName() : "No File Loaded", juce::dontSendNotification);

    formatTimeText(audioPlayer.getCurrentPosition(), audioPlayer.getLengthInSeconds());
    waveformView.zoomFit();
    waveformView.repaint();
}

void PlaybackComponent::formatTimeText(double currentSecs, double totalSecs)
{
    int cTotalMs = (int)(currentSecs * 1000.0);
    int cMins = cTotalMs / 60000;
    int cSecs = (cTotalMs % 60000) / 1000;
    int cMs = cTotalMs % 1000;
    timeDisplayCurrent.setText(juce::String::formatted("%02d:%02d.%03d", cMins, cSecs, cMs), juce::dontSendNotification);

    int tTotalMs = (int)(totalSecs * 1000.0);
    int tMins = tTotalMs / 60000;
    int tSecs = (tTotalMs % 60000) / 1000;
    int tMs = tTotalMs % 1000;
    timeDisplayTotal.setText(juce::String::formatted("%02d:%02d.%03d", tMins, tSecs, tMs), juce::dontSendNotification);
}

void PlaybackComponent::timerCallback()
{
    // Update playhead and digital timestamp
    if (audioPlayer.isPlaying() || audioPlayer.isPaused())
    {
        formatTimeText(audioPlayer.getCurrentPosition(), audioPlayer.getLengthInSeconds());
        waveformView.repaint();
    }

    // Read real-time meter levels
    meterL = audioPlayer.getMeterPeakL();
    meterR = audioPlayer.getMeterPeakR();
    testMeterL = signalGen.getPeakLevelL();
    testMeterR = signalGen.getPeakLevelR();

    repaint();
}

void PlaybackComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff14181f)); // Dark Console Background

    // Draw stereo level meter ladder in Lower Right
    if (!showingTestSignal)
    {
        int meterX = getWidth() - 250;
        int meterY = getHeight() - 55;
        int meterW = 230;
        int meterH = 14;

        // Background groove
        g.setColour(juce::Colour(0xff0d1117));
        g.fillRoundedRectangle((float)meterX, (float)meterY, (float)meterW, (float)(meterH * 2 + 4), 3.0f);

        // dB Scale Labels: Sig, -72, -60, -48, -36, -24, -12, 0, +6
        g.setFont(juce::FontOptions(9.0f));
        g.setColour(juce::Colours::grey);
        g.drawText("Sig", meterX - 24, meterY, 20, meterH, juce::Justification::centredRight);
        g.drawText("-48", meterX + 40, meterY - 12, 24, 10, juce::Justification::centred);
        g.drawText("-24", meterX + 110, meterY - 12, 24, 10, juce::Justification::centred);
        g.drawText("-12", meterX + 150, meterY - 12, 24, 10, juce::Justification::centred);
        g.drawText("0 dB", meterX + 190, meterY - 12, 26, 10, juce::Justification::centred);

        // Left Channel Bar
        float fillL = juce::jlimit(0.0f, 1.0f, meterL);
        juce::Colour colourL = (fillL >= 0.95f) ? juce::Colour(0xffef4444) : ((fillL >= 0.7f) ? juce::Colour(0xfff59e0b) : juce::Colour(0xff10b981));
        g.setColour(colourL);
        g.fillRoundedRectangle((float)(meterX + 2), (float)(meterY + 2), fillL * (float)(meterW - 4), (float)(meterH - 2), 2.0f);

        // Right Channel Bar
        float fillR = juce::jlimit(0.0f, 1.0f, meterR);
        juce::Colour colourR = (fillR >= 0.95f) ? juce::Colour(0xffef4444) : ((fillR >= 0.7f) ? juce::Colour(0xfff59e0b) : juce::Colour(0xff10b981));
        g.setColour(colourR);
        g.fillRoundedRectangle((float)(meterX + 2), (float)(meterY + meterH + 2), fillR * (float)(meterW - 4), (float)(meterH - 2), 2.0f);
    }
}

void PlaybackComponent::resized()
{
    int w = getWidth();
    int h = getHeight();

    // 1. Header Bar (32px)
    headerTitleLabel.setBounds(12, 6, 260, 24);
    optionsMenuBtn.setBounds(w - 40, 6, 30, 24);
    bypassBtn.setBounds(w - 110, 6, 65, 24);
    modeSwitchBtn.setBounds(w - 300, 6, 180, 24);

    if (showingTestSignal)
    {
        waveformView.setVisible(false);
        fileListTabBtn.setVisible(false);
        markerListTabBtn.setVisible(false);
        addFilesBtn.setVisible(false);
        shuffleBtn.setVisible(false);
        clearPlaylistBtn.setVisible(false);
        savePlaylistBtn.setVisible(false);
        loadPlaylistBtn.setVisible(false);
        autoNextToggle.setVisible(false);
        playlistListBox.setVisible(false);
        markerListBox.setVisible(false);
        timeDisplayCurrent.setVisible(false);
        timeDisplayTotal.setVisible(false);
        zoomInBtn.setVisible(false);
        zoomOutBtn.setVisible(false);
        zoomFitBtn.setVisible(false);
        addMarkerBtn.setVisible(false);
        currentTrackNameLabel.setVisible(false);
        prevTrackBtn.setVisible(false);
        playPauseBtn.setVisible(false);
        nextTrackBtn.setVisible(false);
        rewindBtn.setVisible(false);
        fastFwdBtn.setVisible(false);
        loopBtn.setVisible(false);
        muteBtn.setVisible(false);
        jumpLoopStartBtn.setVisible(false);
        stopBtn.setVisible(false);
        jumpLoopEndBtn.setVisible(false);
        routeStreamToggle.setVisible(false);
        routeMonitorToggle.setVisible(false);
        speedKnob.setVisible(false);
        speedLabel.setVisible(false);
        pitchKnob.setVisible(false);
        pitchLabel.setVisible(false);
        pitchLockToggle.setVisible(false);
        masterGainSlider.setVisible(false);
        masterGainLabel.setVisible(false);

        // Show Test Signal Generator Panel
        int pX = (w - 650) / 2;
        int pY = 46;
        int pW = 650;
        int pH = h - 60;
        testSignalGroup.setBounds(pX, pY, pW, pH);

        int cY = pY + 36;
        signalTypeCombo.setBounds(pX + 24, cY, pW - 48, 28);
        cY += 38;

        signalFreqLabel.setBounds(pX + 24, cY, 110, 24);
        signalFreqSlider.setBounds(pX + 140, cY, pW - 164, 24);
        cY += 34;

        signalLevelLabel.setBounds(pX + 24, cY, 110, 24);
        signalLevelSlider.setBounds(pX + 140, cY, pW - 164, 24);
        cY += 34;

        signalDestCombo.setBounds(pX + 24, cY, pW - 48, 28);
        cY += 34;

        signalChannelCombo.setBounds(pX + 24, cY, pW - 48, 28);
        cY += 34;

        signalContinuousToggle.setBounds(pX + 24, cY, 160, 24);
        signalBurst1sBtn.setBounds(pX + 200, cY, 90, 24);
        signalBurst5sBtn.setBounds(pX + 300, cY, 90, 24);
        cY += 40;

        signalStartStopBtn.setBounds(pX + 24, cY, pW - 48, 38);
        cY += 44;

        signalWarningLabel.setBounds(pX + 24, cY, pW - 48, 20);
        return;
    }

    // Restore File Player Components
    waveformView.setVisible(true);
    fileListTabBtn.setVisible(true);
    markerListTabBtn.setVisible(true);
    timeDisplayCurrent.setVisible(true);
    timeDisplayTotal.setVisible(true);
    zoomInBtn.setVisible(true);
    zoomOutBtn.setVisible(true);
    zoomFitBtn.setVisible(true);
    addMarkerBtn.setVisible(true);
    currentTrackNameLabel.setVisible(true);
    prevTrackBtn.setVisible(true);
    playPauseBtn.setVisible(true);
    nextTrackBtn.setVisible(true);
    rewindBtn.setVisible(true);
    fastFwdBtn.setVisible(true);
    loopBtn.setVisible(true);
    muteBtn.setVisible(true);
    jumpLoopStartBtn.setVisible(true);
    stopBtn.setVisible(true);
    jumpLoopEndBtn.setVisible(true);
    routeStreamToggle.setVisible(true);
    routeMonitorToggle.setVisible(true);
    speedKnob.setVisible(true);
    speedLabel.setVisible(true);
    pitchKnob.setVisible(true);
    pitchLabel.setVisible(true);
    pitchLockToggle.setVisible(true);
    masterGainSlider.setVisible(true);
    masterGainLabel.setVisible(true);

    // 2. Waveform Viewport (Top ~40% of height)
    int waveH = juce::jlimit(160, 280, (int)(h * 0.42f));
    waveformView.setBounds(10, 36, w - 20, waveH);

    // 3. Waveform Controls & Time Display Bar (30px)
    int barY = 36 + waveH + 4;
    timeDisplayCurrent.setBounds(12, barY, 130, 26);
    timeDisplayTotal.setBounds(145, barY + 4, 100, 20);
    zoomInBtn.setBounds(250, barY + 2, 28, 24);
    zoomOutBtn.setBounds(282, barY + 2, 28, 24);
    zoomFitBtn.setBounds(314, barY + 2, 36, 24);
    addMarkerBtn.setBounds(356, barY + 2, 60, 24);
    currentTrackNameLabel.setBounds(426, barY + 2, w - 440, 24);

    // 4. Lower Area Division: Left = Playlist/Markers (50%), Right = Transport & Controls (50%)
    int lowerY = barY + 34;
    int lowerH = h - lowerY - 8;
    int splitW = (w - 24) / 2;

    // Lower Left: SubTabs + Table
    fileListTabBtn.setBounds(12, lowerY, 140, 26);
    markerListTabBtn.setBounds(154, lowerY, 140, 26);

    int tbY = lowerY + 30;
    if (activeSubTab == SubTabFileList)
    {
        addFilesBtn.setVisible(true);
        shuffleBtn.setVisible(true);
        clearPlaylistBtn.setVisible(true);
        savePlaylistBtn.setVisible(true);
        loadPlaylistBtn.setVisible(true);
        autoNextToggle.setVisible(true);
        playlistListBox.setVisible(true);
        markerListBox.setVisible(false);

        addFilesBtn.setBounds(12, tbY, 90, 24);
        shuffleBtn.setBounds(106, tbY, 60, 24);
        clearPlaylistBtn.setBounds(170, tbY, 50, 24);
        savePlaylistBtn.setBounds(224, tbY, 65, 24);
        loadPlaylistBtn.setBounds(293, tbY, 65, 24);
        autoNextToggle.setBounds(362, tbY, 80, 24);

        playlistListBox.setBounds(12, tbY + 28, splitW, lowerH - 60);
    }
    else
    {
        addFilesBtn.setVisible(false);
        shuffleBtn.setVisible(false);
        clearPlaylistBtn.setVisible(false);
        savePlaylistBtn.setVisible(false);
        loadPlaylistBtn.setVisible(false);
        autoNextToggle.setVisible(false);
        playlistListBox.setVisible(false);
        markerListBox.setVisible(true);

        markerListBox.setBounds(12, tbY, splitW, lowerH - 32);
    }

    // Lower Right: Transport Grid, Speed/Pitch, Level Meter
    int rightX = 16 + splitW;
    int tY = lowerY + 4;

    // Row 1: |<< , PLAY/PAUSE , >>|
    prevTrackBtn.setBounds(rightX, tY, 55, 34);
    playPauseBtn.setBounds(rightX + 60, tY, 110, 34);
    nextTrackBtn.setBounds(rightX + 175, tY, 55, 34);

    // Row 2: << , >> , Loop , Mute
    rewindBtn.setBounds(rightX, tY + 40, 55, 28);
    fastFwdBtn.setBounds(rightX + 60, tY + 40, 55, 28);
    loopBtn.setBounds(rightX + 120, tY + 40, 60, 28);
    muteBtn.setBounds(rightX + 185, tY + 40, 55, 28);

    // Row 3: |< , STOP , >|
    jumpLoopStartBtn.setBounds(rightX, tY + 74, 55, 30);
    stopBtn.setBounds(rightX + 60, tY + 74, 110, 30);
    jumpLoopEndBtn.setBounds(rightX + 175, tY + 74, 55, 30);

    // Routing Toggles
    routeStreamToggle.setBounds(rightX, tY + 110, 115, 22);
    routeMonitorToggle.setBounds(rightX + 120, tY + 110, 115, 22);

    // Speed & Pitch Knobs
    int knobX = rightX + 250;
    speedKnob.setBounds(knobX, tY, 68, 80);
    speedLabel.setBounds(knobX, tY + 80, 68, 16);

    pitchKnob.setBounds(knobX + 80, tY, 68, 80);
    pitchLabel.setBounds(knobX + 80, tY + 80, 68, 16);
    pitchLockToggle.setBounds(knobX + 80, tY + 98, 68, 18);

    // Master Output Level Slider
    masterGainLabel.setBounds(rightX, h - 84, 80, 20);
    masterGainSlider.setBounds(rightX + 85, h - 84, w - rightX - 95, 24);
}

} // namespace LiveBus
