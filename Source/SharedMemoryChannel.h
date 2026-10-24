#pragma once
#include "PluginHostProtocol.h"
#include <string>
#include <memory>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace LiveBusIPC
{
    class SharedMemoryChannel
    {
    public:
        SharedMemoryChannel();
        ~SharedMemoryChannel();

        bool create(const std::string& channelName, bool isHost);
        void close();

        SharedMemoryData* getData() noexcept { return dataPtr; }
        const SharedMemoryData* getData() const noexcept { return dataPtr; }

        bool signalRequest();
        bool waitRequest(uint32_t timeoutMs = 100);

        bool signalResponse();
        bool waitResponse(uint32_t timeoutMs = 100);

        bool isConnected() const noexcept { return dataPtr != nullptr; }
        static uint32_t loadOutputReadySequence(const SharedMemoryData& data) noexcept;
        static void publishOutputReadySequence(SharedMemoryData& data, uint32_t sequence) noexcept;
        static bool isProcessAlive(uint32_t pid);

    private:
        bool isHostInstance = false;
        SharedMemoryData* dataPtr = nullptr;
#if defined(_WIN32)
        HANDLE hMapFile = nullptr;
        HANDLE hRequestEvent = nullptr;
        HANDLE hResponseEvent = nullptr;
#endif
    };
}
