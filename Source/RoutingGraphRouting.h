#pragma once

#include "RoutingMatrix.h"
#include <cstdint>

namespace LiveBus
{

enum class RoutingGraphNodeType
{
    Channel,
    Submix,
    Aux,
    Stream,
    Monitor
};

enum class RoutingGraphPortId : std::uint8_t
{
    AudioInput,
    AudioOutput
};

enum class RoutingGraphChannelMapping : std::uint8_t
{
    EngineNative
};

enum class RoutingGraphValidationResult : std::uint8_t
{
    Valid,
    NodeNotFound,
    PortNotFound,
    DirectionMismatch,
    ChannelLayoutUnsupported,
    CycleUnsupported,
    ConnectionUnsupported
};

struct RoutingGraphNodeId
{
    RoutingGraphNodeType type;
    int index;

    bool operator==(const RoutingGraphNodeId& other) const noexcept
    {
        return type == other.type && index == other.index;
    }

    juce::String toStableString() const
    {
        switch (type)
        {
            case RoutingGraphNodeType::Channel: return "channel:" + juce::String(index);
            case RoutingGraphNodeType::Submix: return "submix:" + juce::String(index);
            case RoutingGraphNodeType::Aux: return "aux:" + juce::String(index);
            case RoutingGraphNodeType::Stream: return "stream:0";
            case RoutingGraphNodeType::Monitor: return "monitor:0";
        }
        return {};
    }

    bool operator!=(const RoutingGraphNodeId& other) const noexcept
    {
        return !(*this == other);
    }
};

struct RoutingGraphEndpoint
{
    RoutingGraphNodeType type;
    int index;

    RoutingGraphNodeId toNodeId() const noexcept { return { type, index }; }
};

struct RoutingGraphConnection
{
    RoutingGraphNodeId sourceNode;
    RoutingGraphPortId sourcePort = RoutingGraphPortId::AudioOutput;
    RoutingGraphNodeId destinationNode;
    RoutingGraphPortId destinationPort = RoutingGraphPortId::AudioInput;
    RoutingGraphChannelMapping channelMapping = RoutingGraphChannelMapping::EngineNative;

    bool operator==(const RoutingGraphConnection& other) const noexcept
    {
        return sourceNode == other.sourceNode && sourcePort == other.sourcePort &&
               destinationNode == other.destinationNode &&
               destinationPort == other.destinationPort && channelMapping == other.channelMapping;
    }
};

enum class RoutingGraphEditResult
{
    Updated,
    AlreadyConnected,
    Unsupported
};

inline bool routingGraphNodeExists(const RoutingMatrix& matrix,
                                   RoutingGraphNodeId node) noexcept
{
    switch (node.type)
    {
        case RoutingGraphNodeType::Channel:
            return node.index >= 0 && node.index < matrix.getNumActiveChannels();
        case RoutingGraphNodeType::Submix:
            return node.index >= 0 && node.index < maxSubmixBuses;
        case RoutingGraphNodeType::Aux:
            return node.index >= 0 && node.index < maxAuxBuses;
        case RoutingGraphNodeType::Stream:
        case RoutingGraphNodeType::Monitor:
            return node.index == 0;
    }
    return false;
}

inline bool routingGraphNodeHasPort(RoutingGraphNodeId node, RoutingGraphPortId port) noexcept
{
    switch (node.type)
    {
        case RoutingGraphNodeType::Channel:
            return port == RoutingGraphPortId::AudioOutput;
        case RoutingGraphNodeType::Submix:
        case RoutingGraphNodeType::Aux:
            return port == RoutingGraphPortId::AudioInput ||
                   port == RoutingGraphPortId::AudioOutput;
        case RoutingGraphNodeType::Stream:
        case RoutingGraphNodeType::Monitor:
            return port == RoutingGraphPortId::AudioInput;
    }
    return false;
}

inline bool isSupportedRoutingGraphConnection(RoutingGraphEndpoint source,
                                              RoutingGraphEndpoint target) noexcept
{
    if (source.type == RoutingGraphNodeType::Channel)
        return ((target.type == RoutingGraphNodeType::Submix &&
                 target.index >= 0 && target.index < maxSubmixBuses) ||
                ((target.type == RoutingGraphNodeType::Stream ||
                  target.type == RoutingGraphNodeType::Monitor) && target.index == 0) ||
                (target.type == RoutingGraphNodeType::Aux &&
                 target.index >= 0 && target.index < maxAuxBuses));

    if (source.type == RoutingGraphNodeType::Submix || source.type == RoutingGraphNodeType::Aux)
        return (target.type == RoutingGraphNodeType::Stream ||
                target.type == RoutingGraphNodeType::Monitor) && target.index == 0;

    return false;
}

inline RoutingGraphValidationResult validateRoutingGraphConnection(
    const RoutingMatrix& matrix, const RoutingGraphConnection& connection) noexcept
{
    if (!routingGraphNodeExists(matrix, connection.sourceNode) ||
        !routingGraphNodeExists(matrix, connection.destinationNode))
        return RoutingGraphValidationResult::NodeNotFound;

    if (connection.sourcePort != RoutingGraphPortId::AudioInput &&
        connection.sourcePort != RoutingGraphPortId::AudioOutput)
        return RoutingGraphValidationResult::PortNotFound;
    if (connection.destinationPort != RoutingGraphPortId::AudioInput &&
        connection.destinationPort != RoutingGraphPortId::AudioOutput)
        return RoutingGraphValidationResult::PortNotFound;

    if ((connection.sourceNode.type == RoutingGraphNodeType::Stream ||
         connection.sourceNode.type == RoutingGraphNodeType::Monitor) &&
        connection.sourcePort == RoutingGraphPortId::AudioOutput)
        return RoutingGraphValidationResult::CycleUnsupported;

    if (!routingGraphNodeHasPort(connection.sourceNode, connection.sourcePort) ||
        !routingGraphNodeHasPort(connection.destinationNode, connection.destinationPort))
        return RoutingGraphValidationResult::DirectionMismatch;

    if (connection.channelMapping != RoutingGraphChannelMapping::EngineNative)
        return RoutingGraphValidationResult::ChannelLayoutUnsupported;

    if (!isSupportedRoutingGraphConnection(
            { connection.sourceNode.type, connection.sourceNode.index },
            { connection.destinationNode.type, connection.destinationNode.index }))
        return RoutingGraphValidationResult::ConnectionUnsupported;

    return RoutingGraphValidationResult::Valid;
}

inline RoutingGraphConnection makeRoutingGraphConnection(RoutingGraphEndpoint source,
                                                           RoutingGraphEndpoint target) noexcept
{
    return { source.toNodeId(), RoutingGraphPortId::AudioOutput,
             target.toNodeId(), RoutingGraphPortId::AudioInput,
             RoutingGraphChannelMapping::EngineNative };
}

inline juce::String routingGraphNodeTypeId(RoutingGraphNodeType type)
{
    switch (type)
    {
        case RoutingGraphNodeType::Channel: return "channel";
        case RoutingGraphNodeType::Submix: return "submix";
        case RoutingGraphNodeType::Aux: return "aux";
        case RoutingGraphNodeType::Stream: return "stream";
        case RoutingGraphNodeType::Monitor: return "monitor";
    }
    return {};
}

inline std::vector<RoutingGraphConnection> getRoutingGraphConnections(const RoutingMatrix& matrix)
{
    std::vector<RoutingGraphConnection> connections;
    const auto add = [&connections](RoutingGraphNodeType sourceType, int sourceIndex,
                                    RoutingGraphNodeType targetType, int targetIndex)
    {
        connections.push_back(makeRoutingGraphConnection(
            { sourceType, sourceIndex }, { targetType, targetIndex }));
    };

    for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
    {
        const auto& state = matrix.getChannel(channel);
        const int submix = state.submixGroup.load(std::memory_order_relaxed);
        if (submix >= 0 && submix < maxSubmixBuses)
            add(RoutingGraphNodeType::Channel, channel, RoutingGraphNodeType::Submix, submix);
        else
        {
            if (state.routeEnable[0].load(std::memory_order_relaxed))
                add(RoutingGraphNodeType::Channel, channel, RoutingGraphNodeType::Stream, 0);
            if (state.routeEnable[1].load(std::memory_order_relaxed))
                add(RoutingGraphNodeType::Channel, channel, RoutingGraphNodeType::Monitor, 0);
        }

        for (int aux = 0; aux < maxAuxBuses; ++aux)
            if (state.auxRouteEnable[(size_t)aux].load(std::memory_order_relaxed))
                add(RoutingGraphNodeType::Channel, channel, RoutingGraphNodeType::Aux, aux);
    }

    for (int submix = 0; submix < maxSubmixBuses; ++submix)
    {
        const auto& state = matrix.getSubmixBus(submix);
        if (state.routeToStream.load(std::memory_order_relaxed))
            add(RoutingGraphNodeType::Submix, submix, RoutingGraphNodeType::Stream, 0);
        if (state.routeToMonitor.load(std::memory_order_relaxed))
            add(RoutingGraphNodeType::Submix, submix, RoutingGraphNodeType::Monitor, 0);
    }

    for (int aux = 0; aux < maxAuxBuses; ++aux)
    {
        const auto& state = matrix.getAuxBus(aux);
        if (state.sendToStream.load(std::memory_order_relaxed))
            add(RoutingGraphNodeType::Aux, aux, RoutingGraphNodeType::Stream, 0);
        if (state.sendToMonitor.load(std::memory_order_relaxed))
            add(RoutingGraphNodeType::Aux, aux, RoutingGraphNodeType::Monitor, 0);
    }
    return connections;
}

inline std::unique_ptr<juce::XmlElement> serializeRoutingGraphConnections(
    const RoutingMatrix& matrix)
{
    auto layout = std::make_unique<juce::XmlElement>("CONNECTIONS");
    for (const auto& connection : getRoutingGraphConnections(matrix))
    {
        auto* element = layout->createNewChildElement("CONNECTION");
        element->setAttribute("sourceType", routingGraphNodeTypeId(connection.sourceNode.type));
        element->setAttribute("sourceIndex", connection.sourceNode.index);
        element->setAttribute("sourcePort", (int)connection.sourcePort);
        element->setAttribute("destinationType",
                              routingGraphNodeTypeId(connection.destinationNode.type));
        element->setAttribute("destinationIndex", connection.destinationNode.index);
        element->setAttribute("destinationPort", (int)connection.destinationPort);
        element->setAttribute("channelMapping", (int)connection.channelMapping);
    }
    return layout;
}

inline RoutingGraphEditResult connectRoutingGraphConnection(
    RoutingMatrix& matrix, const RoutingGraphConnection& connection) noexcept
{
    if (validateRoutingGraphConnection(matrix, connection) != RoutingGraphValidationResult::Valid)
        return RoutingGraphEditResult::Unsupported;

    const auto source = RoutingGraphEndpoint{ connection.sourceNode.type, connection.sourceNode.index };
    const auto target = RoutingGraphEndpoint{ connection.destinationNode.type,
                                               connection.destinationNode.index };

    if (source.type == RoutingGraphNodeType::Channel)
    {
        auto& channel = matrix.getChannel(source.index);
        if (target.type == RoutingGraphNodeType::Submix)
        {
            const bool alreadyConnected =
                channel.submixGroup.load(std::memory_order_relaxed) == target.index;
            channel.submixGroup.store(target.index, std::memory_order_relaxed);
            channel.routeEnable[0].store(false, std::memory_order_relaxed);
            channel.routeEnable[1].store(false, std::memory_order_relaxed);
            return alreadyConnected ? RoutingGraphEditResult::AlreadyConnected
                                    : RoutingGraphEditResult::Updated;
        }

        if (target.type == RoutingGraphNodeType::Stream || target.type == RoutingGraphNodeType::Monitor)
        {
            const int bus = target.type == RoutingGraphNodeType::Stream ? 0 : 1;
            const bool alreadyConnected =
                channel.submixGroup.load(std::memory_order_relaxed) < 0 &&
                channel.routeEnable[(size_t)bus].load(std::memory_order_relaxed);
            channel.submixGroup.store(-1, std::memory_order_relaxed);
            channel.routeEnable[(size_t)bus].store(true, std::memory_order_relaxed);
            return alreadyConnected ? RoutingGraphEditResult::AlreadyConnected
                                    : RoutingGraphEditResult::Updated;
        }

        const bool alreadyConnected =
            channel.auxRouteEnable[(size_t)target.index].load(std::memory_order_relaxed);
        if (!alreadyConnected)
        {
            channel.auxRouteEnable[(size_t)target.index].store(true, std::memory_order_relaxed);
            if (channel.auxSendLevel[(size_t)target.index].load(std::memory_order_relaxed) <= 0.0f)
                channel.auxSendLevel[(size_t)target.index].store(1.0f, std::memory_order_relaxed);
        }
        return alreadyConnected ? RoutingGraphEditResult::AlreadyConnected
                                : RoutingGraphEditResult::Updated;
    }

    const bool toStream = target.type == RoutingGraphNodeType::Stream;
    if (source.type == RoutingGraphNodeType::Submix)
    {
        auto& route = toStream ? matrix.getSubmixBus(source.index).routeToStream
                               : matrix.getSubmixBus(source.index).routeToMonitor;
        const bool alreadyConnected = route.load(std::memory_order_relaxed);
        route.store(true, std::memory_order_relaxed);
        return alreadyConnected ? RoutingGraphEditResult::AlreadyConnected
                                : RoutingGraphEditResult::Updated;
    }

    auto& route = toStream ? matrix.getAuxBus(source.index).sendToStream
                           : matrix.getAuxBus(source.index).sendToMonitor;
    const bool alreadyConnected = route.load(std::memory_order_relaxed);
    route.store(true, std::memory_order_relaxed);
    return alreadyConnected ? RoutingGraphEditResult::AlreadyConnected
                            : RoutingGraphEditResult::Updated;
}

inline RoutingGraphEditResult connectRoutingGraphEndpoints(
    RoutingMatrix& matrix, RoutingGraphEndpoint source, RoutingGraphEndpoint target) noexcept
{
    return connectRoutingGraphConnection(matrix, makeRoutingGraphConnection(source, target));
}

inline RoutingGraphEditResult disconnectRoutingGraphConnection(
    RoutingMatrix& matrix, const RoutingGraphConnection& connection) noexcept
{
    if (validateRoutingGraphConnection(matrix, connection) != RoutingGraphValidationResult::Valid)
        return RoutingGraphEditResult::Unsupported;

    const auto source = RoutingGraphEndpoint{ connection.sourceNode.type, connection.sourceNode.index };
    const auto target = RoutingGraphEndpoint{ connection.destinationNode.type,
                                               connection.destinationNode.index };

    if (source.type == RoutingGraphNodeType::Channel)
    {
        auto& channel = matrix.getChannel(source.index);
        if (target.type == RoutingGraphNodeType::Submix)
        {
            const bool connected = channel.submixGroup.load(std::memory_order_relaxed) == target.index;
            if (!connected)
                return RoutingGraphEditResult::AlreadyConnected;
            channel.submixGroup.store(-1, std::memory_order_relaxed);
            channel.routeEnable[0].store(false, std::memory_order_relaxed);
            channel.routeEnable[1].store(false, std::memory_order_relaxed);
            return RoutingGraphEditResult::Updated;
        }

        if (target.type == RoutingGraphNodeType::Stream || target.type == RoutingGraphNodeType::Monitor)
        {
            const int bus = target.type == RoutingGraphNodeType::Stream ? 0 : 1;
            const bool connected = channel.submixGroup.load(std::memory_order_relaxed) < 0 &&
                channel.routeEnable[(size_t)bus].load(std::memory_order_relaxed);
            channel.routeEnable[(size_t)bus].store(false, std::memory_order_relaxed);
            return connected ? RoutingGraphEditResult::Updated : RoutingGraphEditResult::AlreadyConnected;
        }

        const bool connected = channel.auxRouteEnable[(size_t)target.index].load(std::memory_order_relaxed);
        channel.auxRouteEnable[(size_t)target.index].store(false, std::memory_order_relaxed);
        return connected ? RoutingGraphEditResult::Updated : RoutingGraphEditResult::AlreadyConnected;
    }

    const bool toStream = target.type == RoutingGraphNodeType::Stream;
    if (source.type == RoutingGraphNodeType::Submix)
    {
        auto& route = toStream ? matrix.getSubmixBus(source.index).routeToStream
                               : matrix.getSubmixBus(source.index).routeToMonitor;
        const bool connected = route.load(std::memory_order_relaxed);
        route.store(false, std::memory_order_relaxed);
        return connected ? RoutingGraphEditResult::Updated : RoutingGraphEditResult::AlreadyConnected;
    }

    auto& route = toStream ? matrix.getAuxBus(source.index).sendToStream
                           : matrix.getAuxBus(source.index).sendToMonitor;
    const bool connected = route.load(std::memory_order_relaxed);
    route.store(false, std::memory_order_relaxed);
    return connected ? RoutingGraphEditResult::Updated : RoutingGraphEditResult::AlreadyConnected;
}

inline RoutingGraphEditResult disconnectRoutingGraphEndpoints(
    RoutingMatrix& matrix, RoutingGraphEndpoint source, RoutingGraphEndpoint target) noexcept
{
    return disconnectRoutingGraphConnection(matrix, makeRoutingGraphConnection(source, target));
}

} // namespace LiveBus
