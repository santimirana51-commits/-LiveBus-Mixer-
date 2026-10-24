#include "AudioDeviceService.h"

namespace LiveBus
{

AudioDeviceService::AudioDeviceService(juce::AudioDeviceManager& devMgr)
    : deviceManager(devMgr)
{
    supervisor = std::make_unique<DeviceSupervisor>(deviceManager);
    supervisor->setStatusCallback([this](DeviceState state, const juce::String& msg) {
        lastStatusMessage = msg;
        if (listener != nullptr)
            listener->onDeviceStateChanged(state, msg);
    });

    deviceManager.addChangeListener(this);
}

AudioDeviceService::~AudioDeviceService()
{
    deviceManager.removeChangeListener(this);
    if (supervisor != nullptr)
        supervisor->stop();
}

void AudioDeviceService::setListener(AudioDeviceServiceListener* newListener)
{
    listener = newListener;
}

// =============================================================================
// Backend Management
// =============================================================================
juce::StringArray AudioDeviceService::getAvailableBackendTypes() const
{
    juce::StringArray types;
    const auto& available = deviceManager.getAvailableDeviceTypes();
    for (auto* type : available)
    {
        if (type != nullptr)
            types.add(type->getTypeName());
    }
    return types;
}

juce::String AudioDeviceService::getCurrentBackendType() const
{
    return deviceManager.getCurrentAudioDeviceType();
}

juce::String AudioDeviceService::setBackendType(const juce::String& typeName)
{
    if (typeName.isEmpty() || typeName == getCurrentBackendType())
        return {};

    testToneActive.store(false); // Stop test tone on backend switch
    deviceManager.setCurrentAudioDeviceType(typeName, true);

    if (supervisor != nullptr)
        supervisor->saveCurrentDeviceSettings();

    if (listener != nullptr)
    {
        listener->onDeviceSetupChanged();
        listener->onDeviceStateChanged(getDeviceState(), "Backend switched to " + typeName);
    }
    return {};
}

// =============================================================================
// Device Enumeration & Selection
// =============================================================================
juce::StringArray AudioDeviceService::getAvailableOutputDevices() const
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames(false);
    }
    return {};
}

juce::StringArray AudioDeviceService::getAvailableInputDevices() const
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames(true);
    }
    return {};
}

juce::String AudioDeviceService::getCurrentOutputDevice() const
{
    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.outputDeviceName.isNotEmpty())
        return setup.outputDeviceName;

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getName();

    return {};
}

juce::String AudioDeviceService::getCurrentInputDevice() const
{
    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.inputDeviceName.isNotEmpty())
        return setup.inputDeviceName;

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getName();

    return {};
}

juce::String AudioDeviceService::setOutputDevice(const juce::String& deviceName)
{
    testToneActive.store(false);

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = deviceName;
    setup.useDefaultOutputChannels = true;

    // For ASIO, input and output are handled by the same unified driver
    if (getCurrentBackendType().containsIgnoreCase("ASIO"))
    {
        setup.inputDeviceName = deviceName;
        setup.useDefaultInputChannels = true;
    }

    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty())
    {
        if (supervisor != nullptr)
            supervisor->saveCurrentDeviceSettings();

        if (listener != nullptr)
            listener->onDeviceSetupChanged();
    }
    return err;
}

juce::String AudioDeviceService::setInputDevice(const juce::String& deviceName)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = deviceName;
    setup.useDefaultInputChannels = true;

    // For ASIO, input and output are handled by the same unified driver
    if (getCurrentBackendType().containsIgnoreCase("ASIO"))
    {
        setup.outputDeviceName = deviceName;
        setup.useDefaultOutputChannels = true;
    }

    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty())
    {
        if (supervisor != nullptr)
            supervisor->saveCurrentDeviceSettings();

        if (listener != nullptr)
            listener->onDeviceSetupChanged();
    }
    return err;
}

// =============================================================================
// Channel Configuration
// =============================================================================
juce::StringArray AudioDeviceService::getAvailableOutputChannelNames() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getOutputChannelNames();

    return {"Output channel 1", "Output channel 2"};
}

juce::StringArray AudioDeviceService::getAvailableInputChannelNames() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getInputChannelNames();

    return {"Input channel 1", "Input channel 2"};
}

juce::BigInteger AudioDeviceService::getActiveOutputChannels() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getActiveOutputChannels();

    return deviceManager.getAudioDeviceSetup().outputChannels;
}

juce::BigInteger AudioDeviceService::getActiveInputChannels() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getActiveInputChannels();

    return deviceManager.getAudioDeviceSetup().inputChannels;
}

juce::String AudioDeviceService::setOutputChannelActive(int channelIndex, bool active)
{
    auto channels = getActiveOutputChannels();
    channels.setBit(channelIndex, active);
    return setOutputChannels(channels);
}

juce::String AudioDeviceService::setInputChannelActive(int channelIndex, bool active)
{
    auto channels = getActiveInputChannels();
    channels.setBit(channelIndex, active);
    return setInputChannels(channels);
}

juce::String AudioDeviceService::setOutputChannels(const juce::BigInteger& activeChannels)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.useDefaultOutputChannels = false;
    setup.outputChannels = activeChannels;

    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty() && listener != nullptr)
        listener->onDeviceSetupChanged();

    return err;
}

juce::String AudioDeviceService::setInputChannels(const juce::BigInteger& activeChannels)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.useDefaultInputChannels = false;
    setup.inputChannels = activeChannels;

    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty() && listener != nullptr)
        listener->onDeviceSetupChanged();

    return err;
}

// =============================================================================
// Sample Rate & Buffer Size
// =============================================================================
juce::Array<double> AudioDeviceService::getAvailableSampleRates() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        auto rates = dev->getAvailableSampleRates();
        if (!rates.isEmpty())
            return rates;
    }
    return {44100.0, 48000.0, 88200.0, 96000.0};
}

double AudioDeviceService::getCurrentSampleRate() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getCurrentSampleRate();

    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.sampleRate > 0.0)
        return setup.sampleRate;

    return 48000.0;
}

juce::String AudioDeviceService::setSampleRate(double newSampleRate)
{
    if (newSampleRate <= 0.0)
        return "Invalid sample rate";

    auto setup = deviceManager.getAudioDeviceSetup();
    if (std::abs(setup.sampleRate - newSampleRate) < 1.0)
        return {};

    setup.sampleRate = newSampleRate;
    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty())
    {
        if (listener != nullptr)
            listener->onDeviceSetupChanged();
    }
    return err;
}

juce::Array<int> AudioDeviceService::getAvailableBufferSizes() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        auto sizes = dev->getAvailableBufferSizes();
        if (!sizes.isEmpty())
            return sizes;
    }
    return {64, 128, 256, 480, 512, 1024};
}

int AudioDeviceService::getCurrentBufferSize() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getCurrentBufferSizeSamples();

    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.bufferSize > 0)
        return setup.bufferSize;

    return 480;
}

juce::String AudioDeviceService::setBufferSize(int newBufferSize)
{
    if (newBufferSize <= 0)
        return "Invalid buffer size";

    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.bufferSize == newBufferSize)
        return {};

    setup.bufferSize = newBufferSize;
    juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
    if (err.isEmpty())
    {
        if (listener != nullptr)
            listener->onDeviceSetupChanged();
    }
    return err;
}

// =============================================================================
// Driver Latency Reporting
// =============================================================================
int AudioDeviceService::getInputLatencySamples() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getInputLatencyInSamples();
    return -1;
}

int AudioDeviceService::getOutputLatencySamples() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getOutputLatencyInSamples();
    return -1;
}

float AudioDeviceService::getInputLatencyMs() const
{
    auto sr = getCurrentSampleRate();
    if (sr <= 0.0) return -1.0f;
    int samples = getInputLatencySamples();
    if (samples < 0) return -1.0f;
    return (float)((double)samples / sr * 1000.0);
}

float AudioDeviceService::getOutputLatencyMs() const
{
    auto sr = getCurrentSampleRate();
    if (sr <= 0.0) return -1.0f;
    int samples = getOutputLatencySamples();
    if (samples < 0) return -1.0f;
    return (float)((double)samples / sr * 1000.0);
}

// =============================================================================
// Control Panel (ASIO)
// =============================================================================
bool AudioDeviceService::hasControlPanel() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->hasControlPanel();

    return false;
}

void AudioDeviceService::openControlPanel()
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        if (dev->hasControlPanel())
            dev->showControlPanel();
    }
}

// =============================================================================
// Safe Test Tone Generator (-18 dBFS)
// =============================================================================
void AudioDeviceService::setTestToneEnabled(bool enabled)
{
    testToneActive.store(enabled);
}

void AudioDeviceService::renderTestTone(float* const* outputChannels, int numChannels, int numSamples, double sampleRate)
{
    if (outputChannels == nullptr || numChannels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return;

    const bool active = testToneActive.load();
    const float targetGain = active ? targetTestToneLevel : 0.0f;

    // If completely stopped and current gain is 0, exit early
    if (!active && testToneCurrentGain <= 0.0001f)
    {
        testToneCurrentGain = 0.0f;
        return;
    }

    const float phaseDelta = (float)(440.0 * juce::MathConstants<double>::twoPi / sampleRate);
    const float gainStep = (targetGain > testToneCurrentGain) ? 0.002f : -0.002f; // ~10 ms ramp

    for (int s = 0; s < numSamples; ++s)
    {
        if (std::abs(testToneCurrentGain - targetGain) > 0.002f)
            testToneCurrentGain += gainStep;
        else
            testToneCurrentGain = targetGain;

        const float sampleVal = testToneCurrentGain * std::sin(testTonePhase);
        testTonePhase += phaseDelta;
        if (testTonePhase >= juce::MathConstants<float>::twoPi)
            testTonePhase -= juce::MathConstants<float>::twoPi;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (outputChannels[ch] != nullptr)
                outputChannels[ch][s] += sampleVal;
        }
    }
}

// =============================================================================
// Live Input Audio Metering
// =============================================================================
void AudioDeviceService::processInputMeter(const float* const* inputChannels, int numChannels, int numSamples)
{
    if (inputChannels == nullptr || numChannels <= 0 || numSamples <= 0)
    {
        inputPeakMagnitude.store(0.0f);
        inputRmsMagnitude.store(0.0f);
        return;
    }

    float maxPeak = 0.0f;
    double sumSq = 0.0;
    int totalCount = 0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = inputChannels[ch];
        if (src == nullptr) continue;

        for (int s = 0; s < numSamples; ++s)
        {
            const float val = std::abs(src[s]);
            if (val > maxPeak)
                maxPeak = val;
            sumSq += (double)(val * val);
            totalCount++;
        }
    }

    const float rms = (totalCount > 0) ? (float)std::sqrt(sumSq / totalCount) : 0.0f;

    // Smooth peak decay
    float prevPeak = inputPeakMagnitude.load();
    if (maxPeak >= prevPeak)
        prevPeak = maxPeak;
    else
        prevPeak = prevPeak * 0.92f + maxPeak * 0.08f; // natural ~150ms decay

    inputPeakMagnitude.store(prevPeak);
    inputRmsMagnitude.store(rms);
}

float AudioDeviceService::getInputPeakDb() const noexcept
{
    const float mag = inputPeakMagnitude.load();
    if (mag <= 0.00001f)
        return -100.0f;
    return juce::Decibels::gainToDecibels(mag, -100.0f);
}

float AudioDeviceService::getInputRmsDb() const noexcept
{
    const float rms = inputRmsMagnitude.load();
    if (rms <= 0.00001f)
        return -100.0f;
    return juce::Decibels::gainToDecibels(rms, -100.0f);
}

// =============================================================================
// Stability, Disconnect Detection & Auto-Recovery
// =============================================================================
DeviceState AudioDeviceService::getDeviceState() const noexcept
{
    if (supervisor != nullptr)
        return supervisor->getState();

    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev != nullptr && dev->isOpen())
        return DeviceState::Active;

    return DeviceState::Disconnected;
}

juce::String AudioDeviceService::getStatusMessage() const
{
    if (!lastStatusMessage.isEmpty())
        return lastStatusMessage;

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        if (dev->isOpen())
        {
            return "Active: " + dev->getName() + " (" +
                   juce::String(dev->getCurrentSampleRate() * 0.001, 1) + " kHz, " +
                   juce::String(dev->getCurrentBufferSizeSamples()) + " samples, " +
                   juce::String(calculateBufferDurationMs(dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate()), 1) + " ms duration)";
        }
    }
    return "Audio device disconnected.";
}

void AudioDeviceService::triggerReconnection()
{
    if (supervisor != nullptr)
        supervisor->triggerReconnection();
}

void AudioDeviceService::manualRetry()
{
    if (supervisor != nullptr)
    {
        supervisor->stop();
        supervisor->handleDeviceChange();
    }
}

void AudioDeviceService::handleDeviceChange()
{
    if (supervisor != nullptr)
        supervisor->handleDeviceChange();

    if (listener != nullptr)
        listener->onDeviceSetupChanged();
}

void AudioDeviceService::changeListenerCallback(juce::ChangeBroadcaster*)
{
    handleDeviceChange();
}

} // namespace LiveBus
