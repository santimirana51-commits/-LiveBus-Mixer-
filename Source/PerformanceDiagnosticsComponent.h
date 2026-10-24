#pragma once
#include <JuceHeader.h>
#include "AudioDeviceService.h"
#include "AudioPerformanceMonitor.h"
#include "DiagnosticsLogManager.h"
#include "VST3HostEngine.h"
#include "RoutingMatrix.h"

namespace LiveBus
{

/**
 * Notched Buffer Size Slider Component.
 * Pixel-accurate implementation matching the reference Audio Settings screenshot:
 * Standard buffer sizes: 32, 64, 128, 256, 512, 1024, 2048 with ticks, labels, and white thumb capsule.
 */
class NotchedBufferSizeSlider : public juce::Component
{
public:
    NotchedBufferSizeSlider();

    void setSupportedBufferSizes(const juce::Array<int>& sizes);
    void setCurrentBufferSize(int sizeSamples, double sampleRate);
    int getCurrentBufferSize() const noexcept { return currentSize; }

    std::function<void(int)> onBufferSizeChanged;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void handleMousePosition(int mouseX);
    int getIndexForSize(int size) const;

    juce::Array<int> standardSizes{32, 64, 128, 256, 512, 1024, 2048};
    juce::Array<int> supportedSizes{32, 64, 128, 256, 512, 1024, 2048};
    int currentSize = 128;
    double currentSampleRate = 48000.0;
    int currentIndex = 2; // Default 128
    bool isDragging = false;
};

/**
 * Professional Performance & Diagnostics Component.
 * Implements:
 * 1. Reference Audio Settings Card (Interface, Sample Rate, Buffer Size notch slider, Latency, Driver, CPU Load bar)
 * 2. Real-Time Telemetry & Dropouts Card (Audio load avg/max, deadline miss counter, dropouts, budget)
 * 3. Diagnostics Log & Report Card (Severity filters, log viewer, Clear, Export Log, Diagnostic Report)
 */
class PerformanceDiagnosticsComponent : public juce::Component,
                                        public juce::Timer,
                                        public AudioDeviceServiceListener
{
public:
    PerformanceDiagnosticsComponent(AudioDeviceService& devService,
                                    AudioPerformanceMonitor& perfMon,
                                    DiagnosticsLogManager& logMgr,
                                    VST3HostEngine* vst3Engine = nullptr,
                                    RoutingMatrix* routingMatrix = nullptr);
    ~PerformanceDiagnosticsComponent() override;

    void refreshFromHardware();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioDeviceServiceListener callbacks
    void onDeviceStateChanged(DeviceState newState, const juce::String& message) override;
    void onDeviceSetupChanged() override;

    // Timer callback for real-time telemetry (30 Hz)
    void timerCallback() override;

private:
    AudioDeviceService& service;
    AudioPerformanceMonitor& monitor;
    DiagnosticsLogManager& logManager;
    VST3HostEngine* hostEngine;
    RoutingMatrix* routing;

    bool isUpdatingUI{false};

    // =========================================================================
    // Card 1: Audio Settings (Exact Reference Screenshot)
    // =========================================================================
    juce::Label cardTitleLabel{"cardTitle", "Audio Settings"};

    juce::Label interfaceLabel{"ifaceLbl", "Interface:"};
    juce::ComboBox interfaceCombo;

    juce::Label sampleRateLabel{"srLbl", "Sample Rate:"};
    juce::ComboBox sampleRateCombo;

    juce::Label bufferSizeTitleLabel{"bufTitle", "Buffer Size"};
    juce::Label bufferSizeTopValLabel{"bufTopVal", "128 Samples"};
    NotchedBufferSizeSlider bufferSlider;
    juce::Label bufferSizeBottomValLabel{"bufBottomVal", "128 Samples"};

    juce::Label inputLatencyPrefixLabel{"inLatPfx", "Input Latency:"};
    juce::Label inputLatencyValueLabel{"inLatVal", "2.9ms"};
    juce::Label outputLatencyPrefixLabel{"outLatPfx", "Output Latency:"};
    juce::Label outputLatencyValueLabel{"outLatVal", "5.8ms"};
    juce::Label latencyNoteLabel{"latNote", "(Driver reported values; 1 buffer duration: 2.67 ms)"};

    juce::Label interfaceDriverLabel{"drvLbl", "Interface Driver:"};
    juce::ComboBox interfaceDriverCombo;

    juce::Label cpuLoadLabel{"cpuLbl", "CPU Load:"};
    float currentCpuLoadDisplay{0.0f};
    juce::Label cpuLoadValueLabel{"cpuVal", "0.0%"};

    // =========================================================================
    // Card 2: Real-Time Telemetry & Dropouts
    // =========================================================================
    juce::Label telemetryCardTitle{"telemTitle", "Real-Time Telemetry & Audio Stability"};
    juce::Label engineStatusBadge{"statusBadge", "AUDIO ENGINE: ACTIVE"};

    struct TelemetryTile
    {
        juce::String title;
        juce::String value;
        juce::Colour valueColor{0xfff0f6fc};
    };
    std::array<TelemetryTile, 6> telemetryTiles;

    juce::Label warningBannerLabel{"warnBanner", ""};
    juce::TextButton clearStatsBtn{"Clear Statistics"};
    juce::TextButton testToneBtn{"Test Tone (-18 dBFS)"};
    juce::TextButton asioControlBtn{"Open ASIO Control Panel"};

    // =========================================================================
    // Card 3: Diagnostics Log & Reporting
    // =========================================================================
    juce::Label logCardTitle{"logTitle", "Diagnostics Event Log"};
    juce::TextButton filterAllBtn{"All"};
    juce::TextButton filterInfoBtn{"Info"};
    juce::TextButton filterWarnBtn{"Warnings"};
    juce::TextButton filterErrorBtn{"Errors"};

    DiagnosticsLogManager::FilterMode currentFilterMode{DiagnosticsLogManager::FilterMode::All};

    class LogListBoxModel : public juce::ListBoxModel
    {
    public:
        LogListBoxModel(PerformanceDiagnosticsComponent& owner);
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    private:
        PerformanceDiagnosticsComponent& parent;
    };

    friend class LogListBoxModel;
    LogListBoxModel logListModel;
    juce::ListBox logListBox;
    std::vector<DiagnosticEvent> displayedEvents;

    juce::TextButton clearLogBtn{"Clear Log"};
    juce::TextButton exportLogBtn{"Export Log..."};
    juce::TextButton previewReportBtn{"Preview Report"};
    juce::TextButton exportReportBtn{"Export Diagnostic Report..."};

    void updateFilterButtons();
    void showReportPreviewModal();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceDiagnosticsComponent)
};

/**
 * Diagnostic Report Preview Dialog Window.
 */
class DiagnosticReportPreviewDialog : public juce::DocumentWindow
{
public:
    DiagnosticReportPreviewDialog(const juce::String& reportContent, DiagnosticsLogManager& logMgr);

    void closeButtonPressed() override;

private:
    juce::TextEditor reportViewer;
    juce::TextButton copyBtn{"Copy to Clipboard"};
    juce::TextButton saveBtn{"Save Report to File..."};
    juce::TextButton closeBtn{"Close"};
    DiagnosticsLogManager& logManager;
    juce::String fullReportText;
};

} // namespace LiveBus
