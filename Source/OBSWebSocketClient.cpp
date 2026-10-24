#include "OBSWebSocketClient.h"

namespace LiveBus
{

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
    switch (currentState.load())
    {
        case OBSWebSocketState::Disconnected:
            return "Disconnected";
        case OBSWebSocketState::Connecting:
            return "Connecting to OBS (" + hostAddress + ":" + juce::String(portNumber) + ")...";
        case OBSWebSocketState::Connected:
            return "Connected to OBS WebSocket (port " + juce::String(portNumber) + ")";
        case OBSWebSocketState::ConnectionFailed:
            return "Connection Failed: " + (lastErrorMessage.isNotEmpty() ? lastErrorMessage : "OBS WebSocket server not responding at " + hostAddress + ":" + juce::String(portNumber));
    }
    return "Disconnected";
}

void OBSWebSocketClient::configure(const juce::String& host, int port, const juce::String& password)
{
    hostAddress = host.isNotEmpty() ? host : "127.0.0.1";
    portNumber = port > 0 ? port : 4455;
    authPassword = password;
}

void OBSWebSocketClient::connectAsync()
{
    disconnect();
    updateState(OBSWebSocketState::Connecting, "Initiating socket connection to OBS...");
    startThread();
}

void OBSWebSocketClient::disconnect()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        if (socket != nullptr)
            socket->close();
        stopThread(1000);
    }
    if (socket != nullptr)
    {
        socket->close();
        socket.reset();
    }
    updateState(OBSWebSocketState::Disconnected, "Disconnected");
}

void OBSWebSocketClient::run()
{
    socket = std::make_unique<juce::StreamingSocket>();

    // Non-blocking connect with 2.5 second timeout
    bool ok = socket->connect(hostAddress, portNumber, 2500);

    if (threadShouldExit())
        return;

    if (!ok)
    {
        lastErrorMessage = "Cannot connect to OBS Studio at " + hostAddress + ":" + juce::String(portNumber) + ". Make sure OBS is running and WebSocket Server is enabled in OBS (Tools -> WebSocket Server Settings).";
        updateState(OBSWebSocketState::ConnectionFailed, lastErrorMessage);
        return;
    }

    updateState(OBSWebSocketState::Connected, "Connected to OBS Studio WebSocket.");

    // Keep connection alive or listen until thread exit
    while (!threadShouldExit() && socket->isConnected())
    {
        wait(500);
    }

    if (threadShouldExit())
        return;

    if (!socket->isConnected())
    {
        lastErrorMessage = "OBS WebSocket disconnected by remote host.";
        updateState(OBSWebSocketState::Disconnected, lastErrorMessage);
    }
}

void OBSWebSocketClient::updateState(OBSWebSocketState newState, const juce::String& msg)
{
    currentState.store(newState);
    if (onStateChanged)
    {
        juce::MessageManager::callAsync([this, newState, msg]() {
            if (onStateChanged)
                onStateChanged(newState, msg);
        });
    }
}

} // namespace LiveBus
