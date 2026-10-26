#include "RoutingGraphComponent.h"
#include "RoutingGraphRouting.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace LiveBus
{

class RoutingGraphCanvas final : public juce::Component, private juce::Timer
{
public:
    explicit RoutingGraphCanvas(RoutingMatrix& routingMatrix,
                                std::function<void(int)> onChannelSelected,
                                std::function<void(const juce::String&)> onConnectionStatus,
                                std::function<void(int)> onDSPNodeSelected,
                                std::function<void(int)> onVSTChainSelected,
                                RoutingGraphComponent::VSTChainPluginsProvider vstChainPluginsProvider)
        : matrix(routingMatrix), channelSelected(std::move(onChannelSelected)),
          connectionStatus(std::move(onConnectionStatus)),
          dspNodeSelected(std::move(onDSPNodeSelected)),
          vstChainSelected(std::move(onVSTChainSelected)),
          vstChainPluginsProvider(std::move(vstChainPluginsProvider))
    {
        syncNodes();
        startTimerHz(20);
        setWantsKeyboardFocus(true);
    }

    void setZoom(float newZoom)
    {
        zoom = juce::jlimit(0.45f, 1.5f, newZoom);
        updateSize();
        repaint();
    }

    float getZoom() const noexcept { return zoom; }
    bool isFocusedView() const noexcept { return focusedView; }

    void setPanHandler(std::function<void(int, int)> handler)
    {
        panHandler = std::move(handler);
    }

    void setFocusChangedHandler(std::function<void()> handler)
    {
        focusChangedHandler = std::move(handler);
    }

    void setMasterBusControlCallbacks(RoutingGraphComponent::MasterBusControlCallbacks callbacks)
    {
        masterBusControlCallbacks = std::move(callbacks);
        repaint();
    }

    void setDSPControlCallbacks(RoutingGraphComponent::DSPControlCallbacks callbacks)
    {
        dspControlCallbacks = std::move(callbacks);
        repaint();
    }

    void focusNode(const juce::String& typeId, int index, int currentX, int currentY,
                   int viewWidth, int viewHeight)
    {
        const auto type = nodeTypeFromId(typeId);
        const int nodeIndex = type.has_value() ? findNode(*type, index) : -1;
        if (nodeIndex < 0)
            return;
        if (!toggleFocusForNode(nodes[(size_t)nodeIndex]))
            return;
        selectedNode = nodeIndex;
        juce::ignoreUnused(currentX, currentY, viewWidth, viewHeight);
        if (focusChangedHandler)
            focusChangedHandler();
        repaint();
    }

    void showAllNodes()
    {
        if (!focusedView)
            return;
        restoreUnfocusedNodePositions();
        focusedView = false;
        focusedRootType = NodeType::Channel;
        focusedRootIndex = -1;
        focusedAnchorType = NodeType::Channel;
        focusedAnchorIndex = -1;
        focusedChannelIndices.clear();
        unfocusedNodePositions.clear();
        updateSize();
        repaint();
    }

    std::unique_ptr<juce::XmlElement> serializePositions() const
    {
        auto layout = std::make_unique<juce::XmlElement>("NODES");
        for (const auto& node : nodes)
        {
            auto position = node.bounds.getPosition();
            if (focusedView)
            {
                const auto saved = std::find_if(unfocusedNodePositions.begin(),
                                                unfocusedNodePositions.end(),
                    [&node](const SavedNodePosition& item)
                    {
                        return item.type == node.type && item.index == node.index;
                    });
                if (saved != unfocusedNodePositions.end())
                    position = saved->position;
            }
            auto* element = layout->createNewChildElement("NODE");
            element->setAttribute("type", nodeTypeId(node.type));
            element->setAttribute("index", node.index);
            element->setAttribute("targetIndex", node.targetIndex);
            element->setAttribute("slotIndex", node.slotIndex);
            element->setAttribute("x", (double)position.x);
            element->setAttribute("y", (double)position.y);
        }
        return layout;
    }

    std::unique_ptr<juce::XmlElement> serializeConnections() const
    {
        auto layout = std::make_unique<juce::XmlElement>("CONNECTIONS");
        for (const auto& connection : buildConnections(true))
        {
            auto* element = layout->createNewChildElement("CONNECTION");
            element->setAttribute("sourceType",
                                  routingGraphNodeTypeId(connection.model.sourceNode.type));
            element->setAttribute("sourceIndex", connection.model.sourceNode.index);
            element->setAttribute("sourcePort", (int)connection.model.sourcePort);
            element->setAttribute("destinationType",
                                  routingGraphNodeTypeId(connection.model.destinationNode.type));
            element->setAttribute("destinationIndex", connection.model.destinationNode.index);
            element->setAttribute("destinationPort", (int)connection.model.destinationPort);
            element->setAttribute("channelMapping", (int)connection.model.channelMapping);
        }
        return layout;
    }

    bool validateSerializedConnections(const juce::XmlElement& layout) const
    {
        if (!layout.hasTagName("CONNECTIONS"))
            return false;

        const auto activeConnections = buildConnections(true);
        std::vector<bool> matched(activeConnections.size(), false);
        size_t serializedCount = 0;
        for (auto* element : layout.getChildIterator())
        {
            if (!element->hasTagName("CONNECTION"))
                continue;

            const auto sourceType = nodeTypeFromId(element->getStringAttribute("sourceType"));
            const auto destinationType = nodeTypeFromId(element->getStringAttribute("destinationType"));
            if (!sourceType.has_value() || !destinationType.has_value())
                return false;
            if (*sourceType == NodeType::DSP || *sourceType == NodeType::Gate ||
                *sourceType == NodeType::Equalizer || *sourceType == NodeType::Compressor ||
                *sourceType == NodeType::VSTChain || *sourceType == NodeType::VSTPlugin ||
                *destinationType == NodeType::DSP || *destinationType == NodeType::Gate ||
                *destinationType == NodeType::Equalizer || *destinationType == NodeType::Compressor ||
                *destinationType == NodeType::VSTChain || *destinationType == NodeType::VSTPlugin)
                return false;

            RoutingGraphConnection connection{
                { toRoutingGraphNodeType(*sourceType), element->getIntAttribute("sourceIndex", -1) },
                static_cast<RoutingGraphPortId>(element->getIntAttribute("sourcePort", -1)),
                { toRoutingGraphNodeType(*destinationType), element->getIntAttribute("destinationIndex", -1) },
                static_cast<RoutingGraphPortId>(element->getIntAttribute("destinationPort", -1)),
                static_cast<RoutingGraphChannelMapping>(element->getIntAttribute("channelMapping", -1))
            };
            if (validateRoutingGraphConnection(matrix, connection) != RoutingGraphValidationResult::Valid)
                return false;

            const auto found = std::find_if(activeConnections.begin(), activeConnections.end(),
                [&connection](const Connection& active)
                {
                    return active.model == connection;
                });
            if (found == activeConnections.end())
                return false;

            const auto index = (size_t)std::distance(activeConnections.begin(), found);
            if (matched[index])
                return false;
            matched[index] = true;
            ++serializedCount;
        }

        return serializedCount == activeConnections.size();
    }

    bool restorePositions(const juce::XmlElement& layout)
    {
        if (!layout.hasTagName("NODES"))
            return false;

        nodes.clear();
        syncNodes();
        std::vector<bool> restored(nodes.size(), false);

        for (auto* element : layout.getChildIterator())
        {
            if (!element->hasTagName("NODE"))
                continue;

            const auto type = nodeTypeFromId(element->getStringAttribute("type"));
            const int index = element->getIntAttribute("index", -1);
            const int nodeIndex = type.has_value() ? findNode(*type, index) : -1;
            if (nodeIndex < 0)
                return false;
            if (restored[(size_t)nodeIndex] ||
                !element->hasAttribute("x") || !element->hasAttribute("y"))
                return false;
            if (*type == NodeType::VSTPlugin &&
                (element->getIntAttribute("targetIndex", -1) != nodes[(size_t)nodeIndex].targetIndex ||
                 element->getIntAttribute("slotIndex", -1) != nodes[(size_t)nodeIndex].slotIndex))
                return false;

            auto& bounds = nodes[(size_t)nodeIndex].bounds;
            const float x = (float)element->getDoubleAttribute("x", bounds.getX());
            const float y = (float)element->getDoubleAttribute("y", bounds.getY());
            if (!std::isfinite(x) || !std::isfinite(y))
                return false;
            bounds.setPosition(clampNodePosition(nodes[(size_t)nodeIndex], { x, y }));
            restored[(size_t)nodeIndex] = true;
        }

        updateSize();
        repaint();
        return true;
    }

    void resetPositions()
    {
        showAllNodes();
        nodes.clear();
        syncNodes();
    }

    void fitTo(float viewWidth, float viewHeight)
    {
        syncNodes();
        float width = graphWidth;
        float height = graphHeight;
        if (focusedView)
        {
            float right = 0.0f;
            float bottom = 0.0f;
            for (const auto& node : nodes)
            {
                if (!isNodeVisible(node))
                    continue;
                right = juce::jmax(right, node.bounds.getRight());
                bottom = juce::jmax(bottom, node.bounds.getBottom());
            }
            width = juce::jmax(320.0f, right + 48.0f);
            height = juce::jmax(180.0f, bottom + 48.0f);
        }
        const float widthScale = viewWidth / juce::jmax(1.0f, width);
        const float heightScale = viewHeight / juce::jmax(1.0f, height);
        setZoom(juce::jlimit(0.35f, 1.0f, juce::jmin(widthScale, heightScale)));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0b1018));
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(zoom));

        g.setColour(juce::Colour(0xff182230));
        const float visibleWidth = (float)getWidth() / zoom;
        const float visibleHeight = (float)getHeight() / zoom;
        for (int x = 0; x <= visibleWidth; x += 32)
            g.drawVerticalLine(x, 0.0f, visibleHeight);
        for (int y = 0; y <= visibleHeight; y += 32)
            g.drawHorizontalLine(y, 0.0f, visibleWidth);

        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        if (focusedView)
            g.drawText("FOCUSED AUDIO ROUTE", 30, 14, 260, 18,
                       juce::Justification::centredLeft);
        else
        {
            g.drawText("INPUTS", 30, 14, 220, 18, juce::Justification::centredLeft);
            g.drawText("DSP CORE", 300, 14, 220, 18, juce::Justification::centredLeft);
            g.drawText("GATE", 560, 14, 150, 18, juce::Justification::centredLeft);
            g.drawText("EQ CURVE", 730, 14, 150, 18, juce::Justification::centredLeft);
            g.drawText("COMPRESSOR", 900, 14, 150, 18, juce::Justification::centredLeft);
            g.drawText("VST3 INSERT CHAINS", 1080, 14, 240, 18, juce::Justification::centredLeft);
            g.drawText("BUSES / RETURNS", (int)submixX, 14, 220, 18, juce::Justification::centredLeft);
            g.drawText("BUS DSP / VST3", (int)busDSPX, 14, 300, 18, juce::Justification::centredLeft);
            g.drawText("BUS OUTPUTS", (int)busOutputX, 14, 140, 18, juce::Justification::centredLeft);
        }

        drawProcessingLinks(g);
        const auto connections = buildConnections();
        for (const auto& connection : connections)
            drawConnection(g, connection, true);
        if (draggingConnection)
            drawTemporaryConnection(g);

        for (const auto& node : nodes)
            if (isNodeVisible(node))
                drawNode(g, node);

        g.restoreState();
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        const auto point = event.position / zoom;
        if (event.mods.isMiddleButtonDown())
        {
            panning = true;
            lastMousePosition = event.getScreenPosition();
            return;
        }

        if (event.mods.isRightButtonDown())
        {
            disconnectNearest(point);
            return;
        }

        if (!event.mods.isLeftButtonDown())
            return;

        suppressNodeSelection = false;
        pressedNodeIdentity.reset();
        const int outputNode = findNodeAtOutputPort(point);
        if (outputNode >= 0)
        {
            selectedNode = -1;
            draggingConnection = true;
            connectionSource = outputNode;
            connectionEnd = point;
            repaint();
            return;
        }

        const int nodeIndex = findNodeAt(point);
        if (nodeIndex < 0)
        {
            selectedNode = -1;
            repaint();
            return;
        }

        selectedNode = nodeIndex;
        const auto& node = nodes[(size_t)nodeIndex];
        pressedNodeIdentity = NodeIdentity{ node.type, node.index,
                                            node.targetIndex, node.slotIndex };
        nodeDragStartPosition = point;
        nodeDragMoved = false;
        if (beginNodeControlDrag(nodeIndex, point) ||
            handleNodeButtonClick(nodeIndex, point))
        {
            suppressNodeSelection = true;
            repaint();
            return;
        }
        draggingNode = true;
        dragOffset = point - node.bounds.getPosition();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (panning)
        {
            const auto current = event.getScreenPosition();
            const auto delta = current - lastMousePosition;
            lastMousePosition = current;
            if (panHandler)
                panHandler(delta.x, delta.y);
            return;
        }

        const auto point = event.position / zoom;
        if (draggingControl)
        {
            updateNodeControl(point);
            repaint();
        }
        else if (draggingConnection)
        {
            connectionEnd = point;
            repaint();
        }
        else if (draggingNode && selectedNode >= 0)
        {
            if (point.getDistanceFrom(nodeDragStartPosition) > 4.0f)
                nodeDragMoved = true;
            auto& node = nodes[(size_t)selectedNode];
            node.bounds.setPosition(clampNodePosition(node, point - dragOffset));
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        const bool nodeWasDragged = draggingNode && nodeDragMoved;
        const auto pressedIdentity = pressedNodeIdentity;
        pressedNodeIdentity.reset();
        panning = false;
        draggingNode = false;
        draggingControl = false;
        controlMode = NodeControlMode::None;
        nodeDragMoved = false;

        if (draggingConnection)
        {
            const auto point = event.position / zoom;
            const int targetNode = findNodeAtInputPort(point);
            if (targetNode >= 0)
            {
                const auto& source = nodes[(size_t)connectionSource];
                const auto& target = nodes[(size_t)targetNode];
                const auto connection = makeRoutingGraphConnection(endpointFor(source), endpointFor(target));
                const auto validation = validateRoutingGraphConnection(matrix, connection);
                if (validation != RoutingGraphValidationResult::Valid)
                    reportConnectionStatus("Connection rejected: " + validationMessage(validation));
                else
                    connectNodes(connection);
            }
            else
                reportConnectionStatus("Connection cancelled: drop the cable on a compatible input port.");
            draggingConnection = false;
            connectionSource = -1;
            repaint();
            return;
        }

        if (suppressNodeSelection)
        {
            suppressNodeSelection = false;
            repaint();
            return;
        }

        if (nodeWasDragged)
        {
            repaint();
            return;
        }

        if (!pressedIdentity.has_value())
            return;

        const int releasedNode = findNodeAt(event.position / zoom);
        if (releasedNode < 0 ||
            findNode(*pressedIdentity) != releasedNode)
            return;

        selectedNode = releasedNode;
        const auto node = nodes[(size_t)releasedNode];
        if (toggleFocusForNode(node) && focusChangedHandler)
            focusChangedHandler();
        if (node.type == NodeType::Channel && channelSelected)
            channelSelected(node.index);
        else if (node.type == NodeType::DSP && dspNodeSelected)
            dspNodeSelected(node.index);
        else if ((node.type == NodeType::Gate || node.type == NodeType::Equalizer ||
                  node.type == NodeType::Compressor) && dspNodeSelected)
            dspNodeSelected(node.index);
        else if (node.type == NodeType::VSTChain && vstChainSelected)
            vstChainSelected(node.index);
        else if (node.type == NodeType::VSTPlugin && vstChainSelected)
            vstChainSelected(node.targetIndex);
    }

    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override
    {
        if (event.mods.isCtrlDown())
        {
            setZoom(zoom * (wheel.deltaY > 0.0f ? 1.1f : 0.9f));
            return;
        }
        juce::Component::mouseWheelMove(event, wheel);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
        {
            disconnectSelectedNodeConnections();
            return true;
        }
        return false;
    }

private:
    enum class NodeType
    {
        Channel, DSP, Gate, Equalizer, Compressor, VSTChain, VSTPlugin,
        Submix, Aux, Stream, Monitor
    };
    enum class NodeControlMode
    {
        None, ChannelGain, ChannelPan, ChannelStreamSend, ChannelMonitorSend,
        ChannelAux0, ChannelAux1, ChannelAux2, ChannelAux3,
        ChannelHardware0, ChannelHardware1, ChannelHardware2, ChannelHardware3,
        ChannelHardware4, ChannelHardware5, ChannelHardware6, ChannelHardware7,
        SubmixGain, SubmixPan, SubmixStreamSend, SubmixMonitorSend,
        AuxReturn, AuxPan, AuxStreamSend, AuxMonitorSend, AuxDelayTime, AuxDelayFeedback,
        MasterGain, DSPParameter0, DSPParameter1, DSPParameter2, DSPParameter3,
        DSPParameter4, DSPParameter5, DSPParameter6, DSPParameter7
    };

    struct Node
    {
        NodeType type = NodeType::Channel;
        int index = 0;
        int targetIndex = -1;
        int slotIndex = -1;
        juce::String title;
        juce::String subtitle;
        juce::Rectangle<float> bounds;
        juce::Colour colour;
        bool hasInput = true;
        bool hasOutput = true;
    };

    struct NodeIdentity
    {
        NodeType type = NodeType::Channel;
        int index = 0;
        int targetIndex = -1;
        int slotIndex = -1;
    };

    struct Connection
    {
        int sourceNode = -1;
        int targetNode = -1;
        juce::Point<float> start;
        juce::Point<float> end;
        juce::Colour colour;
        bool active = false;
        RoutingGraphConnection model;
    };

    struct SavedNodePosition
    {
        NodeType type = NodeType::Channel;
        int index = 0;
        juce::Point<float> position;
    };

    bool channelFeedsBus(int channelIndex, int bus) const
    {
        if (channelIndex < 0 || channelIndex >= matrix.getNumActiveChannels() ||
            bus < 0 || bus >= numBuses)
            return false;
        const auto& channel = matrix.getChannel(channelIndex);
        const int submix = channel.submixGroup.load(std::memory_order_relaxed);
        bool feedsBus = false;
        if (submix >= 0 && submix < maxSubmixBuses)
        {
            const auto& state = matrix.getSubmixBus(submix);
            feedsBus = bus == 0 ? state.routeToStream.load(std::memory_order_relaxed)
                                : state.routeToMonitor.load(std::memory_order_relaxed);
        }
        else
        {
            feedsBus = channel.routeEnable[(size_t)bus].load(std::memory_order_relaxed);
        }

        for (int aux = 0; aux < maxAuxBuses; ++aux)
        {
            if (!channel.auxRouteEnable[(size_t)aux].load(std::memory_order_relaxed))
                continue;
            const auto& auxState = matrix.getAuxBus(aux);
            feedsBus = feedsBus || (bus == 0
                ? auxState.sendToStream.load(std::memory_order_relaxed)
                : auxState.sendToMonitor.load(std::memory_order_relaxed));
        }
        return feedsBus;
    }

    bool channelFeedsAux(int channelIndex, int auxIndex) const
    {
        return matrix.getChannel(channelIndex).auxRouteEnable[(size_t)auxIndex]
            .load(std::memory_order_relaxed);
    }

    bool channelFeedsSubmix(int channelIndex, int submixIndex) const
    {
        return matrix.getChannel(channelIndex).submixGroup.load(std::memory_order_relaxed)
            == submixIndex;
    }

    bool channelIsInFocusedRoute(int channelIndex) const
    {
        if (!focusedView)
            return true;
        switch (focusedRootType)
        {
            case NodeType::Channel:
                return std::find(focusedChannelIndices.begin(), focusedChannelIndices.end(),
                                 channelIndex) != focusedChannelIndices.end();
            case NodeType::Submix:
                return channelFeedsSubmix(channelIndex, focusedRootIndex);
            case NodeType::Aux:
                return focusedRootIndex >= 0 && focusedRootIndex < maxAuxBuses &&
                       channelFeedsAux(channelIndex, focusedRootIndex);
            case NodeType::Stream:
                return channelFeedsBus(channelIndex, 0);
            case NodeType::Monitor:
                return channelFeedsBus(channelIndex, 1);
            default:
                return false;
        }
    }

    bool busIsInFocusedRoute(int bus) const
    {
        switch (focusedRootType)
        {
            case NodeType::Channel:
                return std::any_of(focusedChannelIndices.begin(), focusedChannelIndices.end(),
                    [this, bus](int channelIndex)
                    {
                        return channelFeedsBus(channelIndex, bus);
                    });
            case NodeType::Submix:
            {
                if (focusedRootIndex < 0 || focusedRootIndex >= maxSubmixBuses)
                    return false;
                const auto& submix = matrix.getSubmixBus(focusedRootIndex);
                return bus == 0 ? submix.routeToStream.load(std::memory_order_relaxed)
                                : submix.routeToMonitor.load(std::memory_order_relaxed);
            }
            case NodeType::Aux:
            {
                if (focusedRootIndex < 0 || focusedRootIndex >= maxAuxBuses)
                    return false;
                const auto& aux = matrix.getAuxBus(focusedRootIndex);
                return bus == 0 ? aux.sendToStream.load(std::memory_order_relaxed)
                                : aux.sendToMonitor.load(std::memory_order_relaxed);
            }
            case NodeType::Stream: return bus == 0;
            case NodeType::Monitor: return bus == 1;
            default: return false;
        }
    }

    bool isNodeVisible(const Node& node) const
    {
        if (!focusedView)
            return true;

        if (node.type == focusedAnchorType && node.index == focusedAnchorIndex)
            return true;

        const int nodeTarget = node.type == NodeType::VSTPlugin ? node.targetIndex
            : node.targetIndex >= 0 ? node.targetIndex : node.index;
        if (focusedRootType == NodeType::Channel && nodeTarget == focusedRootIndex &&
            node.type != NodeType::Channel)
            return true;

        if (node.type == NodeType::Channel)
            return focusedRootType == NodeType::Channel ||
                   channelIsInFocusedRoute(node.index);
        if (node.type == NodeType::Submix)
        {
            if (focusedRootType == NodeType::Submix && node.index == focusedRootIndex)
                return true;
            for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
                if (channelIsInFocusedRoute(channel) && channelFeedsSubmix(channel, node.index))
                    return true;
            return false;
        }
        if (node.type == NodeType::Aux)
        {
            if (focusedRootType == NodeType::Aux && node.index == focusedRootIndex)
                return true;
            for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
                if (channelIsInFocusedRoute(channel) && channelFeedsAux(channel, node.index))
                    return true;
            return false;
        }
        if (node.type == NodeType::Stream || node.type == NodeType::Monitor)
            return busIsInFocusedRoute(node.type == NodeType::Stream ? 0 : 1);

        const int target = node.type == NodeType::VSTPlugin ? node.targetIndex
                                                            : node.targetIndex >= 0
                                                                ? node.targetIndex : node.index;
        if (target < maxInputs)
            return target >= 0 && channelIsInFocusedRoute(target);
        return target <= maxInputs + 1 && busIsInFocusedRoute(target - maxInputs);
    }

    void captureUnfocusedNodePositions()
    {
        unfocusedNodePositions.clear();
        unfocusedNodePositions.reserve(nodes.size());
        for (const auto& node : nodes)
            unfocusedNodePositions.push_back({ node.type, node.index, node.bounds.getPosition() });
    }

    void restoreUnfocusedNodePositions()
    {
        if (!focusedView)
            return;
        for (auto& node : nodes)
        {
            const auto saved = std::find_if(unfocusedNodePositions.begin(),
                                            unfocusedNodePositions.end(),
                [&node](const SavedNodePosition& position)
                {
                    return position.type == node.type && position.index == node.index;
                });
            if (saved != unfocusedNodePositions.end())
                node.bounds.setPosition(saved->position);
        }
    }

    void setFocusForNode(const Node& node)
    {
        restoreUnfocusedNodePositions();
        focusedChannelIndices.clear();
        if (node.type == NodeType::Channel || node.type == NodeType::Submix ||
            node.type == NodeType::Aux || node.type == NodeType::Stream ||
            node.type == NodeType::Monitor)
        {
            focusedRootType = node.type;
            focusedRootIndex = node.index;
            if (node.type == NodeType::Channel)
                focusedChannelIndices.push_back(node.index);
        }
        else
        {
            const int target = node.type == NodeType::VSTPlugin ? node.targetIndex
                                                                : node.targetIndex >= 0
                                                                    ? node.targetIndex : node.index;
            focusedRootType = target < maxInputs ? NodeType::Channel
                : target == maxInputs ? NodeType::Stream : NodeType::Monitor;
            focusedRootIndex = target < maxInputs ? target : 0;
        }
        focusedView = true;
        focusedAnchorType = node.type;
        focusedAnchorIndex = node.index;
        captureUnfocusedNodePositions();
        updateFocusedLayout();
    }

    bool toggleFocusForNode(const Node& node)
    {
        if (node.type == NodeType::Channel)
        {
            if (!focusedView || focusedRootType != NodeType::Channel)
            {
                setFocusForNode(node);
            }
            else
            {
                const auto selected = std::find(focusedChannelIndices.begin(),
                                                focusedChannelIndices.end(), node.index);
                if (selected == focusedChannelIndices.end())
                    focusedChannelIndices.push_back(node.index);
                else
                    focusedChannelIndices.erase(selected);

                if (focusedChannelIndices.empty())
                {
                    showAllNodes();
                    if (focusChangedHandler)
                        focusChangedHandler();
                    return false;
                }

                std::sort(focusedChannelIndices.begin(), focusedChannelIndices.end());
                focusedRootIndex = focusedChannelIndices.front();
                focusedAnchorType = NodeType::Channel;
                focusedAnchorIndex = node.index;
                restoreUnfocusedNodePositions();
                captureUnfocusedNodePositions();
                updateFocusedLayout();
            }
            updateSize();
            return true;
        }

        if (focusedView && focusedAnchorType == node.type &&
            focusedAnchorIndex == node.index)
        {
            showAllNodes();
            if (focusChangedHandler)
                focusChangedHandler();
            return false;
        }

        setFocusForNode(node);
        updateSize();
        return true;
    }

    void updateFocusedLayout()
    {
        if (!focusedView)
            return;

        const float sourceX[] = { 30.0f, 300.0f, 560.0f, 730.0f, 900.0f, 1080.0f };
        const float routeX = juce::jmax(submixX + 260.0f, 1360.0f);
        const float busDspX = routeX + 240.0f;
        const float busGateX = busDspX + 240.0f;
        const float busEqX = busGateX + 170.0f;
        const float busCompX = busEqX + 170.0f;
        const float busVstX = busCompX + 170.0f;
        const float outputX = busVstX + 300.0f;
        const float routeRow = 62.0f;

        int sourceRow = 0;
        int otherChannelRow = 0;
        const bool stackFocusedChannels =
            focusedRootType != NodeType::Channel || focusedChannelIndices.size() > 1;
        for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
        {
            if (!channelIsInFocusedRoute(channel))
                continue;

            float y = routeRow;
            if (stackFocusedChannels)
                y += sourceRow++ * 210.0f;
            else
                ++sourceRow;
            for (auto& node : nodes)
            {
                if (node.type == NodeType::Channel && node.index == channel)
                    node.bounds.setPosition(sourceX[0], y);
                else if ((node.type == NodeType::DSP || node.type == NodeType::Gate ||
                          node.type == NodeType::Equalizer || node.type == NodeType::Compressor ||
                          node.type == NodeType::VSTChain || node.type == NodeType::VSTPlugin) &&
                         (node.type == NodeType::VSTPlugin ? node.targetIndex
                             : node.targetIndex >= 0 ? node.targetIndex : node.index) == channel)
                {
                    float x = sourceX[1];
                    if (node.type == NodeType::Gate) x = sourceX[2];
                    else if (node.type == NodeType::Equalizer) x = sourceX[3];
                    else if (node.type == NodeType::Compressor) x = sourceX[4];
                    else if (node.type == NodeType::VSTChain || node.type == NodeType::VSTPlugin)
                        x = sourceX[5] + juce::jmax(0, node.slotIndex) * 230.0f;
                    node.bounds.setPosition(x, y + (node.type == NodeType::Gate ||
                        node.type == NodeType::Equalizer || node.type == NodeType::Compressor
                            ? 14.0f : 0.0f));
                }
            }
        }
        if (focusedRootType == NodeType::Channel)
        {
            for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
            {
                if (channelIsInFocusedRoute(channel))
                    continue;

                const int channelNode = findNode(NodeType::Channel, channel);
                if (channelNode >= 0)
                    nodes[(size_t)channelNode].bounds.setPosition(
                        sourceX[0], routeRow + (sourceRow + otherChannelRow++) * 210.0f);
            }
        }

        for (int submix = 0; submix < maxSubmixBuses; ++submix)
            for (auto& node : nodes)
                if (node.type == NodeType::Submix && node.index == submix && isNodeVisible(node))
                    node.bounds.setPosition(routeX, routeRow + submix * 170.0f);

        for (int aux = 0; aux < maxAuxBuses; ++aux)
            for (auto& node : nodes)
                if (node.type == NodeType::Aux && node.index == aux && isNodeVisible(node))
                    node.bounds.setPosition(routeX, routeRow + 50.0f + aux * 170.0f);

        for (int bus = 0; bus < numBuses; ++bus)
        {
            if (!busIsInFocusedRoute(bus))
                continue;
            const int target = maxInputs + bus;
            const float y = routeRow + bus * 190.0f;
            for (auto& node : nodes)
            {
                if (!isNodeVisible(node))
                    continue;
                const int nodeTarget = node.type == NodeType::VSTPlugin ? node.targetIndex
                    : node.targetIndex >= 0 ? node.targetIndex : node.index;
                if (nodeTarget != target)
                    continue;
                float x = busDspX;
                if (node.type == NodeType::Gate) x = busGateX;
                else if (node.type == NodeType::Equalizer) x = busEqX;
                else if (node.type == NodeType::Compressor) x = busCompX;
                else if (node.type == NodeType::VSTChain || node.type == NodeType::VSTPlugin)
                    x = busVstX + juce::jmax(0, node.slotIndex) * 230.0f;
                node.bounds.setPosition(x, y + (node.type == NodeType::Gate ||
                    node.type == NodeType::Equalizer || node.type == NodeType::Compressor
                        ? 14.0f : 0.0f));
            }
            const auto outputType = bus == 0 ? NodeType::Stream : NodeType::Monitor;
            const int output = findNode(outputType, 0);
            if (output >= 0)
                nodes[(size_t)output].bounds.setPosition(outputX, y);
        }

    }

    float graphWidth = 1780.0f;
    static constexpr float channelRowHeight = 212.0f;

    static int pluginNodeKey(int targetIndex, int slotIndex) noexcept
    {
        return targetIndex * 1000 + slotIndex;
    }

    void timerCallback() override
    {
        syncNodes();
        repaint();
    }

    void updateSize()
    {
        float width = graphWidth;
        float height = graphHeight;
        if (focusedView)
        {
            width = 0.0f;
            height = 0.0f;
            for (const auto& node : nodes)
            {
                if (!isNodeVisible(node))
                    continue;
                width = juce::jmax(width, node.bounds.getRight() + 48.0f);
                height = juce::jmax(height, node.bounds.getBottom() + 48.0f);
            }
            width = juce::jmax(width, 320.0f);
            height = juce::jmax(height, 180.0f);
        }
        setSize(juce::roundToInt(width * zoom),
                juce::roundToInt(height * zoom));
    }

    void syncNodes()
    {
        restoreUnfocusedNodePositions();
        const int activeChannels = matrix.getNumActiveChannels();
        std::array<std::vector<juce::String>, maxInputs + 2> pluginNames;
        int processingNodeCount = 0;
        int maxChannelPluginCount = 0;
        int maxBusPluginCount = 0;
        for (int ordinal = 0; ordinal < activeChannels + 2; ++ordinal)
        {
            const int target = ordinal < activeChannels
                ? ordinal : maxInputs + ordinal - activeChannels;
            if (vstChainPluginsProvider)
                pluginNames[(size_t)target] = vstChainPluginsProvider(target);
            const int count = (int)pluginNames[(size_t)target].size();
            processingNodeCount += juce::jmax(1, count);
            if (target < maxInputs)
                maxChannelPluginCount = juce::jmax(maxChannelPluginCount, count);
            else
                maxBusPluginCount = juce::jmax(maxBusPluginCount, count);
        }
        submixX = 1040.0f + juce::jmax(0, maxChannelPluginCount - 1) * 230.0f + 260.0f;
        busDSPX = submixX + 270.0f;
        busVSTX = busDSPX + 260.0f;
        busOutputX = busVSTX + juce::jmax(0, maxBusPluginCount - 1) * 230.0f + 260.0f;
        graphWidth = juce::jmax(1780.0f, busOutputX + 140.0f);
        const int expectedCount = activeChannels + (activeChannels + 2) * 4 +
                                  processingNodeCount + maxSubmixBuses + maxAuxBuses + 2;
        bool desiredProcessingNodesPresent = true;
        for (int ordinal = 0; ordinal < activeChannels + 2; ++ordinal)
        {
            const int target = ordinal < activeChannels
                ? ordinal : maxInputs + ordinal - activeChannels;
            const auto& plugins = pluginNames[(size_t)target];
            if (findNode(NodeType::DSP, target) < 0)
                desiredProcessingNodesPresent = false;
            desiredProcessingNodesPresent = desiredProcessingNodesPresent &&
                findNode(NodeType::Gate, target) >= 0 &&
                findNode(NodeType::Equalizer, target) >= 0 &&
                findNode(NodeType::Compressor, target) >= 0;
            if (plugins.empty())
                desiredProcessingNodesPresent = desiredProcessingNodesPresent &&
                    findNode(NodeType::VSTChain, target) >= 0;
            else
            {
                for (int slot = 0; slot < (int)plugins.size(); ++slot)
                    desiredProcessingNodesPresent = desiredProcessingNodesPresent &&
                        findNode(NodeType::VSTPlugin, pluginNodeKey(target, slot)) >= 0;
            }
        }
        if ((int)nodes.size() == expectedCount && desiredProcessingNodesPresent)
        {
            for (int channel = 0; channel < activeChannels; ++channel)
            {
                const int channelNode = findNode(NodeType::Channel, channel);
                if (channelNode >= 0)
                {
                    nodes[(size_t)channelNode].title = matrix.getChannel(channel).name;
                    nodes[(size_t)channelNode].subtitle = sourceName(matrix.getChannel(channel));
                }
            }
            for (int submix = 0; submix < maxSubmixBuses; ++submix)
            {
                const int node = findNode(NodeType::Submix, submix);
                if (node >= 0)
                    nodes[(size_t)node].title = matrix.getSubmixBus(submix).name;
            }
            for (int aux = 0; aux < maxAuxBuses; ++aux)
            {
                const int node = findNode(NodeType::Aux, aux);
                if (node >= 0)
                {
                    nodes[(size_t)node].title = matrix.getAuxBus(aux).name;
                    nodes[(size_t)node].subtitle =
                        auxFxName(matrix.getAuxBus(aux).fxType.load(std::memory_order_relaxed));
                }
            }
            for (int ordinal = 0; ordinal < activeChannels + 2; ++ordinal)
            {
                const int target = ordinal < activeChannels
                    ? ordinal : maxInputs + ordinal - activeChannels;
                const bool channelTarget = target < activeChannels;
                for (const auto type : { NodeType::Gate, NodeType::Equalizer,
                                          NodeType::Compressor })
                {
                    const int moduleNode = findNode(type, target);
                    if (moduleNode >= 0)
                    {
                        const juce::String moduleName = type == NodeType::Gate ? "Gate"
                            : type == NodeType::Equalizer ? "EQ Curve" : "Compressor";
                        nodes[(size_t)moduleNode].title = moduleName + " • " +
                            (channelTarget ? matrix.getChannel(target).name
                                : target == maxInputs ? "Stream" : "Monitor");
                    }
                }
                const int dspNode = findNode(NodeType::DSP, target);
                if (dspNode >= 0)
                {
                    nodes[(size_t)dspNode].title = channelTarget
                        ? "DSP • " + matrix.getChannel(target).name
                        : target == maxInputs ? "Stream Bus DSP" : "Monitor Bus DSP";
                    nodes[(size_t)dspNode].subtitle = channelTarget
                        ? "Modules + parameters • click header for full DSP"
                        : target == maxInputs ? "Stream DSP • click header for full controls"
                                              : "Monitor DSP • click header for full controls";
                }
                const auto& plugins = pluginNames[(size_t)target];
                if (plugins.empty())
                {
                    const int chainNode = findNode(NodeType::VSTChain, target);
                    if (chainNode >= 0)
                    {
                        nodes[(size_t)chainNode].title = channelTarget
                            ? "VST3 • " + matrix.getChannel(target).name
                            : target == maxInputs ? "VST3 • Stream" : "VST3 • Monitor";
                        nodes[(size_t)chainNode].subtitle = "Empty insert • click to manage";
                    }
                }
                else
                {
                    for (int slot = 0; slot < (int)plugins.size(); ++slot)
                    {
                        const int pluginNode = findNode(
                            NodeType::VSTPlugin, pluginNodeKey(target, slot));
                        if (pluginNode >= 0)
                        {
                            nodes[(size_t)pluginNode].title = plugins[(size_t)slot];
                            nodes[(size_t)pluginNode].subtitle =
                                "VST3 insert " + juce::String(slot + 1) + " / " +
                                juce::String(plugins.size()) + " • click to manage";
                        }
                    }
                }
            }
            if (focusedView)
                captureUnfocusedNodePositions();
            updateFocusedLayout();
            updateSize();
            return;
        }

        std::vector<Node> updated;
        updated.reserve((size_t)expectedCount);

        const auto preservePosition = [this](Node& node)
        {
            const auto found = std::find_if(nodes.begin(), nodes.end(), [&node](const Node& oldNode)
            {
                return oldNode.type == node.type && oldNode.index == node.index;
            });
            if (found != nodes.end())
                node.bounds.setPosition(found->bounds.getPosition());
        };

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            const auto& state = matrix.getChannel(channel);
            Node node;
            node.type = NodeType::Channel;
            node.index = channel;
            node.title = state.name;
            node.subtitle = sourceName(state);
            node.bounds = { 30.0f, 42.0f + channel * channelRowHeight, 230.0f, 202.0f };
            node.colour = juce::Colour(0xff10b981);
            node.hasInput = false;
            node.hasOutput = true;
            preservePosition(node);
            updated.push_back(std::move(node));
        }

        for (int ordinal = 0; ordinal < activeChannels + 2; ++ordinal)
        {
            const int target = ordinal < activeChannels
                ? ordinal : maxInputs + ordinal - activeChannels;
            const bool channelTarget = target < activeChannels;
            const float y = channelTarget
                ? 42.0f + target * channelRowHeight
                : target == maxInputs ? 178.0f : 330.0f;
            Node dspNode;
            dspNode.type = NodeType::DSP;
            dspNode.index = target;
            dspNode.title = channelTarget
                ? "DSP • " + matrix.getChannel(target).name
                : target == maxInputs ? "Stream Bus DSP" : "Monitor Bus DSP";
            dspNode.subtitle = channelTarget
                ? "Modules + parameters • click header for full DSP"
                : target == maxInputs ? "Stream DSP • click header for full controls"
                                      : "Monitor DSP • click header for full controls";
            dspNode.bounds = { channelTarget ? 300.0f : busDSPX, y, 240.0f, 198.0f };
            dspNode.colour = juce::Colour(0xff38bdf8);
            dspNode.hasInput = !channelTarget;
            dspNode.hasOutput = false;
            preservePosition(dspNode);
            updated.push_back(std::move(dspNode));

            const std::array<NodeType, 3> moduleTypes{
                NodeType::Gate, NodeType::Equalizer, NodeType::Compressor
            };
            const std::array<juce::String, 3> moduleTitles{
                "Gate", "EQ Curve", "Compressor"
            };
            const std::array<juce::Colour, 3> moduleColours{
                juce::Colour(0xfff59e0b), juce::Colour(0xffa78bfa),
                juce::Colour(0xfffb923c)
            };
            const std::array<float, 3> moduleX{ 560.0f, 730.0f, 900.0f };
            for (size_t module = 0; module < moduleTypes.size(); ++module)
            {
                Node moduleNode;
                moduleNode.type = moduleTypes[module];
                moduleNode.index = target;
                moduleNode.targetIndex = target;
                moduleNode.title = moduleTitles[module] + " • " +
                    (channelTarget ? matrix.getChannel(target).name
                                   : target == maxInputs ? "Stream" : "Monitor");
                moduleNode.subtitle = channelTarget ? "Channel processing control"
                    : "Bus processing control";
                moduleNode.bounds = { moduleX[module], y + 14.0f, 150.0f, 118.0f };
                moduleNode.colour = moduleColours[module];
                moduleNode.hasInput = false;
                moduleNode.hasOutput = false;
                preservePosition(moduleNode);
                updated.push_back(std::move(moduleNode));
            }

            const auto& plugins = pluginNames[(size_t)target];
            if (plugins.empty())
            {
                Node vstNode;
                vstNode.type = NodeType::VSTChain;
                vstNode.index = target;
                vstNode.title = channelTarget
                    ? "VST3 • " + matrix.getChannel(target).name
                    : target == maxInputs ? "VST3 • Stream" : "VST3 • Monitor";
                vstNode.subtitle = "Empty insert • click to manage";
                vstNode.bounds = { channelTarget ? 1080.0f : busVSTX,
                                   y, 210.0f, 52.0f };
                vstNode.colour = juce::Colour(0xfff472b6);
                vstNode.hasInput = false;
                vstNode.hasOutput = channelTarget;
                vstNode.targetIndex = target;
                preservePosition(vstNode);
                updated.push_back(std::move(vstNode));
            }
            else
            {
                for (int slot = 0; slot < (int)plugins.size(); ++slot)
                {
                    Node vstNode;
                    vstNode.type = NodeType::VSTPlugin;
                    vstNode.index = pluginNodeKey(target, slot);
                    vstNode.targetIndex = target;
                    vstNode.slotIndex = slot;
                    vstNode.title = plugins[(size_t)slot];
                    vstNode.subtitle = "VST3 insert " + juce::String(slot + 1) + " / " +
                                       juce::String(plugins.size()) + " • click to manage";
                    const float baseX = channelTarget ? 1080.0f : busVSTX;
                    vstNode.bounds = { baseX + slot * 230.0f, y, 210.0f, 52.0f };
                    vstNode.colour = juce::Colour(0xfff472b6);
                    vstNode.hasInput = false;
                    vstNode.hasOutput = channelTarget && slot == (int)plugins.size() - 1;
                    preservePosition(vstNode);
                    updated.push_back(std::move(vstNode));
                }
            }
        }

        for (int submix = 0; submix < maxSubmixBuses; ++submix)
        {
            Node node;
            node.type = NodeType::Submix;
            node.index = submix;
            node.title = matrix.getSubmixBus(submix).name;
            node.subtitle = "Group bus";
            node.bounds = { submixX, 50.0f + submix * 162.0f, 210.0f, 152.0f };
            node.colour = juce::Colour(0xfff59e0b);
            preservePosition(node);
            updated.push_back(std::move(node));
        }

        for (int aux = 0; aux < maxAuxBuses; ++aux)
        {
            Node node;
            node.type = NodeType::Aux;
            node.index = aux;
            node.title = matrix.getAuxBus(aux).name;
            node.subtitle = auxFxName(matrix.getAuxBus(aux).fxType.load());
            node.bounds = { submixX, 720.0f + aux * 202.0f, 210.0f, 192.0f };
            node.colour = juce::Colour(0xffa78bfa);
            preservePosition(node);
            updated.push_back(std::move(node));
        }

        Node stream;
        stream.type = NodeType::Stream;
        stream.index = 0;
        stream.title = "Stream Bus";
        stream.subtitle = "OBS / Streaming";
        stream.bounds = { busOutputX, 178.0f, 170.0f, 88.0f };
        stream.colour = juce::Colour(0xffc084fc);
        stream.hasInput = true;
        stream.hasOutput = false;
        preservePosition(stream);
        updated.push_back(std::move(stream));

        Node monitor;
        monitor.type = NodeType::Monitor;
        monitor.index = 0;
        monitor.title = "Monitor Bus";
        monitor.subtitle = "Headphones / Control room";
        monitor.bounds = { busOutputX, 330.0f, 170.0f, 88.0f };
        monitor.colour = juce::Colour(0xff22d3ee);
        monitor.hasInput = true;
        monitor.hasOutput = false;
        preservePosition(monitor);
        updated.push_back(std::move(monitor));

        graphHeight = juce::jmax(620.0f, activeChannels * channelRowHeight + 100.0f,
                                  720.0f + maxAuxBuses * 202.0f + 40.0f);
        for (auto& node : updated)
            node.bounds.setPosition(clampNodePosition(node, node.bounds.getPosition()));
        std::optional<NodeIdentity> selectedIdentity;
        if (selectedNode >= 0 && selectedNode < (int)nodes.size())
        {
            const auto& selected = nodes[(size_t)selectedNode];
            selectedIdentity = NodeIdentity{ selected.type, selected.index,
                                             selected.targetIndex, selected.slotIndex };
        }
        nodes = std::move(updated);
        selectedNode = selectedIdentity.has_value()
            ? findNode(*selectedIdentity) : -1;
        draggingNode = false;
        draggingControl = false;
        controlMode = NodeControlMode::None;
        draggingConnection = false;
        connectionSource = -1;
        nodeDragMoved = false;
        if (focusedView)
            captureUnfocusedNodePositions();
        updateFocusedLayout();
        updateSize();
    }

    static juce::String sourceName(const InputChannelState& channel)
    {
        switch (channel.inputSource.load(std::memory_order_relaxed))
        {
            case ChannelInputSource::Hardware:
                return "Hardware In " + juce::String(channel.hardwareInputChannel.load() + 1);
            case ChannelInputSource::Playback:
                return "Audio Playback";
            case ChannelInputSource::TestSignal:
                return "Test Signal";
            case ChannelInputSource::YouTube:
                return "YouTube Capture";
            case ChannelInputSource::Disabled:
            default:
                return "Disabled";
        }
    }

    static juce::String auxFxName(int type)
    {
        switch (type)
        {
            case 1: return "Built-in Reverb";
            case 2: return "Built-in Stereo Delay";
            default: return "Pass-through Return";
        }
    }

    static juce::String nodeTypeId(NodeType type)
    {
        switch (type)
        {
            case NodeType::Channel: return "channel";
            case NodeType::DSP: return "dsp";
            case NodeType::Gate: return "gate";
            case NodeType::Equalizer: return "eq";
            case NodeType::Compressor: return "compressor";
            case NodeType::VSTChain: return "vst3";
            case NodeType::VSTPlugin: return "vst3-plugin";
            case NodeType::Submix: return "submix";
            case NodeType::Aux: return "aux";
            case NodeType::Stream: return "stream";
            case NodeType::Monitor: return "monitor";
        }
        return {};
    }

    static std::optional<NodeType> nodeTypeFromId(const juce::String& type)
    {
        if (type == "channel") return NodeType::Channel;
        if (type == "dsp") return NodeType::DSP;
        if (type == "gate") return NodeType::Gate;
        if (type == "eq") return NodeType::Equalizer;
        if (type == "compressor") return NodeType::Compressor;
        if (type == "vst3") return NodeType::VSTChain;
        if (type == "vst3-plugin") return NodeType::VSTPlugin;
        if (type == "submix") return NodeType::Submix;
        if (type == "aux") return NodeType::Aux;
        if (type == "stream") return NodeType::Stream;
        if (type == "monitor") return NodeType::Monitor;
        return {};
    }

    juce::Point<float> clampNodePosition(const Node& node, juce::Point<float> position) const
    {
        return {
            juce::jlimit(0.0f, graphWidth - node.bounds.getWidth(), position.x),
            juce::jlimit(30.0f, graphHeight - node.bounds.getHeight(), position.y)
        };
    }

    int findNodeAt(juce::Point<float> point) const
    {
        for (int i = (int)nodes.size() - 1; i >= 0; --i)
            if (isNodeVisible(nodes[(size_t)i]) && nodes[(size_t)i].bounds.contains(point))
                return i;
        return -1;
    }

    int findNodeAtOutputPort(juce::Point<float> point) const
    {
        for (int i = (int)nodes.size() - 1; i >= 0; --i)
        {
            const auto& node = nodes[(size_t)i];
            if (!isNodeVisible(node) || !node.hasOutput)
                continue;
            const juce::Point<float> port{ node.bounds.getRight(), node.bounds.getCentreY() };
            if (point.getDistanceFrom(port) <= 13.0f)
                return i;
        }
        return -1;
    }

    static RoutingGraphNodeType toRoutingGraphNodeType(NodeType type)
    {
        switch (type)
        {
            case NodeType::Channel: return RoutingGraphNodeType::Channel;
            case NodeType::DSP:
            case NodeType::Gate:
            case NodeType::Equalizer:
            case NodeType::Compressor:
            case NodeType::VSTChain:
            case NodeType::VSTPlugin:
                break;
            case NodeType::Submix: return RoutingGraphNodeType::Submix;
            case NodeType::Aux: return RoutingGraphNodeType::Aux;
            case NodeType::Stream: return RoutingGraphNodeType::Stream;
            case NodeType::Monitor: return RoutingGraphNodeType::Monitor;
        }
        jassertfalse;
        return RoutingGraphNodeType::Channel;
    }

    static RoutingGraphEndpoint endpointFor(const Node& node)
    {
        if (node.type == NodeType::DSP || node.type == NodeType::Gate ||
            node.type == NodeType::Equalizer || node.type == NodeType::Compressor ||
            node.type == NodeType::VSTChain || node.type == NodeType::VSTPlugin)
        {
            const int target = node.type == NodeType::VSTPlugin ? node.targetIndex : node.index;
            if (target < maxInputs)
                return { RoutingGraphNodeType::Channel, target };
            return { target == maxInputs ? RoutingGraphNodeType::Stream
                                         : RoutingGraphNodeType::Monitor, 0 };
        }
        return { toRoutingGraphNodeType(node.type), node.index };
    }

    static juce::String validationMessage(RoutingGraphValidationResult result)
    {
        switch (result)
        {
            case RoutingGraphValidationResult::Valid: return "valid";
            case RoutingGraphValidationResult::NodeNotFound: return "a node no longer exists";
            case RoutingGraphValidationResult::PortNotFound: return "the selected port does not exist";
            case RoutingGraphValidationResult::DirectionMismatch: return "the cable direction is invalid";
            case RoutingGraphValidationResult::ChannelLayoutUnsupported:
                return "the channel mapping is not supported";
            case RoutingGraphValidationResult::CycleUnsupported:
                return "this connection would create unsupported feedback";
            case RoutingGraphValidationResult::ConnectionUnsupported:
                return "this route is not supported by the audio engine";
        }
        return "the connection is invalid";
    }

    int findNodeAtInputPort(juce::Point<float> point) const
    {
        for (int i = (int)nodes.size() - 1; i >= 0; --i)
        {
            const auto& node = nodes[(size_t)i];
            if (!isNodeVisible(node) || !node.hasInput)
                continue;
            const juce::Point<float> port{ node.bounds.getX(), node.bounds.getCentreY() };
            if (point.getDistanceFrom(port) <= 13.0f)
                return i;
        }
        return -1;
    }

    static juce::String nodeTypeName(NodeType type)
    {
        switch (type)
        {
            case NodeType::Channel: return "Input";
            case NodeType::DSP: return "DSP";
            case NodeType::Gate: return "Gate";
            case NodeType::Equalizer: return "EQ";
            case NodeType::Compressor: return "Compressor";
            case NodeType::VSTChain: return "VST3";
            case NodeType::VSTPlugin: return "VST3 Plugin";
            case NodeType::Submix: return "Submix";
            case NodeType::Aux: return "AUX";
            case NodeType::Stream: return "Stream Bus";
            case NodeType::Monitor: return "Monitor Bus";
        }
        return "Node";
    }

    void reportConnectionStatus(const juce::String& message)
    {
        if (connectionStatus)
            connectionStatus(message);
    }

    static juce::Rectangle<float> channelGainTrack(const Node& node)
    {
        return { node.bounds.getX() + 12.0f, node.bounds.getY() + 55.0f, 92.0f, 8.0f };
    }

    static juce::Rectangle<float> channelPanTrack(const Node& node)
    {
        return { node.bounds.getX() + 126.0f, node.bounds.getY() + 55.0f, 92.0f, 8.0f };
    }

    static juce::Rectangle<float> channelAuxTrack(const Node& node, int auxIndex)
    {
        return { node.bounds.getX() + 12.0f + auxIndex * 51.0f,
                 node.bounds.getY() + 130.0f, 42.0f, 7.0f };
    }

    static juce::Rectangle<float> channelBusSendTrack(const Node& node, int busIndex)
    {
        return { node.bounds.getX() + 12.0f + busIndex * 104.0f,
                 node.bounds.getY() + 99.0f, 96.0f, 7.0f };
    }

    static juce::Rectangle<float> channelHardwareTrack(const Node& node, int outputIndex)
    {
        return { node.bounds.getX() + 12.0f + outputIndex * 25.0f,
                 node.bounds.getY() + 169.0f, 22.0f, 7.0f };
    }

    static juce::Rectangle<float> busGainTrack(const Node& node)
    {
        const bool master = node.type == NodeType::Stream || node.type == NodeType::Monitor;
        const float width = master ? 108.0f : 126.0f;
        const float yOffset = master ? 57.0f : 50.0f;
        return { node.bounds.getX() + 12.0f, node.bounds.getY() + yOffset, width, 8.0f };
    }

    static juce::Rectangle<float> busPanTrack(const Node& node)
    {
        return { node.bounds.getX() + 12.0f, node.bounds.getY() + 78.0f, 126.0f, 8.0f };
    }

    static juce::Rectangle<float> busSendTrack(const Node& node, int busIndex)
    {
        return { node.bounds.getX() + 12.0f + busIndex * 82.0f,
                 node.bounds.getY() + 116.0f, 72.0f, 7.0f };
    }

    static juce::Rectangle<float> auxEffectTrack(const Node& node, int parameter)
    {
        return { node.bounds.getX() + 12.0f, node.bounds.getY() + 158.0f + parameter * 18.0f,
                 126.0f, 7.0f };
    }

    static juce::Rectangle<float> dspParameterTrack(const Node& node, int parameter)
    {
        return { node.bounds.getX() + 78.0f, node.bounds.getY() + 86.0f + parameter * 13.0f,
                 node.bounds.getWidth() - 90.0f, 6.0f };
    }

    static juce::Rectangle<float> moduleParameterTrack(const Node& node)
    {
        return { node.bounds.getX() + 12.0f, node.bounds.getY() + 98.0f,
                 node.bounds.getWidth() - 24.0f, 6.0f };
    }

    static RoutingGraphDSPModule coreModuleForIndex(int index)
    {
        static constexpr std::array<RoutingGraphDSPModule, 5> modules{
            RoutingGraphDSPModule::HPF, RoutingGraphDSPModule::LPF,
            RoutingGraphDSPModule::Limiter, RoutingGraphDSPModule::Reverb,
            RoutingGraphDSPModule::Delay
        };
        return modules[(size_t)juce::jlimit(0, (int)modules.size() - 1, index)];
    }

    static RoutingGraphDSPParameter coreParameterForIndex(int index)
    {
        static constexpr std::array<RoutingGraphDSPParameter, 5> parameters{
            RoutingGraphDSPParameter::HPFFrequency,
            RoutingGraphDSPParameter::LPFFrequency,
            RoutingGraphDSPParameter::LimiterCeiling,
            RoutingGraphDSPParameter::ReverbWet,
            RoutingGraphDSPParameter::DelayTime
        };
        return parameters[(size_t)juce::jlimit(0, (int)parameters.size() - 1, index)];
    }

    static RoutingGraphDSPModule moduleForNode(NodeType type)
    {
        switch (type)
        {
            case NodeType::Gate: return RoutingGraphDSPModule::Gate;
            case NodeType::Equalizer: return RoutingGraphDSPModule::EQ;
            case NodeType::Compressor: return RoutingGraphDSPModule::Compressor;
            default: return RoutingGraphDSPModule::HPF;
        }
    }

    static RoutingGraphDSPParameter parameterForNode(NodeType type)
    {
        switch (type)
        {
            case NodeType::Gate: return RoutingGraphDSPParameter::GateThreshold;
            case NodeType::Equalizer: return RoutingGraphDSPParameter::EQGain;
            case NodeType::Compressor: return RoutingGraphDSPParameter::CompressorThreshold;
            default: return RoutingGraphDSPParameter::HPFFrequency;
        }
    }

    static juce::Rectangle<float> dspModuleButton(const Node& node, int module)
    {
        const int column = module % 4;
        const int row = module / 4;
        return { node.bounds.getX() + 9.0f + column * 56.0f,
                 node.bounds.getY() + 43.0f + row * 17.0f, 51.0f, 14.0f };
    }

    static juce::String dspParameterName(RoutingGraphDSPParameter parameter)
    {
        switch (parameter)
        {
            case RoutingGraphDSPParameter::HPFFrequency: return "HPF";
            case RoutingGraphDSPParameter::LPFFrequency: return "LPF";
            case RoutingGraphDSPParameter::EQGain: return "EQ GAIN";
            case RoutingGraphDSPParameter::GateThreshold: return "GATE";
            case RoutingGraphDSPParameter::CompressorThreshold: return "COMP";
            case RoutingGraphDSPParameter::LimiterCeiling: return "LIMIT";
            case RoutingGraphDSPParameter::ReverbWet: return "REVERB";
            case RoutingGraphDSPParameter::DelayTime: return "DELAY";
        }
        return {};
    }

    static float dspParameterToNormalized(RoutingGraphDSPParameter parameter, float value)
    {
        float minimum = 0.0f;
        float maximum = 1.0f;
        bool logarithmic = false;
        switch (parameter)
        {
            case RoutingGraphDSPParameter::HPFFrequency:
                minimum = 20.0f; maximum = 1000.0f; logarithmic = true; break;
            case RoutingGraphDSPParameter::LPFFrequency:
                minimum = 1000.0f; maximum = 22000.0f; logarithmic = true; break;
            case RoutingGraphDSPParameter::EQGain:
                minimum = -18.0f; maximum = 18.0f; break;
            case RoutingGraphDSPParameter::GateThreshold:
                minimum = -80.0f; maximum = 0.0f; break;
            case RoutingGraphDSPParameter::CompressorThreshold:
                minimum = -60.0f; maximum = 0.0f; break;
            case RoutingGraphDSPParameter::LimiterCeiling:
                minimum = -20.0f; maximum = 0.0f; break;
            case RoutingGraphDSPParameter::ReverbWet:
                minimum = 0.0f; maximum = 1.0f; break;
            case RoutingGraphDSPParameter::DelayTime:
                minimum = 10.0f; maximum = 1500.0f; logarithmic = true; break;
        }
        value = juce::jlimit(minimum, maximum, value);
        if (logarithmic)
            return juce::jlimit(0.0f, 1.0f,
                std::log(value / minimum) / std::log(maximum / minimum));
        return juce::jlimit(0.0f, 1.0f, (value - minimum) / (maximum - minimum));
    }

    static float normalizedToDSPParameter(RoutingGraphDSPParameter parameter, float normalized)
    {
        float minimum = 0.0f;
        float maximum = 1.0f;
        bool logarithmic = false;
        switch (parameter)
        {
            case RoutingGraphDSPParameter::HPFFrequency:
                minimum = 20.0f; maximum = 1000.0f; logarithmic = true; break;
            case RoutingGraphDSPParameter::LPFFrequency:
                minimum = 1000.0f; maximum = 22000.0f; logarithmic = true; break;
            case RoutingGraphDSPParameter::EQGain:
                minimum = -18.0f; maximum = 18.0f; break;
            case RoutingGraphDSPParameter::GateThreshold:
                minimum = -80.0f; maximum = 0.0f; break;
            case RoutingGraphDSPParameter::CompressorThreshold:
                minimum = -60.0f; maximum = 0.0f; break;
            case RoutingGraphDSPParameter::LimiterCeiling:
                minimum = -20.0f; maximum = 0.0f; break;
            case RoutingGraphDSPParameter::ReverbWet:
                minimum = 0.0f; maximum = 1.0f; break;
            case RoutingGraphDSPParameter::DelayTime:
                minimum = 10.0f; maximum = 1500.0f; logarithmic = true; break;
        }
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        if (logarithmic)
            return minimum * std::pow(maximum / minimum, normalized);
        return minimum + normalized * (maximum - minimum);
    }

    static juce::String dspParameterValueText(RoutingGraphDSPParameter parameter, float value)
    {
        if (parameter == RoutingGraphDSPParameter::HPFFrequency ||
            parameter == RoutingGraphDSPParameter::LPFFrequency)
            return value >= 1000.0f ? juce::String(value / 1000.0f, 1) + "k"
                                    : juce::String(juce::roundToInt(value));
        if (parameter == RoutingGraphDSPParameter::DelayTime)
            return juce::String(juce::roundToInt(value)) + "ms";
        if (parameter == RoutingGraphDSPParameter::ReverbWet)
            return juce::String(juce::roundToInt(value * 100.0f)) + "%";
        return juce::String(value, 1) + "dB";
    }

    static RoutingGraphDSPModule dspModuleForIndex(int module)
    {
        return static_cast<RoutingGraphDSPModule>(module);
    }

    static RoutingGraphDSPParameter dspParameterForIndex(int parameter)
    {
        return static_cast<RoutingGraphDSPParameter>(parameter);
    }

    bool beginNodeControlDrag(int nodeIndex, juce::Point<float> point)
    {
        if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
            return false;
        const auto& node = nodes[(size_t)nodeIndex];
        if ((node.type == NodeType::DSP || node.type == NodeType::Gate ||
             node.type == NodeType::Equalizer || node.type == NodeType::Compressor) &&
            dspControlCallbacks.setParameter)
        {
            const bool moduleNode = node.type != NodeType::DSP;
            const int parameterCount = moduleNode ? 1 : 5;
            for (int parameter = 0; parameter < parameterCount; ++parameter)
            {
                const auto track = moduleNode ? moduleParameterTrack(node)
                                              : dspParameterTrack(node, parameter);
                if (!track.expanded(2.0f, 5.0f).contains(point))
                    continue;
                controlMode = static_cast<NodeControlMode>(
                    static_cast<int>(NodeControlMode::DSPParameter0) + parameter);
                break;
            }
            if (controlMode < NodeControlMode::DSPParameter0 ||
                controlMode > NodeControlMode::DSPParameter4)
                return false;
        }
        else if (node.type == NodeType::Channel)
        {
            if (channelGainTrack(node).expanded(5.0f, 8.0f).contains(point))
                controlMode = NodeControlMode::ChannelGain;
            else if (channelPanTrack(node).expanded(5.0f, 8.0f).contains(point))
                controlMode = NodeControlMode::ChannelPan;
            else if (channelBusSendTrack(node, 0).expanded(3.0f, 7.0f).contains(point))
                controlMode = NodeControlMode::ChannelStreamSend;
            else if (channelBusSendTrack(node, 1).expanded(3.0f, 7.0f).contains(point))
                controlMode = NodeControlMode::ChannelMonitorSend;
            else
            {
                bool foundAux = false;
                for (int aux = 0; aux < maxAuxBuses; ++aux)
                {
                    if (!channelAuxTrack(node, aux).expanded(3.0f, 7.0f).contains(point))
                        continue;
                    controlMode = static_cast<NodeControlMode>(
                        static_cast<int>(NodeControlMode::ChannelAux0) + aux);
                    foundAux = true;
                    break;
                }
                if (!foundAux)
                {
                    bool foundHardware = false;
                    for (int output = 0; output < maxHwOutputs; ++output)
                    {
                        if (!channelHardwareTrack(node, output).expanded(2.0f, 6.0f).contains(point))
                            continue;
                        controlMode = static_cast<NodeControlMode>(
                            static_cast<int>(NodeControlMode::ChannelHardware0) + output);
                        foundHardware = true;
                        break;
                    }
                    if (!foundHardware)
                        return false;
                }
            }
        }
        else if (node.type == NodeType::Submix &&
                 busGainTrack(node).expanded(5.0f, 8.0f).contains(point))
            controlMode = NodeControlMode::SubmixGain;
        else if (node.type == NodeType::Submix &&
                 busPanTrack(node).expanded(5.0f, 8.0f).contains(point))
            controlMode = NodeControlMode::SubmixPan;
        else if (node.type == NodeType::Submix &&
                 busSendTrack(node, 0).expanded(3.0f, 7.0f).contains(point))
            controlMode = NodeControlMode::SubmixStreamSend;
        else if (node.type == NodeType::Submix &&
                 busSendTrack(node, 1).expanded(3.0f, 7.0f).contains(point))
            controlMode = NodeControlMode::SubmixMonitorSend;
        else if (node.type == NodeType::Aux &&
                 busGainTrack(node).expanded(5.0f, 8.0f).contains(point))
            controlMode = NodeControlMode::AuxReturn;
        else if (node.type == NodeType::Aux &&
                 busPanTrack(node).expanded(5.0f, 8.0f).contains(point))
            controlMode = NodeControlMode::AuxPan;
        else if (node.type == NodeType::Aux &&
                 busSendTrack(node, 0).expanded(3.0f, 7.0f).contains(point))
            controlMode = NodeControlMode::AuxStreamSend;
        else if (node.type == NodeType::Aux &&
                 busSendTrack(node, 1).expanded(3.0f, 7.0f).contains(point))
            controlMode = NodeControlMode::AuxMonitorSend;
        else if (node.type == NodeType::Aux && node.index == 1 &&
                 matrix.getAuxBus(node.index).fxType.load(std::memory_order_relaxed) == 2)
        {
            if (auxEffectTrack(node, 0).expanded(3.0f, 5.0f).contains(point))
                controlMode = NodeControlMode::AuxDelayTime;
            else if (auxEffectTrack(node, 1).expanded(3.0f, 5.0f).contains(point))
                controlMode = NodeControlMode::AuxDelayFeedback;
            else
            {
                return false;
            }
        }
        else if ((node.type == NodeType::Stream || node.type == NodeType::Monitor) &&
                 busGainTrack(node).expanded(5.0f, 8.0f).contains(point) &&
                 masterBusControlCallbacks.setGainDb)
            controlMode = NodeControlMode::MasterGain;
        else
            return false;

        selectedNode = nodeIndex;
        draggingControl = true;
        updateNodeControl(point);
        return true;
    }

    void updateNodeControl(juce::Point<float> point)
    {
        if (selectedNode < 0 || selectedNode >= (int)nodes.size())
            return;
        const auto& node = nodes[(size_t)selectedNode];
        if (controlMode >= NodeControlMode::DSPParameter0 &&
            controlMode <= NodeControlMode::DSPParameter4 && dspControlCallbacks.setParameter)
        {
            const int parameterIndex = static_cast<int>(controlMode) -
                                       static_cast<int>(NodeControlMode::DSPParameter0);
            const bool moduleNode = node.type != NodeType::DSP;
            const auto parameter = moduleNode ? parameterForNode(node.type)
                                              : coreParameterForIndex(parameterIndex);
            const auto track = moduleNode ? moduleParameterTrack(node)
                                          : dspParameterTrack(node, parameterIndex);
            const float normalized = juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            dspControlCallbacks.setParameter(node.index, parameter,
                normalizedToDSPParameter(parameter, normalized));
            if (dspControlCallbacks.controlChanged)
                dspControlCallbacks.controlChanged(node.index);
        }
        else if (controlMode == NodeControlMode::ChannelGain)
        {
            const auto track = channelGainTrack(node);
            const float db = -60.0f + juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth()) * 72.0f;
            matrix.getChannel(node.index).gain.store(
                db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain(db),
                std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::AuxStreamSend ||
                 controlMode == NodeControlMode::AuxMonitorSend)
        {
            const int bus = controlMode == NodeControlMode::AuxStreamSend ? 0 : 1;
            const auto track = busSendTrack(node, bus);
            const float level = 2.0f * juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            auto& aux = matrix.getAuxBus(node.index);
            (bus == 0 ? aux.sendLevelStream : aux.sendLevelMonitor)
                .store(level, std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::AuxDelayTime)
        {
            auto& aux = matrix.getAuxBus(node.index);
            const auto track = auxEffectTrack(node, 0);
            const float value = juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            aux.fxParam1.store(0.01f + value * 1.49f, std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::AuxDelayFeedback)
        {
            auto& aux = matrix.getAuxBus(node.index);
            const auto track = auxEffectTrack(node, 1);
            const float value = juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            aux.fxParam2.store(value * 0.85f, std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::ChannelPan)
        {
            const auto track = channelPanTrack(node);
            matrix.getChannel(node.index).pan.store(
                juce::jlimit(-1.0f, 1.0f,
                    2.0f * (point.x - track.getX()) / track.getWidth() - 1.0f),
                std::memory_order_relaxed);
        }
        else if (controlMode >= NodeControlMode::ChannelAux0 &&
                 controlMode <= NodeControlMode::ChannelAux3)
        {
            const int aux = static_cast<int>(controlMode) -
                            static_cast<int>(NodeControlMode::ChannelAux0);
            const auto track = channelAuxTrack(node, aux);
            const float level = 2.0f * juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            matrix.getChannel(node.index).auxSendLevel[(size_t)aux].store(
                level, std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::ChannelStreamSend ||
                 controlMode == NodeControlMode::ChannelMonitorSend)
        {
            const int bus = controlMode == NodeControlMode::ChannelStreamSend ? 0 : 1;
            const auto track = channelBusSendTrack(node, bus);
            const float level = 2.0f * juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            matrix.getChannel(node.index).sendLevel[(size_t)bus].store(
                level, std::memory_order_relaxed);
        }
        else if (controlMode >= NodeControlMode::ChannelHardware0 &&
                 controlMode <= NodeControlMode::ChannelHardware7)
        {
            const int output = static_cast<int>(controlMode) -
                               static_cast<int>(NodeControlMode::ChannelHardware0);
            const auto track = channelHardwareTrack(node, output);
            const float level = 2.0f * juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            matrix.getChannel(node.index).hwSendLevel[(size_t)output].store(
                level, std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::SubmixGain)
        {
            const auto track = busGainTrack(node);
            const float db = -60.0f + juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth()) * 66.0f;
            matrix.getSubmixBus(node.index).gain.store(
                db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain(db),
                std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::SubmixPan)
        {
            const auto track = busPanTrack(node);
            matrix.getSubmixBus(node.index).pan.store(
                juce::jlimit(-1.0f, 1.0f,
                    2.0f * (point.x - track.getX()) / track.getWidth() - 1.0f),
                std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::SubmixStreamSend ||
                 controlMode == NodeControlMode::SubmixMonitorSend)
        {
            const int bus = controlMode == NodeControlMode::SubmixStreamSend ? 0 : 1;
            const auto track = busSendTrack(node, bus);
            const float level = 2.0f * juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth());
            auto& submix = matrix.getSubmixBus(node.index);
            (bus == 0 ? submix.sendLevelStream : submix.sendLevelMonitor)
                .store(level, std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::AuxReturn)
        {
            const auto track = busGainTrack(node);
            const float db = -60.0f + juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth()) * 66.0f;
            matrix.getAuxBus(node.index).returnLevel.store(
                db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain(db),
                std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::AuxPan)
        {
            const auto track = busPanTrack(node);
            matrix.getAuxBus(node.index).pan.store(
                juce::jlimit(-1.0f, 1.0f,
                    2.0f * (point.x - track.getX()) / track.getWidth() - 1.0f),
                std::memory_order_relaxed);
        }
        else if (controlMode == NodeControlMode::MasterGain && masterBusControlCallbacks.setGainDb)
        {
            const auto track = busGainTrack(node);
            const float db = -60.0f + juce::jlimit(0.0f, 1.0f,
                (point.x - track.getX()) / track.getWidth()) * 72.0f;
            masterBusControlCallbacks.setGainDb(node.type == NodeType::Stream ? 0 : 1, db);
        }
    }

    static bool hitToggle(juce::Point<float> point, float x, float y, float width, float height)
    {
        return juce::Rectangle<float>(x, y, width, height).contains(point);
    }

    bool handleNodeButtonClick(int nodeIndex, juce::Point<float> point)
    {
        if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
            return false;
        const auto& node = nodes[(size_t)nodeIndex];
        const float x = node.bounds.getX();
        const float y = node.bounds.getY();
        if ((node.type == NodeType::DSP || node.type == NodeType::Gate ||
             node.type == NodeType::Equalizer || node.type == NodeType::Compressor) &&
            dspControlCallbacks.setModuleEnabled)
        {
            const bool moduleNode = node.type != NodeType::DSP;
            const int moduleCount = moduleNode ? 1 : 5;
            for (int module = 0; module < moduleCount; ++module)
            {
                const auto button = moduleNode
                    ? juce::Rectangle<float>(node.bounds.getX() + 10.0f,
                        node.bounds.getY() + 39.0f, 52.0f, 16.0f)
                    : dspModuleButton(node, module);
                if (!button.contains(point))
                    continue;
                const auto moduleId = moduleNode ? moduleForNode(node.type)
                                                  : coreModuleForIndex(module);
                const bool enabled = dspControlCallbacks.getModuleEnabled &&
                    dspControlCallbacks.getModuleEnabled(node.index, moduleId);
                dspControlCallbacks.setModuleEnabled(node.index, moduleId, !enabled);
                if (dspControlCallbacks.controlChanged)
                    dspControlCallbacks.controlChanged(node.index);
                return true;
            }
        }

        if (node.type == NodeType::Channel)
        {
            auto& channel = matrix.getChannel(node.index);
            if (hitToggle(point, x + 164.0f, y + 6.0f, 28.0f, 15.0f))
            {
                showChannelOptions(node.index, true);
                return true;
            }
            if (hitToggle(point, x + 194.0f, y + 6.0f, 28.0f, 15.0f))
            {
                showChannelOptions(node.index, false);
                return true;
            }
            const float buttonY = y + 70.0f;
            const float buttonX = x + 12.0f;
            const float buttonWidth = 38.0f;
            const float step = 40.0f;
            for (int aux = 0; aux < maxAuxBuses; ++aux)
            {
                const float auxX = x + 12.0f + aux * 51.0f;
                if (hitToggle(point, auxX, y + 112.0f, 42.0f, 14.0f))
                {
                    channel.auxRouteEnable[(size_t)aux].store(
                        !channel.auxRouteEnable[(size_t)aux].load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    return true;
                }
            }
            for (int output = 0; output < maxHwOutputs; ++output)
            {
                const float outputX = x + 12.0f + output * 25.0f;
                if (!hitToggle(point, outputX, y + 148.0f, 22.0f, 14.0f))
                    continue;
                if (!matrix.isHardwareOutputAvailable(output))
                    return true;
                channel.hwRouteEnable[(size_t)output].store(
                    !channel.hwRouteEnable[(size_t)output].load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
                return true;
            }
            if (!hitToggle(point, buttonX, buttonY, buttonWidth, 18.0f) &&
                !hitToggle(point, buttonX + step, buttonY, buttonWidth, 18.0f) &&
                !hitToggle(point, buttonX + step * 2.0f, buttonY, buttonWidth, 18.0f) &&
                !hitToggle(point, buttonX + step * 3.0f, buttonY, buttonWidth, 18.0f) &&
                !hitToggle(point, buttonX + step * 4.0f, buttonY, buttonWidth, 18.0f))
                return false;

            if (hitToggle(point, buttonX, buttonY, buttonWidth, 18.0f))
                channel.mute.store(!channel.mute.load(std::memory_order_relaxed), std::memory_order_relaxed);
            else if (hitToggle(point, buttonX + step, buttonY, buttonWidth, 18.0f))
                channel.solo.store(!channel.solo.load(std::memory_order_relaxed), std::memory_order_relaxed);
            else if (hitToggle(point, buttonX + step * 2.0f, buttonY, buttonWidth, 18.0f))
                matrix.applyChannelRouteState(node.index,
                    !channel.routeEnable[0].load(std::memory_order_relaxed),
                    channel.routeEnable[1].load(std::memory_order_relaxed));
            else if (hitToggle(point, buttonX + step * 3.0f, buttonY, buttonWidth, 18.0f))
                matrix.applyChannelRouteState(node.index,
                    channel.routeEnable[0].load(std::memory_order_relaxed),
                    !channel.routeEnable[1].load(std::memory_order_relaxed));
            else
                channel.isEnabled.store(!channel.isEnabled.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
            return true;
        }

        if (node.type == NodeType::Submix || node.type == NodeType::Aux)
        {
            const float buttonY = y + 134.0f;
            const float buttonX = x + 12.0f;
            const float step = 82.0f;
            const float muteX = x + 160.0f;
            if (node.type == NodeType::Aux &&
                hitToggle(point, x + 160.0f, y + 94.0f, 40.0f, 18.0f))
            {
                if (node.index == 0)
                {
                    auto& fxType = matrix.getAuxBus(node.index).fxType;
                    fxType.store(fxType.load(std::memory_order_relaxed) == 1 ? 0 : 1,
                                 std::memory_order_relaxed);
                }
                else if (node.index == 1)
                {
                    auto& fxType = matrix.getAuxBus(node.index).fxType;
                    fxType.store(fxType.load(std::memory_order_relaxed) == 2 ? 0 : 2,
                                 std::memory_order_relaxed);
                }
                else
                {
                    reportConnectionStatus("Built-in effects are only implemented on Aux 1 (Reverb) and Aux 2 (Delay).");
                }
                return true;
            }
            if (hitToggle(point, muteX, y + 44.0f, 40.0f, 18.0f))
            {
                if (node.type == NodeType::Submix)
                {
                    auto& mute = matrix.getSubmixBus(node.index).mute;
                    mute.store(!mute.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
                else
                {
                    auto& mute = matrix.getAuxBus(node.index).mute;
                    mute.store(!mute.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
                return true;
            }
            if (hitToggle(point, muteX, y + 67.0f, 40.0f, 18.0f))
            {
                if (node.type == NodeType::Submix)
                {
                    auto& solo = matrix.getSubmixBus(node.index).solo;
                    solo.store(!solo.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
                else
                {
                    auto& solo = matrix.getAuxBus(node.index).solo;
                    solo.store(!solo.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
                return true;
            }
            const bool streamRouteHit = hitToggle(point, buttonX, buttonY, 72.0f, 14.0f);
            const bool monitorRouteHit = hitToggle(point, buttonX + step, buttonY, 72.0f, 14.0f);
            if (!streamRouteHit && !monitorRouteHit)
                return false;

            const int route = streamRouteHit ? 0 : 1;
            if (node.type == NodeType::Submix)
            {
                auto& bus = matrix.getSubmixBus(node.index);
                auto& enabled = route == 0 ? bus.routeToStream : bus.routeToMonitor;
                enabled.store(!enabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            else
            {
                auto& bus = matrix.getAuxBus(node.index);
                auto& enabled = route == 0 ? bus.sendToStream : bus.sendToMonitor;
                enabled.store(!enabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            return true;
        }

        if (node.type == NodeType::Stream || node.type == NodeType::Monitor)
        {
            if (hitToggle(point, x + 130.0f, y + 53.0f, 32.0f, 18.0f) &&
                masterBusControlCallbacks.setMute)
            {
                const int bus = node.type == NodeType::Stream ? 0 : 1;
                const bool muted = masterBusControlCallbacks.getMute &&
                    masterBusControlCallbacks.getMute(bus);
                masterBusControlCallbacks.setMute(bus, !muted);
                return true;
            }
        }
        return false;
    }

    void showChannelOptions(int channelIndex, bool inputOptions)
    {
        juce::PopupMenu menu;
        if (inputOptions)
        {
            juce::PopupMenu sources;
            juce::PopupMenu hardware;
            const int availableHardwareInputs = matrix.getNumAvailableHardwareInputs();
            for (int port = 0; port < availableHardwareInputs; ++port)
                hardware.addItem(100 + port, "Hardware input " + juce::String(port + 1));
            if (availableHardwareInputs == 0)
                hardware.addItem(99, "No physical inputs available", false);
            sources.addSubMenu("Hardware", hardware);
            sources.addItem(1, "Audio playback");
            sources.addItem(2, "Test signal");
            sources.addItem(3, "Disabled");
            sources.addItem(4, "YouTube capture");
            menu.addSubMenu("Input source", sources);
            juce::PopupMenu playbackChannels;
            playbackChannels.addItem(10, "Left");
            playbackChannels.addItem(11, "Right");
            playbackChannels.addItem(12, "Sum");
            menu.addSubMenu("Playback channel", playbackChannels);
        }
        else
        {
            juce::PopupMenu modes;
            modes.addItem(20, "Mono");
            modes.addItem(21, "Stereo pair");
            modes.addItem(22, "Dual mono");
            menu.addSubMenu("Channel mode", modes);
        }

        juce::Component::SafePointer<RoutingGraphCanvas> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safeThis, channelIndex, inputOptions](int result)
            {
                if (safeThis == nullptr || result == 0)
                    return;
                auto& channel = safeThis->matrix.getChannel(channelIndex);
                if (inputOptions)
                {
                    if (result >= 100 && result < 100 + maxInputs)
                    {
                        channel.inputSource.store(ChannelInputSource::Hardware, std::memory_order_relaxed);
                        channel.hardwareInputChannel.store(result - 100, std::memory_order_relaxed);
                    }
                    else if (result == 1)
                        channel.inputSource.store(ChannelInputSource::Playback, std::memory_order_relaxed);
                    else if (result == 2)
                        channel.inputSource.store(ChannelInputSource::TestSignal, std::memory_order_relaxed);
                    else if (result == 3)
                        channel.inputSource.store(ChannelInputSource::Disabled, std::memory_order_relaxed);
                    else if (result == 4)
                        channel.inputSource.store(ChannelInputSource::YouTube, std::memory_order_relaxed);
                    else if (result >= 10 && result <= 12)
                        channel.playbackChannel.store(result - 10, std::memory_order_relaxed);
                }
                else if (result >= 20 && result <= 22)
                    channel.stereoMode.store(static_cast<ChannelStereoMode>(result - 20),
                                             std::memory_order_relaxed);
                safeThis->matrix.validateAvailableInputs(
                    safeThis->matrix.getNumAvailableHardwareInputs());
                safeThis->matrix.updateDuplicateWarnings();
                safeThis->repaint();
            });
    }

    static float normalizedGain(float gain)
    {
        const float db = juce::Decibels::gainToDecibels(gain, -60.0f);
        return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 72.0f);
    }

    static float normalizedBusGain(float gain)
    {
        const float db = juce::Decibels::gainToDecibels(gain, -60.0f);
        return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
    }

    static void drawControlTrack(juce::Graphics& g, juce::Rectangle<float> track,
                                 float normalized, juce::Colour colour)
    {
        g.setColour(juce::Colour(0xff080c12));
        g.fillRoundedRectangle(track, 2.0f);
        const float fillWidth = track.getWidth() * juce::jlimit(0.0f, 1.0f, normalized);
        g.setColour(colour.withAlpha(0.85f));
        g.fillRoundedRectangle(track.withWidth(fillWidth), 2.0f);
        g.setColour(juce::Colour(0xffe2e8f0));
        g.fillEllipse(track.getX() + fillWidth - 3.0f, track.getY() - 2.0f, 6.0f, 12.0f);
    }

    static void drawToggle(juce::Graphics& g, juce::Rectangle<float> bounds,
                           const juce::String& text, bool enabled, juce::Colour colour)
    {
        g.setColour(enabled ? colour.withAlpha(0.24f) : juce::Colour(0xff1b2430));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(enabled ? colour : juce::Colour(0xff64748b));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        g.drawText(text, bounds, juce::Justification::centred);
    }

    void drawNode(juce::Graphics& g, const Node& node)
    {
        auto bounds = node.bounds;
        const auto& channel = node.type == NodeType::Channel ? &matrix.getChannel(node.index) : nullptr;
        const bool inactive = channel != nullptr &&
            (!channel->isEnabled.load(std::memory_order_relaxed) ||
             channel->inputSource.load(std::memory_order_relaxed) == ChannelInputSource::Disabled ||
             !channel->isSourceAvailable.load(std::memory_order_relaxed));
        const bool muted = channel != nullptr && channel->mute.load(std::memory_order_relaxed);

        g.setColour(inactive || muted ? juce::Colour(0xff1b2028) : juce::Colour(0xff121b27));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(node.colour.withAlpha(inactive || muted ? 0.45f : 1.0f));
        const bool selected = selectedNode >= 0 && selectedNode < (int)nodes.size() &&
                              &node == &nodes[(size_t)selectedNode];
        g.drawRoundedRectangle(bounds, 8.0f, selected ? 2.0f : 1.2f);

        if (node.hasInput)
            drawPort(g, { node.bounds.getX(), node.bounds.getCentreY() }, node.colour);
        if (node.hasOutput)
            drawPort(g, { node.bounds.getRight(), node.bounds.getCentreY() }, node.colour);

        auto titleBounds = bounds.reduced(12.0f, 7.0f);
        g.setColour(juce::Colour(0xfff1f5f9));
        g.setFont(juce::FontOptions(node.type == NodeType::Channel ? 12.0f : 13.0f, juce::Font::bold));
        g.drawText(node.title, titleBounds.removeFromTop(19.0f)
                                      .withTrimmedRight(node.type == NodeType::Channel ? 76.0f : 18.0f),
                   juce::Justification::centredLeft, true);
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(node.subtitle, titleBounds.removeFromTop(16.0f).withTrimmedRight(10.0f),
                   juce::Justification::centredLeft, true);

        if (node.type == NodeType::DSP)
        {
            static const std::array<juce::String, 5> moduleNames{
                "HPF", "LPF", "LIMIT", "REVERB", "DELAY"
            };
            static const std::array<juce::Colour, 5> moduleColours{
                juce::Colour(0xff38bdf8), juce::Colour(0xff38bdf8),
                juce::Colour(0xffef4444), juce::Colour(0xffc084fc),
                juce::Colour(0xff22d3ee)
            };
            for (int module = 0; module < 5; ++module)
            {
                const auto moduleId = coreModuleForIndex(module);
                const bool enabled = dspControlCallbacks.getModuleEnabled &&
                    dspControlCallbacks.getModuleEnabled(node.index, moduleId);
                drawToggle(g, dspModuleButton(node, module), moduleNames[(size_t)module],
                           enabled, moduleColours[(size_t)module]);
            }

            if (dspControlCallbacks.getParameter)
            {
                g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                for (int parameterIndex = 0; parameterIndex < 5; ++parameterIndex)
                {
                    const auto parameter = coreParameterForIndex(parameterIndex);
                    const float value = dspControlCallbacks.getParameter(node.index, parameter);
                    const auto label = dspParameterName(parameter) + " " +
                                       dspParameterValueText(parameter, value);
                    const auto track = dspParameterTrack(node, parameterIndex);
                    g.setColour(juce::Colour(0xffcbd5e1));
                    g.drawText(label,
                        juce::roundToInt(node.bounds.getX() + 9.0f),
                        juce::roundToInt(track.getY() - 3.0f), 65, 12,
                        juce::Justification::centredLeft, true);
                    drawControlTrack(g, track, dspParameterToNormalized(parameter, value),
                                     node.colour);
                }
            }
        }
        else if (node.type == NodeType::Gate || node.type == NodeType::Equalizer ||
                 node.type == NodeType::Compressor)
        {
            const auto module = moduleForNode(node.type);
            const auto parameter = parameterForNode(node.type);
            const bool enabled = dspControlCallbacks.getModuleEnabled &&
                dspControlCallbacks.getModuleEnabled(node.index, module);
            const float value = dspControlCallbacks.getParameter
                ? dspControlCallbacks.getParameter(node.index, parameter) : 0.0f;
            const juce::String toggleText = node.type == NodeType::Equalizer ? "EQ"
                : node.type == NodeType::Gate ? "GATE" : "COMP";
            drawToggle(g, { node.bounds.getX() + 10.0f, node.bounds.getY() + 39.0f,
                            52.0f, 16.0f }, toggleText, enabled, node.colour);
            if (node.type == NodeType::Equalizer)
            {
                const juce::Rectangle<float> curveBounds{
                    node.bounds.getX() + 10.0f, node.bounds.getY() + 61.0f,
                    node.bounds.getWidth() - 20.0f, 22.0f };
                g.setColour(juce::Colour(0xff080c12));
                g.fillRoundedRectangle(curveBounds, 3.0f);
                g.setColour(juce::Colour(0xff334155));
                g.drawHorizontalLine(curveBounds.getCentreY(), curveBounds.getX(),
                                     curveBounds.getRight());
                juce::Path curve;
                curve.startNewSubPath(curveBounds.getX(), curveBounds.getCentreY());
                for (int step = 1; step <= 24; ++step)
                {
                    const float x = (float)step / 24.0f;
                    const float shape = std::exp(-std::pow((x - 0.5f) * 5.0f, 2.0f));
                    const float normalizedGain = juce::jlimit(-1.0f, 1.0f, value / 18.0f);
                    const float y = curveBounds.getCentreY() -
                        shape * normalizedGain * (curveBounds.getHeight() * 0.42f);
                    curve.lineTo(curveBounds.getX() + x * curveBounds.getWidth(), y);
                }
                g.setColour(node.colour);
                g.strokePath(curve, juce::PathStrokeType(1.6f));
            }
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xffcbd5e1));
            g.drawText(dspParameterName(parameter) + " " +
                       dspParameterValueText(parameter, value),
                       node.bounds.getX() + 10.0f, node.bounds.getY() + 83.0f,
                       node.bounds.getWidth() - 20.0f, 12.0f,
                       juce::Justification::centredLeft);
            drawControlTrack(g, moduleParameterTrack(node),
                             dspParameterToNormalized(parameter, value), node.colour);
        }
        else if (node.type == NodeType::Channel)
        {
            const float level = juce::jlimit(0.0f, 1.0f, channel->inputMeter.load(std::memory_order_relaxed));
            drawMeter(g, bounds.getX() + 12.0f, bounds.getBottom() - 5.0f,
                     bounds.getWidth() - 24.0f, 3.0f, level, node.colour);
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xff94a3b8));
            const float gain = channel->gain.load(std::memory_order_relaxed);
            const auto gainText = gain <= 0.0001f ? juce::String("-inf dB")
                : juce::String(juce::Decibels::gainToDecibels(gain), 1) + " dB";
            g.drawText(gainText, channelGainTrack(node).withY(bounds.getY() + 43.0f).withHeight(10.0f),
                       juce::Justification::centredLeft);
            const float currentPan = juce::jlimit(-1.0f, 1.0f, channel->pan.load());
            const auto panText = std::abs(currentPan) < 0.05f ? juce::String("C")
                : (currentPan < 0.0f ? "L " : "R ") +
                    juce::String(juce::roundToInt(std::abs(currentPan) * 100.0f)) + "%";
            g.drawText(panText, channelPanTrack(node).withY(bounds.getY() + 43.0f).withHeight(10.0f),
                       juce::Justification::centredLeft);
            drawControlTrack(g, channelGainTrack(node), normalizedGain(gain),
                             juce::Colour(0xff10b981));
            const float pan = currentPan;
            const auto panTrack = channelPanTrack(node);
            const float panCentre = panTrack.getX() + panTrack.getWidth() * 0.5f;
            g.setColour(juce::Colour(0xff080c12));
            g.fillRoundedRectangle(panTrack, 2.0f);
            g.setColour(node.colour.withAlpha(0.85f));
            const float panX = pan >= 0.0f ? panCentre : panCentre + pan * panTrack.getWidth() * 0.5f;
            const float panWidth = std::abs(pan) * panTrack.getWidth() * 0.5f;
            g.fillRoundedRectangle(pan >= 0.0f ? juce::Rectangle<float>(panCentre, panTrack.getY(), panWidth, panTrack.getHeight())
                                               : juce::Rectangle<float>(panX, panTrack.getY(), panWidth, panTrack.getHeight()), 2.0f);
            g.setColour(juce::Colour(0xffe2e8f0));
            g.fillEllipse(panCentre + pan * panTrack.getWidth() * 0.5f - 3.0f,
                          panTrack.getY() - 2.0f, 6.0f, 12.0f);

            const float buttonY = bounds.getY() + 70.0f;
            const float buttonX = bounds.getX() + 12.0f;
            const float step = 40.0f;
            drawToggle(g, { buttonX, buttonY, 38.0f, 18.0f }, "MUTE",
                       channel->mute.load(), juce::Colour(0xffef4444));
            drawToggle(g, { buttonX + step, buttonY, 38.0f, 18.0f }, "SOLO",
                       channel->solo.load(), juce::Colour(0xffeab308));
            drawToggle(g, { buttonX + step * 2.0f, buttonY, 38.0f, 18.0f }, "STR",
                       channel->routeEnable[0].load(), juce::Colour(0xffc084fc));
            drawToggle(g, { buttonX + step * 3.0f, buttonY, 38.0f, 18.0f }, "MON",
                       channel->routeEnable[1].load(), juce::Colour(0xff22d3ee));
            drawToggle(g, { buttonX + step * 4.0f, buttonY, 38.0f, 18.0f }, "EN",
                       channel->isEnabled.load(), juce::Colour(0xff10b981));
            drawToggle(g, { bounds.getX() + 164.0f, bounds.getY() + 6.0f, 28.0f, 15.0f },
                       "SRC", true, juce::Colour(0xff38bdf8));
            drawToggle(g, { bounds.getX() + 194.0f, bounds.getY() + 6.0f, 28.0f, 15.0f },
                       "MODE", true, juce::Colour(0xfff59e0b));
            for (int aux = 0; aux < maxAuxBuses; ++aux)
            {
                const auto track = channelAuxTrack(node, aux);
                const float auxX = bounds.getX() + 12.0f + aux * 51.0f;
                const float auxSend = channel->auxSendLevel[(size_t)aux].load(
                    std::memory_order_relaxed);
                const auto auxText = auxSend <= 0.0001f ? juce::String("-inf")
                    : juce::String(juce::Decibels::gainToDecibels(auxSend), 0) + " dB";
                drawToggle(g, { auxX, bounds.getY() + 112.0f, 42.0f, 14.0f },
                           "A" + juce::String(aux + 1) + " " + auxText,
                           channel->auxRouteEnable[(size_t)aux].load(),
                           juce::Colour(0xffa78bfa));
                drawControlTrack(g, track,
                    auxSend * 0.5f,
                    juce::Colour(0xffa78bfa));
            }
            for (int bus = 0; bus < numBuses; ++bus)
            {
                const auto track = channelBusSendTrack(node, bus);
                const auto colour = bus == 0 ? juce::Colour(0xffc084fc)
                                             : juce::Colour(0xff22d3ee);
                const float send = channel->sendLevel[(size_t)bus].load(
                    std::memory_order_relaxed);
                g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                g.setColour(colour);
                const auto sendLabel = (bus == 0 ? "STR " : "MON ") +
                    (send <= 0.0001f ? juce::String("-inf")
                                     : juce::String(juce::Decibels::gainToDecibels(send), 1) + " dB");
                g.drawText(sendLabel,
                           track.withY(bounds.getY() + 86.0f).withHeight(10.0f),
                           juce::Justification::centredLeft);
                drawControlTrack(g, track, send * 0.5f, colour);
            }
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText("DIRECT HW OUTPUTS",
                       juce::Rectangle<float>(bounds.getX() + 12.0f,
                                              bounds.getY() + 137.0f, 180.0f, 10.0f),
                       juce::Justification::centredLeft);
            for (int output = 0; output < maxHwOutputs; ++output)
            {
                const float outputX = bounds.getX() + 12.0f + output * 25.0f;
                const bool available = matrix.isHardwareOutputAvailable(output);
                const float outputGain = channel->hwSendLevel[(size_t)output].load(
                    std::memory_order_relaxed);
                drawToggle(g, { outputX, bounds.getY() + 148.0f, 22.0f, 14.0f },
                           "H" + juce::String(output + 1),
                           available && channel->hwRouteEnable[(size_t)output].load(),
                           available ? juce::Colour(0xff38bdf8) : juce::Colour(0xff475569));
                drawControlTrack(g, channelHardwareTrack(node, output),
                    outputGain * 0.5f,
                    available ? juce::Colour(0xff38bdf8) : juce::Colour(0xff334155));
            }
        }
        else if (node.type == NodeType::DSP || node.type == NodeType::VSTChain ||
                 node.type == NodeType::VSTPlugin)
        {
            const int target = node.type == NodeType::VSTPlugin ? node.targetIndex : node.index;
            float level = 0.0f;
            if (target < maxInputs)
            {
                const auto& channelState = matrix.getChannel(target);
                level = node.type == NodeType::DSP
                    ? channelState.inputMeter.load(std::memory_order_relaxed)
                    : channelState.peakMeter.load(std::memory_order_relaxed);
            }
            else
                level = matrix.getMasterBusPeak(target == maxInputs ? 0 : 1);
            drawMeter(g, bounds.getX() + 12.0f, bounds.getBottom() - 7.0f,
                     bounds.getWidth() - 24.0f, 3.0f, juce::jlimit(0.0f, 1.0f, level), node.colour);
        }
        else if (node.type == NodeType::Submix || node.type == NodeType::Aux)
        {
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText(node.type == NodeType::Submix ? "GROUP LEVEL" : "RETURN LEVEL",
                       busGainTrack(node).withY(bounds.getY() + 42.0f).withHeight(10.0f),
                       juce::Justification::centredLeft);
            const float gain = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).gain.load()
                : matrix.getAuxBus(node.index).returnLevel.load();
            drawControlTrack(g, busGainTrack(node), normalizedBusGain(gain), node.colour);
            const float pan = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).pan.load()
                : matrix.getAuxBus(node.index).pan.load();
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText("PAN", busPanTrack(node).withY(bounds.getY() + 68.0f).withHeight(10.0f),
                       juce::Justification::centredLeft);
            const auto panTrack = busPanTrack(node);
            const float panCentre = panTrack.getX() + panTrack.getWidth() * 0.5f;
            const float boundedPan = juce::jlimit(-1.0f, 1.0f, pan);
            const float panX = boundedPan >= 0.0f
                ? panCentre : panCentre + boundedPan * panTrack.getWidth() * 0.5f;
            const float panWidth = std::abs(boundedPan) * panTrack.getWidth() * 0.5f;
            g.setColour(juce::Colour(0xff080c12));
            g.fillRoundedRectangle(panTrack, 2.0f);
            g.setColour(node.colour.withAlpha(0.85f));
            g.fillRoundedRectangle(juce::Rectangle<float>(
                panX, panTrack.getY(), panWidth, panTrack.getHeight()), 2.0f);
            g.setColour(juce::Colour(0xffe2e8f0));
            g.fillEllipse(panCentre + boundedPan * panTrack.getWidth() * 0.5f - 3.0f,
                          panTrack.getY() - 2.0f, 6.0f, 12.0f);
            const bool isMuted = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).mute.load()
                : matrix.getAuxBus(node.index).mute.load();
            drawToggle(g, { bounds.getX() + 160.0f, bounds.getY() + 44.0f, 40.0f, 18.0f },
                       "MUTE", isMuted, juce::Colour(0xffef4444));
            const bool isSolo = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).solo.load()
                : matrix.getAuxBus(node.index).solo.load();
            drawToggle(g, { bounds.getX() + 160.0f, bounds.getY() + 67.0f, 40.0f, 18.0f },
                       "SOLO", isSolo, juce::Colour(0xffeab308));
            const float streamSend = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).sendLevelStream.load(std::memory_order_relaxed)
                : matrix.getAuxBus(node.index).sendLevelStream.load(std::memory_order_relaxed);
            const float monitorSend = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).sendLevelMonitor.load(std::memory_order_relaxed)
                : matrix.getAuxBus(node.index).sendLevelMonitor.load(std::memory_order_relaxed);
            const bool streamEnabled = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).routeToStream.load()
                : matrix.getAuxBus(node.index).sendToStream.load();
            const bool monitorEnabled = node.type == NodeType::Submix
                ? matrix.getSubmixBus(node.index).routeToMonitor.load()
                : matrix.getAuxBus(node.index).sendToMonitor.load();
            const auto sendText = [](float level)
            {
                return level <= 0.0001f ? juce::String("-inf")
                    : juce::String(juce::Decibels::gainToDecibels(level), 0) + " dB";
            };
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xffc084fc));
            g.drawText("STR " + sendText(streamSend),
                       busSendTrack(node, 0).withY(bounds.getY() + 104.0f).withHeight(10.0f),
                       juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xff22d3ee));
            g.drawText("MON " + sendText(monitorSend),
                       busSendTrack(node, 1).withY(bounds.getY() + 104.0f).withHeight(10.0f),
                       juce::Justification::centredLeft);
            drawControlTrack(g, busSendTrack(node, 0), streamSend * 0.5f,
                             juce::Colour(0xffc084fc));
            drawControlTrack(g, busSendTrack(node, 1), monitorSend * 0.5f,
                             juce::Colour(0xff22d3ee));
            drawToggle(g, { bounds.getX() + 12.0f, bounds.getY() + 134.0f, 72.0f, 14.0f },
                       "STREAM", streamEnabled, juce::Colour(0xffc084fc));
            drawToggle(g, { bounds.getX() + 94.0f, bounds.getY() + 134.0f, 72.0f, 14.0f },
                       "MON", monitorEnabled, juce::Colour(0xff22d3ee));
            if (node.type == NodeType::Aux)
            {
                const auto& aux = matrix.getAuxBus(node.index);
                const int fxType = aux.fxType.load(std::memory_order_relaxed);
                const juce::String fxLabel = node.index == 0 ? "REVERB"
                    : node.index == 1 ? "DELAY" : "FX N/A";
                drawToggle(g, { bounds.getX() + 160.0f, bounds.getY() + 94.0f, 40.0f, 18.0f },
                           fxType == 0 ? "FX OFF" : fxLabel,
                           fxType != 0, node.index < 2 ? node.colour : juce::Colour(0xff475569));
                if (node.index == 1 && fxType == 2)
                {
                    const float delayTime = aux.fxParam1.load(std::memory_order_relaxed);
                    const float feedback = aux.fxParam2.load(std::memory_order_relaxed);
                    const auto timeTrack = auxEffectTrack(node, 0);
                    const auto feedbackTrack = auxEffectTrack(node, 1);
                    g.setFont(juce::FontOptions(7.0f, juce::Font::bold));
                    g.setColour(node.colour);
                    g.drawText("TIME " + juce::String(delayTime, 2) + " s",
                               timeTrack.withY(bounds.getY() + 148.0f).withHeight(10.0f),
                               juce::Justification::centredLeft);
                    drawControlTrack(g, timeTrack,
                        juce::jlimit(0.0f, 1.0f, (delayTime - 0.01f) / 1.49f), node.colour);
                    g.drawText("FEEDBACK " + juce::String(feedback, 2),
                               feedbackTrack.withY(bounds.getY() + 166.0f).withHeight(10.0f),
                               juce::Justification::centredLeft);
                    drawControlTrack(g, feedbackTrack,
                        juce::jlimit(0.0f, 1.0f, feedback / 0.85f), node.colour);
                }
            }
            float level = 0.0f;
            if (node.type == NodeType::Submix)
                level = juce::jmax(matrix.getSubmixBus(node.index).peakMeterL.load(),
                                   matrix.getSubmixBus(node.index).peakMeterR.load());
            else
                level = juce::jmax(matrix.getAuxBus(node.index).peakMeterL.load(),
                                   matrix.getAuxBus(node.index).peakMeterR.load());
            drawMeter(g, bounds.getX() + 12.0f, bounds.getBottom() - 3.0f,
                     bounds.getWidth() - 24.0f, 2.0f, juce::jlimit(0.0f, 1.0f, level), node.colour);
        }
        else if (node.type == NodeType::Stream || node.type == NodeType::Monitor)
        {
            const int busIndex = node.type == NodeType::Stream ? 0 : 1;
            const float level = matrix.getMasterBusPeak(busIndex);
            drawMeter(g, bounds.getX() + 12.0f, bounds.getBottom() - 9.0f,
                     bounds.getWidth() - 24.0f, 4.0f, juce::jlimit(0.0f, 1.0f, level), node.colour);
            if (masterBusControlCallbacks.getGainDb)
            {
                const float db = masterBusControlCallbacks.getGainDb(busIndex);
                drawControlTrack(g, busGainTrack(node),
                    juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 72.0f), node.colour);
                g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                g.setColour(juce::Colour(0xff94a3b8));
                g.drawText("MASTER " + (db <= -59.0f ? juce::String("-inf") : juce::String(db, 1) + " dB"),
                           juce::roundToInt(bounds.getX() + 12.0f),
                           juce::roundToInt(bounds.getY() + 43.0f), 120, 12,
                           juce::Justification::centredLeft);
                const bool busMuted = masterBusControlCallbacks.getMute &&
                    masterBusControlCallbacks.getMute(busIndex);
                drawToggle(g, { bounds.getX() + 130.0f, bounds.getY() + 53.0f, 32.0f, 18.0f },
                           "M", busMuted, juce::Colour(0xffef4444));
            }
        }
    }

    static void drawPort(juce::Graphics& g, juce::Point<float> center, juce::Colour colour)
    {
        g.setColour(juce::Colour(0xff0b1018));
        g.fillEllipse(center.x - 7.0f, center.y - 7.0f, 14.0f, 14.0f);
        g.setColour(colour);
        g.fillEllipse(center.x - 4.0f, center.y - 4.0f, 8.0f, 8.0f);
    }

    static void drawMeter(juce::Graphics& g, float x, float y, float width, float height,
                          float level, juce::Colour colour)
    {
        g.setColour(juce::Colour(0xff080c12));
        g.fillRoundedRectangle(x, y, width, height, 1.5f);
        if (level > 0.0f)
        {
            g.setColour(level > 0.9f ? juce::Colour(0xffef4444) : colour);
            g.fillRoundedRectangle(x, y, width * level, height, 1.5f);
        }
    }

    void drawProcessingLinks(juce::Graphics& g) const
    {
        const auto drawLink = [this, &g](NodeType sourceType, int sourceIndex,
                                         NodeType targetType, int targetIndex)
        {
            const int sourceIndexInGraph = findNode(sourceType, sourceIndex);
            const int targetIndexInGraph = findNode(targetType, targetIndex);
            if (sourceIndexInGraph < 0 || targetIndexInGraph < 0)
                return;
            if (!isNodeVisible(nodes[(size_t)sourceIndexInGraph]) ||
                !isNodeVisible(nodes[(size_t)targetIndexInGraph]))
                return;
            const auto& source = nodes[(size_t)sourceIndexInGraph].bounds;
            const auto& target = nodes[(size_t)targetIndexInGraph].bounds;
            g.setColour(juce::Colour(0xff64748b).withAlpha(0.75f));
            g.drawArrow({ source.getRight(), source.getCentreY(),
                          target.getX(), target.getCentreY() }, 1.4f, 5.0f, 5.0f);
        };

        const auto chainNodes = [this](int target)
        {
            std::vector<int> chain;
            const int emptyChain = findNode(NodeType::VSTChain, target);
            if (emptyChain >= 0)
                chain.push_back(emptyChain);
            else
            {
                for (int slot = 0; ; ++slot)
                {
                    const int plugin = findNode(NodeType::VSTPlugin, pluginNodeKey(target, slot));
                    if (plugin < 0)
                        break;
                    chain.push_back(plugin);
                }
            }
            return chain;
        };

        const int channelCount = matrix.getNumActiveChannels();
        for (int channel = 0; channel < channelCount; ++channel)
        {
            drawLink(NodeType::Channel, channel, NodeType::DSP, channel);
            drawLink(NodeType::DSP, channel, NodeType::Gate, channel);
            drawLink(NodeType::Gate, channel, NodeType::Equalizer, channel);
            drawLink(NodeType::Equalizer, channel, NodeType::Compressor, channel);
            const auto chain = chainNodes(channel);
            if (!chain.empty())
            {
                const auto& first = nodes[(size_t)chain.front()];
                drawLink(NodeType::Compressor, channel, first.type, first.index);
                for (size_t slot = 1; slot < chain.size(); ++slot)
                {
                    const auto& previous = nodes[(size_t)chain[slot - 1]];
                    const auto& current = nodes[(size_t)chain[slot]];
                    drawLink(previous.type, previous.index, current.type, current.index);
                }
            }
        }
        for (int bus = 0; bus < numBuses; ++bus)
        {
            const int target = maxInputs + bus;
            drawLink(NodeType::DSP, target, NodeType::Gate, target);
            drawLink(NodeType::Gate, target, NodeType::Equalizer, target);
            drawLink(NodeType::Equalizer, target, NodeType::Compressor, target);
            const auto chain = chainNodes(target);
            if (!chain.empty())
            {
                const auto& first = nodes[(size_t)chain.front()];
                const auto& last = nodes[(size_t)chain.back()];
                drawLink(NodeType::Compressor, target, first.type, first.index);
                for (size_t slot = 1; slot < chain.size(); ++slot)
                {
                    const auto& previous = nodes[(size_t)chain[slot - 1]];
                    const auto& current = nodes[(size_t)chain[slot]];
                    drawLink(previous.type, previous.index, current.type, current.index);
                }
                drawLink(last.type, last.index,
                         bus == 0 ? NodeType::Stream : NodeType::Monitor, 0);
            }
        }
    }

    std::vector<Connection> buildConnections(bool includeHiddenNodes = false) const
    {
        std::vector<Connection> result;
        const auto addByNodeIndex = [this, &result, includeHiddenNodes](
            int source, int target, juce::Colour colour, bool active)
        {
            if (source < 0 || target < 0 ||
                (!includeHiddenNodes &&
                 (!isNodeVisible(nodes[(size_t)source]) ||
                  !isNodeVisible(nodes[(size_t)target]))))
                return;
            const auto& sourceBounds = nodes[(size_t)source].bounds;
            const auto model = makeRoutingGraphConnection(
                endpointFor(nodes[(size_t)source]), endpointFor(nodes[(size_t)target]));
            result.push_back({ source, target,
                               juce::Point<float>{ sourceBounds.getRight(), sourceBounds.getCentreY() },
                               juce::Point<float>{ nodes[(size_t)target].bounds.getX(),
                                                   nodes[(size_t)target].bounds.getCentreY() },
                               colour, active, model });
        };
        const auto add = [this, &addByNodeIndex](NodeType sourceType, int sourceIndex,
                                                 NodeType targetType, int targetIndex,
                                                 juce::Colour colour, bool active)
        {
            addByNodeIndex(findNode(sourceType, sourceIndex),
                           findNode(targetType, targetIndex), colour, active);
        };

        const int channels = matrix.getNumActiveChannels();
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto& state = matrix.getChannel(channel);
            const bool canPass = state.isEnabled.load() && !state.mute.load() &&
                state.inputSource.load() != ChannelInputSource::Disabled &&
                state.isSourceAvailable.load();
            const int submix = state.submixGroup.load();
            if (submix >= 0 && submix < maxSubmixBuses)
                addByNodeIndex(chainLastNode(channel), findNode(NodeType::Submix, submix),
                               juce::Colour(0xfff59e0b), canPass);
            else
            {
                if (state.routeEnable[0].load())
                    addByNodeIndex(chainLastNode(channel), findNode(NodeType::DSP, maxInputs),
                                   juce::Colour(0xffc084fc), canPass);
                if (state.routeEnable[1].load())
                    addByNodeIndex(chainLastNode(channel), findNode(NodeType::DSP, maxInputs + 1),
                                   juce::Colour(0xff22d3ee), canPass);
            }

            for (int aux = 0; aux < maxAuxBuses; ++aux)
                if (state.auxRouteEnable[(size_t)aux].load())
                    addByNodeIndex(chainLastNode(channel), findNode(NodeType::Aux, aux),
                                   juce::Colour(0xffa78bfa),
                                   canPass && state.auxSendLevel[(size_t)aux].load() > 0.0f);
        }

        for (int submix = 0; submix < maxSubmixBuses; ++submix)
        {
            const auto& state = matrix.getSubmixBus(submix);
            const bool canPass = !state.mute.load() && state.gain.load() > 0.0f;
            if (state.routeToStream.load())
                add(NodeType::Submix, submix, NodeType::DSP, maxInputs,
                    juce::Colour(0xffc084fc), canPass);
            if (state.routeToMonitor.load())
                add(NodeType::Submix, submix, NodeType::DSP, maxInputs + 1,
                    juce::Colour(0xff22d3ee), canPass);
        }

        for (int aux = 0; aux < maxAuxBuses; ++aux)
        {
            const auto& state = matrix.getAuxBus(aux);
            const bool canPass = !state.mute.load() && state.returnLevel.load() > 0.0f;
            if (state.sendToStream.load())
                add(NodeType::Aux, aux, NodeType::DSP, maxInputs,
                    juce::Colour(0xffc084fc), canPass);
            if (state.sendToMonitor.load())
                add(NodeType::Aux, aux, NodeType::DSP, maxInputs + 1,
                    juce::Colour(0xff22d3ee), canPass);
        }
        return result;
    }

    int chainLastNode(int targetIndex) const
    {
        const int emptyChain = findNode(NodeType::VSTChain, targetIndex);
        if (emptyChain >= 0)
            return emptyChain;

        int lastPlugin = -1;
        for (int slot = 0; ; ++slot)
        {
            const int plugin = findNode(NodeType::VSTPlugin, pluginNodeKey(targetIndex, slot));
            if (plugin < 0)
                break;
            lastPlugin = plugin;
        }
        return lastPlugin;
    }

    void drawConnection(juce::Graphics& g, const Connection& connection, bool showLevel)
    {
        const float controlOffset = juce::jmax(42.0f, std::abs(connection.end.x - connection.start.x) * 0.45f);
        juce::Path path;
        path.startNewSubPath(connection.start);
        path.cubicTo(connection.start.x + controlOffset, connection.start.y,
                     connection.end.x - controlOffset, connection.end.y,
                     connection.end.x, connection.end.y);
        g.setColour(connection.active ? connection.colour.withAlpha(0.82f)
                                      : connection.colour.withAlpha(0.28f));
        g.strokePath(path, juce::PathStrokeType(connection.active ? 2.5f : 1.5f));
        drawPort(g, connection.end, connection.colour.withAlpha(connection.active ? 1.0f : 0.45f));

        if (showLevel && connection.active)
        {
            const auto labelBounds = juce::Rectangle<float>(
                (connection.start.x + connection.end.x) * 0.5f - 21.0f,
                (connection.start.y + connection.end.y) * 0.5f - 9.0f, 42.0f, 16.0f);
            g.setColour(juce::Colour(0xee0b1018));
            g.fillRoundedRectangle(labelBounds, 3.0f);
            g.setColour(connection.colour);
            g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
            g.drawText(connectionLabel(connection), labelBounds, juce::Justification::centred);
        }
    }

    juce::String connectionLabel(const Connection& connection) const
    {
        const auto sourceType = connection.model.sourceNode.type;
        const int sourceIndex = connection.model.sourceNode.index;
        const auto targetType = connection.model.destinationNode.type;
        const int targetIndex = connection.model.destinationNode.index;
        if (sourceType == RoutingGraphNodeType::Channel)
        {
            const auto& channel = matrix.getChannel(sourceIndex);
            if (targetType == RoutingGraphNodeType::Stream)
                return gainLabel(channel.sendLevel[0].load());
            if (targetType == RoutingGraphNodeType::Monitor)
                return gainLabel(channel.sendLevel[1].load());
            if (targetType == RoutingGraphNodeType::Aux)
                return gainLabel(channel.auxSendLevel[(size_t)targetIndex].load());
        }
        return {};
    }

    static juce::String gainLabel(float gain)
    {
        return gain <= 0.0001f ? "-inf" : juce::String(juce::Decibels::gainToDecibels(gain), 0) + " dB";
    }

    void drawTemporaryConnection(juce::Graphics& g)
    {
        if (connectionSource < 0 || connectionSource >= (int)nodes.size())
            return;
        const auto& sourceBounds = nodes[(size_t)connectionSource].bounds;
        const juce::Point<float> start{ sourceBounds.getRight(), sourceBounds.getCentreY() };
        juce::Path path;
        path.startNewSubPath(start);
        const float controlOffset = juce::jmax(42.0f, std::abs(connectionEnd.x - start.x) * 0.45f);
        path.cubicTo(start.x + controlOffset, start.y,
                     connectionEnd.x - controlOffset, connectionEnd.y,
                     connectionEnd.x, connectionEnd.y);
        g.setColour(juce::Colour(0xfff8fafc).withAlpha(0.85f));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    int findNode(NodeType type, int index) const
    {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].type == type && nodes[i].index == index)
                return (int)i;
        return -1;
    }

    int findNode(const NodeIdentity& identity) const
    {
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto& node = nodes[i];
            if (node.type == identity.type && node.index == identity.index &&
                node.targetIndex == identity.targetIndex && node.slotIndex == identity.slotIndex)
                return (int)i;
        }
        return -1;
    }

    void connectNodes(const RoutingGraphConnection& connection)
    {
        const auto result = connectRoutingGraphConnection(matrix, connection);
        reportConnectionStatus(result == RoutingGraphEditResult::AlreadyConnected
            ? "Route already connected."
            : result == RoutingGraphEditResult::Updated
                ? "Routing updated in the audio engine."
                : "Connection rejected: this route is not supported.");
    }

    void disconnect(const RoutingGraphConnection& connection)
    {
        disconnectRoutingGraphConnection(matrix, connection);
    }

    float distanceToConnection(const Connection& connection, juce::Point<float> point) const
    {
        const float controlOffset = juce::jmax(
            42.0f, std::abs(connection.end.x - connection.start.x) * 0.45f);
        const juce::Point<float> control1{ connection.start.x + controlOffset, connection.start.y };
        const juce::Point<float> control2{ connection.end.x - controlOffset, connection.end.y };
        auto previous = connection.start;
        float closestDistance = std::numeric_limits<float>::max();

        for (int step = 1; step <= 32; ++step)
        {
            const float t = (float)step / 32.0f;
            const float inverseT = 1.0f - t;
            const auto current =
                connection.start * (inverseT * inverseT * inverseT) +
                control1 * (3.0f * inverseT * inverseT * t) +
                control2 * (3.0f * inverseT * t * t) +
                connection.end * (t * t * t);
            const auto segment = current - previous;
            const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
            const auto relative = point - previous;
            const float projection = lengthSquared > 0.0f
                ? juce::jlimit(0.0f, 1.0f,
                    (relative.x * segment.x + relative.y * segment.y) / lengthSquared)
                : 0.0f;
            closestDistance = juce::jmin(
                closestDistance, point.getDistanceFrom(previous + segment * projection));
            previous = current;
        }
        return closestDistance;
    }

    void disconnectNearest(juce::Point<float> point)
    {
        auto connections = buildConnections();
        float closestDistance = 10.0f;
        int closestIndex = -1;
        for (size_t i = 0; i < connections.size(); ++i)
        {
            const float distance = distanceToConnection(connections[i], point);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestIndex = (int)i;
            }
        }
        if (closestIndex >= 0)
        {
            const auto& connection = connections[(size_t)closestIndex];
            disconnect(connection.model);
            repaint();
        }
    }

    void disconnectSelectedNodeConnections()
    {
        if (selectedNode < 0 || selectedNode >= (int)nodes.size())
            return;
        const auto selected = nodes[(size_t)selectedNode];
        const auto connections = buildConnections();
        for (const auto& connection : connections)
        {
            const bool selectedVisualEndpoint =
                connection.sourceNode == selectedNode || connection.targetNode == selectedNode;
            const bool selectedChannelSource = selected.type == NodeType::Channel &&
                connection.model.sourceNode.type == RoutingGraphNodeType::Channel &&
                connection.model.sourceNode.index == selected.index;
            if (selectedVisualEndpoint || selectedChannelSource)
                disconnect(connection.model);
        }
        repaint();
    }

    RoutingMatrix& matrix;
    std::function<void(int)> channelSelected;
    std::function<void(const juce::String&)> connectionStatus;
    std::function<void(int)> dspNodeSelected;
    std::function<void(int)> vstChainSelected;
    RoutingGraphComponent::VSTChainPluginsProvider vstChainPluginsProvider;
    RoutingGraphComponent::MasterBusControlCallbacks masterBusControlCallbacks;
    RoutingGraphComponent::DSPControlCallbacks dspControlCallbacks;
    std::function<void(int, int)> panHandler;
    std::function<void()> focusChangedHandler;
    std::vector<Node> nodes;
    std::vector<SavedNodePosition> unfocusedNodePositions;
    float graphHeight = 620.0f;
    float submixX = 850.0f;
    float busDSPX = 1120.0f;
    float busVSTX = 1380.0f;
    float busOutputX = 1640.0f;
    float zoom = 1.0f;
    int selectedNode = -1;
    std::optional<NodeIdentity> pressedNodeIdentity;
    int connectionSource = -1;
    juce::Point<float> connectionEnd;
    juce::Point<float> dragOffset;
    juce::Point<float> nodeDragStartPosition;
    juce::Point<int> lastMousePosition;
    bool draggingNode = false;
    bool nodeDragMoved = false;
    bool draggingConnection = false;
    bool draggingControl = false;
    bool panning = false;
    bool suppressNodeSelection = false;
    bool focusedView = false;
    NodeType focusedRootType = NodeType::Channel;
    int focusedRootIndex = -1;
    NodeType focusedAnchorType = NodeType::Channel;
    int focusedAnchorIndex = -1;
    std::vector<int> focusedChannelIndices;
    NodeControlMode controlMode = NodeControlMode::None;
};

RoutingGraphComponent::RoutingGraphComponent(RoutingMatrix& routingMatrix,
                                             std::function<void(int)> onChannelSelected,
                                             std::function<void()> onSignalFlowRequested,
                                             std::function<void(juce::Component*)> onAddInputRequested,
                                             std::function<void(int)> onDSPNodeSelected,
                                             std::function<void(int)> onVSTChainSelected,
                                             VSTChainPluginsProvider vstChainPluginsProviderIn)
    : matrix(routingMatrix), channelSelected(std::move(onChannelSelected)),
      dspNodeSelected(std::move(onDSPNodeSelected)),
      vstChainSelected(std::move(onVSTChainSelected)),
      vstChainPluginsProvider(std::move(vstChainPluginsProviderIn)),
      canvas(std::make_unique<RoutingGraphCanvas>(matrix, [this](int channelIndex)
      {
          if (channelSelected)
              channelSelected(channelIndex);
      }, [this](const juce::String& message)
      {
          helpLabel.setText(message, juce::dontSendNotification);
      }, [this](int targetIndex)
      {
          if (dspNodeSelected)
              dspNodeSelected(targetIndex);
      }, [this](int targetIndex)
      {
          if (vstChainSelected)
              vstChainSelected(targetIndex);
      }, vstChainPluginsProvider))
{
    addInputCallback = std::move(onAddInputRequested);
    setOpaque(true);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(canvas.get(), false);
    viewport.setScrollBarsShown(true, true);
    canvas->setPanHandler([this](int dx, int dy)
    {
        viewport.setViewPosition(juce::jmax(0, viewport.getViewPositionX() - dx),
                                 juce::jmax(0, viewport.getViewPositionY() - dy));
        viewPositionX = viewport.getViewPositionX();
        viewPositionY = viewport.getViewPositionY();
    });
    canvas->setFocusChangedHandler([this] { fitToView(); });

    for (auto* button : { &zoomOutButton, &zoomInButton, &fitButton, &addNodeButton,
                          &signalFlowButton, &showAllButton })
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        addAndMakeVisible(button);
    }
    zoomOutButton.onClick = [this] { setZoom(zoom * 0.85f); };
    zoomInButton.onClick = [this] { setZoom(zoom * 1.18f); };
    fitButton.onClick = [this] { fitToView(); };
    showAllButton.onClick = [this]
    {
        if (canvas != nullptr)
            canvas->showAllNodes();
        fitToView();
    };
    addNodeButton.onClick = [this]
    {
        if (matrix.getNumActiveChannels() >= maxInputs)
        {
            helpLabel.setText("All input channel nodes are already active.", juce::dontSendNotification);
            return;
        }
        if (addInputCallback)
            addInputCallback(&addNodeButton);
    };
    addNodeButton.setTooltip("Add an active mixer input node to the graph");
    addNodeButton.setEnabled(matrix.getNumActiveChannels() < maxInputs);
    signalFlowButton.onClick = [callback = std::move(onSignalFlowRequested)]
    {
        if (callback)
            callback();
    };

    helpLabel.setText("Click a node to focus its route; in channel focus, click other channels to add or remove them. Click empty space to clear selection; use Show All to restore the full graph. Click DSP / EQ or VST3 to edit.",
                      juce::dontSendNotification);
    helpLabel.setFont(juce::FontOptions(10.0f));
    helpLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(helpLabel);

    paletteSearch.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111827));
    paletteSearch.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
    paletteSearch.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff334155));
    paletteSearch.setTextToShowWhenEmpty("Search nodes...", juce::Colour(0xff64748b));
    paletteSearch.onTextChange = [this] { rebuildPaletteItems(); };
    addAndMakeVisible(paletteSearch);

    for (int category = 0; category < (int)paletteCategoryButtons.size(); ++category)
    {
        auto& button = paletteCategoryButtons[(size_t)category];
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffe2e8f0));
        button.setClickingTogglesState(true);
        button.onClick = [this, category]
        {
            selectedPaletteCategory = category;
            rebuildPaletteItems();
        };
        addAndMakeVisible(button);
    }
    paletteViewport.setViewedComponent(&paletteListContent, false);
    paletteViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(paletteViewport);

    dockViewport.setViewedComponent(&dockContent, false);
    dockViewport.setScrollBarsShown(false, true);
    addAndMakeVisible(dockViewport);
    rebuildPaletteItems();
    rebuildDockItems();
}

RoutingGraphComponent::~RoutingGraphComponent() = default;

void RoutingGraphComponent::rebuildPaletteItems()
{
    if (paletteItems.empty())
    {
        paletteItems = {
            { "Add Input Channel", "Create an active input channel in the mixer.", 0,
              RoutingGraphPaletteAction::AddInput, true },
            { "YouTube Capture", "Open YouTube capture and source settings.", 0,
              RoutingGraphPaletteAction::YouTubeCapture, true },
            { "Gate", "Focus the channel or bus Gate processor card.", 1,
              RoutingGraphPaletteAction::Gate, true },
            { "EQ Curve", "Focus the parametric EQ curve processor card.", 1,
              RoutingGraphPaletteAction::Equalizer, true },
            { "Compressor", "Focus the Compressor processor card.", 1,
              RoutingGraphPaletteAction::Compressor, true },
            { "VST3 FX Chain", "Open the insert chain for the selected channel or bus.", 1,
              RoutingGraphPaletteAction::VST3Chain, true },
            { "Submix Bus", "Select a Submix routing node.", 2,
              RoutingGraphPaletteAction::Submix, true },
            { "Aux Return", "Select an Aux return node.", 2,
              RoutingGraphPaletteAction::Aux, true },
            { "Stream Bus", "Select the Stream bus output node.", 2,
              RoutingGraphPaletteAction::StreamBus, true },
            { "Monitor Bus", "Select the Monitor bus output node.", 2,
              RoutingGraphPaletteAction::MonitorBus, true },
            { "Virtual Cable Out", "Open OBS output routing and virtual-device status.", 3,
              RoutingGraphPaletteAction::VirtualCable, true },
            { "OBS / RTMP", "Open OBS integration and stream routing.", 3,
              RoutingGraphPaletteAction::OBS, true },
            { "Audio Device", "Open active audio-device configuration.", 3,
              RoutingGraphPaletteAction::AudioDevice, true },
            { "Record Stream Bus", "Start or stop recording the post-safety Stream Bus to WAV.", 3,
              RoutingGraphPaletteAction::Recorder, true }
        };

        paletteItemButtons.reserve(paletteItems.size());
        for (size_t index = 0; index < paletteItems.size(); ++index)
        {
            auto button = std::make_unique<juce::TextButton>(paletteItems[index].label);
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff172131));
            button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
            button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff164e63));
            button->setTooltip(paletteItems[index].tooltip);
            button->setEnabled(paletteItems[index].enabled);
            button->onClick = [this, index]
            {
                const auto action = paletteItems[index].action;
                if (action == RoutingGraphPaletteAction::AddInput)
                {
                    if (matrix.getNumActiveChannels() >= maxInputs)
                    {
                        helpLabel.setText("All input channel nodes are already active.",
                                          juce::dontSendNotification);
                        return;
                    }
                    if (addInputCallback)
                        addInputCallback(&paletteSearch);
                    return;
                }
                if (onPaletteAction)
                    onPaletteAction(action);
                else
                    helpLabel.setText(paletteItems[index].tooltip, juce::dontSendNotification);
            };
            paletteListContent.addAndMakeVisible(button.get());
            paletteItemButtons.push_back(std::move(button));
        }
    }

    const auto query = paletteSearch.getText().trim();
    int visibleCount = 0;
    for (size_t i = 0; i < paletteItems.size(); ++i)
    {
        const auto& item = paletteItems[i];
        const bool visible = item.category == selectedPaletteCategory &&
            (query.isEmpty() || item.label.containsIgnoreCase(query) ||
             item.tooltip.containsIgnoreCase(query));
        auto* button = paletteItemButtons[i].get();
        button->setVisible(visible);
        button->setEnabled(item.enabled &&
            (item.action != RoutingGraphPaletteAction::AddInput ||
             matrix.getNumActiveChannels() < maxInputs));
        if (visible)
        {
            button->setBounds(8, 8 + visibleCount * 36, 174, 30);
            ++visibleCount;
        }
    }
    paletteListContent.setSize(190, juce::jmax(8 + visibleCount * 36, 250));
    for (int i = 0; i < (int)paletteCategoryButtons.size(); ++i)
        paletteCategoryButtons[(size_t)i].setToggleState(i == selectedPaletteCategory,
                                                          juce::dontSendNotification);
    paletteViewport.repaint();
}

void RoutingGraphComponent::rebuildDockItems()
{
    for (auto& button : dockButtons)
        dockContent.removeChildComponent(button.get());
    dockButtons.clear();
    dockDimButtons.fill(nullptr);
    dockMuteButtons.fill(nullptr);

    int x = 8;
    const auto addButton = [this, &x](const juce::String& text, const juce::String& tooltip,
                                      int width, std::function<void(juce::TextButton*)> onClick)
    {
        auto button = std::make_unique<juce::TextButton>(text);
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff172131));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff164e63));
        button->setTooltip(tooltip);
        button->setBounds(x, 8, width, 32);
        x += width + 6;
        auto* rawButton = button.get();
        rawButton->onClick = [onClick, rawButton] { if (onClick) onClick(rawButton); };
        dockContent.addAndMakeVisible(rawButton);
        dockButtons.push_back(std::move(button));
    };

    for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
    {
        addButton("CH " + juce::String(channel + 1), "Select input channel " +
                  juce::String(channel + 1), 56,
            [this, channel](juce::TextButton*)
            {
                focusNode("channel", channel);
                if (channelSelected)
                    channelSelected(channel);
            });
    }
    for (int submix = 0; submix < maxSubmixBuses; ++submix)
    {
        addButton("SUB " + juce::String(submix + 1), "Focus Submix " +
                  juce::String(submix + 1), 62,
            [this, submix](juce::TextButton*)
            {
                canvas->focusNode("submix", submix, viewport.getViewPositionX(),
                                  viewport.getViewPositionY(), viewport.getWidth(),
                                  viewport.getHeight());
            });
    }
    for (int aux = 0; aux < maxAuxBuses; ++aux)
    {
        addButton("AUX " + juce::String(aux + 1), "Focus Aux " +
                  juce::String(aux + 1), 62,
            [this, aux](juce::TextButton*)
            {
                canvas->focusNode("aux", aux, viewport.getViewPositionX(),
                                  viewport.getViewPositionY(), viewport.getWidth(),
                                  viewport.getHeight());
            });
    }

    for (int bus = 0; bus < numBuses; ++bus)
    {
        const auto busName = bus == 0 ? juce::String("STREAM") : juce::String("MONITOR");
        addButton(busName, "Focus " + busName + " bus output", 78,
            [this, bus](juce::TextButton*)
            {
                canvas->focusNode(bus == 0 ? "stream" : "monitor", 0,
                    viewport.getViewPositionX(), viewport.getViewPositionY(),
                    viewport.getWidth(), viewport.getHeight());
                if (dspNodeSelected)
                    dspNodeSelected(maxInputs + bus);
            });
    }
    for (int bus = 0; bus < numBuses; ++bus)
    {
        const auto busName = bus == 0 ? juce::String("STREAM") : juce::String("MONITOR");
        addButton("DIM " + busName, "Toggle 20 dB master dim for " + busName, 80,
            [this, bus](juce::TextButton* button)
            {
                if (masterBusControlCallbacks.setDim)
                    masterBusControlCallbacks.setDim(bus, button->getToggleState());
            });
        dockDimButtons[(size_t)bus] = dockButtons.back().get();
        dockDimButtons[(size_t)bus]->setClickingTogglesState(true);
        addButton("MUTE " + busName, "Toggle master mute for " + busName, 88,
            [this, bus](juce::TextButton* button)
            {
                if (masterBusControlCallbacks.setMute)
                    masterBusControlCallbacks.setMute(bus, button->getToggleState());
            });
        dockMuteButtons[(size_t)bus] = dockButtons.back().get();
        dockMuteButtons[(size_t)bus]->setClickingTogglesState(true);
    }

    dockContent.setSize(x + 8, 48);
    dockChannelCount = matrix.getNumActiveChannels();
    refresh();
}

juce::String RoutingGraphComponent::serializeLayout() const
{
    juce::XmlElement layout("ROUTING_GRAPH_LAYOUT");
    layout.setAttribute("version", 2);
    const bool focused = canvas != nullptr && canvas->isFocusedView();
    layout.setAttribute("zoom", (double)(focused ? preFocusZoom : zoom));
    layout.setAttribute("viewX", focused ? preFocusViewX : viewport.getViewPositionX());
    layout.setAttribute("viewY", focused ? preFocusViewY : viewport.getViewPositionY());
    if (canvas != nullptr)
    {
        layout.addChildElement(canvas->serializePositions().release());
        layout.addChildElement(canvas->serializeConnections().release());
    }
    return layout.toString();
}

bool RoutingGraphComponent::restoreLayout(const juce::String& xmlText)
{
    if (canvas == nullptr)
        return false;
    canvas->showAllNodes();
    hasFocusedViewSnapshot = false;

    if (xmlText.isEmpty())
    {
        resetLayout();
        helpLabel.setText("Click a node to focus its route; in channel focus, click other channels to add or remove them. Click empty space to clear selection; use Show All to restore the full graph. Click DSP / EQ or VST3 to edit.",
                          juce::dontSendNotification);
        return true;
    }

    auto layout = juce::XmlDocument::parse(xmlText);
    const int layoutVersion = layout != nullptr ? layout->getIntAttribute("version", 0) : 0;
    if (layout == nullptr || !layout->hasTagName("ROUTING_GRAPH_LAYOUT") ||
        (layoutVersion != 1 && layoutVersion != 2))
    {
        resetLayout();
        helpLabel.setText("Saved graph layout is invalid; using the default layout.",
                          juce::dontSendNotification);
        return false;
    }

    auto* nodes = layout->getChildByName("NODES");
    if (nodes == nullptr || !canvas->restorePositions(*nodes))
    {
        resetLayout();
        helpLabel.setText("Saved graph nodes are invalid; using the default layout.",
                          juce::dontSendNotification);
        return false;
    }

    if (layoutVersion >= 2)
    {
        auto* connections = layout->getChildByName("CONNECTIONS");
        if (connections == nullptr || !canvas->validateSerializedConnections(*connections))
        {
            resetLayout();
            helpLabel.setText("Saved graph connections do not match the restored routing state.",
                              juce::dontSendNotification);
            return false;
        }
    }

    const float restoredZoom = (float)layout->getDoubleAttribute("zoom", 1.0);
    if (!std::isfinite(restoredZoom))
    {
        resetLayout();
        helpLabel.setText("Saved graph zoom is invalid; using the default layout.",
                          juce::dontSendNotification);
        return false;
    }
    zoom = juce::jlimit(0.45f, 1.5f, restoredZoom);
    viewPositionX = juce::jmax(0, layout->getIntAttribute("viewX", 0));
    viewPositionY = juce::jmax(0, layout->getIntAttribute("viewY", 0));
    hasRestoredLayout = true;
    canvas->setZoom(zoom);
    viewport.setViewPosition(viewPositionX, viewPositionY);
    return true;
}

void RoutingGraphComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b1018));
}

void RoutingGraphComponent::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop(32);
    fitButton.setBounds(toolbar.removeFromRight(54).reduced(2));
    showAllButton.setBounds(toolbar.removeFromRight(78).reduced(2));
    signalFlowButton.setBounds(toolbar.removeFromRight(92).reduced(2));
    addNodeButton.setBounds(toolbar.removeFromRight(82).reduced(2));
    zoomInButton.setBounds(toolbar.removeFromRight(36).reduced(2));
    zoomOutButton.setBounds(toolbar.removeFromRight(36).reduced(2));
    helpLabel.setBounds(toolbar.reduced(4, 0));

    auto dockArea = area.removeFromBottom(54);
    dockViewport.setBounds(dockArea.reduced(0, 2));

    auto paletteArea = area.removeFromLeft(194);
    paletteSearch.setBounds(paletteArea.getX() + 8, paletteArea.getY() + 8,
                            paletteArea.getWidth() - 16, 26);
    auto categories = juce::Rectangle<int>(paletteArea.getX() + 8, paletteArea.getY() + 40,
                                            paletteArea.getWidth() - 16, 48);
    const int categoryWidth = categories.getWidth() / 2;
    for (int category = 0; category < (int)paletteCategoryButtons.size(); ++category)
        paletteCategoryButtons[(size_t)category].setBounds(
            categories.getX() + (category % 2) * categoryWidth,
            categories.getY() + (category / 2) * 24,
            categoryWidth - 2, 22);
    paletteViewport.setBounds(paletteArea.withTrimmedTop(94).reduced(2, 0));

    viewport.setBounds(area.reduced(4, 0));
    viewport.setViewPosition(viewPositionX, viewPositionY);
    if (!hasRestoredLayout && zoom == 1.0f && canvas != nullptr)
        fitToView();
}

void RoutingGraphComponent::refresh()
{
    if (canvas != nullptr)
        canvas->repaint();
    addNodeButton.setEnabled(matrix.getNumActiveChannels() < maxInputs);
    rebuildPaletteItems();
    if (dockChannelCount != matrix.getNumActiveChannels())
        rebuildDockItems();
    for (int bus = 0; bus < numBuses; ++bus)
    {
        if (dockDimButtons[(size_t)bus] != nullptr && masterBusControlCallbacks.getDim)
            dockDimButtons[(size_t)bus]->setToggleState(
                masterBusControlCallbacks.getDim(bus), juce::dontSendNotification);
        if (dockMuteButtons[(size_t)bus] != nullptr && masterBusControlCallbacks.getMute)
            dockMuteButtons[(size_t)bus]->setToggleState(
                masterBusControlCallbacks.getMute(bus), juce::dontSendNotification);
    }
}

void RoutingGraphComponent::setMasterBusControlCallbacks(MasterBusControlCallbacks callbacks)
{
    masterBusControlCallbacks = callbacks;
    if (canvas != nullptr)
        canvas->setMasterBusControlCallbacks(callbacks);
    refresh();
}

void RoutingGraphComponent::setDSPControlCallbacks(DSPControlCallbacks callbacks)
{
    if (canvas != nullptr)
        canvas->setDSPControlCallbacks(std::move(callbacks));
}

void RoutingGraphComponent::setPaletteActionCallback(
    std::function<void(RoutingGraphPaletteAction)> callback)
{
    onPaletteAction = std::move(callback);
}

void RoutingGraphComponent::focusNode(const juce::String& typeId, int index)
{
    if (canvas != nullptr)
        canvas->focusNode(typeId, index, viewport.getViewPositionX(),
                          viewport.getViewPositionY(), viewport.getWidth(),
                          viewport.getHeight());
}

void RoutingGraphComponent::showAllNodes()
{
    if (canvas != nullptr)
        canvas->showAllNodes();
    fitToView();
}

void RoutingGraphComponent::setZoom(float newZoom)
{
    zoom = juce::jlimit(0.45f, 1.5f, newZoom);
    canvas->setZoom(zoom);
    viewPositionX = viewport.getViewPositionX();
    viewPositionY = viewport.getViewPositionY();
}

void RoutingGraphComponent::resetLayout()
{
    canvas->resetPositions();
    zoom = 1.0f;
    viewPositionX = 0;
    viewPositionY = 0;
    hasRestoredLayout = false;
    canvas->setZoom(zoom);
    viewport.setViewPosition(0, 0);
}

void RoutingGraphComponent::fitToView()
{
    const bool isFocused = canvas != nullptr && canvas->isFocusedView();
    if (isFocused && !hasFocusedViewSnapshot)
    {
        preFocusZoom = zoom;
        preFocusViewX = viewport.getViewPositionX();
        preFocusViewY = viewport.getViewPositionY();
        hasFocusedViewSnapshot = true;
    }
    else if (!isFocused)
    {
        hasFocusedViewSnapshot = false;
    }
    canvas->fitTo((float)viewport.getWidth(), (float)viewport.getHeight());
    zoom = canvas->getZoom();
    viewPositionX = 0;
    viewPositionY = 0;
    hasRestoredLayout = false;
    viewport.setViewPosition(0, 0);
}

} // namespace LiveBus
