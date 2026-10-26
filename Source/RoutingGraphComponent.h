#pragma once

#include <JuceHeader.h>
#include "RoutingMatrix.h"
#include <vector>

namespace LiveBus
{

class RoutingGraphCanvas;

enum class RoutingGraphDSPModule : int
{
    HPF,
    LPF,
    EQ,
    Gate,
    Compressor,
    Limiter,
    Reverb,
    Delay
};

enum class RoutingGraphDSPParameter : int
{
    HPFFrequency,
    LPFFrequency,
    EQGain,
    GateThreshold,
    CompressorThreshold,
    LimiterCeiling,
    ReverbWet,
    DelayTime
};

enum class RoutingGraphPaletteAction : int
{
    AddInput,
    YouTubeCapture,
    Gate,
    Equalizer,
    Compressor,
    VST3Chain,
    Submix,
    Aux,
    StreamBus,
    MonitorBus,
    VirtualCable,
    OBS,
    AudioDevice,
    Recorder
};

class RoutingGraphComponent final : public juce::Component
{
public:
    using VSTChainPluginsProvider = std::function<std::vector<juce::String>(int)>;

    struct DSPControlCallbacks
    {
        std::function<bool(int, RoutingGraphDSPModule)> getModuleEnabled;
        std::function<void(int, RoutingGraphDSPModule, bool)> setModuleEnabled;
        std::function<float(int, RoutingGraphDSPParameter)> getParameter;
        std::function<void(int, RoutingGraphDSPParameter, float)> setParameter;
        std::function<void(int)> controlChanged;
    };

    struct MasterBusControlCallbacks
    {
        std::function<float(int)> getGainDb;
        std::function<void(int, float)> setGainDb;
        std::function<bool(int)> getMute;
        std::function<void(int, bool)> setMute;
        std::function<bool(int)> getDim;
        std::function<void(int, bool)> setDim;
    };

    explicit RoutingGraphComponent(RoutingMatrix& routingMatrix,
                                   std::function<void(int)> onChannelSelected = {},
                                   std::function<void()> onSignalFlowRequested = {},
                                   std::function<void(juce::Component*)> onAddInputRequested = {},
                                   std::function<void(int)> onDSPNodeSelected = {},
                                   std::function<void(int)> onVSTChainSelected = {},
                                   VSTChainPluginsProvider vstChainPluginsProvider = {});
    ~RoutingGraphComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void refresh();
    void setMasterBusControlCallbacks(MasterBusControlCallbacks callbacks);
    void setDSPControlCallbacks(DSPControlCallbacks callbacks);
    void setPaletteActionCallback(std::function<void(RoutingGraphPaletteAction)> callback);
    void focusNode(const juce::String& typeId, int index);
    void showAllNodes();
    std::function<void(RoutingGraphPaletteAction)> onPaletteAction;
    juce::String serializeLayout() const;
    bool restoreLayout(const juce::String& xml);

private:
    struct PaletteItem
    {
        juce::String label;
        juce::String tooltip;
        int category = 0;
        RoutingGraphPaletteAction action = RoutingGraphPaletteAction::AddInput;
        bool enabled = true;
    };

    void setZoom(float newZoom);
    void fitToView();
    void resetLayout();
    void rebuildPaletteItems();
    void rebuildDockItems();

    RoutingMatrix& matrix;
    std::function<void(int)> channelSelected;
    std::function<void(int)> dspNodeSelected;
    std::function<void(int)> vstChainSelected;
    VSTChainPluginsProvider vstChainPluginsProvider;
    DSPControlCallbacks dspControlCallbacks;
    MasterBusControlCallbacks masterBusControlCallbacks;
    juce::Viewport viewport;
    std::unique_ptr<RoutingGraphCanvas> canvas;
    int viewPositionX = 0;
    int viewPositionY = 0;
    bool hasRestoredLayout = false;
    juce::TextButton zoomOutButton{"-"};
    juce::TextButton zoomInButton{"+"};
    juce::TextButton fitButton{"Fit"};
    juce::TextButton addNodeButton{"+ Input"};
    juce::TextButton signalFlowButton{"Signal Flow"};
    juce::TextButton showAllButton{"Show All"};
    juce::Label helpLabel;
    juce::TextEditor paletteSearch;
    juce::Viewport paletteViewport;
    juce::Component paletteListContent;
    std::array<juce::TextButton, 4> paletteCategoryButtons{
        juce::TextButton("INPUTS"), juce::TextButton("PROCESSING"),
        juce::TextButton("BUS"), juce::TextButton("OUTPUTS")
    };
    std::vector<std::unique_ptr<juce::TextButton>> paletteItemButtons;
    std::vector<PaletteItem> paletteItems;
    juce::Viewport dockViewport;
    juce::Component dockContent;
    std::vector<std::unique_ptr<juce::TextButton>> dockButtons;
    std::array<juce::TextButton*, 2> dockDimButtons{ nullptr, nullptr };
    std::array<juce::TextButton*, 2> dockMuteButtons{ nullptr, nullptr };
    std::function<void(RoutingGraphPaletteAction)> paletteActionCallback;
    std::function<void(juce::Component*)> addInputCallback;
    int selectedPaletteCategory = 0;
    int dockChannelCount = 0;
    float zoom = 1.0f;
    float preFocusZoom = 1.0f;
    int preFocusViewX = 0;
    int preFocusViewY = 0;
    bool hasFocusedViewSnapshot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingGraphComponent)
};

} // namespace LiveBus
