#pragma once
#include <JuceHeader.h>
#include "AudioPlaybackModel.h"
#include "WaveformThumbnailCache.h"
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>

namespace LiveBus
{

class AudioFilePlayer final : public juce::ChangeBroadcaster
{
public:
    AudioFilePlayer();
    ~AudioFilePlayer();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    // File loading and reading
    bool loadFile(const juce::File& file);
    juce::String getLoadedFileName() const;
    juce::File getCurrentFile() const { return currentFile; }
    double getLengthInSeconds() const;
    double getCurrentPosition() const;
    void setPosition(double posSeconds);
    double getSampleRate() const { return fileSampleRate; }
    int getNumChannels() const { return fileChannels; }

    // Transport controls
    void start();
    void stop();
    void pause();
    bool isPlaying() const;
    bool isPaused() const;

    // Gain & Mute
    void setGain(float gain);
    float getGain() const { return playbackGain.load(std::memory_order_relaxed); }
    void setMute(bool mute);
    bool isMuted() const { return playbackMuted.load(std::memory_order_relaxed); }

    // Routing
    void setRouteStream(bool enable) { routeToStream.store(enable, std::memory_order_relaxed); }
    bool isRouteStream() const { return routeToStream.load(std::memory_order_relaxed); }
    void setRouteMonitor(bool enable) { routeToMonitor.store(enable, std::memory_order_relaxed); }
    bool isRouteMonitor() const { return routeToMonitor.load(std::memory_order_relaxed); }

    // Looping
    void setLoopEnabled(bool enable);
    bool isLoopEnabled() const { return loopActive.load(std::memory_order_relaxed); }
    void setLoopRange(double startSeconds, double endSeconds);
    double getLoopStart() const { return loopStart.load(std::memory_order_relaxed); }
    double getLoopEnd() const { return loopEnd.load(std::memory_order_relaxed); }

    // Speed (Resampling Ratio 0.5x to 2.0x)
    void setSpeedRatio(float ratio);
    float getSpeedRatio() const { return speedRatio.load(std::memory_order_relaxed); }

    // Marker Management
    void addMarker(double posSeconds, const juce::String& name = "");
    void removeMarker(int markerId);
    void renameMarker(int markerId, const juce::String& newName);
    void clearMarkers();
    std::vector<AudioMarker> getMarkers() const;

    // Playlist Management
    bool addTrackToPlaylist(const juce::File& file);
    void removeTrackFromPlaylist(int index);
    void clearPlaylist();
    std::vector<PlaylistItem> getPlaylist() const;
    int getCurrentTrackIndex() const { return currentPlaylistIndex; }
    bool selectTrack(int index);
    bool nextTrack();
    bool prevTrack();
    void setAutoNextTrack(bool autoNext)
    {
        autoNextTrackActive.store(autoNext, std::memory_order_relaxed);
        if (!autoNext)
            autoNextPending.store(false, std::memory_order_relaxed);
    }
    bool isAutoNextTrack() const { return autoNextTrackActive.load(std::memory_order_relaxed); }
    bool consumeAutoNextRequest() noexcept { return autoNextPending.exchange(false, std::memory_order_relaxed); }
    bool savePlaylistXml(const juce::File& destFile);
    bool loadPlaylistXml(const juce::File& srcFile);

    // Waveform Cache
    const WaveformThumbnailCache& getWaveformCache() const { return waveformCache; }

    // Metering
    float getMeterPeakL() const { return meterPeakL.load(std::memory_order_relaxed); }
    float getMeterPeakR() const { return meterPeakR.load(std::memory_order_relaxed); }
    bool isClipping() const { return clipDetected.load(std::memory_order_relaxed); }
    void resetClip() { clipDetected.store(false, std::memory_order_relaxed); }

    // Audio thread rendering: zero heap allocation, real-time safe
    void renderBlock(juce::AudioBuffer<float>& destBuffer, int numSamples);
    void processMix(juce::AudioBuffer<float>& streamBuf,
                    juce::AudioBuffer<float>& monitorBuf,
                    int numSamples);

    // Serialization for Scenes
    PlaybackState exportState() const;
    void importState(const PlaybackState& state);

private:
    void requestAutoNext() noexcept { autoNextPending.store(true, std::memory_order_relaxed); }

    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread thread{"BGM Player Thread"};
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;

    WaveformThumbnailCache waveformCache;

    juce::AudioBuffer<float> tempPlaybackBuffer;
    juce::AudioBuffer<float> resampleBuffer;

    std::atomic<float> playbackGain{0.8f};
    std::atomic<bool> playbackMuted{false};
    std::atomic<bool> routeToStream{true};
    std::atomic<bool> routeToMonitor{true};
    std::atomic<bool> hasFileLoaded{false};

    std::atomic<bool> loopActive{false};
    std::atomic<double> loopStart{0.0};
    std::atomic<double> loopEnd{10.0};
    std::atomic<float> speedRatio{1.0f};
    std::atomic<bool> autoNextTrackActive{true};
    std::atomic<bool> autoNextPending{false};

    // Anti-click fade on play/pause/loop
    float currentGainRamp{0.0f};

    // Meters
    std::atomic<float> meterPeakL{0.0f};
    std::atomic<float> meterPeakR{0.0f};
    std::atomic<bool> clipDetected{false};

    mutable std::mutex dataMutex;
    std::vector<AudioMarker> markers;
    std::vector<PlaylistItem> playlist;
    int currentPlaylistIndex{-1};
    int nextMarkerId{1};
    int nextTrackId{1};

    juce::File currentFile;
    double currentDeviceSampleRate{48000.0};
    int currentBlockSize{512};
    double fileSampleRate{44100.0};
    int fileChannels{2};
    std::atomic<double> fileDuration{0.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilePlayer)
};

} // namespace LiveBus

using LiveBus::AudioFilePlayer;
using LiveBus::AudioMarker;
using LiveBus::PlaylistItem;
using LiveBus::PlaybackState;
