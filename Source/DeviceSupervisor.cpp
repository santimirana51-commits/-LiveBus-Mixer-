#include "DeviceSupervisor.h"

DeviceSupervisor::DeviceSupervisor(juce::AudioDeviceManager& devMgr)
    : deviceManager(devMgr)
{
    saveCurrentDeviceSettings();
}

DeviceSupervisor::~DeviceSupervisor()
{
    stop();
}

void DeviceSupervisor::stop()
{
    stopTimer();
}

void DeviceSupervisor::saveCurrentDeviceSettings()
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        savedDeviceName = dev->getName();
        savedDeviceType = dev->getTypeName();
    }
}

void DeviceSupervisor::handleDeviceChange()
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr || !dev->isOpen())
    {
        currentState.store(DeviceState::Disconnected);
        if (statusCallback)
            statusCallback(DeviceState::Disconnected, "Audio device disconnected. Starting supervisor recovery...");
        triggerReconnection();
    }
    else
    {
        saveCurrentDeviceSettings();
        currentState.store(DeviceState::Active);
        retryCount = 0;
        backoffDelayMs = 500;
        stopTimer();
        if (statusCallback)
            statusCallback(DeviceState::Active, "Audio device active: " + dev->getName());
    }
}

void DeviceSupervisor::triggerReconnection()
{
    if (retryCount >= maxRetries)
    {
        currentState.store(DeviceState::Failed);
        if (statusCallback)
            statusCallback(DeviceState::Failed, "Auto-recovery failed after " + juce::String(maxRetries) +
                                                  " attempts. Please connect device and select in Audio Settings.");
        stopTimer();
        return;
    }

    currentState.store(DeviceState::Reconnecting);
    retryCount++;
    int currentWait = backoffDelayMs;
    backoffDelayMs = juce::jmin(backoffDelayMs * 2, 8000); // exponential backoff up to 8s

    if (statusCallback)
        statusCallback(DeviceState::Reconnecting, "Attempting reconnect " + juce::String(retryCount) +
                                                  "/" + juce::String(maxRetries) + " in " +
                                                  juce::String(currentWait) + " ms...");

    startTimer(currentWait);
}

void DeviceSupervisor::timerCallback()
{
    stopTimer();

    bool recovered = attemptReopenDevice();
    if (recovered)
    {
        currentState.store(DeviceState::Active);
        retryCount = 0;
        backoffDelayMs = 500;
        saveCurrentDeviceSettings();
        if (statusCallback)
            statusCallback(DeviceState::Active, "Device successfully recovered: " + savedDeviceName);
    }
    else
    {
        triggerReconnection();
    }
}

bool DeviceSupervisor::attemptReopenDevice()
{
    // Rescan device list
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        type->scanForDevices();

    auto setup = deviceManager.getAudioDeviceSetup();
    if (savedDeviceName.isNotEmpty())
    {
        setup.outputDeviceName = savedDeviceName;
        if (deviceManager.getCurrentAudioDeviceType().containsIgnoreCase("ASIO"))
            setup.inputDeviceName = savedDeviceName;
    }

    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty())
    {
        auto* dev = deviceManager.getCurrentAudioDevice();
        if (dev != nullptr && dev->isOpen())
            return true;
    }
    return false;
}
