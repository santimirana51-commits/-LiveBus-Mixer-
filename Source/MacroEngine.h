#pragma once
#include <JuceHeader.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>

namespace LiveBus
{

enum class MacroCurveType
{
    Linear = 0,
    Logarithmic,
    Exponential,
    Inverted
};

enum class QuickActionType
{
    EmergencyMuteAll = 0,
    ToggleStreamMute,
    ToggleMonitorMute,
    ToggleDucking,
    ResetPeakWarnings,
    TogglePlayback,
    RecallScene
};

struct MacroAssignment
{
    juce::String targetParameterId;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    MacroCurveType curve = MacroCurveType::Linear;
    bool invert = false;
    bool enabled = true;

    float mapNormalizedValue(float normVal) const noexcept;
};

struct MacroDefinition
{
    int id = 0;
    juce::String name;
    juce::String description;
    float currentValue = 0.0f; // 0.0f .. 1.0f
    float defaultValue = 0.0f;
    bool enabled = true;
    std::vector<MacroAssignment> assignments;
};

struct RegisteredParameter
{
    juce::String id;
    juce::String displayName;
    juce::String group; // e.g. "Mixer", "DSP", "Ducking", "Safety"
    float minVal = 0.0f;
    float maxVal = 1.0f;
    float defaultValue = 0.0f;
    std::function<void(float)> setter;
    std::function<float()> getter;
};

class MacroEngine
{
public:
    MacroEngine();
    ~MacroEngine() = default;

    // =========================================================================
    // Macro Definitions & Operations
    // =========================================================================
    int getNumMacros() const noexcept { return (int)macros.size(); }
    const MacroDefinition* getMacro(int index) const noexcept;
    MacroDefinition* getMacro(int index) noexcept;
    const MacroDefinition* getMacroById(int id) const noexcept;
    MacroDefinition* getMacroById(int id) noexcept;

    void setMacroValue(int macroIndex, float normalizedValue, bool notify = true);
    float getMacroValue(int macroIndex) const noexcept;

    void addAssignment(int macroIndex, const MacroAssignment& assignment);
    void removeAssignment(int macroIndex, int assignmentIndex);
    void updateAssignment(int macroIndex, int assignmentIndex, const MacroAssignment& assignment);

    void resetMacroToDefault(int macroIndex);
    void resetAllMacros();

    // =========================================================================
    // Parameter Registry
    // =========================================================================
    void registerParameter(const RegisteredParameter& param);
    bool hasParameter(const juce::String& paramId) const noexcept;
    const RegisteredParameter* getParameter(const juce::String& paramId) const noexcept;
    const std::vector<RegisteredParameter>& getAllParameters() const noexcept { return registeredParams; }

    // =========================================================================
    // Quick Actions Engine
    // =========================================================================
    void registerQuickActionHandler(QuickActionType type, std::function<void()> handler);
    void executeQuickAction(QuickActionType type);

    // =========================================================================
    // Factory Presets & Serialization
    // =========================================================================
    void loadFactoryDefaults();
    std::unique_ptr<juce::XmlElement> createXml() const;
    void restoreFromXml(const juce::XmlElement& xml);

    // Listener for UI updates
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onMacroValueChanged(int /*macroIndex*/, float /*newValue*/) {}
        virtual void onMacroStructureChanged() {}
    };
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    void applyAssignments(int macroIndex);

    std::vector<MacroDefinition> macros;
    std::vector<RegisteredParameter> registeredParams;
    std::unordered_map<std::string, size_t> paramMap; // id -> index in registeredParams
    std::unordered_map<int, std::function<void()>> quickActionHandlers;
    juce::ListenerList<Listener> listeners;
};

} // namespace LiveBus
