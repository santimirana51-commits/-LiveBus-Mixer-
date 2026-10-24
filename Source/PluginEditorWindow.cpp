#include "PluginEditorWindow.h"

namespace LiveBus
{

juce::Array<PluginEditorWindow*> PluginEditorWindow::activeWindows;

PluginEditorWindow::PluginEditorWindow(juce::AudioProcessor& processor, const juce::String& title, std::function<void()> onClose)
    : juce::DocumentWindow(title, juce::Colour(0xff161b22), juce::DocumentWindow::closeButton),
      onCloseCallback(std::move(onClose))
{
    setUsingNativeTitleBar(true);

    juce::AudioProcessorEditor* editor = processor.createEditorIfNeeded();
    if (editor == nullptr)
    {
        // Fallback to generic processor editor for parameter controls
        editor = new juce::GenericAudioProcessorEditor(processor);
    }

    if (editor != nullptr)
    {
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
    }
    else
    {
        auto* label = new juce::Label("NoEditor", "No editor interface available for this plug-in.");
        label->setSize(350, 150);
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        setContentOwned(label, true);
    }

    centreWithSize(getWidth(), getHeight());
    setVisible(true);

    activeWindows.add(this);
}

PluginEditorWindow::~PluginEditorWindow()
{
    activeWindows.removeFirstMatchingValue(this);
    clearContentComponent();
}

void PluginEditorWindow::closeButtonPressed()
{
    if (onCloseCallback)
        onCloseCallback();

    delete this;
}

void PluginEditorWindow::openWindowFor(juce::AudioProcessor& processor, const juce::String& title, std::function<void()> onClose)
{
    // If a window for this processor is already open, bring to front
    for (auto* w : activeWindows)
    {
        if (w->getName() == title)
        {
            w->toFront(true);
            return;
        }
    }

    new PluginEditorWindow(processor, title, std::move(onClose));
}

void PluginEditorWindow::closeAllWindows()
{
    while (!activeWindows.isEmpty())
    {
        delete activeWindows.getLast();
    }
}

} // namespace LiveBus
