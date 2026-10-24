#include "OBSIntegrationService.h"

namespace LiveBus
{

OBSIntegrationService::OBSIntegrationService(juce::AudioDeviceManager& primaryDevManager)
    : primaryDeviceManager(primaryDevManager)
{
    fifoBuffer.clear();
}

OBSIntegrationService::~OBSIntegrationService()
{
    closeSecondaryDevice();
}

bool OBSIntegrationService::isVirtualAudioDevice(const juce::String& deviceName) noexcept
{
    auto lower = deviceName.toLowerCase();
    return lower.contains("cable") ||
           lower.contains("vb-audio") ||
           lower.contains("voicemeeter") ||
           lower.contains("virtual") ||
           lower.contains("vac") ||
           lower.contains("wave link") ||
           lower.contains("wavelink") ||
           lower.contains("elgato") ||
           lower.contains("line 1") ||
           lower.contains("line 2") ||
           lower.contains("obs") ||
           lower.contains("blackhole") ||
           lower.contains("soundflower") ||
           lower.contains("loopback");
}

juce::String OBSIntegrationService::identifyVirtualDriverType(const juce::String& deviceName)
{
    auto lower = deviceName.toLowerCase();
    if (lower.contains("virtual audio cable") || lower.contains("vac"))
        return "Virtual Audio Cable (VAC)";
    if (lower.contains("voicemeeter"))
        return "VB-Audio VoiceMeeter";
    if (lower.contains("vb-audio") || lower.contains("cable"))
        return "VB-Audio CABLE";
    if (lower.contains("wave link") || lower.contains("wavelink") || lower.contains("elgato"))
        return "Elgato Wave Link";
    if (lower.contains("obs"))
        return "OBS Virtual Audio";
    if (isVirtualAudioDevice(deviceName))
        return "Virtual Audio Device";
    return "Physical Hardware";
}

std::vector<DetectedAudioDevice> OBSIntegrationService::scanAvailableOutputDevices()
{
    std::vector<DetectedAudioDevice> devices;

    for (auto* type : primaryDeviceManager.getAvailableDeviceTypes())
    {
        if (type == nullptr) continue;
        type->scanForDevices();
        auto outNames = type->getDeviceNames(false); // output devices

        for (const auto& name : outNames)
        {
            DetectedAudioDevice dev;
            dev.name = name;
            dev.isVirtual = isVirtualAudioDevice(name);
            dev.virtualDriverType = identifyVirtualDriverType(name);
            dev.maxOutputChannels = 2; // Default stereo
            devices.push_back(dev);
        }
    }
    return devices;
}

juce::StringArray OBSIntegrationService::getAvailableVirtualCables()
{
    juce::StringArray result;
    auto allDevices = scanAvailableOutputDevices();
    for (const auto& dev : allDevices)
    {
        if (dev.isVirtual)
            result.add(dev.name);
    }
    return result;
}

juce::String OBSIntegrationService::getDetectedVirtualCableSummary() const
{
    auto list = const_cast<OBSIntegrationService*>(this)->getAvailableVirtualCables();
    if (list.isEmpty())
        return "No virtual audio cable detected. Install VB-CABLE or VAC for OBS streaming.";
    return "Detected " + juce::String(list.size()) + " virtual device(s): " + list.joinIntoString(", ");
}

void OBSIntegrationService::setRoutingMode(OBSRoutingMode newMode)
{
    currentRoutingMode.store(newMode);
    notifyStatusChanged();
}

bool OBSIntegrationService::isSecondaryDeviceActive() const noexcept
{
    return secondaryDeviceManager != nullptr && streamRunning.load();
}

juce::String OBSIntegrationService::getSecondaryDeviceName() const
{
    if (secondaryDeviceManager != nullptr && secondaryDeviceManager->getCurrentAudioDevice() != nullptr)
        return secondaryDeviceManager->getCurrentAudioDevice()->getName();
    return chosenVirtualDeviceName;
}

double OBSIntegrationService::getSecondarySampleRate() const
{
    if (secondaryDeviceManager != nullptr && secondaryDeviceManager->getCurrentAudioDevice() != nullptr)
        return secondaryDeviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
    return secondarySampleRate;
}

int OBSIntegrationService::getSecondaryBufferSize() const
{
    if (secondaryDeviceManager != nullptr && secondaryDeviceManager->getCurrentAudioDevice() != nullptr)
        return secondaryDeviceManager->getCurrentAudioDevice()->getCurrentBufferSizeSamples();
    return secondaryBlockSize;
}

juce::String OBSIntegrationService::openSecondaryVirtualDevice(const juce::String& deviceName)
{
    closeSecondaryDevice();

    chosenVirtualDeviceName = deviceName;
    secondaryDeviceManager = std::make_unique<juce::AudioDeviceManager>();
    secondaryDeviceManager->addChangeListener(this);

    // Initialise for 0 inputs and 2 outputs
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    secondaryDeviceManager->getAudioDeviceSetup(setup);
    setup.inputDeviceName = "";
    setup.outputDeviceName = deviceName;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;

    juce::String err = secondaryDeviceManager->initialise(0, 2, nullptr, true, deviceName, &setup);
    if (err.isNotEmpty())
    {
        deviceErrorState.store(true);
        lastErrorMessage = "Failed to open virtual device '" + deviceName + "': " + err;
        secondaryDeviceManager.reset();
        notifyStatusChanged();
        return err;
    }

    secondaryDeviceManager->addAudioCallback(this);
    deviceErrorState.store(false);
    lastErrorMessage = "";

    fifo.reset();
    fifoBuffer.clear();

    notifyStatusChanged();
    return {};
}

void OBSIntegrationService::closeSecondaryDevice()
{
    if (secondaryDeviceManager != nullptr)
    {
        secondaryDeviceManager->removeAudioCallback(this);
        secondaryDeviceManager->removeChangeListener(this);
        secondaryDeviceManager->closeAudioDevice();
        secondaryDeviceManager.reset();
    }
    streamRunning.store(false);
    deviceErrorState.store(false);
    notifyStatusChanged();
}

void OBSIntegrationService::pushStreamAudioBlock(const juce::AudioBuffer<float>& streamBuffer, int numSamples)
{
    // Real-Time Safe: Lock-free writing into circular FIFO
    int freeSpace = fifo.getFreeSpace();
    int samplesToWrite = juce::jmin(numSamples, freeSpace);
    if (samplesToWrite <= 0)
        return;

    int startIndex1, blockSize1, startIndex2, blockSize2;
    fifo.prepareToWrite(samplesToWrite, startIndex1, blockSize1, startIndex2, blockSize2);

    auto sanitizeBlock = [](float* dest, const float* src, int count) {
        for (int i = 0; i < count; ++i)
        {
            float v = src[i];
            dest[i] = (std::isnan(v) || std::isinf(v)) ? 0.0f : juce::jlimit(-1.0f, 1.0f, v);
        }
    };

    if (blockSize1 > 0)
    {
        sanitizeBlock(fifoBuffer.getWritePointer(0, startIndex1), streamBuffer.getReadPointer(0), blockSize1);
        sanitizeBlock(fifoBuffer.getWritePointer(1, startIndex1), streamBuffer.getReadPointer(1), blockSize1);
    }
    if (blockSize2 > 0)
    {
        sanitizeBlock(fifoBuffer.getWritePointer(0, startIndex2), streamBuffer.getReadPointer(0) + blockSize1, blockSize2);
        sanitizeBlock(fifoBuffer.getWritePointer(1, startIndex2), streamBuffer.getReadPointer(1) + blockSize1, blockSize2);
    }

    fifo.finishedWrite(samplesToWrite);
}

void OBSIntegrationService::audioDeviceIOCallbackWithContext(const float* const*, int,
                                                             float* const* outputChannelData, int numOutputChannels,
                                                             int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    if (outputChannelData == nullptr || numOutputChannels < 1)
        return;

    // Clear outputs initially
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    // Read from FIFO
    int numReady = fifo.getNumReady();
    int samplesToRead = juce::jmin(numSamples, numReady);

    if (samplesToRead > 0)
    {
        int startIndex1, blockSize1, startIndex2, blockSize2;
        fifo.prepareToRead(samplesToRead, startIndex1, blockSize1, startIndex2, blockSize2);

        if (blockSize1 > 0)
        {
            if (numOutputChannels >= 1 && outputChannelData[0] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[0], fifoBuffer.getReadPointer(0, startIndex1), blockSize1);
            if (numOutputChannels >= 2 && outputChannelData[1] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[1], fifoBuffer.getReadPointer(1, startIndex1), blockSize1);
        }
        if (blockSize2 > 0)
        {
            if (numOutputChannels >= 1 && outputChannelData[0] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[0] + blockSize1, fifoBuffer.getReadPointer(0, startIndex2), blockSize2);
            if (numOutputChannels >= 2 && outputChannelData[1] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[1] + blockSize1, fifoBuffer.getReadPointer(1, startIndex2), blockSize2);
        }

        fifo.finishedRead(samplesToRead);
    }

    // Apply Test Signal if enabled
    if (testSignalActive.load(std::memory_order_relaxed))
    {
        renderTestSignal(outputChannelData, numOutputChannels, numSamples, secondarySampleRate);
    }

    // Calculate Peak and RMS for OBS meter
    float maxL = 0.0f, maxR = 0.0f;
    float sumSqL = 0.0f, sumSqR = 0.0f;

    if (outputChannelData[0] != nullptr)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            float v = outputChannelData[0][s];
            float absV = std::abs(v);
            if (absV > maxL) maxL = absV;
            sumSqL += v * v;
        }
    }
    if (numOutputChannels >= 2 && outputChannelData[1] != nullptr)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            float v = outputChannelData[1][s];
            float absV = std::abs(v);
            if (absV > maxR) maxR = absV;
            sumSqR += v * v;
        }
    }
    else
    {
        maxR = maxL;
        sumSqR = sumSqL;
    }

    float rmsL = std::sqrt(sumSqL / (float)juce::jmax(1, numSamples));
    float rmsR = std::sqrt(sumSqR / (float)juce::jmax(1, numSamples));

    updateStreamMeter(maxL, maxR, rmsL, rmsR);
}

void OBSIntegrationService::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (!device) return;
    secondarySampleRate = device->getCurrentSampleRate();
    secondaryBlockSize = device->getCurrentBufferSizeSamples();
    streamRunning.store(true);
    deviceErrorState.store(false);
    notifyStatusChanged();
}

void OBSIntegrationService::audioDeviceStopped()
{
    streamRunning.store(false);
    meterPeakL.store(0.0f);
    meterPeakR.store(0.0f);
    meterRmsL.store(0.0f);
    meterRmsR.store(0.0f);
    notifyStatusChanged();
}

void OBSIntegrationService::changeListenerCallback(juce::ChangeBroadcaster*)
{
    notifyStatusChanged();
}

void OBSIntegrationService::setTestSignalEnabled(bool enabled)
{
    testSignalActive.store(enabled);
}

void OBSIntegrationService::renderTestSignal(float* const* outputChannels, int numChannels, int numSamples, double sampleRate)
{
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    const float phaseDelta = (float)(440.0 * juce::MathConstants<double>::twoPi / sampleRate);
    const float rampDelta = 0.005f; // ~200 samples smooth anti-click ramp

    for (int s = 0; s < numSamples; ++s)
    {
        if (testSignalActive.load(std::memory_order_relaxed))
        {
            if (testToneCurrentGain < targetTestToneGain)
                testToneCurrentGain = juce::jmin(targetTestToneGain, testToneCurrentGain + rampDelta);
        }
        else
        {
            if (testToneCurrentGain > 0.0f)
                testToneCurrentGain = juce::jmax(0.0f, testToneCurrentGain - rampDelta);
        }

        float sample = std::sin(testTonePhase) * testToneCurrentGain;
        testTonePhase += phaseDelta;
        if (testTonePhase >= juce::MathConstants<float>::twoPi)
            testTonePhase -= juce::MathConstants<float>::twoPi;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (outputChannels[ch] != nullptr)
                outputChannels[ch][s] += sample;
        }
    }
}

juce::String OBSIntegrationService::getStatusDescription() const
{
    if (deviceErrorState.load())
        return "Device Error: " + lastErrorMessage;

    switch (currentRoutingMode.load())
    {
        case OBSRoutingMode::PrimaryMultiChannel:
            return "Primary Interface Multi-Channel: Stream Bus routed to Ch 1-2 (OBS Capture), Monitor Bus to Ch 3-4 (Headphones).";
        case OBSRoutingMode::DedicatedSecondaryDevice:
            if (isSecondaryDeviceActive())
                return "Dedicated Virtual Cable Streaming: Stream Bus sending live audio to '" + getSecondaryDeviceName() + "' for OBS.";
            return "Dedicated Virtual Device Standby: Please select and activate a virtual audio cable.";
        case OBSRoutingMode::PrimaryStreamOnly:
            return "Primary Stream Only: Entire primary device dedicated to Stream Bus for OBS. Monitor bus is muted to prevent OBS bleed.";
        case OBSRoutingMode::CombinedAudition:
            return "Combined Audition Mode (Audition Only): Stream Bus + Monitor Bus mixed on Output 1-2. (Warning: Monitor bleed to OBS).";
    }
    return "Status: Active";
}

void OBSIntegrationService::notifyStatusChanged()
{
    if (onStatusChanged)
    {
        juce::MessageManager::callAsync([this]() {
            if (onStatusChanged) onStatusChanged();
        });
    }
}

} // namespace LiveBus
