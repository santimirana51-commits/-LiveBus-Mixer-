#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>

namespace LiveBus
{

/**
 * Metadata and user preferences for a single plug-in in the library.
 */
struct PluginInfoItem
{
    juce::PluginDescription description;
    bool isFavorite{false};
    bool isDefault{false};
    juce::String profile{"Factory"}; // "Factory", "User", "Custom"
    juce::String userTag;
    bool isEnabled{true};
    juce::String lastError;
    int latencySamples{0};

    juce::String getDisplayName() const
    {
        return description.name.isNotEmpty() ? description.name : description.fileOrIdentifier;
    }

    juce::String getFormat() const
    {
        return description.pluginFormatName.isNotEmpty() ? description.pluginFormatName : "VST3";
    }

    juce::String getCategory() const
    {
        return description.category.isNotEmpty() ? description.category : "Fx|Uncategorized";
    }

    juce::String getManufacturer() const
    {
        return description.manufacturerName.isNotEmpty() ? description.manufacturerName : "Unknown";
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("PluginItem");
        xml->setAttribute("name", description.name);
        xml->setAttribute("id", description.fileOrIdentifier);
        xml->setAttribute("format", description.pluginFormatName);
        xml->setAttribute("category", description.category);
        xml->setAttribute("manufacturer", description.manufacturerName);
        xml->setAttribute("version", description.version);
        xml->setAttribute("isInstrument", description.isInstrument);
        xml->setAttribute("favorite", isFavorite);
        xml->setAttribute("default", isDefault);
        xml->setAttribute("profile", profile);
        xml->setAttribute("userTag", userTag);
        xml->setAttribute("enabled", isEnabled);
        return xml;
    }

    static PluginInfoItem fromXml(const juce::XmlElement& xml)
    {
        PluginInfoItem item;
        item.description.name = xml.getStringAttribute("name", "Unknown Plugin");
        item.description.fileOrIdentifier = xml.getStringAttribute("id", "");
        item.description.pluginFormatName = xml.getStringAttribute("format", "VST3");
        item.description.category = xml.getStringAttribute("category", "Fx|Uncategorized");
        item.description.manufacturerName = xml.getStringAttribute("manufacturer", "Unknown");
        item.description.version = xml.getStringAttribute("version", "1.0");
        item.description.isInstrument = xml.getBoolAttribute("isInstrument", false);
        item.isFavorite = xml.getBoolAttribute("favorite", false);
        item.isDefault = xml.getBoolAttribute("default", false);
        item.profile = xml.getStringAttribute("profile", "Factory");
        item.userTag = xml.getStringAttribute("userTag", "");
        item.isEnabled = xml.getBoolAttribute("enabled", true);
        return item;
    }
};

/**
 * Represents a single loaded plug-in slot in a processing chain.
 */
struct PluginChainSlot
{
    juce::String instanceId;
    juce::String pluginName;
    juce::String fileOrIdentifier;
    bool isBypassed{false};
    bool isIsolatedMode{false}; // In-Process vs Out-of-Process
    int latencySamples{0};
    juce::MemoryBlock savedState;

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("Slot");
        xml->setAttribute("instanceId", instanceId);
        xml->setAttribute("name", pluginName);
        xml->setAttribute("id", fileOrIdentifier);
        xml->setAttribute("bypassed", isBypassed);
        xml->setAttribute("isolated", isIsolatedMode);
        xml->setAttribute("latency", latencySamples);
        if (savedState.getSize() > 0)
            xml->setAttribute("stateBlob", savedState.toBase64Encoding());
        return xml;
    }

    static PluginChainSlot fromXml(const juce::XmlElement& xml)
    {
        PluginChainSlot slot;
        slot.instanceId = xml.getStringAttribute("instanceId", juce::Uuid().toString());
        slot.pluginName = xml.getStringAttribute("name", "Unknown");
        slot.fileOrIdentifier = xml.getStringAttribute("id", "");
        slot.isBypassed = xml.getBoolAttribute("bypassed", false);
        slot.isIsolatedMode = xml.getBoolAttribute("isolated", false);
        slot.latencySamples = xml.getIntAttribute("latency", 0);
        auto b64 = xml.getStringAttribute("stateBlob", "");
        if (b64.isNotEmpty())
            slot.savedState.fromBase64Encoding(b64);
        return slot;
    }
};

/**
 * Scan settings and path configuration.
 */
struct PluginScanSettings
{
    juce::StringArray customScanPaths;
    bool scanStandardVst3{true};
    bool rescanFailedPlugins{false};

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("ScanSettings");
        xml->setAttribute("scanStandardVst3", scanStandardVst3);
        xml->setAttribute("rescanFailed", rescanFailedPlugins);
        auto* pathsXml = xml->createNewChildElement("CustomPaths");
        for (const auto& p : customScanPaths)
        {
            auto* pNode = pathsXml->createNewChildElement("Path");
            pNode->setAttribute("dir", p);
        }
        return xml;
    }

    static PluginScanSettings fromXml(const juce::XmlElement& xml)
    {
        PluginScanSettings s;
        s.scanStandardVst3 = xml.getBoolAttribute("scanStandardVst3", true);
        s.rescanFailedPlugins = xml.getBoolAttribute("rescanFailed", false);
        if (auto* pathsXml = xml.getChildByName("CustomPaths"))
        {
            for (auto* c = pathsXml->getFirstChildElement(); c != nullptr; c = c->getNextElement())
            {
                if (c->hasTagName("Path"))
                    s.customScanPaths.add(c->getStringAttribute("dir"));
            }
        }
        return s;
    }
};

} // namespace LiveBus
