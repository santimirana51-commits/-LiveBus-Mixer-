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
    Connected,
    ConnectionFailed
};

/**
 * OBS WebSocket v5 connection client & status evaluator.
 *
 * Runs non-blocking TCP socket connection attempts to the OBS Studio
 * WebSocket server (default port 4455).
 * Completely decoupled from real-time audio routing.
 */
class OBSWebSocketClient : public juce::Thread
{
public:
    OBSWebSocketClient();
    ~OBSWebSocketClient() override;

    OBSWebSocketState getState() const noexcept { return currentState.load(); }
    juce::String getStatusString() const;
    juce::String getHost() const { return hostAddress; }
    int getPort() const noexcept { return portNumber; }

    void configure(const juce::String& host, int port, const juce::String& password);
    void connectAsync();
    void disconnect();

    bool isConnected() const noexcept { return currentState.load() == OBSWebSocketState::Connected; }

    std::function<void(OBSWebSocketState newState, const juce::String& message)> onStateChanged;

private:
    void run() override;

    std::atomic<OBSWebSocketState> currentState{OBSWebSocketState::Disconnected};
    juce::String hostAddress{"127.0.0.1"};
    int portNumber = 4455;
    juce::String authPassword;
    juce::String lastErrorMessage;

    std::unique_ptr<juce::StreamingSocket> socket;

    void updateState(OBSWebSocketState newState, const juce::String& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OBSWebSocketClient)
};

} // namespace LiveBus
