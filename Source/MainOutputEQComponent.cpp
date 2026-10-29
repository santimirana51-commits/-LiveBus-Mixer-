#include "MainOutputEQComponent.h"
#include <cmath>

MainOutputEQComponent::MainOutputEQComponent(
    BusDSPProcessor& target, LiveBus::PresetManager& presets)
    : processor(target), presetManager(presets),
      responseDisplay(std::make_unique<ResponseDisplay>(*this))
{
    enabledToggle.addListener(this);
    addAndMakeVisible(enabledToggle);
    for (auto* toggle : { &analyzerEnabledToggle, &preSpectrumToggle,
                          &postSpectrumToggle, &peakHoldToggle })
    {
        toggle->addListener(this);
        addAndMakeVisible(*toggle);
    }
    bandSoloToggle.addListener(this);
    bandSoloToggle.setTooltip(
        "Audition the selected band's frequency range using a band-pass filter. "
        "This temporarily replaces the output and does not change EQ settings.");
    addAndMakeVisible(bandSoloToggle);
    preSpectrumToggle.setToggleState(true, juce::dontSendNotification);
    postSpectrumToggle.setToggleState(true, juce::dontSendNotification);
    preSpectrumDb.fill(-90.0f);
    postSpectrumDb.fill(-90.0f);
    prePeakHoldDb.fill(-90.0f);
    postPeakHoldDb.fill(-90.0f);
    peakHoldToggle.setTooltip("Hold spectrum peaks and let them decay over time");

    averagingTimeCombo.addItem("Avg 50 ms", 1);
    averagingTimeCombo.addItem("Avg 250 ms", 2);
    averagingTimeCombo.addItem("Avg 1 s", 3);
    averagingTimeCombo.setSelectedId(2, juce::dontSendNotification);
    averagingTimeCombo.setTooltip(
        "Spectrum averaging time: shorter reacts faster; longer is smoother");
    averagingTimeCombo.addListener(this);
    addAndMakeVisible(averagingTimeCombo);

    fftSizeCombo.addItem("FFT 1024", 1);
    fftSizeCombo.addItem("FFT 2048", 2);
    fftSizeCombo.addItem("FFT 4096", 3);
    fftSizeCombo.setSelectedId(2, juce::dontSendNotification);
    fftSizeCombo.setTooltip(
        "FFT size: larger sizes improve low-frequency resolution but update less often");
    fftSizeCombo.addListener(this);
    addAndMakeVisible(fftSizeCombo);

    spectrumModeCombo.addItem("Mono", 1);
    spectrumModeCombo.addItem("Stereo", 2);
    spectrumModeCombo.setSelectedId(1, juce::dontSendNotification);
    spectrumModeCombo.setTooltip(
        "Mono averages left and right; Stereo shows each channel separately");
    spectrumModeCombo.addListener(this);
    addAndMakeVisible(spectrumModeCombo);

    presetCombo.setTooltip(
        "Apply a factory or user EQ Catalog preset to the 24-band Main Output EQ");
    presetCombo.addListener(this);
    presetLabel.setFont(juce::FontOptions(12.0f));
    presetLabel.setColour(juce::Label::textColourId,
                          juce::Colour(0xffcbd5e1));
    addAndMakeVisible(presetLabel);
    addAndMakeVisible(presetCombo);
    refreshPresetCatalog();

    for (int band = 0; band < numMainOutputEQBands; ++band)
    {
        selectedBandCombo.addItem("Band " + juce::String(band + 1), band + 1);
        auto& slider = gainSliders[(size_t)band];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(-18.0, 18.0, 0.1);
        slider.setColour(juce::Slider::trackColourId,
                         juce::Colour(0xff38bdf8));
        slider.setColour(juce::Slider::thumbColourId,
                         juce::Colour(0xffdbeafe));
        slider.setTooltip("Main Output EQ band " + juce::String(band + 1) +
                          " gain");
        slider.addListener(this);
        addAndMakeVisible(slider);

        const float frequency = mainOutputEQFrequencies[(size_t)band];
        frequencyLabels[(size_t)band].setText(
            frequency >= 1000.0f
                ? juce::String(frequency / 1000.0f,
                               frequency >= 10000.0f ? 0 : 1) + "k"
                : juce::String(frequency,
                               std::floor(frequency) == frequency ? 0 : 1),
            juce::dontSendNotification);
        frequencyLabels[(size_t)band].setFont(juce::FontOptions(9.0f));
        frequencyLabels[(size_t)band].setJustificationType(juce::Justification::centred);
        frequencyLabels[(size_t)band].setColour(juce::Label::textColourId,
                                                 juce::Colour(0xffcbd5e1));
        addAndMakeVisible(frequencyLabels[(size_t)band]);
    }

    selectedBandCombo.setSelectedId(1, juce::dontSendNotification);
    selectedBandCombo.addListener(this);
    addAndMakeVisible(selectedBandCombo);

    const std::array<const char*, 6> filterNames{
        "HPF", "Low Shelf", "Bell", "High Shelf", "Notch", "LPF"
    };
    for (int index = 0; index < (int)filterNames.size(); ++index)
        filterTypeCombo.addItem(filterNames[(size_t)index], index + 1);
    filterTypeCombo.addListener(this);
    addAndMakeVisible(filterTypeCombo);

    bandEnabledToggle.addListener(this);
    addAndMakeVisible(bandEnabledToggle);
    dynamicEnabledToggle.addListener(this);
    dynamicEnabledToggle.setTooltip(
        "Enable downward dynamic EQ: gain is reduced when the band's detected level exceeds threshold.");
    addAndMakeVisible(dynamicEnabledToggle);

    frequencySlider.setSliderStyle(juce::Slider::LinearBar);
    frequencySlider.setRange(20.0, 20000.0, 1.0);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.addListener(this);
    addAndMakeVisible(frequencySlider);

    qSlider.setSliderStyle(juce::Slider::LinearBar);
    qSlider.setRange(0.1, 10.0, 0.01);
    qSlider.setTextValueSuffix(" Q");
    qSlider.addListener(this);
    addAndMakeVisible(qSlider);
    const auto configureDynamicSlider = [this](juce::Slider& slider,
                                               double minimum, double maximum,
                                               double step, const juce::String& suffix,
                                               const juce::String& tooltip)
    {
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setRange(minimum, maximum, step);
        slider.setTextValueSuffix(suffix);
        slider.setTooltip(tooltip);
        slider.addListener(this);
        addAndMakeVisible(slider);
    };
    configureDynamicSlider(dynamicThresholdSlider, -80.0, 0.0, 0.1, " dB",
                           "Dynamic detector threshold in dBFS");
    configureDynamicSlider(dynamicAttackSlider, 0.1, 100.0, 0.1, " ms",
                           "Dynamic EQ gain reduction attack time");
    configureDynamicSlider(dynamicReleaseSlider, 5.0, 1000.0, 1.0, " ms",
                           "Dynamic EQ gain reduction release time");
    configureDynamicSlider(dynamicRangeSlider, 0.0, 18.0, 0.1, " dB",
                           "Maximum dynamic gain reduction");
    dynamicDetectorCombo.addItem("Peak", (int)MainOutputEQDetector::Peak + 1);
    dynamicDetectorCombo.addItem("RMS", (int)MainOutputEQDetector::RMS + 1);
    dynamicDetectorCombo.setTooltip("Select peak or RMS detector measurement");
    dynamicDetectorCombo.addListener(this);
    addAndMakeVisible(dynamicDetectorCombo);

    for (auto* button : { &slotAButton, &slotBButton, &copyButton, &pasteButton })
    {
        button->addListener(this);
        addAndMakeVisible(*button);
    }
    pasteButton.setEnabled(false);
    slotAButton.setTooltip("Select or store configuration A");
    slotBButton.setTooltip("Select or store configuration B");
    copyButton.setTooltip("Copy the current 24-band configuration");
    pasteButton.setTooltip("Paste the copied 24-band configuration");

    addAndMakeVisible(*responseDisplay);
    abStates[0] = processor.getMainOutputEQ24State();
    abStates[1] = abStates[0];
    updateUIFromProcessor();
    startTimerHz(30);
}

MainOutputEQComponent::~MainOutputEQComponent()
{
    stopTimer();
    processor.setMainOutputEQAnalyzerEnabled(false);
}

void MainOutputEQComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff0b0f17));
    graphics.setColour(juce::Colour(0xff1e293b));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f, 1.0f);

    const auto sectionColour = juce::Colour(0xff101722);
    const auto sectionBorder = juce::Colour(0xff1e293b);
    const int sectionX = 12;
    const int sectionWidth = juce::jmax(0, getWidth() - 24);
    for (const int y : { 270, 302 })
    {
        const juce::Rectangle<float> section(
            (float)sectionX, (float)y, (float)sectionWidth, 28.0f);
        graphics.setColour(sectionColour);
        graphics.fillRoundedRectangle(section, 5.0f);
        graphics.setColour(sectionBorder);
        graphics.drawRoundedRectangle(section, 5.0f, 1.0f);
    }

    const int bankY = 340;
    const int rowHeight = juce::jmax(
        80, (getHeight() - bankY - 12) / 2);
    for (int row = 0; row < 2; ++row)
    {
        const juce::Rectangle<float> bank(
            (float)sectionX, (float)(bankY + row * rowHeight),
            (float)sectionWidth, (float)rowHeight - 4.0f);
        graphics.setColour(sectionColour);
        graphics.fillRoundedRectangle(bank, 5.0f);
        graphics.setColour(sectionBorder);
        graphics.drawRoundedRectangle(bank, 5.0f, 1.0f);
    }

    const int bankWidth = juce::jmax(0, getWidth() - 36);
    const int bandWidth = bankWidth / 12;
    const int selectedRow = selectedBand / 12;
    const int selectedColumn = selectedBand % 12;
    const juce::Rectangle<float> selectedBandBounds(
        (float)(18 + selectedColumn * bandWidth + 2),
        (float)(bankY + selectedRow * rowHeight + 2),
        (float)juce::jmax(0, bandWidth - 4),
        (float)rowHeight - 8.0f);
    graphics.setColour(juce::Colour(0x1838bdf8));
    graphics.fillRoundedRectangle(selectedBandBounds, 4.0f);
    graphics.setColour(juce::Colour(0x6038bdf8));
    graphics.drawRoundedRectangle(selectedBandBounds, 4.0f, 1.0f);

    graphics.setColour(juce::Colour(0xffe2e8f0));
    graphics.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    graphics.drawText("MAIN OUTPUT EQUALIZER",
                      18, 11, 360, 24, juce::Justification::left);
    graphics.setColour(juce::Colour(0xff94a3b8));
    graphics.setFont(juce::FontOptions(11.0f));
    graphics.drawText("24-band parametric EQ  |  Stream Bus",
                      18, 35, 360, 18, juce::Justification::left);
}

void MainOutputEQComponent::resized()
{
    const int width = getWidth();
    constexpr int left = 18;
    constexpr int gap = 6;
    enabledToggle.setBounds(left, 64, 164, 24);
    slotAButton.setBounds(190, 64, 34, 24);
    slotBButton.setBounds(228, 64, 34, 24);
    copyButton.setBounds(270, 64, 58, 24);
    pasteButton.setBounds(334, 64, 58, 24);

    presetLabel.setBounds(left, 96, 72, 24);
    presetCombo.setBounds(left + 76, 96, 330, 24);
    const int analyzerRowY = 96;
    const int analyzerRowRight = width - left;
    int right = analyzerRowRight;
    const auto placeRight = [&](juce::Component& component, int componentWidth)
    {
        right -= componentWidth;
        component.setBounds(right, analyzerRowY, componentWidth, 24);
        right -= gap;
    };
    placeRight(fftSizeCombo, 88);
    placeRight(averagingTimeCombo, 104);
    placeRight(spectrumModeCombo, 72);
    placeRight(peakHoldToggle, 72);
    placeRight(bandSoloToggle, 72);
    placeRight(postSpectrumToggle, 42);
    placeRight(preSpectrumToggle, 38);
    placeRight(analyzerEnabledToggle, 72);

    responseDisplay->setBounds(left, 128, width - 2 * left, 136);

    const int controlsWidth = width - 2 * left;
    int controlX = left;
    const int bandWidth = juce::jmax(82, controlsWidth * 10 / 100);
    const int typeWidth = juce::jmax(94, controlsWidth * 12 / 100);
    const int enabledWidth = juce::jmax(112, controlsWidth * 13 / 100);
    const int frequencyWidth = juce::jmax(150, controlsWidth * 31 / 100);
    const int qWidth = juce::jmax(100, controlsWidth - bandWidth - typeWidth -
                                             enabledWidth - frequencyWidth -
                                             gap * 4);
    selectedBandCombo.setBounds(controlX, 272, bandWidth, 24);
    controlX += bandWidth + gap;
    filterTypeCombo.setBounds(controlX, 272, typeWidth, 24);
    controlX += typeWidth + gap;
    bandEnabledToggle.setBounds(controlX, 272, enabledWidth, 24);
    controlX += enabledWidth + gap;
    frequencySlider.setBounds(controlX, 272, frequencyWidth, 24);
    controlX += frequencyWidth + gap;
    qSlider.setBounds(controlX, 272, qWidth, 24);

    const int dynamicY = 304;
    const int dynamicWidth = controlsWidth;
    const int dynamicToggleWidth = 105;
    const int detectorWidth = 88;
    const int dynamicFieldWidth = juce::jmax(
        90, (dynamicWidth - dynamicToggleWidth - detectorWidth - gap * 5) / 4);
    int dynamicX = left;
    dynamicEnabledToggle.setBounds(dynamicX, dynamicY, dynamicToggleWidth, 24);
    dynamicX += dynamicToggleWidth + gap;
    dynamicThresholdSlider.setBounds(dynamicX, dynamicY, dynamicFieldWidth, 24);
    dynamicX += dynamicFieldWidth + gap;
    dynamicAttackSlider.setBounds(dynamicX, dynamicY, dynamicFieldWidth, 24);
    dynamicX += dynamicFieldWidth + gap;
    dynamicReleaseSlider.setBounds(dynamicX, dynamicY, dynamicFieldWidth, 24);
    dynamicX += dynamicFieldWidth + gap;
    dynamicRangeSlider.setBounds(dynamicX, dynamicY, dynamicFieldWidth, 24);
    dynamicX += dynamicFieldWidth + gap;
    dynamicDetectorCombo.setBounds(dynamicX, dynamicY, detectorWidth, 24);

    const int bankX = left;
    const int bankY = 340;
    const int bankWidth = controlsWidth;
    const int columnWidth = bankWidth / 12;
    const int rowHeight = juce::jmax(
        80, (getHeight() - bankY - 12) / 2);
    for (int band = 0; band < numMainOutputEQBands; ++band)
    {
        const int row = band / 12;
        const int column = band % 12;
        const int x = bankX + column * columnWidth;
        const int y = bankY + row * rowHeight;
        frequencyLabels[(size_t)band].setBounds(x + 3, y + 4,
                                                 columnWidth - 6, 18);
        gainSliders[(size_t)band].setBounds(x + 5, y + 24,
                                             columnWidth - 10,
                                             rowHeight - 34);
    }
}

void MainOutputEQComponent::updateUIFromProcessor()
{
    updatingUI = true;
    enabledToggle.setToggleState(
        processor.params.mainOutputEQEnabled.load(std::memory_order_relaxed),
        juce::dontSendNotification);
    for (int band = 0; band < numMainOutputEQBands; ++band)
    {
        gainSliders[(size_t)band].setValue(
            processor.params.mainOutputEQGainDb[(size_t)band].load(
                std::memory_order_relaxed),
            juce::dontSendNotification);
        const float frequency = processor.params.mainOutputEQFreq[(size_t)band].load(
            std::memory_order_relaxed);
        frequencyLabels[(size_t)band].setText(
            frequency >= 1000.0f
                ? juce::String(frequency / 1000.0f,
                               frequency >= 10000.0f ? 0 : 1) + "k"
                : juce::String(frequency,
                               std::floor(frequency) == frequency ? 0 : 1),
            juce::dontSendNotification);
        const bool bandEnabled =
            processor.params.mainOutputEQBandEnabled[(size_t)band].load(
                std::memory_order_relaxed);
        frequencyLabels[(size_t)band].setColour(
            juce::Label::textColourId,
            band == selectedBand ? juce::Colour(0xff7dd3fc)
                : bandEnabled ? juce::Colour(0xffcbd5e1)
                              : juce::Colour(0xff64748b));
    }

    const auto& band = processor.params;
    selectedBandCombo.setSelectedId(selectedBand + 1, juce::dontSendNotification);
    filterTypeCombo.setSelectedId(
        band.mainOutputEQType[(size_t)selectedBand].load(std::memory_order_relaxed) + 1,
        juce::dontSendNotification);
    bandEnabledToggle.setToggleState(
        band.mainOutputEQBandEnabled[(size_t)selectedBand].load(std::memory_order_relaxed),
        juce::dontSendNotification);
    frequencySlider.setValue(
        band.mainOutputEQFreq[(size_t)selectedBand].load(std::memory_order_relaxed),
        juce::dontSendNotification);
    qSlider.setValue(
        band.mainOutputEQQ[(size_t)selectedBand].load(std::memory_order_relaxed),
        juce::dontSendNotification);
    dynamicEnabledToggle.setToggleState(
        band.mainOutputEQDynamicEnabled[(size_t)selectedBand].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    dynamicThresholdSlider.setValue(
        band.mainOutputEQDynamicThresholdDb[(size_t)selectedBand].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    dynamicAttackSlider.setValue(
        band.mainOutputEQDynamicAttackMs[(size_t)selectedBand].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    dynamicReleaseSlider.setValue(
        band.mainOutputEQDynamicReleaseMs[(size_t)selectedBand].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    dynamicRangeSlider.setValue(
        band.mainOutputEQDynamicRangeDb[(size_t)selectedBand].load(
            std::memory_order_relaxed),
        juce::dontSendNotification);
    dynamicDetectorCombo.setSelectedId(
        band.mainOutputEQDynamicDetector[(size_t)selectedBand].load(
            std::memory_order_relaxed) + 1,
        juce::dontSendNotification);
    const int soloBand = processor.getMainOutputEQSoloBand();
    bandSoloToggle.setToggleState(soloBand >= 0, juce::dontSendNotification);
    bandSoloToggle.setColour(juce::ToggleButton::textColourId,
        soloBand >= 0 ? juce::Colour(0xffffb74d) : juce::Colour(0xffcbd5e1));
    if (soloBand >= 0 && soloBand != selectedBand)
        processor.setMainOutputEQSoloBand(selectedBand);
    slotAButton.setColour(juce::TextButton::buttonColourId,
        activeSlot == 0 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
    slotBButton.setColour(juce::TextButton::buttonColourId,
        activeSlot == 1 ? juce::Colour(0xff2563eb) : juce::Colour(0xff334155));
    abStates[(size_t)activeSlot] = processor.getMainOutputEQ24State();
    updatingUI = false;
    responseDisplay->repaint();
}

void MainOutputEQComponent::timerCallback()
{
    const bool shouldCapture = analyzerEnabledToggle.getToggleState() && isShowing();
    processor.setMainOutputEQAnalyzerEnabled(shouldCapture);
    if (!shouldCapture)
    {
        const int fftSize = getEQAnalyzerFFTSize(fftSizeCombo.getSelectedId());
        processor.pullMainOutputEQPreAnalyzerStereo(
            analyzerLeftInput.data(), analyzerRightInput.data(), fftSize);
        processor.pullMainOutputEQPostAnalyzerStereo(
            analyzerLeftInput.data(), analyzerRightInput.data(), fftSize);
        lastAnalyzerUpdateMs = 0.0;
        return;
    }

    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    analyzerElapsedSeconds = lastAnalyzerUpdateMs > 0.0
        ? (float)juce::jlimit(0.0, 0.25, (nowMs - lastAnalyzerUpdateMs) / 1000.0)
        : 1.0f / 30.0f;
    lastAnalyzerUpdateMs = nowMs;
    updateSpectrum(false);
    updateSpectrum(true);
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
    responseDisplay->repaint();
}

void MainOutputEQComponent::updateSpectrum(bool post)
{
    const int fftSize = getEQAnalyzerFFTSize(fftSizeCombo.getSelectedId());
    const bool received = post
        ? processor.pullMainOutputEQPostAnalyzerStereo(
              analyzerLeftInput.data(), analyzerRightInput.data(), fftSize)
        : processor.pullMainOutputEQPreAnalyzerStereo(
              analyzerLeftInput.data(), analyzerRightInput.data(), fftSize);
    if (!received)
        return;

    auto& leftSpectrum = post ? postSpectrumDb : preSpectrumDb;
    auto& rightSpectrum = post ? postRightSpectrumDb : preRightSpectrumDb;
    auto& hasSpectrum = post ? hasPostSpectrum : hasPreSpectrum;
    if (spectrumModeCombo.getSelectedId() == 2)
    {
        calculateSpectrum(analyzerLeftInput.data(), leftSpectrum,
                          hasSpectrum);
        calculateSpectrum(analyzerRightInput.data(), rightSpectrum,
                          hasSpectrum);
        std::array<float, analyzerDisplayBins> combinedSpectrum{};
        for (int bin = 0; bin < analyzerDisplayBins; ++bin)
            combinedSpectrum[(size_t)bin] =
                0.5f * (leftSpectrum[(size_t)bin] +
                        rightSpectrum[(size_t)bin]);
        if (!post)
        {
            const float maxFrequency = juce::jmin(
                20000.0f, (float)processor.getSampleRate() * 0.495f);
            peakAnalysis = findEQFrequencyPeakCandidates(
                combinedSpectrum, 20.0f, maxFrequency);
        }
    }
    else
    {
        for (int sample = 0; sample < fftSize; ++sample)
            analyzerLeftInput[(size_t)sample] = 0.5f *
                (analyzerLeftInput[(size_t)sample] +
                 analyzerRightInput[(size_t)sample]);
        calculateSpectrum(analyzerLeftInput.data(), leftSpectrum,
                          hasSpectrum);
        if (!post)
        {
            const float maxFrequency = juce::jmin(
                20000.0f, (float)processor.getSampleRate() * 0.495f);
            peakAnalysis = findEQFrequencyPeakCandidates(
                leftSpectrum, 20.0f, maxFrequency);
        }
    }
    hasSpectrum = true;
}

void MainOutputEQComponent::calculateSpectrum(
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
    std::memcpy(analyzerWindowedInput.data(), input,
                (size_t)fftSize * sizeof(float));
    window->multiplyWithWindowingTable(analyzerWindowedInput.data(), fftSize);
    std::fill_n(analyzerFftData.data(), fftSize * 2, 0.0f);
    std::memcpy(analyzerFftData.data(), analyzerWindowedInput.data(),
                (size_t)fftSize * sizeof(float));
    fft->performFrequencyOnlyForwardTransform(analyzerFftData.data());

    std::array<float, analyzerDisplayBins> currentSpectrumDb{};
    const float sampleRate = (float)processor.getSampleRate();
    const float maxFrequency = juce::jmin(20000.0f, sampleRate * 0.495f);
    for (int displayBin = 0; displayBin < analyzerDisplayBins; ++displayBin)
    {
        const float proportion = (float)displayBin / (float)(analyzerDisplayBins - 1);
        const float frequency = 20.0f * std::pow(maxFrequency / 20.0f, proportion);
        const int fftBin = juce::jlimit(1, fftSize / 2 - 1,
            (int)std::round(frequency * (float)fftSize / sampleRate));
        const float magnitude = analyzerFftData[(size_t)fftBin] *
                                (4.0f / (float)fftSize);
        const float levelDb = juce::jlimit(-90.0f, 0.0f,
            juce::Decibels::gainToDecibels(magnitude, -90.0f));
        currentSpectrumDb[(size_t)displayBin] = levelDb;
    }
    const int averagingId = averagingTimeCombo.getSelectedId();
    const float averagingTimeSeconds = averagingId == 1 ? 0.05f
        : averagingId == 3 ? 1.0f : 0.25f;
    updateEQSpectrumAverage(currentSpectrumDb, spectrum, hasPreviousSpectrum,
                            analyzerElapsedSeconds, averagingTimeSeconds);
}

void MainOutputEQComponent::setSelectedBand(int band)
{
    selectedBand = juce::jlimit(0, numMainOutputEQBands - 1, band);
    updateUIFromProcessor();
}

void MainOutputEQComponent::storeActiveSlot()
{
    abStates[(size_t)activeSlot] = processor.getMainOutputEQ24State();
}

MainOutputEQComponent::ABSlots MainOutputEQComponent::getSceneABStates()
{
    storeActiveSlot();
    return abStates;
}

void MainOutputEQComponent::setSceneABStates(const ABSlots& states, int active)
{
    abStates = states;
    activeSlot = juce::jlimit(0, 1, active);
    updateUIFromProcessor();
}

void MainOutputEQComponent::selectSlot(int slot)
{
    storeActiveSlot();
    activeSlot = juce::jlimit(0, 1, slot);
    processor.setMainOutputEQ24State(abStates[(size_t)activeSlot]);
    updateUIFromProcessor();
}

void MainOutputEQComponent::sliderValueChanged(juce::Slider* slider)
{
    if (updatingUI)
        return;

    for (int band = 0; band < numMainOutputEQBands; ++band)
    {
        if (slider == &gainSliders[(size_t)band])
        {
            processor.params.mainOutputEQGainDb[(size_t)band].store(
                (float)slider->getValue(), std::memory_order_relaxed);
            processor.updateMainOutputEQCoefficients();
            storeActiveSlot();
            responseDisplay->repaint();
            return;
        }
    }

    if (slider == &frequencySlider)
        processor.params.mainOutputEQFreq[(size_t)selectedBand].store(
            (float)frequencySlider.getValue(), std::memory_order_relaxed);
    else if (slider == &qSlider)
        processor.params.mainOutputEQQ[(size_t)selectedBand].store(
            (float)qSlider.getValue(), std::memory_order_relaxed);
    else if (slider == &dynamicThresholdSlider)
        processor.params.mainOutputEQDynamicThresholdDb[(size_t)selectedBand].store(
            (float)dynamicThresholdSlider.getValue(), std::memory_order_relaxed);
    else if (slider == &dynamicAttackSlider)
        processor.params.mainOutputEQDynamicAttackMs[(size_t)selectedBand].store(
            (float)dynamicAttackSlider.getValue(), std::memory_order_relaxed);
    else if (slider == &dynamicReleaseSlider)
        processor.params.mainOutputEQDynamicReleaseMs[(size_t)selectedBand].store(
            (float)dynamicReleaseSlider.getValue(), std::memory_order_relaxed);
    else if (slider == &dynamicRangeSlider)
        processor.params.mainOutputEQDynamicRangeDb[(size_t)selectedBand].store(
            (float)dynamicRangeSlider.getValue(), std::memory_order_relaxed);
    else
        return;

    if (slider == &frequencySlider || slider == &qSlider)
        processor.updateMainOutputEQCoefficients();
    storeActiveSlot();
    responseDisplay->repaint();
}

void MainOutputEQComponent::buttonClicked(juce::Button* button)
{
    if (button == &bandSoloToggle)
    {
        const int soloBand = bandSoloToggle.getToggleState() ? selectedBand : -1;
        processor.setMainOutputEQSoloBand(soloBand);
        bandSoloToggle.setColour(juce::ToggleButton::textColourId,
            soloBand >= 0 ? juce::Colour(0xffffb74d) : juce::Colour(0xffcbd5e1));
    }
    else if (button == &analyzerEnabledToggle)
    {
        const bool enabled = analyzerEnabledToggle.getToggleState();
        processor.setMainOutputEQAnalyzerEnabled(enabled);
        if (!enabled)
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
            peakAnalysis = {};
            lastAnalyzerUpdateMs = 0.0;
        }
        responseDisplay->repaint();
    }
    else if (button == &peakHoldToggle)
    {
        prePeakHoldDb = preSpectrumDb;
        preRightPeakHoldDb = preRightSpectrumDb;
        postPeakHoldDb = postSpectrumDb;
        postRightPeakHoldDb = postRightSpectrumDb;
        responseDisplay->repaint();
    }
    else if (button == &enabledToggle)
    {
        processor.params.mainOutputEQEnabled.store(
            enabledToggle.getToggleState(), std::memory_order_relaxed);
        storeActiveSlot();
        responseDisplay->repaint();
    }
    else if (button == &bandEnabledToggle)
    {
        processor.params.mainOutputEQBandEnabled[(size_t)selectedBand].store(
            bandEnabledToggle.getToggleState(), std::memory_order_relaxed);
        storeActiveSlot();
        responseDisplay->repaint();
    }
    else if (button == &dynamicEnabledToggle)
    {
        processor.params.mainOutputEQDynamicEnabled[(size_t)selectedBand].store(
            dynamicEnabledToggle.getToggleState(), std::memory_order_relaxed);
        storeActiveSlot();
    }
    else if (button == &slotAButton)
        selectSlot(0);
    else if (button == &slotBButton)
        selectSlot(1);
    else if (button == &copyButton)
    {
        copiedState = processor.getMainOutputEQ24State();
        hasCopiedState = true;
        pasteButton.setEnabled(true);
    }
    else if (button == &pasteButton && hasCopiedState)
    {
        processor.setMainOutputEQ24State(copiedState);
        storeActiveSlot();
        updateUIFromProcessor();
    }
}

void MainOutputEQComponent::comboBoxChanged(juce::ComboBox* combo)
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

        auto state = processor.getMainOutputEQ24State();
        if (!LiveBus::EQPresetCatalog::mapToMainOutput24Band(
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
                "24-band Main Output EQ frequency grid. Continue?",
                "Apply", "Cancel"))
        {
            presetCombo.setSelectedId(1, juce::dontSendNotification);
            return;
        }

        processor.setMainOutputEQ24State(state);
        storeActiveSlot();
        updateUIFromProcessor();
        return;
    }

    if (combo == &averagingTimeCombo)
        return;
    if (combo == &fftSizeCombo)
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
        peakAnalysis = {};
        lastAnalyzerUpdateMs = 0.0;
        responseDisplay->repaint();
        return;
    }
    if (combo == &spectrumModeCombo)
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
        peakAnalysis = {};
        lastAnalyzerUpdateMs = 0.0;
        responseDisplay->repaint();
        return;
    }
    if (combo == &selectedBandCombo)
    {
        setSelectedBand(selectedBandCombo.getSelectedId() - 1);
        return;
    }
    if (combo != &filterTypeCombo)
    {
        if (combo == &dynamicDetectorCombo)
        {
            processor.params.mainOutputEQDynamicDetector[(size_t)selectedBand].store(
                juce::jlimit(0, 1, dynamicDetectorCombo.getSelectedId() - 1),
                std::memory_order_relaxed);
            storeActiveSlot();
        }
        return;
    }

    processor.params.mainOutputEQType[(size_t)selectedBand].store(
        juce::jlimit(0, 5, filterTypeCombo.getSelectedId() - 1),
        std::memory_order_relaxed);
    processor.updateMainOutputEQCoefficients();
    storeActiveSlot();
    responseDisplay->repaint();
}

void MainOutputEQComponent::refreshPresetCatalog()
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
    presetCombo.setSelectedId(1, juce::dontSendNotification);
}

void MainOutputEQComponent::ResponseDisplay::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();
    graphics.fillAll(juce::Colour(0xff090d14));
    graphics.setColour(juce::Colour(0xff1e293b));
    graphics.drawRect(bounds, 1.0f);
    const auto drawSpectrum = [&](const std::array<float, analyzerDisplayBins>& spectrum,
                                  bool hasSpectrum, bool visible, juce::Colour colour)
    {
        if (!hasSpectrum || !visible)
            return;
        juce::Path path;
        for (int bin = 0; bin < analyzerDisplayBins; ++bin)
        {
            const float x = (float)bin / (float)(analyzerDisplayBins - 1) * width;
            const float level = juce::jlimit(-90.0f, 0.0f, spectrum[(size_t)bin]);
            const float y = height - (level + 90.0f) / 90.0f * height;
            if (bin == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        graphics.setColour(colour);
        graphics.strokePath(path, juce::PathStrokeType(1.25f));
    };
    drawSpectrum(owner.preSpectrumDb, owner.hasPreSpectrum,
                 owner.preSpectrumToggle.getToggleState(), juce::Colour(0xff38bdf8));
    if (owner.spectrumModeCombo.getSelectedId() == 2)
        drawSpectrum(owner.preRightSpectrumDb, owner.hasPreSpectrum,
                     owner.preSpectrumToggle.getToggleState(),
                     juce::Colour(0xff818cf8));
    drawSpectrum(owner.postSpectrumDb, owner.hasPostSpectrum,
                 owner.postSpectrumToggle.getToggleState(), juce::Colour(0xfffbbf24));
    if (owner.spectrumModeCombo.getSelectedId() == 2)
        drawSpectrum(owner.postRightSpectrumDb, owner.hasPostSpectrum,
                     owner.postSpectrumToggle.getToggleState(),
                     juce::Colour(0xfffb7185));
    if (owner.peakHoldToggle.getToggleState())
    {
        drawSpectrum(owner.prePeakHoldDb, owner.hasPreSpectrum,
                     owner.preSpectrumToggle.getToggleState(), juce::Colour(0xffa5e8ff));
        if (owner.spectrumModeCombo.getSelectedId() == 2)
            drawSpectrum(owner.preRightPeakHoldDb, owner.hasPreSpectrum,
                         owner.preSpectrumToggle.getToggleState(),
                         juce::Colour(0xffc4b5fd));
        drawSpectrum(owner.postPeakHoldDb, owner.hasPostSpectrum,
                     owner.postSpectrumToggle.getToggleState(), juce::Colour(0xffffdc8a));
        if (owner.spectrumModeCombo.getSelectedId() == 2)
            drawSpectrum(owner.postRightPeakHoldDb, owner.hasPostSpectrum,
                         owner.postSpectrumToggle.getToggleState(),
                         juce::Colour(0xfffda4af));
    }
    const auto xForFrequency = [width](float frequency)
    {
        return std::log10(frequency / 20.0f) / std::log10(1000.0f) * width;
    };
    if (owner.analyzerEnabledToggle.getToggleState())
    {
        for (std::size_t index = 0; index < owner.peakAnalysis.count; ++index)
        {
            const auto& candidate = owner.peakAnalysis.candidates[index];
            const float x = juce::jlimit(0.0f, width,
                xForFrequency(candidate.frequencyHz));
            const float y = height - (candidate.levelDb + 90.0f) / 90.0f * height;
            graphics.setColour(juce::Colour(0xffff8c42));
            graphics.fillEllipse(x - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }

        const float panelWidth = juce::jmin(196.0f, width - 12.0f);
        const juce::Rectangle<float> panel(width - panelWidth - 6.0f,
                                            16.0f, panelWidth, 54.0f);
        graphics.setColour(juce::Colour(0xdd111827));
        graphics.fillRoundedRectangle(panel, 4.0f);
        graphics.setFont(juce::FontOptions(9.0f));
        graphics.setColour(juce::Colour(0xffffb36b));
        graphics.drawText("Peak candidates · suggested, not applied",
                          panel.toNearestInt().withHeight(14), juce::Justification::centredLeft);
        graphics.setColour(juce::Colour(0xffe2e8f0));
        if (owner.peakAnalysis.count == 0)
        {
            graphics.drawText("No prominent peaks detected",
                              panel.toNearestInt().withTrimmedTop(15),
                              juce::Justification::centredLeft);
        }
        else
        {
            for (std::size_t index = 0; index < owner.peakAnalysis.count; ++index)
            {
                const auto& candidate = owner.peakAnalysis.candidates[index];
                const juce::String frequencyText = candidate.frequencyHz >= 1000.0f
                    ? juce::String(candidate.frequencyHz / 1000.0f, 2) + " kHz"
                    : juce::String(candidate.frequencyHz, 0) + " Hz";
                graphics.drawText(
                    frequencyText + " · try -3 dB, Q 4",
                    panel.toNearestInt().withY((int)panel.getY() + 14 + (int)index * 12)
                        .withHeight(12),
                    juce::Justification::centredLeft);
            }
        }
    }
    const auto yForGain = [height](float gainDb)
    {
        return (height - 12.0f) -
               (juce::jlimit(-18.0f, 18.0f, gainDb) + 18.0f) / 36.0f *
                   (height - 24.0f);
    };

    graphics.setFont(juce::FontOptions(9.0f));
    for (const float frequency : { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
    {
        const float x = xForFrequency(frequency);
        graphics.setColour(juce::Colour(0x2238bdf8));
        graphics.drawVerticalLine((int)x, 0.0f, height - 12.0f);
        graphics.setColour(juce::Colour(0xff94a3b8));
        const juce::String label = frequency >= 1000.0f
            ? juce::String(frequency / 1000.0f, 0) + "k"
            : juce::String((int)frequency);
        graphics.drawText(label, (int)x - 18, (int)height - 12, 36, 11,
                          juce::Justification::centred);
    }
    for (const float gain : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
    {
        const float y = yForGain(gain);
        graphics.setColour(gain == 0.0f ? juce::Colour(0x6638bdf8)
                                        : juce::Colour(0x2238bdf8));
        graphics.drawHorizontalLine((int)y, 0.0f, width);
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.drawText(juce::String((int)gain), 2, (int)y - 5, 28, 10,
                          juce::Justification::left);
    }

    juce::Path response;
    constexpr int pointCount = 240;
    for (int point = 0; point <= pointCount; ++point)
    {
        const float normalized = (float)point / (float)pointCount;
        const float frequency = 20.0f * std::pow(1000.0f, normalized);
        const float gainDb = owner.enabledToggle.getToggleState()
            ? owner.processor.getMainOutputEQ24MagnitudeDb(frequency) : 0.0f;
        const float y = yForGain(gainDb);
        const float x = normalized * width;
        if (point == 0)
            response.startNewSubPath(x, y);
        else
            response.lineTo(x, y);
    }
    graphics.setColour(juce::Colour(0xffe040d0));
    graphics.strokePath(response, juce::PathStrokeType(2.0f));
    graphics.setFont(juce::FontOptions(9.0f));
    graphics.setColour(juce::Colour(0xff94a3b8));
    for (const float level : { 0.0f, -30.0f, -60.0f, -90.0f })
    {
        const float y = height - (level + 90.0f) / 90.0f * height;
        graphics.drawText(juce::String((int)level), (int)width - 28, (int)y - 5,
                          26, 10, juce::Justification::right);
    }
    graphics.setColour(juce::Colour(0xff38bdf8));
    graphics.drawText("Pre dBFS", 8, 2, 60, 12, juce::Justification::left);
    graphics.setColour(juce::Colour(0xfffbbf24));
    graphics.drawText("Post dBFS", 72, 2, 65, 12, juce::Justification::left);
}
