#include "PuzuDSPComponent.h"
#include <cmath>

// ==============================================================================
// PuzuCurveVisualizer Implementation
// ==============================================================================
PuzuDSPComponent::PuzuCurveVisualizer::PuzuCurveVisualizer(PuzuDSPComponent& o)
    : owner(o)
{
    setRepaintsOnMouseActivity(true);
}

float PuzuDSPComponent::PuzuCurveVisualizer::freqToX(float freq) const
{
    float w = (float)getWidth();
    float minF = 20.0f;
    float maxF = 20000.0f;
    float clamped = juce::jlimit(minF, maxF, freq);
    float norm = std::log10(clamped / minF) / std::log10(maxF / minF);
    return norm * w;
}

float PuzuDSPComponent::PuzuCurveVisualizer::xToFreq(float x) const
{
    float w = (float)getWidth();
    if (w <= 0.0f) return 1000.0f;
    float norm = juce::jlimit(0.0f, 1.0f, x / w);
    float minF = 20.0f;
    float maxF = 20000.0f;
    return minF * std::pow(maxF / minF, norm);
}

float PuzuDSPComponent::PuzuCurveVisualizer::gainToY(float gainDb) const
{
    float h = (float)getHeight();
    // Scale: +20dB at top (y=10), -20dB at bottom (y=h-15)
    float norm = (gainDb - (-20.0f)) / 40.0f;
    norm = juce::jlimit(0.0f, 1.0f, norm);
    return (h - 20.0f) - (norm * (h - 30.0f));
}

float PuzuDSPComponent::PuzuCurveVisualizer::yToGain(float y) const
{
    float h = (float)getHeight();
    if (h <= 30.0f) return 0.0f;
    float norm = ((h - 20.0f) - y) / (h - 30.0f);
    norm = juce::jlimit(0.0f, 1.0f, norm);
    return -20.0f + (norm * 40.0f);
}

void PuzuDSPComponent::PuzuCurveVisualizer::paint(juce::Graphics& g)
{
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // 1. Dark high-tech background
    g.fillAll(juce::Colour(0xff090d14));

    // Outer border
    g.setColour(juce::Colour(0xff1b2636));
    g.drawRect(getLocalBounds().toFloat(), 1.0f);

    // 2. Neon green logarithmic grid
    g.setColour(juce::Colour(0x2800e676));

    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : freqs)
    {
        float x = freqToX(f);
        g.drawVerticalLine((int)x, 0.0f, h - 18.0f);
    }

    const float dbs[] = { 20.0f, 10.0f, 0.0f, -10.0f, -20.0f };
    for (float db : dbs)
    {
        float y = gainToY(db);
        if (std::abs(db) < 0.1f)
            g.setColour(juce::Colour(0x5500e676)); // 0dB baseline brighter
        else
            g.setColour(juce::Colour(0x2200e676));
        g.drawHorizontalLine((int)y, 0.0f, w);
    }

    // 3. Grid text markers (dB labels on left, Hz labels at bottom)
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colour(0xff94a3b8));

    g.drawText("+20", 4, (int)gainToY(20.0f) - 6, 25, 12, juce::Justification::left);
    g.drawText("+10", 4, (int)gainToY(10.0f) - 6, 25, 12, juce::Justification::left);
    g.drawText("0", 4, (int)gainToY(0.0f) - 6, 25, 12, juce::Justification::left);
    g.drawText("-10", 4, (int)gainToY(-10.0f) - 6, 25, 12, juce::Justification::left);
    g.drawText("-20", 4, (int)gainToY(-20.0f) - 6, 25, 12, juce::Justification::left);

    const char* freqLabels[] = { "20", "50", "100", "200", "500", "1K", "2K", "5K", "10K", "20K" };
    for (size_t i = 0; i < 10; ++i)
    {
        float x = freqToX(freqs[i]);
        g.drawText(freqLabels[i], (int)x - 14, (int)h - 16, 28, 14, juce::Justification::centred);
    }

    owner.drawSpectrum(g, getLocalBounds().toFloat());

    // 4. Calculate real-time EQ & Filter response curve
    const auto& dsp = owner.getCurrentProcessor();
    juce::Path curvePath;
    float startY = gainToY(dsp.getMagnitudeForFrequency(20.0f));
    curvePath.startNewSubPath(0.0f, startY);

    const int numPoints = 180;
    for (int i = 1; i <= numPoints; ++i)
    {
        float x = (float)i / (float)numPoints * w;
        float f = xToFreq(x);
        float magDb = dsp.getMagnitudeForFrequency(f);
        float y = gainToY(magDb);
        curvePath.lineTo(x, y);
    }

    // Gradient fill under the curve
    juce::Path fillPath = curvePath;
    fillPath.lineTo(w, gainToY(-20.0f));
    fillPath.lineTo(0.0f, gainToY(-20.0f));
    fillPath.closeSubPath();

    juce::ColourGradient fillGrad(juce::Colour(0x35d926b8), 0.0f, gainToY(10.0f),
                                 juce::Colour(0x05d926b8), 0.0f, gainToY(-20.0f), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // Glowing response curve line (Bright Magenta / Violet)
    g.setColour(juce::Colour(0x60e040d0));
    g.strokePath(curvePath, juce::PathStrokeType(4.0f));
    g.setColour(juce::Colour(0xffe040d0));
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // 5. Draw HP and LP filter cutoff indicators ('H' and 'L' markers)
    float hpF = dsp.params.hpfFreq.load(std::memory_order_relaxed);
    float hpX = freqToX(hpF);
    g.setColour(juce::Colour(0xffffca28)); // Yellow marker
    g.drawText("H", (int)hpX - 6, (int)gainToY(-14.0f), 14, 14, juce::Justification::centred);

    float lpF = dsp.params.lpfFreq.load(std::memory_order_relaxed);
    if (dsp.params.lpfSlope.load(std::memory_order_relaxed) > 0)
    {
        float lpX = freqToX(lpF);
        g.setColour(juce::Colour(0xffffca28));
        g.drawText("L", (int)lpX - 6, (int)gainToY(-14.0f), 14, 14, juce::Justification::centred);
    }

    // 6. Draw 15 Interactive Node Badges (1..15)
    for (int b = 0; b < numPuzuBands; ++b)
    {
        float f = dsp.params.puzuBands[b].freq.load(std::memory_order_relaxed);
        float gDb = dsp.params.puzuBands[b].gainDb.load(std::memory_order_relaxed);
        float nx = freqToX(f);
        float ny = gainToY(gDb);

        bool isHovered = (hoveredBand == b || activeDragBand == b);

        // Node circle background
        g.setColour(isHovered ? juce::Colour(0xffff1744) : juce::Colour(0xff1e293b));
        g.fillEllipse(nx - 7.0f, ny - 7.0f, 14.0f, 14.0f);

        // Node border
        g.setColour(isHovered ? juce::Colours::white : juce::Colour(0xffe040d0));
        g.drawEllipse(nx - 7.0f, ny - 7.0f, 14.0f, 14.0f, 1.5f);

        // Band number text
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(b + 1), (int)nx - 7, (int)ny - 7, 14, 14, juce::Justification::centred);

        // Tooltip overlay when active
        if (isHovered)
        {
            float q = dsp.params.puzuBands[b].q.load(std::memory_order_relaxed);
            juce::String tip = "F:" + juce::String(f, 1) + " Q:" + juce::String(q, 2) + " G:" + juce::String(gDb, 1);
            float tipW = 120.0f;
            float tipH = 18.0f;
            float tipX = juce::jlimit(4.0f, w - tipW - 4.0f, nx - tipW * 0.5f);
            float tipY = juce::jmax(4.0f, ny - 24.0f);

            g.setColour(juce::Colour(0xee0f172a));
            g.fillRoundedRectangle(tipX, tipY, tipW, tipH, 4.0f);
            g.setColour(juce::Colour(0xffe040d0));
            g.drawRoundedRectangle(tipX, tipY, tipW, tipH, 4.0f, 1.0f);

            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.setColour(juce::Colours::white);
            g.drawText(tip, (int)tipX, (int)tipY, (int)tipW, (int)tipH, juce::Justification::centred);
        }
    }
}

void PuzuDSPComponent::PuzuCurveVisualizer::mouseDown(const juce::MouseEvent& e)
{
    activeDragBand = -1;
    const auto& dsp = owner.getCurrentProcessor();

    for (int b = 0; b < numPuzuBands; ++b)
    {
        float f = dsp.params.puzuBands[b].freq.load(std::memory_order_relaxed);
        float gDb = dsp.params.puzuBands[b].gainDb.load(std::memory_order_relaxed);
        float nx = freqToX(f);
        float ny = gainToY(gDb);

        if (std::hypot(e.x - nx, e.y - ny) < 12.0f)
        {
            activeDragBand = b;
            repaint();
            break;
        }
    }
}

void PuzuDSPComponent::PuzuCurveVisualizer::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDragBand < 0 || activeDragBand >= numPuzuBands) return;

    auto& dsp = owner.getCurrentProcessor();

    // Adjust gain via vertical drag
    float newGain = juce::jlimit(-20.0f, 20.0f, yToGain((float)e.y));
    dsp.params.puzuBands[activeDragBand].gainDb.store(newGain, std::memory_order_relaxed);

    // Adjust frequency via horizontal drag (within bounds)
    float newFreq = juce::jlimit(20.0f, 20000.0f, xToFreq((float)e.x));
    dsp.params.puzuBands[activeDragBand].freq.store(newFreq, std::memory_order_relaxed);

    dsp.updatePuzuCoefficients();
    owner.storeActiveABSlot();
    owner.updateUIFromProcessor();
    repaint();
}

void PuzuDSPComponent::PuzuCurveVisualizer::mouseUp(const juce::MouseEvent&)
{
    activeDragBand = -1;
    repaint();
}

void PuzuDSPComponent::PuzuCurveVisualizer::mouseMove(const juce::MouseEvent& e)
{
    const auto& dsp = owner.getCurrentProcessor();
    int prevHovered = hoveredBand;
    hoveredBand = -1;

    for (int b = 0; b < numPuzuBands; ++b)
    {
        float f = dsp.params.puzuBands[b].freq.load(std::memory_order_relaxed);
        float gDb = dsp.params.puzuBands[b].gainDb.load(std::memory_order_relaxed);
        float nx = freqToX(f);
        float ny = gainToY(gDb);

        if (std::hypot(e.x - nx, e.y - ny) < 12.0f)
        {
            hoveredBand = b;
            break;
        }
    }

    if (hoveredBand != prevHovered)
        repaint();
}

void PuzuDSPComponent::PuzuCurveVisualizer::mouseExit(const juce::MouseEvent&)
{
    if (hoveredBand != -1)
    {
        hoveredBand = -1;
        repaint();
    }
}

// ==============================================================================
// PuzuDSPComponent Main Implementation
// ==============================================================================
PuzuDSPComponent::PuzuDSPComponent(
    std::array<BusDSPProcessor*, maxInputs + 2> processors,
    LiveBus::PresetManager& presets)
    : targetProcessors(processors), presetManager(presets)
{
    // 1. Brand Logo & Header
    brandLabel.setText("PUZU", juce::dontSendNotification);
    brandLabel.setFont(juce::FontOptions(26.0f, juce::Font::bold | juce::Font::italic));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe53935)); // Red PUZU Logo
    addAndMakeVisible(brandLabel);

    subBrandLabel.setText("Inspired Audio DSP Tuning", juce::dontSendNotification);
    subBrandLabel.setFont(juce::FontOptions(11.0f));
    subBrandLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(subBrandLabel);

    for (int channel = 0; channel < maxInputs; ++channel)
        targetCombo.addItem("Input " + juce::String(channel + 1), channel + 1);
    targetCombo.addItem("Stream Bus", maxInputs + 1);
    targetCombo.addItem("Monitor Bus", maxInputs + 2);
    targetCombo.setSelectedId(maxInputs + 1, juce::dontSendNotification);
    targetCombo.addListener(this);
    addAndMakeVisible(targetCombo);
    presetCombo.setTooltip(
        "Apply a factory or user EQ Catalog preset to the selected 15-band target");
    presetCombo.addListener(this);
    addAndMakeVisible(presetCombo);
    refreshPresetCatalog();

    for (auto* toggle : { &analyzerEnabledToggle, &preSpectrumToggle, &postSpectrumToggle })
    {
        toggle->addListener(this);
        addAndMakeVisible(toggle);
    }
    analyzerEnabledToggle.setToggleState(true, juce::dontSendNotification);
    preSpectrumToggle.setToggleState(true, juce::dontSendNotification);
    postSpectrumToggle.setToggleState(true, juce::dontSendNotification);
    preSpectrumDb.fill(-90.0f);
    preRightSpectrumDb.fill(-90.0f);
    postSpectrumDb.fill(-90.0f);
    postRightSpectrumDb.fill(-90.0f);
    prePeakHoldDb.fill(-90.0f);
    preRightPeakHoldDb.fill(-90.0f);
    postPeakHoldDb.fill(-90.0f);
    postRightPeakHoldDb.fill(-90.0f);
    peakHoldToggle.setTooltip("Hold spectrum peaks and let them decay over time");
    peakHoldToggle.addListener(this);
    addAndMakeVisible(peakHoldToggle);
    spectrumModeCombo.addItem("Mono", 1);
    spectrumModeCombo.addItem("Stereo", 2);
    spectrumModeCombo.setSelectedId(1, juce::dontSendNotification);
    spectrumModeCombo.setTooltip(
        "Mono averages left and right; Stereo shows each channel separately");
    spectrumModeCombo.addListener(this);
    addAndMakeVisible(spectrumModeCombo);
    fftSizeCombo.addItem("FFT 1024", 1);
    fftSizeCombo.addItem("FFT 2048", 2);
    fftSizeCombo.addItem("FFT 4096", 3);
    fftSizeCombo.setSelectedId(2, juce::dontSendNotification);
    fftSizeCombo.setTooltip(
        "Larger FFT sizes improve low-frequency resolution but update less often");
    fftSizeCombo.addListener(this);
    addAndMakeVisible(fftSizeCombo);
    averagingTimeCombo.addItem("Avg 50 ms", 1);
    averagingTimeCombo.addItem("Avg 250 ms", 2);
    averagingTimeCombo.addItem("Avg 1 s", 3);
    averagingTimeCombo.setSelectedId(2, juce::dontSendNotification);
    averagingTimeCombo.setTooltip(
        "Spectrum averaging time: shorter reacts faster; longer is smoother");
    averagingTimeCombo.addListener(this);
    addAndMakeVisible(averagingTimeCombo);

    // PEQ Mode, RST, Pass, Sys Set buttons
    peqModeBtn.setButtonText("PEQ Mode");
    peqModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1976d2));
    peqModeBtn.addListener(this);
    addAndMakeVisible(peqModeBtn);

    rstBtn.setButtonText("RST");
    rstBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    rstBtn.addListener(this);
    addAndMakeVisible(rstBtn);

    passBtn.setButtonText("Pass");
    passBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
    passBtn.addListener(this);
    addAndMakeVisible(passBtn);

    sysSetBtn.setButtonText("Sys Set");
    sysSetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
    sysSetBtn.addListener(this);
    addAndMakeVisible(sysSetBtn);

    for (auto* button : { &slotAButton, &slotBButton, &copyEQButton, &pasteEQButton })
    {
        button->addListener(this);
        addAndMakeVisible(*button);
    }
    slotAButton.setTooltip("Select EQ slot A; edits are stored in the active slot");
    slotBButton.setTooltip("Select EQ slot B; edits are stored in the active slot");
    copyEQButton.setTooltip("Copy this target's 15-band EQ configuration");
    pasteEQButton.setTooltip("Paste the copied 15-band EQ configuration to this target");
    pasteEQButton.setEnabled(false);

    for (int band = 0; band < numPuzuBands; ++band)
        selectedBandCombo.addItem("Band " + juce::String(band + 1), band + 1);
    selectedBandCombo.setSelectedId(1, juce::dontSendNotification);
    selectedBandCombo.addListener(this);
    addAndMakeVisible(selectedBandCombo);

    const std::array<const char*, 6> filterNames{
        "High-pass", "Low shelf", "Bell", "High shelf", "Low-pass", "Notch"
    };
    for (int type = 0; type < (int)filterNames.size(); ++type)
        filterTypeCombo.addItem(filterNames[(size_t)type], type + 1);
    filterTypeCombo.addListener(this);
    addAndMakeVisible(filterTypeCombo);

    bandEnabledToggle.addListener(this);
    addAndMakeVisible(bandEnabledToggle);

    bandFrequencySlider.setSliderStyle(juce::Slider::LinearBar);
    bandFrequencySlider.setRange(20.0, 20000.0, 1.0);
    bandFrequencySlider.setTextValueSuffix(" Hz");
    bandFrequencySlider.addListener(this);
    addAndMakeVisible(bandFrequencySlider);

    bandQSlider.setSliderStyle(juce::Slider::LinearBar);
    bandQSlider.setRange(0.1, 10.0, 0.01);
    bandQSlider.setTextValueSuffix(" Q");
    bandQSlider.addListener(this);
    addAndMakeVisible(bandQSlider);

    // 2. Center Curve Visualizer
    curveVisualizer = std::make_unique<PuzuCurveVisualizer>(*this);
    addAndMakeVisible(*curveVisualizer);
    curveVisualizer->toBack();

    // 3. Left Controls: HP Filter
    hpHeader.setText("HP Filter", juce::dontSendNotification);
    hpHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    hpHeader.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(hpHeader);

    hpFreqLabel.setText("Fre:", juce::dontSendNotification);
    hpFreqLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(hpFreqLabel);

    hpFreqSlider.setSliderStyle(juce::Slider::LinearBar);
    hpFreqSlider.setRange(20.0, 2000.0, 0.1);
    hpFreqSlider.setValue(40.1);
    hpFreqSlider.setTextValueSuffix(" Hz");
    hpFreqSlider.addListener(this);
    addAndMakeVisible(hpFreqSlider);

    hpTypeCombo.addItem("Butter", 1);
    hpTypeCombo.addItem("Bessel", 2);
    hpTypeCombo.addItem("Linkw", 3);
    hpTypeCombo.setSelectedId(1, juce::dontSendNotification);
    hpTypeCombo.addListener(this);
    addAndMakeVisible(hpTypeCombo);

    hpSlopeCombo.addItem("6dB/Oct", 1);
    hpSlopeCombo.addItem("12dB/Oct", 2);
    hpSlopeCombo.addItem("18dB/Oct", 3);
    hpSlopeCombo.addItem("24dB/Oct", 4);
    hpSlopeCombo.setSelectedId(2, juce::dontSendNotification);
    hpSlopeCombo.addListener(this);
    addAndMakeVisible(hpSlopeCombo);

    // LP Filter
    lpHeader.setText("LP Filter", juce::dontSendNotification);
    lpHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    lpHeader.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lpHeader);

    lpFreqLabel.setText("Fre:", juce::dontSendNotification);
    lpFreqLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(lpFreqLabel);

    lpFreqSlider.setSliderStyle(juce::Slider::LinearBar);
    lpFreqSlider.setRange(1000.0, 22000.0, 1.0);
    lpFreqSlider.setValue(20600.0);
    lpFreqSlider.setTextValueSuffix(" Hz");
    lpFreqSlider.addListener(this);
    addAndMakeVisible(lpFreqSlider);

    lpTypeCombo.addItem("Butter", 1);
    lpTypeCombo.addItem("Bessel", 2);
    lpTypeCombo.addItem("Linkw", 3);
    lpTypeCombo.setSelectedId(1, juce::dontSendNotification);
    lpTypeCombo.addListener(this);
    addAndMakeVisible(lpTypeCombo);

    lpSlopeCombo.addItem("OFF", 1);
    lpSlopeCombo.addItem("6dB/Oct", 2);
    lpSlopeCombo.addItem("12dB/Oct", 3);
    lpSlopeCombo.addItem("18dB/Oct", 4);
    lpSlopeCombo.addItem("24dB/Oct", 5);
    lpSlopeCombo.setSelectedId(1, juce::dontSendNotification);
    lpSlopeCombo.addListener(this);
    addAndMakeVisible(lpSlopeCombo);

    // Limiter
    limiterHeader.setText("Limiter", juce::dontSendNotification);
    limiterHeader.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    limiterHeader.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(limiterHeader);

    limiterDbuSlider.setSliderStyle(juce::Slider::LinearBar);
    limiterDbuSlider.setRange(-30.0, 0.0, 0.1);
    limiterDbuSlider.setValue(-30.0);
    limiterDbuSlider.setTextValueSuffix(" dBu");
    limiterDbuSlider.addListener(this);
    addAndMakeVisible(limiterDbuSlider);

    limiterAtLabel.setText("At.Time: 0.3ms", juce::dontSendNotification);
    limiterAtLabel.setFont(juce::FontOptions(10.0f));
    limiterAtLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(limiterAtLabel);

    limiterReLabel.setText("Re.Time: Atk*2", juce::dontSendNotification);
    limiterReLabel.setFont(juce::FontOptions(10.0f));
    limiterReLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(limiterReLabel);

    // 4. Setup 15 EQ Band Channel Strips
    for (int b = 0; b < numPuzuBands; ++b)
    {
        auto& ui = bandUIs[(size_t)b];

        ui.noLabel.setText(juce::String(b + 1), juce::dontSendNotification);
        ui.noLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        ui.noLabel.setJustificationType(juce::Justification::centred);
        ui.noLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(ui.noLabel);

        ui.freqLabel.setText(juce::String((int)defaultPuzuFreqs[b]), juce::dontSendNotification);
        ui.freqLabel.setFont(juce::FontOptions(9.0f));
        ui.freqLabel.setJustificationType(juce::Justification::centred);
        ui.freqLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(ui.freqLabel);

        ui.qLabel.setText(juce::String(defaultPuzuQs[b], 2), juce::dontSendNotification);
        ui.qLabel.setFont(juce::FontOptions(9.0f));
        ui.qLabel.setJustificationType(juce::Justification::centred);
        ui.qLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(ui.qLabel);

        ui.gainLabel.setText("0.0", juce::dontSendNotification);
        ui.gainLabel.setFont(juce::FontOptions(9.0f));
        ui.gainLabel.setJustificationType(juce::Justification::centred);
        ui.gainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(ui.gainLabel);

        ui.fader.setSliderStyle(juce::Slider::LinearVertical);
        ui.fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ui.fader.setRange(-20.0, 20.0, 0.1);
        ui.fader.setValue(0.0);
        ui.fader.setColour(juce::Slider::trackColourId, juce::Colour(0xffdc2626)); // Red vertical groove
        ui.fader.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe2e8f0)); // Silver metallic thumb
        ui.fader.addListener(this);
        addAndMakeVisible(ui.fader);
    }

    // 5. Master Volume & Delay Controls
    masterVolLabel.setText("Volume", juce::dontSendNotification);
    masterVolLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    masterVolLabel.setJustificationType(juce::Justification::centred);
    masterVolLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(masterVolLabel);

    masterVolSlider.setSliderStyle(juce::Slider::LinearVertical);
    masterVolSlider.setRange(0.0, 1.5, 0.01);
    masterVolSlider.setValue(1.0);
    masterVolSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 14);
    masterVolSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xffdc2626));
    masterVolSlider.addListener(this);
    addAndMakeVisible(masterVolSlider);

    delayUnitCombo.addItem("CM", 1);
    delayUnitCombo.addItem("MS", 2);
    delayUnitCombo.addItem("INCH", 3);
    delayUnitCombo.setSelectedId(1, juce::dontSendNotification);
    delayUnitCombo.addListener(this);
    addAndMakeVisible(delayUnitCombo);

    // 6. Bottom Output Channel Strips (Front Left, Front Right, etc.)
    const char* outNames[numOutputs] = { "Front Left", "Front Right", "Rear Left", "Rear Right", "Center", "Sub" };
    for (int i = 0; i < numOutputs; ++i)
    {
        auto& ui = outputUIs[(size_t)i];

        ui.tabBtn.setButtonText(outNames[i]);
        ui.tabBtn.setColour(juce::TextButton::buttonColourId, (i < 2) ? juce::Colour(0xff1d4ed8) : juce::Colour(0xff1e293b));
        ui.tabBtn.addListener(this);
        addAndMakeVisible(ui.tabBtn);

        ui.fader.setSliderStyle(juce::Slider::LinearVertical);
        ui.fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        ui.fader.setRange(0.0, 1.5, 0.01);
        ui.fader.setValue(1.0);
        ui.fader.setColour(juce::Slider::trackColourId, juce::Colour(0xffdc2626));
        ui.fader.addListener(this);
        addAndMakeVisible(ui.fader);

        ui.phaseBtn.setButtonText("0'");
        ui.phaseBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2563eb));
        ui.phaseBtn.addListener(this);
        addAndMakeVisible(ui.phaseBtn);

        ui.muteBtn.setButtonText("MUTE");
        ui.muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
        ui.muteBtn.addListener(this);
        addAndMakeVisible(ui.muteBtn);

        ui.gainLabel.setText("0dB", juce::dontSendNotification);
        ui.gainLabel.setFont(juce::FontOptions(10.0f));
        ui.gainLabel.setJustificationType(juce::Justification::centred);
        ui.gainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(ui.gainLabel);
    }

    // Grouping Toggles
    for (auto* t : { &groupFrontToggle, &groupRearToggle, &groupAllToggle, &groupBassToggle })
    {
        t->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
        addAndMakeVisible(t);
    }
    groupFrontToggle.setButtonText("Grouping Front");
    groupRearToggle.setButtonText("Grouping Rear");
    groupAllToggle.setButtonText("Grouping All");
    groupBassToggle.setButtonText("Grouping Bass");
    groupFrontToggle.setToggleState(true, juce::dontSendNotification);

    initializeABForSelectedTarget();
    updateUIFromProcessor();
    preSpectrumDb.fill(-90.0f);
    preRightSpectrumDb.fill(-90.0f);
    postSpectrumDb.fill(-90.0f);
    postRightSpectrumDb.fill(-90.0f);
    getCurrentProcessor().setPuzuAnalyzerEnabled(true);
    startTimerHz(30);
}

PuzuDSPComponent::~PuzuDSPComponent()
{
    stopTimer();
    getCurrentProcessor().setPuzuAnalyzerEnabled(false);
}

void PuzuDSPComponent::paint(juce::Graphics& g)
{
    // High-tech dark metallic body
    g.fillAll(juce::Colour(0xff0b0f17));

    // Decorative frame panels
    g.setColour(juce::Colour(0xff161f2e));
    g.drawRoundedRectangle(10.0f, 6.0f, (float)getWidth() - 20.0f, 320.0f, 6.0f, 1.0f);
    g.drawRoundedRectangle(10.0f, 335.0f, (float)getWidth() - 20.0f, (float)getHeight() - 345.0f, 6.0f, 1.0f);

    // Green LED indicators below the 15 faders
    for (int b = 0; b < numPuzuBands; ++b)
    {
        auto bounds = bandUIs[(size_t)b].fader.getBounds();
        float cx = (float)bounds.getCentreX();
        float cy = (float)bounds.getBottom() + 6.0f;

        g.setColour(juce::Colour(0xff00e676));
        g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }
}

void PuzuDSPComponent::resized()
{
    // Header & Logo
    brandLabel.setBounds(18, 10, 110, 28);
    subBrandLabel.setBounds(18, 38, 140, 16);
    targetCombo.setBounds(170, 14, 180, 26);
    presetCombo.setBounds(360, 14, 250, 26);

    // Right header buttons
    int rx = getWidth() - 280;
    peqModeBtn.setBounds(rx, 14, 75, 26);
    rstBtn.setBounds(rx + 80, 14, 45, 26);
    passBtn.setBounds(rx + 130, 14, 45, 26);
    sysSetBtn.setBounds(rx + 180, 14, 65, 26);

    // Left Filter & Dynamics Panel
    int lx = 18;
    int ly = 60;
    hpHeader.setBounds(lx, ly, 110, 18);
    hpFreqLabel.setBounds(lx, ly + 20, 30, 20);
    hpFreqSlider.setBounds(lx + 32, ly + 20, 78, 20);
    hpTypeCombo.setBounds(lx, ly + 44, 52, 20);
    hpSlopeCombo.setBounds(lx + 56, ly + 44, 54, 20);

    ly += 72;
    lpHeader.setBounds(lx, ly, 110, 18);
    lpFreqLabel.setBounds(lx, ly + 20, 30, 20);
    lpFreqSlider.setBounds(lx + 32, ly + 20, 78, 20);
    lpTypeCombo.setBounds(lx, ly + 44, 52, 20);
    lpSlopeCombo.setBounds(lx + 56, ly + 44, 54, 20);

    ly += 72;
    limiterHeader.setBounds(lx, ly, 110, 18);
    limiterDbuSlider.setBounds(lx, ly + 20, 110, 20);
    limiterAtLabel.setBounds(lx, ly + 42, 110, 14);
    limiterReLabel.setBounds(lx, ly + 56, 110, 14);

    // Center Curve Visualizer
    int curveX = 145;
    int curveW = getWidth() - curveX - 20;
    curveVisualizer->setBounds(curveX, 55, curveW, 145);
    fftSizeCombo.setBounds(curveX + curveW - 574, 58, 78, 20);
    averagingTimeCombo.setBounds(curveX + curveW - 490, 58, 92, 20);
    peakHoldToggle.setBounds(curveX + curveW - 390, 58, 74, 20);
    spectrumModeCombo.setBounds(curveX + curveW - 310, 58, 92, 20);
    analyzerEnabledToggle.setBounds(curveX + curveW - 210, 58, 84, 20);
    preSpectrumToggle.setBounds(curveX + curveW - 122, 58, 52, 20);
    postSpectrumToggle.setBounds(curveX + curveW - 66, 58, 60, 20);

    selectedBandCombo.setBounds(curveX, 205, 90, 24);
    filterTypeCombo.setBounds(curveX + 96, 205, 105, 24);
    bandEnabledToggle.setBounds(curveX + 208, 205, 110, 24);
    bandFrequencySlider.setBounds(curveX + 324, 205, 170, 24);
    bandQSlider.setBounds(curveX + 500, 205, 105, 24);
    const int abButtonWidth = 42;
    const int copyPasteWidth = 54;
    const int abControlsWidth = abButtonWidth * 2 + copyPasteWidth * 2 + 22;
    const int abControlsX = curveX + juce::jmax(610, curveW - abControlsWidth);
    slotAButton.setBounds(abControlsX, 205, abButtonWidth, 24);
    slotBButton.setBounds(abControlsX + abButtonWidth + 4, 205, abButtonWidth, 24);
    copyEQButton.setBounds(abControlsX + abButtonWidth * 2 + 12, 205, copyPasteWidth, 24);
    pasteEQButton.setBounds(abControlsX + abButtonWidth * 2 + copyPasteWidth + 16,
                            205, copyPasteWidth, 24);

    // 15 EQ Band Fader Bank
    int faderStartY = 235;
    int faderColW = curveW / numPuzuBands;

    for (int b = 0; b < numPuzuBands; ++b)
    {
        int bx = curveX + (b * faderColW);
        bandUIs[(size_t)b].noLabel.setBounds(bx, faderStartY, faderColW, 14);
        bandUIs[(size_t)b].freqLabel.setBounds(bx, faderStartY + 14, faderColW, 12);
        bandUIs[(size_t)b].qLabel.setBounds(bx, faderStartY + 26, faderColW, 12);
        bandUIs[(size_t)b].gainLabel.setBounds(bx, faderStartY + 38, faderColW, 12);
        bandUIs[(size_t)b].fader.setBounds(bx + (faderColW - 18) / 2, faderStartY + 52, 18, 62);
    }

    // Bottom Output Channels & Routing Section
    int by = 345;
    masterVolLabel.setBounds(20, by + 5, 50, 18);
    masterVolSlider.setBounds(25, by + 25, 40, 100);

    int outStartX = 90;
    int outColW = 85;
    for (int i = 0; i < numOutputs; ++i)
    {
        int ox = outStartX + (i * (outColW + 8));
        outputUIs[(size_t)i].tabBtn.setBounds(ox, by + 4, outColW, 24);
        outputUIs[(size_t)i].fader.setBounds(ox + 8, by + 34, 24, 75);
        outputUIs[(size_t)i].phaseBtn.setBounds(ox + 38, by + 36, 40, 20);
        outputUIs[(size_t)i].muteBtn.setBounds(ox + 38, by + 60, 40, 20);
        outputUIs[(size_t)i].gainLabel.setBounds(ox + 38, by + 84, 40, 16);
    }

    // Delay & Schematic
    int delayX = outStartX + (numOutputs * (outColW + 8)) + 15;
    delayUnitCombo.setBounds(delayX + 50, by + 4, 60, 22);

    // Grouping Checkboxes
    int groupY = getHeight() - 28;
    groupFrontToggle.setBounds(90, groupY, 115, 20);
    groupRearToggle.setBounds(215, groupY, 115, 20);
    groupAllToggle.setBounds(340, groupY, 110, 20);
    groupBassToggle.setBounds(460, groupY, 115, 20);
}

void PuzuDSPComponent::setSelectedTarget(int targetIndex)
{
    const int nextTarget = juce::jlimit(0, maxInputs + 1, targetIndex);
    if (nextTarget != selectedTarget)
    {
        storeActiveABSlot();
        getCurrentProcessor().setPuzuAnalyzerEnabled(false);
    }
    selectedTarget = nextTarget;
    refreshPresetCatalog();
    initializeABForSelectedTarget();
    getCurrentProcessor().setPuzuAnalyzerEnabled(analyzerEnabledToggle.getToggleState());
    targetCombo.setSelectedId(selectedTarget + 1, juce::dontSendNotification);
    hasPreSpectrum = false;
    hasPostSpectrum = false;
    preSpectrumDb.fill(-90.0f);
    preRightSpectrumDb.fill(-90.0f);
    postSpectrumDb.fill(-90.0f);
    postRightSpectrumDb.fill(-90.0f);
    updateUIFromProcessor();
    curveVisualizer->repaint();
}

PuzuDSPComponent::ABSlots PuzuDSPComponent::getSceneABStates()
{
    storeActiveABSlot();
    for (size_t target = 0; target < abStates.size(); ++target)
    {
        if (abStateInitialized[target])
            continue;
        const auto current = targetProcessors[target]->getPuzuEQState();
        abStates[target][0] = current;
        abStates[target][1] = current;
        activeABSlots[target] = 0;
        abStateInitialized[target] = true;
    }
    return abStates;
}

void PuzuDSPComponent::setSceneABStates(
    const ABSlots& states, const ActiveABSlots& activeSlots)
{
    abStates = states;
    for (size_t target = 0; target < abStates.size(); ++target)
    {
        activeABSlots[target] = juce::jlimit(0, 1, activeSlots[target]);
        abStateInitialized[target] = true;
    }
    updateUIFromProcessor();
    const int active = activeABSlots[(size_t)selectedTarget];
    slotAButton.setColour(juce::TextButton::buttonColourId,
        active == 0 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
    slotBButton.setColour(juce::TextButton::buttonColourId,
        active == 1 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
}

void PuzuDSPComponent::initializeABForSelectedTarget()
{
    const size_t target = (size_t)selectedTarget;
    if (!abStateInitialized[target])
    {
        const auto initialState = getCurrentProcessor().getPuzuEQState();
        abStates[target][0] = initialState;
        abStates[target][1] = initialState;
        activeABSlots[target] = 0;
        abStateInitialized[target] = true;
    }
    storeActiveABSlot();
}

void PuzuDSPComponent::storeActiveABSlot()
{
    const size_t target = (size_t)selectedTarget;
    if (abStateInitialized[target])
        abStates[target][(size_t)activeABSlots[target]] = getCurrentProcessor().getPuzuEQState();
}

void PuzuDSPComponent::selectABSlot(int slot)
{
    const size_t target = (size_t)selectedTarget;
    if (!abStateInitialized[target])
        initializeABForSelectedTarget();

    storeActiveABSlot();
    const int nextSlot = juce::jlimit(0, 1, slot);
    if (nextSlot != activeABSlots[target])
    {
        activeABSlots[target] = nextSlot;
        getCurrentProcessor().setPuzuEQState(abStates[target][(size_t)nextSlot]);
        updateUIFromProcessor();
        curveVisualizer->repaint();
    }
    slotAButton.setColour(juce::TextButton::buttonColourId,
        nextSlot == 0 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
    slotBButton.setColour(juce::TextButton::buttonColourId,
        nextSlot == 1 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
}

void PuzuDSPComponent::copyPuzuEQ()
{
    copiedPuzuEQ = getCurrentProcessor().getPuzuEQState();
    hasCopiedPuzuEQ = true;
    pasteEQButton.setEnabled(true);
}

void PuzuDSPComponent::pastePuzuEQ()
{
    if (!hasCopiedPuzuEQ)
        return;

    getCurrentProcessor().setPuzuEQState(copiedPuzuEQ);
    storeActiveABSlot();
    updateUIFromProcessor();
    curveVisualizer->repaint();
}

void PuzuDSPComponent::timerCallback()
{
    if (!analyzerEnabledToggle.getToggleState())
        return;

    auto& processor = getCurrentProcessor();
    const int fftSize = getEQAnalyzerFFTSize(fftSizeCombo.getSelectedId());
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    analyzerElapsedSeconds = lastAnalyzerUpdateMs > 0.0
        ? (float)juce::jlimit(0.0, 1.0, (nowMs - lastAnalyzerUpdateMs) * 0.001)
        : 1.0f / 30.0f;
    lastAnalyzerUpdateMs = nowMs;
    if (!isShowing())
    {
        processor.pullPuzuPreAnalyzerStereo(
            analyzerLeftInput.data(), analyzerRightInput.data(), fftSize);
        processor.pullPuzuPostAnalyzerStereo(
            analyzerLeftInput.data(), analyzerRightInput.data(), fftSize);
        return;
    }

    updateSpectrum(processor, false);
    updateSpectrum(processor, true);
    if (peakHoldToggle.getToggleState())
    {
        updateEQSpectrumPeakHold(preSpectrumDb, prePeakHoldDb, 18.0f,
                                 analyzerElapsedSeconds);
        updateEQSpectrumPeakHold(preRightSpectrumDb, preRightPeakHoldDb, 18.0f,
                                 analyzerElapsedSeconds);
        updateEQSpectrumPeakHold(postSpectrumDb, postPeakHoldDb, 18.0f,
                                 analyzerElapsedSeconds);
        updateEQSpectrumPeakHold(postRightSpectrumDb, postRightPeakHoldDb, 18.0f,
                                 analyzerElapsedSeconds);
    }
    curveVisualizer->repaint();
}

void PuzuDSPComponent::updateSpectrum(BusDSPProcessor& processor, bool post)
{
    const int fftSize = getEQAnalyzerFFTSize(fftSizeCombo.getSelectedId());
    const bool received = post
        ? processor.pullPuzuPostAnalyzerStereo(
              analyzerLeftInput.data(), analyzerRightInput.data(), fftSize)
        : processor.pullPuzuPreAnalyzerStereo(
              analyzerLeftInput.data(), analyzerRightInput.data(), fftSize);
    if (!received)
        return;

    auto& leftSpectrum = post ? postSpectrumDb : preSpectrumDb;
    auto& rightSpectrum = post ? postRightSpectrumDb : preRightSpectrumDb;
    const bool hasPreviousSpectrum = post ? hasPostSpectrum : hasPreSpectrum;
    if (spectrumModeCombo.getSelectedId() == 2)
    {
        calculateSpectrum(analyzerLeftInput.data(), leftSpectrum,
                          hasPreviousSpectrum);
        calculateSpectrum(analyzerRightInput.data(), rightSpectrum,
                          hasPreviousSpectrum);
    }
    else
    {
        for (int sample = 0; sample < fftSize; ++sample)
            analyzerLeftInput[(size_t)sample] = 0.5f *
                (analyzerLeftInput[(size_t)sample] +
                 analyzerRightInput[(size_t)sample]);
        calculateSpectrum(analyzerLeftInput.data(), leftSpectrum,
                          hasPreviousSpectrum);
    }

    if (post)
        hasPostSpectrum = true;
    else
        hasPreSpectrum = true;
}

void PuzuDSPComponent::calculateSpectrum(
    const float* input, std::array<float, analyzerDisplayBins>& spectrum,
    bool hasPreviousSpectrum)
{
    const int fftSize = getEQAnalyzerFFTSize(fftSizeCombo.getSelectedId());
    juce::dsp::FFT* fft = nullptr;
    juce::dsp::WindowingFunction<float>* window = nullptr;
    if (fftSize == 1024)
    {
        fft = &analyzerFFT1024;
        window = &analyzerWindow1024;
    }
    else if (fftSize == 4096)
    {
        fft = &analyzerFFT4096;
        window = &analyzerWindow4096;
    }
    else
    {
        fft = &analyzerFFT2048;
        window = &analyzerWindow2048;
    }
    analyzerWindowedInput.fill(0.0f);
    std::memcpy(analyzerWindowedInput.data(), input,
                (size_t)fftSize * sizeof(float));
    window->multiplyWithWindowingTable(analyzerWindowedInput.data(), fftSize);
    analyzerFftData.fill(0.0f);
    std::memcpy(analyzerFftData.data(), analyzerWindowedInput.data(),
                (size_t)fftSize * sizeof(float));
    fft->performFrequencyOnlyForwardTransform(analyzerFftData.data());

    const float sampleRate = (float)getCurrentProcessor().getSampleRate();
    const float maxFrequency = juce::jmin(20000.0f, sampleRate * 0.495f);
    std::array<float, analyzerDisplayBins> currentSpectrum{};
    for (int displayBin = 0; displayBin < analyzerDisplayBins; ++displayBin)
    {
        const float proportion = (float)displayBin / (float)(analyzerDisplayBins - 1);
        const float frequency = 20.0f * std::pow(maxFrequency / 20.0f, proportion);
        const int fftBin = juce::jlimit(1, fftSize / 2 - 1,
            (int)std::round(frequency * (float)fftSize / sampleRate));
        const float magnitude = analyzerFftData[(size_t)fftBin] *
                                (4.0f / (float)fftSize);
        currentSpectrum[(size_t)displayBin] = juce::jlimit(-90.0f, 0.0f,
            juce::Decibels::gainToDecibels(magnitude, -90.0f));
    }
    const float averagingTime = averagingTimeCombo.getSelectedId() == 1
        ? 0.05f : averagingTimeCombo.getSelectedId() == 3 ? 1.0f : 0.25f;
    updateEQSpectrumAverage(currentSpectrum, spectrum, hasPreviousSpectrum,
                            analyzerElapsedSeconds, averagingTime);
}

void PuzuDSPComponent::clearAnalyzerHistory()
{
    hasPreSpectrum = false;
    hasPostSpectrum = false;
    preSpectrumDb.fill(-90.0f);
    preRightSpectrumDb.fill(-90.0f);
    postSpectrumDb.fill(-90.0f);
    postRightSpectrumDb.fill(-90.0f);
    prePeakHoldDb.fill(-90.0f);
    preRightPeakHoldDb.fill(-90.0f);
    postPeakHoldDb.fill(-90.0f);
    postRightPeakHoldDb.fill(-90.0f);
    lastAnalyzerUpdateMs = 0.0;
}

void PuzuDSPComponent::drawSpectrum(juce::Graphics& graphics,
                                    const juce::Rectangle<float>& bounds)
{
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();
    const float bottom = height - 18.0f;
    auto draw = [&](const std::array<float, analyzerDisplayBins>& spectrum,
                    juce::Colour colour, bool visible, bool hasData)
    {
        if (!visible || !hasData)
            return;

        juce::Path path;
        for (int bin = 0; bin < analyzerDisplayBins; ++bin)
        {
            const float x = (float)bin / (float)(analyzerDisplayBins - 1) * width;
            const float level = juce::jlimit(-90.0f, 0.0f, spectrum[(size_t)bin]);
            const float y = bottom - ((level + 90.0f) / 90.0f) * (height - 30.0f);
            if (bin == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        graphics.setColour(colour.withAlpha(0.8f));
        graphics.strokePath(path, juce::PathStrokeType(1.5f));
    };

    draw(preSpectrumDb, juce::Colour(0xff38bdf8),
         preSpectrumToggle.getToggleState(), hasPreSpectrum);
    if (spectrumModeCombo.getSelectedId() == 2)
        draw(preRightSpectrumDb, juce::Colour(0xff818cf8),
             preSpectrumToggle.getToggleState(), hasPreSpectrum);
    draw(postSpectrumDb, juce::Colour(0xff34d399),
         postSpectrumToggle.getToggleState(), hasPostSpectrum);
    if (spectrumModeCombo.getSelectedId() == 2)
        draw(postRightSpectrumDb, juce::Colour(0xfffb7185),
             postSpectrumToggle.getToggleState(), hasPostSpectrum);
    if (peakHoldToggle.getToggleState())
    {
        draw(prePeakHoldDb, juce::Colour(0xffbae6fd),
             preSpectrumToggle.getToggleState(), hasPreSpectrum);
        if (spectrumModeCombo.getSelectedId() == 2)
            draw(preRightPeakHoldDb, juce::Colour(0xffc4b5fd),
                 preSpectrumToggle.getToggleState(), hasPreSpectrum);
        draw(postPeakHoldDb, juce::Colour(0xffffe0a3),
             postSpectrumToggle.getToggleState(), hasPostSpectrum);
        if (spectrumModeCombo.getSelectedId() == 2)
            draw(postRightPeakHoldDb, juce::Colour(0xfffda4af),
                 postSpectrumToggle.getToggleState(), hasPostSpectrum);
    }

    graphics.setFont(juce::FontOptions(9.0f));
    graphics.setColour(juce::Colour(0xff94a3b8));
    graphics.drawText("Spectrum dBFS", 4, 2, 75, 12, juce::Justification::left);
    for (int tick = 0; tick <= 3; ++tick)
    {
        const float db = (float)(-30 * tick);
        const float y = bottom - ((db + 90.0f) / 90.0f) * (height - 30.0f);
        graphics.drawText(juce::String((int)db), (int)width - 30,
                          (int)y - 5, 28, 10, juce::Justification::right);
    }
}

void PuzuDSPComponent::updateUIFromProcessor()
{
    isUpdatingUI = true;
    auto& dsp = getCurrentProcessor();

    hpFreqSlider.setValue(dsp.params.hpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);
    hpSlopeCombo.setSelectedId(dsp.params.hpfSlope.load(std::memory_order_relaxed) + 1, juce::dontSendNotification);

    lpFreqSlider.setValue(dsp.params.lpfFreq.load(std::memory_order_relaxed), juce::dontSendNotification);
    lpSlopeCombo.setSelectedId(dsp.params.lpfSlope.load(std::memory_order_relaxed) + 1, juce::dontSendNotification);

    limiterDbuSlider.setValue(dsp.params.limiterDbu.load(std::memory_order_relaxed), juce::dontSendNotification);

    passBtn.setButtonText(dsp.params.puzuEnabled.load(std::memory_order_relaxed) ? "Pass" : "Bypass");
    passBtn.setColour(juce::TextButton::buttonColourId,
                      dsp.params.puzuEnabled.load(std::memory_order_relaxed) ? juce::Colour(0xff2563eb) : juce::Colour(0xffc62828));
    const int activeSlot = activeABSlots[(size_t)selectedTarget];
    slotAButton.setColour(juce::TextButton::buttonColourId,
        activeSlot == 0 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
    slotBButton.setColour(juce::TextButton::buttonColourId,
        activeSlot == 1 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));

    for (int b = 0; b < numPuzuBands; ++b)
    {
        float f = dsp.params.puzuBands[b].freq.load(std::memory_order_relaxed);
        float q = dsp.params.puzuBands[b].q.load(std::memory_order_relaxed);
        float g = dsp.params.puzuBands[b].gainDb.load(std::memory_order_relaxed);

        bandUIs[(size_t)b].freqLabel.setText(juce::String(f, 0), juce::dontSendNotification);
        bandUIs[(size_t)b].qLabel.setText(juce::String(q, 2), juce::dontSendNotification);
        bandUIs[(size_t)b].gainLabel.setText(juce::String(g, 1), juce::dontSendNotification);
        bandUIs[(size_t)b].fader.setValue(g, juce::dontSendNotification);
    }

    selectedBand = juce::jlimit(0, numPuzuBands - 1, selectedBand);
    selectedBandCombo.setSelectedId(selectedBand + 1, juce::dontSendNotification);
    const auto& band = dsp.params.puzuBands[(size_t)selectedBand];
    filterTypeCombo.setSelectedId((int)band.type.load(std::memory_order_relaxed) + 1,
                                  juce::dontSendNotification);
    bandEnabledToggle.setToggleState(band.enabled.load(std::memory_order_relaxed),
                                     juce::dontSendNotification);
    bandFrequencySlider.setValue(band.freq.load(std::memory_order_relaxed),
                                 juce::dontSendNotification);
    bandQSlider.setValue(band.q.load(std::memory_order_relaxed), juce::dontSendNotification);

    const bool isInput = selectedTarget < maxInputs;
    masterVolLabel.setVisible(!isInput);
    masterVolSlider.setVisible(!isInput);
    delayUnitCombo.setVisible(!isInput);
    groupFrontToggle.setVisible(!isInput);
    groupRearToggle.setVisible(!isInput);
    groupAllToggle.setVisible(!isInput);
    groupBassToggle.setVisible(!isInput);
    for (auto& output : outputUIs)
    {
        output.tabBtn.setVisible(!isInput);
        output.fader.setVisible(!isInput);
        output.phaseBtn.setVisible(!isInput);
        output.muteBtn.setVisible(!isInput);
        output.gainLabel.setVisible(!isInput);
        output.delaySlider.setVisible(!isInput);
    }

    masterVolSlider.setValue(dsp.params.masterGain.load(std::memory_order_relaxed), juce::dontSendNotification);

    isUpdatingUI = false;
    storeActiveABSlot();
}

void PuzuDSPComponent::sliderValueChanged(juce::Slider* slider)
{
    if (isUpdatingUI) return;
    auto& dsp = getCurrentProcessor();

    // Check 15 EQ faders
    for (int b = 0; b < numPuzuBands; ++b)
    {
        if (slider == &bandUIs[(size_t)b].fader)
        {
            float val = (float)slider->getValue();
            dsp.params.puzuBands[b].gainDb.store(val, std::memory_order_relaxed);
            bandUIs[(size_t)b].gainLabel.setText(juce::String(val, 1), juce::dontSendNotification);
            dsp.updatePuzuCoefficients();
            storeActiveABSlot();
            curveVisualizer->repaint();
            return;
        }
    }

    auto& band = dsp.params.puzuBands[(size_t)selectedBand];
    if (slider == &bandFrequencySlider)
    {
        band.freq.store((float)bandFrequencySlider.getValue(), std::memory_order_relaxed);
        dsp.updatePuzuCoefficients();
        storeActiveABSlot();
        updateUIFromProcessor();
        curveVisualizer->repaint();
        return;
    }
    if (slider == &bandQSlider)
    {
        band.q.store((float)bandQSlider.getValue(), std::memory_order_relaxed);
        dsp.updatePuzuCoefficients();
        storeActiveABSlot();
        updateUIFromProcessor();
        curveVisualizer->repaint();
        return;
    }

    if (slider == &hpFreqSlider)
    {
        dsp.params.hpfFreq.store((float)hpFreqSlider.getValue(), std::memory_order_relaxed);
        dsp.updateCoefficients();
        curveVisualizer->repaint();
    }
    else if (slider == &lpFreqSlider)
    {
        dsp.params.lpfFreq.store((float)lpFreqSlider.getValue(), std::memory_order_relaxed);
        dsp.updateCoefficients();
        curveVisualizer->repaint();
    }
    else if (slider == &limiterDbuSlider)
    {
        dsp.params.limiterDbu.store((float)limiterDbuSlider.getValue(), std::memory_order_relaxed);
        dsp.params.limiterCeilingDb.store((float)limiterDbuSlider.getValue() / 30.0f * 6.0f - 1.0f, std::memory_order_relaxed);
        dsp.updateCoefficients();
    }
    else if (slider == &masterVolSlider)
    {
        dsp.params.masterGain.store((float)masterVolSlider.getValue(), std::memory_order_relaxed);
    }
}

void PuzuDSPComponent::buttonClicked(juce::Button* button)
{
    auto& dsp = getCurrentProcessor();

    if (button == &analyzerEnabledToggle)
    {
        dsp.setPuzuAnalyzerEnabled(analyzerEnabledToggle.getToggleState());
        if (!analyzerEnabledToggle.getToggleState())
            clearAnalyzerHistory();
        curveVisualizer->repaint();
        return;
    }
    if (button == &peakHoldToggle)
    {
        prePeakHoldDb = preSpectrumDb;
        preRightPeakHoldDb = preRightSpectrumDb;
        postPeakHoldDb = postSpectrumDb;
        postRightPeakHoldDb = postRightSpectrumDb;
        curveVisualizer->repaint();
        return;
    }
    if (button == &preSpectrumToggle || button == &postSpectrumToggle)
    {
        curveVisualizer->repaint();
        return;
    }
    if (button == &slotAButton)
    {
        selectABSlot(0);
        return;
    }
    if (button == &slotBButton)
    {
        selectABSlot(1);
        return;
    }
    if (button == &copyEQButton)
    {
        copyPuzuEQ();
        return;
    }
    if (button == &pasteEQButton)
    {
        pastePuzuEQ();
        return;
    }

    if (button == &rstBtn)
    {
        // Reset all 15 EQ gains to 0 dB
        for (int b = 0; b < numPuzuBands; ++b)
        {
            dsp.params.puzuBands[b].gainDb.store(0.0f, std::memory_order_relaxed);
            dsp.params.puzuBands[b].freq.store(defaultPuzuFreqs[b], std::memory_order_relaxed);
            dsp.params.puzuBands[b].q.store(defaultPuzuQs[b], std::memory_order_relaxed);
            dsp.params.puzuBands[b].type.store(PuzuFilterType::Bell, std::memory_order_relaxed);
            dsp.params.puzuBands[b].enabled.store(true, std::memory_order_relaxed);
        }
        dsp.updatePuzuCoefficients();
        storeActiveABSlot();
        updateUIFromProcessor();
        curveVisualizer->repaint();
    }
    else if (button == &passBtn)
    {
        bool current = dsp.params.puzuEnabled.load(std::memory_order_relaxed);
        dsp.params.puzuEnabled.store(!current, std::memory_order_relaxed);
        storeActiveABSlot();
        passBtn.setButtonText(!current ? "Pass" : "Bypass");
        passBtn.setColour(juce::TextButton::buttonColourId, !current ? juce::Colour(0xff2563eb) : juce::Colour(0xffc62828));
        curveVisualizer->repaint();
    }
    else if (button == &bandEnabledToggle)
    {
        dsp.params.puzuBands[(size_t)selectedBand].enabled.store(
            bandEnabledToggle.getToggleState(), std::memory_order_relaxed);
        dsp.updatePuzuCoefficients();
        storeActiveABSlot();
        curveVisualizer->repaint();
    }
    else
    {
        // Check output channel tab buttons
        for (int i = 0; i < numOutputs; ++i)
        {
            if (button == &outputUIs[(size_t)i].tabBtn)
            {
                // Toggle between bus 0 (Stream) and bus 1 (Monitor)
                setSelectedTarget(i < 2 ? maxInputs : maxInputs + 1);
                for (int j = 0; j < numOutputs; ++j)
                {
                    outputUIs[(size_t)j].tabBtn.setColour(juce::TextButton::buttonColourId,
                        (j == i) ? juce::Colour(0xff2563eb) : juce::Colour(0xff1e293b));
                }
                break;
            }
            else if (button == &outputUIs[(size_t)i].phaseBtn)
            {
                bool current = dsp.params.phaseInverted.load(std::memory_order_relaxed);
                dsp.params.phaseInverted.store(!current, std::memory_order_relaxed);
                outputUIs[(size_t)i].phaseBtn.setButtonText(!current ? "180'" : "0'");
                outputUIs[(size_t)i].phaseBtn.setColour(juce::TextButton::buttonColourId,
                    !current ? juce::Colour(0xffdc2626) : juce::Colour(0xff2563eb));
                break;
            }
            else if (button == &outputUIs[(size_t)i].muteBtn)
            {
                bool current = dsp.params.masterMute.load(std::memory_order_relaxed);
                dsp.params.masterMute.store(!current, std::memory_order_relaxed);
                outputUIs[(size_t)i].muteBtn.setColour(juce::TextButton::buttonColourId,
                    !current ? juce::Colour(0xffdc2626) : juce::Colour(0xff334155));
                break;
            }
        }
    }
}

void PuzuDSPComponent::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &presetCombo)
    {
        const int presetIndex = presetCombo.getSelectedId() - 2;
        if (presetIndex < 0 ||
            presetIndex >= (int)catalogPresetItems.size())
            return;

        const auto& item = catalogPresetItems[(size_t)presetIndex];
        LiveBus::EQPresetDefinition definition;
        juce::String error;
        if (!presetManager.loadEQCatalogPreset(item, definition, error))
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon, "EQ Preset Error",
                "Could not load EQ preset '" + item.name + "': " + error, "OK");
            presetCombo.setSelectedId(1, juce::dontSendNotification);
            return;
        }

        PuzuEQState state = getCurrentProcessor().getPuzuEQState();
        if (!LiveBus::EQPresetCatalog::mapToPuzu15Band(
                definition, state, error))
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon, "EQ Preset Error",
                "Could not map EQ preset '" + item.name + "': " + error, "OK");
            presetCombo.setSelectedId(1, juce::dontSendNotification);
            return;
        }
        if (!juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon, "Approximate EQ Mapping",
                "This 8-band catalog curve will be approximated on the "
                "selected Puzu 15-band frequency grid. Continue?",
                "Apply", "Cancel"))
        {
            presetCombo.setSelectedId(1, juce::dontSendNotification);
            return;
        }

        auto& processor = getCurrentProcessor();
        processor.setPuzuEQState(state);
        storeActiveABSlot();
        updateUIFromProcessor();
        curveVisualizer->repaint();
        return;
    }

    if (combo == &spectrumModeCombo || combo == &fftSizeCombo ||
        combo == &averagingTimeCombo)
    {
        clearAnalyzerHistory();
        curveVisualizer->repaint();
        return;
    }

    if (combo == &targetCombo)
    {
        const int id = targetCombo.getSelectedId();
        const int target = id >= 1 && id <= maxInputs ? id - 1
            : id == maxInputs + 1 ? maxInputs
            : maxInputs + 1;
        setSelectedTarget(target);
        if (onTargetSelected)
            onTargetSelected(target);
        return;
    }

    if (combo == &selectedBandCombo)
    {
        selectedBand = juce::jlimit(0, numPuzuBands - 1, selectedBandCombo.getSelectedId() - 1);
        updateUIFromProcessor();
        curveVisualizer->repaint();
        return;
    }

    auto& dsp = getCurrentProcessor();

    if (combo == &filterTypeCombo)
    {
        dsp.params.puzuBands[(size_t)selectedBand].type.store(
            (PuzuFilterType)juce::jlimit(0, 5, filterTypeCombo.getSelectedId() - 1),
            std::memory_order_relaxed);
        dsp.updatePuzuCoefficients();
        storeActiveABSlot();
        updateUIFromProcessor();
        curveVisualizer->repaint();
    }
    else if (combo == &hpSlopeCombo)
    {
        int slopeIdx = hpSlopeCombo.getSelectedId() - 1; // 0=6dB, 1=12dB, 2=18dB, 3=24dB
        dsp.params.hpfSlope.store(juce::jlimit(0, 3, slopeIdx), std::memory_order_relaxed);
        dsp.updateCoefficients();
        curveVisualizer->repaint();
    }
    else if (combo == &lpSlopeCombo)
    {
        int slopeIdx = lpSlopeCombo.getSelectedId() - 1; // 0=OFF, 1=6dB, 2=12dB, 3=18dB, 4=24dB
        dsp.params.lpfSlope.store(juce::jlimit(0, 4, slopeIdx), std::memory_order_relaxed);
        dsp.params.lpfEnabled.store(slopeIdx > 0, std::memory_order_relaxed);
        dsp.updateCoefficients();
        curveVisualizer->repaint();
    }
}

void PuzuDSPComponent::refreshPresetCatalog()
{
    presetCombo.clear(juce::dontSendNotification);
    presetCombo.addItem("Select EQ preset...", 1);
    catalogPresetItems = presetManager.getPresetsForCategory(
        LiveBus::PresetCategory::EQCatalog);

    int itemId = 2;
    bool hasFactoryHeading = false;
    bool hasUserHeading = false;
    for (const auto& item : catalogPresetItems)
    {
        if (item.isFactory && !hasFactoryHeading)
        {
            presetCombo.addSectionHeading("Factory EQ presets");
            hasFactoryHeading = true;
        }
        else if (!item.isFactory && !hasUserHeading)
        {
            presetCombo.addSectionHeading("Your EQ presets");
            hasUserHeading = true;
        }
        presetCombo.addItem(item.name, itemId++);
    }
}
