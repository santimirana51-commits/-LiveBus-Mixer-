#include "PluginHostClient.h"
#include <chrono>

PluginHostClient::PluginHostClient()
{
#if defined(_WIN32)
    channelId = "LiveBusChannel_" + std::to_string(GetCurrentProcessId());
#else
    channelId = "LiveBusChannel_1";
#endif
    channel.create(channelId, true);
    dryRingBuffer.setSize(2, LiveBusIPC::maxFrames);
    dryRingBuffer.clear();
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
    dryRingBuffer.setSize(2, juce::jmax((int)LiveBusIPC::maxFrames, currentBlockSize));
    dryRingBuffer.clear();
    pendingBlock.reset();
    activeSequence = 0;
}

bool PluginHostClient::launchHelper()
{
    terminateHelper();

    auto appFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::Array<juce::File> candidates = {
        appFile.getSiblingFile("LiveBusPluginHost.exe"),
        appFile.getSiblingFile("LiveBus Plugin Host.exe"),
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
        auto* data = channel.getData();
        if (data && data->helperPid != 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    return true;
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
    std::strncpy(data->pluginPath, path.toRawUTF8(), sizeof(data->pluginPath) - 1);
    data->status = LiveBusIPC::Status::Idle;

    channel.signalRequest();

    // Non-real-time wait for plugin instantiation
    if (channel.waitResponse(3000))
    {
        if (data->status == LiveBusIPC::Status::Success)
        {
            currentPluginName = juce::String::fromUTF8(data->pluginName);
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
    if (!data || !isRunning() || !pluginLoaded.load()) return false;

    data->command = LiveBusIPC::Command::RestoreState;
    data->stateSize = (uint32_t)juce::jmin(size, (size_t)LiveBusIPC::maxStateSize);
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
    auto* data = channel.getData();
    if (data && isRunning())
    {
        data->command = LiveBusIPC::Command::SetBypass;
        data->bypass = bypass ? 1 : 0;
        channel.signalRequest();
        channel.waitResponse(100);
    }
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
    const bool supportedBlock = numSamples <= buffer.getNumSamples()
        && LiveBusIPC::isSupportedFrameCount((std::uint32_t)juce::jmax(0, numSamples));

    // If not loaded, bypassed, or helper marked crashed: pass through dry audio
    if (!pluginLoaded.load(std::memory_order_relaxed) || crashed.load(std::memory_order_relaxed))
    {
        pendingBlock.reset();
        return; // dry buffer left untouched
    }

    if (bypassed.load(std::memory_order_relaxed))
    {
        if (pendingBlock.isPending())
            pendingBlock.discardLateOutput();
        return;
    }

    auto* data = channel.getData();
    if (!data)
    {
        pendingBlock.reset();
        return;
    }

    // -------------------------------------------------------------
    // TRUE REAL-TIME ASYNCHRONOUS PIPELINE: ZERO-WAIT (0ms)
    // -------------------------------------------------------------
    // Poll the completion marker for the previously dispatched block.
    if (pendingBlock.isPending())
    {
        const auto completedSequence = LiveBusIPC::SharedMemoryChannel::loadOutputReadySequence(*data);

        if (!pendingBlock.isComplete(completedSequence))
        {
            pendingBlock.discardLateOutput();
            return;
        }

        if (supportedBlock && pendingBlock.canUseOutput(completedSequence, data->status, (std::uint32_t)numSamples))
        {
            for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
                std::memcpy(buffer.getWritePointer(ch), data->outputSamples[ch], (size_t)numSamples * sizeof(float));
        }

        channel.waitResponse(0);
        pendingBlock.reset();
    }

    if (!supportedBlock)
        return;

    for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
    {
        std::memcpy(data->inputSamples[ch], buffer.getReadPointer(ch), (size_t)numSamples * sizeof(float));
    }

    activeSequence++;
    data->frames = (uint32_t)numSamples;
    data->sequence = activeSequence;
    data->command = LiveBusIPC::Command::ProcessBlock;
    pendingBlock.begin(activeSequence, (std::uint32_t)numSamples);

    // Fast non-blocking signal (sub-microsecond kernel event set)
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
