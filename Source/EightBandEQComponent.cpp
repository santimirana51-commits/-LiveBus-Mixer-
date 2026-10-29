#include "EightBandEQComponent.h"
#include "ChannelStripDSP.h"
#include <iomanip>
#include <sstream>

namespace LiveBus
{
    juce::String EightBandEQComponent::getTargetName(TargetIndex target)
    {
        if ((int)target >= 0 && (int)target < maxInputs)
            return "CH" + juce::String((int)target + 1);
        if (target == TargetStreamBus)
            return "Stream Bus";
        if (target == TargetMonitorBus)
            return "Monitor Bus";
        return "Target";
    }

    juce::Colour EightBandEQComponent::getBandColour(int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return juce::Colour(0xffff2d60); // Band 1: Pink / Coral
            case 1: return juce::Colour(0xffff7033); // Band 2: Orange
            case 2: return juce::Colour(0xffffc800); // Band 3: Yellow
            case 3: return juce::Colour(0xff10d060); // Band 4: Green
            case 4: return juce::Colour(0xff00d4d4); // Band 5: Cyan
            case 5: return juce::Colour(0xff3b82f6); // Band 6: Blue
            case 6: return juce::Colour(0xff9333ea); // Band 7: Purple
            case 7: return juce::Colour(0xfff43f5e); // Band 8: Magenta / Hot Pink
            default: return juce::Colour(0xff00f5d4);
        }
    }

    void EightBandEQComponent::BandKnobLookAndFeel::drawRotarySlider(
        juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider& /*slider*/)
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.44f;
        const auto center = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knob body background
        g.setColour(juce::Colour(0xff111827));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff1f2937));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        // Active Arc
        juce::Path arcPath;
        arcPath.addCentredArc(center.x, center.y, radius - 2.5f, radius - 2.5f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(activeColor);
        g.strokePath(arcPath, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pointer dot
        const auto pointerLen = radius * 0.65f;
        const auto px = center.x + pointerLen * std::cos(angle - juce::MathConstants<float>::halfPi);
        const auto py = center.y + pointerLen * std::sin(angle - juce::MathConstants<float>::halfPi);

        g.setColour(juce::Colours::white);
        g.fillEllipse(px - 2.2f, py - 2.2f, 4.4f, 4.4f);
    }

    // =========================================================================
    // EQGraphVisualizer Implementation
    // =========================================================================
    EightBandEQComponent::EQGraphVisualizer::EQGraphVisualizer(EightBandEQComponent& o)
        : owner(o)
    {
        setRepaintsOnMouseActivity(true);
    }

    float EightBandEQComponent::EQGraphVisualizer::freqToX(float freq) const
    {
        const float clamped = juce::jlimit(20.0f, 20000.0f, freq);
        const float norm = std::log10(clamped / 20.0f) / std::log10(20000.0f / 20.0f);
        const float graphW = (float)getWidth() - 70.0f;
        return 40.0f + norm * graphW;
    }

    float EightBandEQComponent::EQGraphVisualizer::xToFreq(float x) const
    {
        const float graphW = (float)getWidth() - 70.0f;
        const float norm = juce::jlimit(0.0f, 1.0f, (x - 40.0f) / graphW);
        return 20.0f * std::pow(1000.0f, norm);
    }

    float EightBandEQComponent::EQGraphVisualizer::gainToY(float gainDb) const
    {
        const float clamped = juce::jlimit(-24.0f, 24.0f, gainDb);
        const float norm = (clamped - (-24.0f)) / 48.0f;
        const float graphH = (float)getHeight() - 40.0f;
        return 15.0f + (1.0f - norm) * graphH;
    }

    float EightBandEQComponent::EQGraphVisualizer::yToGain(float y) const
    {
        const float graphH = (float)getHeight() - 40.0f;
        const float norm = juce::jlimit(0.0f, 1.0f, (y - 15.0f) / graphH);
        return 24.0f - norm * 48.0f;
    }

    void EightBandEQComponent::EQGraphVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const float meterW = 60.0f;
        const auto graphBounds = bounds.withTrimmedRight(meterW);

        // 1. Graph Background
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(graphBounds);

        // 2. Grid lines & dB labels
        g.setFont(juce::FontOptions(10.0f));
        g.setColour(juce::Colour(0xff1e293b));

        const float dBs[] = { 24.0f, 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f };
        for (float db : dBs)
        {
            float y = gainToY(db);
            if (db == 0.0f)
            {
                g.setColour(juce::Colour(0xff334155));
                g.drawHorizontalLine((int)y, 35.0f, graphBounds.getRight());
                g.setColour(juce::Colour(0xff1e293b));
            }
            else
            {
                g.drawHorizontalLine((int)y, 35.0f, graphBounds.getRight());
            }

            g.setColour(juce::Colour(0xff64748b));
            juce::String label = (db > 0.0f ? "+" : "") + juce::String((int)db);
            g.drawText(label, 4, (int)y - 7, 28, 14, juce::Justification::centredRight);
            g.setColour(juce::Colour(0xff1e293b));
        }

        // Frequency grid lines & labels
        const float freqs[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        for (float f : freqs)
        {
            float x = freqToX(f);
            g.drawVerticalLine((int)x, 15.0f, bounds.getBottom() - 20.0f);

            juce::String fStr;
            if (f >= 1000.0f)
                fStr = juce::String((int)(f / 1000.0f)) + "k";
            else
                fStr = juce::String((int)f);

            g.setColour(juce::Colour(0xff64748b));
            g.drawText(fStr, (int)x - 16, (int)bounds.getBottom() - 18, 32, 14, juce::Justification::centred);
            g.setColour(juce::Colour(0xff1e293b));
        }

        // 3. Spectrum Analyzer Display
        const auto& proc = owner.getCurrentProcessor();
        if (proc.isAnalyzerEnabled())
        {
            juce::Path fftPath;
            bool started = false;
            const float graphH = bounds.getHeight() - 40.0f;
            const float binFrequency = (float)proc.getSampleRate() * 4.0f / (float)fftSize;

            for (size_t i = 0; i < owner.smoothedFftBins.size(); ++i)
            {
                float freq = (float)i * binFrequency;
                if (freq < 20.0f || freq > 20000.0f) continue;

                float x = freqToX(freq);
                float db = owner.smoothedFftBins[i];
                float norm = juce::jlimit(0.0f, 1.0f, (db - (-80.0f)) / 90.0f);
                float y = bounds.getBottom() - 25.0f - norm * (graphH * 0.9f);

                if (!started)
                {
                    fftPath.startNewSubPath(x, y);
                    started = true;
                }
                else
                {
                    fftPath.lineTo(x, y);
                }
            }

            if (started)
            {
                juce::Path fillPath = fftPath;
                fillPath.lineTo(freqToX(20000.0f), bounds.getBottom() - 25.0f);
                fillPath.lineTo(freqToX(20.0f), bounds.getBottom() - 25.0f);
                fillPath.closeSubPath();

                g.setColour(juce::Colour(0xff1e3a8a).withAlpha(0.25f));
                g.fillPath(fillPath);

                g.setColour(juce::Colour(0xff60a5fa).withAlpha(0.45f));
                g.strokePath(fftPath, juce::PathStrokeType(1.0f));
            }
        }

        // 4. Combined EQ Curve
        const int numPoints = (int)graphBounds.getWidth() - 40;
        if (numPoints > 10)
        {
            juce::Path curvePath;
            bool first = true;

            for (int i = 0; i < numPoints; ++i)
            {
                float x = 40.0f + (float)i;
                float f = xToFreq(x);
                float magDb = proc.getMagnitudeForFrequency(f);
                float y = gainToY(magDb);

                if (first)
                {
                    curvePath.startNewSubPath(x, y);
                    first = false;
                }
                else
                {
                    curvePath.lineTo(x, y);
                }
            }

            // Fill area under curve
            juce::Path fillPath = curvePath;
            fillPath.lineTo(graphBounds.getRight() - 1.0f, gainToY(0.0f));
            fillPath.lineTo(40.0f, gainToY(0.0f));
            fillPath.closeSubPath();

            juce::ColourGradient fillGrad(
                juce::Colour(0xff00f5d4).withAlpha(0.20f), 0.0f, gainToY(12.0f),
                juce::Colour(0xff00f5d4).withAlpha(0.02f), 0.0f, gainToY(0.0f), false);
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);

            // Neon glowing stroke
            g.setColour(juce::Colour(0xff00f5d4).withAlpha(0.35f));
            g.strokePath(curvePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(juce::Colour(0xff00f5d4));
            g.strokePath(curvePath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 5. Band Interactive Nodes
        for (int b = 0; b < numEQBands; ++b)
        {
            const float f = proc.params.bands[b].freq.load(std::memory_order_relaxed);
            const float gDb = proc.params.bands[b].gainDb.load(std::memory_order_relaxed);
            const auto type = proc.params.bands[b].type.load(std::memory_order_relaxed);
            const bool enabled = proc.params.bands[b].enabled.load(std::memory_order_relaxed);

            const float nx = freqToX(f);
            const float ny = (type == EQFilterType::HighPass || type == EQFilterType::LowPass)
                ? gainToY(0.0f)
                : gainToY(gDb);

            const auto nodeColour = enabled ? getBandColour(b) : juce::Colours::grey;
            const bool isSelected = (owner.selectedBand == b);
            const bool isHovered = (hoveredBand == b);

            if (isSelected || isHovered)
            {
                g.setColour(nodeColour.withAlpha(0.35f));
                g.fillEllipse(nx - 14.0f, ny - 14.0f, 28.0f, 28.0f);
            }

            g.setColour(nodeColour);
            g.fillEllipse(nx - 9.0f, ny - 9.0f, 18.0f, 18.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(nx - 9.0f, ny - 9.0f, 18.0f, 18.0f, isSelected ? 2.0f : 1.2f);

            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(juce::String(b + 1), (int)nx - 9, (int)ny - 9, 18, 18, juce::Justification::centred);
        }

        // 6. Right-Side Master Peak Meters (IN and OUT)
        const auto meterBounds = juce::Rectangle<float>(bounds.getRight() - meterW, bounds.getY(), meterW, bounds.getHeight());
        const float inL = proc.getMeterInL();
        const float inR = proc.getMeterInR();
        const float outL = proc.getMeterOutL();
        const float outR = proc.getMeterOutR();

        g.setColour(juce::Colour(0xff0d121c));
        g.fillRoundedRectangle(meterBounds.reduced(2.0f), 4.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(meterBounds.reduced(2.0f), 4.0f, 1.0f);

        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("IN", (int)meterBounds.getX() + 6, (int)meterBounds.getY() + 4, 20, 12, juce::Justification::centred);
        g.drawText("OUT", (int)meterBounds.getX() + 32, (int)meterBounds.getY() + 4, 22, 12, juce::Justification::centred);

        const float barTop = meterBounds.getY() + 20.0f;
        const float barBottom = meterBounds.getBottom() - 24.0f;
        const float barH = barBottom - barTop;

        auto drawVuBar = [&](float val, int x, int w)
        {
            float db = juce::Decibels::gainToDecibels(val, -60.0f);
            float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 72.0f);
            float fillH = norm * barH;
            float fillY = barBottom - fillH;

            g.setColour(juce::Colour(0xff161f2e));
            g.fillRect((float)x, barTop, (float)w, barH);

            juce::ColourGradient grad(
                juce::Colour(0xff10b981), (float)x, barBottom,
                juce::Colour(0xffef4444), (float)x, barTop, false);
            grad.addColour(0.7, juce::Colour(0xffeab308));

            g.setGradientFill(grad);
            g.fillRect((float)x, fillY, (float)w, fillH);
        };

        drawVuBar((inL + inR) * 0.5f, (int)meterBounds.getX() + 10, 12);
        drawVuBar((outL + outR) * 0.5f, (int)meterBounds.getX() + 36, 12);

        float peakInDb = juce::Decibels::gainToDecibels(juce::jmax(inL, inR), -60.0f);
        float peakOutDb = juce::Decibels::gainToDecibels(juce::jmax(outL, outR), -60.0f);

        g.setFont(juce::FontOptions(9.0f));
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawText(juce::String(peakInDb, 1), (int)meterBounds.getX() + 2, (int)barBottom + 4, 28, 14, juce::Justification::centred);
        g.drawText(juce::String(peakOutDb, 1), (int)meterBounds.getX() + 30, (int)barBottom + 4, 28, 14, juce::Justification::centred);
    }

    void EightBandEQComponent::EQGraphVisualizer::mouseDown(const juce::MouseEvent& e)
    {
        activeDragBand = -1;
        const auto& proc = owner.getCurrentProcessor();

        for (int b = 0; b < numEQBands; ++b)
        {
            const float f = proc.params.bands[b].freq.load(std::memory_order_relaxed);
            const float gDb = proc.params.bands[b].gainDb.load(std::memory_order_relaxed);
            const auto type = proc.params.bands[b].type.load(std::memory_order_relaxed);

            const float nx = freqToX(f);
            const float ny = (type == EQFilterType::HighPass || type == EQFilterType::LowPass)
                ? gainToY(0.0f)
                : gainToY(gDb);

            if (e.position.getDistanceFrom(juce::Point<float>(nx, ny)) <= 18.0f)
            {
                activeDragBand = b;
                owner.selectedBand = b;
                repaint();
                break;
            }
        }
    }

    void EightBandEQComponent::EQGraphVisualizer::mouseDrag(const juce::MouseEvent& e)
    {
        if (activeDragBand < 0 || activeDragBand >= numEQBands) return;

        auto& proc = owner.getCurrentProcessor();
        const auto type = proc.params.bands[activeDragBand].type.load(std::memory_order_relaxed);

        const float newFreq = juce::jlimit(20.0f, 20000.0f, xToFreq(e.position.x));
        proc.params.bands[activeDragBand].freq.store(newFreq);

        if (type != EQFilterType::HighPass && type != EQFilterType::LowPass)
        {
            const float newGain = juce::jlimit(-24.0f, 24.0f, yToGain(e.position.y));
            proc.params.bands[activeDragBand].gainDb.store(newGain);
        }

        proc.updateCoefficients();
        owner.updateUIFromProcessor();
        if (owner.onParametersChanged)
            owner.onParametersChanged(owner.currentTarget);
        repaint();
    }

    void EightBandEQComponent::EQGraphVisualizer::mouseUp(const juce::MouseEvent& /*e*/)
    {
        activeDragBand = -1;
        repaint();
    }

    void EightBandEQComponent::EQGraphVisualizer::mouseMove(const juce::MouseEvent& e)
    {
        int newHover = -1;
        const auto& proc = owner.getCurrentProcessor();

        for (int b = 0; b < numEQBands; ++b)
        {
            const float f = proc.params.bands[b].freq.load(std::memory_order_relaxed);
            const float gDb = proc.params.bands[b].gainDb.load(std::memory_order_relaxed);
            const auto type = proc.params.bands[b].type.load(std::memory_order_relaxed);

            const float nx = freqToX(f);
            const float ny = (type == EQFilterType::HighPass || type == EQFilterType::LowPass)
                ? gainToY(0.0f)
                : gainToY(gDb);

            if (e.position.getDistanceFrom(juce::Point<float>(nx, ny)) <= 18.0f)
            {
                newHover = b;
                break;
            }
        }

        if (newHover != hoveredBand)
        {
            hoveredBand = newHover;
            repaint();
        }
    }

    void EightBandEQComponent::EQGraphVisualizer::mouseExit(const juce::MouseEvent& /*e*/)
    {
        hoveredBand = -1;
        repaint();
    }

    // =========================================================================
    // 2. High-Pass Filter Visualizer
    // =========================================================================
    EightBandEQComponent::HPFCurveVisualizer::HPFCurveVisualizer(EightBandEQComponent& o) : owner(o) {}

    void EightBandEQComponent::HPFCurveVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(bounds);

        auto* dsp = owner.getCurrentTargetDSP();
        float cutoff = dsp ? dsp->params.hpfFreq.load(std::memory_order_relaxed) : 80.0f;
        int slope = dsp ? dsp->params.hpfSlope.load(std::memory_order_relaxed) : 1;
        bool enabled = dsp ? dsp->params.hpfEnabled.load(std::memory_order_relaxed) : true;

        // Grid lines
        g.setColour(juce::Colour(0xff1e293b));
        for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            float norm = std::log10(f / 20.0f) / std::log10(20000.0f / 20.0f);
            float x = bounds.getX() + 30.0f + norm * (bounds.getWidth() - 60.0f);
            g.drawVerticalLine((int)x, 10.0f, bounds.getBottom() - 20.0f);
        }

        for (float db : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f })
        {
            float norm = (db - (-24.0f)) / 36.0f;
            float y = bounds.getBottom() - 25.0f - norm * (bounds.getHeight() - 40.0f);
            g.drawHorizontalLine((int)y, 25.0f, bounds.getRight() - 25.0f);
        }

        // Response Curve
        juce::Path p;
        bool first = true;
        float slopeDbPerOct = (slope == 0 ? 6.0f : slope == 1 ? 12.0f : slope == 2 ? 18.0f : 24.0f);

        for (int x = 30; x <= (int)bounds.getWidth() - 30; ++x)
        {
            float norm = (float)(x - 30) / (bounds.getWidth() - 60.0f);
            float f = 20.0f * std::pow(1000.0f, norm);
            float db = 0.0f;

            if (enabled)
            {
                if (f < cutoff)
                {
                    float octavesBelow = std::log2(cutoff / f);
                    db = -octavesBelow * slopeDbPerOct;
                }
                else
                {
                    db = 0.0f;
                }
            }
            db = juce::jlimit(-36.0f, 12.0f, db);
            float normY = (db - (-24.0f)) / 36.0f;
            float y = bounds.getBottom() - 25.0f - normY * (bounds.getHeight() - 40.0f);

            if (first) { p.startNewSubPath((float)x, y); first = false; }
            else { p.lineTo((float)x, y); }
        }

        g.setColour(enabled ? juce::Colour(0xff00f5d4) : juce::Colour(0xff64748b));
        g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Cutoff Marker Line
        float cNorm = std::log10(juce::jlimit(20.0f, 20000.0f, cutoff) / 20.0f) / std::log10(20000.0f / 20.0f);
        float cx = bounds.getX() + 30.0f + cNorm * (bounds.getWidth() - 60.0f);
        g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.6f));
        g.drawVerticalLine((int)cx, 10.0f, bounds.getBottom() - 20.0f);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawText("Cutoff: " + juce::String((int)cutoff) + " Hz (" + juce::String((int)slopeDbPerOct) + " dB/Oct)",
                   (int)cx + 6, 12, 180, 16, juce::Justification::left);
    }

    // =========================================================================
    // 3. Noise Gate Visualizer
    // =========================================================================
    EightBandEQComponent::GateVisualizer::GateVisualizer(EightBandEQComponent& o) : owner(o) {}

    void EightBandEQComponent::GateVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(bounds);

        auto* dsp = owner.getCurrentTargetDSP();
        float gr = dsp ? dsp->getGateGainReductionDb() : 0.0f;
        int state = dsp ? dsp->getGateState() : 2; // 0=Closed, 1=Hold, 2=Open
        float inDb = dsp ? dsp->getInputPeakDb() : -60.0f;
        float thresh = dsp ? dsp->params.gateThresholdDb.load() : -45.0f;

        // Draw State Indicator Pill
        juce::Colour stateColour = (state == 2) ? juce::Colour(0xff10b981) : (state == 1) ? juce::Colour(0xfff59e0b) : juce::Colour(0xffef4444);
        juce::String stateText = (state == 2) ? "GATE: OPEN" : (state == 1) ? "GATE: HOLD" : "GATE: CLOSED";

        g.setColour(stateColour.withAlpha(0.2f));
        g.fillRoundedRectangle(20.0f, 15.0f, 130.0f, 26.0f, 13.0f);
        g.setColour(stateColour);
        g.drawRoundedRectangle(20.0f, 15.0f, 130.0f, 26.0f, 13.0f, 1.5f);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(stateText, 20, 15, 130, 26, juce::Justification::centred);

        // Draw Live Meters: Input Level vs Gain Reduction Bar
        float meterY = 60.0f;
        float barH = bounds.getHeight() - meterY - 30.0f;

        // Input Level Meter
        g.setFont(juce::FontOptions(11.0f));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Input Level", 30, (int)meterY, 100, 16, juce::Justification::left);
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRect(30.0f, meterY + 20.0f, 24.0f, barH);
        float normIn = juce::jlimit(0.0f, 1.0f, (inDb + 70.0f) / 70.0f);
        g.setColour(inDb >= thresh ? juce::Colour(0xff10b981) : juce::Colour(0xff3b82f6));
        g.fillRect(30.0f, meterY + 20.0f + barH * (1.0f - normIn), 24.0f, barH * normIn);

        // Threshold marker
        float normThresh = juce::jlimit(0.0f, 1.0f, (thresh + 70.0f) / 70.0f);
        float ty = meterY + 20.0f + barH * (1.0f - normThresh);
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawHorizontalLine((int)ty, 25.0f, 60.0f);
        g.drawText("Threshold: " + juce::String(thresh, 1) + " dB", 70, (int)ty - 7, 120, 14, juce::Justification::left);

        // Gain Reduction Meter
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Gain Reduction", 220, (int)meterY, 120, 16, juce::Justification::left);
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRect(220.0f, meterY + 20.0f, 24.0f, barH);
        float normGR = juce::jlimit(0.0f, 1.0f, gr / 60.0f);
        g.setColour(juce::Colour(0xffef4444));
        g.fillRect(220.0f, meterY + 20.0f, 24.0f, barH * normGR);

        g.setColour(juce::Colour(0xfff87171));
        g.drawText("-" + juce::String(gr, 1) + " dB", 252, (int)meterY + 20, 80, 16, juce::Justification::left);
    }

    // =========================================================================
    // 4. Dynamics Compressor Visualizer
    // =========================================================================
    EightBandEQComponent::CompCurveVisualizer::CompCurveVisualizer(EightBandEQComponent& o) : owner(o) {}

    void EightBandEQComponent::CompCurveVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(bounds);

        auto* dsp = owner.getCurrentTargetDSP();
        float thresh = dsp ? dsp->params.compThresholdDb.load() : -18.0f;
        float ratio = dsp ? dsp->params.compRatio.load() : 3.0f;
        float gr = dsp ? dsp->getCompGainReductionDb() : 0.0f;
        float inDb = dsp ? dsp->getInputPeakDb() : -60.0f;

        // Knee transfer graph area
        const auto graphArea = bounds.reduced(35.0f, 25.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRect(graphArea, 1.0f);

        // 1:1 diagonal line
        g.setColour(juce::Colour(0xff334155));
        g.drawLine(graphArea.getX(), graphArea.getBottom(), graphArea.getRight(), graphArea.getY(), 1.0f);

        auto dbToX = [&](float db) { return graphArea.getX() + juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f) * graphArea.getWidth(); };
        auto dbToY = [&](float db) { return graphArea.getBottom() - juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f) * graphArea.getHeight(); };

        // Draw Compressor transfer curve
        juce::Path p;
        p.startNewSubPath(dbToX(-60.0f), dbToY(-60.0f));
        p.lineTo(dbToX(thresh), dbToY(thresh));

        float outAt0 = thresh + (0.0f - thresh) / ratio;
        p.lineTo(dbToX(0.0f), dbToY(outAt0));

        g.setColour(juce::Colour(0xff00f5d4));
        g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Operating point dot
        float currentOutDb = inDb <= thresh ? inDb : thresh + (inDb - thresh) / ratio;
        float dotX = dbToX(inDb);
        float dotY = dbToY(currentOutDb);
        g.setColour(juce::Colour(0xfff59e0b));
        g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);

        // Labels
        g.setFont(juce::FontOptions(11.0f));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Thresh: " + juce::String(thresh, 1) + " dB", (int)graphArea.getX() + 6, (int)graphArea.getY() + 4, 120, 14, juce::Justification::left);
        g.drawText("Ratio: " + juce::String(ratio, 1) + ":1", (int)graphArea.getX() + 6, (int)graphArea.getY() + 20, 120, 14, juce::Justification::left);

        // Live GR Text
        g.setColour(juce::Colour(0xfff87171));
        g.drawText("Gain Reduction: -" + juce::String(gr, 1) + " dB", (int)graphArea.getRight() - 150, (int)graphArea.getY() + 4, 144, 14, juce::Justification::right);
    }

    // =========================================================================
    // 5. Limiter Visualizer
    // =========================================================================
    EightBandEQComponent::LimiterVisualizer::LimiterVisualizer(EightBandEQComponent& o) : owner(o) {}

    void EightBandEQComponent::LimiterVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(bounds);

        auto* dsp = owner.getCurrentTargetDSP();
        float ceil = dsp ? dsp->params.limiterCeilingDb.load() : -1.0f;
        float gr = dsp ? dsp->getLimiterGainReductionDb() : 0.0f;
        float outDb = dsp ? dsp->getOutputPeakDb() : -60.0f;
        bool clip = dsp ? dsp->isClipping() : false;

        float barW = 32.0f;
        float barH = bounds.getHeight() - 70.0f;
        float cy = 35.0f;

        // Ceiling marker line across visualizer
        float normCeil = juce::jlimit(0.0f, 1.0f, (ceil + 24.0f) / 24.0f);
        float lineY = cy + barH * (1.0f - normCeil);

        g.setColour(juce::Colour(0xfff59e0b));
        g.drawHorizontalLine((int)lineY, 30.0f, bounds.getRight() - 30.0f);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("Ceiling Limit: " + juce::String(ceil, 1) + " dBFS", (int)bounds.getWidth() - 170, (int)lineY - 16, 140, 14, juce::Justification::right);

        // Live Output Peak Bar
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRect(60.0f, cy, barW, barH);
        float normOut = juce::jlimit(0.0f, 1.0f, (outDb + 24.0f) / 24.0f);
        g.setColour(clip ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));
        g.fillRect(60.0f, cy + barH * (1.0f - normOut), barW, barH * normOut);

        // Live Gain Reduction Bar
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRect(160.0f, cy, barW, barH);
        float normGR = juce::jlimit(0.0f, 1.0f, gr / 20.0f);
        g.setColour(juce::Colour(0xffef4444));
        g.fillRect(160.0f, cy, barW, barH * normGR);

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawText("Output: " + juce::String(outDb, 1) + " dBFS", 40, (int)(cy + barH + 8), 110, 16, juce::Justification::centred);
        g.drawText("GR: -" + juce::String(gr, 1) + " dB", 140, (int)(cy + barH + 8), 100, 16, juce::Justification::centred);

        // Clip Badge
        if (clip)
        {
            g.setColour(juce::Colour(0xffef4444));
            g.fillRoundedRectangle(bounds.getRight() - 90.0f, 15.0f, 70.0f, 22.0f, 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText("CLIPPING", (int)bounds.getRight() - 90, 15, 70, 22, juce::Justification::centred);
        }
    }

    // =========================================================================
    // 6. Reverb Visualizer
    // =========================================================================
    EightBandEQComponent::ReverbVisualizer::ReverbVisualizer(EightBandEQComponent& o) : owner(o) {}

    void EightBandEQComponent::ReverbVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(bounds);

        auto* dsp = owner.getCurrentTargetDSP();
        float room = dsp ? dsp->params.reverbRoomSize.load() : 0.5f;
        float damp = dsp ? dsp->params.reverbDamping.load() : 0.5f;
        float wet = dsp ? dsp->params.reverbWetLevel.load() : 0.33f;
        float dry = dsp ? dsp->params.reverbDryLevel.load() : 1.0f;

        // Draw Acoustic Room Boundary
        auto roomBox = bounds.reduced(50.0f, 30.0f);
        float rw = roomBox.getWidth() * (0.3f + room * 0.7f);
        float rh = roomBox.getHeight() * (0.3f + room * 0.7f);
        auto currentRoom = roomBox.withSizeKeepingCentre(rw, rh);

        g.setColour(juce::Colour(0xff1e3a8a).withAlpha(0.25f));
        g.fillRoundedRectangle(currentRoom, 8.0f);
        g.setColour(juce::Colour(0xff3b82f6));
        g.drawRoundedRectangle(currentRoom, 8.0f, 1.5f);

        // Reflection ripples
        const int ripples = 4;
        for (int r = 1; r <= ripples; ++r)
        {
            float rippleAlpha = (1.0f - (float)r / (float)(ripples + 1)) * (1.0f - damp * 0.5f);
            g.setColour(juce::Colour(0xff00f5d4).withAlpha(rippleAlpha * 0.4f));
            g.drawRoundedRectangle(currentRoom.reduced((float)r * 12.0f), 6.0f, 1.0f);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff00f5d4));
        g.drawText("Acoustic Space: Room " + juce::String((int)(room * 100.0f)) + "% | Damping " + juce::String((int)(damp * 100.0f)) + "%",
                   (int)bounds.getX(), (int)bounds.getY() + 10, (int)bounds.getWidth(), 20, juce::Justification::centred);

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Wet Level: " + juce::String((int)(wet * 100.0f)) + "% | Dry Level: " + juce::String((int)(dry * 100.0f)) + "%",
                   (int)bounds.getX(), (int)bounds.getBottom() - 25, (int)bounds.getWidth(), 20, juce::Justification::centred);
    }

    // =========================================================================
    // 7. Delay Visualizer
    // =========================================================================
    EightBandEQComponent::DelayVisualizer::DelayVisualizer(EightBandEQComponent& o) : owner(o) {}

    void EightBandEQComponent::DelayVisualizer::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0a0d14));
        g.fillRect(bounds);

        auto* dsp = owner.getCurrentTargetDSP();
        float ms = dsp ? dsp->params.delayTimeMs.load() : 350.0f;
        float fb = dsp ? dsp->params.delayFeedback.load() : 0.35f;
        bool pingPong = dsp ? dsp->params.delayPingPong.load() : false;

        // Draw Echo Trail timeline
        float startX = 40.0f;
        float endX = bounds.getWidth() - 40.0f;
        float midY = bounds.getCentreY();

        g.setColour(juce::Colour(0xff1e293b));
        g.drawHorizontalLine((int)midY, startX, endX);

        // Initial Tap (Dry input)
        g.setColour(juce::Colour(0xffffffff));
        g.fillRect(startX, midY - 35.0f, 6.0f, 70.0f);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Dry 0ms", (int)startX - 15, (int)midY + 38, 50, 14, juce::Justification::centred);

        // Echo Repeats
        float currentAmp = 1.0f;
        float spacingPx = juce::jlimit(40.0f, (endX - startX) * 0.45f, (ms / 1500.0f) * (endX - startX - 40.0f));

        for (int tap = 1; tap <= 6; ++tap)
        {
            currentAmp *= fb;
            if (currentAmp < 0.02f) break;

            float tapX = startX + (float)tap * spacingPx;
            if (tapX > endX) break;

            float tapH = 70.0f * currentAmp;
            juce::Colour tapCol = (pingPong && (tap % 2 == 1)) ? juce::Colour(0xffec4899) : juce::Colour(0xff00f5d4);

            g.setColour(tapCol);
            g.fillRect(tapX, midY - tapH * 0.5f, 4.0f, tapH);

            g.setFont(juce::FontOptions(9.0f));
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText(juce::String((int)(ms * tap)) + "ms", (int)tapX - 20, (int)midY + 38, 44, 14, juce::Justification::centred);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff00f5d4));
        juce::String ppStr = pingPong ? " (Ping-Pong Stereo Active)" : " (Standard Stereo)";
        g.drawText("Delay Time: " + juce::String((int)ms) + " ms | Feedback: " + juce::String((int)(fb * 100.0f)) + "%" + ppStr,
                   (int)bounds.getX(), (int)bounds.getY() + 10, (int)bounds.getWidth(), 20, juce::Justification::centred);
    }

    // =========================================================================
    // EightBandEQComponent Implementation
    // =========================================================================
    EightBandEQComponent::EightBandEQComponent(
        std::array<BusDSPProcessor*, NumTargets> processors,
        PresetManager* presets)
        : targetDSPs(processors), presetManager(presets)
    {
        for (int t = 0; t < NumTargets; ++t)
        {
            if (targetDSPs[t] != nullptr)
                targetProcessors[t] = &targetDSPs[t]->eightBandEQ;
        }

        // Setup Look and Feels for Knobs
        hpfKnobLaf = std::make_unique<BandKnobLookAndFeel>();
        hpfKnobLaf->activeColor = juce::Colour(0xff00f5d4);

        gateKnobLaf = std::make_unique<BandKnobLookAndFeel>();
        gateKnobLaf->activeColor = juce::Colour(0xfff59e0b);

        compKnobLaf = std::make_unique<BandKnobLookAndFeel>();
        compKnobLaf->activeColor = juce::Colour(0xff3b82f6);

        limiterKnobLaf = std::make_unique<BandKnobLookAndFeel>();
        limiterKnobLaf->activeColor = juce::Colour(0xffef4444);

        reverbKnobLaf = std::make_unique<BandKnobLookAndFeel>();
        reverbKnobLaf->activeColor = juce::Colour(0xff10b981);

        delayKnobLaf = std::make_unique<BandKnobLookAndFeel>();
        delayKnobLaf->activeColor = juce::Colour(0xffec4899);

        // 1. Left Sidebar: DSP Processor list
        dspProcessorHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        dspProcessorHeader.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(dspProcessorHeader);

        eqNavBtn.addListener(this);
        addAndMakeVisible(eqNavBtn);
        hpfNavBtn.addListener(this);
        addAndMakeVisible(hpfNavBtn);
        gateNavBtn.addListener(this);
        addAndMakeVisible(gateNavBtn);
        compNavBtn.addListener(this);
        addAndMakeVisible(compNavBtn);
        limiterNavBtn.addListener(this);
        addAndMakeVisible(limiterNavBtn);
        reverbNavBtn.addListener(this);
        addAndMakeVisible(reverbNavBtn);
        delayNavBtn.addListener(this);
        addAndMakeVisible(delayNavBtn);

        // 2. Left Sidebar: Apply To Targets
        applyToHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        applyToHeader.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(applyToHeader);

        for (int t = 0; t < NumTargets; ++t)
        {
            targetButtons[t].setButtonText(getTargetName((TargetIndex)t));
            targetButtons[t].addListener(this);
            addAndMakeVisible(targetButtons[t]);
        }

        // Shared Module Header Bar
        modulePowerBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
        modulePowerBtn.addListener(this);
        addChildComponent(modulePowerBtn);

        moduleTitleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        moduleTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(moduleTitleLabel);

        moduleTargetBadge.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        moduleTargetBadge.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
        addChildComponent(moduleTargetBadge);

        moduleResetBtn.addListener(this);
        addChildComponent(moduleResetBtn);

        moduleCopyLabel.setText("Copy To", juce::dontSendNotification);
        addChildComponent(moduleCopyLabel);

        for (int t = 0; t < NumTargets; ++t)
            moduleCopyCombo.addItem(getTargetName((TargetIndex)t), t + 1);
        moduleCopyCombo.setSelectedId(1, juce::dontSendNotification);
        addChildComponent(moduleCopyCombo);

        moduleCopyBtn.addListener(this);
        addChildComponent(moduleCopyBtn);

        // =====================================================================
        // Initialize 1. Equalizer Controls
        // =====================================================================
        powerBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
        powerBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        powerBtn.addListener(this);
        addAndMakeVisible(powerBtn);

        titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        eqOnToggle.setToggleState(true, juce::dontSendNotification);
        eqOnToggle.onStateChange = [this]() {
            getCurrentProcessor().params.masterEnabled.store(eqOnToggle.getToggleState());
            updateUIFromProcessor();
            if (graphVisualizer) graphVisualizer->repaint();
        };
        addAndMakeVisible(eqOnToggle);

        analyzerToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
        analyzerToggleBtn.addListener(this);
        addAndMakeVisible(analyzerToggleBtn);

        preBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        preBtn.addListener(this);
        addAndMakeVisible(preBtn);

        postBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
        postBtn.addListener(this);
        addAndMakeVisible(postBtn);

        fftSizeLabel.setFont(juce::FontOptions(11.0f));
        fftSizeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(fftSizeLabel);

        graphVisualizer = std::make_unique<EQGraphVisualizer>(*this);
        addAndMakeVisible(graphVisualizer.get());

        // Setup 8 EQ Band Strips
        const char* eqTypes[] = { "High-Pass", "Low Shelf", "Bell", "High Shelf", "Low-Pass", "Notch" };
        for (int b = 0; b < numEQBands; ++b)
        {
            auto& strip = bandStrips[b];
            strip.knobLaf = std::make_unique<BandKnobLookAndFeel>();
            strip.knobLaf->activeColor = getBandColour(b);

            strip.bandPill.setText(juce::String(b + 1), juce::dontSendNotification);
            strip.bandPill.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            strip.bandPill.setColour(juce::Label::backgroundColourId, getBandColour(b));
            strip.bandPill.setColour(juce::Label::textColourId, juce::Colours::white);
            strip.bandPill.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(strip.bandPill);

            for (int i = 0; i < 6; ++i)
                strip.typeCombo.addItem(eqTypes[i], i + 1);
            strip.typeCombo.addListener(this);
            addAndMakeVisible(strip.typeCombo);

            setupKnob(strip.freqSlider, strip.freqValueLabel, strip.knobLaf.get(), 20.0, 20000.0, 1000.0, " Hz", 0.35);
            setupKnob(strip.gainSlider, strip.gainValueLabel, strip.knobLaf.get(), -24.0, 24.0, 0.0, " dB");
            setupKnob(strip.qSlider, strip.qValueLabel, strip.knobLaf.get(), 0.1, 10.0, 0.71, "");

            strip.bypassBtn.addListener(this);
            addAndMakeVisible(strip.bypassBtn);
        }

        presetLabel.setFont(juce::FontOptions(12.0f));
        presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(presetLabel);

        presetCombo.addListener(this);
        addAndMakeVisible(presetCombo);

        savePresetBtn.addListener(this);
        addAndMakeVisible(savePresetBtn);
        saveAsPresetBtn.addListener(this);
        addAndMakeVisible(saveAsPresetBtn);
        deletePresetBtn.addListener(this);
        addAndMakeVisible(deletePresetBtn);

        copyToLabel.setFont(juce::FontOptions(12.0f));
        copyToLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(copyToLabel);

        for (int t = 0; t < NumTargets; ++t)
            copyToCombo.addItem(getTargetName((TargetIndex)t), t + 1);
        copyToCombo.setSelectedId(2, juce::dontSendNotification);
        addAndMakeVisible(copyToCombo);

        copyBtn.addListener(this);
        addAndMakeVisible(copyBtn);

        resetEQBtn.addListener(this);
        addAndMakeVisible(resetEQBtn);

        // =====================================================================
        // Initialize 2. HPF Controls
        // =====================================================================
        hpfVisualizer = std::make_unique<HPFCurveVisualizer>(*this);
        addChildComponent(hpfVisualizer.get());
        setupKnob(hpfCutoffSlider, hpfCutoffLabel, hpfKnobLaf.get(), 20.0, 1000.0, 80.0, " Hz", 0.4);

        hpfSlopeCombo.addItem("6 dB / Oct", 1);
        hpfSlopeCombo.addItem("12 dB / Oct", 2);
        hpfSlopeCombo.addItem("18 dB / Oct", 3);
        hpfSlopeCombo.addItem("24 dB / Oct", 4);
        hpfSlopeCombo.setSelectedId(2, juce::dontSendNotification);
        hpfSlopeCombo.addListener(this);
        addChildComponent(hpfSlopeCombo);
        addChildComponent(hpfSlopeLabel);

        hpfTypeCombo.addItem("Butterworth", 1);
        hpfTypeCombo.addItem("Bessel", 2);
        hpfTypeCombo.addItem("Linkwitz-Riley", 3);
        hpfTypeCombo.setSelectedId(1, juce::dontSendNotification);
        hpfTypeCombo.addListener(this);
        addChildComponent(hpfTypeCombo);
        addChildComponent(hpfTypeLabel);

        // =====================================================================
        // Initialize 3. Noise Gate Controls
        // =====================================================================
        gateVisualizer = std::make_unique<GateVisualizer>(*this);
        addChildComponent(gateVisualizer.get());
        setupKnob(gateThreshSlider, gateThreshLabel, gateKnobLaf.get(), -80.0, 0.0, -45.0, " dB");
        setupKnob(gateAttackSlider, gateAttackLabel, gateKnobLaf.get(), 0.1, 100.0, 2.0, " ms", 0.35);
        setupKnob(gateHoldSlider, gateHoldLabel, gateKnobLaf.get(), 0.0, 500.0, 20.0, " ms", 0.4);
        setupKnob(gateReleaseSlider, gateReleaseLabel, gateKnobLaf.get(), 10.0, 1000.0, 100.0, " ms", 0.4);
        setupKnob(gateFloorSlider, gateFloorLabel, gateKnobLaf.get(), -80.0, 0.0, -60.0, " dB");
        setupKnob(gateHystSlider, gateHystLabel, gateKnobLaf.get(), 1.0, 12.0, 4.0, " dB");
        addChildComponent(gateStatusBadge);

        // =====================================================================
        // Initialize 4. Compressor Controls
        // =====================================================================
        compVisualizer = std::make_unique<CompCurveVisualizer>(*this);
        addChildComponent(compVisualizer.get());
        setupKnob(compThreshSlider, compThreshLabel, compKnobLaf.get(), -60.0, 0.0, -18.0, " dB");
        setupKnob(compRatioSlider, compRatioLabel, compKnobLaf.get(), 1.0, 20.0, 3.0, ":1", 0.5);
        setupKnob(compAttackSlider, compAttackLabel, compKnobLaf.get(), 0.1, 100.0, 10.0, " ms", 0.35);
        setupKnob(compReleaseSlider, compReleaseLabel, compKnobLaf.get(), 10.0, 1000.0, 100.0, " ms", 0.4);

        compKneeCombo.addItem("Soft Knee (~10 dB)", 1);
        compKneeCombo.addItem("Normal Knee (~5 dB)", 2);
        compKneeCombo.addItem("Hard Knee (0 dB)", 3);
        compKneeCombo.setSelectedId(2, juce::dontSendNotification);
        compKneeCombo.addListener(this);
        addChildComponent(compKneeCombo);
        addChildComponent(compKneeLabel);

        setupKnob(compMakeupSlider, compMakeupLabel, compKnobLaf.get(), -12.0, 24.0, 0.0, " dB");
        setupKnob(compMixSlider, compMixLabel, compKnobLaf.get(), 0.0, 100.0, 100.0, " %");

        // =====================================================================
        // Initialize 5. Limiter Controls
        // =====================================================================
        limiterVisualizer = std::make_unique<LimiterVisualizer>(*this);
        addChildComponent(limiterVisualizer.get());
        setupKnob(limiterCeilSlider, limiterCeilLabel, limiterKnobLaf.get(), -20.0, 0.0, -1.0, " dBFS");
        setupKnob(limiterReleaseSlider, limiterReleaseLabel, limiterKnobLaf.get(), 1.0, 500.0, 60.0, " ms", 0.4);
        addChildComponent(limiterClipLed);

        // =====================================================================
        // Initialize 6. Reverb Controls
        // =====================================================================
        reverbVisualizer = std::make_unique<ReverbVisualizer>(*this);
        addChildComponent(reverbVisualizer.get());
        setupKnob(reverbRoomSlider, reverbRoomLabel, reverbKnobLaf.get(), 0.0, 100.0, 50.0, " %");
        setupKnob(reverbDampSlider, reverbDampLabel, reverbKnobLaf.get(), 0.0, 100.0, 50.0, " %");
        setupKnob(reverbWidthSlider, reverbWidthLabel, reverbKnobLaf.get(), 0.0, 100.0, 100.0, " %");
        setupKnob(reverbWetSlider, reverbWetLabel, reverbKnobLaf.get(), 0.0, 100.0, 33.0, " %");
        setupKnob(reverbDrySlider, reverbDryLabel, reverbKnobLaf.get(), 0.0, 100.0, 100.0, " %");

        // =====================================================================
        // Initialize 7. Delay Controls
        // =====================================================================
        delayVisualizer = std::make_unique<DelayVisualizer>(*this);
        addChildComponent(delayVisualizer.get());
        setupKnob(delayTimeSlider, delayTimeLabel, delayKnobLaf.get(), 10.0, 1500.0, 350.0, " ms", 0.4);
        setupKnob(delayFeedbackSlider, delayFeedbackLabel, delayKnobLaf.get(), 0.0, 90.0, 35.0, " %");
        setupKnob(delayWetSlider, delayWetLabel, delayKnobLaf.get(), 0.0, 100.0, 25.0, " %");
        setupKnob(delayDrySlider, delayDryLabel, delayKnobLaf.get(), 0.0, 100.0, 100.0, " %");
        delayPingPongToggle.addListener(this);
        addChildComponent(delayPingPongToggle);

        loadFactoryPresets();
        setSelectedTarget(TargetStreamBus);
        setSelectedNavModule(NavEqualizer);

        startTimerHz(25); // 25 Hz UI refresh
    }

    EightBandEQComponent::EightBandEQComponent(std::array<EightBandEQProcessor*, NumTargets> processors)
        : targetProcessors(processors)
    {
        // Minimal setup for backward compatibility constructor
        for (int b = 0; b < numEQBands; ++b)
        {
            bandStrips[b].knobLaf = std::make_unique<BandKnobLookAndFeel>();
            bandStrips[b].knobLaf->activeColor = getBandColour(b);
        }
        graphVisualizer = std::make_unique<EQGraphVisualizer>(*this);
        addAndMakeVisible(graphVisualizer.get());
        loadFactoryPresets();
        startTimerHz(25);
    }

    EightBandEQComponent::~EightBandEQComponent()
    {
        stopTimer();
        for (auto& strip : bandStrips)
        {
            strip.freqSlider.setLookAndFeel(nullptr);
            strip.gainSlider.setLookAndFeel(nullptr);
            strip.qSlider.setLookAndFeel(nullptr);
        }
        hpfCutoffSlider.setLookAndFeel(nullptr);
        gateThreshSlider.setLookAndFeel(nullptr);
        gateAttackSlider.setLookAndFeel(nullptr);
        gateHoldSlider.setLookAndFeel(nullptr);
        gateReleaseSlider.setLookAndFeel(nullptr);
        gateFloorSlider.setLookAndFeel(nullptr);
        gateHystSlider.setLookAndFeel(nullptr);
        compThreshSlider.setLookAndFeel(nullptr);
        compRatioSlider.setLookAndFeel(nullptr);
        compAttackSlider.setLookAndFeel(nullptr);
        compReleaseSlider.setLookAndFeel(nullptr);
        compMakeupSlider.setLookAndFeel(nullptr);
        compMixSlider.setLookAndFeel(nullptr);
        limiterCeilSlider.setLookAndFeel(nullptr);
        limiterReleaseSlider.setLookAndFeel(nullptr);
        reverbRoomSlider.setLookAndFeel(nullptr);
        reverbDampSlider.setLookAndFeel(nullptr);
        reverbWidthSlider.setLookAndFeel(nullptr);
        reverbWetSlider.setLookAndFeel(nullptr);
        reverbDrySlider.setLookAndFeel(nullptr);
        delayTimeSlider.setLookAndFeel(nullptr);
        delayFeedbackSlider.setLookAndFeel(nullptr);
        delayWetSlider.setLookAndFeel(nullptr);
        delayDrySlider.setLookAndFeel(nullptr);
    }

    EightBandEQProcessor& EightBandEQComponent::getCurrentProcessor()
    {
        if (targetProcessors[(size_t)currentTarget] != nullptr)
            return *targetProcessors[(size_t)currentTarget];
        static EightBandEQProcessor fallback;
        return fallback;
    }

    const EightBandEQProcessor& EightBandEQComponent::getCurrentProcessor() const
    {
        if (targetProcessors[(size_t)currentTarget] != nullptr)
            return *targetProcessors[(size_t)currentTarget];
        static EightBandEQProcessor fallback;
        return fallback;
    }

    BusDSPProcessor* EightBandEQComponent::getCurrentTargetDSP()
    {
        return targetDSPs[(size_t)currentTarget];
    }

    const BusDSPProcessor* EightBandEQComponent::getCurrentTargetDSP() const
    {
        return targetDSPs[(size_t)currentTarget];
    }

    void EightBandEQComponent::setupKnob(juce::Slider& slider, juce::Label& label, BandKnobLookAndFeel* laf,
                                         double min, double max, double def, const juce::String& suffix, double skew)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(laf);
        slider.setRange(min, max, (max - min) > 100.0 ? 1.0 : 0.1);
        if (skew != 1.0) slider.setSkewFactor(skew);
        slider.setValue(def, juce::dontSendNotification);
        slider.addListener(this);
        addChildComponent(slider);

        label.setFont(juce::FontOptions(11.0f));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
        label.setJustificationType(juce::Justification::centred);
        addChildComponent(label);
    }

    void EightBandEQComponent::setSelectedNavModule(DSPNavModule module)
    {
        currentNavModule = module;
        if (module == NavEqualizer)
            refreshEQCatalogPresets();

        juce::TextButton* navBtns[] = {
            &eqNavBtn, &hpfNavBtn, &gateNavBtn, &compNavBtn, &limiterNavBtn, &reverbNavBtn, &delayNavBtn
        };

        for (int m = 0; m < NumNavModules; ++m)
        {
            if (m == (int)currentNavModule)
            {
                navBtns[m]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
                navBtns[m]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            }
            else
            {
                navBtns[m]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161f2e));
                navBtns[m]->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
            }
        }

        updateModuleVisibility();
        updateUIFromProcessor();
        resized();
        repaint();
    }

    void EightBandEQComponent::setSelectedTarget(TargetIndex target)
    {
        currentTarget = target;

        for (int t = 0; t < NumTargets; ++t)
        {
            if (t == currentTarget)
            {
                targetButtons[t].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
                targetButtons[t].setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            }
            else
            {
                targetButtons[t].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161f2e));
                targetButtons[t].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
            }
        }

        moduleTargetBadge.setText("[" + juce::String(getTargetName(currentTarget)).toUpperCase() + "]", juce::dontSendNotification);

        updateUIFromProcessor();
        if (graphVisualizer) graphVisualizer->repaint();
        if (hpfVisualizer) hpfVisualizer->repaint();
        if (gateVisualizer) gateVisualizer->repaint();
        if (compVisualizer) compVisualizer->repaint();
        if (limiterVisualizer) limiterVisualizer->repaint();
        if (reverbVisualizer) reverbVisualizer->repaint();
        if (delayVisualizer) delayVisualizer->repaint();
    }

    void EightBandEQComponent::updateModuleVisibility()
    {
        const bool isEQ = (currentNavModule == NavEqualizer);
        const bool isHPF = (currentNavModule == NavHPF);
        const bool isGate = (currentNavModule == NavGate);
        const bool isComp = (currentNavModule == NavCompressor);
        const bool isLimiter = (currentNavModule == NavLimiter);
        const bool isReverb = (currentNavModule == NavReverb);
        const bool isDelay = (currentNavModule == NavDelay);

        // Header for non-EQ modules
        modulePowerBtn.setVisible(!isEQ);
        moduleTitleLabel.setVisible(!isEQ);
        moduleTargetBadge.setVisible(!isEQ);
        moduleResetBtn.setVisible(!isEQ);
        moduleCopyLabel.setVisible(!isEQ);
        moduleCopyCombo.setVisible(!isEQ);
        moduleCopyBtn.setVisible(!isEQ);

        // 1. Equalizer visibility
        powerBtn.setVisible(isEQ);
        titleLabel.setVisible(isEQ);
        eqOnToggle.setVisible(isEQ);
        analyzerToggleBtn.setVisible(isEQ);
        preBtn.setVisible(isEQ);
        postBtn.setVisible(isEQ);
        fftSizeLabel.setVisible(isEQ);
        if (graphVisualizer) graphVisualizer->setVisible(isEQ);
        for (auto& s : bandStrips)
        {
            s.bandPill.setVisible(isEQ);
            s.typeCombo.setVisible(isEQ);
            s.freqSlider.setVisible(isEQ);
            s.freqValueLabel.setVisible(isEQ);
            s.gainSlider.setVisible(isEQ);
            s.gainValueLabel.setVisible(isEQ);
            s.qSlider.setVisible(isEQ);
            s.qValueLabel.setVisible(isEQ);
            s.bypassBtn.setVisible(isEQ);
        }
        presetLabel.setVisible(isEQ);
        presetCombo.setVisible(isEQ);
        savePresetBtn.setVisible(isEQ);
        saveAsPresetBtn.setVisible(isEQ);
        deletePresetBtn.setVisible(isEQ);
        copyToLabel.setVisible(isEQ);
        copyToCombo.setVisible(isEQ);
        copyBtn.setVisible(isEQ);
        resetEQBtn.setVisible(isEQ);

        // 2. HPF visibility
        if (hpfVisualizer) hpfVisualizer->setVisible(isHPF);
        hpfCutoffSlider.setVisible(isHPF);
        hpfCutoffLabel.setVisible(isHPF);
        hpfSlopeCombo.setVisible(isHPF);
        hpfSlopeLabel.setVisible(isHPF);
        hpfTypeCombo.setVisible(isHPF);
        hpfTypeLabel.setVisible(isHPF);

        // 3. Gate visibility
        if (gateVisualizer) gateVisualizer->setVisible(isGate);
        gateThreshSlider.setVisible(isGate);
        gateThreshLabel.setVisible(isGate);
        gateAttackSlider.setVisible(isGate);
        gateAttackLabel.setVisible(isGate);
        gateHoldSlider.setVisible(isGate);
        gateHoldLabel.setVisible(isGate);
        gateReleaseSlider.setVisible(isGate);
        gateReleaseLabel.setVisible(isGate);
        gateFloorSlider.setVisible(isGate);
        gateFloorLabel.setVisible(isGate);
        gateHystSlider.setVisible(isGate);
        gateHystLabel.setVisible(isGate);
        gateStatusBadge.setVisible(isGate);

        // 4. Compressor visibility
        if (compVisualizer) compVisualizer->setVisible(isComp);
        compThreshSlider.setVisible(isComp);
        compThreshLabel.setVisible(isComp);
        compRatioSlider.setVisible(isComp);
        compRatioLabel.setVisible(isComp);
        compAttackSlider.setVisible(isComp);
        compAttackLabel.setVisible(isComp);
        compReleaseSlider.setVisible(isComp);
        compReleaseLabel.setVisible(isComp);
        compKneeCombo.setVisible(isComp);
        compKneeLabel.setVisible(isComp);
        compMakeupSlider.setVisible(isComp);
        compMakeupLabel.setVisible(isComp);
        compMixSlider.setVisible(isComp);
        compMixLabel.setVisible(isComp);

        // 5. Limiter visibility
        if (limiterVisualizer) limiterVisualizer->setVisible(isLimiter);
        limiterCeilSlider.setVisible(isLimiter);
        limiterCeilLabel.setVisible(isLimiter);
        limiterReleaseSlider.setVisible(isLimiter);
        limiterReleaseLabel.setVisible(isLimiter);
        limiterClipLed.setVisible(isLimiter);

        // 6. Reverb visibility
        if (reverbVisualizer) reverbVisualizer->setVisible(isReverb);
        reverbRoomSlider.setVisible(isReverb);
        reverbRoomLabel.setVisible(isReverb);
        reverbDampSlider.setVisible(isReverb);
        reverbDampLabel.setVisible(isReverb);
        reverbWidthSlider.setVisible(isReverb);
        reverbWidthLabel.setVisible(isReverb);
        reverbWetSlider.setVisible(isReverb);
        reverbWetLabel.setVisible(isReverb);
        reverbDrySlider.setVisible(isReverb);
        reverbDryLabel.setVisible(isReverb);

        // 7. Delay visibility
        if (delayVisualizer) delayVisualizer->setVisible(isDelay);
        delayTimeSlider.setVisible(isDelay);
        delayTimeLabel.setVisible(isDelay);
        delayFeedbackSlider.setVisible(isDelay);
        delayFeedbackLabel.setVisible(isDelay);
        delayWetSlider.setVisible(isDelay);
        delayWetLabel.setVisible(isDelay);
        delayDrySlider.setVisible(isDelay);
        delayDryLabel.setVisible(isDelay);
        delayPingPongToggle.setVisible(isDelay);

        // Set Module Titles
        switch (currentNavModule)
        {
            case NavHPF:        moduleTitleLabel.setText("High-Pass Filter (Low-Cut)", juce::dontSendNotification); break;
            case NavGate:       moduleTitleLabel.setText("Studio Noise Gate / Expander", juce::dontSendNotification); break;
            case NavCompressor: moduleTitleLabel.setText("Studio Dynamics Compressor", juce::dontSendNotification); break;
            case NavLimiter:    moduleTitleLabel.setText("Broadcast Peak Limiter", juce::dontSendNotification); break;
            case NavReverb:     moduleTitleLabel.setText("Studio Acoustic Reverb", juce::dontSendNotification); break;
            case NavDelay:      moduleTitleLabel.setText("Stereo Echo & Delay", juce::dontSendNotification); break;
            default:            break;
        }
    }

    void EightBandEQComponent::updateUIFromProcessor()
    {
        isUpdatingUI = true;
        const auto& proc = getCurrentProcessor();
        auto* dsp = getCurrentTargetDSP();

        // 1. Equalizer UI Update
        const bool masterEn = proc.params.masterEnabled.load(std::memory_order_relaxed);
        eqOnToggle.setToggleState(masterEn, juce::dontSendNotification);
        powerBtn.setColour(juce::TextButton::buttonColourId, masterEn ? juce::Colour(0xff10b981) : juce::Colour(0xff475569));
        powerBtn.setButtonText(masterEn ? "ON" : "OFF");

        for (int b = 0; b < numEQBands; ++b)
        {
            auto& strip = bandStrips[b];
            const auto type = proc.params.bands[b].type.load(std::memory_order_relaxed);
            const float f = proc.params.bands[b].freq.load(std::memory_order_relaxed);
            const float g = proc.params.bands[b].gainDb.load(std::memory_order_relaxed);
            const float qVal = proc.params.bands[b].q.load(std::memory_order_relaxed);
            const bool en = proc.params.bands[b].enabled.load(std::memory_order_relaxed);

            strip.typeCombo.setSelectedId((int)type + 1, juce::dontSendNotification);
            strip.freqSlider.setValue(f, juce::dontSendNotification);
            strip.freqValueLabel.setText((f >= 1000.0f ? juce::String(f * 0.001f, 1) + " kHz" : juce::String((int)f) + " Hz"), juce::dontSendNotification);

            const bool hasGain = (type != EQFilterType::HighPass && type != EQFilterType::LowPass);
            strip.gainSlider.setEnabled(hasGain);
            strip.gainSlider.setValue(g, juce::dontSendNotification);
            strip.gainValueLabel.setText((hasGain ? (g >= 0.0f ? "+" : "") + juce::String(g, 1) + " dB" : "0.0 dB"), juce::dontSendNotification);

            strip.qSlider.setValue(qVal, juce::dontSendNotification);
            strip.qValueLabel.setText(juce::String(qVal, 2), juce::dontSendNotification);

            strip.bypassBtn.setButtonText(en ? "ON" : "OFF");
            strip.bypassBtn.setColour(juce::TextButton::buttonColourId, en ? getBandColour(b) : juce::Colour(0xff334155));
        }

        // 2..7 Other DSP Modules Update
        if (dsp != nullptr)
        {
            bool modEn = true;
            switch (currentNavModule)
            {
                case NavHPF:
                    modEn = dsp->params.hpfEnabled.load();
                    hpfCutoffSlider.setValue(dsp->params.hpfFreq.load(), juce::dontSendNotification);
                    hpfCutoffLabel.setText("Cutoff: " + juce::String((int)dsp->params.hpfFreq.load()) + " Hz", juce::dontSendNotification);
                    hpfSlopeCombo.setSelectedId(dsp->params.hpfSlope.load() + 1, juce::dontSendNotification);
                    hpfTypeCombo.setSelectedId(dsp->params.hpfType.load() + 1, juce::dontSendNotification);
                    break;

                case NavGate:
                    modEn = dsp->params.gateEnabled.load();
                    gateThreshSlider.setValue(dsp->params.gateThresholdDb.load(), juce::dontSendNotification);
                    gateThreshLabel.setText("Thresh: " + juce::String(dsp->params.gateThresholdDb.load(), 1) + " dB", juce::dontSendNotification);
                    gateAttackSlider.setValue(dsp->params.gateAttackMs.load(), juce::dontSendNotification);
                    gateAttackLabel.setText("Attack: " + juce::String(dsp->params.gateAttackMs.load(), 1) + " ms", juce::dontSendNotification);
                    gateHoldSlider.setValue(dsp->params.gateHoldMs.load(), juce::dontSendNotification);
                    gateHoldLabel.setText("Hold: " + juce::String((int)dsp->params.gateHoldMs.load()) + " ms", juce::dontSendNotification);
                    gateReleaseSlider.setValue(dsp->params.gateReleaseMs.load(), juce::dontSendNotification);
                    gateReleaseLabel.setText("Release: " + juce::String((int)dsp->params.gateReleaseMs.load()) + " ms", juce::dontSendNotification);
                    gateFloorSlider.setValue(dsp->params.gateFloorDb.load(), juce::dontSendNotification);
                    gateFloorLabel.setText("Floor: " + juce::String((int)dsp->params.gateFloorDb.load()) + " dB", juce::dontSendNotification);
                    gateHystSlider.setValue(dsp->params.gateHysteresisDb.load(), juce::dontSendNotification);
                    gateHystLabel.setText("Hysteresis: " + juce::String(dsp->params.gateHysteresisDb.load(), 1) + " dB", juce::dontSendNotification);
                    break;

                case NavCompressor:
                    modEn = dsp->params.compEnabled.load();
                    compThreshSlider.setValue(dsp->params.compThresholdDb.load(), juce::dontSendNotification);
                    compThreshLabel.setText("Thresh: " + juce::String(dsp->params.compThresholdDb.load(), 1) + " dB", juce::dontSendNotification);
                    compRatioSlider.setValue(dsp->params.compRatio.load(), juce::dontSendNotification);
                    compRatioLabel.setText("Ratio: " + juce::String(dsp->params.compRatio.load(), 1) + ":1", juce::dontSendNotification);
                    compAttackSlider.setValue(dsp->params.compAttackMs.load(), juce::dontSendNotification);
                    compAttackLabel.setText("Attack: " + juce::String(dsp->params.compAttackMs.load(), 1) + " ms", juce::dontSendNotification);
                    compReleaseSlider.setValue(dsp->params.compReleaseMs.load(), juce::dontSendNotification);
                    compReleaseLabel.setText("Release: " + juce::String((int)dsp->params.compReleaseMs.load()) + " ms", juce::dontSendNotification);
                    compKneeCombo.setSelectedId(dsp->params.compKnee.load() + 1, juce::dontSendNotification);
                    compMakeupSlider.setValue(dsp->params.compMakeupDb.load(), juce::dontSendNotification);
                    compMakeupLabel.setText(juce::String("Makeup: ") + (dsp->params.compMakeupDb.load() >= 0 ? "+" : "") + juce::String(dsp->params.compMakeupDb.load(), 1) + " dB", juce::dontSendNotification);
                    compMixSlider.setValue(dsp->params.compMixPercent.load(), juce::dontSendNotification);
                    compMixLabel.setText("Mix: " + juce::String((int)dsp->params.compMixPercent.load()) + "%", juce::dontSendNotification);
                    break;

                case NavLimiter:
                    modEn = dsp->params.limiterEnabled.load();
                    limiterCeilSlider.setValue(dsp->params.limiterCeilingDb.load(), juce::dontSendNotification);
                    limiterCeilLabel.setText("Ceiling: " + juce::String(dsp->params.limiterCeilingDb.load(), 1) + " dBFS", juce::dontSendNotification);
                    limiterReleaseSlider.setValue(dsp->params.limiterReleaseMs.load(), juce::dontSendNotification);
                    limiterReleaseLabel.setText("Release: " + juce::String((int)dsp->params.limiterReleaseMs.load()) + " ms", juce::dontSendNotification);
                    break;

                case NavReverb:
                    modEn = dsp->params.reverbEnabled.load();
                    reverbRoomSlider.setValue(dsp->params.reverbRoomSize.load() * 100.0, juce::dontSendNotification);
                    reverbRoomLabel.setText("Room: " + juce::String((int)(dsp->params.reverbRoomSize.load() * 100.0f)) + "%", juce::dontSendNotification);
                    reverbDampSlider.setValue(dsp->params.reverbDamping.load() * 100.0, juce::dontSendNotification);
                    reverbDampLabel.setText("Damping: " + juce::String((int)(dsp->params.reverbDamping.load() * 100.0f)) + "%", juce::dontSendNotification);
                    reverbWidthSlider.setValue(dsp->params.reverbWidth.load() * 100.0, juce::dontSendNotification);
                    reverbWidthLabel.setText("Width: " + juce::String((int)(dsp->params.reverbWidth.load() * 100.0f)) + "%", juce::dontSendNotification);
                    reverbWetSlider.setValue(dsp->params.reverbWetLevel.load() * 100.0, juce::dontSendNotification);
                    reverbWetLabel.setText("Wet: " + juce::String((int)(dsp->params.reverbWetLevel.load() * 100.0f)) + "%", juce::dontSendNotification);
                    reverbDrySlider.setValue(dsp->params.reverbDryLevel.load() * 100.0, juce::dontSendNotification);
                    reverbDryLabel.setText("Dry: " + juce::String((int)(dsp->params.reverbDryLevel.load() * 100.0f)) + "%", juce::dontSendNotification);
                    break;

                case NavDelay:
                    modEn = dsp->params.delayEnabled.load();
                    delayTimeSlider.setValue(dsp->params.delayTimeMs.load(), juce::dontSendNotification);
                    delayTimeLabel.setText("Time: " + juce::String((int)dsp->params.delayTimeMs.load()) + " ms", juce::dontSendNotification);
                    delayFeedbackSlider.setValue(dsp->params.delayFeedback.load() * 100.0, juce::dontSendNotification);
                    delayFeedbackLabel.setText("Feedback: " + juce::String((int)(dsp->params.delayFeedback.load() * 100.0f)) + "%", juce::dontSendNotification);
                    delayWetSlider.setValue(dsp->params.delayWetLevel.load() * 100.0, juce::dontSendNotification);
                    delayWetLabel.setText("Wet: " + juce::String((int)(dsp->params.delayWetLevel.load() * 100.0f)) + "%", juce::dontSendNotification);
                    delayDrySlider.setValue(dsp->params.delayDryLevel.load() * 100.0, juce::dontSendNotification);
                    delayDryLabel.setText("Dry: " + juce::String((int)(dsp->params.delayDryLevel.load() * 100.0f)) + "%", juce::dontSendNotification);
                    delayPingPongToggle.setToggleState(dsp->params.delayPingPong.load(), juce::dontSendNotification);
                    break;

                default:
                    break;
            }

            modulePowerBtn.setButtonText(modEn ? "ON" : "BYPASS");
            modulePowerBtn.setColour(juce::TextButton::buttonColourId, modEn ? juce::Colour(0xff10b981) : juce::Colour(0xff475569));
        }

        isUpdatingUI = false;
    }

    void EightBandEQComponent::sliderValueChanged(juce::Slider* slider)
    {
        if (isUpdatingUI) return;

        auto& proc = getCurrentProcessor();
        auto* dsp = getCurrentTargetDSP();

        // 1. Equalizer Sliders
        for (int b = 0; b < numEQBands; ++b)
        {
            auto& strip = bandStrips[b];
            if (slider == &strip.freqSlider)
            {
                const float f = (float)strip.freqSlider.getValue();
                proc.params.bands[b].freq.store(f);
                strip.freqValueLabel.setText((f >= 1000.0f ? juce::String(f * 0.001f, 1) + " kHz" : juce::String((int)f) + " Hz"), juce::dontSendNotification);
                proc.updateCoefficients();
                if (graphVisualizer) graphVisualizer->repaint();
                return;
            }
            if (slider == &strip.gainSlider)
            {
                const float g = (float)strip.gainSlider.getValue();
                proc.params.bands[b].gainDb.store(g);
                strip.gainValueLabel.setText((g >= 0.0f ? "+" : "") + juce::String(g, 1) + " dB", juce::dontSendNotification);
                proc.updateCoefficients();
                if (graphVisualizer) graphVisualizer->repaint();
                return;
            }
            if (slider == &strip.qSlider)
            {
                const float qVal = (float)strip.qSlider.getValue();
                proc.params.bands[b].q.store(qVal);
                strip.qValueLabel.setText(juce::String(qVal, 2), juce::dontSendNotification);
                proc.updateCoefficients();
                if (graphVisualizer) graphVisualizer->repaint();
                return;
            }
        }

        if (dsp == nullptr) return;

        // 2. HPF Sliders
        if (slider == &hpfCutoffSlider)
        {
            float f = (float)hpfCutoffSlider.getValue();
            dsp->params.hpfFreq.store(f);
            hpfCutoffLabel.setText("Cutoff: " + juce::String((int)f) + " Hz", juce::dontSendNotification);
            if (hpfVisualizer) hpfVisualizer->repaint();
            return;
        }

        // 3. Gate Sliders
        if (slider == &gateThreshSlider)
        {
            float v = (float)gateThreshSlider.getValue();
            dsp->params.gateThresholdDb.store(v);
            gateThreshLabel.setText("Thresh: " + juce::String(v, 1) + " dB", juce::dontSendNotification);
            return;
        }
        if (slider == &gateAttackSlider)
        {
            float v = (float)gateAttackSlider.getValue();
            dsp->params.gateAttackMs.store(v);
            gateAttackLabel.setText("Attack: " + juce::String(v, 1) + " ms", juce::dontSendNotification);
            return;
        }
        if (slider == &gateHoldSlider)
        {
            float v = (float)gateHoldSlider.getValue();
            dsp->params.gateHoldMs.store(v);
            gateHoldLabel.setText("Hold: " + juce::String((int)v) + " ms", juce::dontSendNotification);
            return;
        }
        if (slider == &gateReleaseSlider)
        {
            float v = (float)gateReleaseSlider.getValue();
            dsp->params.gateReleaseMs.store(v);
            gateReleaseLabel.setText("Release: " + juce::String((int)v) + " ms", juce::dontSendNotification);
            return;
        }
        if (slider == &gateFloorSlider)
        {
            float v = (float)gateFloorSlider.getValue();
            dsp->params.gateFloorDb.store(v);
            gateFloorLabel.setText("Floor: " + juce::String((int)v) + " dB", juce::dontSendNotification);
            return;
        }
        if (slider == &gateHystSlider)
        {
            float v = (float)gateHystSlider.getValue();
            dsp->params.gateHysteresisDb.store(v);
            gateHystLabel.setText("Hysteresis: " + juce::String(v, 1) + " dB", juce::dontSendNotification);
            return;
        }

        // 4. Compressor Sliders
        if (slider == &compThreshSlider)
        {
            float v = (float)compThreshSlider.getValue();
            dsp->params.compThresholdDb.store(v);
            compThreshLabel.setText("Thresh: " + juce::String(v, 1) + " dB", juce::dontSendNotification);
            if (compVisualizer) compVisualizer->repaint();
            return;
        }
        if (slider == &compRatioSlider)
        {
            float v = (float)compRatioSlider.getValue();
            dsp->params.compRatio.store(v);
            compRatioLabel.setText("Ratio: " + juce::String(v, 1) + ":1", juce::dontSendNotification);
            if (compVisualizer) compVisualizer->repaint();
            return;
        }
        if (slider == &compAttackSlider)
        {
            float v = (float)compAttackSlider.getValue();
            dsp->params.compAttackMs.store(v);
            compAttackLabel.setText("Attack: " + juce::String(v, 1) + " ms", juce::dontSendNotification);
            return;
        }
        if (slider == &compReleaseSlider)
        {
            float v = (float)compReleaseSlider.getValue();
            dsp->params.compReleaseMs.store(v);
            compReleaseLabel.setText("Release: " + juce::String((int)v) + " ms", juce::dontSendNotification);
            return;
        }
        if (slider == &compMakeupSlider)
        {
            float v = (float)compMakeupSlider.getValue();
            dsp->params.compMakeupDb.store(v);
            compMakeupLabel.setText(juce::String("Makeup: ") + (v >= 0 ? "+" : "") + juce::String(v, 1) + " dB", juce::dontSendNotification);
            return;
        }
        if (slider == &compMixSlider)
        {
            float v = (float)compMixSlider.getValue();
            dsp->params.compMixPercent.store(v);
            compMixLabel.setText("Mix: " + juce::String((int)v) + "%", juce::dontSendNotification);
            return;
        }

        // 5. Limiter Sliders
        if (slider == &limiterCeilSlider)
        {
            float v = (float)limiterCeilSlider.getValue();
            dsp->params.limiterCeilingDb.store(v);
            limiterCeilLabel.setText("Ceiling: " + juce::String(v, 1) + " dBFS", juce::dontSendNotification);
            if (limiterVisualizer) limiterVisualizer->repaint();
            return;
        }
        if (slider == &limiterReleaseSlider)
        {
            float v = (float)limiterReleaseSlider.getValue();
            dsp->params.limiterReleaseMs.store(v);
            limiterReleaseLabel.setText("Release: " + juce::String((int)v) + " ms", juce::dontSendNotification);
            return;
        }

        // 6. Reverb Sliders
        if (slider == &reverbRoomSlider)
        {
            float v = (float)reverbRoomSlider.getValue() * 0.01f;
            dsp->params.reverbRoomSize.store(v);
            reverbRoomLabel.setText("Room: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            if (reverbVisualizer) reverbVisualizer->repaint();
            return;
        }
        if (slider == &reverbDampSlider)
        {
            float v = (float)reverbDampSlider.getValue() * 0.01f;
            dsp->params.reverbDamping.store(v);
            reverbDampLabel.setText("Damping: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            if (reverbVisualizer) reverbVisualizer->repaint();
            return;
        }
        if (slider == &reverbWidthSlider)
        {
            float v = (float)reverbWidthSlider.getValue() * 0.01f;
            dsp->params.reverbWidth.store(v);
            reverbWidthLabel.setText("Width: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            return;
        }
        if (slider == &reverbWetSlider)
        {
            float v = (float)reverbWetSlider.getValue() * 0.01f;
            dsp->params.reverbWetLevel.store(v);
            reverbWetLabel.setText("Wet: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            if (reverbVisualizer) reverbVisualizer->repaint();
            return;
        }
        if (slider == &reverbDrySlider)
        {
            float v = (float)reverbDrySlider.getValue() * 0.01f;
            dsp->params.reverbDryLevel.store(v);
            reverbDryLabel.setText("Dry: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            if (reverbVisualizer) reverbVisualizer->repaint();
            return;
        }

        // 7. Delay Sliders
        if (slider == &delayTimeSlider)
        {
            float v = (float)delayTimeSlider.getValue();
            dsp->params.delayTimeMs.store(v);
            delayTimeLabel.setText("Time: " + juce::String((int)v) + " ms", juce::dontSendNotification);
            if (delayVisualizer) delayVisualizer->repaint();
            return;
        }
        if (slider == &delayFeedbackSlider)
        {
            float v = (float)delayFeedbackSlider.getValue() * 0.01f;
            dsp->params.delayFeedback.store(v);
            delayFeedbackLabel.setText("Feedback: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            if (delayVisualizer) delayVisualizer->repaint();
            return;
        }
        if (slider == &delayWetSlider)
        {
            float v = (float)delayWetSlider.getValue() * 0.01f;
            dsp->params.delayWetLevel.store(v);
            delayWetLabel.setText("Wet: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            return;
        }
        if (slider == &delayDrySlider)
        {
            float v = (float)delayDrySlider.getValue() * 0.01f;
            dsp->params.delayDryLevel.store(v);
            delayDryLabel.setText("Dry: " + juce::String((int)(v * 100.0f)) + "%", juce::dontSendNotification);
            return;
        }
    }

    void EightBandEQComponent::comboBoxChanged(juce::ComboBox* combo)
    {
        if (isUpdatingUI) return;

        auto& proc = getCurrentProcessor();
        auto* dsp = getCurrentTargetDSP();

        for (int b = 0; b < numEQBands; ++b)
        {
            if (combo == &bandStrips[b].typeCombo)
            {
                const auto type = (EQFilterType)(combo->getSelectedId() - 1);
                proc.params.bands[b].type.store(type);
                proc.updateCoefficients();
                updateUIFromProcessor();
                if (graphVisualizer) graphVisualizer->repaint();
                return;
            }
        }

        if (combo == &presetCombo)
        {
            const int selectedId = presetCombo.getSelectedId();
            if (selectedId >= 1000)
            {
                const int catalogIndex = selectedId - 1000;
                if (presetManager == nullptr ||
                    catalogIndex < 0 ||
                    catalogIndex >= (int)catalogPresetItems.size())
                    return;

                const auto& item =
                    catalogPresetItems[(size_t)catalogIndex];
                EQPresetDefinition definition;
                juce::String error;
                if (!presetManager->loadEQCatalogPreset(item, definition, error))
                {
                    juce::AlertWindow::showMessageBox(
                        juce::AlertWindow::WarningIcon, "EQ Preset Error",
                        "Could not load EQ preset '" + item.name + "': " + error,
                        "OK");
                    presetCombo.setSelectedId(1, juce::dontSendNotification);
                    return;
                }

                SceneData::EightBandEQSceneData mappedState;
                if (!EQPresetCatalog::mapToEightBand(
                        definition, mappedState, error))
                {
                    juce::AlertWindow::showMessageBox(
                        juce::AlertWindow::WarningIcon, "EQ Preset Error",
                        "Could not map EQ preset '" + item.name + "': " + error,
                        "OK");
                    presetCombo.setSelectedId(1, juce::dontSendNotification);
                    return;
                }

                EightBandEQParameters mappedParameters;
                mappedParameters.masterEnabled.store(mappedState.masterEnabled);
                for (int band = 0; band < numEQBands; ++band)
                {
                    const auto& source = mappedState.bands[(size_t)band];
                    auto& target = mappedParameters.bands[(size_t)band];
                    target.type.store((EQFilterType)source.type);
                    target.freq.store(source.freq);
                    target.gainDb.store(source.gainDb);
                    target.q.store(source.q);
                    target.enabled.store(source.enabled);
                }
                applyPreset(mappedParameters);
                return;
            }

            const int idx = selectedId - 1;
            if (idx >= 0 && idx < (int)userPresets.size())
            {
                applyPreset(userPresets[idx].params);
            }
            return;
        }

        if (dsp == nullptr) return;

        if (combo == &hpfSlopeCombo)
        {
            dsp->params.hpfSlope.store(hpfSlopeCombo.getSelectedId() - 1);
            if (hpfVisualizer) hpfVisualizer->repaint();
            return;
        }

        if (combo == &hpfTypeCombo)
        {
            dsp->params.hpfType.store(hpfTypeCombo.getSelectedId() - 1);
            return;
        }

        if (combo == &compKneeCombo)
        {
            dsp->params.compKnee.store(compKneeCombo.getSelectedId() - 1);
            if (compVisualizer) compVisualizer->repaint();
            return;
        }
    }

    void EightBandEQComponent::buttonClicked(juce::Button* button)
    {
        // 1. Sidebar Navigation Buttons
        if (button == &eqNavBtn)      { setSelectedNavModule(NavEqualizer); return; }
        if (button == &hpfNavBtn)     { setSelectedNavModule(NavHPF); return; }
        if (button == &gateNavBtn)    { setSelectedNavModule(NavGate); return; }
        if (button == &compNavBtn)    { setSelectedNavModule(NavCompressor); return; }
        if (button == &limiterNavBtn) { setSelectedNavModule(NavLimiter); return; }
        if (button == &reverbNavBtn)  { setSelectedNavModule(NavReverb); return; }
        if (button == &delayNavBtn)   { setSelectedNavModule(NavDelay); return; }

        // 2. Target switching
        for (int t = 0; t < NumTargets; ++t)
        {
            if (button == &targetButtons[t])
            {
                setSelectedTarget((TargetIndex)t);
                if (onTargetSelected)
                    onTargetSelected(currentTarget);
                return;
            }
        }

        auto* dsp = getCurrentTargetDSP();

        // 3. Module Power Button
        if (button == &modulePowerBtn && dsp != nullptr)
        {
            bool cur = true;
            switch (currentNavModule)
            {
                case NavHPF:
                    cur = !dsp->params.hpfEnabled.load();
                    dsp->params.hpfEnabled.store(cur);
                    break;
                case NavGate:
                    cur = !dsp->params.gateEnabled.load();
                    dsp->params.gateEnabled.store(cur);
                    break;
                case NavCompressor:
                    cur = !dsp->params.compEnabled.load();
                    dsp->params.compEnabled.store(cur);
                    break;
                case NavLimiter:
                    cur = !dsp->params.limiterEnabled.load();
                    dsp->params.limiterEnabled.store(cur);
                    break;
                case NavReverb:
                    cur = !dsp->params.reverbEnabled.load();
                    dsp->params.reverbEnabled.store(cur);
                    break;
                case NavDelay:
                    cur = !dsp->params.delayEnabled.load();
                    dsp->params.delayEnabled.store(cur);
                    break;
                default:
                    break;
            }
            updateUIFromProcessor();
            return;
        }

        // 4. Module Reset Button
        if (button == &moduleResetBtn && dsp != nullptr)
        {
            switch (currentNavModule)
            {
                case NavHPF:
                    dsp->params.hpfEnabled.store(true);
                    dsp->params.hpfFreq.store(80.0f);
                    dsp->params.hpfSlope.store(1);
                    break;
                case NavGate:
                    dsp->params.gateEnabled.store(false);
                    dsp->params.gateThresholdDb.store(-45.0f);
                    dsp->params.gateAttackMs.store(2.0f);
                    dsp->params.gateHoldMs.store(20.0f);
                    dsp->params.gateReleaseMs.store(100.0f);
                    dsp->params.gateFloorDb.store(-60.0f);
                    dsp->params.gateHysteresisDb.store(4.0f);
                    break;
                case NavCompressor:
                    dsp->params.compEnabled.store(true);
                    dsp->params.compThresholdDb.store(-18.0f);
                    dsp->params.compRatio.store(3.0f);
                    dsp->params.compAttackMs.store(10.0f);
                    dsp->params.compReleaseMs.store(100.0f);
                    dsp->params.compKnee.store(1);
                    dsp->params.compMakeupDb.store(0.0f);
                    dsp->params.compMixPercent.store(100.0f);
                    break;
                case NavLimiter:
                    dsp->params.limiterEnabled.store(true);
                    dsp->params.limiterCeilingDb.store(-1.0f);
                    dsp->params.limiterReleaseMs.store(60.0f);
                    break;
                case NavReverb:
                    dsp->params.reverbEnabled.store(false);
                    dsp->params.reverbRoomSize.store(0.5f);
                    dsp->params.reverbDamping.store(0.5f);
                    dsp->params.reverbWidth.store(1.0f);
                    dsp->params.reverbWetLevel.store(0.33f);
                    dsp->params.reverbDryLevel.store(1.0f);
                    break;
                case NavDelay:
                    dsp->params.delayEnabled.store(false);
                    dsp->params.delayTimeMs.store(350.0f);
                    dsp->params.delayFeedback.store(0.35f);
                    dsp->params.delayWetLevel.store(0.25f);
                    dsp->params.delayDryLevel.store(1.0f);
                    dsp->params.delayPingPong.store(false);
                    break;
                default:
                    break;
            }
            updateUIFromProcessor();
            return;
        }

        // 5. Module Copy To Button
        if (button == &moduleCopyBtn && dsp != nullptr)
        {
            int destIdx = moduleCopyCombo.getSelectedId() - 1;
            if (destIdx >= 0 && destIdx < NumTargets && destIdx != (int)currentTarget && targetDSPs[destIdx] != nullptr)
            {
                auto* destDSP = targetDSPs[destIdx];
                switch (currentNavModule)
                {
                    case NavHPF:
                        destDSP->params.hpfEnabled.store(dsp->params.hpfEnabled.load());
                        destDSP->params.hpfFreq.store(dsp->params.hpfFreq.load());
                        destDSP->params.hpfSlope.store(dsp->params.hpfSlope.load());
                        destDSP->params.hpfType.store(dsp->params.hpfType.load());
                        break;
                    case NavGate:
                        destDSP->params.gateEnabled.store(dsp->params.gateEnabled.load());
                        destDSP->params.gateThresholdDb.store(dsp->params.gateThresholdDb.load());
                        destDSP->params.gateAttackMs.store(dsp->params.gateAttackMs.load());
                        destDSP->params.gateHoldMs.store(dsp->params.gateHoldMs.load());
                        destDSP->params.gateReleaseMs.store(dsp->params.gateReleaseMs.load());
                        destDSP->params.gateFloorDb.store(dsp->params.gateFloorDb.load());
                        destDSP->params.gateHysteresisDb.store(dsp->params.gateHysteresisDb.load());
                        break;
                    case NavCompressor:
                        destDSP->params.compEnabled.store(dsp->params.compEnabled.load());
                        destDSP->params.compThresholdDb.store(dsp->params.compThresholdDb.load());
                        destDSP->params.compRatio.store(dsp->params.compRatio.load());
                        destDSP->params.compAttackMs.store(dsp->params.compAttackMs.load());
                        destDSP->params.compReleaseMs.store(dsp->params.compReleaseMs.load());
                        destDSP->params.compKnee.store(dsp->params.compKnee.load());
                        destDSP->params.compMakeupDb.store(dsp->params.compMakeupDb.load());
                        destDSP->params.compMixPercent.store(dsp->params.compMixPercent.load());
                        break;
                    case NavLimiter:
                        destDSP->params.limiterEnabled.store(dsp->params.limiterEnabled.load());
                        destDSP->params.limiterCeilingDb.store(dsp->params.limiterCeilingDb.load());
                        destDSP->params.limiterReleaseMs.store(dsp->params.limiterReleaseMs.load());
                        break;
                    case NavReverb:
                        destDSP->params.reverbEnabled.store(dsp->params.reverbEnabled.load());
                        destDSP->params.reverbRoomSize.store(dsp->params.reverbRoomSize.load());
                        destDSP->params.reverbDamping.store(dsp->params.reverbDamping.load());
                        destDSP->params.reverbWidth.store(dsp->params.reverbWidth.load());
                        destDSP->params.reverbWetLevel.store(dsp->params.reverbWetLevel.load());
                        destDSP->params.reverbDryLevel.store(dsp->params.reverbDryLevel.load());
                        break;
                    case NavDelay:
                        destDSP->params.delayEnabled.store(dsp->params.delayEnabled.load());
                        destDSP->params.delayTimeMs.store(dsp->params.delayTimeMs.load());
                        destDSP->params.delayFeedback.store(dsp->params.delayFeedback.load());
                        destDSP->params.delayWetLevel.store(dsp->params.delayWetLevel.load());
                        destDSP->params.delayDryLevel.store(dsp->params.delayDryLevel.load());
                        destDSP->params.delayPingPong.store(dsp->params.delayPingPong.load());
                        break;
                    default:
                        break;
                }
            }
            return;
        }

        // Delay Ping-Pong Toggle
        if (button == &delayPingPongToggle && dsp != nullptr)
        {
            dsp->params.delayPingPong.store(delayPingPongToggle.getToggleState());
            if (delayVisualizer) delayVisualizer->repaint();
            return;
        }

        // 6. EQ Specific Buttons
        if (button == &powerBtn)
        {
            bool cur = getCurrentProcessor().params.masterEnabled.load();
            getCurrentProcessor().params.masterEnabled.store(!cur);
            updateUIFromProcessor();
            if (graphVisualizer) graphVisualizer->repaint();
            return;
        }

        if (button == &analyzerToggleBtn)
        {
            bool cur = getCurrentProcessor().isAnalyzerEnabled();
            getCurrentProcessor().setAnalyzerEnabled(!cur);
            analyzerToggleBtn.setColour(juce::TextButton::buttonColourId, !cur ? juce::Colour(0xff2563eb) : juce::Colour(0xff1e293b));
            if (graphVisualizer) graphVisualizer->repaint();
            return;
        }

        if (button == &preBtn)
        {
            getCurrentProcessor().setAnalyzerPost(false);
            preBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
            postBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
            return;
        }

        if (button == &postBtn)
        {
            getCurrentProcessor().setAnalyzerPost(true);
            postBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
            preBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
            return;
        }

        for (int b = 0; b < numEQBands; ++b)
        {
            if (button == &bandStrips[b].bypassBtn)
            {
                bool cur = getCurrentProcessor().params.bands[b].enabled.load();
                getCurrentProcessor().params.bands[b].enabled.store(!cur);
                getCurrentProcessor().updateCoefficients();
                updateUIFromProcessor();
                if (graphVisualizer) graphVisualizer->repaint();
                return;
            }
        }

        if (button == &copyBtn)
        {
            int destIdx = copyToCombo.getSelectedId() - 1;
            if (destIdx >= 0 && destIdx < NumTargets && destIdx != currentTarget && targetProcessors[(size_t)destIdx] != nullptr)
            {
                targetProcessors[(size_t)destIdx]->params = getCurrentProcessor().params;
                targetProcessors[(size_t)destIdx]->updateCoefficients();
            }
            return;
        }

        if (button == &resetEQBtn)
        {
            getCurrentProcessor().params.setToDefault();
            getCurrentProcessor().updateCoefficients();
            updateUIFromProcessor();
            if (graphVisualizer) graphVisualizer->repaint();
            return;
        }

        if (button == &savePresetBtn || button == &saveAsPresetBtn)
        {
            const int selectedId = presetCombo.getSelectedId();
            const PresetItem* existingItem = nullptr;
            if (button == &savePresetBtn && selectedId >= 1000)
            {
                const int catalogIndex = selectedId - 1000;
                if (catalogIndex >= 0 &&
                    catalogIndex < (int)catalogPresetItems.size() &&
                    !catalogPresetItems[(size_t)catalogIndex].isFactory)
                    existingItem = &catalogPresetItems[(size_t)catalogIndex];
            }

            if (button == &savePresetBtn && existingItem != nullptr)
            {
                if (saveCurrentEQCatalogPreset(existingItem->name, existingItem))
                    refreshEQCatalogPresets();
                return;
            }

            juce::AlertWindow alert("Save EQ Preset As",
                                    "Enter a name for this EQ preset:",
                                    juce::AlertWindow::QuestionIcon);
            alert.addTextEditor("name", "My EQ Preset");
            alert.addButton("Save", 1);
            alert.addButton("Cancel", 0);
            if (alert.runModalLoop() == 1)
            {
                const auto name = alert.getTextEditorContents("name").trim();
                if (name.isNotEmpty() && saveCurrentEQCatalogPreset(name, nullptr))
                    refreshEQCatalogPresets();
            }
            return;
        }

        if (button == &deletePresetBtn)
        {
            const int selectedId = presetCombo.getSelectedId();
            if (selectedId >= 1000 && presetManager != nullptr)
            {
                const int catalogIndex = selectedId - 1000;
                if (catalogIndex >= 0 &&
                    catalogIndex < (int)catalogPresetItems.size())
                {
                    const auto item =
                        catalogPresetItems[(size_t)catalogIndex];
                    if (item.isFactory)
                    {
                        juce::AlertWindow::showMessageBox(
                            juce::AlertWindow::WarningIcon, "Factory Preset",
                            "Factory EQ presets cannot be deleted.", "OK");
                        return;
                    }
                    if (juce::AlertWindow::showOkCancelBox(
                            juce::AlertWindow::QuestionIcon, "Delete EQ Preset",
                            "Delete preset '" + item.name + "'?",
                            "Delete", "Cancel") &&
                        presetManager->deleteUserPreset(item))
                        refreshEQCatalogPresets();
                }
            }
            return;
        }
    }

    void EightBandEQComponent::timerCallback()
    {
        // 1. Equalizer FFT Update
        if (currentNavModule == NavEqualizer && getCurrentProcessor().isAnalyzerEnabled())
        {
            float incoming[fftSize];
            if (getCurrentProcessor().pullFromAnalyzer(incoming, fftSize))
            {
                fftWindow.multiplyWithWindowingTable(incoming, fftSize);
                std::fill(fftData.begin(), fftData.end(), 0.0f);
                std::memcpy(fftData.data(), incoming, fftSize * sizeof(float));
                fft.performFrequencyOnlyForwardTransform(fftData.data());

                for (size_t i = 0; i < smoothedFftBins.size(); ++i)
                {
                    size_t binIndex = juce::jlimit<size_t>(0, (size_t)fftSize / 2, i * 4);
                    float mag = fftData[binIndex] * (4.0f / (float)fftSize);
                    float db = juce::Decibels::gainToDecibels(mag, -90.0f);
                    smoothedFftBins[i] = smoothedFftBins[i] * 0.7f + db * 0.3f;
                }
            }
            if (graphVisualizer) graphVisualizer->repaint();
        }

        // 2. Repaint active module visualizer
        switch (currentNavModule)
        {
            case NavEqualizer:
                if (graphVisualizer) graphVisualizer->repaint();
                break;
            case NavHPF:
                if (hpfVisualizer) hpfVisualizer->repaint();
                break;
            case NavGate:
                if (gateVisualizer) gateVisualizer->repaint();
                break;
            case NavCompressor:
                if (compVisualizer) compVisualizer->repaint();
                break;
            case NavLimiter:
                if (limiterVisualizer) limiterVisualizer->repaint();
                break;
            case NavReverb:
                if (reverbVisualizer) reverbVisualizer->repaint();
                break;
            case NavDelay:
                if (delayVisualizer) delayVisualizer->repaint();
                break;
            default:
                break;
        }
    }

    void EightBandEQComponent::loadFactoryPresets()
    {
        userPresets.clear();

        EQPreset p1;
        p1.name = "Vocal Clear";
        p1.params.setToDefault();
        userPresets.push_back(p1);

        EQPreset p2;
        p2.name = "Podcast Warmth";
        p2.params.bands[0] = EQBandParameter(EQFilterType::HighPass, 70.0f, 0.0f, 0.71f);
        p2.params.bands[1] = EQBandParameter(EQFilterType::LowShelf, 150.0f, 3.0f, 0.71f);
        p2.params.bands[2] = EQBandParameter(EQFilterType::Bell, 400.0f, -3.5f, 1.4f);
        p2.params.bands[3] = EQBandParameter(EQFilterType::Bell, 1000.0f, 2.0f, 1.0f);
        p2.params.bands[4] = EQBandParameter(EQFilterType::Bell, 2800.0f, 3.5f, 1.2f);
        p2.params.bands[5] = EQBandParameter(EQFilterType::Bell, 5500.0f, 2.5f, 1.0f);
        p2.params.bands[6] = EQBandParameter(EQFilterType::Bell, 12000.0f, 2.0f, 0.71f);
        p2.params.bands[7] = EQBandParameter(EQFilterType::LowPass, 18000.0f, 0.0f, 0.71f);
        userPresets.push_back(p2);

        EQPreset p3;
        p3.name = "Acoustic Guitar";
        p3.params.bands[0] = EQBandParameter(EQFilterType::HighPass, 90.0f, 0.0f, 0.71f);
        p3.params.bands[1] = EQBandParameter(EQFilterType::Bell, 200.0f, -3.0f, 1.5f);
        p3.params.bands[2] = EQBandParameter(EQFilterType::Bell, 500.0f, -2.0f, 1.0f);
        p3.params.bands[3] = EQBandParameter(EQFilterType::Bell, 1200.0f, 1.5f, 1.2f);
        p3.params.bands[4] = EQBandParameter(EQFilterType::Bell, 3500.0f, 3.0f, 1.0f);
        p3.params.bands[5] = EQBandParameter(EQFilterType::Bell, 7000.0f, 2.5f, 1.0f);
        p3.params.bands[6] = EQBandParameter(EQFilterType::HighShelf, 10000.0f, 3.5f, 0.71f);
        p3.params.bands[7] = EQBandParameter(EQFilterType::LowPass, 18000.0f, 0.0f, 0.71f);
        userPresets.push_back(p3);

        EQPreset p4;
        p4.name = "Broadcast Master";
        p4.params.bands[0] = EQBandParameter(EQFilterType::HighPass, 32.0f, 0.0f, 0.71f);
        p4.params.bands[1] = EQBandParameter(EQFilterType::LowShelf, 90.0f, 1.8f, 0.71f);
        p4.params.bands[2] = EQBandParameter(EQFilterType::Bell, 250.0f, -1.5f, 1.2f);
        p4.params.bands[3] = EQBandParameter(EQFilterType::Bell, 1000.0f, 0.5f, 1.0f);
        p4.params.bands[4] = EQBandParameter(EQFilterType::Bell, 3000.0f, 1.5f, 1.0f);
        p4.params.bands[5] = EQBandParameter(EQFilterType::Bell, 6500.0f, 2.0f, 1.0f);
        p4.params.bands[6] = EQBandParameter(EQFilterType::HighShelf, 12000.0f, 2.5f, 0.71f);
        p4.params.bands[7] = EQBandParameter(EQFilterType::LowPass, 20000.0f, 0.0f, 0.71f);
        userPresets.push_back(p4);

        EQPreset p5;
        p5.name = "Flat / Reset";
        p5.params.setToFlat();
        userPresets.push_back(p5);

        refreshEQCatalogPresets();
    }

    void EightBandEQComponent::refreshEQCatalogPresets()
    {
        presetCombo.clear(juce::dontSendNotification);
        for (size_t i = 0; i < userPresets.size(); ++i)
            presetCombo.addItem(userPresets[i].name, (int)i + 1);

        catalogPresetItems.clear();
        if (presetManager != nullptr)
        {
            catalogPresetItems = presetManager->getPresetsForCategory(
                PresetCategory::EQCatalog);
            bool hasFactoryHeading = false;
            bool hasUserHeading = false;
            for (size_t i = 0; i < catalogPresetItems.size(); ++i)
            {
                const auto& item = catalogPresetItems[i];
                if (item.isFactory && !hasFactoryHeading)
                {
                    presetCombo.addSectionHeading("EQ Catalog - Factory");
                    hasFactoryHeading = true;
                }
                else if (!item.isFactory && !hasUserHeading)
                {
                    presetCombo.addSectionHeading("EQ Catalog - Your Presets");
                    hasUserHeading = true;
                }
                presetCombo.addItem(item.name, 1000 + (int)i);
            }
        }
        presetCombo.setSelectedId(1, juce::dontSendNotification);
    }

    bool EightBandEQComponent::saveCurrentEQCatalogPreset(
        const juce::String& name, const PresetItem* existingItem)
    {
        if (presetManager == nullptr)
            return false;

        EQPresetDefinition definition;
        definition.name = name;
        definition.category = EQPresetCategory::Vocal;
        juce::String description =
            "User-created starting curve. Adjust to the source and listener.";
        bool favorite = false;
        if (existingItem != nullptr)
        {
            EQPresetDefinition previous;
            juce::String error;
            if (!presetManager->loadEQCatalogPreset(*existingItem, previous, error))
            {
                juce::AlertWindow::showMessageBox(
                    juce::AlertWindow::WarningIcon, "EQ Preset Error",
                    "Could not load the selected preset: " + error, "OK");
                return false;
            }
            definition.category = previous.category;
            description = previous.description;
            favorite = existingItem->isFavorite;
        }

        const auto& parameters = getCurrentProcessor().params;
        for (int band = 0; band < numEQBands; ++band)
        {
            auto& target = definition.bands[(size_t)band];
            const auto& source = parameters.bands[(size_t)band];
            target.type = (EQPresetFilterType)
                source.type.load(std::memory_order_relaxed);
            target.frequencyHz =
                source.freq.load(std::memory_order_relaxed);
            target.gainDb =
                source.gainDb.load(std::memory_order_relaxed);
            target.q = source.q.load(std::memory_order_relaxed);
            target.enabled =
                source.enabled.load(std::memory_order_relaxed);
        }

        juce::String error;
        if (!presetManager->saveEQCatalogPreset(
                name, definition, description, favorite,
                existingItem != nullptr, error))
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon, "Could Not Save EQ Preset",
                error, "OK");
            return false;
        }
        return true;
    }

    void EightBandEQComponent::applyPreset(const EightBandEQParameters& p)
    {
        getCurrentProcessor().params = p;
        getCurrentProcessor().updateCoefficients();
        updateUIFromProcessor();
        if (graphVisualizer) graphVisualizer->repaint();
    }

    void EightBandEQComponent::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xff090c12));

        // Sidebar background
        g.setColour(juce::Colour(0xff0e131d));
        g.fillRect(0, 0, 160, getHeight());
        g.setColour(juce::Colour(0xff1e293b));
        g.drawVerticalLine(160, 0.0f, (float)getHeight());
    }

    void EightBandEQComponent::resized()
    {
        const int sidebarW = 160;
        const int w = getWidth();
        const int h = getHeight();

        // 1. Sidebar Layout
        dspProcessorHeader.setBounds(12, 10, sidebarW - 24, 20);
        eqNavBtn.setBounds(10, 34, sidebarW - 20, 26);
        hpfNavBtn.setBounds(10, 64, sidebarW - 20, 26);
        gateNavBtn.setBounds(10, 94, sidebarW - 20, 26);
        compNavBtn.setBounds(10, 124, sidebarW - 20, 26);
        limiterNavBtn.setBounds(10, 154, sidebarW - 20, 26);
        reverbNavBtn.setBounds(10, 184, sidebarW - 20, 26);
        delayNavBtn.setBounds(10, 214, sidebarW - 20, 26);

        applyToHeader.setBounds(12, 256, sidebarW - 24, 20);
        constexpr int targetButtonHeight = 22;
        constexpr int targetButtonGap = 3;
        for (int t = 0; t < NumTargets; ++t)
        {
            targetButtons[t].setBounds(10, 280 + t * (targetButtonHeight + targetButtonGap),
                                       sidebarW - 20, targetButtonHeight);
        }

        // 2. Main Content Area Layout
        const int mainX = sidebarW + 16;
        const int mainW = w - mainX - 16;

        // Shared Module Header Bar
        modulePowerBtn.setBounds(mainX, 12, 54, 26);
        moduleTitleLabel.setBounds(mainX + 62, 12, 260, 26);
        moduleTargetBadge.setBounds(mainX + 326, 12, 140, 26);
        moduleCopyLabel.setBounds(mainX + mainW - 354, 12, 54, 26);
        moduleCopyCombo.setBounds(mainX + mainW - 294, 12, 110, 26);
        moduleCopyBtn.setBounds(mainX + mainW - 178, 12, 48, 26);
        moduleResetBtn.setBounds(mainX + mainW - 120, 12, 120, 26);

        if (currentNavModule == NavEqualizer)
        {
            // EQ Top Header
            powerBtn.setBounds(mainX, 12, 28, 28);
            titleLabel.setBounds(mainX + 36, 12, 250, 28);
            eqOnToggle.setBounds(mainX + 290, 12, 70, 28);
            analyzerToggleBtn.setBounds(mainX + 370, 12, 70, 28);
            preBtn.setBounds(mainX + 444, 12, 40, 28);
            postBtn.setBounds(mainX + 486, 12, 44, 28);
            fftSizeLabel.setBounds(mainX + 540, 12, 120, 28);

            const int bottomStripsH = 150;
            const int actionBarH = 34;
            const int graphH = juce::jmax(180, h - 50 - bottomStripsH - actionBarH - 24);
            if (graphVisualizer) graphVisualizer->setBounds(mainX, 48, mainW, graphH);

            const int stripsY = 48 + graphH + 8;
            const int stripW = (mainW - (numEQBands - 1) * 4) / numEQBands;

            for (int b = 0; b < numEQBands; ++b)
            {
                auto& strip = bandStrips[b];
                const int bx = mainX + b * (stripW + 4);

                strip.bandPill.setBounds(bx + 2, stripsY, stripW - 4, 18);
                strip.typeCombo.setBounds(bx, stripsY + 22, stripW, 20);

                const int knobW = (stripW - 4) / 3;
                const int knobY = stripsY + 46;
                const int knobSize = juce::jmin(knobW, 36);

                strip.freqSlider.setBounds(bx, knobY, knobSize, knobSize);
                strip.freqValueLabel.setBounds(bx - 2, knobY + knobSize, knobSize + 8, 14);

                strip.gainSlider.setBounds(bx + knobW, knobY, knobSize, knobSize);
                strip.gainValueLabel.setBounds(bx + knobW - 4, knobY + knobSize, knobSize + 8, 14);

                strip.qSlider.setBounds(bx + knobW * 2, knobY, knobSize, knobSize);
                strip.qValueLabel.setBounds(bx + knobW * 2 - 4, knobY + knobSize, knobSize + 8, 14);

                strip.bypassBtn.setBounds(bx + stripW / 2 - 14, stripsY + bottomStripsH - 24, 28, 22);
            }

            const int barY = stripsY + bottomStripsH + 6;
            presetLabel.setBounds(mainX, barY, 90, 26);
            presetCombo.setBounds(mainX + 94, barY, 150, 26);
            savePresetBtn.setBounds(mainX + 250, barY, 50, 26);
            saveAsPresetBtn.setBounds(mainX + 304, barY, 70, 26);
            deletePresetBtn.setBounds(mainX + 378, barY, 60, 26);

            copyToLabel.setBounds(mainX + 460, barY, 60, 26);
            copyToCombo.setBounds(mainX + 524, barY, 130, 26);
            copyBtn.setBounds(mainX + 658, barY, 50, 26);

            resetEQBtn.setBounds(mainX + mainW - 100, barY, 100, 26);
        }
        else
        {
            // Center Visualizer Area for Modules
            const int visH = juce::jmax(200, h - 220);
            const int controlsY = 48 + visH + 12;

            if (currentNavModule == NavHPF)
            {
                if (hpfVisualizer) hpfVisualizer->setBounds(mainX, 48, mainW, visH);

                hpfCutoffSlider.setBounds(mainX + 40, controlsY, 70, 70);
                hpfCutoffLabel.setBounds(mainX + 20, controlsY + 72, 110, 16);

                hpfSlopeLabel.setBounds(mainX + 180, controlsY + 8, 120, 16);
                hpfSlopeCombo.setBounds(mainX + 180, controlsY + 28, 130, 26);

                hpfTypeLabel.setBounds(mainX + 340, controlsY + 8, 120, 16);
                hpfTypeCombo.setBounds(mainX + 340, controlsY + 28, 130, 26);
            }
            else if (currentNavModule == NavGate)
            {
                if (gateVisualizer) gateVisualizer->setBounds(mainX, 48, mainW, visH);

                int kX = mainX + 20;
                const int kSpacing = (mainW - 40) / 6;

                gateThreshSlider.setBounds(kX + 0 * kSpacing, controlsY, 60, 60);
                gateThreshLabel.setBounds(kX + 0 * kSpacing - 10, controlsY + 62, 80, 16);

                gateAttackSlider.setBounds(kX + 1 * kSpacing, controlsY, 60, 60);
                gateAttackLabel.setBounds(kX + 1 * kSpacing - 10, controlsY + 62, 80, 16);

                gateHoldSlider.setBounds(kX + 2 * kSpacing, controlsY, 60, 60);
                gateHoldLabel.setBounds(kX + 2 * kSpacing - 10, controlsY + 62, 80, 16);

                gateReleaseSlider.setBounds(kX + 3 * kSpacing, controlsY, 60, 60);
                gateReleaseLabel.setBounds(kX + 3 * kSpacing - 10, controlsY + 62, 80, 16);

                gateFloorSlider.setBounds(kX + 4 * kSpacing, controlsY, 60, 60);
                gateFloorLabel.setBounds(kX + 4 * kSpacing - 10, controlsY + 62, 80, 16);

                gateHystSlider.setBounds(kX + 5 * kSpacing, controlsY, 60, 60);
                gateHystLabel.setBounds(kX + 5 * kSpacing - 10, controlsY + 62, 80, 16);
            }
            else if (currentNavModule == NavCompressor)
            {
                if (compVisualizer) compVisualizer->setBounds(mainX, 48, mainW, visH);

                int kX = mainX + 16;
                const int kSpacing = (mainW - 32) / 7;

                compThreshSlider.setBounds(kX + 0 * kSpacing, controlsY, 60, 60);
                compThreshLabel.setBounds(kX + 0 * kSpacing - 10, controlsY + 62, 80, 16);

                compRatioSlider.setBounds(kX + 1 * kSpacing, controlsY, 60, 60);
                compRatioLabel.setBounds(kX + 1 * kSpacing - 10, controlsY + 62, 80, 16);

                compAttackSlider.setBounds(kX + 2 * kSpacing, controlsY, 60, 60);
                compAttackLabel.setBounds(kX + 2 * kSpacing - 10, controlsY + 62, 80, 16);

                compReleaseSlider.setBounds(kX + 3 * kSpacing, controlsY, 60, 60);
                compReleaseLabel.setBounds(kX + 3 * kSpacing - 10, controlsY + 62, 80, 16);

                compKneeLabel.setBounds(kX + 4 * kSpacing, controlsY + 8, 90, 16);
                compKneeCombo.setBounds(kX + 4 * kSpacing, controlsY + 28, 95, 24);

                compMakeupSlider.setBounds(kX + 5 * kSpacing, controlsY, 60, 60);
                compMakeupLabel.setBounds(kX + 5 * kSpacing - 10, controlsY + 62, 80, 16);

                compMixSlider.setBounds(kX + 6 * kSpacing, controlsY, 60, 60);
                compMixLabel.setBounds(kX + 6 * kSpacing - 10, controlsY + 62, 80, 16);
            }
            else if (currentNavModule == NavLimiter)
            {
                if (limiterVisualizer) limiterVisualizer->setBounds(mainX, 48, mainW, visH);

                limiterCeilSlider.setBounds(mainX + 60, controlsY, 70, 70);
                limiterCeilLabel.setBounds(mainX + 40, controlsY + 72, 110, 16);

                limiterReleaseSlider.setBounds(mainX + 200, controlsY, 70, 70);
                limiterReleaseLabel.setBounds(mainX + 180, controlsY + 72, 110, 16);
            }
            else if (currentNavModule == NavReverb)
            {
                if (reverbVisualizer) reverbVisualizer->setBounds(mainX, 48, mainW, visH);

                int kX = mainX + 30;
                const int kSpacing = (mainW - 60) / 5;

                reverbRoomSlider.setBounds(kX + 0 * kSpacing, controlsY, 60, 60);
                reverbRoomLabel.setBounds(kX + 0 * kSpacing - 10, controlsY + 62, 80, 16);

                reverbDampSlider.setBounds(kX + 1 * kSpacing, controlsY, 60, 60);
                reverbDampLabel.setBounds(kX + 1 * kSpacing - 10, controlsY + 62, 80, 16);

                reverbWidthSlider.setBounds(kX + 2 * kSpacing, controlsY, 60, 60);
                reverbWidthLabel.setBounds(kX + 2 * kSpacing - 10, controlsY + 62, 80, 16);

                reverbWetSlider.setBounds(kX + 3 * kSpacing, controlsY, 60, 60);
                reverbWetLabel.setBounds(kX + 3 * kSpacing - 10, controlsY + 62, 80, 16);

                reverbDrySlider.setBounds(kX + 4 * kSpacing, controlsY, 60, 60);
                reverbDryLabel.setBounds(kX + 4 * kSpacing - 10, controlsY + 62, 80, 16);
            }
            else if (currentNavModule == NavDelay)
            {
                if (delayVisualizer) delayVisualizer->setBounds(mainX, 48, mainW, visH);

                int kX = mainX + 20;
                const int kSpacing = (mainW - 40) / 5;

                delayTimeSlider.setBounds(kX + 0 * kSpacing, controlsY, 60, 60);
                delayTimeLabel.setBounds(kX + 0 * kSpacing - 10, controlsY + 62, 80, 16);

                delayFeedbackSlider.setBounds(kX + 1 * kSpacing, controlsY, 60, 60);
                delayFeedbackLabel.setBounds(kX + 1 * kSpacing - 10, controlsY + 62, 80, 16);

                delayWetSlider.setBounds(kX + 2 * kSpacing, controlsY, 60, 60);
                delayWetLabel.setBounds(kX + 2 * kSpacing - 10, controlsY + 62, 80, 16);

                delayDrySlider.setBounds(kX + 3 * kSpacing, controlsY, 60, 60);
                delayDryLabel.setBounds(kX + 3 * kSpacing - 10, controlsY + 62, 80, 16);

                delayPingPongToggle.setBounds(kX + 4 * kSpacing, controlsY + 20, 140, 26);
            }
        }
    }
}
