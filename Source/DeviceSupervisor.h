#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>

enum class DeviceState
{
    Active,
    Disconnected,
    Reconnecting,
    Failed
};

class DeviceSupervisor : private juce::Timer
{
public:
    DeviceSupervisor(juce::AudioDeviceManager& devManager);
    ~DeviceSupervisor() override;

    void setStatusCallback(std::function<void(DeviceState state, const juce::String& message)> cb)
    {
        statusCallback = std::move(cb);
    }

    void handleDeviceChange();
    void triggerReconnection();
    void stop();

    DeviceState getState() const noexcept { return currentState.load(); }
    juce::String getSavedDeviceName() const { return savedDeviceName; }
    void saveCurrentDeviceSettings();

private:
    void timerCallback() override;
    bool attemptReopenDevice();

    juce::AudioDeviceManager& deviceManager;
    std::atomic<DeviceState> currentState{DeviceState::Active};
    std::function<void(DeviceState, const juce::String&)> statusCallback;

    juce::String savedDeviceName;
    juce::String savedDeviceType;
    int retryCount = 0;
    static constexpr int maxRetries = 5;
    int backoffDelayMs = 500;
};
