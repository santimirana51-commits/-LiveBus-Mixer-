#include "WaveformThumbnailCache.h"

namespace LiveBus
{

WaveformThumbnailCache::WaveformThumbnailCache()
{
}

void WaveformThumbnailCache::clear()
{
    std::lock_guard<std::mutex> lock(peakMutex);
    overviewPeaks.clear();
    detailPeaks.clear();
    fileDuration = 0.0;
}

bool WaveformThumbnailCache::generatePeaks(juce::AudioFormatReader& reader, int overviewResolution, int detailResolution)
{
    if (reader.lengthInSamples <= 0)
        return false;

    std::vector<PeakPoint> newOverview(overviewResolution);
    std::vector<PeakPoint> newDetail(detailResolution);

    juce::int64 totalSamples = reader.lengthInSamples;
    int channels = (int)reader.numChannels;
    double sr = reader.sampleRate;
    double dur = (double)totalSamples / sr;

    // Buffer chunk for reading (e.g. 16384 samples)
    const int bufferSize = 16384;
    juce::AudioBuffer<float> tempBuf(juce::jmax(2, channels), bufferSize);

    juce::int64 overviewBlockSize = juce::jmax((juce::int64)1, totalSamples / overviewResolution);
    juce::int64 detailBlockSize = juce::jmax((juce::int64)1, totalSamples / detailResolution);

    for (int i = 0; i < overviewResolution; ++i)
    {
        juce::int64 startSample = i * overviewBlockSize;
        int numToRead = (int)juce::jmin((juce::int64)bufferSize, juce::jmin(overviewBlockSize, totalSamples - startSample));
        if (numToRead <= 0) break;

        reader.read(&tempBuf, 0, numToRead, startSample, true, true);

        auto* readL = tempBuf.getReadPointer(0);
        auto* readR = (channels > 1) ? tempBuf.getReadPointer(1) : readL;

        float minL = 0.0f, maxL = 0.0f, minR = 0.0f, maxR = 0.0f;
        for (int s = 0; s < numToRead; ++s)
        {
            float sL = readL[s];
            float sR = readR[s];
            if (sL < minL) minL = sL;
            if (sL > maxL) maxL = sL;
            if (sR < minR) minR = sR;
            if (sR > maxR) maxR = sR;
        }

        newOverview[i].minL = minL;
        newOverview[i].maxL = maxL;
        newOverview[i].minR = minR;
        newOverview[i].maxR = maxR;
    }

    for (int i = 0; i < detailResolution; ++i)
    {
        juce::int64 startSample = i * detailBlockSize;
        int numToRead = (int)juce::jmin((juce::int64)bufferSize, juce::jmin(detailBlockSize, totalSamples - startSample));
        if (numToRead <= 0) break;

        reader.read(&tempBuf, 0, numToRead, startSample, true, true);

        auto* readL = tempBuf.getReadPointer(0);
        auto* readR = (channels > 1) ? tempBuf.getReadPointer(1) : readL;

        float minL = 0.0f, maxL = 0.0f, minR = 0.0f, maxR = 0.0f;
        for (int s = 0; s < numToRead; ++s)
        {
            float sL = readL[s];
            float sR = readR[s];
            if (sL < minL) minL = sL;
            if (sL > maxL) maxL = sL;
            if (sR < minR) minR = sR;
            if (sR > maxR) maxR = sR;
        }

        newDetail[i].minL = minL;
        newDetail[i].maxL = maxL;
        newDetail[i].minR = minR;
        newDetail[i].maxR = maxR;
    }

    {
        std::lock_guard<std::mutex> lock(peakMutex);
        overviewPeaks = std::move(newOverview);
        detailPeaks = std::move(newDetail);
        fileDuration = dur;
        numChannels = channels;
        sampleRate = sr;
    }

    return true;
}

void WaveformThumbnailCache::drawOverview(juce::Graphics& g,
                                          juce::Rectangle<float> bounds,
                                          double loopStart,
                                          double loopEnd,
                                          double currentPos,
                                          double viewStart,
                                          double viewEnd,
                                          const std::vector<AudioMarker>& markers) const
{
    std::lock_guard<std::mutex> lock(peakMutex);

    g.fillAll(juce::Colour(0xff12171e)); // Dark console background

    if (overviewPeaks.empty() || fileDuration <= 0.0)
    {
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawText("No Audio File Loaded", bounds, juce::Justification::centred, false);
        return;
    }

    float w = bounds.getWidth();
    float h = bounds.getHeight();
    float midY = bounds.getY() + h * 0.5f;
    float halfH = h * 0.45f;

    // Draw stereo dividing line
    g.setColour(juce::Colour(0xff222a36));
    g.drawHorizontalLine((int)midY, bounds.getX(), bounds.getRight());

    // Draw waveform peaks (vivid sea-green #22c55e / #10b981)
    juce::Colour waveColour(0xff10b981);
    g.setColour(waveColour);

    int numPoints = (int)overviewPeaks.size();
    for (int x = 0; x < (int)w; ++x)
    {
        int pIdx = juce::jlimit(0, numPoints - 1, (int)((float)x / w * (float)numPoints));
        const auto& p = overviewPeaks[pIdx];

        // Left channel (upper half: midY - halfH)
        float topL = midY - p.maxL * halfH * 0.9f;
        float btmL = midY - p.minL * halfH * 0.9f;
        if (btmL - topL < 1.0f) btmL = topL + 1.0f;
        g.drawVerticalLine((int)(bounds.getX() + x), topL, btmL);

        // Right channel (lower half: midY + halfH)
        float topR = midY + p.minR * halfH * 0.9f;
        float btmR = midY + p.maxR * halfH * 0.9f;
        if (btmR - topR < 1.0f) btmR = topR + 1.0f;
        g.drawVerticalLine((int)(bounds.getX() + x), topR, btmR);
    }

    // Draw Loop Region Box (Amber #f59e0b)
    if (loopEnd > loopStart && fileDuration > 0.0)
    {
        float startX = bounds.getX() + (float)(loopStart / fileDuration) * w;
        float endX = bounds.getX() + (float)(loopEnd / fileDuration) * w;
        juce::Rectangle<float> loopBox(startX, bounds.getY() + 2.0f, endX - startX, h - 4.0f);

        g.setColour(juce::Colour(0x33f59e0b)); // Semi-transparent amber
        g.fillRect(loopBox);
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawRect(loopBox, 1.5f);
    }

    // Draw View Window Box (Cyan/Blue border indicating zoomed area)
    if (viewEnd > viewStart && fileDuration > 0.0)
    {
        float vx1 = bounds.getX() + (float)(viewStart / fileDuration) * w;
        float vx2 = bounds.getX() + (float)(viewEnd / fileDuration) * w;
        juce::Rectangle<float> viewBox(vx1, bounds.getY(), juce::jmax(2.0f, vx2 - vx1), h);

        g.setColour(juce::Colour(0x2200e5ff));
        g.fillRect(viewBox);
        g.setColour(juce::Colour(0x8800e5ff));
        g.drawRect(viewBox, 1.0f);
    }

    // Draw Marker Pins on overview
    for (const auto& m : markers)
    {
        if (m.positionSeconds >= 0.0 && m.positionSeconds <= fileDuration)
        {
            float mx = bounds.getX() + (float)(m.positionSeconds / fileDuration) * w;
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawVerticalLine((int)mx, bounds.getY(), bounds.getBottom());

            // Marker flag tag
            g.fillEllipse(mx - 3.0f, bounds.getY() + 2.0f, 6.0f, 6.0f);
        }
    }

    // Draw Playhead (Red)
    if (fileDuration > 0.0 && currentPos >= 0.0)
    {
        float px = bounds.getX() + (float)(currentPos / fileDuration) * w;
        g.setColour(juce::Colour(0xffef4444));
        g.drawVerticalLine((int)px, bounds.getY(), bounds.getBottom());
    }

    // Subtle border
    g.setColour(juce::Colour(0xff2d3748));
    g.drawRect(bounds, 1.0f);
}

void WaveformThumbnailCache::drawDetail(juce::Graphics& g,
                                        juce::Rectangle<float> bounds,
                                        double viewStart,
                                        double viewEnd,
                                        double currentPos,
                                        double loopStart,
                                        double loopEnd,
                                        bool loopEnabled,
                                        const std::vector<AudioMarker>& markers) const
{
    std::lock_guard<std::mutex> lock(peakMutex);

    g.fillAll(juce::Colour(0xff10151c)); // Deep dark console

    if (detailPeaks.empty() || fileDuration <= 0.0 || viewEnd <= viewStart)
    {
        g.setColour(juce::Colours::grey.withAlpha(0.2f));
        g.drawText("No Waveform Data", bounds, juce::Justification::centred, false);
        return;
    }

    float w = bounds.getWidth();
    float h = bounds.getHeight();
    float midY = bounds.getY() + h * 0.5f;
    float halfH = h * 0.45f;
    double viewSpan = viewEnd - viewStart;

    // Draw stereo channel division
    g.setColour(juce::Colour(0xff1f2937));
    g.drawHorizontalLine((int)midY, bounds.getX(), bounds.getRight());

    // Waveform: Electric Cyan / Aqua #00e5ff
    juce::Colour waveColour(0xff00e5ff);
    g.setColour(waveColour);

    int numDetail = (int)detailPeaks.size();
    for (int x = 0; x < (int)w; ++x)
    {
        double t = viewStart + ((double)x / (double)w) * viewSpan;
        if (t < 0.0 || t > fileDuration)
            continue;

        float normTime = (float)(t / fileDuration);
        int pIdx = juce::jlimit(0, numDetail - 1, (int)(normTime * (float)numDetail));
        const auto& p = detailPeaks[pIdx];

        // Left channel (upper half)
        float topL = midY - p.maxL * halfH * 0.92f;
        float btmL = midY - p.minL * halfH * 0.92f;
        if (btmL - topL < 1.0f) btmL = topL + 1.0f;
        g.drawVerticalLine((int)(bounds.getX() + x), topL, btmL);

        // Right channel (lower half)
        float topR = midY + p.minR * halfH * 0.92f;
        float btmR = midY + p.maxR * halfH * 0.92f;
        if (btmR - topR < 1.0f) btmR = topR + 1.0f;
        g.drawVerticalLine((int)(bounds.getX() + x), topR, btmR);
    }

    // Draw Loop Region if visible in detail view
    if (loopEnd > loopStart)
    {
        float lx1 = bounds.getX() + (float)((loopStart - viewStart) / viewSpan) * w;
        float lx2 = bounds.getX() + (float)((loopEnd - viewStart) / viewSpan) * w;

        if (loopEnabled)
        {
            float fillX1 = juce::jmax(bounds.getX(), lx1);
            float fillX2 = juce::jmin(bounds.getRight(), lx2);
            if (fillX2 > fillX1)
            {
                g.setColour(juce::Colour(0x1af59e0b)); // Soft amber tint
                g.fillRect(fillX1, bounds.getY(), fillX2 - fillX1, h);
            }
        }

        // Loop Start Handle & Line
        if (lx1 >= bounds.getX() - 10 && lx1 <= bounds.getRight() + 10)
        {
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawVerticalLine((int)lx1, bounds.getY(), bounds.getBottom());

            // Loop Start Flag at bottom
            juce::Path startFlag;
            startFlag.startNewSubPath(lx1, bounds.getBottom() - 18.0f);
            startFlag.lineTo(lx1 + 42.0f, bounds.getBottom() - 18.0f);
            startFlag.lineTo(lx1 + 42.0f, bounds.getBottom() - 4.0f);
            startFlag.lineTo(lx1, bounds.getBottom() - 4.0f);
            startFlag.closeSubPath();
            g.fillPath(startFlag);

            g.setColour(juce::Colours::black);
            g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
            g.drawText("Loop", (int)lx1 + 2, (int)bounds.getBottom() - 18, 40, 14, juce::Justification::centred, false);
        }

        // Loop End Handle & Line
        if (lx2 >= bounds.getX() - 10 && lx2 <= bounds.getRight() + 10)
        {
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawVerticalLine((int)lx2, bounds.getY(), bounds.getBottom());

            // Loop End Flag handle
            juce::Path endFlag;
            endFlag.startNewSubPath(lx2, bounds.getBottom() - 18.0f);
            endFlag.lineTo(lx2 - 14.0f, bounds.getBottom() - 18.0f);
            endFlag.lineTo(lx2, bounds.getBottom() - 4.0f);
            endFlag.closeSubPath();
            g.fillPath(endFlag);
        }
    }

    // Draw Markers
    for (const auto& m : markers)
    {
        if (m.positionSeconds >= viewStart && m.positionSeconds <= viewEnd)
        {
            float mx = bounds.getX() + (float)((m.positionSeconds - viewStart) / viewSpan) * w;

            // Vertical marker line
            g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.8f));
            const float dashes[] = { 4.0f, 4.0f };
            g.drawDashedLine(juce::Line<float>(mx, bounds.getY(), mx, bounds.getBottom()), dashes, 2, 1.0f);

            // Marker flag badge with text
            float flagW = (float)juce::jmax(50, m.name.length() * 8 + 14);
            juce::Rectangle<float> flagBox(mx, bounds.getBottom() - 20.0f, flagW, 16.0f);

            g.setColour(juce::Colour(0xfff59e0b));
            g.fillRoundedRectangle(flagBox, 3.0f);

            g.setColour(juce::Colours::black);
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText(m.name, flagBox, juce::Justification::centred, true);
        }
    }

    // Draw Playhead (Vertical Red Line with Pointer Flag)
    if (currentPos >= viewStart && currentPos <= viewEnd)
    {
        float px = bounds.getX() + (float)((currentPos - viewStart) / viewSpan) * w;

        g.setColour(juce::Colour(0xffef4444)); // Neon Red
        g.drawLine(px, bounds.getY(), px, bounds.getBottom(), 2.0f);

        // Triangular playhead marker cursor at top
        juce::Path head;
        head.startNewSubPath(px - 7.0f, bounds.getY());
        head.lineTo(px + 7.0f, bounds.getY());
        head.lineTo(px, bounds.getY() + 10.0f);
        head.closeSubPath();
        g.fillPath(head);
    }

    // Border
    g.setColour(juce::Colour(0xff2d3748));
    g.drawRect(bounds, 1.0f);
}

} // namespace LiveBus
