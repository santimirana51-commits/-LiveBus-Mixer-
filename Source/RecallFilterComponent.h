#pragma once
#include <JuceHeader.h>
#include "RecallFilter.h"

namespace LiveBus
{

/**
 * RecallFilterComponent — Professional Broadcast Console Recall Filter Matrix
 * Inspired by Allen & Heath dLive / DiGiCo console recall filter architecture.
 *
 * Features:
 * - Top Action Bar: Block All, Allow All, Recall Filter - [Scene Name], Apply, Cancel
 * - Left Category Selector: Inputs, Mixes, FX Units, FX Sends, FX Returns, DCAs, Mute Groups, MIDI, Controllers, Dyn8, Other
 * - Matrix Grid: Circular LED 3D discs (Allowed = Glowing Crimson, Blocked = Slate Charcoal)
 * - Header Column '+' toggles and Master 'ALL' disc toggle
 * - Safe recall execution: Apply triggers real filtered state; Cancel does not alter live mixer.
 */
class RecallFilterComponent : public juce::Component
{
public:
    RecallFilterComponent();
    ~RecallFilterComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void setSceneName(const juce::String& name);
    juce::String getSceneName() const { return targetSceneName; }

    void setFilter(const RecallFilter& filter);
    const RecallFilter& getFilter() const { return currentFilter; }

    std::function<void(const RecallFilter&)> onApply;
    std::function<void()> onCancel;
    std::function<void()> onFilterChanged;

    void blockAll();
    void allowAll();

private:
    juce::String targetSceneName{ "Active Scene" };
    RecallFilter currentFilter;

    // Top action buttons
    juce::TextButton blockAllBtn{ "Block All" };
    juce::TextButton allowAllBtn{ "Allow All" };
    juce::TextButton applyBtn{ "Apply" };
    juce::TextButton cancelBtn{ "Cancel" };

    // Left category buttons
    struct CategoryItem
    {
        juce::String name;
        bool isSupported = true;
    };
    std::vector<CategoryItem> categories;
    int selectedCategoryIndex = 0;

    // Column definitions for matrix
    struct ColumnDef
    {
        juce::String name;
        int colIndex; // 0..9
        bool isSupported = true;
    };
    std::vector<ColumnDef> columns;

    // Row definitions for matrix
    struct RowDef
    {
        juce::String label;
        int rowIndex; // 0=Inputs 1-4, 1=Stream Bus, 2=Monitor Bus, 3=FX Units, 4=MIDI, 5=Device/Other
    };
    std::vector<RowDef> rows;

    // Hit-testing rects
    struct DiscHitTarget
    {
        juce::Rectangle<float> bounds;
        int row = -1; // -1 for column header, -2 for master ALL, -3 for row header '+'
        int col = -1; // -1 for row header
        bool isMasterAll = false;
        bool isColumnPlus = false;
        bool isRowPlus = false;
    };
    std::vector<DiscHitTarget> hitTargets;
    int hoveredTargetIndex = -1;

    void drawDisc(juce::Graphics& g, const juce::Rectangle<float>& r, bool active, bool hovered, bool supported = true);
    void updateHitTargets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecallFilterComponent)
};

} // namespace LiveBus
