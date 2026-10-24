#pragma once
#include <JuceHeader.h>
#include <vector>

namespace LiveBus
{

/**
 * Represents a timestamp marker on the playback timeline.
 */
struct AudioMarker
{
    int id{0};
    double positionSeconds{0.0};
    juce::String name{"Marker"};
    juce::Colour color{juce::Colours::orange};

    juce::String getFormattedTime() const
    {
        int totalMs = (int)(positionSeconds * 1000.0);
        int mins = totalMs / 60000;
        int secs = (totalMs % 60000) / 1000;
        int ms = totalMs % 1000;
        return juce::String::formatted("%02d:%02d.%03d", mins, secs, ms);
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("Marker");
        xml->setAttribute("id", id);
        xml->setAttribute("pos", positionSeconds);
        xml->setAttribute("name", name);
        xml->setAttribute("color", color.toDisplayString(true));
        return xml;
    }

    static AudioMarker fromXml(const juce::XmlElement& xml)
    {
        AudioMarker m;
        m.id = xml.getIntAttribute("id", 0);
        m.positionSeconds = xml.getDoubleAttribute("pos", 0.0);
        m.name = xml.getStringAttribute("name", "Marker");
        m.color = juce::Colour::fromString(xml.getStringAttribute("color", "ffffa500"));
        return m;
    }
};

/**
 * Represents a track in the playback playlist.
 */
struct PlaylistItem
{
    int id{0};
    juce::File file;
    juce::String title{"Unknown Track"};
    double durationSeconds{0.0};
    double sampleRate{44100.0};
    int numChannels{2};
    float gainDb{0.0f};
    bool isAvailable{true};
    int bpm{120};

    juce::String getFormattedDuration() const
    {
        int totalSecs = (int)durationSeconds;
        int mins = totalSecs / 60;
        int secs = totalSecs % 60;
        return juce::String::formatted("%02d:%02d", mins, secs);
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("Track");
        xml->setAttribute("id", id);
        xml->setAttribute("path", file.getFullPathName());
        xml->setAttribute("title", title);
        xml->setAttribute("duration", durationSeconds);
        xml->setAttribute("sr", sampleRate);
        xml->setAttribute("channels", numChannels);
        xml->setAttribute("gainDb", (double)gainDb);
        xml->setAttribute("bpm", bpm);
        return xml;
    }

    static PlaylistItem fromXml(const juce::XmlElement& xml)
    {
        PlaylistItem item;
        item.id = xml.getIntAttribute("id", 0);
        item.file = juce::File(xml.getStringAttribute("path"));
        item.title = xml.getStringAttribute("title", item.file.getFileNameWithoutExtension());
        item.durationSeconds = xml.getDoubleAttribute("duration", 0.0);
        item.sampleRate = xml.getDoubleAttribute("sr", 44100.0);
        item.numChannels = xml.getIntAttribute("channels", 2);
        item.gainDb = (float)xml.getDoubleAttribute("gainDb", 0.0);
        item.bpm = xml.getIntAttribute("bpm", 120);
        item.isAvailable = item.file.existsAsFile();
        return item;
    }
};

/**
 * Configuration and persistent state for Audio Player.
 */
struct PlaybackState
{
    bool loopEnabled{false};
    double loopStartSeconds{0.0};
    double loopEndSeconds{10.0};
    float speedRatio{1.0f};       // 0.5 to 2.0 (1.0 = 100%)
    float pitchSemitones{0.0f};   // Honest display: linked to speed ratio unless dedicated DSP is bound
    bool autoNextTrack{true};
    float masterGain{1.0f};       // Linear gain 0.0 to 2.0
    bool routeStream{true};
    bool routeMonitor{true};
    int currentTrackIndex{0};

    std::vector<AudioMarker> markers;
    std::vector<PlaylistItem> playlist;

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("PlaybackState");
        xml->setAttribute("loopEnabled", loopEnabled);
        xml->setAttribute("loopStart", loopStartSeconds);
        xml->setAttribute("loopEnd", loopEndSeconds);
        xml->setAttribute("speedRatio", (double)speedRatio);
        xml->setAttribute("pitchSemitones", (double)pitchSemitones);
        xml->setAttribute("autoNextTrack", autoNextTrack);
        xml->setAttribute("masterGain", (double)masterGain);
        xml->setAttribute("routeStream", routeStream);
        xml->setAttribute("routeMonitor", routeMonitor);
        xml->setAttribute("currentTrackIndex", currentTrackIndex);

        auto* markersXml = xml->createNewChildElement("Markers");
        for (const auto& m : markers)
            markersXml->addChildElement(m.toXml().release());

        auto* playlistXml = xml->createNewChildElement("Playlist");
        for (const auto& item : playlist)
            playlistXml->addChildElement(item.toXml().release());

        return xml;
    }

    void fromXml(const juce::XmlElement& xml)
    {
        loopEnabled = xml.getBoolAttribute("loopEnabled", false);
        loopStartSeconds = xml.getDoubleAttribute("loopStart", 0.0);
        loopEndSeconds = xml.getDoubleAttribute("loopEnd", 10.0);
        speedRatio = juce::jlimit(0.25f, 4.0f, (float)xml.getDoubleAttribute("speedRatio", 1.0));
        pitchSemitones = (float)xml.getDoubleAttribute("pitchSemitones", 0.0);
        autoNextTrack = xml.getBoolAttribute("autoNextTrack", true);
        masterGain = juce::jlimit(0.0f, 2.0f, (float)xml.getDoubleAttribute("masterGain", 1.0));
        routeStream = xml.getBoolAttribute("routeStream", true);
        routeMonitor = xml.getBoolAttribute("routeMonitor", true);
        currentTrackIndex = xml.getIntAttribute("currentTrackIndex", 0);

        markers.clear();
        if (auto* mNode = xml.getChildByName("Markers"))
        {
            for (auto* c = mNode->getFirstChildElement(); c != nullptr; c = c->getNextElement())
            {
                if (c->hasTagName("Marker"))
                    markers.push_back(AudioMarker::fromXml(*c));
            }
        }

        playlist.clear();
        if (auto* pNode = xml.getChildByName("Playlist"))
        {
            for (auto* c = pNode->getFirstChildElement(); c != nullptr; c = c->getNextElement())
            {
                if (c->hasTagName("Track"))
                    playlist.push_back(PlaylistItem::fromXml(*c));
            }
        }
    }
};

} // namespace LiveBus
