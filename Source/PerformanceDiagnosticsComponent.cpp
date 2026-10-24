#include "PerformanceDiagnosticsComponent.h"
#include <cmath>

namespace LiveBus
{

// =============================================================================
// NotchedBufferSizeSlider Implementation
// =============================================================================
NotchedBufferSizeSlider::NotchedBufferSizeSlider()
{
    setOpaque(false);
}

void NotchedBufferSizeSlider::setSupportedBufferSizes(const juce::Array<int>& sizes)
{
    if (!sizes.isEmpty())
        supportedSizes = sizes;
    repaint();
}

void NotchedBufferSizeSlider::setCurrentBufferSize(int sizeSamples, double sampleRate)
{
    currentSize = sizeSamples;
    currentSampleRate = sampleRate;
    currentIndex = getIndexForSize(sizeSamples);
    repaint();
}

int NotchedBufferSizeSlider::getIndexForSize(int size) const
{
    int bestIdx = 0;
    int bestDiff = 999999;
    for (int i = 0; i < standardSizes.size(); ++i)
    {
        int diff = std::abs(standardSizes[i] - size);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void NotchedBufferSizeSlider::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float trackY = bounds.getY() + 38.0f;
    float trackLeft = bounds.getX() + 24.0f;
    float trackRight = bounds.getRight() - 24.0f;
    float trackWidth = trackRight - trackLeft;

    int numNotches = standardSizes.size();
    float stepX = (numNotches > 1) ? (trackWidth / (float)(numNotches - 1)) : trackWidth;

    // Background track rail
    g.setColour(juce::Colour(0xff12141a));
    g.fillRoundedRectangle(trackLeft - 2.0f, trackY - 3.0f, trackWidth + 4.0f, 6.0f, 3.0f);
    g.setColour(juce::Colour(0xff2a2e39));
    g.drawRoundedRectangle(trackLeft - 2.0f, trackY - 3.0f, trackWidth + 4.0f, 6.0f, 3.0f, 1.0f);

    // Active track fill up to thumb
    float thumbX = trackLeft + currentIndex * stepX;
    g.setColour(juce::Colour(0xff00f5d4).withAlpha(0.35f));
    g.fillRoundedRectangle(trackLeft, trackY - 2.0f, thumbX - trackLeft, 4.0f, 2.0f);

    // Ticks and Labels
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));

    for (int i = 0; i < numNotches; ++i)
    {
        float x = trackLeft + i * stepX;
        bool isSupported = supportedSizes.contains(standardSizes[i]);

        // Vertical tick lines
        g.setColour(isSupported ? juce::Colour(0xff64748b) : juce::Colour(0xff334155));
        g.drawLine(x, trackY - 8.0f, x, trackY + 8.0f, 1.5f);

        // Standard labels above
        juce::String labelStr = juce::String(standardSizes[i]);
        g.setColour((i == currentIndex) ? juce::Colour(0xfff0f6fc) : (isSupported ? juce::Colour(0xff94a3b8) : juce::Colour(0xff475569)));
        g.drawText(labelStr, (int)(x - 20.0f), (int)(trackY - 26.0f), 40, 16, juce::Justification::centred);
    }

    // White thumb capsule (Matching reference screenshot)
    float thumbW = 12.0f;
    float thumbH = 22.0f;
    juce::Rectangle<float> thumbRect(thumbX - thumbW * 0.5f, trackY - thumbH * 0.5f, thumbW, thumbH);

    // Subtle drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(thumbRect.translated(0.0f, 2.0f), 3.0f);

    // Pill body
    g.setColour(juce::Colour(0xfff8fafc));
    g.fillRoundedRectangle(thumbRect, 3.0f);
    g.setColour(juce::Colour(0xffcbd5e1));
    g.drawRoundedRectangle(thumbRect, 3.0f, 1.0f);

    // Thumb center grip lines
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawLine(thumbX, trackY - 4.0f, thumbX, trackY + 4.0f, 1.0f);
}

void NotchedBufferSizeSlider::handleMousePosition(int mouseX)
{
    auto bounds = getLocalBounds().toFloat();
    float trackLeft = bounds.getX() + 24.0f;
    float trackRight = bounds.getRight() - 24.0f;
    float trackWidth = trackRight - trackLeft;

    int numNotches = standardSizes.size();
    if (numNotches <= 1 || trackWidth <= 1.0f) return;

    float stepX = trackWidth / (float)(numNotches - 1);
    float clampedX = juce::jlimit(trackLeft, trackRight, (float)mouseX);
    int nearestIdx = (int)std::round((clampedX - trackLeft) / stepX);
    nearestIdx = juce::jlimit(0, numNotches - 1, nearestIdx);

    if (nearestIdx != currentIndex)
    {
        currentIndex = nearestIdx;
        currentSize = standardSizes[currentIndex];
        repaint();
        if (onBufferSizeChanged)
            onBufferSizeChanged(currentSize);
    }
}

void NotchedBufferSizeSlider::mouseDown(const juce::MouseEvent& e)
{
    isDragging = true;
    handleMousePosition(e.x);
}

void NotchedBufferSizeSlider::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging)
        handleMousePosition(e.x);
}

// =============================================================================
// LogListBoxModel Implementation
// =============================================================================
PerformanceDiagnosticsComponent::LogListBoxModel::LogListBoxModel(PerformanceDiagnosticsComponent& owner)
    : parent(owner)
{
}

int PerformanceDiagnosticsComponent::LogListBoxModel::getNumRows()
{
    return (int)parent.displayedEvents.size();
}

void PerformanceDiagnosticsComponent::LogListBoxModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int)parent.displayedEvents.size())
        return;

    const auto& ev = parent.displayedEvents[(size_t)rowNumber];

    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff1f293d));
    else if (rowNumber % 2 == 1)
        g.fillAll(juce::Colour(0xff12141a).withAlpha(0.4f));

    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);

    // Timestamp
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff64748b));
    g.drawText(ev.getFormattedTime(), bounds.removeFromLeft(90), juce::Justification::centredLeft);

    // Severity Pill Badge
    auto sevBounds = bounds.removeFromLeft(60).reduced(2, 2);
    auto sevCol = ev.getSeverityColour();
    g.setColour(sevCol.withAlpha(0.2f));
    g.fillRoundedRectangle(sevBounds.toFloat(), 3.0f);
    g.setColour(sevCol);
    g.drawRoundedRectangle(sevBounds.toFloat(), 3.0f, 1.0f);
    g.drawText(ev.getSeverityString(), sevBounds, juce::Justification::centred);

    bounds.removeFromLeft(8);

    // Category Pill
    auto catBounds = bounds.removeFromLeft(80).reduced(2, 2);
    g.setColour(juce::Colour(0xff1e293b));
    g.fillRoundedRectangle(catBounds.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawRoundedRectangle(catBounds.toFloat(), 3.0f, 1.0f);
    g.drawText(ev.getCategoryString(), catBounds, juce::Justification::centred);

    bounds.removeFromLeft(10);

    // Message
    g.setColour(juce::Colour(0xffe2e8f0));
    g.drawText(juce::String(ev.message), bounds, juce::Justification::centredLeft, true);
}

// =============================================================================
// PerformanceDiagnosticsComponent Implementation
// =============================================================================
PerformanceDiagnosticsComponent::PerformanceDiagnosticsComponent(AudioDeviceService& devService,
                                                                 AudioPerformanceMonitor& perfMon,
                                                                 DiagnosticsLogManager& logMgr,
                                                                 VST3HostEngine* vst3Engine,
                                                                 RoutingMatrix* routingMatrix)
    : service(devService),
      monitor(perfMon),
      logManager(logMgr),
      hostEngine(vst3Engine),
      routing(routingMatrix),
      logListModel(*this)
{
    service.setListener(this);

    // -------------------------------------------------------------------------
    // Card 1: Audio Settings (Exact Reference Screenshot)
    // -------------------------------------------------------------------------
    cardTitleLabel.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    cardTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(cardTitleLabel);

    interfaceLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    interfaceLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(interfaceLabel);

    interfaceCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff14161c));
    interfaceCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a2e39));
    interfaceCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(interfaceCombo);

    sampleRateLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    sampleRateLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(sampleRateLabel);

    sampleRateCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff14161c));
    sampleRateCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a2e39));
    sampleRateCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(sampleRateCombo);

    bufferSizeTitleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    bufferSizeTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(bufferSizeTitleLabel);

    bufferSizeTopValLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    bufferSizeTopValLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    bufferSizeTopValLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bufferSizeTopValLabel);

    addAndMakeVisible(bufferSlider);
    bufferSlider.onBufferSizeChanged = [this](int newSamples) {
        if (!isUpdatingUI)
        {
            auto err = service.setBufferSize(newSamples);
            if (err.isNotEmpty())
            {
                logManager.addEvent(DiagnosticEvent::Severity::Error, DiagnosticEvent::Category::Buffer,
                                    "Failed to set buffer size: " + err);
            }
            else
            {
                logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::Buffer,
                                    "Buffer size changed to " + juce::String(newSamples) + " samples");
            }
            refreshFromHardware();
        }
    };

    bufferSizeBottomValLabel.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    bufferSizeBottomValLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    bufferSizeBottomValLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bufferSizeBottomValLabel);

    inputLatencyPrefixLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    inputLatencyPrefixLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(inputLatencyPrefixLabel);

    inputLatencyValueLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    inputLatencyValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4)); // Cyan / Teal
    addAndMakeVisible(inputLatencyValueLabel);

    outputLatencyPrefixLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    outputLatencyPrefixLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(outputLatencyPrefixLabel);

    outputLatencyValueLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    outputLatencyValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4)); // Cyan / Teal
    addAndMakeVisible(outputLatencyValueLabel);

    latencyNoteLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    latencyNoteLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(latencyNoteLabel);

    interfaceDriverLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    interfaceDriverLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(interfaceDriverLabel);

    interfaceDriverCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff14161c));
    interfaceDriverCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a2e39));
    interfaceDriverCombo.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(interfaceDriverCombo);

    cpuLoadLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    cpuLoadLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    addAndMakeVisible(cpuLoadLabel);

    cpuLoadValueLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    cpuLoadValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    cpuLoadValueLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(cpuLoadValueLabel);

    // -------------------------------------------------------------------------
    // Card 2: Real-Time Telemetry & Dropouts
    // -------------------------------------------------------------------------
    telemetryCardTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    telemetryCardTitle.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(telemetryCardTitle);

    engineStatusBadge.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
    engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    engineStatusBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(engineStatusBadge);

    warningBannerLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    warningBannerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb703));
    warningBannerLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ffb703));
    warningBannerLabel.setJustificationType(juce::Justification::centred);
    warningBannerLabel.setVisible(false);
    addAndMakeVisible(warningBannerLabel);

    clearStatsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    clearStatsBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    clearStatsBtn.onClick = [this]() {
        monitor.clearStatistics();
        logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, "Audio statistics counters cleared.");
    };
    addAndMakeVisible(clearStatsBtn);

    testToneBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    testToneBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    testToneBtn.onClick = [this]() {
        bool newState = !service.isTestToneEnabled();
        service.setTestToneEnabled(newState);
        testToneBtn.setColour(juce::TextButton::buttonColourId, newState ? juce::Colour(0xff00f5d4) : juce::Colour(0xff1f293d));
        testToneBtn.setColour(juce::TextButton::textColourOffId, newState ? juce::Colour(0xff0d1117) : juce::Colour(0xfff0f6fc));
        logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine,
                            newState ? "Safety test tone (440 Hz -18 dBFS) enabled" : "Safety test tone disabled");
    };
    addAndMakeVisible(testToneBtn);

    asioControlBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    asioControlBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    asioControlBtn.onClick = [this]() {
        if (service.hasControlPanel())
            service.openControlPanel();
    };
    addAndMakeVisible(asioControlBtn);

    // -------------------------------------------------------------------------
    // Card 3: Diagnostics Log & Reporting
    // -------------------------------------------------------------------------
    logCardTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    logCardTitle.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    addAndMakeVisible(logCardTitle);

    auto setupFilterBtn = [this](juce::TextButton& btn, DiagnosticsLogManager::FilterMode mode) {
        btn.onClick = [this, mode]() {
            currentFilterMode = mode;
            updateFilterButtons();
        };
        addAndMakeVisible(btn);
    };

    setupFilterBtn(filterAllBtn, DiagnosticsLogManager::FilterMode::All);
    setupFilterBtn(filterInfoBtn, DiagnosticsLogManager::FilterMode::InfoOnly);
    setupFilterBtn(filterWarnBtn, DiagnosticsLogManager::FilterMode::WarningAndError);
    setupFilterBtn(filterErrorBtn, DiagnosticsLogManager::FilterMode::ErrorOnly);

    logListBox.setModel(&logListModel);
    logListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d1017));
    logListBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff1e293b));
    logListBox.setRowHeight(24);
    addAndMakeVisible(logListBox);

    clearLogBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    clearLogBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    clearLogBtn.onClick = [this]() {
        logManager.clearLog();
        updateFilterButtons();
    };
    addAndMakeVisible(clearLogBtn);

    exportLogBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    exportLogBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    exportLogBtn.onClick = [this]() {
        juce::FileChooser chooser("Save Diagnostic Event Log",
                                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                      .getChildFile("LiveBusMixer_Log_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".txt"),
                                  "*.txt");
        if (chooser.browseForFileToSave(true))
        {
            auto f = chooser.getResult();
            if (logManager.exportLogToFile(f))
                logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, "Exported event log to " + f.getFileName());
        }
    };
    addAndMakeVisible(exportLogBtn);

    previewReportBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    previewReportBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    previewReportBtn.onClick = [this]() {
        showReportPreviewModal();
    };
    addAndMakeVisible(previewReportBtn);

    exportReportBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f5d4));
    exportReportBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0d1117));
    exportReportBtn.onClick = [this]() {
        juce::FileChooser chooser("Save Diagnostic Report",
                                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                      .getChildFile("LiveBusMixer_Report_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".txt"),
                                  "*.txt");
        if (chooser.browseForFileToSave(true))
        {
            auto f = chooser.getResult();
            auto rep = logManager.generateDiagnosticReport(service, monitor, hostEngine, routing);
            if (logManager.exportReportToFile(f, rep))
                logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, "Exported full report to " + f.getFileName());
        }
    };
    addAndMakeVisible(exportReportBtn);

    // Initial setup
    refreshFromHardware();
    updateFilterButtons();

    // Start timer for real-time monitoring (30 Hz)
    startTimerHz(30);
}

PerformanceDiagnosticsComponent::~PerformanceDiagnosticsComponent()
{
    stopTimer();
    service.setListener(nullptr);
}

void PerformanceDiagnosticsComponent::updateFilterButtons()
{
    auto applyStyle = [](juce::TextButton& btn, bool active, juce::Colour activeColor) {
        btn.setColour(juce::TextButton::buttonColourId, active ? activeColor : juce::Colour(0xff1f293d));
        btn.setColour(juce::TextButton::textColourOffId, active ? juce::Colour(0xff0d1117) : juce::Colour(0xfff0f6fc));
    };

    applyStyle(filterAllBtn, currentFilterMode == DiagnosticsLogManager::FilterMode::All, juce::Colour(0xff00f5d4));
    applyStyle(filterInfoBtn, currentFilterMode == DiagnosticsLogManager::FilterMode::InfoOnly, juce::Colour(0xff00f5d4));
    applyStyle(filterWarnBtn, currentFilterMode == DiagnosticsLogManager::FilterMode::WarningAndError, juce::Colour(0xffffb703));
    applyStyle(filterErrorBtn, currentFilterMode == DiagnosticsLogManager::FilterMode::ErrorOnly, juce::Colour(0xffff4d6d));

    displayedEvents = logManager.getFilteredEvents(currentFilterMode);
    logListBox.updateContent();
    logListBox.repaint();
}

void PerformanceDiagnosticsComponent::showReportPreviewModal()
{
    auto reportText = logManager.generateDiagnosticReport(service, monitor, hostEngine, routing);
    auto* dialog = new DiagnosticReportPreviewDialog(reportText, logManager);
    dialog->enterModalState(true, nullptr, true);
}

void PerformanceDiagnosticsComponent::refreshFromHardware()
{
    isUpdatingUI = true;

    // 1. Interface Driver / Backend Combo
    interfaceDriverCombo.clear(juce::dontSendNotification);
    auto backends = service.getAvailableBackendTypes();
    int activeBackendId = 1;
    for (int i = 0; i < backends.size(); ++i)
    {
        interfaceDriverCombo.addItem(backends[i], i + 1);
        if (backends[i] == service.getCurrentBackendType())
            activeBackendId = i + 1;
    }
    interfaceDriverCombo.setSelectedId(activeBackendId, juce::dontSendNotification);
    interfaceDriverCombo.onChange = [this]() {
        if (!isUpdatingUI)
        {
            auto selected = interfaceDriverCombo.getText();
            service.setBackendType(selected);
            refreshFromHardware();
        }
    };

    // 2. Interface (Output Device Name)
    interfaceCombo.clear(juce::dontSendNotification);
    auto outDevices = service.getAvailableOutputDevices();
    int activeOutId = 1;
    juce::String currentOut = service.getCurrentOutputDevice();
    for (int i = 0; i < outDevices.size(); ++i)
    {
        interfaceCombo.addItem(outDevices[i], i + 1);
        if (outDevices[i] == currentOut)
            activeOutId = i + 1;
    }
    interfaceCombo.setSelectedId(activeOutId, juce::dontSendNotification);
    interfaceCombo.onChange = [this]() {
        if (!isUpdatingUI)
        {
            service.setOutputDevice(interfaceCombo.getText());
            refreshFromHardware();
        }
    };

    // 3. Sample Rate Combo
    sampleRateCombo.clear(juce::dontSendNotification);
    auto rates = service.getAvailableSampleRates();
    double currentRate = service.getCurrentSampleRate();
    int activeRateId = 1;
    for (int i = 0; i < rates.size(); ++i)
    {
        juce::String rateStr;
        if (rates[i] >= 1000.0)
            rateStr = juce::String(rates[i] / 1000.0, (rates[i] == 44100.0 || rates[i] == 88200.0) ? 1 : 0) + " kHz";
        else
            rateStr = juce::String(rates[i], 0) + " Hz";

        sampleRateCombo.addItem(rateStr, i + 1);
        if (std::abs(rates[i] - currentRate) < 1.0)
            activeRateId = i + 1;
    }
    sampleRateCombo.setSelectedId(activeRateId, juce::dontSendNotification);
    sampleRateCombo.onChange = [this]() {
        if (!isUpdatingUI)
        {
            int idx = sampleRateCombo.getSelectedId() - 1;
            auto available = service.getAvailableSampleRates();
            if (idx >= 0 && idx < available.size())
            {
                service.setSampleRate(available[idx]);
                refreshFromHardware();
            }
        }
    };

    // 4. Buffer Size
    auto sizes = service.getAvailableBufferSizes();
    bufferSlider.setSupportedBufferSizes(sizes);
    int currentBuf = service.getCurrentBufferSize();
    bufferSlider.setCurrentBufferSize(currentBuf, currentRate);

    float bufDurationMs = AudioDeviceService::calculateBufferDurationMs(currentBuf, currentRate);
    bufferSizeTopValLabel.setText(juce::String(currentBuf) + " Samples", juce::dontSendNotification);
    bufferSizeBottomValLabel.setText(juce::String(currentBuf) + " Samples (" + juce::String(bufDurationMs, 2) + " ms)", juce::dontSendNotification);

    // 5. Latency Values
    float inLatMs = service.getInputLatencyMs();
    float outLatMs = service.getOutputLatencyMs();
    inputLatencyValueLabel.setText((inLatMs >= 0.0f) ? (juce::String(inLatMs, 1) + "ms") : "N/A", juce::dontSendNotification);
    outputLatencyValueLabel.setText((outLatMs >= 0.0f) ? (juce::String(outLatMs, 1) + "ms") : "N/A", juce::dontSendNotification);
    latencyNoteLabel.setText("(Driver reported values; 1 buffer duration: " + juce::String(bufDurationMs, 2) + " ms)", juce::dontSendNotification);

    // 6. ASIO Control Panel button
    asioControlBtn.setEnabled(service.hasControlPanel());

    // 7. Update Telemetry Tiles from Snapshot
    auto snap = monitor.getSnapshot();
    currentCpuLoadDisplay = snap.currentCpuLoadPercent;
    cpuLoadValueLabel.setText(juce::String(currentCpuLoadDisplay, 1) + "%", juce::dontSendNotification);

    telemetryTiles[0] = { "Average Audio Load", juce::String(snap.averageCpuLoadPercent, 1) + "%",
                          snap.averageCpuLoadPercent > 80.0f ? juce::Colour(0xffffb703) : juce::Colour(0xff00f5d4) };
    telemetryTiles[1] = { "Peak Audio Load", juce::String(snap.maxCpuLoadPercent, 1) + "%",
                          snap.maxCpuLoadPercent > 90.0f ? juce::Colour(0xffff4d6d) : juce::Colour(0xfff0f6fc) };
    telemetryTiles[2] = { "Callback Duration", juce::String(snap.currentProcessingTimeMs, 2) + " ms (" + juce::String(bufDurationMs, 2) + " ms budget)",
                          snap.currentProcessingTimeMs > bufDurationMs ? juce::Colour(0xffff4d6d) : juce::Colour(0xfff0f6fc) };
    telemetryTiles[3] = { "Deadline Misses", juce::String(snap.deadlineMissCount),
                          snap.deadlineMissCount > 0 ? juce::Colour(0xffff4d6d) : juce::Colour(0xff10b981) };
    telemetryTiles[4] = { "Driver Dropouts", juce::String(snap.dropoutCount),
                          snap.dropoutCount > 0 ? juce::Colour(0xffffb703) : juce::Colour(0xff10b981) };
    telemetryTiles[5] = { "Hardware Errors", juce::String(snap.deviceErrorCount),
                          snap.deviceErrorCount > 0 ? juce::Colour(0xffff4d6d) : juce::Colour(0xff10b981) };

    // Refresh Diagnostics Log Table
    displayedEvents = logManager.getFilteredEvents(currentFilterMode);
    logListBox.updateContent();

    isUpdatingUI = false;
    repaint();
}

void PerformanceDiagnosticsComponent::onDeviceStateChanged(DeviceState /*newState*/, const juce::String& message)
{
    logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::Device, message);
    refreshFromHardware();
}

void PerformanceDiagnosticsComponent::onDeviceSetupChanged()
{
    refreshFromHardware();
}

void PerformanceDiagnosticsComponent::timerCallback()
{
    // Drain real-time lock-free queue
    logManager.drainQueue(monitor.getQueue());
    displayedEvents = logManager.getFilteredEvents(currentFilterMode);
    logListBox.updateContent();

    // Query Snapshot
    auto snap = monitor.getSnapshot();
    currentCpuLoadDisplay = snap.currentCpuLoadPercent;
    cpuLoadValueLabel.setText(juce::String(currentCpuLoadDisplay, 1) + "%", juce::dontSendNotification);

    // Update Telemetry Tiles
    double sr = service.getCurrentSampleRate();
    int bufSamples = service.getCurrentBufferSize();
    float bufMs = AudioDeviceService::calculateBufferDurationMs(bufSamples, sr);

    telemetryTiles[0] = { "Average Audio Load", juce::String(snap.averageCpuLoadPercent, 1) + "%",
                          snap.averageCpuLoadPercent > 80.0f ? juce::Colour(0xffffb703) : juce::Colour(0xff00f5d4) };
    telemetryTiles[1] = { "Peak Audio Load", juce::String(snap.maxCpuLoadPercent, 1) + "%",
                          snap.maxCpuLoadPercent > 90.0f ? juce::Colour(0xffff4d6d) : juce::Colour(0xfff0f6fc) };
    telemetryTiles[2] = { "Callback Duration", juce::String(snap.currentProcessingTimeMs, 2) + " ms (" + juce::String(bufMs, 2) + " ms budget)",
                          snap.currentProcessingTimeMs > bufMs ? juce::Colour(0xffff4d6d) : juce::Colour(0xfff0f6fc) };
    telemetryTiles[3] = { "Deadline Misses", juce::String(snap.deadlineMissCount),
                          snap.deadlineMissCount > 0 ? juce::Colour(0xffff4d6d) : juce::Colour(0xff10b981) };
    telemetryTiles[4] = { "Driver Dropouts", juce::String(snap.dropoutCount),
                          snap.dropoutCount > 0 ? juce::Colour(0xffffb703) : juce::Colour(0xff10b981) };
    telemetryTiles[5] = { "Hardware Errors", juce::String(snap.deviceErrorCount),
                          snap.deviceErrorCount > 0 ? juce::Colour(0xffff4d6d) : juce::Colour(0xff10b981) };

    // Engine status badge
    auto devState = service.getDeviceState();
    if (devState == DeviceState::Active)
    {
        engineStatusBadge.setText("AUDIO ENGINE: ACTIVE", juce::dontSendNotification);
        engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x2210b981));
    }
    else if (devState == DeviceState::Reconnecting)
    {
        engineStatusBadge.setText("DEVICE RECONNECTING...", juce::dontSendNotification);
        engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffffb703));
        engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ffb703));
    }
    else
    {
        engineStatusBadge.setText("ENGINE: DISCONNECTED", juce::dontSendNotification);
        engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffff4d6d));
        engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0x22ff4d6d));
    }

    // Performance warning banner
    if (snap.hasWarning)
    {
        warningBannerLabel.setText("WARNING: " + snap.activeWarningMessage, juce::dontSendNotification);
        warningBannerLabel.setVisible(true);
    }
    else
    {
        warningBannerLabel.setVisible(false);
    }

    repaint();
}

void PerformanceDiagnosticsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117)); // Broadcast dark backdrop

    // Paint Card 1: Audio Settings (Exact Reference Look)
    auto c1Bounds = juce::Rectangle<int>(16, 16, 520, 360).toFloat();
    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(c1Bounds.translated(0.0f, 4.0f), 10.0f);

    // Dark gradient background matching screenshot (#1c1e24 to #14151a)
    juce::ColourGradient grad1(juce::Colour(0xff1e2129), c1Bounds.getX(), c1Bounds.getY(),
                               juce::Colour(0xff13151b), c1Bounds.getX(), c1Bounds.getBottom(), false);
    g.setGradientFill(grad1);
    g.fillRoundedRectangle(c1Bounds, 10.0f);
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRoundedRectangle(c1Bounds, 10.0f, 1.2f);

    // Inner recessed box for Buffer Size
    auto bufBox = juce::Rectangle<int>(28, 126, 496, 100).toFloat();
    g.setColour(juce::Colour(0xff0e1015));
    g.fillRoundedRectangle(bufBox, 6.0f);
    g.setColour(juce::Colour(0xff232733));
    g.drawRoundedRectangle(bufBox, 6.0f, 1.0f);

    // Paint CPU Load progress bar (Teal gradient fill on dark track)
    auto cpuBar = juce::Rectangle<int>(130, 336, 394, 20).toFloat();
    g.setColour(juce::Colour(0xff0e1015));
    g.fillRoundedRectangle(cpuBar, 4.0f);
    g.setColour(juce::Colour(0xff232733));
    g.drawRoundedRectangle(cpuBar, 4.0f, 1.0f);

    float loadFraction = juce::jlimit(0.0f, 1.0f, currentCpuLoadDisplay / 100.0f);
    if (loadFraction > 0.001f)
    {
        auto fillBar = cpuBar.withWidth(cpuBar.getWidth() * loadFraction);
        juce::ColourGradient cpuGrad(juce::Colour(0xff00f5d4), fillBar.getX(), fillBar.getY(),
                                     juce::Colour(0xff00bbf9), fillBar.getRight(), fillBar.getY(), false);
        if (currentCpuLoadDisplay > 85.0f)
            g.setColour(juce::Colour(0xffff4d6d));
        else if (currentCpuLoadDisplay > 70.0f)
            g.setColour(juce::Colour(0xffffb703));
        else
            g.setGradientFill(cpuGrad);

        g.fillRoundedRectangle(fillBar, 4.0f);
    }

    // Paint Card 2: Real-Time Telemetry & Dropouts
    auto c2Bounds = juce::Rectangle<int>(16, 390, 520, getHeight() - 406).toFloat();
    g.setColour(juce::Colour(0xff161922));
    g.fillRoundedRectangle(c2Bounds, 10.0f);
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRoundedRectangle(c2Bounds, 10.0f, 1.0f);

    // Paint Telemetry Tiles
    int tileX = (int)c2Bounds.getX() + 16;
    int tileY = (int)c2Bounds.getY() + 48;
    int tileW = 236;
    int tileH = 50;

    for (size_t i = 0; i < telemetryTiles.size(); ++i)
    {
        int col = (int)i % 2;
        int row = (int)i / 2;
        auto tileBounds = juce::Rectangle<int>(tileX + col * (tileW + 16), tileY + row * (tileH + 10), tileW, tileH).toFloat();

        g.setColour(juce::Colour(0xff0e1015));
        g.fillRoundedRectangle(tileBounds, 6.0f);
        g.setColour(juce::Colour(0xff232733));
        g.drawRoundedRectangle(tileBounds, 6.0f, 1.0f);

        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText(telemetryTiles[i].title, (int)tileBounds.getX() + 8, (int)tileBounds.getY() + 6, (int)tileBounds.getWidth() - 16, 14, juce::Justification::left);

        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.setColour(telemetryTiles[i].valueColor);
        g.drawText(telemetryTiles[i].value, (int)tileBounds.getX() + 8, (int)tileBounds.getY() + 22, (int)tileBounds.getWidth() - 16, 22, juce::Justification::left);
    }

    // Paint Card 3: Diagnostics Log Container
    int c3X = 552;
    int c3W = getWidth() - c3X - 16;
    auto c3Bounds = juce::Rectangle<int>(c3X, 16, c3W, getHeight() - 32).toFloat();
    g.setColour(juce::Colour(0xff161922));
    g.fillRoundedRectangle(c3Bounds, 10.0f);
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRoundedRectangle(c3Bounds, 10.0f, 1.0f);
}

void PerformanceDiagnosticsComponent::resized()
{
    // Card 1 Controls (Exact pixel positions matching screenshot layout)
    cardTitleLabel.setBounds(28, 22, 200, 24);

    // Row 1: Interface
    interfaceLabel.setBounds(28, 54, 90, 24);
    interfaceCombo.setBounds(126, 54, 396, 26);

    // Row 2: Sample Rate
    sampleRateLabel.setBounds(28, 88, 120, 24);
    sampleRateCombo.setBounds(362, 88, 160, 26);

    // Row 3: Buffer Size Box
    bufferSizeTitleLabel.setBounds(38, 132, 120, 20);
    bufferSizeTopValLabel.setBounds(370, 132, 140, 20);
    bufferSlider.setBounds(32, 154, 488, 50);
    bufferSizeBottomValLabel.setBounds(300, 202, 210, 18);

    // Row 4: Latencies
    inputLatencyPrefixLabel.setBounds(28, 234, 95, 22);
    inputLatencyValueLabel.setBounds(124, 234, 60, 22);
    outputLatencyPrefixLabel.setBounds(200, 234, 105, 22);
    outputLatencyValueLabel.setBounds(306, 234, 60, 22);
    latencyNoteLabel.setBounds(28, 258, 490, 18);

    // Row 5: Interface Driver
    interfaceDriverLabel.setBounds(28, 286, 120, 24);
    interfaceDriverCombo.setBounds(150, 286, 372, 26);

    // Row 6: CPU Load
    cpuLoadLabel.setBounds(28, 334, 95, 24);
    cpuLoadValueLabel.setBounds(440, 316, 80, 18);

    // Card 2: Telemetry
    telemetryCardTitle.setBounds(32, 400, 280, 24);
    engineStatusBadge.setBounds(336, 400, 184, 24);

    int c2BottomY = getHeight() - 56;
    warningBannerLabel.setBounds(32, c2BottomY - 32, 488, 24);
    clearStatsBtn.setBounds(32, c2BottomY, 130, 28);
    testToneBtn.setBounds(172, c2BottomY, 160, 28);
    asioControlBtn.setBounds(342, c2BottomY, 178, 28);

    // Card 3: Diagnostics Log
    int c3X = 552;
    int c3W = getWidth() - c3X - 16;
    logCardTitle.setBounds(c3X + 16, 24, 220, 24);

    // Filter buttons
    int fBtnW = 60;
    filterAllBtn.setBounds(c3X + c3W - 270, 24, fBtnW, 24);
    filterInfoBtn.setBounds(c3X + c3W - 205, 24, fBtnW, 24);
    filterWarnBtn.setBounds(c3X + c3W - 140, 24, 70, 24);
    filterErrorBtn.setBounds(c3X + c3W - 65, 24, fBtnW, 24);

    // Log list box
    int logTop = 60;
    int logH = getHeight() - 130;
    logListBox.setBounds(c3X + 16, logTop, c3W - 32, logH);

    // Bottom action bar
    int bY = getHeight() - 56;
    clearLogBtn.setBounds(c3X + 16, bY, 90, 28);
    exportLogBtn.setBounds(c3X + 114, bY, 110, 28);
    previewReportBtn.setBounds(c3X + c3W - 370, bY, 140, 28);
    exportReportBtn.setBounds(c3X + c3W - 220, bY, 204, 28);
}

// =============================================================================
// DiagnosticReportPreviewDialog Implementation
// =============================================================================
DiagnosticReportPreviewDialog::DiagnosticReportPreviewDialog(const juce::String& reportContent, DiagnosticsLogManager& logMgr)
    : DocumentWindow("System & Audio Performance Diagnostic Report",
                     juce::Colour(0xff12151b),
                     DocumentWindow::closeButton),
      logManager(logMgr),
      fullReportText(reportContent)
{
    setUsingNativeTitleBar(true);

    auto* container = new juce::Component();
    container->setSize(760, 560);

    reportViewer.setMultiLine(true);
    reportViewer.setReadOnly(true);
    reportViewer.setScrollbarsShown(true);
    reportViewer.setCaretVisible(false);
    reportViewer.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
    reportViewer.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1017));
    reportViewer.setColour(juce::TextEditor::textColourId, juce::Colour(0xfff0f6fc));
    reportViewer.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2d3340));
    reportViewer.setText(reportContent);
    reportViewer.setBounds(16, 16, 728, 480);
    container->addAndMakeVisible(reportViewer);

    copyBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    copyBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    copyBtn.setBounds(16, 510, 140, 30);
    copyBtn.onClick = [this]() {
        juce::SystemClipboard::copyTextToClipboard(fullReportText);
    };
    container->addAndMakeVisible(copyBtn);

    saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f5d4));
    saveBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0d1117));
    saveBtn.setBounds(166, 510, 180, 30);
    saveBtn.onClick = [this]() {
        juce::FileChooser chooser("Save Diagnostic Report",
                                  juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                      .getChildFile("LiveBusMixer_DiagnosticReport_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".txt"),
                                  "*.txt");
        if (chooser.browseForFileToSave(true))
        {
            auto f = chooser.getResult();
            if (logManager.exportReportToFile(f, fullReportText))
                logManager.addEvent(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, "Saved diagnostic report to " + f.getFileName());
        }
    };
    container->addAndMakeVisible(saveBtn);

    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f293d));
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    closeBtn.setBounds(634, 510, 110, 30);
    closeBtn.onClick = [this]() {
        closeButtonPressed();
    };
    container->addAndMakeVisible(closeBtn);

    setContentOwned(container, true);
    centreWithSize(780, 600);
    setVisible(true);
}

void DiagnosticReportPreviewDialog::closeButtonPressed()
{
    exitModalState(0);
}

} // namespace LiveBus
