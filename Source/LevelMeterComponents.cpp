#include "LevelMeterComponents.h"
#include <iomanip>
#include <sstream>

namespace LiveBus
{
    // =========================================================================
    // AnalogVUMeterComponent Implementation
    // =========================================================================
    AnalogVUMeterComponent::AnalogVUMeterComponent(const juce::String& channelLabel)
        : label(channelLabel)
    {
    }

    void AnalogVUMeterComponent::setChannelLabel(const juce::String& newLabel)
    {
        label = newLabel;
        repaint();
    }

    void AnalogVUMeterComponent::updateReading(float vuNormalized, float vuLevel, float peakDbFS, bool isClipped)
    {
        currentNormalized = vuNormalized;
        currentVuLevel = vuLevel;
        currentPeakDb = peakDbFS;
        clipped = isClipped;
        repaint();
    }

    void AnalogVUMeterComponent::mouseDown(const juce::MouseEvent&)
    {
        if (onResetClicked)
            onResetClicked();
    }

    void AnalogVUMeterComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Outer Chassis
        g.setColour(juce::Colour(0xff121722));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xff222c3d));
        g.drawRoundedRectangle(bounds, 6.0f, 1.2f);

        // 2. Dial Face Area
        auto dialArea = bounds.reduced(5.0f);
        dialArea.removeFromBottom(22.0f); // Space for bottom numeric readouts

        juce::ColourGradient dialGradient(
            juce::Colour(0xff161d2b), dialArea.getCentreX(), dialArea.getY(),
            juce::Colour(0xff0d1118), dialArea.getCentreX(), dialArea.getBottom(), false);
        g.setGradientFill(dialGradient);
        g.fillRoundedRectangle(dialArea, 4.0f);

        g.setColour(juce::Colour(0xff283344));
        g.drawRoundedRectangle(dialArea, 4.0f, 1.0f);

        // Pivot and Arc Dimensions
        const float pivotX = dialArea.getCentreX();
        const float pivotY = dialArea.getBottom() + dialArea.getHeight() * 0.15f;
        const float radius = dialArea.getHeight() * 0.95f;

        const float startAngle = -juce::MathConstants<float>::pi * 0.22f; // ~ -39.6 deg
        const float endAngle   =  juce::MathConstants<float>::pi * 0.22f; // ~ +39.6 deg
        const float zeroVuAngle = startAngle + 0.76f * (endAngle - startAngle);

        // 3. Scale Arcs (0 VU is demarcation between normal and red zone)
        juce::Path normalArc;
        normalArc.addCentredArc(pivotX, pivotY, radius, radius, 0.0f, startAngle, zeroVuAngle, true);
        g.setColour(juce::Colour(0xffe2e8f0)); // Light cream / silver arc
        g.strokePath(normalArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path redArc;
        redArc.addCentredArc(pivotX, pivotY, radius, radius, 0.0f, zeroVuAngle, endAngle, true);
        g.setColour(juce::Colour(0xffef4444)); // Vivid red over-0 zone
        g.strokePath(redArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Tick Marks and Labels
        struct VUTick { float vu; float norm; const char* txt; bool isMajor; };
        static const VUTick ticks[] = {
            { -20.0f, 0.00f, "20", true },
            { -10.0f, 0.22f, "10", true },
            {  -7.0f, 0.35f, "7",  false },
            {  -5.0f, 0.46f, "5",  true },
            {  -3.0f, 0.58f, "3",  true },
            {  -2.0f, 0.64f, "2",  false },
            {  -1.0f, 0.70f, "1",  false },
            {   0.0f, 0.76f, "0",  true },
            {  +1.0f, 0.83f, "1",  false },
            {  +2.0f, 0.91f, "2",  false },
            {  +3.0f, 1.00f, "3",  true }
        };

        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));

        for (const auto& t : ticks)
        {
            const float angle = startAngle + t.norm * (endAngle - startAngle);
            const float sinA = std::sin(angle);
            const float cosA = std::cos(angle);

            const float tickLen = t.isMajor ? 6.0f : 3.5f;
            const float rInner = radius - tickLen;
            const float rOuter = radius + 1.0f;

            const float x1 = pivotX + rInner * sinA;
            const float y1 = pivotY - rInner * cosA;
            const float x2 = pivotX + rOuter * sinA;
            const float y2 = pivotY - rOuter * cosA;

            const bool isRed = (t.vu >= 0.0f);
            g.setColour(isRed ? juce::Colour(0xffef4444) : juce::Colour(0xffcbd5e1));
            g.drawLine(x1, y1, x2, y2, t.isMajor ? 1.5f : 1.0f);

            if (t.isMajor)
            {
                const float textR = radius - 14.0f;
                const float tx = pivotX + textR * sinA;
                const float ty = pivotY - textR * cosA;
                g.drawText(t.txt, (int)tx - 10, (int)ty - 6, 20, 12, juce::Justification::centred);
            }
        }

        // Center "VU" dial text
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("VU", (int)pivotX - 16, (int)dialArea.getY() + 38, 32, 14, juce::Justification::centred);

        // Channel Target Label
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff00f5d4));
        g.drawText(label, (int)dialArea.getX() + 8, (int)dialArea.getY() + 6, 90, 16, juce::Justification::left);

        // Peak / Clip LED
        const float ledX = dialArea.getRight() - 20.0f;
        const float ledY = dialArea.getY() + 8.0f;
        if (clipped)
        {
            g.setColour(juce::Colour(0xffff2d55));
            g.fillEllipse(ledX, ledY, 9.0f, 9.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(ledX, ledY, 9.0f, 9.0f, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff2d1217));
            g.fillEllipse(ledX, ledY, 9.0f, 9.0f);
            g.setColour(juce::Colour(0xff451a22));
            g.drawEllipse(ledX, ledY, 9.0f, 9.0f, 1.0f);
        }

        g.setFont(juce::FontOptions(8.0f));
        g.setColour(clipped ? juce::Colour(0xffff2d55) : juce::Colour(0xff64748b));
        g.drawText("CLIP", (int)ledX - 22, (int)ledY - 1, 20, 11, juce::Justification::right);

        // 5. Moving Needle
        const float needleNorm = juce::jlimit(0.0f, 1.15f, currentNormalized);
        const float needleAngle = startAngle + needleNorm * (endAngle - startAngle);
        const float needleLen = radius + 3.0f;

        const float needleSin = std::sin(needleAngle);
        const float needleCos = std::cos(needleAngle);

        const float tipX = pivotX + needleLen * needleSin;
        const float tipY = pivotY - needleLen * needleCos;

        // Needle Drop Shadow
        g.setColour(juce::Colour(0x66000000));
        g.drawLine(pivotX + 2.0f, pivotY + 2.0f, tipX + 2.0f, tipY + 2.0f, 2.0f);

        // Needle Body (Vibrant orange/silver tip)
        g.setColour(juce::Colour(0xffff5500));
        g.drawLine(pivotX, pivotY, tipX, tipY, 1.8f);

        // 6. Pivot Hub / Cap
        const float hubRadius = 9.0f;
        g.setColour(juce::Colour(0xff090c12));
        g.fillEllipse(pivotX - hubRadius, pivotY - hubRadius, hubRadius * 2.0f, hubRadius * 2.0f);
        g.setColour(juce::Colour(0xff334155));
        g.drawEllipse(pivotX - hubRadius, pivotY - hubRadius, hubRadius * 2.0f, hubRadius * 2.0f, 1.5f);

        g.setColour(juce::Colour(0xff94a3b8));
        g.fillEllipse(pivotX - 2.5f, pivotY - 2.5f, 5.0f, 5.0f);

        // 7. Bottom Numerical Readouts
        const int by = (int)bounds.getBottom() - 20;
        const int bw = (int)bounds.getWidth() / 2 - 8;

        // VU Readout
        juce::String vuStr = (currentVuLevel <= -19.5f) ? "-inf VU" : (juce::String(currentVuLevel >= 0 ? "+" : "") + juce::String(currentVuLevel, 1) + " VU");
        g.setColour(juce::Colour(0xff0f141d));
        g.fillRoundedRectangle((float)bounds.getX() + 6, (float)by, (float)bw, 16.0f, 3.0f);
        g.setColour(currentVuLevel >= 0.0f ? juce::Colour(0xffef4444) : juce::Colour(0xff38bdf8));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(vuStr, (int)bounds.getX() + 6, by, bw, 16, juce::Justification::centred);

        // Peak dBFS Readout
        juce::String pkStr = (currentPeakDb <= -80.0f) ? "-inf dB" : (juce::String(currentPeakDb, 1) + " dB");
        g.setColour(juce::Colour(0xff0f141d));
        g.fillRoundedRectangle((float)bounds.getX() + bw + 10, (float)by, (float)bw, 16.0f, 3.0f);
        g.setColour(currentPeakDb >= -0.1f ? juce::Colour(0xffef4444) : (currentPeakDb >= -6.0f ? juce::Colour(0xffeab308) : juce::Colour(0xff10b981)));
        g.drawText(pkStr, (int)bounds.getX() + bw + 10, by, bw, 16, juce::Justification::centred);
    }

    // =========================================================================
    // DigitalPeakMeterComponent Implementation
    // =========================================================================
    DigitalPeakMeterComponent::DigitalPeakMeterComponent(bool isStereo, Orientation orient)
        : stereo(isStereo), orientation(orient)
    {
    }

    void DigitalPeakMeterComponent::updateReading(const MeterSnapshot& snapshot)
    {
        currentSnapshot = snapshot;
        repaint();
    }

    void DigitalPeakMeterComponent::mouseDown(const juce::MouseEvent&)
    {
        if (onResetClicked)
            onResetClicked();
    }

    float DigitalPeakMeterComponent::dbToFraction(float db)
    {
        if (db <= -60.0f) return 0.0f;
        if (db >= 0.0f) return 1.0f;

        // Piecewise mapping for standard digital meter scale:
        // -60 dB -> 0.00
        // -48 dB -> 0.12
        // -36 dB -> 0.28
        // -24 dB -> 0.44
        // -18 dB -> 0.58
        // -12 dB -> 0.72
        //  -6 dB -> 0.86
        //   0 dB -> 1.00
        static const struct { float db; float frac; } map[] = {
            { -60.0f, 0.00f },
            { -48.0f, 0.12f },
            { -36.0f, 0.28f },
            { -24.0f, 0.44f },
            { -18.0f, 0.58f },
            { -12.0f, 0.72f },
            {  -6.0f, 0.86f },
            {   0.0f, 1.00f }
        };

        for (int i = 0; i < 7; ++i)
        {
            if (db >= map[i].db && db <= map[i + 1].db)
            {
                const float t = (db - map[i].db) / (map[i + 1].db - map[i].db);
                return map[i].frac + t * (map[i + 1].frac - map[i].frac);
            }
        }

        return 1.0f;
    }

    void DigitalPeakMeterComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xff0d1117));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff1f293d));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Header: CLIP LED indicator
        const float clipHeaderH = 14.0f;
        auto clipArea = bounds.removeFromTop(clipHeaderH).reduced(2.0f, 1.0f);
        const bool isClipped = currentSnapshot.clippedL || currentSnapshot.clippedR;

        g.setColour(isClipped ? juce::Colour(0xffef4444) : juce::Colour(0xff271014));
        g.fillRoundedRectangle(clipArea, 2.0f);
        g.setColour(isClipped ? juce::Colours::white : juce::Colour(0xff7f1d1d));
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText("CLIP", clipArea, juce::Justification::centred);

        // Footer: Peak dB value
        const float footerH = 15.0f;
        auto footerArea = bounds.removeFromBottom(footerH);
        const float maxPeakDb = std::max(currentSnapshot.peakDbL, currentSnapshot.peakDbR);
        juce::String peakStr = (maxPeakDb <= -60.0f) ? "-inf" : juce::String(maxPeakDb, 1);
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(maxPeakDb >= -0.1f ? juce::Colour(0xffef4444) : (maxPeakDb >= -6.0f ? juce::Colour(0xffeab308) : juce::Colour(0xff94a3b8)));
        g.drawText(peakStr, footerArea, juce::Justification::centred);

        // Scale & Bar Area
        auto meterArea = bounds.reduced(3.0f, 2.0f);

        const bool drawScale = showScale && meterArea.getWidth() >= 44.0f;
        const float scaleW = drawScale ? 18.0f : 0.0f;
        auto scaleArea = meterArea.removeFromRight(scaleW);

        // Draw dB scale ticks & numbers
        if (drawScale)
        {
            static const int scaleMarks[] = { 0, -6, -12, -18, -24, -36, -48 };
            g.setFont(juce::FontOptions(7.5f));
            for (int mark : scaleMarks)
            {
                const float frac = dbToFraction((float)mark);
                const float y = meterArea.getBottom() - frac * meterArea.getHeight();

                g.setColour(juce::Colour(0xff475569));
                g.drawHorizontalLine((int)y, scaleArea.getX(), scaleArea.getX() + 3.0f);

                g.setColour(mark >= 0 ? juce::Colour(0xffef4444) : (mark >= -12 ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff64748b)));
                g.drawText(juce::String(mark), (int)scaleArea.getX() + 4, (int)y - 4, (int)scaleW - 4, 8, juce::Justification::centredLeft);
            }
        }

        // Channels layout
        const int numChannels = stereo ? 2 : 1;
        const float totalSpacing = (numChannels - 1) * 2.0f;
        const float barW = juce::jmax(0.0f, (meterArea.getWidth() - totalSpacing) / numChannels);

        auto drawSingleBar = [&](float bx, float peakDb, float holdDb) {
            // Rail background
            g.setColour(juce::Colour(0xff161e2c));
            g.fillRect(bx, meterArea.getY(), barW, meterArea.getHeight());

            const float frac = dbToFraction(peakDb);
            const float barH = frac * meterArea.getHeight();
            const float barY = meterArea.getBottom() - barH;

            // Draw color zones
            if (barH > 0.0f)
            {
                juce::ColourGradient grad(
                    juce::Colour(0xff10b981), bx, meterArea.getBottom(),
                    juce::Colour(0xffef4444), bx, meterArea.getY(), false);
                grad.addColour(0.55, juce::Colour(0xff10b981)); // Safe up to -18 dB
                grad.addColour(0.75, juce::Colour(0xffeab308)); // Mid around -12 dB
                grad.addColour(0.90, juce::Colour(0xfff97316)); // High around -6 dB
                grad.addColour(0.99, juce::Colour(0xffef4444)); // Clip >= 0 dB

                g.setGradientFill(grad);
                g.fillRect(bx, barY, barW, barH);
            }

            // Draw Peak-Hold Tick
            if (holdDb > -60.0f)
            {
                const float holdFrac = dbToFraction(holdDb);
                const float holdY = meterArea.getBottom() - holdFrac * meterArea.getHeight();
                g.setColour(juce::Colour(0xff38bdf8)); // Bright cyan peak marker
                g.fillRect(bx, holdY - 1.0f, barW, 2.0f);
            }
        };

        drawSingleBar(meterArea.getX(), currentSnapshot.peakDbL, currentSnapshot.peakHoldDbL);
        if (stereo)
        {
            drawSingleBar(meterArea.getX() + barW + 2.0f, currentSnapshot.peakDbR, currentSnapshot.peakHoldDbR);
        }
    }

    // =========================================================================
    // MasterMeterPanel Implementation
    // =========================================================================
    MasterMeterPanel::MasterMeterPanel()
    {
        // Title Label
        panelTitleLabel.setText("Broadcast Audio Metering Station", juce::dontSendNotification);
        panelTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        panelTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
        addAndMakeVisible(panelTitleLabel);

        // Target Selector
        targetCombo.addItem("Target: Stream Bus (Master Out)", 1);
        targetCombo.addItem("Target: Monitor Bus (Phones)", 2);
        targetCombo.addItem("Target: Input 1 (Microphone)", 3);
        targetCombo.addItem("Target: Input 2 (Guitar / Inst)", 4);
        targetCombo.addItem("Target: Input 3 (Music Left)", 5);
        targetCombo.addItem("Target: Input 4 (Music Right)", 6);
        targetCombo.setSelectedId(1, juce::dontSendNotification);
        targetCombo.addListener(this);
        addAndMakeVisible(targetCombo);

        // Mode Selector
        modeCombo.addItem("Mode: Combined (VU + Peak)", 1);
        modeCombo.addItem("Mode: Analog VU Only", 2);
        modeCombo.addItem("Mode: Digital Peak Only", 3);
        modeCombo.setSelectedId(1, juce::dontSendNotification);
        modeCombo.addListener(this);
        addAndMakeVisible(modeCombo);

        // 0 VU Calibration
        calibrationCombo.addItem("0 VU = -18 dBFS (Broadcast/EBU)", 1);
        calibrationCombo.addItem("0 VU = -20 dBFS (SMPTE/Cinema)", 2);
        calibrationCombo.addItem("0 VU = -14 dBFS (Live/Music)", 3);
        calibrationCombo.addItem("0 VU = -12 dBFS (Hot Stream)", 4);
        calibrationCombo.setSelectedId(1, juce::dontSendNotification);
        calibrationCombo.addListener(this);
        addAndMakeVisible(calibrationCombo);

        // Peak Hold Button
        peakHoldBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        peakHoldBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00f5d4));
        peakHoldBtn.addListener(this);
        addAndMakeVisible(peakHoldBtn);

        // Reset Button
        resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d1217));
        resetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff87171));
        resetBtn.addListener(this);
        addAndMakeVisible(resetBtn);

        // Meters
        vuMeterL = std::make_unique<AnalogVUMeterComponent>("LEFT");
        vuMeterL->setResetCallback([this]() { if (activeProcessor) activeProcessor->resetClip(); });
        addAndMakeVisible(vuMeterL.get());

        vuMeterR = std::make_unique<AnalogVUMeterComponent>("RIGHT");
        vuMeterR->setResetCallback([this]() { if (activeProcessor) activeProcessor->resetClip(); });
        addAndMakeVisible(vuMeterR.get());

        peakMeterStereo = std::make_unique<DigitalPeakMeterComponent>(true, DigitalPeakMeterComponent::Orientation::Vertical);
        peakMeterStereo->setResetCallback([this]() {
            if (activeProcessor) {
                activeProcessor->resetPeakHold();
                activeProcessor->resetClip();
            }
        });
        addAndMakeVisible(peakMeterStereo.get());
    }

    void MasterMeterPanel::setTargetProcessor(LevelMeterProcessor* processor, const juce::String& targetName)
    {
        activeProcessor = processor;
        if (vuMeterL) vuMeterL->setChannelLabel(targetName + " L");
        if (vuMeterR) vuMeterR->setChannelLabel(targetName + " R");
    }

    void MasterMeterPanel::comboBoxChanged(juce::ComboBox* combo)
    {
        if (combo == &targetCombo)
        {
            int idx = targetCombo.getSelectedId() - 1;
            if (onTargetChanged)
                onTargetChanged(idx);
        }
        else if (combo == &modeCombo)
        {
            currentMode = (MeterMode)(modeCombo.getSelectedId() - 1);
            resized();
            repaint();
        }
        else if (combo == &calibrationCombo)
        {
            float ref = -18.0f;
            switch (calibrationCombo.getSelectedId())
            {
                case 1: ref = -18.0f; break;
                case 2: ref = -20.0f; break;
                case 3: ref = -14.0f; break;
                case 4: ref = -12.0f; break;
                default: ref = -18.0f; break;
            }
            if (activeProcessor)
                activeProcessor->setVuReference(ref);
            if (onCalibrationChanged)
                onCalibrationChanged(ref);
        }
    }

    void MasterMeterPanel::buttonClicked(juce::Button* button)
    {
        if (button == &peakHoldBtn)
        {
            if (activeProcessor)
            {
                const bool current = activeProcessor->isPeakHoldEnabled();
                activeProcessor->setPeakHoldEnabled(!current);
                peakHoldBtn.setButtonText(!current ? "Hold: ON" : "Hold: OFF");
                if (onPeakHoldToggled) onPeakHoldToggled(!current);
            }
        }
        else if (button == &resetBtn)
        {
            if (activeProcessor)
            {
                activeProcessor->resetPeakHold();
                activeProcessor->resetClip();
            }
            if (onResetRequested) onResetRequested();
        }
    }

    void MasterMeterPanel::updateMeters()
    {
        if (activeProcessor == nullptr)
            return;

        const auto snap = activeProcessor->getSnapshot();

        if (vuMeterL)
            vuMeterL->updateReading(snap.vuNormalizedL, snap.vuLevelL, snap.peakDbL, snap.clippedL);
        if (vuMeterR)
            vuMeterR->updateReading(snap.vuNormalizedR, snap.vuLevelR, snap.peakDbR, snap.clippedR);
        if (peakMeterStereo)
            peakMeterStereo->updateReading(snap);
    }

    void MasterMeterPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff111722));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xff222e42));
        g.drawRoundedRectangle(bounds, 6.0f, 1.2f);
    }

    void MasterMeterPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);

        // Header controls (height: 28px)
        auto headerArea = area.removeFromTop(26);
        panelTitleLabel.setBounds(headerArea.removeFromLeft(220));

        resetBtn.setBounds(headerArea.removeFromRight(80));
        headerArea.removeFromRight(6);
        peakHoldBtn.setBounds(headerArea.removeFromRight(76));
        headerArea.removeFromRight(6);
        calibrationCombo.setBounds(headerArea.removeFromRight(190));
        headerArea.removeFromRight(6);
        modeCombo.setBounds(headerArea.removeFromRight(170));
        headerArea.removeFromRight(6);
        targetCombo.setBounds(headerArea);

        area.removeFromTop(8); // spacing

        // Layout Meters based on currentMode
        if (currentMode == ModeCombined)
        {
            // Combined: Left 72% has dual VU meters, Right 28% has stereo Peak meter
            vuMeterL->setVisible(true);
            vuMeterR->setVisible(true);
            peakMeterStereo->setVisible(true);

            auto peakArea = area.removeFromRight(juce::jmax(65, (int)(area.getWidth() * 0.22f)));
            area.removeFromRight(8); // separator

            const int vuW = (area.getWidth() - 6) / 2;
            vuMeterL->setBounds(area.removeFromLeft(vuW));
            area.removeFromLeft(6);
            vuMeterR->setBounds(area);

            peakMeterStereo->setBounds(peakArea);
        }
        else if (currentMode == ModeAnalogVUOnly)
        {
            vuMeterL->setVisible(true);
            vuMeterR->setVisible(true);
            peakMeterStereo->setVisible(false);

            const int vuW = (area.getWidth() - 8) / 2;
            vuMeterL->setBounds(area.removeFromLeft(vuW));
            area.removeFromLeft(8);
            vuMeterR->setBounds(area);
        }
        else // ModeDigitalPeakOnly
        {
            vuMeterL->setVisible(false);
            vuMeterR->setVisible(false);
            peakMeterStereo->setVisible(true);

            // Centered wide digital peak meter
            int peakW = juce::jmin(180, area.getWidth());
            peakMeterStereo->setBounds(area.getCentreX() - peakW / 2, area.getY(), peakW, area.getHeight());
        }
    }
}
