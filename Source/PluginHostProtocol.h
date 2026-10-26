#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace LiveBusIPC
{
    constexpr std::uint32_t magicNumber     = 0x4C495645; // "LIVE"
    constexpr std::uint32_t protocolVersion = 4;
    constexpr std::uint32_t maxFrames       = 8192;
    constexpr std::uint32_t maxChannels     = 2;
    constexpr std::uint32_t maxStateSize    = 65536; // 64KB for VST3 state blob
    constexpr std::uint32_t maxPlugins      = 8;

    class FixedBlockAudioDelay
    {
    public:
        void reset() noexcept
        {
            hasCurrent = false;
            hasPrevious = false;
            currentFrames = 0;
            previousFrames = 0;
            activeBuffer = 0;
        }

        bool hasAudio() const noexcept { return hasCurrent; }

        void storeCurrent(const float* const* input, int channels,
                          std::uint32_t frames) noexcept
        {
            if (input == nullptr || channels <= 0 || channels > (int)maxChannels ||
                frames == 0 || frames > maxFrames)
            {
                reset();
                return;
            }

            previousBuffer = activeBuffer;
            previousFrames = currentFrames;
            hasPrevious = hasCurrent;
            activeBuffer = 1 - activeBuffer;
            for (int channel = 0; channel < channels; ++channel)
                std::memcpy(audio[(size_t)activeBuffer][(size_t)channel].data(),
                            input[channel], (size_t)frames * sizeof(float));
            currentFrames = frames;
            hasCurrent = true;
        }

        bool copyPrevious(float* const* output, int channels,
                          std::uint32_t frames) const noexcept
        {
            if (!hasPrevious || output == nullptr || channels <= 0 ||
                channels > (int)maxChannels || frames != previousFrames)
                return false;

            for (int channel = 0; channel < channels; ++channel)
                std::memcpy(output[channel],
                            audio[(size_t)previousBuffer][(size_t)channel].data(),
                            (size_t)frames * sizeof(float));
            return true;
        }

    private:
        std::array<std::array<std::array<float, maxFrames>, maxChannels>, 2> audio{};
        std::uint32_t currentFrames = 0;
        std::uint32_t previousFrames = 0;
        int activeBuffer = 0;
        int previousBuffer = 0;
        bool hasCurrent = false;
        bool hasPrevious = false;
    };

    constexpr bool isSupportedFrameCount(std::uint32_t frames) noexcept
    {
        return frames > 0 && frames <= maxFrames;
    }

    inline void copyNullTerminatedString(char* destination, std::size_t capacity, const char* source) noexcept
    {
        if (capacity == 0)
            return;

        std::size_t copied = 0;
        if (source != nullptr)
        {
            while (copied + 1 < capacity && source[copied] != '\0')
            {
                destination[copied] = source[copied];
                ++copied;
            }
        }
        destination[copied] = '\0';
    }

    enum class Command : std::uint32_t
    {
        None = 0,
        Ping,
        LoadPlugin,
        ReleasePlugin,
        SetParameter,
        SaveState,
        RestoreState,
        ProcessBlock,
        SetBypass,
        SimulateCrash,
        Shutdown
    };

    enum class Status : std::uint32_t
    {
        Idle = 0,
        Success,
        Error,
        Timeout,
        PluginNotFound,
        InvalidFormat,
        Busy
    };

    class PendingAudioBlock
    {
    public:
        void begin(std::uint32_t sequence, std::uint32_t frames) noexcept
        {
            requestSequence = sequence;
            frameCount = frames;
            pending = true;
            discardOutput = false;
        }

        bool isPending() const noexcept { return pending; }

        bool isComplete(std::uint32_t completedSequence) const noexcept
        {
            return pending && requestSequence == completedSequence;
        }

        bool canUseOutput(std::uint32_t completedSequence, Status status,
                          std::uint32_t currentFrames) const noexcept
        {
            return isComplete(completedSequence)
                && status == Status::Success
                && !discardOutput
                && frameCount == currentFrames;
        }

        void discardLateOutput() noexcept { discardOutput = true; }
        void reset() noexcept { pending = false; discardOutput = false; frameCount = 0; }

    private:
        std::uint32_t requestSequence = 0;
        std::uint32_t frameCount = 0;
        bool pending = false;
        bool discardOutput = false;
    };

    #pragma pack(push, 8)
    struct SharedMemoryData
    {
        std::uint32_t magic;
        std::uint32_t version;
        std::uint32_t hostPid;
        std::uint32_t helperPid;
        std::uint64_t heartbeatCounter;

        // Command and Response
        Command       command;
        Status        status;
        std::uint32_t sequence;
        std::uint32_t outputReadySequence; // Updated by helper upon block completion
        std::uint64_t requestId;

        // Audio parameters
        double        sampleRate;
        std::uint32_t frames;
        std::uint32_t channels;
        std::uint32_t bypass;
        std::uint32_t pluginIndex;
        std::uint32_t pluginLatencySamples;

        // Parameters / Control
        std::uint32_t parameterIndex;
        float         parameterValue;

        // Strings and State
        char          pluginPath[1024];
        char          pluginName[256];
        char          errorMessage[256];
        std::uint32_t stateSize;
        std::uint8_t  stateData[maxStateSize];

        // Audio sample buffers (Inter-process stereo audio transfer)
        float         inputSamples[maxChannels][maxFrames];
        float         outputSamples[maxChannels][maxFrames];
    };
    #pragma pack(pop)
}
