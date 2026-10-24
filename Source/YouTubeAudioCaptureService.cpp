#include "YouTubeAudioCaptureService.h"
#include <cmath>

namespace LiveBus
{

YouTubeAudioCaptureService::YouTubeAudioCaptureService()
{
    fifoBuffer.clear();
}

YouTubeAudioCaptureService::~YouTubeAudioCaptureService()
{
    lifetimeToken->store(false, std::memory_order_release);
    stopDeviceCapture();
}

void YouTubeAudioCaptureService::setCaptureMode(YouTubeCaptureMode mode)
{
    if (mode != YouTubeCaptureMode::VirtualCableOrDevice)
        stopDeviceCapture();

    captureMode.store(mode);
    if (mode == YouTubeCaptureMode::Off)
    {
        simulatedStreamActive.store(false);
        hasError.store(false);
    }
    else if (mode == YouTubeCaptureMode::SimulatedTestStream)
    {
        simulatedStreamActive.store(true);
        hasError.store(false);
    }
    else if (mode == YouTubeCaptureMode::VirtualCableOrDevice)
    {
        simulatedStreamActive.store(false);
        if (selectedDeviceName.isEmpty())
        {
            hasError.store(true);
            lastErrorMessage = "Select a capture input endpoint.";
        }
        else
        {
            startDeviceCapture(selectedDeviceName);
        }
    }
    notifyStatus();
}

void YouTubeAudioCaptureService::setSimulatedStreamEnabled(bool enabled)
{
    simulatedStreamActive.store(enabled);
    if (enabled)
    {
        captureMode.store(YouTubeCaptureMode::SimulatedTestStream);
    }
    else if (captureMode.load() == YouTubeCaptureMode::SimulatedTestStream)
    {
        captureMode.store(YouTubeCaptureMode::Off);
    }
    notifyStatus();
}

juce::StringArray YouTubeAudioCaptureService::getAvailableInputDevices()
{
    juce::AudioDeviceManager tempMgr;
    juce::OwnedArray<juce::AudioIODeviceType> deviceTypes;
    tempMgr.createAudioDeviceTypes(deviceTypes);
    juce::StringArray names;
    for (auto* type : deviceTypes)
    {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames(true))
            names.addIfNotAlreadyThere(name);
    }
    return names;
}

juce::String YouTubeAudioCaptureService::getCurrentDeviceName() const
{
    return selectedDeviceName;
}

bool YouTubeAudioCaptureService::startDeviceCapture(const juce::String& deviceName)
{
    std::lock_guard<std::mutex> lock(deviceControlMutex);
    stopDeviceCaptureUnlocked();
    selectedDeviceName = deviceName;
    captureMode.store(YouTubeCaptureMode::VirtualCableOrDevice);
    simulatedStreamActive.store(false);

    if (deviceName.isEmpty())
    {
        lastErrorMessage = "No capture input endpoint was selected.";
        hasError.store(true);
        notifyStatus();
        return false;
    }

    captureDeviceManager = std::make_unique<juce::AudioDeviceManager>();
    captureDeviceManager->addChangeListener(this);

    // Initialise device manager for input only
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.inputDeviceName = deviceName;
    setup.outputDeviceName = ""; // no output needed
    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = false;

    juce::String err = captureDeviceManager->initialise(2, 0, nullptr, true, deviceName, &setup);
    if (err.isNotEmpty())
    {
        lastErrorMessage = err;
        hasError.store(true);
        captureRunning.store(false);
        captureDeviceManager.reset();
        notifyStatus();
        return false;
    }

    captureDeviceManager->addAudioCallback(this);
    captureRunning.store(true);
    hasError.store(false);
    captureMode.store(YouTubeCaptureMode::VirtualCableOrDevice);
    notifyStatus();
    return true;
}

void YouTubeAudioCaptureService::stopDeviceCapture()
{
    std::lock_guard<std::mutex> lock(deviceControlMutex);
    stopDeviceCaptureUnlocked();
}

void YouTubeAudioCaptureService::stopDeviceCaptureUnlocked()
{
    if (captureDeviceManager != nullptr)
    {
        captureDeviceManager->removeAudioCallback(this);
        captureDeviceManager->removeChangeListener(this);
        captureDeviceManager->closeAudioDevice();
        captureDeviceManager.reset();
    }
    captureRunning.store(false);
    signalDetected.store(false);
    peakMeterL.store(0.0f, std::memory_order_relaxed);
    peakMeterR.store(0.0f, std::memory_order_relaxed);
    notifyStatus();
}

bool YouTubeAudioCaptureService::isCaptureActive() const noexcept
{
    auto mode = captureMode.load();
    if (mode == YouTubeCaptureMode::SimulatedTestStream)
        return simulatedStreamActive.load();
    if (mode == YouTubeCaptureMode::VirtualCableOrDevice)
        return captureRunning.load();
    return false;
}

juce::String YouTubeAudioCaptureService::getStatusDescription() const
{
    auto mode = captureMode.load();
    if (mode == YouTubeCaptureMode::Off)
        return "Audio Capture: Off (Muted)";

    if (mode == YouTubeCaptureMode::SimulatedTestStream)
    {
        return simulatedStreamActive.load() ? "Synthetic test tone active (not YouTube audio)"
                                            : "Synthetic test tone paused";
    }

    if (hasError.load())
        return "Capture Device Error: " + lastErrorMessage;

    if (captureRunning.load())
    {
        juce::String desc = "Endpoint: " + selectedDeviceName;
        if (signalDetected.load())
            desc += " [Signal Present]";
        else
            desc += " [Silent / Standby]";
        return desc;
    }

    return "Capture Standby (Select Virtual Cable or Endpoint)";
}

void YouTubeAudioCaptureService::notifyStatus()
{
    if (onStatusChanged)
    {
        std::weak_ptr<std::atomic<bool>> weakToken(lifetimeToken);
        juce::MessageManager::callAsync([this, weakToken]() {
            const auto token = weakToken.lock();
            if (token != nullptr && token->load(std::memory_order_acquire) && onStatusChanged)
                onStatusChanged();
        });
    }
}

void YouTubeAudioCaptureService::changeListenerCallback(juce::ChangeBroadcaster*)
{
    notifyStatus();
}

void YouTubeAudioCaptureService::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
        captureSampleRate.store(device->getCurrentSampleRate(), std::memory_order_relaxed);
    resamplePhase = 0.0;
    fifo.reset();
    fifoBuffer.clear();
}

void YouTubeAudioCaptureService::audioDeviceStopped()
{
    signalDetected.store(false);
}

void YouTubeAudioCaptureService::audioDeviceError(const juce::String& errorMessage)
{
    lastErrorMessage = errorMessage;
    hasError.store(true);
    notifyStatus();
}

void YouTubeAudioCaptureService::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData, int numInputChannels,
    float* const* /*outputChannelData*/, int /*numOutputChannels*/,
    int numSamples, const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (numInputChannels <= 0 || inputChannelData == nullptr
        || inputChannelData[0] == nullptr || numSamples <= 0)
        return;

    const float* inL = inputChannelData[0];
    const float* inR = (numInputChannels > 1 && inputChannelData[1] != nullptr) ? inputChannelData[1] : inL;

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    float maxSampleL = 0.0f;
    float maxSampleR = 0.0f;
    float sumSquares = 0.0f;

    auto sanitize = [](float val) noexcept -> float {
        if (std::isnan(val) || std::isinf(val)) return 0.0f;
        return juce::jlimit(-2.0f, 2.0f, val);
    };

    if (size1 > 0)
    {
        float* wL = fifoBuffer.getWritePointer(0, start1);
        float* wR = fifoBuffer.getWritePointer(1, start1);
        for (int i = 0; i < size1; ++i)
        {
            float sL = sanitize(inL[i]);
            float sR = sanitize(inR[i]);
            wL[i] = sL;
            wR[i] = sR;
            float absL = std::abs(sL);
            float absR = std::abs(sR);
            maxSampleL = juce::jmax(maxSampleL, absL);
            maxSampleR = juce::jmax(maxSampleR, absR);
            sumSquares += (sL * sL + sR * sR) * 0.5f;
        }
    }

    if (size2 > 0)
    {
        float* wL = fifoBuffer.getWritePointer(0, start2);
        float* wR = fifoBuffer.getWritePointer(1, start2);
        for (int i = 0; i < size2; ++i)
        {
            int srcIdx = size1 + i;
            float sL = sanitize(inL[srcIdx]);
            float sR = sanitize(inR[srcIdx]);
            wL[i] = sL;
            wR[i] = sR;
            float absL = std::abs(sL);
            float absR = std::abs(sR);
            maxSampleL = juce::jmax(maxSampleL, absL);
            maxSampleR = juce::jmax(maxSampleR, absR);
            sumSquares += (sL * sL + sR * sR) * 0.5f;
        }
    }

    fifo.finishedWrite(size1 + size2);

    // Update level meters
    peakMeterL.store(maxSampleL, std::memory_order_relaxed);
    peakMeterR.store(maxSampleR, std::memory_order_relaxed);
    float rms = (numSamples > 0) ? std::sqrt(sumSquares / (float)numSamples) : 0.0f;
    rmsMeter.store(rms, std::memory_order_relaxed);
    signalDetected.store(juce::jmax(maxSampleL, maxSampleR) > 0.0005f, std::memory_order_relaxed);
}

void YouTubeAudioCaptureService::pushAudioBlock(const juce::AudioBuffer<float>& sourceBuffer, int numSamples)
{
    if (numSamples <= 0 || sourceBuffer.getNumChannels() <= 0
        || sourceBuffer.getNumSamples() < numSamples)
        return;
    int numCh = sourceBuffer.getNumChannels();
    const float* inL = sourceBuffer.getReadPointer(0);
    const float* inR = (numCh > 1) ? sourceBuffer.getReadPointer(1) : inL;

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    float maxSampleL = 0.0f;
    float maxSampleR = 0.0f;

    auto sanitize = [](float val) noexcept -> float {
        if (std::isnan(val) || std::isinf(val)) return 0.0f;
        return juce::jlimit(-2.0f, 2.0f, val);
    };

    if (size1 > 0)
    {
        float* wL = fifoBuffer.getWritePointer(0, start1);
        float* wR = fifoBuffer.getWritePointer(1, start1);
        for (int i = 0; i < size1; ++i)
        {
            float sL = sanitize(inL[i]);
            float sR = sanitize(inR[i]);
            wL[i] = sL;
            wR[i] = sR;
            maxSampleL = juce::jmax(maxSampleL, std::abs(sL));
            maxSampleR = juce::jmax(maxSampleR, std::abs(sR));
        }
    }

    if (size2 > 0)
    {
        float* wL = fifoBuffer.getWritePointer(0, start2);
        float* wR = fifoBuffer.getWritePointer(1, start2);
        for (int i = 0; i < size2; ++i)
        {
            int srcIdx = size1 + i;
            float sL = sanitize(inL[srcIdx]);
            float sR = sanitize(inR[srcIdx]);
            wL[i] = sL;
            wR[i] = sR;
            maxSampleL = juce::jmax(maxSampleL, std::abs(sL));
            maxSampleR = juce::jmax(maxSampleR, std::abs(sR));
        }
    }

    fifo.finishedWrite(size1 + size2);
    peakMeterL.store(maxSampleL, std::memory_order_relaxed);
    peakMeterR.store(maxSampleR, std::memory_order_relaxed);
    signalDetected.store(juce::jmax(maxSampleL, maxSampleR) > 0.0005f, std::memory_order_relaxed);
}

void YouTubeAudioCaptureService::pullAudioBlock(juce::AudioBuffer<float>& destinationBuffer, int numSamples, double currentSampleRate)
{
    destinationBuffer.clear();
    if (numSamples <= 0 || destinationBuffer.getNumChannels() < 2
        || destinationBuffer.getNumSamples() < numSamples)
        return;

    auto mode = captureMode.load(std::memory_order_relaxed);
    if (mode == YouTubeCaptureMode::Off)
    {
        signalDetected.store(false, std::memory_order_relaxed);
        peakMeterL.store(0.0f, std::memory_order_relaxed);
        peakMeterR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (mode == YouTubeCaptureMode::SimulatedTestStream)
    {
        if (simulatedStreamActive.load(std::memory_order_relaxed))
        {
            renderSimulatedAudio(destinationBuffer, numSamples, currentSampleRate);
        }
        return;
    }

    // VirtualCableOrDevice mode: read from FIFO
    const int numReady = fifo.getNumReady();
    if (numReady <= 0)
    {
        signalDetected.store(false, std::memory_order_relaxed);
        peakMeterL.store(0.0f, std::memory_order_relaxed);
        peakMeterR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    int start1, size1, start2, size2;
    fifo.prepareToRead(numReady, start1, size1, start2, size2);

    float maxSampleL = 0.0f;
    float maxSampleR = 0.0f;

    auto sanitize = [](float val) noexcept -> float {
        if (std::isnan(val) || std::isinf(val)) return 0.0f;
        return juce::jlimit(-1.0f, 1.0f, val);
    };

    const auto sampleAt = [this, start1, size1, start2, size2](int channel, int index) noexcept
    {
        if (index < size1)
            return fifoBuffer.getSample(channel, start1 + index);
        return fifoBuffer.getSample(channel, start2 + index - size1);
    };
    const auto captureRate = captureSampleRate.load(std::memory_order_relaxed);
    const auto ratio = currentSampleRate > 0.0 && captureRate > 0.0
                     ? captureRate / currentSampleRate : 1.0;
    double sourcePosition = resamplePhase;
    int outputSamples = 0;
    float* destL = destinationBuffer.getWritePointer(0);
    float* destR = destinationBuffer.getWritePointer(1);

    while (outputSamples < numSamples)
    {
        const int index = static_cast<int>(sourcePosition);
        if (index >= numReady)
            break;

        const float fraction = static_cast<float>(sourcePosition - index);
        if (fraction > 0.0f && index + 1 >= numReady)
            break;

        const int nextIndex = juce::jmin(index + 1, numReady - 1);
        const float left = sanitize(juce::jmap(fraction, sampleAt(0, index), sampleAt(0, nextIndex)));
        const float right = sanitize(juce::jmap(fraction, sampleAt(1, index), sampleAt(1, nextIndex)));
        destL[outputSamples] = left;
        destR[outputSamples] = right;
        maxSampleL = juce::jmax(maxSampleL, std::abs(left));
        maxSampleR = juce::jmax(maxSampleR, std::abs(right));
        sourcePosition += ratio;
        ++outputSamples;
    }

    const int consumedSamples = juce::jmin(static_cast<int>(sourcePosition), numReady);
    fifo.finishedRead(consumedSamples);
    resamplePhase = sourcePosition - consumedSamples;

    peakMeterL.store(maxSampleL, std::memory_order_relaxed);
    peakMeterR.store(maxSampleR, std::memory_order_relaxed);
    signalDetected.store(juce::jmax(maxSampleL, maxSampleR) > 0.0005f, std::memory_order_relaxed);
}

void YouTubeAudioCaptureService::renderSimulatedAudio(juce::AudioBuffer<float>& dest, int numSamples, double sampleRate)
{
    if (sampleRate <= 0.0) sampleRate = 48000.0;
    float invSr = (float)(1.0 / sampleRate);

    // Warm chord: A4 (432 Hz), C#5 (544 Hz), E5 (648 Hz)
    float delta1 = 432.0f * juce::MathConstants<float>::twoPi * invSr;
    float delta2 = 544.0f * juce::MathConstants<float>::twoPi * invSr;
    float delta3 = 648.0f * juce::MathConstants<float>::twoPi * invSr;

    float* wL = dest.getWritePointer(0);
    float* wR = dest.getWritePointer(1);

    float maxSample = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float s1 = std::sin(synthPhase1);
        float s2 = std::sin(synthPhase2);
        float s3 = std::sin(synthPhase3);

        synthPhase1 += delta1;
        if (synthPhase1 > juce::MathConstants<float>::twoPi) synthPhase1 -= juce::MathConstants<float>::twoPi;

        synthPhase2 += delta2;
        if (synthPhase2 > juce::MathConstants<float>::twoPi) synthPhase2 -= juce::MathConstants<float>::twoPi;

        synthPhase3 += delta3;
        if (synthPhase3 > juce::MathConstants<float>::twoPi) synthPhase3 -= juce::MathConstants<float>::twoPi;

        // Rich stereo acoustic sound
        float monoSignal = (s1 * 0.45f + s2 * 0.35f + s3 * 0.20f) * synthGain;
        float sampleL = monoSignal * 0.90f;
        float sampleR = monoSignal * 1.10f;

        wL[i] = sampleL;
        wR[i] = sampleR;

        float absL = std::abs(sampleL);
        if (absL > maxSample) maxSample = absL;
    }

    peakMeterL.store(maxSample, std::memory_order_relaxed);
    peakMeterR.store(maxSample, std::memory_order_relaxed);
    rmsMeter.store(maxSample * 0.707f, std::memory_order_relaxed);
    signalDetected.store(maxSample > 0.0005f, std::memory_order_relaxed);
}

} // namespace LiveBus
