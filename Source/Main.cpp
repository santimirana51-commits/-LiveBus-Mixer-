#include <JuceHeader.h>
#include "MainComponent.h"

class LiveBusMixerApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "LiveBus Mixer"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override
    {
        juce::File logFile = juce::File::getCurrentWorkingDirectory().getChildFile("launch_debug.txt");
        logFile.replaceWithText("Initialise called with cmdLine: [" + commandLine + "]\n");

        window = std::make_unique<Window>();
        logFile.appendText("Window created successfully.\n");

        if (commandLine.trim().equalsIgnoreCase("--snapshot"))
        {
            logFile.appendText("Snapshot flag matched. Quitting after capture.\n");
            if (auto* mc = dynamic_cast<MainComponent*>(window->getContentComponent()))
            {
                juce::File currentDir = juce::File::getCurrentWorkingDirectory();
                mc->captureTabSnapshots(currentDir);
            }
            quit();
        }
        else
        {
            logFile.appendText("Normal launch: window visible=" + juce::String(window->isVisible() ? "true" : "false") + "\n");
        }
    }
    void shutdown() override
    {
        juce::File logFile = juce::File::getCurrentWorkingDirectory().getChildFile("launch_debug.txt");
        logFile.appendText("Shutdown callback entered.\n");
        window.reset();
    }
    void systemRequestedQuit() override
    {
        juce::File logFile = juce::File::getCurrentWorkingDirectory().getChildFile("launch_debug.txt");
        logFile.appendText("systemRequestedQuit() was triggered! Stack/quit requested.\n");
        quit();
    }
    void anotherInstanceStarted(const juce::String&) override {}

    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow("LiveBus Mixer v1.0", juce::Colour(0xff0e1217),
                                  DocumentWindow::allButtons)
        {
            auto* mainComp = new MainComponent();
            mainComp->setVisible(true);
            setUsingNativeTitleBar(true);
            setContentOwned(mainComp, true);
            centreWithSize(1380, 860);
            setResizable(true, true);
            setResizeLimits(1280, 760, 10000, 10000);
            setVisible(true);
            toFront(true);
            juce::Process::makeForegroundProcess();
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
private:
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(LiveBusMixerApplication)
