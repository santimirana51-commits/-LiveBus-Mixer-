#include "MacroEngine.h"
#include <cmath>

namespace LiveBus
{

float MacroAssignment::mapNormalizedValue(float normVal) const noexcept
{
    float v = juce::jlimit(0.0f, 1.0f, normVal);
    if (invert)
        v = 1.0f - v;

    float curved = v;
    switch (curve)
    {
        case MacroCurveType::Logarithmic:
            // Logarithmic shape for frequency or attenuation
            curved = (std::exp(v * 3.0f) - 1.0f) / (std::exp(3.0f) - 1.0f);
            break;
        case MacroCurveType::Exponential:
            // S-curve / exponential
            curved = std::pow(v, 2.5f);
            break;
        case MacroCurveType::Inverted:
            curved = 1.0f - v;
            break;
        case MacroCurveType::Linear:
        default:
            curved = v;
            break;
    }

    return minVal + curved * (maxVal - minVal);
}

MacroEngine::MacroEngine()
{
    loadFactoryDefaults();
}

const MacroDefinition* MacroEngine::getMacro(int index) const noexcept
{
    if (index >= 0 && index < (int)macros.size())
        return &macros[(size_t)index];
    return nullptr;
}

MacroDefinition* MacroEngine::getMacro(int index) noexcept
{
    if (index >= 0 && index < (int)macros.size())
        return &macros[(size_t)index];
    return nullptr;
}

const MacroDefinition* MacroEngine::getMacroById(int id) const noexcept
{
    for (const auto& m : macros)
        if (m.id == id) return &m;
    return nullptr;
}

MacroDefinition* MacroEngine::getMacroById(int id) noexcept
{
    for (auto& m : macros)
        if (m.id == id) return &m;
    return nullptr;
}

void MacroEngine::setMacroValue(int macroIndex, float normalizedValue, bool notify)
{
    if (macroIndex < 0 || macroIndex >= (int)macros.size()) return;

    float clamped = juce::jlimit(0.0f, 1.0f, normalizedValue);
    macros[(size_t)macroIndex].currentValue = clamped;

    applyAssignments(macroIndex);

    if (notify)
    {
        listeners.call([macroIndex, clamped](Listener& l) {
            l.onMacroValueChanged(macroIndex, clamped);
        });
    }
}

float MacroEngine::getMacroValue(int macroIndex) const noexcept
{
    if (macroIndex >= 0 && macroIndex < (int)macros.size())
        return macros[(size_t)macroIndex].currentValue;
    return 0.0f;
}

void MacroEngine::addAssignment(int macroIndex, const MacroAssignment& assignment)
{
    if (macroIndex < 0 || macroIndex >= (int)macros.size()) return;
    macros[(size_t)macroIndex].assignments.push_back(assignment);
    applyAssignments(macroIndex);
    listeners.call([](Listener& l) { l.onMacroStructureChanged(); });
}

void MacroEngine::removeAssignment(int macroIndex, int assignmentIndex)
{
    if (macroIndex < 0 || macroIndex >= (int)macros.size()) return;
    auto& list = macros[(size_t)macroIndex].assignments;
    if (assignmentIndex >= 0 && assignmentIndex < (int)list.size())
    {
        list.erase(list.begin() + assignmentIndex);
        listeners.call([](Listener& l) { l.onMacroStructureChanged(); });
    }
}

void MacroEngine::updateAssignment(int macroIndex, int assignmentIndex, const MacroAssignment& assignment)
{
    if (macroIndex < 0 || macroIndex >= (int)macros.size()) return;
    auto& list = macros[(size_t)macroIndex].assignments;
    if (assignmentIndex >= 0 && assignmentIndex < (int)list.size())
    {
        list[(size_t)assignmentIndex] = assignment;
        applyAssignments(macroIndex);
        listeners.call([](Listener& l) { l.onMacroStructureChanged(); });
    }
}

void MacroEngine::resetMacroToDefault(int macroIndex)
{
    if (macroIndex < 0 || macroIndex >= (int)macros.size()) return;
    setMacroValue(macroIndex, macros[(size_t)macroIndex].defaultValue, true);
}

void MacroEngine::resetAllMacros()
{
    for (size_t i = 0; i < macros.size(); ++i)
        resetMacroToDefault((int)i);
}

void MacroEngine::registerParameter(const RegisteredParameter& param)
{
    std::string key = param.id.toStdString();
    auto it = paramMap.find(key);
    if (it != paramMap.end())
    {
        registeredParams[it->second] = param;
    }
    else
    {
        size_t idx = registeredParams.size();
        registeredParams.push_back(param);
        paramMap[key] = idx;
    }
}

bool MacroEngine::hasParameter(const juce::String& paramId) const noexcept
{
    return paramMap.find(paramId.toStdString()) != paramMap.end();
}

const RegisteredParameter* MacroEngine::getParameter(const juce::String& paramId) const noexcept
{
    auto it = paramMap.find(paramId.toStdString());
    if (it != paramMap.end() && it->second < registeredParams.size())
        return &registeredParams[it->second];
    return nullptr;
}

void MacroEngine::applyAssignments(int macroIndex)
{
    if (macroIndex < 0 || macroIndex >= (int)macros.size()) return;
    const auto& m = macros[(size_t)macroIndex];
    if (!m.enabled) return;

    for (const auto& a : m.assignments)
    {
        if (!a.enabled) continue;
        auto p = getParameter(a.targetParameterId);
        if (p != nullptr && p->setter)
        {
            float targetValue = a.mapNormalizedValue(m.currentValue);
            p->setter(targetValue);
        }
    }
}

void MacroEngine::registerQuickActionHandler(QuickActionType type, std::function<void()> handler)
{
    quickActionHandlers[(int)type] = handler;
}

void MacroEngine::executeQuickAction(QuickActionType type)
{
    auto it = quickActionHandlers.find((int)type);
    if (it != quickActionHandlers.end() && it->second)
    {
        it->second();
    }
}

void MacroEngine::loadFactoryDefaults()
{
    macros.clear();

    // 1. Vocal Clarity
    {
        MacroDefinition m;
        m.id = 1;
        m.name = "Vocal Clarity";
        m.description = "Enhances high-mid presence while keeping dynamics smooth and controlled";
        m.currentValue = 0.5f;
        m.defaultValue = 0.5f;
        m.enabled = true;

        MacroAssignment a1;
        a1.targetParameterId = "ch.0.eq.midGain";
        a1.minVal = -3.0f;
        a1.maxVal = 6.0f;
        a1.curve = MacroCurveType::Linear;
        m.assignments.push_back(a1);

        MacroAssignment a2;
        a2.targetParameterId = "ch.0.comp.makeup";
        a2.minVal = 0.0f;
        a2.maxVal = 4.0f;
        a2.curve = MacroCurveType::Linear;
        m.assignments.push_back(a2);

        macros.push_back(m);
    }

    // 2. Music Ducking Master
    {
        MacroDefinition m;
        m.id = 2;
        m.name = "Music Ducking Master";
        m.description = "Simultaneously scales ducking depth and threshold sensitivity for background music";
        m.currentValue = 0.6f;
        m.defaultValue = 0.6f;
        m.enabled = true;

        MacroAssignment a1;
        a1.targetParameterId = "ducking.depth";
        a1.minVal = -4.0f;
        a1.maxVal = -24.0f;
        a1.curve = MacroCurveType::Linear;
        m.assignments.push_back(a1);

        MacroAssignment a2;
        a2.targetParameterId = "ducking.threshold";
        a2.minVal = -40.0f;
        a2.maxVal = -20.0f;
        a2.curve = MacroCurveType::Linear;
        m.assignments.push_back(a2);

        macros.push_back(m);
    }

    // 3. Broadcast Stream Loudness
    {
        MacroDefinition m;
        m.id = 3;
        m.name = "Stream Loudness";
        m.description = "Coordinates master bus compression and limiter ceiling for standard streaming loudness";
        m.currentValue = 0.5f;
        m.defaultValue = 0.5f;
        m.enabled = true;

        MacroAssignment a1;
        a1.targetParameterId = "stream.comp.makeup";
        a1.minVal = 0.0f;
        a1.maxVal = 6.0f;
        a1.curve = MacroCurveType::Linear;
        m.assignments.push_back(a1);

        MacroAssignment a2;
        a2.targetParameterId = "safety.streamCeiling";
        a2.minVal = -3.0f;
        a2.maxVal = -0.5f;
        a2.curve = MacroCurveType::Linear;
        m.assignments.push_back(a2);

        macros.push_back(m);
    }

    // 4. Acoustic Monitor Comfort
    {
        MacroDefinition m;
        m.id = 4;
        m.name = "Monitor Comfort";
        m.description = "Adjusts headphone monitoring level and safety ceiling simultaneously";
        m.currentValue = 0.5f;
        m.defaultValue = 0.5f;
        m.enabled = true;

        MacroAssignment a1;
        a1.targetParameterId = "monitor.gain";
        a1.minVal = 0.2f;
        a1.maxVal = 1.0f;
        a1.curve = MacroCurveType::Linear;
        m.assignments.push_back(a1);

        macros.push_back(m);
    }
}

std::unique_ptr<juce::XmlElement> MacroEngine::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("MacroEngine");
    for (const auto& m : macros)
    {
        auto mXml = xml->createNewChildElement("Macro");
        mXml->setAttribute("id", m.id);
        mXml->setAttribute("name", m.name);
        mXml->setAttribute("description", m.description);
        mXml->setAttribute("currentValue", (double)m.currentValue);
        mXml->setAttribute("defaultValue", (double)m.defaultValue);
        mXml->setAttribute("enabled", m.enabled);

        for (const auto& a : m.assignments)
        {
            auto aXml = mXml->createNewChildElement("Assignment");
            aXml->setAttribute("targetId", a.targetParameterId);
            aXml->setAttribute("minVal", (double)a.minVal);
            aXml->setAttribute("maxVal", (double)a.maxVal);
            aXml->setAttribute("curve", (int)a.curve);
            aXml->setAttribute("invert", a.invert);
            aXml->setAttribute("enabled", a.enabled);
        }
    }
    return xml;
}

void MacroEngine::restoreFromXml(const juce::XmlElement& xml)
{
    if (xml.getTagName() != "MacroEngine") return;

    std::vector<MacroDefinition> loaded;
    for (auto* mXml : xml.getChildIterator())
    {
        if (mXml->getTagName() != "Macro") continue;

        MacroDefinition m;
        m.id = mXml->getIntAttribute("id", (int)loaded.size() + 1);
        m.name = mXml->getStringAttribute("name", "Macro");
        m.description = mXml->getStringAttribute("description", "");
        m.currentValue = (float)mXml->getDoubleAttribute("currentValue", 0.0);
        m.defaultValue = (float)mXml->getDoubleAttribute("defaultValue", 0.0);
        m.enabled = mXml->getBoolAttribute("enabled", true);

        for (auto* aXml : mXml->getChildIterator())
        {
            if (aXml->getTagName() != "Assignment") continue;
            MacroAssignment a;
            a.targetParameterId = aXml->getStringAttribute("targetId", "");
            a.minVal = (float)aXml->getDoubleAttribute("minVal", 0.0);
            a.maxVal = (float)aXml->getDoubleAttribute("maxVal", 1.0);
            a.curve = (MacroCurveType)aXml->getIntAttribute("curve", 0);
            a.invert = aXml->getBoolAttribute("invert", false);
            a.enabled = aXml->getBoolAttribute("enabled", true);
            m.assignments.push_back(a);
        }
        loaded.push_back(m);
    }

    if (!loaded.empty())
    {
        macros = loaded;
        for (size_t i = 0; i < macros.size(); ++i)
            applyAssignments((int)i);
        listeners.call([](Listener& l) { l.onMacroStructureChanged(); });
    }
}

} // namespace LiveBus
