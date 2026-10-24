#pragma once
#include <JuceHeader.h>

namespace LiveBus
{

/**
 * Clean floating window for hosting VST3 custom GUI or generic fallback editor.
 */
class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow(juce::AudioProcessor& processor, const juce::String& title, std::function<void()> onClose = nullptr);
    ~PluginEditorWindow() override;

    void closeButtonPressed() override;

    static void openWindowFor(juce::AudioProcessor& processor, const juce::String& title, std::function<void()> onClose = nullptr);
    static void closeAllWindows();

private:
    std::function<void()> onCloseCallback;
    static juce::Array<PluginEditorWindow*> activeWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
};

} // namespace LiveBus
