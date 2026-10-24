#include "AudioFilePlayer.h"

namespace LiveBus
{

AudioFilePlayer::AudioFilePlayer()
{
    formatManager.registerBasicFormats();
    thread.startThread(juce::Thread::Priority::normal);
}

AudioFilePlayer::~AudioFilePlayer()
{
    transportSource.setSource(nullptr);
    resamplingSource.reset();
    readerSource.reset();
    thread.stopThread(2000);
}

void AudioFilePlayer::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentDeviceSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    currentBlockSize = juce::jmax(1, samplesPerBlock);

    tempPlaybackBuffer.setSize(2, currentBlockSize);
    tempPlaybackBuffer.clear();

    transportSource.prepareToPlay(currentBlockSize, currentDeviceSampleRate);
    if (resamplingSource != nullptr)
        resamplingSource->prepareToPlay(currentBlockSize, currentDeviceSampleRate);
}

void AudioFilePlayer::releaseResources()
{
    transportSource.releaseResources();
    if (resamplingSource != nullptr)
        resamplingSource->releaseResources();
}

bool AudioFilePlayer::loadFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
        return false;

    transportSource.stop();
    transportSource.setSource(nullptr);
    resamplingSource.reset();
    readerSource.reset();

    fileSampleRate = reader->sampleRate;
    fileChannels = (int)reader->numChannels;
    fileDuration = (double)reader->lengthInSamples / fileSampleRate;

    // Generate waveform peak cache
    waveformCache.generatePeaks(*reader);

    // Buffer ahead using TimeSliceThread (65536 samples lookahead cache)
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transportSource.setSource(readerSource.get(), 65536, &thread, reader->sampleRate);
    transportSource.setGain(playbackGain.load(std::memory_order_relaxed));

    // Build Resampling source for speed control wrapping transportSource
    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(&transportSource, false, 2);
    resamplingSource->prepareToPlay(currentBlockSize, currentDeviceSampleRate);
    float speed = speedRatio.load(std::memory_order_relaxed);
    resamplingSource->setResamplingRatio((double)juce::jmax(0.1f, speed));

    currentFile = file;
    loopStart.store(0.0, std::memory_order_relaxed);
    loopEnd.store(fileDuration, std::memory_order_relaxed);
    hasFileLoaded.store(true, std::memory_order_relaxed);

    sendChangeMessage();
    return true;
}

juce::String AudioFilePlayer::getLoadedFileName() const
{
    return currentFile.getFileName();
}

double AudioFilePlayer::getLengthInSeconds() const
{
    return fileDuration > 0.0 ? fileDuration : transportSource.getLengthInSeconds();
}

double AudioFilePlayer::getCurrentPosition() const
{
    return transportSource.getCurrentPosition();
}

void AudioFilePlayer::setPosition(double posSeconds)
{
    double len = getLengthInSeconds();
    double clamped = juce::jlimit(0.0, juce::jmax(0.0, len), posSeconds);
    transportSource.setPosition(clamped);
}

void AudioFilePlayer::start()
{
    if (hasFileLoaded.load(std::memory_order_relaxed))
    {
        transportSource.start();
        sendChangeMessage();
    }
}

void AudioFilePlayer::stop()
{
    transportSource.stop();
    transportSource.setPosition(0.0);
    sendChangeMessage();
}

void AudioFilePlayer::pause()
{
    transportSource.stop();
    sendChangeMessage();
}

bool AudioFilePlayer::isPlaying() const
{
    return transportSource.isPlaying();
}

bool AudioFilePlayer::isPaused() const
{
    return !transportSource.isPlaying() && transportSource.getCurrentPosition() > 0.0;
}

void AudioFilePlayer::setGain(float gain)
{
    float clamped = juce::jlimit(0.0f, 2.0f, gain);
    playbackGain.store(clamped, std::memory_order_relaxed);
    transportSource.setGain(clamped);
}

void AudioFilePlayer::setMute(bool mute)
{
    playbackMuted.store(mute, std::memory_order_relaxed);
}

void AudioFilePlayer::setLoopEnabled(bool enable)
{
    loopActive.store(enable, std::memory_order_relaxed);
}

void AudioFilePlayer::setLoopRange(double startSeconds, double endSeconds)
{
    double len = getLengthInSeconds();
    if (len <= 0.0) len = 1000.0;

    double s = juce::jlimit(0.0, len, startSeconds);
    double e = juce::jlimit(s + 0.05, len, endSeconds);

    loopStart.store(s, std::memory_order_relaxed);
    loopEnd.store(e, std::memory_order_relaxed);
}

void AudioFilePlayer::setSpeedRatio(float ratio)
{
    float clamped = juce::jlimit(0.25f, 4.0f, ratio);
    speedRatio.store(clamped, std::memory_order_relaxed);
    if (resamplingSource != nullptr)
    {
        resamplingSource->setResamplingRatio(1.0 / (double)clamped);
    }
}

// Marker Management
void AudioFilePlayer::addMarker(double posSeconds, const juce::String& name)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    AudioMarker m;
    m.id = nextMarkerId++;
    m.positionSeconds = posSeconds;
    m.name = name.isNotEmpty() ? name : ("Marker " + juce::String(m.id));
    m.color = juce::Colours::orange;
    markers.push_back(m);
    sendChangeMessage();
}

void AudioFilePlayer::removeMarker(int markerId)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    for (auto it = markers.begin(); it != markers.end(); ++it)
    {
        if (it->id == markerId)
        {
            markers.erase(it);
            break;
        }
    }
    sendChangeMessage();
}

void AudioFilePlayer::renameMarker(int markerId, const juce::String& newName)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    for (auto& m : markers)
    {
        if (m.id == markerId)
        {
            m.name = newName;
            break;
        }
    }
    sendChangeMessage();
}

void AudioFilePlayer::clearMarkers()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    markers.clear();
    sendChangeMessage();
}

std::vector<AudioMarker> AudioFilePlayer::getMarkers() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return markers;
}

// Playlist Management
bool AudioFilePlayer::addTrackToPlaylist(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> r(formatManager.createReaderFor(file));
    if (r == nullptr)
        return false;

    PlaylistItem item;
    item.id = nextTrackId++;
    item.file = file;
    item.title = file.getFileNameWithoutExtension();
    item.durationSeconds = (double)r->lengthInSamples / r->sampleRate;
    item.sampleRate = r->sampleRate;
    item.numChannels = (int)r->numChannels;
    item.gainDb = 0.0f;
    item.isAvailable = true;
    item.bpm = 120;

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        playlist.push_back(item);
        if (currentPlaylistIndex < 0)
        {
            currentPlaylistIndex = 0;
            loadFile(file);
        }
    }

    sendChangeMessage();
    return true;
}

void AudioFilePlayer::removeTrackFromPlaylist(int index)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if (index >= 0 && index < (int)playlist.size())
    {
        playlist.erase(playlist.begin() + index);
        if (currentPlaylistIndex >= (int)playlist.size())
            currentPlaylistIndex = (int)playlist.size() - 1;
    }
    sendChangeMessage();
}

void AudioFilePlayer::clearPlaylist()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    playlist.clear();
    currentPlaylistIndex = -1;
    stop();
    sendChangeMessage();
}

std::vector<PlaylistItem> AudioFilePlayer::getPlaylist() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return playlist;
}

bool AudioFilePlayer::selectTrack(int index)
{
    juce::File f;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (index < 0 || index >= (int)playlist.size())
            return false;

        currentPlaylistIndex = index;
        f = playlist[index].file;
    }

    if (f.existsAsFile())
    {
        return loadFile(f);
    }
    return false;
}

bool AudioFilePlayer::nextTrack()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist.empty()) return false;

    int nextIdx = (currentPlaylistIndex + 1) % (int)playlist.size();
    currentPlaylistIndex = nextIdx;
    juce::File f = playlist[nextIdx].file;
    if (f.existsAsFile())
    {
        loadFile(f);
        start();
        return true;
    }
    return false;
}

bool AudioFilePlayer::prevTrack()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if (playlist.empty()) return false;

    int prevIdx = currentPlaylistIndex - 1;
    if (prevIdx < 0) prevIdx = (int)playlist.size() - 1;
    currentPlaylistIndex = prevIdx;
    juce::File f = playlist[prevIdx].file;
    if (f.existsAsFile())
    {
        loadFile(f);
        start();
        return true;
    }
    return false;
}

bool AudioFilePlayer::savePlaylistXml(const juce::File& destFile)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    juce::XmlElement root("LiveBusPlaylist");
    for (const auto& item : playlist)
        root.addChildElement(item.toXml().release());

    return root.writeTo(destFile);
}

bool AudioFilePlayer::loadPlaylistXml(const juce::File& srcFile)
{
    if (!srcFile.existsAsFile()) return false;
    auto xml = juce::parseXML(srcFile);
    if (xml == nullptr || !xml->hasTagName("LiveBusPlaylist")) return false;

    std::vector<PlaylistItem> loaded;
    for (auto* c = xml->getFirstChildElement(); c != nullptr; c = c->getNextElement())
    {
        if (c->hasTagName("Track"))
            loaded.push_back(PlaylistItem::fromXml(*c));
    }

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        playlist = std::move(loaded);
        currentPlaylistIndex = playlist.empty() ? -1 : 0;
    }

    if (!playlist.empty() && playlist[0].file.existsAsFile())
        loadFile(playlist[0].file);

    sendChangeMessage();
    return true;
}

void AudioFilePlayer::triggerAutoNextAsync()
{
    juce::MessageManager::callAsync([this]() {
        nextTrack();
    });
}

void AudioFilePlayer::renderBlock(juce::AudioBuffer<float>& destBuffer, int numSamples)
{
    if (!transportSource.isPlaying() || numSamples <= 0)
    {
        destBuffer.clear(0, numSamples);
        meterPeakL.store(0.0f, std::memory_order_relaxed);
        meterPeakR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (tempPlaybackBuffer.getNumSamples() < numSamples)
    {
        destBuffer.clear(0, numSamples);
        return;
    }

    tempPlaybackBuffer.clear(0, numSamples);

    juce::AudioSourceChannelInfo info(&tempPlaybackBuffer, 0, numSamples);
    if (resamplingSource != nullptr)
        resamplingSource->getNextAudioBlock(info);
    else
        transportSource.getNextAudioBlock(info);

    // Loop Region Boundary Check
    if (loopActive.load(std::memory_order_relaxed))
    {
        double curPos = transportSource.getCurrentPosition();
        double lEnd = loopEnd.load(std::memory_order_relaxed);
        double lStart = loopStart.load(std::memory_order_relaxed);

        if (curPos >= lEnd)
            transportSource.setPosition(lStart);
    }
    else
    {
        // Auto Next Track check
        double curPos = transportSource.getCurrentPosition();
        double totalLen = getLengthInSeconds();
        if (totalLen > 0.0 && curPos >= totalLen - 0.05)
        {
            if (autoNextTrackActive.load(std::memory_order_relaxed))
                triggerAutoNextAsync();
        }
    }

    float gain = playbackMuted.load(std::memory_order_relaxed) ? 0.0f : playbackGain.load(std::memory_order_relaxed);
    float blockPeakL = 0.0f;
    float blockPeakR = 0.0f;

    auto* readL = tempPlaybackBuffer.getReadPointer(0);
    auto* readR = (tempPlaybackBuffer.getNumChannels() > 1) ? tempPlaybackBuffer.getReadPointer(1) : readL;

    for (int s = 0; s < numSamples; ++s)
    {
        float sL = std::abs(readL[s] * gain);
        float sR = std::abs(readR[s] * gain);
        if (sL > blockPeakL) blockPeakL = sL;
        if (sR > blockPeakR) blockPeakR = sR;
    }

    meterPeakL.store(blockPeakL, std::memory_order_relaxed);
    meterPeakR.store(blockPeakR, std::memory_order_relaxed);
    if (blockPeakL >= 0.999f || blockPeakR >= 0.999f)
        clipDetected.store(true, std::memory_order_relaxed);

    for (int ch = 0; ch < juce::jmin(2, destBuffer.getNumChannels()); ++ch)
    {
        destBuffer.copyFrom(ch, 0, tempPlaybackBuffer, ch, 0, numSamples);
        destBuffer.applyGain(ch, 0, numSamples, gain);
    }
}

void AudioFilePlayer::processMix(juce::AudioBuffer<float>& streamBuf,
                                 juce::AudioBuffer<float>& monitorBuf,
                                 int numSamples)
{
    if (!transportSource.isPlaying() || numSamples <= 0)
    {
        meterPeakL.store(0.0f, std::memory_order_relaxed);
        meterPeakR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (tempPlaybackBuffer.getNumSamples() < numSamples)
        return;

    tempPlaybackBuffer.clear(0, numSamples);

    juce::AudioSourceChannelInfo info(&tempPlaybackBuffer, 0, numSamples);
    if (resamplingSource != nullptr)
        resamplingSource->getNextAudioBlock(info);
    else
        transportSource.getNextAudioBlock(info);

    // Loop Region Boundary Check
    if (loopActive.load(std::memory_order_relaxed))
    {
        double curPos = transportSource.getCurrentPosition();
        double lEnd = loopEnd.load(std::memory_order_relaxed);
        double lStart = loopStart.load(std::memory_order_relaxed);

        if (curPos >= lEnd)
        {
            transportSource.setPosition(lStart);
        }
    }
    else
    {
        // Auto Next Track check
        double curPos = transportSource.getCurrentPosition();
        double totalLen = getLengthInSeconds();
        if (totalLen > 0.0 && curPos >= totalLen - 0.05)
        {
            if (autoNextTrackActive.load(std::memory_order_relaxed))
            {
                triggerAutoNextAsync();
            }
        }
    }

    float gain = playbackMuted.load(std::memory_order_relaxed) ? 0.0f : playbackGain.load(std::memory_order_relaxed);
    bool toStream = routeToStream.load(std::memory_order_relaxed);
    bool toMonitor = routeToMonitor.load(std::memory_order_relaxed);

    float blockPeakL = 0.0f;
    float blockPeakR = 0.0f;

    auto* readL = tempPlaybackBuffer.getReadPointer(0);
    auto* readR = (tempPlaybackBuffer.getNumChannels() > 1) ? tempPlaybackBuffer.getReadPointer(1) : readL;

    for (int s = 0; s < numSamples; ++s)
    {
        float sL = std::abs(readL[s] * gain);
        float sR = std::abs(readR[s] * gain);
        if (sL > blockPeakL) blockPeakL = sL;
        if (sR > blockPeakR) blockPeakR = sR;
    }

    meterPeakL.store(blockPeakL, std::memory_order_relaxed);
    meterPeakR.store(blockPeakR, std::memory_order_relaxed);
    if (blockPeakL >= 0.999f || blockPeakR >= 0.999f)
        clipDetected.store(true, std::memory_order_relaxed);

    if (toStream)
    {
        for (int ch = 0; ch < juce::jmin(2, streamBuf.getNumChannels()); ++ch)
        {
            streamBuf.addFrom(ch, 0, tempPlaybackBuffer, ch, 0, numSamples, gain);
        }
    }

    if (toMonitor)
    {
        for (int ch = 0; ch < juce::jmin(2, monitorBuf.getNumChannels()); ++ch)
        {
            monitorBuf.addFrom(ch, 0, tempPlaybackBuffer, ch, 0, numSamples, gain);
        }
    }
}

PlaybackState AudioFilePlayer::exportState() const
{
    PlaybackState s;
    s.loopEnabled = loopActive.load();
    s.loopStartSeconds = loopStart.load();
    s.loopEndSeconds = loopEnd.load();
    s.speedRatio = speedRatio.load();
    s.pitchSemitones = (speedRatio.load() - 1.0f) * 12.0f; // Linked varispeed semitone shift
    s.autoNextTrack = autoNextTrackActive.load();
    s.masterGain = playbackGain.load();
    s.routeStream = routeToStream.load();
    s.routeMonitor = routeToMonitor.load();
    s.currentTrackIndex = currentPlaylistIndex;
    s.markers = getMarkers();
    s.playlist = getPlaylist();
    return s;
}

void AudioFilePlayer::importState(const PlaybackState& state)
{
    loopActive.store(state.loopEnabled);
    loopStart.store(state.loopStartSeconds);
    loopEnd.store(state.loopEndSeconds);
    setSpeedRatio(state.speedRatio);
    autoNextTrackActive.store(state.autoNextTrack);
    setGain(state.masterGain);
    routeToStream.store(state.routeStream);
    routeToMonitor.store(state.routeMonitor);

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        markers = state.markers;
        playlist = state.playlist;
        currentPlaylistIndex = state.currentTrackIndex;
    }

    if (currentPlaylistIndex >= 0 && currentPlaylistIndex < (int)playlist.size())
    {
        if (playlist[currentPlaylistIndex].file.existsAsFile())
            loadFile(playlist[currentPlaylistIndex].file);
    }

    sendChangeMessage();
}

} // namespace LiveBus
