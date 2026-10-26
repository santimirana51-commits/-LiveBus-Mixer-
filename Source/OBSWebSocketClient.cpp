#include "OBSWebSocketClient.h"

#include <array>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

namespace LiveBus
{
namespace
{
constexpr int maxHandshakeBytes = 16384;
constexpr std::size_t maxWebSocketPayloadBytes = 65536;
constexpr auto websocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

bool fillSecureRandom(void* destination, std::uint32_t bytes)
{
#if defined(_WIN32)
    return BCryptGenRandom(nullptr, static_cast<PUCHAR>(destination), (ULONG)bytes,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#else
    juce::ignoreUnused(destination, bytes);
    return false;
#endif
}

bool sha1Base64(const juce::String& input, juce::String& encoded)
{
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    std::vector<UCHAR> hashObject;
    std::array<UCHAR, 20> digest{};
    const auto* bytes = reinterpret_cast<const UCHAR*>(input.toRawUTF8());
    const auto byteCount = (ULONG)std::strlen(input.toRawUTF8());
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &resultSize, 0) >= 0)
    {
        hashObject.resize(objectSize);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) >= 0 &&
            BCryptHashData(hash, const_cast<PUCHAR>(bytes), byteCount, 0) >= 0 &&
            BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) >= 0)
        {
            encoded = juce::Base64::toBase64(digest.data(), digest.size());
            ok = true;
        }
    }

    if (hash != nullptr)
        BCryptDestroyHash(hash);
    if (algorithm != nullptr)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
#else
    juce::ignoreUnused(input, encoded);
    return false;
#endif
}

bool writeAll(juce::StreamingSocket& socket, const void* bytes, int numBytes,
              const std::function<bool()>& shouldExit)
{
    const auto* source = static_cast<const char*>(bytes);
    int written = 0;
    while (written < numBytes && !shouldExit())
    {
        if (socket.waitUntilReady(false, 2500) <= 0)
            return false;
        const int count = socket.write(source + written, numBytes - written);
        if (count <= 0)
            return false;
        written += count;
    }
    return written == numBytes;
}

bool readExact(juce::StreamingSocket& socket, void* destination, int numBytes,
               const std::function<bool()>& shouldExit)
{
    auto* output = static_cast<char*>(destination);
    int received = 0;
    while (received < numBytes && !shouldExit())
    {
        if (socket.waitUntilReady(true, 500) < 0)
            return false;
        if (shouldExit() || !socket.isConnected())
            return false;
        const int count = socket.read(output + received, numBytes - received, false);
        if (count == 0)
            continue;
        if (count <= 0)
            return false;
        received += count;
    }
    return received == numBytes;
}

bool performWebSocketHandshake(juce::StreamingSocket& socket, const juce::String& host, int port,
                               const std::function<bool()>& shouldExit, juce::String& error)
{
    std::array<unsigned char, 16> nonce{};
    if (!fillSecureRandom(nonce.data(), (ULONG)nonce.size()))
    {
        error = "Secure random generation failed during WebSocket handshake.";
        return false;
    }

    const auto key = juce::Base64::toBase64(nonce.data(), nonce.size());
    const auto acceptInput = key + websocketGuid;
    juce::String expectedAccept;
    if (!sha1Base64(acceptInput, expectedAccept))
    {
        error = "WebSocket handshake validation is unavailable on this platform.";
        return false;
    }

    const auto hostHeader = host.containsChar(':') ? "[" + host + "]" : host;
    const juce::String request = "GET / HTTP/1.1\r\n"
                                 "Host: " + hostHeader + ":" + juce::String(port) + "\r\n"
                                 "Upgrade: websocket\r\n"
                                 "Connection: Upgrade\r\n"
                                 "Sec-WebSocket-Key: " + key + "\r\n"
                                 "Sec-WebSocket-Version: 13\r\n\r\n";
    const char* requestUtf8 = request.toRawUTF8();
    if (!writeAll(socket, requestUtf8, (int)std::strlen(requestUtf8), shouldExit))
    {
        error = "Failed to send the WebSocket upgrade request.";
        return false;
    }

    std::string response;
    response.reserve(1024);
    while ((int)response.size() < maxHandshakeBytes && !shouldExit())
    {
        if (socket.waitUntilReady(true, 2500) <= 0)
        {
            error = "Timed out waiting for the WebSocket upgrade response.";
            return false;
        }
        char byte = 0;
        if (socket.read(&byte, 1, false) != 1)
        {
            error = "Connection closed during the WebSocket upgrade.";
            return false;
        }
        response.push_back(byte);
        if (response.size() >= 4 && response.compare(response.size() - 4, 4, "\r\n\r\n") == 0)
            break;
    }

    const auto statusLine = juce::String::fromUTF8(response.c_str()).upToFirstOccurrenceOf("\r\n", false, false);
    if (response.size() >= (std::size_t)maxHandshakeBytes ||
        !(statusLine == "HTTP/1.1 101" || statusLine.startsWith("HTTP/1.1 101 ")))
    {
        error = "OBS did not accept the WebSocket upgrade.";
        return false;
    }

    const auto responseText = juce::String::fromUTF8(response.c_str());
    const auto headerLines = juce::StringArray::fromLines(responseText);
    bool hasUpgrade = false;
    bool hasConnectionUpgrade = false;
    bool hasExpectedAccept = false;
    for (const auto& line : headerLines)
    {
        const int separator = line.indexOfChar(':');
        if (separator < 0)
            continue;
        const auto name = line.substring(0, separator).trim();
        const auto value = line.substring(separator + 1).trim();
        if (name.equalsIgnoreCase("Upgrade") && value.equalsIgnoreCase("websocket"))
            hasUpgrade = true;
        else if (name.equalsIgnoreCase("Connection") && value.containsIgnoreCase("upgrade"))
            hasConnectionUpgrade = true;
        else if (name.equalsIgnoreCase("Sec-WebSocket-Accept") && value == expectedAccept)
            hasExpectedAccept = true;
    }
    if (!hasUpgrade || !hasConnectionUpgrade || !hasExpectedAccept)
    {
        error = "OBS returned an invalid WebSocket upgrade response.";
        return false;
    }
    return true;
}

bool sendWebSocketFrame(juce::StreamingSocket& socket, int opcode, const void* data, std::size_t payloadSize,
                        const std::function<bool()>& shouldExit);

bool readWebSocketFrame(juce::StreamingSocket& socket, std::vector<char>& payload, int& opcode,
                        const std::function<bool()>& shouldExit)
{
    std::vector<char> message;
    int messageOpcode = 0;
    bool messageStarted = false;

    for (;;)
    {
        std::array<unsigned char, 2> header{};
        if (!readExact(socket, header.data(), (int)header.size(), shouldExit))
            return false;

        const bool finalFrame = (header[0] & 0x80) != 0;
        const int frameOpcode = header[0] & 0x0f;
        if ((header[0] & 0x70) != 0 || (header[1] & 0x80) != 0)
            return false;

        std::uint64_t payloadLength = header[1] & 0x7f;
        if (payloadLength == 126)
        {
            std::array<unsigned char, 2> extended{};
            if (!readExact(socket, extended.data(), (int)extended.size(), shouldExit))
                return false;
            payloadLength = ((std::uint64_t)extended[0] << 8) | extended[1];
        }
        else if (payloadLength == 127)
        {
            std::array<unsigned char, 8> extended{};
            if (!readExact(socket, extended.data(), (int)extended.size(), shouldExit) || (extended[0] & 0x80) != 0)
                return false;
            payloadLength = 0;
            for (auto byte : extended)
                payloadLength = (payloadLength << 8) | byte;
        }

        const bool isControlFrame = frameOpcode >= 0x8;
        if (payloadLength > maxWebSocketPayloadBytes ||
            (isControlFrame && (!finalFrame || payloadLength > 125)))
            return false;

        std::vector<char> framePayload((std::size_t)payloadLength);
        if (payloadLength > 0 && !readExact(socket, framePayload.data(), (int)payloadLength, shouldExit))
            return false;

        if (frameOpcode == 0x9)
        {
            if (!sendWebSocketFrame(socket, 0xA, framePayload.data(), framePayload.size(), shouldExit))
                return false;
            continue;
        }
        if (frameOpcode == 0xA)
            continue;
        if (frameOpcode == 0x8)
        {
            payload = std::move(framePayload);
            opcode = frameOpcode;
            return true;
        }

        if (frameOpcode == 0x1)
        {
            if (messageStarted)
                return false;
            messageOpcode = frameOpcode;
            messageStarted = true;
        }
        else if (frameOpcode != 0x0 || !messageStarted)
            return false;

        if (message.size() + framePayload.size() > maxWebSocketPayloadBytes)
            return false;
        message.insert(message.end(), framePayload.begin(), framePayload.end());

        if (finalFrame)
        {
            payload = std::move(message);
            opcode = messageOpcode;
            return true;
        }
    }
}

bool sendWebSocketFrame(juce::StreamingSocket& socket, int opcode, const void* data, std::size_t payloadSize,
                        const std::function<bool()>& shouldExit)
{
    if (payloadSize > maxWebSocketPayloadBytes)
        return false;
    const auto* payload = static_cast<const unsigned char*>(data);

    std::array<unsigned char, 4> mask{};
    if (!fillSecureRandom(mask.data(), (ULONG)mask.size()))
        return false;

    std::vector<unsigned char> frame;
    frame.reserve(2 + 8 + mask.size() + payloadSize);
    frame.push_back((unsigned char)(0x80 | (opcode & 0x0f)));
    if (payloadSize < 126)
        frame.push_back((unsigned char)(0x80 | payloadSize));
    else if (payloadSize <= 0xffff)
    {
        frame.push_back(0xfe);
        frame.push_back((unsigned char)(payloadSize >> 8));
        frame.push_back((unsigned char)payloadSize);
    }
    else
    {
        frame.push_back(0xff);
        for (int i = 7; i >= 0; --i)
            frame.push_back((unsigned char)(payloadSize >> (i * 8)));
    }

    frame.insert(frame.end(), mask.begin(), mask.end());
    for (std::size_t i = 0; i < payloadSize; ++i)
        frame.push_back(payload[i] ^ mask[i % mask.size()]);
    return writeAll(socket, frame.data(), (int)frame.size(), shouldExit);
}

bool sendWebSocketTextFrame(juce::StreamingSocket& socket, int opcode, const juce::String& text,
                            const std::function<bool()>& shouldExit)
{
    const char* utf8 = text.toRawUTF8();
    return sendWebSocketFrame(socket, opcode, utf8, std::strlen(utf8), shouldExit);
}

bool sha256Base64(const juce::String& input, juce::String& encoded)
{
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    std::vector<UCHAR> hashObject;
    std::array<UCHAR, 32> digest{};
    const char* inputUtf8 = input.toRawUTF8();
    const auto byteCount = (ULONG)std::strlen(inputUtf8);
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &resultSize, 0) >= 0)
    {
        hashObject.resize(objectSize);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) >= 0 &&
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(inputUtf8)), byteCount, 0) >= 0 &&
            BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) >= 0)
        {
            encoded = juce::Base64::toBase64(digest.data(), digest.size());
            ok = true;
        }
    }

    if (hash != nullptr)
        BCryptDestroyHash(hash);
    if (algorithm != nullptr)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
#else
    juce::ignoreUnused(input, encoded);
    return false;
#endif
}

bool makeAuthenticationResponse(const juce::String& password, const juce::String& salt,
                                const juce::String& challenge, juce::String& response)
{
    juce::String secret;
    return sha256Base64(password + salt, secret) && sha256Base64(secret + challenge, response);
}

}

OBSWebSocketClient::OBSWebSocketClient()
    : juce::Thread("LiveBus_OBSWebSocketWorker")
{
}

OBSWebSocketClient::~OBSWebSocketClient()
{
    disconnect();
}

juce::String OBSWebSocketClient::getStatusString() const
{
    juce::String host;
    int port = 4455;
    {
        const juce::ScopedLock lock(configurationLock);
        host = hostAddress;
        port = portNumber;
    }
    juce::String error;
    {
        const juce::ScopedLock lock(errorLock);
        error = lastErrorMessage;
    }

    switch (currentState.load())
    {
        case OBSWebSocketState::Disconnected:
            return "Disconnected";
        case OBSWebSocketState::Connecting:
            return "Connecting to OBS (" + host + ":" + juce::String(port) + ")...";
        case OBSWebSocketState::TcpConnected:
            return "TCP connected; waiting for OBS WebSocket upgrade...";
        case OBSWebSocketState::WebSocketConnected:
            return "WebSocket established; waiting for OBS greeting...";
        case OBSWebSocketState::Authenticating:
            return "WebSocket established; authenticating with OBS...";
        case OBSWebSocketState::Connected:
            return "Connected and identified with OBS WebSocket (port " + juce::String(port) + ")";
        case OBSWebSocketState::AuthenticationFailed:
            return "OBS WebSocket authentication failed: " + error;
        case OBSWebSocketState::ConnectionFailed:
            return "Connection Failed: " + (error.isNotEmpty() ? error : "OBS WebSocket server not responding at " + host + ":" + juce::String(port));
    }
    return "Disconnected";
}

juce::String OBSWebSocketClient::getHost() const
{
    const juce::ScopedLock lock(configurationLock);
    return hostAddress;
}

int OBSWebSocketClient::getPort() const noexcept
{
    const juce::ScopedLock lock(configurationLock);
    return portNumber;
}

void OBSWebSocketClient::configure(const juce::String& host, int port, const juce::String& password)
{
    const juce::ScopedLock lock(configurationLock);
    hostAddress = host.isNotEmpty() ? host : "127.0.0.1";
    portNumber = port > 0 && port <= 65535 ? port : 4455;
    authPassword = password;
    const juce::ScopedLock error(errorLock);
    lastErrorMessage.clear();
}

void OBSWebSocketClient::connectAsync()
{
    disconnect();
    updateState(OBSWebSocketState::Connecting, "Initiating OBS WebSocket connection...");
    startThread();
}

void OBSWebSocketClient::disconnect()
{
    std::shared_ptr<juce::StreamingSocket> activeSocket;
    {
        const juce::ScopedLock lock(socketLock);
        activeSocket = socket;
    }
    if (activeSocket != nullptr)
        activeSocket->close();

    if (isThreadRunning())
    {
        signalThreadShouldExit();
        stopThread(1000);
    }

    {
        const juce::ScopedLock lock(socketLock);
        if (socket == activeSocket)
            socket.reset();
    }
    updateState(OBSWebSocketState::Disconnected, "Disconnected");
}

void OBSWebSocketClient::run()
{
    juce::String host;
    juce::String password;
    int port = 4455;
    {
        const juce::ScopedLock lock(configurationLock);
        host = hostAddress;
        password = authPassword;
        port = portNumber;
    }

    auto activeSocket = std::make_shared<juce::StreamingSocket>();
    {
        const juce::ScopedLock lock(socketLock);
        socket = activeSocket;
    }

    const auto shouldStop = [this]() { return threadShouldExit(); };
    if (!activeSocket->connect(host, port, 2500))
    {
        if (!threadShouldExit())
            updateState(OBSWebSocketState::ConnectionFailed,
                        "Cannot connect to OBS Studio at " + host + ":" + juce::String(port) +
                        ". Confirm the server address and WebSocket server settings.");
        return;
    }

    if (threadShouldExit())
        return;
    updateState(OBSWebSocketState::TcpConnected, "TCP connection to OBS established.");

    juce::String error;
    if (!performWebSocketHandshake(*activeSocket, host, port, shouldStop, error))
    {
        if (!threadShouldExit())
            updateState(OBSWebSocketState::ConnectionFailed, error);
        return;
    }
    updateState(OBSWebSocketState::WebSocketConnected, "OBS WebSocket upgrade completed.");

    std::vector<char> payload;
    int opcode = 0;
    if (!readWebSocketFrame(*activeSocket, payload, opcode, shouldStop) || opcode != 0x1)
    {
        if (!threadShouldExit())
            updateState(OBSWebSocketState::ConnectionFailed, "OBS did not send a valid WebSocket greeting.");
        return;
    }

    const auto greeting = juce::JSON::parse(juce::String::fromUTF8(payload.data(), (int)payload.size()));
    if (!greeting.isObject() || (int)greeting.getProperty("op", -1) != 0)
    {
        updateState(OBSWebSocketState::ConnectionFailed, "OBS sent an invalid WebSocket v5 greeting.");
        return;
    }

    auto* greetingData = greeting.getProperty("d", juce::var()).getDynamicObject();
    if (greetingData == nullptr)
    {
        updateState(OBSWebSocketState::ConnectionFailed, "OBS WebSocket greeting is missing its data object.");
        return;
    }

    const auto authentication = greetingData->getProperty("authentication");
    juce::var identifyData(new juce::DynamicObject());
    auto* identifyDataObject = identifyData.getDynamicObject();
    const int rpcVersion = (int)greetingData->getProperty("rpcVersion");
    if (rpcVersion <= 0)
    {
        updateState(OBSWebSocketState::ConnectionFailed, "OBS WebSocket greeting has an invalid RPC version.");
        return;
    }
    identifyDataObject->setProperty("rpcVersion", rpcVersion);
    identifyDataObject->setProperty("eventSubscriptions", 0);

    if (!authentication.isVoid())
    {
        auto* authData = authentication.getDynamicObject();
        const auto salt = authData != nullptr ? authData->getProperty("salt").toString() : juce::String();
        const auto challenge = authData != nullptr ? authData->getProperty("challenge").toString() : juce::String();
        if (salt.isEmpty() || challenge.isEmpty() || password.isEmpty())
        {
            updateState(OBSWebSocketState::AuthenticationFailed,
                        "OBS requires a WebSocket password, but valid authentication details are unavailable.");
            return;
        }

        juce::String authenticationResponse;
        if (!makeAuthenticationResponse(password, salt, challenge, authenticationResponse))
        {
            updateState(OBSWebSocketState::AuthenticationFailed,
                        "Failed to calculate OBS WebSocket authentication proof.");
            return;
        }
        identifyDataObject->setProperty("authentication", authenticationResponse);
        updateState(OBSWebSocketState::Authenticating, "Authenticating with OBS WebSocket v5...");
    }

    juce::var identifyMessage(new juce::DynamicObject());
    identifyMessage.getDynamicObject()->setProperty("op", 1);
    identifyMessage.getDynamicObject()->setProperty("d", identifyData);
    if (!sendWebSocketTextFrame(*activeSocket, 0x1, juce::JSON::toString(identifyMessage), shouldStop))
    {
        if (!threadShouldExit())
            updateState(OBSWebSocketState::ConnectionFailed, "Failed to send the OBS WebSocket Identify request.");
        return;
    }

    for (;;)
    {
        if (threadShouldExit())
            return;

        if (!readWebSocketFrame(*activeSocket, payload, opcode, shouldStop))
        {
            if (!threadShouldExit())
                updateState(OBSWebSocketState::ConnectionFailed, "OBS WebSocket closed or sent an invalid frame.");
            return;
        }

        if (opcode == 0x8)
        {
            updateState(OBSWebSocketState::Disconnected, "OBS closed the WebSocket connection.");
            return;
        }
        if (opcode == 0x9)
        {
            if (!sendWebSocketFrame(*activeSocket, 0xA, payload.data(), payload.size(), shouldStop))
                return;
            continue;
        }
        if (opcode != 0x1)
            continue;

        const auto response = juce::JSON::parse(juce::String::fromUTF8(payload.data(), (int)payload.size()));
        if (!response.isObject())
            continue;
        const int responseOp = (int)response.getProperty("op", -1);
        if (responseOp == 2)
        {
            updateState(OBSWebSocketState::Connected, "OBS WebSocket v5 Identify succeeded.");
            continue;
        }
        if (responseOp == 3)
        {
            auto* responseData = response.getProperty("d", juce::var()).getDynamicObject();
            const auto code = responseData != nullptr ? (int)responseData->getProperty("code") : 0;
            updateState(OBSWebSocketState::AuthenticationFailed,
                        "OBS rejected the Identify request (code " + juce::String(code) + ").");
            return;
        }
    }
}

void OBSWebSocketClient::updateState(OBSWebSocketState newState, const juce::String& msg)
{
    currentState.store(newState);
    {
        const juce::ScopedLock lock(errorLock);
        if (newState == OBSWebSocketState::ConnectionFailed || newState == OBSWebSocketState::AuthenticationFailed)
            lastErrorMessage = msg;
        else if (newState == OBSWebSocketState::Connecting || newState == OBSWebSocketState::Connected ||
                 newState == OBSWebSocketState::Disconnected)
            lastErrorMessage.clear();
    }
    if (onStateChanged)
    {
        juce::MessageManager::callAsync([this, newState, msg]() {
            if (onStateChanged)
                onStateChanged(newState, msg);
        });
    }
}

} // namespace LiveBus
