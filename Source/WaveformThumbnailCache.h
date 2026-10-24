#pragma once
#include <JuceHeader.h>
#include "AudioPlaybackModel.h"
#include <vector>
#include <mutex>

namespace LiveBus
{

/**
 * High-performance, real-time safe stereo waveform peak generator and visualizer.
 * Extracts overview and detailed peaks without ever blocking the audio thread.
 */
class WaveformThumbnailCache
{
public:
    struct PeakPoint
    {
        float minL{0.0f};
        float maxL{0.0f};
        float minR{0.0f};
        float maxR{0.0f};
    };

    WaveformThumbnailCache();
    ~WaveformThumbnailCache() = default;

    void clear();

    /**
     * Extracts peak data from an AudioFormatReader.
     * Safe to call on file-loading worker thread.
     */
    bool generatePeaks(juce::AudioFormatReader& reader, int overviewResolution = 1024, int detailResolution = 8192);

    bool hasData() const { return !overviewPeaks.empty(); }
    double getDuration() const { return fileDuration; }

    /**
     * Renders the full-file Overview strip with loop boundary box, marker tags, and view window.
     */
    void drawOverview(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      double loopStart,
                      double loopEnd,
                      double currentPos,
                      double viewStart,
                      double viewEnd,
                      const std::vector<AudioMarker>& markers) const;

    /**
     * Renders the zoomed-in Detail stereo waveform with playhead, loop handles, and marker tags.
     */
    void drawDetail(juce::Graphics& g,
                    juce::Rectangle<float> bounds,
                    double viewStart,
                    double viewEnd,
                    double currentPos,
                    double loopStart,
                    double loopEnd,
                    bool loopEnabled,
                    const std::vector<AudioMarker>& markers) const;

private:
    mutable std::mutex peakMutex;
    std::vector<PeakPoint> overviewPeaks;
    std::vector<PeakPoint> detailPeaks;
    double fileDuration{0.0};
    int numChannels{2};
    double sampleRate{44100.0};
};

} // namespace LiveBus
