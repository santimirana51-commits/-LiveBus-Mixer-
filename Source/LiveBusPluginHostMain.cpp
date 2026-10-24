#include <JuceHeader.h>
#include "SharedMemoryChannel.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>

class LiveBusPluginHostApp final : public juce::JUCEApplication
{
public:
    LiveBusPluginHostApp() = default;

    const juce::String getApplicationName() override { return "LiveBusPluginHost"; }
    const juce::String getApplicationVersion() override { return "0.4.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::String channelName = "LiveBusDefaultChannel";
        auto args = juce::StringArray::fromTokens(commandLine, true);
        for (int i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--channel" && i + 1 < args.size())
                channelName = args[i + 1];
        }

        formatManager.addDefaultFormats();

        if (!channel.create(channelName.toStdString(), false))
        {
            std::cerr << "Failed to connect to shared memory channel: " << channelName << std::endl;
            setApplicationReturnValue(1);
            quit();
            return;
        }

        auto* data = channel.getData();
        if (data == nullptr || data->magic != LiveBusIPC::magicNumber || data->version != LiveBusIPC::protocolVersion)
        {
            std::cerr << "Shared memory protocol mismatch: " << channelName << std::endl;
            channel.close();
            setApplicationReturnValue(1);
            quit();
            return;
        }

        if (data)
        {
#if defined(_WIN32)
            data->helperPid = (std::uint32_t)GetCurrentProcessId();
#else
            data->helperPid = 1;
#endif
            data->status = LiveBusIPC::Status::Idle;
        }

        shouldRun.store(true);
        workerThread = std::make_unique<std::thread>([this] { runIPCWorker(); });
    }

    void shutdown() override
    {
        shouldRun.store(false);
        if (workerThread && workerThread->joinable())
            workerThread->join();

        releasePlugin();
        channel.close();
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    void runIPCWorker()
    {
        while (shouldRun.load())
        {
            // Wait for incoming host request
            if (!channel.waitRequest(200))
            {
                // Timeout, update heartbeat if connected
                auto* data = channel.getData();
                if (data)
                {
                    data->heartbeatCounter++;
                    // Check if host process died
                    if (!LiveBusIPC::SharedMemoryChannel::isProcessAlive(data->hostPid))
                    {
                        std::cerr << "Host process died. Exiting helper." << std::endl;
                        juce::MessageManager::callAsync([this] { quit(); });
                        break;
                    }
                }
                continue;
            }

            auto* data = channel.getData();
            if (!data) continue;

            handleCommand(data);
            channel.signalResponse();
        }
    }

    void handleCommand(LiveBusIPC::SharedMemoryData* data)
    {
        switch (data->command)
        {
            case LiveBusIPC::Command::Ping:
            {
                data->status = LiveBusIPC::Status::Success;
                break;
            }

            case LiveBusIPC::Command::LoadPlugin:
            {
                loadPlugin(data);
                break;
            }

            case LiveBusIPC::Command::ReleasePlugin:
            {
                releasePlugin();
                data->status = LiveBusIPC::Status::Success;
                break;
            }

            case LiveBusIPC::Command::SetParameter:
            {
                if (pluginInstance && data->parameterIndex < (uint32_t)pluginInstance->getParameters().size())
                {
                    if (auto* param = pluginInstance->getParameters()[(int)data->parameterIndex])
                        param->setValue(data->parameterValue);
                    data->status = LiveBusIPC::Status::Success;
                }
                else
                {
                    data->status = LiveBusIPC::Status::Error;
                }
                break;
            }

            case LiveBusIPC::Command::SaveState:
            {
                if (pluginInstance)
                {
                    juce::MemoryBlock block;
                    pluginInstance->getStateInformation(block);
                    data->stateSize = (std::uint32_t)juce::jmin((size_t)block.getSize(), (size_t)LiveBusIPC::maxStateSize);
                    std::memcpy(data->stateData, block.getData(), data->stateSize);
                    data->status = LiveBusIPC::Status::Success;
                }
                else
                {
                    data->status = LiveBusIPC::Status::Error;
                }
                break;
            }

            case LiveBusIPC::Command::RestoreState:
            {
                if (pluginInstance && data->stateSize > 0)
                {
                    pluginInstance->setStateInformation(data->stateData, (int)data->stateSize);
                    data->status = LiveBusIPC::Status::Success;
                }
                else
                {
                    data->status = LiveBusIPC::Status::Error;
                }
                break;
            }

            case LiveBusIPC::Command::SetBypass:
            {
                isBypassed = (data->bypass != 0);
                data->status = LiveBusIPC::Status::Success;
                break;
            }

            case LiveBusIPC::Command::ProcessBlock:
            {
                processAudioBlock(data);
                break;
            }

            case LiveBusIPC::Command::SimulateCrash:
            {
                // Abrupt process termination to simulate an unhandled crash inside a 3rd-party plugin
                data->status = LiveBusIPC::Status::Error;
                channel.signalResponse();
#if defined(_WIN32)
                ExitProcess(42);
#else
                std::exit(42);
#endif
                break;
            }

            case LiveBusIPC::Command::Shutdown:
            {
                data->status = LiveBusIPC::Status::Success;
                juce::MessageManager::callAsync([this] { quit(); });
                break;
            }

            default:
                data->status = LiveBusIPC::Status::Error;
                break;
        }
    }

    void loadPlugin(LiveBusIPC::SharedMemoryData* data)
    {
        releasePlugin();

        juce::String pluginPath = juce::String::fromUTF8(data->pluginPath);
        juce::PluginDescription desc;
        desc.fileOrIdentifier = pluginPath;
        desc.pluginFormatName = "VST3";

        juce::String errorMessage;
        double sr = data->sampleRate > 0 ? data->sampleRate : 48000.0;
        int bs = data->frames > 0
            ? juce::jmin((int)data->frames, (int)LiveBusIPC::maxFrames)
            : 512;

        pluginInstance = formatManager.createPluginInstance(desc, sr, bs, errorMessage);

        if (!pluginInstance)
        {
            // Try scanning descriptions from file
            juce::OwnedArray<juce::PluginDescription> found;
            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                formatManager.getFormat(i)->findAllTypesForFile(found, pluginPath);
            }

            if (!found.isEmpty())
            {
                pluginInstance = formatManager.createPluginInstance(*found[0], sr, bs, errorMessage);
            }
        }

        if (pluginInstance)
        {
            pluginInstance->setPlayConfigDetails(2, 2, sr, bs);
            pluginInstance->prepareToPlay(sr, bs);
            audioBuffer.setSize(2, LiveBusIPC::maxFrames);
            std::strncpy(data->pluginName, pluginInstance->getName().toRawUTF8(), sizeof(data->pluginName) - 1);
            data->status = LiveBusIPC::Status::Success;
        }
        else
        {
            std::strncpy(data->errorMessage, errorMessage.toRawUTF8(), sizeof(data->errorMessage) - 1);
            data->status = LiveBusIPC::Status::PluginNotFound;
        }
    }

    void releasePlugin()
    {
        if (pluginInstance)
        {
            pluginInstance->releaseResources();
            pluginInstance.reset();
        }
    }

    void processAudioBlock(LiveBusIPC::SharedMemoryData* data)
    {
        if (!LiveBusIPC::isSupportedFrameCount(data->frames))
        {
            data->status = LiveBusIPC::Status::Error;
            LiveBusIPC::SharedMemoryChannel::publishOutputReadySequence(*data, data->sequence);
            return;
        }

        const int frames = (int)data->frames;

        if (!pluginInstance || isBypassed)
        {
            // Passthrough (dry signal)
            for (int ch = 0; ch < 2; ++ch)
            {
                std::memcpy(data->outputSamples[ch], data->inputSamples[ch], (size_t)frames * sizeof(float));
            }
            data->status = LiveBusIPC::Status::Success;
            LiveBusIPC::SharedMemoryChannel::publishOutputReadySequence(*data, data->sequence);
            return;
        }

        audioBuffer.setSize(2, frames, false, false, true);
        for (int ch = 0; ch < 2; ++ch)
        {
            std::memcpy(audioBuffer.getWritePointer(ch), data->inputSamples[ch], (size_t)frames * sizeof(float));
        }

        juce::MidiBuffer midiBuffer;
        pluginInstance->processBlock(audioBuffer, midiBuffer);

        for (int ch = 0; ch < 2; ++ch)
        {
            std::memcpy(data->outputSamples[ch], audioBuffer.getReadPointer(ch), (size_t)frames * sizeof(float));
        }

        data->status = LiveBusIPC::Status::Success;
        LiveBusIPC::SharedMemoryChannel::publishOutputReadySequence(*data, data->sequence);
    }

    LiveBusIPC::SharedMemoryChannel channel;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::AudioBuffer<float> audioBuffer;
    std::atomic<bool> shouldRun{false};
    bool isBypassed{false};
    std::unique_ptr<std::thread> workerThread;
};

START_JUCE_APPLICATION(LiveBusPluginHostApp)
