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
        juce::File initLog("C:\\Users\\miran\\Documents\\muli agen\\init_log.txt");
        initLog.replaceWithText("commandLine arg: [" + commandLine + "]\n"
                                + "getCommandLineParameters: [" + juce::JUCEApplication::getCommandLineParameters() + "]\n");

        window = std::make_unique<Window>();
        juce::String cmd = commandLine + " " + juce::JUCEApplication::getCommandLineParameters() + " " + juce::String(GetCommandLineA());

        if (cmd.containsIgnoreCase("snapshot"))
        {
            initLog.appendText("Starting snapshot capture... cmd: " + cmd + "\n");
            if (auto* mc = dynamic_cast<MainComponent*>(window->getContentComponent()))
            {
                juce::File currentDir = juce::File::getCurrentWorkingDirectory();
                initLog.appendText("Current dir: " + currentDir.getFullPathName() + "\n");
                mc->captureTabSnapshots(currentDir);

                juce::File wsDir("C:\\Users\\miran\\Documents\\muli agen");
                if (wsDir.exists() && wsDir != currentDir)
                {
                    initLog.appendText("Writing to ws dir: " + wsDir.getFullPathName() + "\n");
                    mc->captureTabSnapshots(wsDir);
                }
                initLog.appendText("Snapshots done!\n");
            }
            else
            {
                initLog.appendText("Failed: content component is not MainComponent!\n");
            }
            quit();
        }
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow("LiveBus Mixer v1.0", juce::Colour(0xff0e1217),
                                  DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1380, 860);
            setResizable(true, true);
            setResizeLimits(1280, 760, 10000, 10000);
            setVisible(true);
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
