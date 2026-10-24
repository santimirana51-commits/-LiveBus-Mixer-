#include "VocalDSPRackComponent.h"

namespace LiveBus
{
    // =========================================================================
    // Custom Hardware Rotary Look And Feel
    // =========================================================================
    void VocalDSPRackComponent::HardwareRotaryLookAndFeel::drawRotarySlider(
        juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer Dark Bezel
        g.setColour(juce::Colour(0xff0f141d));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff222b3a));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Arc Track Background
        const float arcRadius = radius - 3.5f;
        juce::Path bgArc;
        bgArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff161e2b));
        g.strokePath(bgArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Value Arc
        juce::Path activeArc;
        activeArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        auto arcColor = slider.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(arcColor);
        g.strokePath(activeArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Inner Metal Cap
        const float capRadius = radius - 8.0f;
        juce::ColourGradient capGrad(juce::Colour(0xff2e384d), center.x - capRadius * 0.5f, center.y - capRadius * 0.5f,
                                     juce::Colour(0xff131822), center.x + capRadius * 0.5f, center.y + capRadius * 0.5f, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
        g.setColour(juce::Colour(0xff3f4d66));
        g.drawEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // Pointer Needle / Line
        juce::Path p;
        p.addRectangle(-1.25f, -capRadius + 1.0f, 2.5f, capRadius * 0.65f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(center.x, center.y));
        g.setColour(juce::Colours::white);
        g.fillPath(p);
    }

    // =========================================================================
    // Dynamics Graph Visualizer
    // =========================================================================
    VocalDSPRackComponent::DynamicsGraphComponent::DynamicsGraphComponent(VocalDSPRackComponent& o)
        : owner(o) {}

    void VocalDSPRackComponent::DynamicsGraphComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0b0e14));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Subtle Grid (-80, -60, -40, -20, 0 dB)
        g.setColour(juce::Colour(0xff182230));
        const float dBs[] = { -80.0f, -60.0f, -40.0f, -20.0f, 0.0f };
        for (float db : dBs)
        {
            float nx = (db + 80.0f) / 80.0f;
            float px = bounds.getX() + nx * bounds.getWidth();
            g.drawVerticalLine((int)px, bounds.getY(), bounds.getBottom());

            float ny = (0.0f - db) / 80.0f;
            float py = bounds.getY() + ny * bounds.getHeight();
            g.drawHorizontalLine((int)py, bounds.getX(), bounds.getRight());
        }

        // 45-degree Unity Reference Line (dashed)
        g.setColour(juce::Colour(0xff2d3b4e));
        g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 1.0f);

        // Draw Transfer Curve
        const auto& proc = owner.getCurrentProcessor();
        juce::Path curvePath;
        const int numPts = (int)bounds.getWidth();
        for (int x = 0; x < numPts; ++x)
        {
            float inDb = -80.0f + ((float)x / (float)numPts) * 80.0f;
            float outDb = proc.getCompressionTransferDb(inDb);
            outDb = juce::jlimit(-80.0f, 10.0f, outDb);

            float py = bounds.getY() + (0.0f - outDb) / 80.0f * bounds.getHeight();
            if (x == 0)
                curvePath.startNewSubPath(bounds.getX() + (float)x, py);
            else
                curvePath.lineTo(bounds.getX() + (float)x, py);
        }

        g.setColour(juce::Colour(0xff38bdf8));
        g.strokePath(curvePath, juce::PathStrokeType(2.0f));

        // Threshold & Knee Marker Dot
        float threshDb = proc.params.compThresholdDb.load(std::memory_order_relaxed);
        float threshOutDb = proc.getCompressionTransferDb(threshDb);
        float tx = bounds.getX() + (threshDb + 80.0f) / 80.0f * bounds.getWidth();
        float ty = bounds.getY() + (0.0f - threshOutDb) / 80.0f * bounds.getHeight();

        g.setColour(juce::Colour(0xfff97316));
        g.fillEllipse(tx - 4.0f, ty - 4.0f, 8.0f, 8.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(tx - 4.0f, ty - 4.0f, 8.0f, 8.0f, 1.5f);

        // Current Live Operating Point (bouncing ball based on inPeakDb)
        float currentInDb = proc.getInputPeakDb();
        if (currentInDb > -75.0f)
        {
            float curOutDb = proc.getCompressionTransferDb(currentInDb);
            float cx = bounds.getX() + (currentInDb + 80.0f) / 80.0f * bounds.getWidth();
            float cy = bounds.getY() + (0.0f - curOutDb) / 80.0f * bounds.getHeight();

            g.setColour(juce::Colour(0xff00f5d4));
            g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
        }

        // Border
        g.setColour(juce::Colour(0xff222d3d));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    // =========================================================================
    // 8-Band EQ Graph Mini Visualizer
    // =========================================================================
    VocalDSPRackComponent::EQGraphMiniComponent::EQGraphMiniComponent(VocalDSPRackComponent& o)
        : owner(o) {}

    float VocalDSPRackComponent::EQGraphMiniComponent::freqToX(float freq) const
    {
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float logF = std::log10(juce::jlimit(20.0f, 20000.0f, freq));
        return getWidth() * (logF - logMin) / (logMax - logMin);
    }

    float VocalDSPRackComponent::EQGraphMiniComponent::xToFreq(float x) const
    {
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float frac = juce::jlimit(0.0f, 1.0f, x / (float)getWidth());
        return std::pow(10.0f, logMin + frac * (logMax - logMin));
    }

    float VocalDSPRackComponent::EQGraphMiniComponent::gainToY(float gainDb) const
    {
        float frac = (18.0f - juce::jlimit(-18.0f, 18.0f, gainDb)) / 36.0f;
        return frac * getHeight();
    }

    float VocalDSPRackComponent::EQGraphMiniComponent::yToGain(float y) const
    {
        float frac = 1.0f - (y / (float)getHeight());
        return -18.0f + frac * 36.0f;
    }

    void VocalDSPRackComponent::EQGraphMiniComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0b0e14));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Frequency Grid Lines (50, 100, 200, 500, 1k, 2k, 5k, 10k)
        g.setColour(juce::Colour(0xff182230));
        const float fGrid[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        for (float f : fGrid)
        {
            float x = freqToX(f);
            g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
        }

        // 0 dB center reference line
        float zeroY = gainToY(0.0f);
        g.setColour(juce::Colour(0xff2d3b4e));
        g.drawHorizontalLine((int)zeroY, bounds.getX(), bounds.getRight());

        // Composite Response Curve
        const auto& eq = owner.getCurrentProcessor().getEQ();
        juce::Path curvePath;
        const int numPts = getWidth();
        for (int x = 0; x < numPts; ++x)
        {
            float f = xToFreq((float)x);
            float gDb = eq.getMagnitudeForFrequency(f);
            float py = gainToY(gDb);

            if (x == 0)
                curvePath.startNewSubPath((float)x, py);
            else
                curvePath.lineTo((float)x, py);
        }

        // Gradient under curve
        juce::Path fillPath = curvePath;
        fillPath.lineTo(bounds.getRight(), zeroY);
        fillPath.lineTo(bounds.getX(), zeroY);
        fillPath.closeSubPath();

        juce::ColourGradient grad(juce::Colour(0x3300f5d4), 0.0f, zeroY - 20.0f,
                                 juce::Colour(0x0500f5d4), 0.0f, bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(fillPath);

        g.setColour(juce::Colour(0xff00f5d4));
        g.strokePath(curvePath, juce::PathStrokeType(1.5f));

        // Draggable Nodes for 8 Bands
        const juce::Colour bandColors[] = {
            juce::Colour(0xfff43f5e), juce::Colour(0xfffb923c), juce::Colour(0xfffacc15),
            juce::Colour(0xff4ade80), juce::Colour(0xff2dd4bf), juce::Colour(0xff38bdf8),
            juce::Colour(0xff818cf8), juce::Colour(0xffe879f9)
        };

        for (int b = 0; b < 8; ++b)
        {
            float f = eq.params.bands[b].freq.load(std::memory_order_relaxed);
            float gDb = eq.params.bands[b].gainDb.load(std::memory_order_relaxed);
            bool en = eq.params.bands[b].enabled.load(std::memory_order_relaxed);

            float bx = freqToX(f);
            float by = gainToY(gDb);

            g.setColour(en ? bandColors[b] : juce::Colour(0xff64748b));
            g.fillEllipse(bx - 4.0f, by - 4.0f, 8.0f, 8.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(bx - 4.0f, by - 4.0f, 8.0f, 8.0f, 1.0f);
        }

        g.setColour(juce::Colour(0xff222d3d));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void VocalDSPRackComponent::EQGraphMiniComponent::mouseDown(const juce::MouseEvent& e)
    {
        activeDragBand = -1;
        const auto& eq = owner.getCurrentProcessor().getEQ();
        for (int b = 0; b < 8; ++b)
        {
            float f = eq.params.bands[b].freq.load(std::memory_order_relaxed);
            float gDb = eq.params.bands[b].gainDb.load(std::memory_order_relaxed);
            float bx = freqToX(f);
            float by = gainToY(gDb);

            if (e.position.getDistanceFrom(juce::Point<float>(bx, by)) <= 12.0f)
            {
                activeDragBand = b;
                break;
            }
        }
    }

    void VocalDSPRackComponent::EQGraphMiniComponent::mouseDrag(const juce::MouseEvent& e)
    {
        if (activeDragBand >= 0 && activeDragBand < 8)
        {
            auto& eq = owner.getCurrentProcessor().getEQ();
            float newF = juce::jlimit(20.0f, 20000.0f, xToFreq(e.position.x));
            float newG = juce::jlimit(-18.0f, 18.0f, yToGain(e.position.y));

            eq.params.bands[activeDragBand].freq.store(newF);
            eq.params.bands[activeDragBand].gainDb.store(newG);
            repaint();
        }
    }

    void VocalDSPRackComponent::EQGraphMiniComponent::mouseUp(const juce::MouseEvent&)
    {
        activeDragBand = -1;
        repaint();
    }

    // =========================================================================
    // Vertical Segmented LED Ladder Meter
    // =========================================================================
    VocalDSPRackComponent::VerticalLedMeterComponent::VerticalLedMeterComponent(
        const juce::String& labelText, MeterType type, juce::Colour activeColor)
        : label(labelText), meterType(type), color(activeColor) {}

    void VocalDSPRackComponent::VerticalLedMeterComponent::setReading(float valueDb)
    {
        currentDb = valueDb;
        repaint();
    }

    void VocalDSPRackComponent::VerticalLedMeterComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Label at Top
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText(label, 0, 0, getWidth(), 12, juce::Justification::centred);

        auto meterRect = bounds.withTrimmedTop(14.0f).withTrimmedBottom(4.0f).reduced(2.0f);
        g.setColour(juce::Colour(0xff090c12));
        g.fillRoundedRectangle(meterRect, 2.0f);

        // Segmented LED steps (20 segments)
        const int numSegments = 20;
        const float segHeight = (meterRect.getHeight() - (numSegments - 1)) / (float)numSegments;

        for (int s = 0; s < numSegments; ++s)
        {
            float y = meterRect.getY() + s * (segHeight + 1.0f);
            float segFraction = (float)(numSegments - 1 - s) / (float)(numSegments - 1);

            bool isLit = false;
            if (meterType == MeterGainReduction)
            {
                // Gain reduction: currentDb is negative, e.g. 0 to -24 dB
                float targetReduction = -currentDb; // positive value
                float segDb = (float)s / (float)(numSegments - 1) * 24.0f;
                isLit = (targetReduction >= segDb && targetReduction > 0.2f);
            }
            else
            {
                // Level meter: -60 to 0 dBFS
                float segDb = -60.0f + segFraction * 60.0f;
                isLit = (currentDb >= segDb);
            }

            if (isLit)
            {
                juce::Colour segColor = color;
                if (meterType != MeterGainReduction && s < 2)
                    segColor = juce::Colour(0xffef4444); // Top red clip zone
                else if (meterType != MeterGainReduction && s < 6)
                    segColor = juce::Colour(0xffeab308); // Yellow warning zone
                g.setColour(segColor);
            }
            else
            {
                g.setColour(juce::Colour(0xff161e2a));
            }

            g.fillRect(meterRect.getX() + 1.0f, y, meterRect.getWidth() - 2.0f, segHeight);
        }

        g.setColour(juce::Colour(0xff222a3a));
        g.drawRoundedRectangle(meterRect, 2.0f, 1.0f);
    }

    // =========================================================================
    // VocalDSPRackComponent Implementation
    // =========================================================================
    VocalDSPRackComponent::VocalDSPRackComponent(std::array<VocalDSPProcessor*, NumTargets> processors)
        : targetProcessors(processors)
    {
        // 1. Top Header Controls
        gateMasterBtn.setClickingTogglesState(true);
        gateMasterBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1d3557));
        gateMasterBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2563eb));
        gateMasterBtn.setToggleState(true, juce::dontSendNotification);
        gateMasterBtn.addListener(this);
        addAndMakeVisible(gateMasterBtn);

        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0f6fc));
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        for (int i = 0; i < NumTargets; ++i)
        {
            const auto targetName = i < maxInputs
                ? "Channel " + juce::String(i + 1)
                : i == maxInputs ? "Stream Bus Master" : "Monitor Bus Master";
            targetCombo.addItem(targetName, i + 1);
        }
        targetCombo.setSelectedId(1, juce::dontSendNotification);
        targetCombo.addListener(this);
        addAndMakeVisible(targetCombo);

        const char* presetNames[] = {
            "Vocal Clean (Broadcast)", "Vocal Warmth / Speech", "Vocal Streaming / Rap",
            "Acoustic Guitar Clear", "Music Bus Glue", "Master Broadcast Limiter", "Reset / Flat"
        };
        for (int i = 0; i < 7; ++i)
            presetCombo.addItem(presetNames[i], i + 1);
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        presetCombo.addListener(this);
        addAndMakeVisible(presetCombo);

        compMasterBtn.setClickingTogglesState(true);
        compMasterBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7c2d12));
        compMasterBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffea580c));
        compMasterBtn.setToggleState(true, juce::dontSendNotification);
        compMasterBtn.addListener(this);
        addAndMakeVisible(compMasterBtn);

        // 2. Setup Noise Gate / Expander Controls
        addAndMakeVisible(gateGroup);
        setupKnob(gateOpenThreshSlider, gateOpenThreshLabel, "OPEN THRESH", -80.0, 0.0, -42.0, 0.5, " dB", juce::Colour(0xff3b82f6));
        setupKnob(gateCloseThreshSlider, gateCloseThreshLabel, "CLOSE (HYST)", -80.0, 0.0, -50.0, 0.5, " dB", juce::Colour(0xff3b82f6));

        gateModeBtn.setClickingTogglesState(false);
        gateModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
        gateModeBtn.addListener(this);
        addAndMakeVisible(gateModeBtn);

        expanderModeBtn.setClickingTogglesState(false);
        expanderModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
        expanderModeBtn.addListener(this);
        addAndMakeVisible(expanderModeBtn);

        setupKnob(gateAttackSlider, gateAttackLabel, "ATTACK", 0.1, 100.0, 1.0, 0.1, " ms", juce::Colour(0xff3b82f6));
        setupKnob(gateReleaseSlider, gateReleaseLabel, "RELEASE", 10.0, 1000.0, 100.0, 1.0, " ms", juce::Colour(0xff3b82f6));
        setupKnob(gateHoldSlider, gateHoldLabel, "HOLD", 0.0, 500.0, 20.0, 1.0, " ms", juce::Colour(0xff3b82f6));
        setupKnob(gateFloorSlider, gateFloorLabel, "FLOOR", -80.0, 0.0, -60.0, 0.5, " dB", juce::Colour(0xff3b82f6));

        gateKeyLabel.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        gateKeyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        addAndMakeVisible(gateKeyLabel);

        gateKeyIntBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
        gateKeyIntBtn.addListener(this);
        addAndMakeVisible(gateKeyIntBtn);

        gateKeyExtBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
        gateKeyExtBtn.addListener(this);
        addAndMakeVisible(gateKeyExtBtn);

        setupKnob(gateKeyHpfSlider, gateKeyHpfLabel, "HPF KEY", 20.0, 1000.0, 70.0, 1.0, " Hz", juce::Colour(0xff3b82f6));
        setupKnob(gateKeyLpfSlider, gateKeyLpfLabel, "LPF KEY", 200.0, 20000.0, 4000.0, 10.0, " Hz", juce::Colour(0xff3b82f6));

        // 3. Center Graphs & Vertical Segmented LED Meters
        gateGrMeter = std::make_unique<VerticalLedMeterComponent>("GATE GR", VerticalLedMeterComponent::MeterGainReduction, juce::Colour(0xff3b82f6));
        addAndMakeVisible(gateGrMeter.get());

        gateInMeter = std::make_unique<VerticalLedMeterComponent>("GATE IN", VerticalLedMeterComponent::MeterLevelIn, juce::Colour(0xff10b981));
        addAndMakeVisible(gateInMeter.get());

        dynamicsGraph = std::make_unique<DynamicsGraphComponent>(*this);
        addAndMakeVisible(dynamicsGraph.get());

        eqGraph = std::make_unique<EQGraphMiniComponent>(*this);
        addAndMakeVisible(eqGraph.get());

        compInMeter = std::make_unique<VerticalLedMeterComponent>("COMP IN", VerticalLedMeterComponent::MeterLevelIn, juce::Colour(0xff10b981));
        addAndMakeVisible(compInMeter.get());

        compGrMeter = std::make_unique<VerticalLedMeterComponent>("COMP GR", VerticalLedMeterComponent::MeterGainReduction, juce::Colour(0xffea580c));
        addAndMakeVisible(compGrMeter.get());

        // 4. Setup Compressor Controls
        addAndMakeVisible(compGroup);
        setupKnob(compThresholdSlider, compThresholdLabel, "THRESHOLD", -60.0, 0.0, -24.0, 0.5, " dB", juce::Colour(0xfff97316));
        setupKnob(compRatioSlider, compRatioLabel, "RATIO", 1.0, 20.0, 3.0, 0.1, ":1", juce::Colour(0xfff97316));

        kneeSoftBtn.addListener(this); addAndMakeVisible(kneeSoftBtn);
        kneeNormBtn.addListener(this); addAndMakeVisible(kneeNormBtn);
        kneeHardBtn.addListener(this); addAndMakeVisible(kneeHardBtn);
        kneeNormBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));

        setupKnob(compAttackSlider, compAttackLabel, "ATTACK", 0.1, 100.0, 10.0, 0.1, " ms", juce::Colour(0xfff97316));
        setupKnob(compReleaseSlider, compReleaseLabel, "RELEASE", 10.0, 1000.0, 120.0, 1.0, " ms", juce::Colour(0xfff97316));

        relManualBtn.addListener(this); addAndMakeVisible(relManualBtn);
        relAutoBtn.addListener(this); addAndMakeVisible(relAutoBtn);
        relManualBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));

        compKeyLabel.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        compKeyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        addAndMakeVisible(compKeyLabel);

        compKeyIntBtn.addListener(this); addAndMakeVisible(compKeyIntBtn);
        compKeyDeSBtn.addListener(this); addAndMakeVisible(compKeyDeSBtn);
        compKeyExtBtn.addListener(this); addAndMakeVisible(compKeyExtBtn);
        compKeyIntBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));

        setupKnob(compKeyHpfSlider, compKeyHpfLabel, "HPF KEY", 20.0, 500.0, 60.0, 1.0, " Hz", juce::Colour(0xfff97316));
        setupKnob(compKeyLpfSlider, compKeyLpfLabel, "LPF KEY", 2000.0, 20000.0, 15000.0, 10.0, " Hz", juce::Colour(0xfff97316));

        // 5. Setup Bottom Row Modules
        // LVL / HPF
        addAndMakeVisible(lvlGroup);
        lvlBypassBtn.setClickingTogglesState(true);
        lvlBypassBtn.setToggleState(true, juce::dontSendNotification);
        lvlBypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff06b6d4));
        lvlBypassBtn.addListener(this);
        addAndMakeVisible(lvlBypassBtn);
        setupKnob(inputGainSlider, inputGainLabel, "IN GAIN", -18.0, 18.0, 0.0, 0.1, " dB", juce::Colour(0xff06b6d4));
        setupKnob(hpfFreqSlider, hpfFreqLabel, "HPF FREQ", 20.0, 500.0, 80.0, 1.0, " Hz", juce::Colour(0xff06b6d4));

        hpfSlopeCombo.addItem("6 dB/Oct", 1);
        hpfSlopeCombo.addItem("12 dB/Oct", 2);
        hpfSlopeCombo.addItem("18 dB/Oct", 3);
        hpfSlopeCombo.addItem("24 dB/Oct", 4);
        hpfSlopeCombo.setSelectedId(2, juce::dontSendNotification);
        hpfSlopeCombo.addListener(this);
        addAndMakeVisible(hpfSlopeCombo);

        // De-Esser
        addAndMakeVisible(deEsserGroup);
        deEsserBypassBtn.setClickingTogglesState(true);
        deEsserBypassBtn.setToggleState(true, juce::dontSendNotification);
        deEsserBypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffa855f7));
        deEsserBypassBtn.addListener(this);
        addAndMakeVisible(deEsserBypassBtn);
        setupKnob(deEsserThreshSlider, deEsserThreshLabel, "THRESH", -40.0, 0.0, -16.0, 0.5, " dB", juce::Colour(0xffa855f7));
        setupKnob(deEsserFreqSlider, deEsserFreqLabel, "FREQ", 1000.0, 12000.0, 5500.0, 10.0, " Hz", juce::Colour(0xffa855f7));
        setupKnob(deEsserRangeSlider, deEsserRangeLabel, "RANGE", -24.0, 0.0, -8.0, 0.5, " dB", juce::Colour(0xffa855f7));
        deEsserTypeBtn.addListener(this);
        addAndMakeVisible(deEsserTypeBtn);
        deEsserGrMeter = std::make_unique<VerticalLedMeterComponent>("DeS GR", VerticalLedMeterComponent::MeterGainReduction, juce::Colour(0xffa855f7));
        addAndMakeVisible(deEsserGrMeter.get());

        // Limiter
        addAndMakeVisible(limGroup);
        limBypassBtn.setClickingTogglesState(true);
        limBypassBtn.setToggleState(true, juce::dontSendNotification);
        limBypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444));
        limBypassBtn.addListener(this);
        addAndMakeVisible(limBypassBtn);
        setupKnob(limCeilingSlider, limCeilingLabel, "CEILING", -20.0, 0.0, -1.0, 0.1, " dB", juce::Colour(0xffef4444));
        setupKnob(limReleaseSlider, limReleaseLabel, "RELEASE", 1.0, 500.0, 50.0, 1.0, " ms", juce::Colour(0xffef4444));
        limGrMeter = std::make_unique<VerticalLedMeterComponent>("LIM GR", VerticalLedMeterComponent::MeterGainReduction, juce::Colour(0xffef4444));
        addAndMakeVisible(limGrMeter.get());

        // Output
        addAndMakeVisible(outputGroup);
        setupKnob(compMixSlider, compMixLabel, "COMP MIX", 0.0, 100.0, 100.0, 1.0, "%", juce::Colour(0xff38bdf8));
        setupKnob(compMakeupSlider, compMakeupLabel, "MAKEUP", -12.0, 24.0, 3.0, 0.1, " dB", juce::Colour(0xfff97316));
        setupKnob(outputGainSlider, outputGainLabel, "OUT GAIN", -18.0, 18.0, 0.0, 0.1, " dB", juce::Colour(0xff00f5d4));
        outMeter = std::make_unique<VerticalLedMeterComponent>("OUTPUT", VerticalLedMeterComponent::MeterLevelOut, juce::Colour(0xff00f5d4));
        addAndMakeVisible(outMeter.get());

        clipLed.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        clipLed.setColour(juce::Label::backgroundColourId, juce::Colour(0xff161e2b));
        clipLed.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
        clipLed.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(clipLed);

        updateUIFromProcessor();
        startTimerHz(30);
    }

    VocalDSPRackComponent::~VocalDSPRackComponent()
    {
        stopTimer();
    }

    void VocalDSPRackComponent::setupKnob(
        juce::Slider& knob, juce::Label& label, const juce::String& text,
        double minVal, double maxVal, double defaultVal, double step,
        const juce::String& suffix, juce::Colour tint)
    {
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
        knob.setRange(minVal, maxVal, step);
        knob.setValue(defaultVal);
        knob.setTextValueSuffix(suffix);
        knob.setLookAndFeel(&knobLA);
        knob.setColour(juce::Slider::rotarySliderFillColourId, tint);
        knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff0f6fc));
        knob.addListener(this);
        addAndMakeVisible(knob);

        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    }

    void VocalDSPRackComponent::setSelectedTarget(TargetIndex target)
    {
        currentTarget = target;
        targetCombo.setSelectedId((int)target + 1, juce::dontSendNotification);
        updateUIFromProcessor();
    }

    void VocalDSPRackComponent::updateUIFromProcessor()
    {
        isUpdatingUI = true;
        auto& proc = getCurrentProcessor();

        // Header Bypasses
        gateMasterBtn.setToggleState(!proc.params.gateBypassed.load(std::memory_order_relaxed), juce::dontSendNotification);
        compMasterBtn.setToggleState(!proc.params.compBypassed.load(std::memory_order_relaxed), juce::dontSendNotification);

        // Gate
        gateOpenThreshSlider.setValue(proc.params.gateOpenThresholdDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateCloseThreshSlider.setValue(proc.params.gateCloseThresholdDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateAttackSlider.setValue(proc.params.gateAttackMs.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateReleaseSlider.setValue(proc.params.gateReleaseMs.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateHoldSlider.setValue(proc.params.gateHoldMs.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateFloorSlider.setValue(proc.params.gateFloorDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateKeyHpfSlider.setValue(proc.params.gateKeyHpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);
        gateKeyLpfSlider.setValue(proc.params.gateKeyLpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);

        bool isExp = (proc.params.gateMode.load(std::memory_order_relaxed) == VocalDSPParameters::ModeExpander);
        gateModeBtn.setColour(juce::TextButton::buttonColourId, isExp ? juce::Colour(0xff162030) : juce::Colour(0xff2563eb));
        expanderModeBtn.setColour(juce::TextButton::buttonColourId, isExp ? juce::Colour(0xff2563eb) : juce::Colour(0xff162030));

        // Compressor
        compThresholdSlider.setValue(proc.params.compThresholdDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        compRatioSlider.setValue(proc.params.compRatio.load(std::memory_order_relaxed), juce::dontSendNotification);
        compAttackSlider.setValue(proc.params.compAttackMs.load(std::memory_order_relaxed), juce::dontSendNotification);
        compReleaseSlider.setValue(proc.params.compReleaseMs.load(std::memory_order_relaxed), juce::dontSendNotification);
        compMakeupSlider.setValue(proc.params.compMakeupDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        compMixSlider.setValue(proc.params.compMixPercent.load(std::memory_order_relaxed), juce::dontSendNotification);
        compKeyHpfSlider.setValue(proc.params.compKeyHpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);
        compKeyLpfSlider.setValue(proc.params.compKeyLpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);

        int knee = proc.params.compKnee.load(std::memory_order_relaxed);
        kneeSoftBtn.setColour(juce::TextButton::buttonColourId, knee == 0 ? juce::Colour(0xffea580c) : juce::Colour(0xff162030));
        kneeNormBtn.setColour(juce::TextButton::buttonColourId, knee == 1 ? juce::Colour(0xffea580c) : juce::Colour(0xff162030));
        kneeHardBtn.setColour(juce::TextButton::buttonColourId, knee == 2 ? juce::Colour(0xffea580c) : juce::Colour(0xff162030));

        int relMode = proc.params.compReleaseMode.load(std::memory_order_relaxed);
        relManualBtn.setColour(juce::TextButton::buttonColourId, relMode == 0 ? juce::Colour(0xffea580c) : juce::Colour(0xff162030));
        relAutoBtn.setColour(juce::TextButton::buttonColourId, relMode == 1 ? juce::Colour(0xffea580c) : juce::Colour(0xff162030));

        // Bottom Modules
        lvlBypassBtn.setToggleState(!proc.params.hpfBypassed.load(std::memory_order_relaxed), juce::dontSendNotification);
        inputGainSlider.setValue(proc.params.inputGainDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        hpfFreqSlider.setValue(proc.params.hpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);
        hpfSlopeCombo.setSelectedId(proc.params.hpfSlope.load(std::memory_order_relaxed) + 1, juce::dontSendNotification);

        deEsserBypassBtn.setToggleState(!proc.params.deEsserBypassed.load(std::memory_order_relaxed), juce::dontSendNotification);
        deEsserThreshSlider.setValue(proc.params.deEsserThresholdDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        deEsserFreqSlider.setValue(proc.params.deEsserFreq.load(std::memory_order_relaxed), juce::dontSendNotification);
        deEsserRangeSlider.setValue(proc.params.deEsserRangeDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        bool isBell = (proc.params.deEsserType.load(std::memory_order_relaxed) == VocalDSPParameters::TypeBell);
        deEsserTypeBtn.setButtonText(isBell ? "BELL" : "SHELF");

        limBypassBtn.setToggleState(!proc.params.limiterBypassed.load(std::memory_order_relaxed), juce::dontSendNotification);
        limCeilingSlider.setValue(proc.params.limiterCeilingDb.load(std::memory_order_relaxed), juce::dontSendNotification);
        limReleaseSlider.setValue(proc.params.limiterReleaseMs.load(std::memory_order_relaxed), juce::dontSendNotification);

        outputGainSlider.setValue(proc.params.outputGainDb.load(std::memory_order_relaxed), juce::dontSendNotification);

        isUpdatingUI = false;
        dynamicsGraph->repaint();
        eqGraph->repaint();
    }

    void VocalDSPRackComponent::applyPreset(int presetIdx)
    {
        auto& proc = getCurrentProcessor();
        switch (presetIdx)
        {
            case 1: // Vocal Clean (Broadcast)
                proc.params.hpfFreq.store(80.0f);
                proc.params.gateOpenThresholdDb.store(-44.0f);
                proc.params.gateCloseThresholdDb.store(-52.0f);
                proc.params.compThresholdDb.store(-22.0f);
                proc.params.compRatio.store(3.0f);
                proc.params.compAttackMs.store(12.0f);
                proc.params.compReleaseMs.store(140.0f);
                proc.params.compMakeupDb.store(3.5f);
                proc.params.deEsserThresholdDb.store(-16.0f);
                proc.params.deEsserFreq.store(5600.0f);
                proc.params.limiterCeilingDb.store(-1.0f);
                break;

            case 2: // Vocal Warmth / Speech
                proc.params.hpfFreq.store(70.0f);
                proc.params.gateOpenThresholdDb.store(-46.0f);
                proc.params.gateCloseThresholdDb.store(-54.0f);
                proc.params.compThresholdDb.store(-26.0f);
                proc.params.compRatio.store(2.5f);
                proc.params.compAttackMs.store(15.0f);
                proc.params.compReleaseMs.store(180.0f);
                proc.params.compMakeupDb.store(4.0f);
                proc.params.deEsserThresholdDb.store(-18.0f);
                proc.params.deEsserFreq.store(4800.0f);
                proc.params.limiterCeilingDb.store(-1.5f);
                break;

            case 3: // Vocal Streaming / Rap
                proc.params.hpfFreq.store(95.0f);
                proc.params.gateOpenThresholdDb.store(-38.0f);
                proc.params.gateCloseThresholdDb.store(-46.0f);
                proc.params.compThresholdDb.store(-20.0f);
                proc.params.compRatio.store(4.5f);
                proc.params.compAttackMs.store(5.0f);
                proc.params.compReleaseMs.store(80.0f);
                proc.params.compMakeupDb.store(5.0f);
                proc.params.deEsserThresholdDb.store(-14.0f);
                proc.params.deEsserFreq.store(6200.0f);
                proc.params.limiterCeilingDb.store(-0.5f);
                break;

            case 4: // Acoustic Guitar Clear
                proc.params.hpfFreq.store(60.0f);
                proc.params.gateOpenThresholdDb.store(-50.0f);
                proc.params.gateCloseThresholdDb.store(-58.0f);
                proc.params.compThresholdDb.store(-24.0f);
                proc.params.compRatio.store(2.0f);
                proc.params.compAttackMs.store(25.0f);
                proc.params.compReleaseMs.store(150.0f);
                proc.params.compMakeupDb.store(2.0f);
                proc.params.deEsserBypassed.store(true);
                proc.params.limiterCeilingDb.store(-1.0f);
                break;

            case 5: // Music Bus Glue
                proc.params.hpfFreq.store(30.0f);
                proc.params.gateBypassed.store(true);
                proc.params.compThresholdDb.store(-16.0f);
                proc.params.compRatio.store(1.5f);
                proc.params.compAttackMs.store(30.0f);
                proc.params.compReleaseMs.store(250.0f);
                proc.params.compMakeupDb.store(1.5f);
                proc.params.deEsserBypassed.store(true);
                proc.params.limiterCeilingDb.store(-0.3f);
                break;

            case 6: // Master Broadcast Limiter
                proc.params.hpfFreq.store(25.0f);
                proc.params.gateBypassed.store(true);
                proc.params.compThresholdDb.store(-12.0f);
                proc.params.compRatio.store(2.0f);
                proc.params.compAttackMs.store(20.0f);
                proc.params.compReleaseMs.store(100.0f);
                proc.params.compMakeupDb.store(2.0f);
                proc.params.limiterCeilingDb.store(-0.5f);
                break;

            case 7: // Reset / Flat
            default:
                proc.reset();
                break;
        }
        updateUIFromProcessor();
    }

    void VocalDSPRackComponent::sliderValueChanged(juce::Slider* slider)
    {
        if (isUpdatingUI) return;
        auto& proc = getCurrentProcessor();

        if (slider == &gateOpenThreshSlider) proc.params.gateOpenThresholdDb.store((float)slider->getValue());
        else if (slider == &gateCloseThreshSlider) proc.params.gateCloseThresholdDb.store((float)slider->getValue());
        else if (slider == &gateAttackSlider) proc.params.gateAttackMs.store((float)slider->getValue());
        else if (slider == &gateReleaseSlider) proc.params.gateReleaseMs.store((float)slider->getValue());
        else if (slider == &gateHoldSlider) proc.params.gateHoldMs.store((float)slider->getValue());
        else if (slider == &gateFloorSlider) proc.params.gateFloorDb.store((float)slider->getValue());
        else if (slider == &gateKeyHpfSlider) proc.params.gateKeyHpfFreq.store((float)slider->getValue());
        else if (slider == &gateKeyLpfSlider) proc.params.gateKeyLpfFreq.store((float)slider->getValue());

        else if (slider == &compThresholdSlider) { proc.params.compThresholdDb.store((float)slider->getValue()); dynamicsGraph->repaint(); }
        else if (slider == &compRatioSlider) { proc.params.compRatio.store((float)slider->getValue()); dynamicsGraph->repaint(); }
        else if (slider == &compAttackSlider) proc.params.compAttackMs.store((float)slider->getValue());
        else if (slider == &compReleaseSlider) proc.params.compReleaseMs.store((float)slider->getValue());
        else if (slider == &compMakeupSlider) { proc.params.compMakeupDb.store((float)slider->getValue()); dynamicsGraph->repaint(); }
        else if (slider == &compMixSlider) proc.params.compMixPercent.store((float)slider->getValue());
        else if (slider == &compKeyHpfSlider) proc.params.compKeyHpfFreq.store((float)slider->getValue());
        else if (slider == &compKeyLpfSlider) proc.params.compKeyLpfFreq.store((float)slider->getValue());

        else if (slider == &inputGainSlider) proc.params.inputGainDb.store((float)slider->getValue());
        else if (slider == &hpfFreqSlider) proc.params.hpfFreq.store((float)slider->getValue());

        else if (slider == &deEsserThreshSlider) proc.params.deEsserThresholdDb.store((float)slider->getValue());
        else if (slider == &deEsserFreqSlider) proc.params.deEsserFreq.store((float)slider->getValue());
        else if (slider == &deEsserRangeSlider) proc.params.deEsserRangeDb.store((float)slider->getValue());

        else if (slider == &limCeilingSlider) proc.params.limiterCeilingDb.store((float)slider->getValue());
        else if (slider == &limReleaseSlider) proc.params.limiterReleaseMs.store((float)slider->getValue());
        else if (slider == &outputGainSlider) proc.params.outputGainDb.store((float)slider->getValue());
    }

    void VocalDSPRackComponent::buttonClicked(juce::Button* b)
    {
        auto& proc = getCurrentProcessor();
        if (b == &gateMasterBtn)
        {
            proc.params.gateBypassed.store(!gateMasterBtn.getToggleState());
        }
        else if (b == &compMasterBtn)
        {
            proc.params.compBypassed.store(!compMasterBtn.getToggleState());
        }
        else if (b == &gateModeBtn)
        {
            proc.params.gateMode.store(VocalDSPParameters::ModeGate);
            gateModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
            expanderModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
        }
        else if (b == &expanderModeBtn)
        {
            proc.params.gateMode.store(VocalDSPParameters::ModeExpander);
            expanderModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
            gateModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
        }
        else if (b == &kneeSoftBtn)
        {
            proc.params.compKnee.store(VocalDSPParameters::KneeSoft);
            kneeSoftBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));
            kneeNormBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
            kneeHardBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
            dynamicsGraph->repaint();
        }
        else if (b == &kneeNormBtn)
        {
            proc.params.compKnee.store(VocalDSPParameters::KneeNormal);
            kneeNormBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));
            kneeSoftBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
            kneeHardBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
            dynamicsGraph->repaint();
        }
        else if (b == &kneeHardBtn)
        {
            proc.params.compKnee.store(VocalDSPParameters::KneeHard);
            kneeHardBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));
            kneeSoftBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
            kneeNormBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
            dynamicsGraph->repaint();
        }
        else if (b == &relManualBtn)
        {
            proc.params.compReleaseMode.store(VocalDSPParameters::ReleaseManual);
            relManualBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));
            relAutoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
        }
        else if (b == &relAutoBtn)
        {
            proc.params.compReleaseMode.store(VocalDSPParameters::ReleaseAuto);
            relAutoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffea580c));
            relManualBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162030));
        }
        else if (b == &lvlBypassBtn)
        {
            proc.params.hpfBypassed.store(!lvlBypassBtn.getToggleState());
        }
        else if (b == &deEsserBypassBtn)
        {
            proc.params.deEsserBypassed.store(!deEsserBypassBtn.getToggleState());
        }
        else if (b == &deEsserTypeBtn)
        {
            bool isBell = (proc.params.deEsserType.load() == VocalDSPParameters::TypeBell);
            proc.params.deEsserType.store(isBell ? VocalDSPParameters::TypeShelf : VocalDSPParameters::TypeBell);
            deEsserTypeBtn.setButtonText(isBell ? "SHELF" : "BELL");
        }
        else if (b == &limBypassBtn)
        {
            proc.params.limiterBypassed.store(!limBypassBtn.getToggleState());
        }
    }

    void VocalDSPRackComponent::comboBoxChanged(juce::ComboBox* combo)
    {
        if (combo == &targetCombo)
        {
            setSelectedTarget((TargetIndex)(targetCombo.getSelectedId() - 1));
            if (onTargetSelected)
                onTargetSelected(currentTarget);
        }
        else if (combo == &presetCombo)
        {
            applyPreset(presetCombo.getSelectedId());
        }
        else if (combo == &hpfSlopeCombo)
        {
            getCurrentProcessor().params.hpfSlope.store(hpfSlopeCombo.getSelectedId() - 1);
        }
    }

    void VocalDSPRackComponent::timerCallback()
    {
        const auto& proc = getCurrentProcessor();

        // Update real-time Gain Reduction and Level Meters
        if (gateGrMeter) gateGrMeter->setReading(proc.getGateGainReductionDb());
        if (gateInMeter) gateInMeter->setReading(proc.getInputPeakDb());
        if (compInMeter) compInMeter->setReading(proc.getInputPeakDb());
        if (compGrMeter) compGrMeter->setReading(proc.getCompGainReductionDb());
        if (deEsserGrMeter) deEsserGrMeter->setReading(proc.getDeEsserGainReductionDb());
        if (limGrMeter) limGrMeter->setReading(proc.getLimiterGainReductionDb());
        if (outMeter) outMeter->setReading(proc.getOutputPeakDb());

        // Clip LED
        bool isClip = proc.isClipping();
        clipLed.setColour(juce::Label::backgroundColourId, isClip ? juce::Colour(0xffef4444) : juce::Colour(0xff161e2b));
        clipLed.setColour(juce::Label::textColourId, isClip ? juce::Colours::white : juce::Colour(0xffef4444));

        if (dynamicsGraph) dynamicsGraph->repaint();
    }

    void VocalDSPRackComponent::paint(juce::Graphics& g)
    {
        // Dark Hardware Console Background
        g.fillAll(juce::Colour(0xff0d1117));

        // Subtle gradient border around entire chassis
        g.setColour(juce::Colour(0xff1f293d));
        g.drawRect(getLocalBounds(), 1);
    }

    void VocalDSPRackComponent::resized()
    {
        const int w = getWidth();
        const int h = getHeight();

        // Top Header (y: 6..42)
        gateMasterBtn.setBounds(14, 8, 90, 28);
        titleLabel.setBounds(112, 10, w - 480, 24);
        targetCombo.setBounds(w - 360, 10, 160, 24);
        presetCombo.setBounds(w - 192, 10, 96, 24);
        compMasterBtn.setBounds(w - 90, 8, 76, 28);

        // Main Layout:
        // Left Column: Gate (w: 220)
        // Center Area: Visualizers & Meters (x: 236 to w - 240)
        // Right Column: Compressor (x: w - 232, w: 220)
        const int mainTop = 44;
        const int mainHeight = h - 220; // leaves 170px for bottom row

        // Left: Noise Gate Group
        const int gateW = 216;
        gateGroup.setBounds(10, mainTop, gateW, mainHeight);

        gateOpenThreshSlider.setBounds(18, mainTop + 24, 90, 70);
        gateOpenThreshLabel.setBounds(18, mainTop + 94, 90, 14);

        gateCloseThreshSlider.setBounds(120, mainTop + 24, 90, 70);
        gateCloseThreshLabel.setBounds(120, mainTop + 94, 90, 14);

        gateModeBtn.setBounds(30, mainTop + 114, 76, 22);
        expanderModeBtn.setBounds(120, mainTop + 114, 76, 22);

        gateAttackSlider.setBounds(18, mainTop + 142, 90, 68);
        gateAttackLabel.setBounds(18, mainTop + 210, 90, 14);

        gateReleaseSlider.setBounds(120, mainTop + 142, 90, 68);
        gateReleaseLabel.setBounds(120, mainTop + 210, 90, 14);

        gateHoldSlider.setBounds(18, mainTop + 228, 90, 68);
        gateHoldLabel.setBounds(18, mainTop + 296, 90, 14);

        gateFloorSlider.setBounds(120, mainTop + 228, 90, 68);
        gateFloorLabel.setBounds(120, mainTop + 296, 90, 14);

        gateKeyLabel.setBounds(20, mainTop + 316, 90, 16);
        gateKeyIntBtn.setBounds(112, mainTop + 316, 44, 20);
        gateKeyExtBtn.setBounds(160, mainTop + 316, 44, 20);

        gateKeyHpfSlider.setBounds(18, mainTop + 340, 90, 66);
        gateKeyHpfLabel.setBounds(18, mainTop + 406, 90, 14);

        gateKeyLpfSlider.setBounds(120, mainTop + 340, 90, 66);
        gateKeyLpfLabel.setBounds(120, mainTop + 406, 90, 14);

        // Right: Compressor Group
        const int compW = 216;
        const int compX = w - compW - 10;
        compGroup.setBounds(compX, mainTop, compW, mainHeight);

        compThresholdSlider.setBounds(compX + 8, mainTop + 24, 90, 70);
        compThresholdLabel.setBounds(compX + 8, mainTop + 94, 90, 14);

        compRatioSlider.setBounds(compX + 110, mainTop + 24, 90, 70);
        compRatioLabel.setBounds(compX + 110, mainTop + 94, 90, 14);

        kneeSoftBtn.setBounds(compX + 14, mainTop + 114, 58, 22);
        kneeNormBtn.setBounds(compX + 78, mainTop + 114, 60, 22);
        kneeHardBtn.setBounds(compX + 144, mainTop + 114, 58, 22);

        compAttackSlider.setBounds(compX + 8, mainTop + 142, 90, 68);
        compAttackLabel.setBounds(compX + 8, mainTop + 210, 90, 14);

        compReleaseSlider.setBounds(compX + 110, mainTop + 142, 90, 68);
        compReleaseLabel.setBounds(compX + 110, mainTop + 210, 90, 14);

        relManualBtn.setBounds(compX + 30, mainTop + 230, 76, 22);
        relAutoBtn.setBounds(compX + 114, mainTop + 230, 76, 22);

        compKeyLabel.setBounds(compX + 14, mainTop + 262, 80, 16);
        compKeyIntBtn.setBounds(compX + 96, mainTop + 262, 34, 20);
        compKeyDeSBtn.setBounds(compX + 134, mainTop + 262, 34, 20);
        compKeyExtBtn.setBounds(compX + 172, mainTop + 262, 34, 20);

        compKeyHpfSlider.setBounds(compX + 8, mainTop + 290, 90, 66);
        compKeyHpfLabel.setBounds(compX + 8, mainTop + 356, 90, 14);

        compKeyLpfSlider.setBounds(compX + 110, mainTop + 290, 90, 66);
        compKeyLpfLabel.setBounds(compX + 110, mainTop + 356, 90, 14);

        // Center Area: Flanking Meters & Visualizers
        const int centerX = gateW + 16;
        const int centerW = compX - centerX - 6;

        gateGrMeter->setBounds(centerX, mainTop + 6, 26, mainHeight - 12);
        gateInMeter->setBounds(centerX + 30, mainTop + 6, 26, mainHeight - 12);

        compInMeter->setBounds(compX - 60, mainTop + 6, 26, mainHeight - 12);
        compGrMeter->setBounds(compX - 32, mainTop + 6, 26, mainHeight - 12);

        const int graphX = centerX + 60;
        const int graphW = compX - 64 - graphX;
        const int singleGraphH = (mainHeight - 18) / 2;

        dynamicsGraph->setBounds(graphX, mainTop + 6, graphW, singleGraphH);
        eqGraph->setBounds(graphX, mainTop + 12 + singleGraphH, graphW, singleGraphH);

        // Bottom Row (4 Modules):
        // 1. LVL & HPF (w: 220)
        // 2. DeESSER (w: 300)
        // 3. LIMITER (w: 220)
        // 4. OUTPUT (remaining)
        const int botY = h - 170;
        const int botH = 160;

        // 1. LVL
        lvlGroup.setBounds(10, botY, 216, botH);
        lvlBypassBtn.setBounds(18, botY + 22, 44, 22);
        hpfSlopeCombo.setBounds(68, botY + 22, 148, 22);
        inputGainSlider.setBounds(18, botY + 50, 90, 80);
        inputGainLabel.setBounds(18, botY + 132, 90, 14);
        hpfFreqSlider.setBounds(116, botY + 50, 90, 80);
        hpfFreqLabel.setBounds(116, botY + 132, 90, 14);

        // 2. De-Esser
        const int deX = 232;
        const int deW = (w - 470) / 2;
        deEsserGroup.setBounds(deX, botY, deW, botH);
        deEsserBypassBtn.setBounds(deX + 8, botY + 22, 60, 22);
        deEsserTypeBtn.setBounds(deX + 74, botY + 22, 54, 22);

        deEsserThreshSlider.setBounds(deX + 8, botY + 50, 78, 80);
        deEsserThreshLabel.setBounds(deX + 8, botY + 132, 78, 14);

        deEsserFreqSlider.setBounds(deX + 90, botY + 50, 78, 80);
        deEsserFreqLabel.setBounds(deX + 90, botY + 132, 78, 14);

        deEsserRangeSlider.setBounds(deX + 172, botY + 50, 78, 80);
        deEsserRangeLabel.setBounds(deX + 172, botY + 132, 78, 14);

        deEsserGrMeter->setBounds(deX + deW - 36, botY + 18, 28, botH - 26);

        // 3. Limiter
        const int limX = deX + deW + 6;
        const int limW = deW;
        limGroup.setBounds(limX, botY, limW, botH);
        limBypassBtn.setBounds(limX + 8, botY + 22, 50, 22);

        limCeilingSlider.setBounds(limX + 14, botY + 50, 90, 80);
        limCeilingLabel.setBounds(limX + 14, botY + 132, 90, 14);

        limReleaseSlider.setBounds(limX + 116, botY + 50, 90, 80);
        limReleaseLabel.setBounds(limX + 116, botY + 132, 90, 14);

        limGrMeter->setBounds(limX + limW - 36, botY + 18, 28, botH - 26);

        // 4. Output
        const int outX = limX + limW + 6;
        const int outW = w - outX - 10;
        outputGroup.setBounds(outX, botY, outW, botH);

        compMixSlider.setBounds(outX + 4, botY + 50, 68, 80);
        compMixLabel.setBounds(outX + 4, botY + 132, 68, 14);

        compMakeupSlider.setBounds(outX + 74, botY + 50, 68, 80);
        compMakeupLabel.setBounds(outX + 74, botY + 132, 68, 14);

        outputGainSlider.setBounds(outX + 144, botY + 50, 68, 80);
        outputGainLabel.setBounds(outX + 144, botY + 132, 68, 14);

        outMeter->setBounds(outX + outW - 40, botY + 18, 24, botH - 42);
        clipLed.setBounds(outX + outW - 40, botY + botH - 22, 24, 16);
    }
}
