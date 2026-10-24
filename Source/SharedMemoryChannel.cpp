#include "SharedMemoryChannel.h"
#include <iostream>

namespace LiveBusIPC
{
    uint32_t SharedMemoryChannel::loadOutputReadySequence(const SharedMemoryData& data) noexcept
    {
#if defined(_WIN32)
        auto* sequence = reinterpret_cast<volatile LONG*>(
            const_cast<std::uint32_t*>(&data.outputReadySequence));
        return static_cast<std::uint32_t>(InterlockedCompareExchange(sequence, 0, 0));
#else
        return data.outputReadySequence;
#endif
    }

    void SharedMemoryChannel::publishOutputReadySequence(SharedMemoryData& data, uint32_t sequence) noexcept
    {
#if defined(_WIN32)
        auto* outputSequence = reinterpret_cast<volatile LONG*>(&data.outputReadySequence);
        InterlockedExchange(outputSequence, static_cast<LONG>(sequence));
#else
        data.outputReadySequence = sequence;
#endif
    }

    SharedMemoryChannel::SharedMemoryChannel() = default;

    SharedMemoryChannel::~SharedMemoryChannel()
    {
        close();
    }

    bool SharedMemoryChannel::create(const std::string& channelName, bool isHost)
    {
        close();
        isHostInstance = isHost;

#if defined(_WIN32)
        std::wstring baseName(channelName.begin(), channelName.end());
        std::wstring mapName = L"Local\\LiveBusShm_" + baseName;
        std::wstring reqEvtName = L"Local\\LiveBusReqEvt_" + baseName;
        std::wstring resEvtName = L"Local\\LiveBusResEvt_" + baseName;

        if (isHost)
        {
            hMapFile = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                sizeof(SharedMemoryData),
                mapName.c_str()
            );

            if (!hMapFile) return false;

            hRequestEvent = CreateEventW(nullptr, FALSE, FALSE, reqEvtName.c_str());
            hResponseEvent = CreateEventW(nullptr, FALSE, FALSE, resEvtName.c_str());
        }
        else
        {
            hMapFile = OpenFileMappingW(
                FILE_MAP_ALL_ACCESS,
                FALSE,
                mapName.c_str()
            );

            if (!hMapFile) return false;

            hRequestEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, reqEvtName.c_str());
            hResponseEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, resEvtName.c_str());
        }

        if (!hMapFile || !hRequestEvent || !hResponseEvent)
        {
            close();
            return false;
        }

        dataPtr = static_cast<SharedMemoryData*>(MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedMemoryData)
        ));

        if (!dataPtr)
        {
            close();
            return false;
        }

        if (isHost)
        {
            std::memset(dataPtr, 0, sizeof(SharedMemoryData));
            dataPtr->magic = magicNumber;
            dataPtr->version = protocolVersion;
            dataPtr->hostPid = GetCurrentProcessId();
            dataPtr->status = Status::Idle;
        }

        return true;
#else
        return false;
#endif
    }

    void SharedMemoryChannel::close()
    {
#if defined(_WIN32)
        if (dataPtr)
        {
            UnmapViewOfFile(dataPtr);
            dataPtr = nullptr;
        }
        if (hRequestEvent)
        {
            CloseHandle(hRequestEvent);
            hRequestEvent = nullptr;
        }
        if (hResponseEvent)
        {
            CloseHandle(hResponseEvent);
            hResponseEvent = nullptr;
        }
        if (hMapFile)
        {
            CloseHandle(hMapFile);
            hMapFile = nullptr;
        }
#endif
    }

    bool SharedMemoryChannel::signalRequest()
    {
#if defined(_WIN32)
        if (!hRequestEvent) return false;
        return SetEvent(hRequestEvent) != 0;
#else
        return false;
#endif
    }

    bool SharedMemoryChannel::waitRequest(uint32_t timeoutMs)
    {
#if defined(_WIN32)
        if (!hRequestEvent) return false;
        DWORD res = WaitForSingleObject(hRequestEvent, timeoutMs);
        return (res == WAIT_OBJECT_0);
#else
        return false;
#endif
    }

    bool SharedMemoryChannel::signalResponse()
    {
#if defined(_WIN32)
        if (!hResponseEvent) return false;
        return SetEvent(hResponseEvent) != 0;
#else
        return false;
#endif
    }

    bool SharedMemoryChannel::waitResponse(uint32_t timeoutMs)
    {
#if defined(_WIN32)
        if (!hResponseEvent) return false;
        DWORD res = WaitForSingleObject(hResponseEvent, timeoutMs);
        return (res == WAIT_OBJECT_0);
#else
        return false;
#endif
    }

    bool SharedMemoryChannel::isProcessAlive(uint32_t pid)
    {
        if (pid == 0) return false;
#if defined(_WIN32)
        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!hProcess) return false;
        DWORD wait = WaitForSingleObject(hProcess, 0);
        CloseHandle(hProcess);
        return (wait == WAIT_TIMEOUT);
#else
        return false;
#endif
    }
}
