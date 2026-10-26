#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>

namespace LiveBus
{

enum class OBSWebSocketState
{
    Disconnected = 0,
    Connecting,
    TcpConnected,
    WebSocketConnected,
    Authenticating,
    Connected,
    AuthenticationFailed,
    ConnectionFailed
};

/**
 * OBS WebSocket v5 connection client & status evaluator.
 *
 * Performs the OBS WebSocket v5 HTTP upgrade, authentication, and Identify
 * handshake on a worker thread (default port 4455).
 * Completely decoupled from real-time audio routing.
 */
class OBSWebSocketClient : public juce::Thread
{
public:
    OBSWebSocketClient();
    ~OBSWebSocketClient() override;

    OBSWebSocketState getState() const noexcept { return currentState.load(); }
    juce::String getStatusString() const;
    juce::String getHost() const;
    int getPort() const noexcept;

    void configure(const juce::String& host, int port, const juce::String& password);
    void connectAsync();
    void disconnect();

    bool isConnected() const noexcept { return currentState.load() == OBSWebSocketState::Connected; }

    std::function<void(OBSWebSocketState newState, const juce::String& message)> onStateChanged;

private:
    void run() override;

    std::atomic<OBSWebSocketState> currentState{OBSWebSocketState::Disconnected};
    juce::CriticalSection configurationLock;
    juce::CriticalSection socketLock;
    juce::CriticalSection errorLock;
    juce::String hostAddress{"127.0.0.1"};
    int portNumber = 4455;
    juce::String authPassword;
    juce::String lastErrorMessage;

    std::shared_ptr<juce::StreamingSocket> socket;

    void updateState(OBSWebSocketState newState, const juce::String& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OBSWebSocketClient)
};

} // namespace LiveBus
