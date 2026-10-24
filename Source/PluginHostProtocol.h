#pragma once
#include <cstdint>
#include <cstddef>

namespace LiveBusIPC
{
    constexpr std::uint32_t magicNumber     = 0x4C495645; // "LIVE"
    constexpr std::uint32_t protocolVersion = 3;
    constexpr std::uint32_t maxFrames       = 8192;
    constexpr std::uint32_t maxChannels     = 2;
    constexpr std::uint32_t maxStateSize    = 65536; // 64KB for VST3 state blob
    constexpr std::uint32_t maxPlugins      = 8;

    constexpr bool isSupportedFrameCount(std::uint32_t frames) noexcept
    {
        return frames > 0 && frames <= maxFrames;
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
