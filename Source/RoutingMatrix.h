#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <string>
#include <vector>

constexpr int maxInputs = 16;
constexpr int defaultNumInputs = 4;
constexpr int numInputs = maxInputs;
constexpr int numBuses  = 2; // 0 = Stream, 1 = Monitor
constexpr int maxHwOutputs = 8; // Physical hardware direct outputs 1..8
constexpr int maxAuxBuses = 4; // Aux Send / Return FX Buses (0=Reverb, 1=Delay, 2=Chorus, 3=FX)
constexpr int maxSubmixBuses = 4; // Submix Group Faders (0=Vocals, 1=BGM/Music, 2=Instruments, 3=Custom)

enum class ChannelInputSource : int
{
    Hardware = 0,    // Physical audio interface input channel (0, 1, 2, ...)
    Playback = 1,    // Audio file player (0=L, 1=R, 2=Sum)
    TestSignal = 2,  // Internal Audio Calibration Test Signal
    Disabled = 3,    // None / Disconnected (digital silence)
    YouTube = 4      // Embedded YouTube Player / Loopback Audio Capture
};

enum class ChannelStereoMode : int
{
    Mono = 0,        // Standard Mono (panned to L/R)
    StereoPair = 1,  // Linked with adjacent channel (0&1, 2&3, etc.)
    DualMono = 2     // Dual Mono duplicate
};

inline bool hardwareInputRangeIsAvailable(int firstInput, ChannelStereoMode mode,
                                          int availableInputs) noexcept
{
    if (firstInput < 0 || firstInput >= availableInputs)
        return false;

    return mode != ChannelStereoMode::StereoPair || firstInput + 1 < availableInputs;
}

struct AuxBusState
{
    juce::String name;
    std::atomic<float> returnLevel{0.707f}; // Aux return gain
    std::atomic<float> pan{0.0f};           // -1.0 to +1.0
    std::atomic<bool>  mute{false};
    std::atomic<bool>  solo{false};
    std::atomic<bool>  sendToStream{true};
    std::atomic<bool>  sendToMonitor{true};
    std::atomic<float> sendLevelStream{1.0f};
    std::atomic<float> sendLevelMonitor{1.0f};
    std::atomic<float> peakMeterL{0.0f};
    std::atomic<float> peakMeterR{0.0f};

    // Built-in DSP mode for Aux Return:
    // 0 = Pass-through, 1 = Built-in Studio Reverb, 2 = Built-in Stereo Delay
    std::atomic<int> fxType{0};
    std::atomic<float> fxParam1{0.5f}; // Reverb RoomSize or Delay Time (sec)
    std::atomic<float> fxParam2{0.5f}; // Reverb Damping or Delay Feedback
    std::atomic<float> fxParam3{0.5f}; // Reverb Width or Delay Wet/Dry
};

struct SubmixBusState
{
    juce::String name;
    std::atomic<float> gain{1.0f};          // Group master fader
    std::atomic<float> pan{0.0f};           // -1.0 to +1.0
    std::atomic<bool>  mute{false};
    std::atomic<bool>  solo{false};
    std::atomic<bool>  routeToStream{true};
    std::atomic<bool>  routeToMonitor{true};
    std::atomic<float> sendLevelStream{1.0f};
    std::atomic<float> sendLevelMonitor{1.0f};
    std::atomic<float> peakMeterL{0.0f};
    std::atomic<float> peakMeterR{0.0f};
};

struct InputChannelState
{
    juce::String name;
    std::atomic<float> gain{0.707f};     // ~ -3 dB
    std::atomic<float> pan{0.0f};        // -1.0 (L) to +1.0 (R)
    std::atomic<bool>  mute{false};
    std::atomic<bool>  solo{false};
    std::atomic<float> peakMeter{0.0f};
    std::atomic<float> inputMeter{0.0f}; // Pre-DSP input level

    // Input Routing
    std::atomic<ChannelInputSource> inputSource{ChannelInputSource::Hardware};
    std::atomic<int> hardwareInputChannel{0}; // 0 = In 1, 1 = In 2, ...
    std::atomic<int> playbackChannel{0};      // 0 = L, 1 = R, 2 = Sum
    std::atomic<ChannelStereoMode> stereoMode{ChannelStereoMode::Mono};
    std::atomic<bool> isEnabled{true};
    std::atomic<bool> isSourceAvailable{true};
    std::atomic<bool> duplicateWarning{false};
    std::atomic<bool> stereoLinked{false};

    // Routing enables, send gains, and Pre/Post fader options for Stream (0) and Monitor (1)
    std::array<std::atomic<bool>,  numBuses> routeEnable;
    std::array<std::atomic<float>, numBuses> sendLevel;
    std::atomic<bool> preFaderSendStream{false};
    std::atomic<bool> preFaderSendMonitor{false};

    // Direct Physical Hardware Output Routing (HW Out 1..8)
    std::array<std::atomic<bool>,  maxHwOutputs> hwRouteEnable;
    std::array<std::atomic<float>, maxHwOutputs> hwSendLevel;
    std::array<std::atomic<bool>,  maxHwOutputs> preFaderHwSend;

    // Aux Send Routing (Aux 1..4, e.g. Reverb Send, Delay Send)
    std::array<std::atomic<bool>,  maxAuxBuses> auxRouteEnable;
    std::array<std::atomic<float>, maxAuxBuses> auxSendLevel;
    std::array<std::atomic<bool>,  maxAuxBuses> preFaderAuxSend;

    // Submix / Group Bus Assignment (-1 = Direct to Master, 0..maxSubmixBuses-1 = Submix 1..4)
    std::atomic<int> submixGroup{-1};

    InputChannelState()
    {
        resetForReuse(0);
    }

    void resetForReuse(int channelIndex)
    {
        name.clear();
        gain.store(0.707f, std::memory_order_relaxed);
        pan.store(0.0f, std::memory_order_relaxed);
        mute.store(false, std::memory_order_relaxed);
        solo.store(false, std::memory_order_relaxed);
        peakMeter.store(0.0f, std::memory_order_relaxed);
        inputMeter.store(0.0f, std::memory_order_relaxed);
        inputSource.store(ChannelInputSource::Hardware, std::memory_order_relaxed);
        hardwareInputChannel.store(channelIndex, std::memory_order_relaxed);
        playbackChannel.store(channelIndex % 2, std::memory_order_relaxed);
        stereoMode.store(ChannelStereoMode::Mono, std::memory_order_relaxed);
        isEnabled.store(true, std::memory_order_relaxed);
        isSourceAvailable.store(true, std::memory_order_relaxed);
        duplicateWarning.store(false, std::memory_order_relaxed);
        stereoLinked.store(false, std::memory_order_relaxed);
        for (int b = 0; b < numBuses; ++b)
        {
            routeEnable[(size_t)b].store(true, std::memory_order_relaxed);
            sendLevel[(size_t)b].store(1.0f, std::memory_order_relaxed);
        }
        for (int h = 0; h < maxHwOutputs; ++h)
        {
            hwRouteEnable[(size_t)h].store(false, std::memory_order_relaxed);
            hwSendLevel[(size_t)h].store(1.0f, std::memory_order_relaxed);
            preFaderHwSend[(size_t)h].store(false, std::memory_order_relaxed);
        }
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auxRouteEnable[(size_t)a].store(false, std::memory_order_relaxed);
            auxSendLevel[(size_t)a].store(0.0f, std::memory_order_relaxed);
            preFaderAuxSend[(size_t)a].store(false, std::memory_order_relaxed);
        }
        preFaderSendStream.store(false, std::memory_order_relaxed);
        preFaderSendMonitor.store(false, std::memory_order_relaxed);
        submixGroup.store(-1, std::memory_order_relaxed);
    }
};

class RoutingMatrix
{
public:
    RoutingMatrix()
    {
        channels[0].name = "Microphone";
        channels[0].hardwareInputChannel.store(0);
        channels[0].playbackChannel.store(0);

        channels[1].name = "Guitar / Inst";
        channels[1].hardwareInputChannel.store(1);
        channels[1].playbackChannel.store(1);

        channels[2].name = "Music Left";
        channels[2].hardwareInputChannel.store(2);
        channels[2].playbackChannel.store(0);
        channels[2].pan.store(-1.0f);

        channels[3].name = "Music Right";
        channels[3].hardwareInputChannel.store(3);
        channels[3].playbackChannel.store(1);
        channels[3].pan.store(1.0f);

        for (int i = 4; i < maxInputs; ++i)
        {
            channels[(size_t)i].name = "Input " + juce::String(i + 1);
            channels[(size_t)i].hardwareInputChannel.store(i);
            channels[(size_t)i].playbackChannel.store(i % 2);
            channels[(size_t)i].gain.store(0.707f);
            channels[(size_t)i].pan.store(0.0f);
            channels[(size_t)i].mute.store(false);
            channels[(size_t)i].solo.store(false);
            channels[(size_t)i].peakMeter.store(0.0f);
            channels[(size_t)i].inputMeter.store(0.0f);
            for (int b = 0; b < numBuses; ++b)
            {
                channels[(size_t)i].routeEnable[(size_t)b].store(true);
                channels[(size_t)i].sendLevel[(size_t)b].store(1.0f);
            }
        }

        // Initialize Aux Buses (FX Tracks)
        auxBuses[0].name = "Aux 1 (Reverb)";
        auxBuses[0].fxType.store(1); // 1 = Built-in Studio Reverb
        auxBuses[0].returnLevel.store(0.707f);

        auxBuses[1].name = "Aux 2 (Delay)";
        auxBuses[1].fxType.store(2); // 2 = Built-in Stereo Delay
        auxBuses[1].returnLevel.store(0.707f);

        auxBuses[2].name = "Aux 3";
        auxBuses[2].fxType.store(0);
        auxBuses[2].returnLevel.store(0.707f);

        auxBuses[3].name = "Aux 4";
        auxBuses[3].fxType.store(0);
        auxBuses[3].returnLevel.store(0.707f);

        // Initialize Submix Group Faders
        submixBuses[0].name = "Sub 1 (Vocals)";
        submixBuses[1].name = "Sub 2 (BGM / Music)";
        submixBuses[2].name = "Sub 3 (Instruments)";
        submixBuses[3].name = "Sub 4 (Custom Group)";

        prepare(44100.0, 512);
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        int block = samplesPerBlock > 0 ? samplesPerBlock : 512;
        for (int a = 0; a < maxAuxBuses; ++a)
            auxBuffers[(size_t)a].setSize(2, block, false, false, true);
        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            submixStreamBuffers[(size_t)s].setSize(2, block, false, false, true);
            submixMonitorBuffers[(size_t)s].setSize(2, block, false, false, true);
        }

        juce::Reverb::Parameters rParams;
        rParams.roomSize = 0.5f;
        rParams.damping = 0.4f;
        rParams.wetLevel = 0.85f;
        rParams.dryLevel = 0.0f; // 100% wet send return
        rParams.width = 1.0f;
        rParams.freezeMode = 0.0f;
        builtInReverb.setParameters(rParams);
        builtInReverb.setSampleRate(currentSampleRate);

        int maxDelaySamples = (int)(currentSampleRate * 2.0);
        if (maxDelaySamples < 4096) maxDelaySamples = 4096;
        delayBufferL.assign((size_t)maxDelaySamples, 0.0f);
        delayBufferR.assign((size_t)maxDelaySamples, 0.0f);
        delayWritePos = 0;
    }

    int getNumActiveChannels() const noexcept { return activeChannels.load(std::memory_order_relaxed); }
    void setNumActiveChannels(int count) noexcept { activeChannels.store(juce::jlimit(1, maxInputs, count), std::memory_order_release); }
    void setActiveChannels(int count) noexcept { setNumActiveChannels(count); }
    void updateMasterBusMeter(int busIndex, float peakLeft, float peakRight) noexcept
    {
        if (busIndex < 0 || busIndex >= numBuses)
            return;
        masterBusPeakL[(size_t)busIndex].store(peakLeft, std::memory_order_relaxed);
        masterBusPeakR[(size_t)busIndex].store(peakRight, std::memory_order_relaxed);
    }

    float getMasterBusPeak(int busIndex) const noexcept
    {
        if (busIndex < 0 || busIndex >= numBuses)
            return 0.0f;
        return juce::jmax(masterBusPeakL[(size_t)busIndex].load(std::memory_order_relaxed),
                          masterBusPeakR[(size_t)busIndex].load(std::memory_order_relaxed));
    }

    bool addChannel(const juce::String& defaultName = "")
    {
        int cur = activeChannels.load(std::memory_order_relaxed);
        if (cur >= maxInputs) return false;
        channels[(size_t)cur].resetForReuse(cur);
        if (defaultName.isNotEmpty())
            channels[(size_t)cur].name = defaultName;
        else
            channels[(size_t)cur].name = "Input " + juce::String(cur + 1);
        activeChannels.store(cur + 1, std::memory_order_release);
        updateDuplicateWarnings();
        return true;
    }

    bool removeLastChannel()
    {
        int cur = activeChannels.load(std::memory_order_relaxed);
        if (cur <= 1) return false;
        activeChannels.store(cur - 1, std::memory_order_release);
        updateDuplicateWarnings();
        return true;
    }

    InputChannelState& getChannel(int index) { return channels[(size_t)juce::jlimit(0, maxInputs - 1, index)]; }
    const InputChannelState& getChannel(int index) const { return channels[(size_t)juce::jlimit(0, maxInputs - 1, index)]; }

    AuxBusState& getAuxBus(int index) { return auxBuses[(size_t)juce::jlimit(0, maxAuxBuses - 1, index)]; }
    const AuxBusState& getAuxBus(int index) const { return auxBuses[(size_t)juce::jlimit(0, maxAuxBuses - 1, index)]; }

    SubmixBusState& getSubmixBus(int index) { return submixBuses[(size_t)juce::jlimit(0, maxSubmixBuses - 1, index)]; }
    const SubmixBusState& getSubmixBus(int index) const { return submixBuses[(size_t)juce::jlimit(0, maxSubmixBuses - 1, index)]; }

    juce::AudioBuffer<float>& getAuxBuffer(int index) { return auxBuffers[(size_t)juce::jlimit(0, maxAuxBuses - 1, index)]; }
    juce::AudioBuffer<float>& getSubmixBuffer(int index) { return submixStreamBuffers[(size_t)juce::jlimit(0, maxSubmixBuses - 1, index)]; }

    // Duplicate input source detection and warning
    void updateDuplicateWarnings()
    {
        int count = activeChannels.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i)
            channels[(size_t)i].duplicateWarning.store(false, std::memory_order_relaxed);

        for (int i = 0; i < count; ++i)
        {
            auto& first = channels[(size_t)i];
            if (first.inputSource.load(std::memory_order_relaxed) != ChannelInputSource::Hardware)
                continue;
            const int firstStart = first.hardwareInputChannel.load(std::memory_order_relaxed);
            const int firstCount = first.stereoMode.load(std::memory_order_relaxed) ==
                                           ChannelStereoMode::StereoPair ? 2 : 1;

            for (int j = i + 1; j < count; ++j)
            {
                auto& second = channels[(size_t)j];
                if (second.inputSource.load(std::memory_order_relaxed) != ChannelInputSource::Hardware)
                    continue;

                const int secondStart = second.hardwareInputChannel.load(std::memory_order_relaxed);
                const int secondCount = second.stereoMode.load(std::memory_order_relaxed) ==
                                            ChannelStereoMode::StereoPair ? 2 : 1;
                const bool rangesOverlap = firstStart < secondStart + secondCount &&
                                           secondStart < firstStart + firstCount;
                if (!rangesOverlap)
                    continue;

                first.duplicateWarning.store(true, std::memory_order_relaxed);
                second.duplicateWarning.store(true, std::memory_order_relaxed);
            }
        }
    }

    void setChannelStereoMode(int channelIndex, ChannelStereoMode mode) noexcept
    {
        if (channelIndex < 0 || channelIndex >= maxInputs)
            return;

        channels[(size_t)channelIndex].stereoMode.store(mode, std::memory_order_release);
        validateAvailableInputs(getNumAvailableHardwareInputs());
        updateDuplicateWarnings();
    }

    bool hasAnyDuplicateWarning() const
    {
        int count = activeChannels.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i)
        {
            if (channels[(size_t)i].duplicateWarning.load(std::memory_order_relaxed))
                return true;
        }
        return false;
    }

    void validateAvailableInputs(int numAvailableHardwareInputs)
    {
        availableHardwareInputs.store(juce::jlimit(0, maxInputs, numAvailableHardwareInputs),
                                      std::memory_order_release);
        int count = activeChannels.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i)
        {
            if (channels[(size_t)i].inputSource.load(std::memory_order_relaxed) == ChannelInputSource::Hardware)
            {
                int hw = channels[(size_t)i].hardwareInputChannel.load(std::memory_order_relaxed);
                const auto mode = channels[(size_t)i].stereoMode.load(std::memory_order_relaxed);
                channels[(size_t)i].isSourceAvailable.store(
                    hardwareInputRangeIsAvailable(hw, mode, numAvailableHardwareInputs),
                    std::memory_order_relaxed);
            }
            else
            {
                channels[(size_t)i].isSourceAvailable.store(true, std::memory_order_relaxed);
            }
        }
    }

    int getNumAvailableHardwareInputs() const noexcept
    {
        return availableHardwareInputs.load(std::memory_order_relaxed);
    }

    void validateAvailableOutputs(int numAvailableHardwareOutputs) noexcept
    {
        availableHwOutputs.store(juce::jlimit(0, maxHwOutputs, numAvailableHardwareOutputs), std::memory_order_release);
    }

    int getNumAvailableHardwareOutputs() const noexcept
    {
        return availableHwOutputs.load(std::memory_order_relaxed);
    }

    bool isHardwareOutputAvailable(int hwIndex) const noexcept
    {
        return hwIndex >= 0 && hwIndex < availableHwOutputs.load(std::memory_order_relaxed);
    }

    void applyChannelRouteState(int channelIndex, bool toStream, bool toMonitor) noexcept
    {
        if (channelIndex < 0 || channelIndex >= maxInputs)
            return;

        auto& channel = channels[(size_t)channelIndex];
        channel.routeEnable[0].store(toStream, std::memory_order_release);
        channel.routeEnable[1].store(toMonitor, std::memory_order_release);

        const bool directRouteSelected = toStream || toMonitor;
        const int currentSubmix = channel.submixGroup.load(std::memory_order_relaxed);
        if (directRouteSelected && currentSubmix >= 0)
            channel.submixGroup.store(-1, std::memory_order_release);

        if (!directRouteSelected && currentSubmix >= 0)
        {
            channel.routeEnable[0].store(false, std::memory_order_release);
            channel.routeEnable[1].store(false, std::memory_order_release);
        }
    }

    void assignChannelToSubmix(int channelIndex, int submixIndex) noexcept
    {
        if (channelIndex < 0 || channelIndex >= maxInputs)
            return;

        auto& channel = channels[(size_t)channelIndex];
        channel.submixGroup.store(submixIndex, std::memory_order_release);
        channel.routeEnable[0].store(false, std::memory_order_release);
        channel.routeEnable[1].store(false, std::memory_order_release);
    }

    // Real-time audio routing and summing into Stream, Monitor, Direct Hardware Output, Aux, and Submix buffers
    void routeAndSum(const float* const* inputs, int numInChannels,
                     juce::AudioBuffer<float>& streamBuf,
                     juce::AudioBuffer<float>& monitorBuf,
                     int numSamples,
                     float duckingMultStream = 1.0f,
                     float duckingMultMonitor = 1.0f,
                     uint16_t duckingTargetMask = 0x000C,
                     int duckingTriggerSource = 0,
                     float* const* hwDirectOutputs = nullptr,
                     int numHwDirectOutputs = 0)
    {
        juce::ScopedNoDenormals noDenormals;

        streamBuf.clear(0, numSamples);
        streamBuf.clear(1, numSamples);
        monitorBuf.clear(0, numSamples);
        monitorBuf.clear(1, numSamples);

        // Buffers pre-allocated in prepare() - no setSize() in audio callback
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auxBuffers[(size_t)a].clear(0, numSamples);
            auxBuffers[(size_t)a].clear(1, numSamples);
        }

        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            submixStreamBuffers[(size_t)s].clear(0, numSamples);
            submixStreamBuffers[(size_t)s].clear(1, numSamples);
            submixMonitorBuffers[(size_t)s].clear(0, numSamples);
            submixMonitorBuffers[(size_t)s].clear(1, numSamples);
        }
        std::array<bool, maxAuxBuses> auxHasDuckedSend{};

        const int maxHwOutAllowed = juce::jmin(numHwDirectOutputs, availableHwOutputs.load(std::memory_order_relaxed));
        for (int h = 0; h < maxHwOutAllowed; ++h)
        {
            if (hwDirectOutputs != nullptr && hwDirectOutputs[h] != nullptr)
                juce::FloatVectorOperations::clear(hwDirectOutputs[h], numSamples);
        }

        int activeCount = activeChannels.load(std::memory_order_relaxed);

        std::array<bool, maxSubmixBuses> submixSolo{};
        std::array<bool, maxAuxBuses> auxSolo{};
        std::array<bool, maxSubmixBuses> submixHasSoloChannel{};
        std::array<bool, maxAuxBuses> auxHasSoloChannel{};
        bool anySolo = false;
        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            submixSolo[(size_t)s] = submixBuses[(size_t)s].solo.load(std::memory_order_relaxed);
            anySolo = anySolo || submixSolo[(size_t)s];
        }
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auxSolo[(size_t)a] = auxBuses[(size_t)a].solo.load(std::memory_order_relaxed);
            anySolo = anySolo || auxSolo[(size_t)a];
        }
        for (int i = 0; i < activeCount; ++i)
        {
            auto& channel = channels[(size_t)i];
            if (!channel.solo.load(std::memory_order_relaxed))
                continue;

            anySolo = true;
            const int group = channel.submixGroup.load(std::memory_order_relaxed);
            if (group >= 0 && group < maxSubmixBuses)
                submixHasSoloChannel[(size_t)group] = true;
            for (int a = 0; a < maxAuxBuses; ++a)
            {
                if (channel.auxRouteEnable[(size_t)a].load(std::memory_order_relaxed))
                    auxHasSoloChannel[(size_t)a] = true;
            }
        }

        for (int i = 0; i < activeCount; ++i)
        {
            auto& ch = channels[(size_t)i];

            // Enabled / Solo / Mute gating
            if (!ch.isEnabled.load(std::memory_order_relaxed) || ch.mute.load(std::memory_order_relaxed))
            {
                ch.peakMeter.store(0.0f, std::memory_order_relaxed);
                continue;
            }
            const bool channelSolo = ch.solo.load(std::memory_order_relaxed);
            const int group = ch.submixGroup.load(std::memory_order_relaxed);
            bool feedsSoloBus = group >= 0 && group < maxSubmixBuses &&
                                submixSolo[(size_t)group];
            for (int a = 0; a < maxAuxBuses && !feedsSoloBus; ++a)
                feedsSoloBus = auxSolo[(size_t)a] &&
                    ch.auxRouteEnable[(size_t)a].load(std::memory_order_relaxed);
            if (anySolo && !channelSolo && !feedsSoloBus)
            {
                ch.peakMeter.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            const float* inData = (i < numInChannels && inputs != nullptr) ? inputs[i] : nullptr;
            if (!inData)
            {
                ch.peakMeter.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            float gain = ch.gain.load(std::memory_order_relaxed);
            float panVal = juce::jlimit(-1.0f, 1.0f, ch.pan.load(std::memory_order_relaxed));

            // Equal power panning
            float angle = (panVal + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float panL  = std::cos(angle) * 1.4142f;
            float panR  = std::sin(angle) * 1.4142f;

            // Measure peak
            float maxSample = 0.0f;
            for (int s = 0; s < numSamples; ++s)
            {
                float absVal = std::abs(inData[s]);
                if (absVal > maxSample) maxSample = absVal;
            }
            ch.peakMeter.store(maxSample * gain, std::memory_order_relaxed);

            // Determine if channel is target for ducking (trigger source is never ducked)
            bool isDucked = ((duckingTargetMask & (1 << i)) != 0) && (i != duckingTriggerSource);
            float duckStream = isDucked ? duckingMultStream : 1.0f;
            float duckMonitor = isDucked ? duckingMultMonitor : 1.0f;

            // 1. Aux Sends (FX Tracks)
            for (int a = 0; a < maxAuxBuses; ++a)
            {
                if ((!anySolo || channelSolo || auxSolo[(size_t)a]) &&
                    ch.auxRouteEnable[(size_t)a].load(std::memory_order_relaxed))
                {
                    bool pre = ch.preFaderAuxSend[(size_t)a].load(std::memory_order_relaxed);
                    float effectiveGain = pre ? 1.0f : gain;
                    float send = ch.auxSendLevel[(size_t)a].load(std::memory_order_relaxed) * effectiveGain;
                    auxHasDuckedSend[(size_t)a] = auxHasDuckedSend[(size_t)a] || (isDucked && send != 0.0f);
                    float sendL = panL * send;
                    float sendR = panR * send;

                    float* aL = auxBuffers[(size_t)a].getWritePointer(0);
                    float* aR = auxBuffers[(size_t)a].getWritePointer(1);
                    for (int s = 0; s < numSamples; ++s)
                    {
                        aL[s] += inData[s] * sendL;
                        aR[s] += inData[s] * sendR;
                    }
                }
            }

            // 2. Direct Hardware Outputs Routing (HW Out 1..8)
            for (int h = 0; h < maxHwOutAllowed; ++h)
            {
                if ((!anySolo || channelSolo) &&
                    ch.hwRouteEnable[(size_t)h].load(std::memory_order_relaxed))
                {
                    if (hwDirectOutputs != nullptr && hwDirectOutputs[h] != nullptr)
                    {
                        bool pre = ch.preFaderHwSend[(size_t)h].load(std::memory_order_relaxed);
                        float effectiveGain = pre ? 1.0f : gain;
                        float send = ch.hwSendLevel[(size_t)h].load(std::memory_order_relaxed) * effectiveGain;

                        float* outPtr = hwDirectOutputs[h];
                        for (int s = 0; s < numSamples; ++s)
                            outPtr[s] += inData[s] * send;
                    }
                }
            }

            // 3. Submix Group vs Master Direct Routing
            int grp = ch.submixGroup.load(std::memory_order_relaxed);
            if (grp >= 0 && grp < maxSubmixBuses)
            {
                if (anySolo && !channelSolo && !submixSolo[(size_t)grp])
                    continue;
                // Route to Submix Group
                float streamSendL = panL * gain * duckStream;
                float streamSendR = panR * gain * duckStream;
                float monitorSendL = panL * gain * duckMonitor;
                float monitorSendR = panR * gain * duckMonitor;
                float* subStreamL = submixStreamBuffers[(size_t)grp].getWritePointer(0);
                float* subStreamR = submixStreamBuffers[(size_t)grp].getWritePointer(1);
                float* subMonitorL = submixMonitorBuffers[(size_t)grp].getWritePointer(0);
                float* subMonitorR = submixMonitorBuffers[(size_t)grp].getWritePointer(1);
                for (int s = 0; s < numSamples; ++s)
                {
                    subStreamL[s] += inData[s] * streamSendL;
                    subStreamR[s] += inData[s] * streamSendR;
                    subMonitorL[s] += inData[s] * monitorSendL;
                    subMonitorR[s] += inData[s] * monitorSendR;
                }
            }
            else
            {
                if (anySolo && !channelSolo)
                    continue;
                // Direct to Master Stream Bus
                if (ch.routeEnable[0].load(std::memory_order_relaxed))
                {
                    bool pre = ch.preFaderSendStream.load(std::memory_order_relaxed);
                    float effectiveGain = pre ? 1.0f : gain;
                    float send = ch.sendLevel[0].load(std::memory_order_relaxed) * effectiveGain * duckStream;
                    float sendL = panL * send;
                    float sendR = panR * send;

                    float* sL = streamBuf.getWritePointer(0);
                    float* sR = streamBuf.getWritePointer(1);
                    for (int s = 0; s < numSamples; ++s)
                    {
                        sL[s] += inData[s] * sendL;
                        sR[s] += inData[s] * sendR;
                    }
                }

                // Direct to Master Monitor Bus
                if (ch.routeEnable[1].load(std::memory_order_relaxed))
                {
                    bool pre = ch.preFaderSendMonitor.load(std::memory_order_relaxed);
                    float effectiveGain = pre ? 1.0f : gain;
                    float send = ch.sendLevel[1].load(std::memory_order_relaxed) * effectiveGain * duckMonitor;
                    float sendL = panL * send;
                    float sendR = panR * send;

                    float* mL = monitorBuf.getWritePointer(0);
                    float* mR = monitorBuf.getWritePointer(1);
                    for (int s = 0; s < numSamples; ++s)
                    {
                        mL[s] += inData[s] * sendL;
                        mR[s] += inData[s] * sendR;
                    }
                }
            }
        }

        // 4. Sum Submix Groups into Master Buses
        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            auto& sub = submixBuses[(size_t)s];
            if (sub.mute.load(std::memory_order_relaxed) ||
                (anySolo && !submixSolo[(size_t)s] && !submixHasSoloChannel[(size_t)s]))
            {
                sub.peakMeterL.store(0.0f, std::memory_order_relaxed);
                sub.peakMeterR.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            float subGain = sub.gain.load(std::memory_order_relaxed);
            float subPan = juce::jlimit(-1.0f, 1.0f, sub.pan.load(std::memory_order_relaxed));
            float balL = (subPan <= 0.0f) ? 1.0f : (1.0f - subPan);
            float balR = (subPan >= 0.0f) ? 1.0f : (1.0f + subPan);

            float peakL = juce::jmax(submixStreamBuffers[(size_t)s].getMagnitude(0, 0, numSamples),
                                     submixMonitorBuffers[(size_t)s].getMagnitude(0, 0, numSamples));
            float peakR = juce::jmax(submixStreamBuffers[(size_t)s].getMagnitude(1, 0, numSamples),
                                     submixMonitorBuffers[(size_t)s].getMagnitude(1, 0, numSamples));
            sub.peakMeterL.store(peakL * subGain, std::memory_order_relaxed);
            sub.peakMeterR.store(peakR * subGain, std::memory_order_relaxed);

            if (sub.routeToStream.load(std::memory_order_relaxed))
            {
                const float* inSubL = submixStreamBuffers[(size_t)s].getReadPointer(0);
                const float* inSubR = submixStreamBuffers[(size_t)s].getReadPointer(1);
                float streamSend = sub.sendLevelStream.load(std::memory_order_relaxed) * subGain;
                float gL = streamSend * balL;
                float gR = streamSend * balR;
                float* sL = streamBuf.getWritePointer(0);
                float* sR = streamBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    sL[i] += inSubL[i] * gL;
                    sR[i] += inSubR[i] * gR;
                }
            }

            if (sub.routeToMonitor.load(std::memory_order_relaxed))
            {
                const float* inSubL = submixMonitorBuffers[(size_t)s].getReadPointer(0);
                const float* inSubR = submixMonitorBuffers[(size_t)s].getReadPointer(1);
                float monSend = sub.sendLevelMonitor.load(std::memory_order_relaxed) * subGain;
                float gL = monSend * balL;
                float gR = monSend * balR;
                float* mL = monitorBuf.getWritePointer(0);
                float* mR = monitorBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    mL[i] += inSubL[i] * gL;
                    mR[i] += inSubR[i] * gR;
                }
            }
        }

        // 5. Process Aux Built-in FX (Reverb & Delay) & Sum to Master Buses
        // Aux 1: Built-in Studio Reverb
        if (auxBuses[0].fxType.load(std::memory_order_relaxed) == 1)
        {
            builtInReverb.processStereo(auxBuffers[0].getWritePointer(0), auxBuffers[0].getWritePointer(1), numSamples);
        }

        // Aux 2: Built-in Ping-Pong Stereo Delay
        if (auxBuses[1].fxType.load(std::memory_order_relaxed) == 2 && !delayBufferL.empty())
        {
            float* aL = auxBuffers[1].getWritePointer(0);
            float* aR = auxBuffers[1].getWritePointer(1);
            int delayBufSize = (int)delayBufferL.size();
            float delaySec = juce::jlimit(0.01f, 1.5f, auxBuses[1].fxParam1.load(std::memory_order_relaxed));
            float feedback = juce::jlimit(0.0f, 0.85f, auxBuses[1].fxParam2.load(std::memory_order_relaxed));
            int delaySamples = (int)(delaySec * currentSampleRate);
            if (delaySamples >= delayBufSize) delaySamples = delayBufSize - 1;

            for (int s = 0; s < numSamples; ++s)
            {
                int readPos = delayWritePos - delaySamples;
                if (readPos < 0) readPos += delayBufSize;

                float inSampleL = aL[s];
                float inSampleR = aR[s];
                float delayedL = delayBufferL[(size_t)readPos];
                float delayedR = delayBufferR[(size_t)readPos];

                delayBufferL[(size_t)delayWritePos] = inSampleL + delayedR * feedback;
                delayBufferR[(size_t)delayWritePos] = inSampleR + delayedL * feedback;

                aL[s] = delayedL;
                aR[s] = delayedR;

                delayWritePos = (delayWritePos + 1) % delayBufSize;
            }
        }

        // Sum Aux Returns to Master Buses
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auto& aux = auxBuses[(size_t)a];
            if (aux.mute.load(std::memory_order_relaxed) ||
                (anySolo && !auxSolo[(size_t)a] && !auxHasSoloChannel[(size_t)a]))
            {
                aux.peakMeterL.store(0.0f, std::memory_order_relaxed);
                aux.peakMeterR.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            float retLevel = aux.returnLevel.load(std::memory_order_relaxed);
            float auxPan = juce::jlimit(-1.0f, 1.0f, aux.pan.load(std::memory_order_relaxed));
            float balL = (auxPan <= 0.0f) ? 1.0f : (1.0f - auxPan);
            float balR = (auxPan >= 0.0f) ? 1.0f : (1.0f + auxPan);

            float peakL = auxBuffers[(size_t)a].getMagnitude(0, 0, numSamples);
            float peakR = auxBuffers[(size_t)a].getMagnitude(1, 0, numSamples);
            aux.peakMeterL.store(peakL * retLevel, std::memory_order_relaxed);
            aux.peakMeterR.store(peakR * retLevel, std::memory_order_relaxed);

            const float* inAuxL = auxBuffers[(size_t)a].getReadPointer(0);
            const float* inAuxR = auxBuffers[(size_t)a].getReadPointer(1);

            if (aux.sendToStream.load(std::memory_order_relaxed))
            {
                float streamDuck = auxHasDuckedSend[(size_t)a] ? duckingMultStream : 1.0f;
                float streamSend = aux.sendLevelStream.load(std::memory_order_relaxed) * retLevel * streamDuck;
                float gL = streamSend * balL;
                float gR = streamSend * balR;
                float* sL = streamBuf.getWritePointer(0);
                float* sR = streamBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    sL[i] += inAuxL[i] * gL;
                    sR[i] += inAuxR[i] * gR;
                }
            }

            if (aux.sendToMonitor.load(std::memory_order_relaxed))
            {
                float monitorDuck = auxHasDuckedSend[(size_t)a] ? duckingMultMonitor : 1.0f;
                float monSend = aux.sendLevelMonitor.load(std::memory_order_relaxed) * retLevel * monitorDuck;
                float gL = monSend * balL;
                float gR = monSend * balR;
                float* mL = monitorBuf.getWritePointer(0);
                float* mR = monitorBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    mL[i] += inAuxL[i] * gL;
                    mR[i] += inAuxR[i] * gR;
                }
            }
        }
    }

    void routeAndSum(const juce::AudioBuffer<float>* buffers, int numChannels,
                     juce::AudioBuffer<float>& streamBuf,
                     juce::AudioBuffer<float>& monitorBuf,
                     int numSamples,
                     float duckingMultStream = 1.0f,
                     float duckingMultMonitor = 1.0f,
                     uint16_t duckingTargetMask = 0x000C,
                     int duckingTriggerSource = 0,
                     float* const* hwDirectOutputs = nullptr,
                     int numHwDirectOutputs = 0)
    {
        juce::ScopedNoDenormals noDenormals;

        streamBuf.clear(0, numSamples);
        streamBuf.clear(1, numSamples);
        monitorBuf.clear(0, numSamples);
        monitorBuf.clear(1, numSamples);

        // Buffers pre-allocated in prepare() - no setSize() in audio callback
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auxBuffers[(size_t)a].clear(0, numSamples);
            auxBuffers[(size_t)a].clear(1, numSamples);
        }

        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            submixStreamBuffers[(size_t)s].clear(0, numSamples);
            submixStreamBuffers[(size_t)s].clear(1, numSamples);
            submixMonitorBuffers[(size_t)s].clear(0, numSamples);
            submixMonitorBuffers[(size_t)s].clear(1, numSamples);
        }
        std::array<bool, maxAuxBuses> auxHasDuckedSend{};

        const int maxHwOutAllowed = juce::jmin(numHwDirectOutputs, availableHwOutputs.load(std::memory_order_relaxed));
        for (int h = 0; h < maxHwOutAllowed; ++h)
        {
            if (hwDirectOutputs != nullptr && hwDirectOutputs[h] != nullptr)
                juce::FloatVectorOperations::clear(hwDirectOutputs[h], numSamples);
        }

        int activeCount = activeChannels.load(std::memory_order_relaxed);

        std::array<bool, maxSubmixBuses> submixSolo{};
        std::array<bool, maxAuxBuses> auxSolo{};
        std::array<bool, maxSubmixBuses> submixHasSoloChannel{};
        std::array<bool, maxAuxBuses> auxHasSoloChannel{};
        bool anySolo = false;
        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            submixSolo[(size_t)s] = submixBuses[(size_t)s].solo.load(std::memory_order_relaxed);
            anySolo = anySolo || submixSolo[(size_t)s];
        }
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auxSolo[(size_t)a] = auxBuses[(size_t)a].solo.load(std::memory_order_relaxed);
            anySolo = anySolo || auxSolo[(size_t)a];
        }
        for (int i = 0; i < activeCount; ++i)
        {
            auto& channel = channels[(size_t)i];
            if (!channel.solo.load(std::memory_order_relaxed))
                continue;

            anySolo = true;
            const int group = channel.submixGroup.load(std::memory_order_relaxed);
            if (group >= 0 && group < maxSubmixBuses)
                submixHasSoloChannel[(size_t)group] = true;
            for (int a = 0; a < maxAuxBuses; ++a)
            {
                if (channel.auxRouteEnable[(size_t)a].load(std::memory_order_relaxed))
                    auxHasSoloChannel[(size_t)a] = true;
            }
        }

        for (int i = 0; i < activeCount; ++i)
        {
            auto& ch = channels[(size_t)i];

            // Enabled / Solo / Mute gating
            if (!ch.isEnabled.load(std::memory_order_relaxed) || ch.mute.load(std::memory_order_relaxed))
            {
                ch.peakMeter.store(0.0f, std::memory_order_relaxed);
                continue;
            }
            const bool channelSolo = ch.solo.load(std::memory_order_relaxed);
            const int group = ch.submixGroup.load(std::memory_order_relaxed);
            bool feedsSoloBus = group >= 0 && group < maxSubmixBuses &&
                                submixSolo[(size_t)group];
            for (int a = 0; a < maxAuxBuses && !feedsSoloBus; ++a)
                feedsSoloBus = auxSolo[(size_t)a] &&
                    ch.auxRouteEnable[(size_t)a].load(std::memory_order_relaxed);
            if (anySolo && !channelSolo && !feedsSoloBus)
            {
                ch.peakMeter.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            if (i >= numChannels || buffers == nullptr)
            {
                ch.peakMeter.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            const auto& buf = buffers[i];
            const float* inL = buf.getReadPointer(0);
            const float* inR = buf.getNumChannels() > 1 ? buf.getReadPointer(1) : inL;

            float gain = ch.gain.load(std::memory_order_relaxed);
            float panVal = juce::jlimit(-1.0f, 1.0f, ch.pan.load(std::memory_order_relaxed));
            auto mode = ch.stereoMode.load(std::memory_order_relaxed);

            float peakL = buf.getMagnitude(0, 0, numSamples);
            float peakR = buf.getNumChannels() > 1 ? buf.getMagnitude(1, 0, numSamples) : peakL;
            ch.peakMeter.store(juce::jmax(peakL, peakR) * gain, std::memory_order_relaxed);

            // Determine if channel is target for ducking
            bool isDucked = ((duckingTargetMask & (1 << i)) != 0) && (i != duckingTriggerSource);
            float duckStream = isDucked ? duckingMultStream : 1.0f;
            float duckMonitor = isDucked ? duckingMultMonitor : 1.0f;

            // Panning multipliers
            float balL = 1.0f, balR = 1.0f;
            if (mode == ChannelStereoMode::StereoPair)
            {
                balL = (panVal <= 0.0f) ? 1.0f : (1.0f - panVal);
                balR = (panVal >= 0.0f) ? 1.0f : (1.0f + panVal);
            }
            else
            {
                float angle = (panVal + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                balL = std::cos(angle) * 1.4142f;
                balR = std::sin(angle) * 1.4142f;
            }

            // 1. Aux Sends (FX Tracks)
            for (int a = 0; a < maxAuxBuses; ++a)
            {
                if ((!anySolo || channelSolo || auxSolo[(size_t)a]) &&
                    ch.auxRouteEnable[(size_t)a].load(std::memory_order_relaxed))
                {
                    bool pre = ch.preFaderAuxSend[(size_t)a].load(std::memory_order_relaxed);
                    float effectiveGain = pre ? 1.0f : gain;
                    float send = ch.auxSendLevel[(size_t)a].load(std::memory_order_relaxed) * effectiveGain;
                    auxHasDuckedSend[(size_t)a] = auxHasDuckedSend[(size_t)a] || (isDucked && send != 0.0f);
                    float* aL = auxBuffers[(size_t)a].getWritePointer(0);
                    float* aR = auxBuffers[(size_t)a].getWritePointer(1);

                    if (mode == ChannelStereoMode::StereoPair)
                    {
                        float gL = send * balL;
                        float gR = send * balR;
                        for (int s = 0; s < numSamples; ++s)
                        {
                            aL[s] += inL[s] * gL;
                            aR[s] += inR[s] * gR;
                        }
                    }
                    else
                    {
                        float gL = send * balL;
                        float gR = send * balR;
                        for (int s = 0; s < numSamples; ++s)
                        {
                            aL[s] += inL[s] * gL;
                            aR[s] += inL[s] * gR;
                        }
                    }
                }
            }

            // 2. Direct Hardware Outputs Routing (HW Out 1..8)
            for (int h = 0; h < maxHwOutAllowed; ++h)
            {
                if ((!anySolo || channelSolo) &&
                    ch.hwRouteEnable[(size_t)h].load(std::memory_order_relaxed))
                {
                    if (hwDirectOutputs != nullptr && hwDirectOutputs[h] != nullptr)
                    {
                        bool pre = ch.preFaderHwSend[(size_t)h].load(std::memory_order_relaxed);
                        float effectiveGain = pre ? 1.0f : gain;
                        float send = ch.hwSendLevel[(size_t)h].load(std::memory_order_relaxed) * effectiveGain;

                        float* outPtr = hwDirectOutputs[h];
                        if (mode == ChannelStereoMode::StereoPair)
                        {
                            const float* srcPtr = (h % 2 == 1 && buf.getNumChannels() > 1) ? inR : inL;
                            for (int s = 0; s < numSamples; ++s)
                                outPtr[s] += srcPtr[s] * send;
                        }
                        else
                        {
                            for (int s = 0; s < numSamples; ++s)
                                outPtr[s] += inL[s] * send;
                        }
                    }
                }
            }

            // 3. Submix Group vs Master Direct Routing
            int grp = ch.submixGroup.load(std::memory_order_relaxed);
            if (grp >= 0 && grp < maxSubmixBuses)
            {
                if (anySolo && !channelSolo && !submixSolo[(size_t)grp])
                    continue;
                float* subStreamL = submixStreamBuffers[(size_t)grp].getWritePointer(0);
                float* subStreamR = submixStreamBuffers[(size_t)grp].getWritePointer(1);
                float* subMonitorL = submixMonitorBuffers[(size_t)grp].getWritePointer(0);
                float* subMonitorR = submixMonitorBuffers[(size_t)grp].getWritePointer(1);
                float streamEffGain = gain * duckStream;
                float monitorEffGain = gain * duckMonitor;

                if (mode == ChannelStereoMode::StereoPair)
                {
                    float streamGL = streamEffGain * balL;
                    float streamGR = streamEffGain * balR;
                    float monitorGL = monitorEffGain * balL;
                    float monitorGR = monitorEffGain * balR;
                    for (int s = 0; s < numSamples; ++s)
                    {
                        subStreamL[s] += inL[s] * streamGL;
                        subStreamR[s] += inR[s] * streamGR;
                        subMonitorL[s] += inL[s] * monitorGL;
                        subMonitorR[s] += inR[s] * monitorGR;
                    }
                }
                else
                {
                    float streamGL = streamEffGain * balL;
                    float streamGR = streamEffGain * balR;
                    float monitorGL = monitorEffGain * balL;
                    float monitorGR = monitorEffGain * balR;
                    for (int s = 0; s < numSamples; ++s)
                    {
                        subStreamL[s] += inL[s] * streamGL;
                        subStreamR[s] += inL[s] * streamGR;
                        subMonitorL[s] += inL[s] * monitorGL;
                        subMonitorR[s] += inL[s] * monitorGR;
                    }
                }
            }
            else
            {
                if (anySolo && !channelSolo)
                    continue;
                // Direct to Stream send
                if (ch.routeEnable[0].load(std::memory_order_relaxed))
                {
                    bool pre = ch.preFaderSendStream.load(std::memory_order_relaxed);
                    float effectiveGain = pre ? 1.0f : gain;
                    float send = ch.sendLevel[0].load(std::memory_order_relaxed) * effectiveGain * duckStream;

                    float* sL = streamBuf.getWritePointer(0);
                    float* sR = streamBuf.getWritePointer(1);

                    if (mode == ChannelStereoMode::StereoPair)
                    {
                        float gainL = send * balL;
                        float gainR = send * balR;
                        for (int s = 0; s < numSamples; ++s)
                        {
                            sL[s] += inL[s] * gainL;
                            sR[s] += inR[s] * gainR;
                        }
                    }
                    else
                    {
                        float gainL = send * balL;
                        float gainR = send * balR;
                        for (int s = 0; s < numSamples; ++s)
                        {
                            sL[s] += inL[s] * gainL;
                            sR[s] += inL[s] * gainR;
                        }
                    }
                }

                // Direct to Monitor send
                if (ch.routeEnable[1].load(std::memory_order_relaxed))
                {
                    bool pre = ch.preFaderSendMonitor.load(std::memory_order_relaxed);
                    float effectiveGain = pre ? 1.0f : gain;
                    float send = ch.sendLevel[1].load(std::memory_order_relaxed) * effectiveGain * duckMonitor;

                    float* mL = monitorBuf.getWritePointer(0);
                    float* mR = monitorBuf.getWritePointer(1);

                    if (mode == ChannelStereoMode::StereoPair)
                    {
                        float gainL = send * balL;
                        float gainR = send * balR;
                        for (int s = 0; s < numSamples; ++s)
                        {
                            mL[s] += inL[s] * gainL;
                            mR[s] += inR[s] * gainR;
                        }
                    }
                    else
                    {
                        float gainL = send * balL;
                        float gainR = send * balR;
                        for (int s = 0; s < numSamples; ++s)
                        {
                            mL[s] += inL[s] * gainL;
                            mR[s] += inR[s] * gainR;
                        }
                    }
                }
            }
        }

        // 4. Sum Submix Groups into Master Buses
        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            auto& sub = submixBuses[(size_t)s];
            if (sub.mute.load(std::memory_order_relaxed) ||
                (anySolo && !submixSolo[(size_t)s] && !submixHasSoloChannel[(size_t)s]))
            {
                sub.peakMeterL.store(0.0f, std::memory_order_relaxed);
                sub.peakMeterR.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            float subGain = sub.gain.load(std::memory_order_relaxed);
            float subPan = juce::jlimit(-1.0f, 1.0f, sub.pan.load(std::memory_order_relaxed));
            float balL = (subPan <= 0.0f) ? 1.0f : (1.0f - subPan);
            float balR = (subPan >= 0.0f) ? 1.0f : (1.0f + subPan);

            float peakL = juce::jmax(submixStreamBuffers[(size_t)s].getMagnitude(0, 0, numSamples),
                                     submixMonitorBuffers[(size_t)s].getMagnitude(0, 0, numSamples));
            float peakR = juce::jmax(submixStreamBuffers[(size_t)s].getMagnitude(1, 0, numSamples),
                                     submixMonitorBuffers[(size_t)s].getMagnitude(1, 0, numSamples));
            sub.peakMeterL.store(peakL * subGain, std::memory_order_relaxed);
            sub.peakMeterR.store(peakR * subGain, std::memory_order_relaxed);

            if (sub.routeToStream.load(std::memory_order_relaxed))
            {
                const float* inSubL = submixStreamBuffers[(size_t)s].getReadPointer(0);
                const float* inSubR = submixStreamBuffers[(size_t)s].getReadPointer(1);
                float streamSend = sub.sendLevelStream.load(std::memory_order_relaxed) * subGain;
                float gL = streamSend * balL;
                float gR = streamSend * balR;
                float* sL = streamBuf.getWritePointer(0);
                float* sR = streamBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    sL[i] += inSubL[i] * gL;
                    sR[i] += inSubR[i] * gR;
                }
            }

            if (sub.routeToMonitor.load(std::memory_order_relaxed))
            {
                const float* inSubL = submixMonitorBuffers[(size_t)s].getReadPointer(0);
                const float* inSubR = submixMonitorBuffers[(size_t)s].getReadPointer(1);
                float monSend = sub.sendLevelMonitor.load(std::memory_order_relaxed) * subGain;
                float gL = monSend * balL;
                float gR = monSend * balR;
                float* mL = monitorBuf.getWritePointer(0);
                float* mR = monitorBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    mL[i] += inSubL[i] * gL;
                    mR[i] += inSubR[i] * gR;
                }
            }
        }

        // 5. Process Aux Built-in FX (Reverb & Delay) & Sum to Master Buses
        if (auxBuses[0].fxType.load(std::memory_order_relaxed) == 1)
        {
            builtInReverb.processStereo(auxBuffers[0].getWritePointer(0), auxBuffers[0].getWritePointer(1), numSamples);
        }

        if (auxBuses[1].fxType.load(std::memory_order_relaxed) == 2 && !delayBufferL.empty())
        {
            float* aL = auxBuffers[1].getWritePointer(0);
            float* aR = auxBuffers[1].getWritePointer(1);
            int delayBufSize = (int)delayBufferL.size();
            float delaySec = juce::jlimit(0.01f, 1.5f, auxBuses[1].fxParam1.load(std::memory_order_relaxed));
            float feedback = juce::jlimit(0.0f, 0.85f, auxBuses[1].fxParam2.load(std::memory_order_relaxed));
            int delaySamples = (int)(delaySec * currentSampleRate);
            if (delaySamples >= delayBufSize) delaySamples = delayBufSize - 1;

            for (int s = 0; s < numSamples; ++s)
            {
                int readPos = delayWritePos - delaySamples;
                if (readPos < 0) readPos += delayBufSize;

                float inSampleL = aL[s];
                float inSampleR = aR[s];
                float delayedL = delayBufferL[(size_t)readPos];
                float delayedR = delayBufferR[(size_t)readPos];

                delayBufferL[(size_t)delayWritePos] = inSampleL + delayedR * feedback;
                delayBufferR[(size_t)delayWritePos] = inSampleR + delayedL * feedback;

                aL[s] = delayedL;
                aR[s] = delayedR;

                delayWritePos = (delayWritePos + 1) % delayBufSize;
            }
        }

        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auto& aux = auxBuses[(size_t)a];
            if (aux.mute.load(std::memory_order_relaxed) ||
                (anySolo && !auxSolo[(size_t)a] && !auxHasSoloChannel[(size_t)a]))
            {
                aux.peakMeterL.store(0.0f, std::memory_order_relaxed);
                aux.peakMeterR.store(0.0f, std::memory_order_relaxed);
                continue;
            }

            float retLevel = aux.returnLevel.load(std::memory_order_relaxed);
            float auxPan = juce::jlimit(-1.0f, 1.0f, aux.pan.load(std::memory_order_relaxed));
            float balL = (auxPan <= 0.0f) ? 1.0f : (1.0f - auxPan);
            float balR = (auxPan >= 0.0f) ? 1.0f : (1.0f + auxPan);

            float peakL = auxBuffers[(size_t)a].getMagnitude(0, 0, numSamples);
            float peakR = auxBuffers[(size_t)a].getMagnitude(1, 0, numSamples);
            aux.peakMeterL.store(peakL * retLevel, std::memory_order_relaxed);
            aux.peakMeterR.store(peakR * retLevel, std::memory_order_relaxed);

            const float* inAuxL = auxBuffers[(size_t)a].getReadPointer(0);
            const float* inAuxR = auxBuffers[(size_t)a].getReadPointer(1);

            if (aux.sendToStream.load(std::memory_order_relaxed))
            {
                float streamDuck = auxHasDuckedSend[(size_t)a] ? duckingMultStream : 1.0f;
                float streamSend = aux.sendLevelStream.load(std::memory_order_relaxed) * retLevel * streamDuck;
                float gL = streamSend * balL;
                float gR = streamSend * balR;
                float* sL = streamBuf.getWritePointer(0);
                float* sR = streamBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    sL[i] += inAuxL[i] * gL;
                    sR[i] += inAuxR[i] * gR;
                }
            }

            if (aux.sendToMonitor.load(std::memory_order_relaxed))
            {
                float monitorDuck = auxHasDuckedSend[(size_t)a] ? duckingMultMonitor : 1.0f;
                float monSend = aux.sendLevelMonitor.load(std::memory_order_relaxed) * retLevel * monitorDuck;
                float gL = monSend * balL;
                float gR = monSend * balR;
                float* mL = monitorBuf.getWritePointer(0);
                float* mR = monitorBuf.getWritePointer(1);
                for (int i = 0; i < numSamples; ++i)
                {
                    mL[i] += inAuxL[i] * gL;
                    mR[i] += inAuxR[i] * gR;
                }
            }
        }
    }

    // Presets
    static std::vector<juce::String> getPresetNames()
    {
        return {
            "Default 4-Channel (Standard Studio)",
            "Broadcast Stream Focus (Music to Stream Only)",
            "Podcast Studio (Playback to Ch 3/4)",
            "Headphone Audition (Pre-Fader Monitor)",
            "Mixcraft DAW Style (BGM Submix & Reverb Aux)"
        };
    }

    void applyRoutingPreset(int presetId)
    {
        int count = activeChannels.load(std::memory_order_relaxed);
        // Ensure we don't exceed available channels
        int channelCount = juce::jmin(count, maxInputs);

        switch (presetId)
        {
            case 0: // Default 4-Channel (or more if available)
                // Configure first 4 channels as default studio inputs
                for (int i = 0; i < channelCount; ++i)
                {
                    channels[(size_t)i].inputSource.store(ChannelInputSource::Hardware);
                    channels[(size_t)i].hardwareInputChannel.store(i);
                    channels[(size_t)i].routeEnable[0].store(true); channels[(size_t)i].routeEnable[1].store(true);
                    channels[(size_t)i].preFaderSendStream.store(false); channels[(size_t)i].preFaderSendMonitor.store(false);
                    channels[(size_t)i].submixGroup.store(-1);
                }
                // Set specific names for first 4
                if (channelCount > 0) channels[0].name = "Microphone";
                if (channelCount > 1) channels[1].name = "Guitar / Inst";
                if (channelCount > 2) channels[2].name = "Music Left";
                if (channelCount > 3) channels[3].name = "Music Right";
                // Initialize remaining channels beyond 4 with default names
                for (int i = 4; i < channelCount; ++i)
                {
                    channels[(size_t)i].name = "Input " + juce::String(i + 1);
                    channels[(size_t)i].inputSource.store(ChannelInputSource::Hardware);
                    channels[(size_t)i].hardwareInputChannel.store(i);
                    channels[(size_t)i].routeEnable[0].store(true); channels[(size_t)i].routeEnable[1].store(true);
                    channels[(size_t)i].preFaderSendStream.store(false); channels[(size_t)i].preFaderSendMonitor.store(false);
                    channels[(size_t)i].submixGroup.store(-1);
                }
                break;

            case 1: // Broadcast Stream Focus (Music only to Stream)
                channels[0].routeEnable[0].store(true); channels[0].routeEnable[1].store(true);
                channels[1].routeEnable[0].store(true); channels[1].routeEnable[1].store(true);
                channels[2].routeEnable[0].store(true); channels[2].routeEnable[1].store(false);
                channels[3].routeEnable[0].store(true); channels[3].routeEnable[1].store(false);
                break;

            case 2: // Podcast Studio (Ch 3&4 set to Playback)
                channels[0].inputSource.store(ChannelInputSource::Hardware); channels[0].hardwareInputChannel.store(0);
                channels[1].inputSource.store(ChannelInputSource::Hardware); channels[1].hardwareInputChannel.store(1);
                channels[2].inputSource.store(ChannelInputSource::Playback); channels[2].playbackChannel.store(0);
                channels[3].inputSource.store(ChannelInputSource::Playback); channels[3].playbackChannel.store(1);
                for (int i = 0; i < count; ++i)
                {
                    channels[(size_t)i].routeEnable[0].store(true);
                    channels[(size_t)i].routeEnable[1].store(true);
                }
                break;

            case 3: // Headphone Audition (Pre-Fader Monitor)
                for (int i = 0; i < count; ++i)
                {
                    channels[(size_t)i].preFaderSendMonitor.store(true);
                    channels[(size_t)i].routeEnable[0].store(true);
                    channels[(size_t)i].routeEnable[1].store(true);
                }
                break;

            case 4: // Mixcraft DAW Style (BGM Submix & Reverb Aux)
                // Mic (Ch 1) & Guitar (Ch 2) send to Aux 1 (Reverb)
                for (int i = 0; i < channelCount && i < 2; ++i)
                {
                    channels[(size_t)i].auxRouteEnable[0].store(true);
                    channels[(size_t)i].auxSendLevel[0].store(i == 0 ? 0.6f : 0.4f);
                }

                // Music L (Ch 3) & Music R (Ch 4) assigned to Submix 2 (BGM Submix)
                for (int i = 2; i < channelCount && i < 4; ++i)
                {
                    channels[(size_t)i].submixGroup.store(1);
                }
                if (channelCount > 2) submixBuses[1].name = "Sub 2 (BGM Submix)";
                if (channelCount > 1) submixBuses[1].gain.store(0.85f);
                submixBuses[1].routeToStream.store(true);
                submixBuses[1].routeToMonitor.store(true);

                // Aux 1 sends to both Stream and Monitor buses
                auxBuses[0].sendToStream.store(true);
                auxBuses[0].sendToMonitor.store(true);
                auxBuses[0].returnLevel.store(0.707f);
                break;
        }
        updateDuplicateWarnings();
    }

    // XML Serialization
    std::unique_ptr<juce::XmlElement> createXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("RoutingMatrix");
        int count = activeChannels.load(std::memory_order_relaxed);
        xml->setAttribute("activeChannels", count);
        xml->setAttribute("availableHwOutputs", availableHwOutputs.load(std::memory_order_relaxed));

        for (int i = 0; i < count; ++i)
        {
            auto& ch = channels[(size_t)i];
            auto* chXml = xml->createNewChildElement("Channel");
            chXml->setAttribute("index", i);
            chXml->setAttribute("name", ch.name);
            chXml->setAttribute("gain", (double)ch.gain.load());
            chXml->setAttribute("pan", (double)ch.pan.load());
            chXml->setAttribute("mute", ch.mute.load());
            chXml->setAttribute("solo", ch.solo.load());
            chXml->setAttribute("isEnabled", ch.isEnabled.load());
            chXml->setAttribute("inputSource", (int)ch.inputSource.load());
            chXml->setAttribute("hardwareInputChannel", ch.hardwareInputChannel.load());
            chXml->setAttribute("playbackChannel", ch.playbackChannel.load());
            chXml->setAttribute("stereoMode", (int)ch.stereoMode.load());
            chXml->setAttribute("submixGroup", ch.submixGroup.load());
            chXml->setAttribute("preFaderStream", ch.preFaderSendStream.load());
            chXml->setAttribute("preFaderMonitor", ch.preFaderSendMonitor.load());
            chXml->setAttribute("routeStream", ch.routeEnable[0].load());
            chXml->setAttribute("sendStream", (double)ch.sendLevel[0].load());
            chXml->setAttribute("routeMonitor", ch.routeEnable[1].load());
            chXml->setAttribute("sendMonitor", (double)ch.sendLevel[1].load());

            for (int h = 0; h < maxHwOutputs; ++h)
            {
                chXml->setAttribute("hwRoute_" + juce::String(h), ch.hwRouteEnable[(size_t)h].load());
                chXml->setAttribute("hwSend_" + juce::String(h), (double)ch.hwSendLevel[(size_t)h].load());
                chXml->setAttribute("hwPre_" + juce::String(h), ch.preFaderHwSend[(size_t)h].load());
            }

            for (int a = 0; a < maxAuxBuses; ++a)
            {
                chXml->setAttribute("auxRoute_" + juce::String(a), ch.auxRouteEnable[(size_t)a].load());
                chXml->setAttribute("auxSend_" + juce::String(a), (double)ch.auxSendLevel[(size_t)a].load());
                chXml->setAttribute("auxPre_" + juce::String(a), ch.preFaderAuxSend[(size_t)a].load());
            }
        }

        // Serialize Aux Buses
        for (int a = 0; a < maxAuxBuses; ++a)
        {
            auto* auxXml = xml->createNewChildElement("AuxBus");
            auxXml->setAttribute("index", a);
            auxXml->setAttribute("name", auxBuses[(size_t)a].name);
            auxXml->setAttribute("returnLevel", (double)auxBuses[(size_t)a].returnLevel.load());
            auxXml->setAttribute("pan", (double)auxBuses[(size_t)a].pan.load());
            auxXml->setAttribute("mute", auxBuses[(size_t)a].mute.load());
            auxXml->setAttribute("solo", auxBuses[(size_t)a].solo.load());
            auxXml->setAttribute("sendToStream", auxBuses[(size_t)a].sendToStream.load());
            auxXml->setAttribute("sendToMonitor", auxBuses[(size_t)a].sendToMonitor.load());
            auxXml->setAttribute("sendLevelStream", (double)auxBuses[(size_t)a].sendLevelStream.load());
            auxXml->setAttribute("sendLevelMonitor", (double)auxBuses[(size_t)a].sendLevelMonitor.load());
            auxXml->setAttribute("fxType", auxBuses[(size_t)a].fxType.load());
            auxXml->setAttribute("fxParam1", (double)auxBuses[(size_t)a].fxParam1.load());
            auxXml->setAttribute("fxParam2", (double)auxBuses[(size_t)a].fxParam2.load());
            auxXml->setAttribute("fxParam3", (double)auxBuses[(size_t)a].fxParam3.load());
        }

        // Serialize Submix Buses
        for (int s = 0; s < maxSubmixBuses; ++s)
        {
            auto* subXml = xml->createNewChildElement("SubmixBus");
            subXml->setAttribute("index", s);
            subXml->setAttribute("name", submixBuses[(size_t)s].name);
            subXml->setAttribute("gain", (double)submixBuses[(size_t)s].gain.load());
            subXml->setAttribute("pan", (double)submixBuses[(size_t)s].pan.load());
            subXml->setAttribute("mute", submixBuses[(size_t)s].mute.load());
            subXml->setAttribute("solo", submixBuses[(size_t)s].solo.load());
            subXml->setAttribute("routeToStream", submixBuses[(size_t)s].routeToStream.load());
            subXml->setAttribute("routeToMonitor", submixBuses[(size_t)s].routeToMonitor.load());
            subXml->setAttribute("sendLevelStream", (double)submixBuses[(size_t)s].sendLevelStream.load());
            subXml->setAttribute("sendLevelMonitor", (double)submixBuses[(size_t)s].sendLevelMonitor.load());
        }

        return xml;
    }

    void restoreFromXml(const juce::XmlElement& xml)
    {
        if (xml.getTagName() != "RoutingMatrix") return;
        const auto readBoundedFloat = [](const juce::XmlElement& element,
                                         const juce::String& attribute,
                                         float fallback, float minimum, float maximum)
        {
            const double value = element.getDoubleAttribute(attribute, fallback);
            if (!std::isfinite(value))
                return fallback;
            return juce::jlimit(minimum, maximum, (float)value);
        };
        const auto readBoundedInt = [](const juce::XmlElement& element,
                                       const juce::String& attribute,
                                       int fallback, int minimum, int maximum)
        {
            const int value = element.getIntAttribute(attribute, fallback);
            return value < minimum || value > maximum ? fallback : value;
        };

        int count = xml.getIntAttribute("activeChannels", defaultNumInputs);
        setNumActiveChannels(count);

        if (xml.hasAttribute("availableHwOutputs"))
            validateAvailableOutputs(xml.getIntAttribute("availableHwOutputs", 2));

        for (auto* chXml : xml.getChildIterator())
        {
            if (chXml->getTagName() == "Channel")
            {
                int idx = chXml->getIntAttribute("index", -1);
                if (idx >= 0 && idx < maxInputs)
                {
                    auto& ch = channels[(size_t)idx];
                    ch.name = chXml->getStringAttribute("name", ch.name);
                    ch.gain.store(readBoundedFloat(*chXml, "gain", 0.707f, 0.0f, 4.0f));
                    ch.pan.store(readBoundedFloat(*chXml, "pan", 0.0f, -1.0f, 1.0f));
                    ch.mute.store(chXml->getBoolAttribute("mute", false));
                    ch.solo.store(chXml->getBoolAttribute("solo", false));
                    ch.isEnabled.store(chXml->getBoolAttribute("isEnabled", true));
                    ch.inputSource.store((ChannelInputSource)readBoundedInt(
                        *chXml, "inputSource", (int)ChannelInputSource::Hardware,
                        (int)ChannelInputSource::Hardware, (int)ChannelInputSource::YouTube));
                    ch.hardwareInputChannel.store(readBoundedInt(
                        *chXml, "hardwareInputChannel", idx, 0, maxInputs - 1));
                    ch.playbackChannel.store(readBoundedInt(
                        *chXml, "playbackChannel", idx % 2, 0, 2));
                    ch.stereoMode.store((ChannelStereoMode)readBoundedInt(
                        *chXml, "stereoMode", (int)ChannelStereoMode::Mono,
                        (int)ChannelStereoMode::Mono, (int)ChannelStereoMode::DualMono));
                    ch.submixGroup.store(readBoundedInt(
                        *chXml, "submixGroup", -1, -1, maxSubmixBuses - 1));
                    ch.preFaderSendStream.store(chXml->getBoolAttribute("preFaderStream", false));
                    ch.preFaderSendMonitor.store(chXml->getBoolAttribute("preFaderMonitor", false));
                    ch.routeEnable[0].store(chXml->getBoolAttribute("routeStream", true));
                    ch.sendLevel[0].store(readBoundedFloat(*chXml, "sendStream", 1.0f, 0.0f, 4.0f));
                    ch.routeEnable[1].store(chXml->getBoolAttribute("routeMonitor", true));
                    ch.sendLevel[1].store(readBoundedFloat(*chXml, "sendMonitor", 1.0f, 0.0f, 4.0f));

                    for (int h = 0; h < maxHwOutputs; ++h)
                    {
                        ch.hwRouteEnable[(size_t)h].store(chXml->getBoolAttribute("hwRoute_" + juce::String(h), false));
                        ch.hwSendLevel[(size_t)h].store(readBoundedFloat(
                            *chXml, "hwSend_" + juce::String(h), 1.0f, 0.0f, 4.0f));
                        ch.preFaderHwSend[(size_t)h].store(chXml->getBoolAttribute("hwPre_" + juce::String(h), false));
                    }

                    for (int a = 0; a < maxAuxBuses; ++a)
                    {
                        ch.auxRouteEnable[(size_t)a].store(chXml->getBoolAttribute("auxRoute_" + juce::String(a), false));
                        ch.auxSendLevel[(size_t)a].store(readBoundedFloat(
                            *chXml, "auxSend_" + juce::String(a), 0.0f, 0.0f, 4.0f));
                        ch.preFaderAuxSend[(size_t)a].store(chXml->getBoolAttribute("auxPre_" + juce::String(a), false));
                    }
                }
            }
            else if (chXml->getTagName() == "AuxBus")
            {
                int a = chXml->getIntAttribute("index", -1);
                if (a >= 0 && a < maxAuxBuses)
                {
                    auto& aux = auxBuses[(size_t)a];
                    aux.name = chXml->getStringAttribute("name", aux.name);
                    aux.returnLevel.store(readBoundedFloat(*chXml, "returnLevel", 0.707f, 0.0f, 4.0f));
                    aux.pan.store(readBoundedFloat(*chXml, "pan", 0.0f, -1.0f, 1.0f));
                    aux.mute.store(chXml->getBoolAttribute("mute", false));
                    aux.solo.store(chXml->getBoolAttribute("solo", false));
                    aux.sendToStream.store(chXml->getBoolAttribute("sendToStream", true));
                    aux.sendToMonitor.store(chXml->getBoolAttribute("sendToMonitor", true));
                    aux.sendLevelStream.store(readBoundedFloat(*chXml, "sendLevelStream", 1.0f, 0.0f, 4.0f));
                    aux.sendLevelMonitor.store(readBoundedFloat(*chXml, "sendLevelMonitor", 1.0f, 0.0f, 4.0f));
                    aux.fxType.store(readBoundedInt(
                        *chXml, "fxType", a == 0 ? 1 : (a == 1 ? 2 : 0), 0, 2));
                    aux.fxParam1.store(readBoundedFloat(*chXml, "fxParam1", 0.5f, 0.0f, 1.5f));
                    aux.fxParam2.store(readBoundedFloat(*chXml, "fxParam2", 0.5f, 0.0f, 0.85f));
                    aux.fxParam3.store(readBoundedFloat(*chXml, "fxParam3", 0.5f, 0.0f, 1.0f));
                }
            }
            else if (chXml->getTagName() == "SubmixBus")
            {
                int s = chXml->getIntAttribute("index", -1);
                if (s >= 0 && s < maxSubmixBuses)
                {
                    auto& sub = submixBuses[(size_t)s];
                    sub.name = chXml->getStringAttribute("name", sub.name);
                    sub.gain.store(readBoundedFloat(*chXml, "gain", 1.0f, 0.0f, 4.0f));
                    sub.pan.store(readBoundedFloat(*chXml, "pan", 0.0f, -1.0f, 1.0f));
                    sub.mute.store(chXml->getBoolAttribute("mute", false));
                    sub.solo.store(chXml->getBoolAttribute("solo", false));
                    sub.routeToStream.store(chXml->getBoolAttribute("routeToStream", true));
                    sub.routeToMonitor.store(chXml->getBoolAttribute("routeToMonitor", true));
                    sub.sendLevelStream.store(readBoundedFloat(*chXml, "sendLevelStream", 1.0f, 0.0f, 4.0f));
                    sub.sendLevelMonitor.store(readBoundedFloat(*chXml, "sendLevelMonitor", 1.0f, 0.0f, 4.0f));
                }
            }
        }
        validateAvailableInputs(getNumAvailableHardwareInputs());
        updateDuplicateWarnings();
    }

private:
    std::atomic<int> activeChannels{defaultNumInputs};
    std::atomic<int> availableHardwareInputs{2};
    std::atomic<int> availableHwOutputs{2}; // Default to 2 outputs until hardware device is validated
    std::array<InputChannelState, numInputs> channels;

    // Aux FX & Submix Engine
    std::array<AuxBusState, maxAuxBuses> auxBuses;
    std::array<SubmixBusState, maxSubmixBuses> submixBuses;
    std::array<std::atomic<float>, numBuses> masterBusPeakL{{ 0.0f, 0.0f }};
    std::array<std::atomic<float>, numBuses> masterBusPeakR{{ 0.0f, 0.0f }};
    std::array<juce::AudioBuffer<float>, maxAuxBuses> auxBuffers;
    std::array<juce::AudioBuffer<float>, maxSubmixBuses> submixStreamBuffers;
    std::array<juce::AudioBuffer<float>, maxSubmixBuses> submixMonitorBuffers;
    juce::Reverb builtInReverb;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int delayWritePos = 0;
    double currentSampleRate = 44100.0;
};

namespace LiveBus
{
    using ChannelInputSource = ::ChannelInputSource;
    using ChannelStereoMode = ::ChannelStereoMode;
    using InputChannelState = ::InputChannelState;
    using RoutingMatrix = ::RoutingMatrix;
}
