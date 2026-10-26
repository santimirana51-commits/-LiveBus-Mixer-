#include "PluginHostClient.h"
#include <chrono>

PluginHostClient::PluginHostClient()
{
    static std::atomic<std::uint64_t> nextClientId{0};
    const auto clientId = nextClientId.fetch_add(1, std::memory_order_relaxed);
#if defined(_WIN32)
    channelId = "LiveBusChannel_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(clientId);
#else
    channelId = "LiveBusChannel_1_" + std::to_string(clientId);
#endif
    channel.create(channelId, true);
    currentInputBuffer.setSize(2, LiveBusIPC::maxFrames);
    currentInputBuffer.clear();
}

PluginHostClient::~PluginHostClient()
{
    terminateHelper();
    channel.close();
}

void PluginHostClient::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    currentBlockSize = juce::jmax(1, blockSize);
    currentInputBuffer.setSize(2, juce::jmax((int)LiveBusIPC::maxFrames, currentBlockSize));
    currentInputBuffer.clear();
    pendingBlock.reset();
    activeSequence = 0;
    dryDelay.reset();
}

bool PluginHostClient::launchHelper()
{
    terminateHelper();

    auto* data = channel.getData();
    if (data == nullptr)
    {
        lastErrorMessage = "Shared-memory channel is unavailable";
        return false;
    }
    data->helperPid = 0;

    auto appFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::Array<juce::File> candidates = {
        appFile.getSiblingFile("LiveBusPluginHost.exe"),
        appFile.getSiblingFile("LiveBus Plugin Host.exe"),
        appFile.getParentDirectory().getParentDirectory().getParentDirectory()
            .getChildFile("LiveBusPluginHost_artefacts/Release/LiveBusPluginHost.exe"),
        appFile.getParentDirectory().getParentDirectory().getParentDirectory()
            .getChildFile("LiveBusPluginHost_artefacts/Release/LiveBus Plugin Host.exe"),
        appFile.getParentDirectory().getParentDirectory().getChildFile("LiveBusPluginHost_artefacts/Release/LiveBusPluginHost.exe"),
        appFile.getParentDirectory().getParentDirectory().getChildFile("LiveBusPluginHost_artefacts/Release/LiveBus Plugin Host.exe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("LiveBusPluginHost_artefacts/Release/LiveBusPluginHost.exe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("LiveBusPluginHost_artefacts/Release/LiveBus Plugin Host.exe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("build/LiveBusPluginHost_artefacts/Release/LiveBusPluginHost.exe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("build/LiveBusPluginHost_artefacts/Release/LiveBus Plugin Host.exe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("LiveBusPluginHost.exe")
    };

    juce::File helperFile;
    for (const auto& c : candidates)
    {
        if (c.existsAsFile())
        {
            helperFile = c;
            break;
        }
    }
    if (!helperFile.existsAsFile())
        helperFile = candidates[0];

#if defined(_WIN32)
    std::wstring cmd = L"\"" + std::wstring(helperFile.getFullPathName().toWideCharPointer()) + L"\" --channel " +
                       std::wstring(channelId.begin(), channelId.end());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL success = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &processInfo
    );

    if (!success)
    {
        lastErrorMessage = "Failed to launch LiveBusPluginHost.exe";
        return false;
    }

    crashed.store(false);

    // Wait briefly on non-real-time thread for helper to register
    for (int i = 0; i < 30; ++i)
    {
        if (data && data->helperPid != 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    lastErrorMessage = "LiveBusPluginHost did not register its shared-memory channel";
    terminateHelper();
    return false;
#else
    return false;
#endif
}

void PluginHostClient::terminateHelper()
{
    auto* data = channel.getData();
    if (data && isRunning())
    {
        data->command = LiveBusIPC::Command::Shutdown;
        channel.signalRequest();
        channel.waitResponse(100);
    }

#if defined(_WIN32)
    if (processInfo.hProcess)
    {
        WaitForSingleObject(processInfo.hProcess, 200);
        TerminateProcess(processInfo.hProcess, 0);
        CloseHandle(processInfo.hProcess);
        processInfo.hProcess = nullptr;
    }
    if (processInfo.hThread)
    {
        CloseHandle(processInfo.hThread);
        processInfo.hThread = nullptr;
    }
#endif
    pendingBlock.reset();
}

bool PluginHostClient::simulateCrash()
{
    auto* data = channel.getData();
    if (!data || !isRunning()) return false;

    data->command = LiveBusIPC::Command::SimulateCrash;
    channel.signalRequest();

    // Give helper 200ms to terminate
#if defined(_WIN32)
    if (processInfo.hProcess)
    {
        DWORD wait = WaitForSingleObject(processInfo.hProcess, 500);
        if (wait == WAIT_OBJECT_0)
        {
            crashed.store(true);
            return true;
        }
    }
#endif
    return true;
}

bool PluginHostClient::isRunning() const
{
#if defined(_WIN32)
    if (!processInfo.hProcess) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(processInfo.hProcess, &exitCode))
    {
        return (exitCode == STILL_ACTIVE);
    }
    return false;
#else
    return false;
#endif
}

bool PluginHostClient::loadPlugin(const juce::String& path, double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    loadedPluginPath = path;
    pluginLoaded.store(false, std::memory_order_relaxed);
    currentPluginName.clear();
    pendingBlock.reset();
    dryDelay.reset();
    lastErrorMessage.clear();
    pluginLatencySamples.store(0, std::memory_order_relaxed);

    if (!isRunning())
    {
        if (!launchHelper())
            return false;
    }

    auto* data = channel.getData();
    if (!data) return false;

    data->command = LiveBusIPC::Command::LoadPlugin;
    data->sampleRate = sampleRate;
    data->frames = (uint32_t)blockSize;
    LiveBusIPC::copyNullTerminatedString(data->pluginPath, sizeof(data->pluginPath), path.toRawUTF8());
    data->status = LiveBusIPC::Status::Idle;

    channel.signalRequest();

    // Non-real-time wait for plugin instantiation
    if (channel.waitResponse(3000))
    {
        if (data->status == LiveBusIPC::Status::Success)
        {
            currentPluginName = juce::String::fromUTF8(data->pluginName);
            pluginLatencySamples.store((int)data->pluginLatencySamples,
                                       std::memory_order_relaxed);
            pluginLoaded.store(true);
            crashed.store(false);
            return true;
        }
        else
        {
            lastErrorMessage = juce::String::fromUTF8(data->errorMessage);
            return false;
        }
    }

    lastErrorMessage = "Helper timed out while loading plugin";
    return false;
}

void PluginHostClient::releasePlugin()
{
    pluginLoaded.store(false);
    pluginLatencySamples.store(0, std::memory_order_relaxed);
    currentPluginName = "";
    loadedPluginPath = "";
    pendingBlock.reset();

    auto* data = channel.getData();
    if (data && isRunning())
    {
        data->command = LiveBusIPC::Command::ReleasePlugin;
        channel.signalRequest();
        channel.waitResponse(500);
    }
}

bool PluginHostClient::saveState(juce::MemoryBlock& outBlock)
{
    auto* data = channel.getData();
    if (!data || !isRunning() || !pluginLoaded.load()) return false;

    data->command = LiveBusIPC::Command::SaveState;
    channel.signalRequest();

    if (channel.waitResponse(500) && data->status == LiveBusIPC::Status::Success)
    {
        outBlock.replaceAll(data->stateData, data->stateSize);
        lastKnownState = outBlock;
        return true;
    }
    return false;
}

bool PluginHostClient::restoreState(const void* dataPtr, size_t size)
{
    auto* data = channel.getData();
    if (!data || dataPtr == nullptr || size == 0 || size > LiveBusIPC::maxStateSize ||
        !isRunning() || !pluginLoaded.load())
        return false;

    data->command = LiveBusIPC::Command::RestoreState;
    data->stateSize = (uint32_t)size;
    std::memcpy(data->stateData, dataPtr, data->stateSize);

    channel.signalRequest();
    if (channel.waitResponse(500) && data->status == LiveBusIPC::Status::Success)
    {
        lastKnownState.replaceAll(dataPtr, size);
        return true;
    }
    return false;
}

void PluginHostClient::setBypass(bool bypass)
{
    bypassed.store(bypass);
}

juce::String PluginHostClient::getPluginName() const
{
    return currentPluginName;
}

juce::String PluginHostClient::getLastError() const
{
    return lastErrorMessage;
}

void PluginHostClient::processBlock(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (numSamples <= 0 || numSamples > buffer.getNumSamples())
        return;

    if (!LiveBusIPC::isSupportedFrameCount((std::uint32_t)numSamples))
        return;

    auto* data = channel.getData();
    const bool pluginActive = pluginLoaded.load(std::memory_order_relaxed) &&
                              !crashed.load(std::memory_order_relaxed) && data != nullptr;
    if (!pluginActive && !dryDelay.hasAudio() && !pendingBlock.isPending())
        return;

    const int channels = juce::jmin(2, buffer.getNumChannels());
    std::array<const float*, 2> currentInput{};
    std::array<float*, 2> output{};
    for (int ch = 0; ch < channels; ++ch)
    {
        currentInputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        currentInput[(size_t)ch] = currentInputBuffer.getReadPointer(ch);
        output[(size_t)ch] = buffer.getWritePointer(ch);
    }

    bool usePluginOutput = false;
    if (pendingBlock.isPending())
    {
        const auto completedSequence = data != nullptr
            ? LiveBusIPC::SharedMemoryChannel::loadOutputReadySequence(*data) : 0;
        if (!pendingBlock.isComplete(completedSequence))
        {
            pendingBlock.discardLateOutput();
            dryDelay.storeCurrent(currentInput.data(), channels,
                                  (std::uint32_t)numSamples);
            if (!dryDelay.copyPrevious(output.data(), channels,
                                       (std::uint32_t)numSamples))
                buffer.clear(0, numSamples);
            return;
        }

        usePluginOutput = pluginActive &&
            !bypassed.load(std::memory_order_relaxed) &&
            pendingBlock.canUseOutput(completedSequence, data->status,
                                      (std::uint32_t)numSamples);
        pendingBlock.reset();
    }

    dryDelay.storeCurrent(currentInput.data(), channels, (std::uint32_t)numSamples);
    if (usePluginOutput)
    {
        for (int ch = 0; ch < channels; ++ch)
            std::memcpy(buffer.getWritePointer(ch), data->outputSamples[ch],
                        (size_t)numSamples * sizeof(float));
    }
    else if (!dryDelay.copyPrevious(output.data(), channels, (std::uint32_t)numSamples))
    {
        buffer.clear(0, numSamples);
    }

    if (!pluginActive || bypassed.load(std::memory_order_relaxed) ||
        pendingBlock.isPending())
        return;

    for (int ch = 0; ch < channels; ++ch)
        std::memcpy(data->inputSamples[ch], currentInput[(size_t)ch],
                    (size_t)numSamples * sizeof(float));

    activeSequence++;
    data->frames = (uint32_t)numSamples;
    data->sequence = activeSequence;
    data->command = LiveBusIPC::Command::ProcessBlock;
    pendingBlock.begin(activeSequence, (std::uint32_t)numSamples);
    channel.signalRequest();
}

void PluginHostClient::checkHealthAndRecover(double sampleRate, int blockSize)
{
    if (crashed.load() || (pluginLoaded.load() && !isRunning()))
    {
        // Helper died or crashed: restart and restore on non-real-time thread
        pendingBlock.reset();
        launchHelper();
        if (loadedPluginPath.isNotEmpty())
        {
            if (loadPlugin(loadedPluginPath, sampleRate, blockSize))
            {
                if (lastKnownState.getSize() > 0)
                {
                    restoreState(lastKnownState.getData(), lastKnownState.getSize());
                }
                crashed.store(false);
            }
        }
    }
}
