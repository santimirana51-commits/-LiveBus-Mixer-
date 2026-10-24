#include "RecallFilterComponent.h"

namespace LiveBus
{

RecallFilterComponent::RecallFilterComponent()
{
    // Configure buttons
    addAndMakeVisible(blockAllBtn);
    addAndMakeVisible(allowAllBtn);
    addAndMakeVisible(applyBtn);
    addAndMakeVisible(cancelBtn);

    blockAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2b2f38));
    blockAllBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    blockAllBtn.onClick = [this] { blockAll(); };

    allowAllBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2b2f38));
    allowAllBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    allowAllBtn.onClick = [this] { allowAll(); };

    applyBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff124d2d));
    applyBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    applyBtn.onClick = [this] {
        if (onApply) onApply(currentFilter);
    };

    cancelBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff521a1f));
    cancelBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    cancelBtn.onClick = [this] {
        if (onCancel) onCancel();
    };

    // Initialize Categories
    categories = {
        { "Inputs", true },
        { "Mixes", true },
        { "FX Units", true },
        { "FX Sends", true },
        { "FX Returns", false },  // 4x2 matrix has direct bus returns
        { "DCAs", false },        // N/A in 4x2 matrix
        { "Mute Groups", false }, // N/A in 4x2 matrix
        { "SoftKeys", false },    // N/A in current hardware layer
        { "MIDI", true },
        { "Controllers", true },
        { "Dyn8 / Vocal", true },
        { "Other", true }
    };

    // Initialize Columns (matching reference image)
    columns = {
        { "Processing", 0, true },
        { "Mute", 1, true },
        { "Level", 2, true },
        { "Main On", 3, true },
        { "Pan", 4, true },
        { "Routing", 5, true },
        { "Direct Out", 6, true },
        { "IEM / Wedge", 7, true },
        { "DCA & Grp", 8, false }, // N/A
        { "Name & Colour", 9, true }
    };

    // Initialize Rows
    rows = {
        { "Inputs 1-4", 0 },
        { "Stream Bus", 1 },
        { "Monitor Bus", 2 },
        { "FX Units", 3 },
        { "MIDI Mapping", 4 },
        { "Device Preferences", 5 }
    };
}

void RecallFilterComponent::setSceneName(const juce::String& name)
{
    targetSceneName = name;
    repaint();
}

void RecallFilterComponent::setFilter(const RecallFilter& filter)
{
    currentFilter = filter;
    repaint();
    if (onFilterChanged) onFilterChanged();
}

void RecallFilterComponent::blockAll()
{
    currentFilter.blockAll();
    repaint();
    if (onFilterChanged) onFilterChanged();
}

void RecallFilterComponent::allowAll()
{
    currentFilter.allowAll();
    repaint();
    if (onFilterChanged) onFilterChanged();
}

void RecallFilterComponent::resized()
{
    auto b = getLocalBounds();

    // Top action bar
    int topY = 10;
    int btnH = 28;
    blockAllBtn.setBounds(16, topY, 110, btnH);
    allowAllBtn.setBounds(134, topY, 110, btnH);

    cancelBtn.setBounds(b.getRight() - 110 - 16, topY, 110, btnH);
    applyBtn.setBounds(cancelBtn.getX() - 110 - 12, topY, 110, btnH);

    updateHitTargets();
}

void RecallFilterComponent::updateHitTargets()
{
    hitTargets.clear();

    auto b = getLocalBounds();
    int leftColW = 160;
    int gridLeft = leftColW + 28;
    int gridTop = 180;
    int colW = 68;
    int rowH = 46;
    float discDiam = 26.0f;

    // Master ALL target (disc above row headers)
    float masterAllX = (float)gridLeft + 20.0f;
    float masterAllY = (float)gridTop - 34.0f;
    hitTargets.push_back({ { masterAllX, masterAllY, 30.0f, 30.0f }, -2, -1, true, false, false });

    // Column '+' headers
    for (size_t c = 0; c < columns.size(); ++c)
    {
        float cx = (float)(gridLeft + 180 + (int)c * colW);
        float cy = (float)(gridTop - 120.0f);
        hitTargets.push_back({ { cx + (colW - discDiam) * 0.5f, cy, discDiam, 20.0f }, -1, (int)c, false, true, false });
    }

    // Row '+' headers and Matrix Grid Discs
    for (size_t r = 0; r < rows.size(); ++r)
    {
        float ry = (float)(gridTop + (int)r * rowH);

        // Row '+' header
        hitTargets.push_back({ { (float)gridLeft + 8.0f, ry + 10.0f, 22.0f, 22.0f }, (int)r, -1, false, false, true });

        // Cell discs
        for (size_t c = 0; c < columns.size(); ++c)
        {
            float cx = (float)(gridLeft + 180 + (int)c * colW);
            float dx = cx + (colW - discDiam) * 0.5f;
            float dy = ry + (rowH - discDiam) * 0.5f;

            hitTargets.push_back({ { dx, dy, discDiam, discDiam }, (int)r, (int)c, false, false, false });
        }
    }
}

void RecallFilterComponent::drawDisc(juce::Graphics& g, const juce::Rectangle<float>& r, bool active, bool hovered, bool supported)
{
    if (!supported)
    {
        // Dimmed out / unsupported
        g.setColour(juce::Colour(0xff22252a));
        g.fillEllipse(r);
        g.setColour(juce::Colour(0xff333840));
        g.drawEllipse(r, 1.0f);
        return;
    }

    if (active)
    {
        // Glowing crimson 3D sphere (matching reference image)
        juce::ColourGradient grad(
            juce::Colour(0xffff4c4c), r.getX() + r.getWidth() * 0.35f, r.getY() + r.getHeight() * 0.35f,
            juce::Colour(0xff990000), r.getRight(), r.getBottom(), true
        );
        grad.addColour(0.6, juce::Colour(0xffd60000));

        // Soft outer glow
        g.setColour(juce::Colour(0x66ff1111));
        g.fillEllipse(r.expanded(2.5f));

        g.setGradientFill(grad);
        g.fillEllipse(r);

        // Specular highlight arc
        g.setColour(juce::Colour(0xaaffffff));
        g.drawEllipse(r.reduced(1.5f), 1.0f);

        if (hovered)
        {
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.drawEllipse(r.expanded(1.5f), 1.5f);
        }
    }
    else
    {
        // Blocked: Sleek dark charcoal 3D sphere
        juce::ColourGradient grad(
            juce::Colour(0xff4a505b), r.getX() + r.getWidth() * 0.35f, r.getY() + r.getHeight() * 0.35f,
            juce::Colour(0xff181b20), r.getRight(), r.getBottom(), true
        );
        grad.addColour(0.6, juce::Colour(0xff2b3038));

        g.setGradientFill(grad);
        g.fillEllipse(r);

        g.setColour(juce::Colour(0xff15181c));
        g.drawEllipse(r, 1.2f);

        if (hovered)
        {
            g.setColour(juce::Colour(0xff00d2ff).withAlpha(0.6f));
            g.drawEllipse(r.expanded(1.0f), 1.5f);
        }
    }
}

void RecallFilterComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // 1. Console Background
    g.setColour(juce::Colour(0xff181a1f));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xff2d323b));
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);

    // 2. Top Header Title: "Recall Filter - [Scene Name]"
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    auto titleArea = juce::Rectangle<int>(250, 10, getWidth() - 500, 28);
    g.drawText("Recall Filter - " + targetSceneName, titleArea, juce::Justification::centred, true);

    // Subtle header bottom line
    g.setColour(juce::Colour(0xff282c35));
    g.drawLine(16.0f, 48.0f, b.getRight() - 16.0f, 48.0f, 1.5f);

    // 3. Left Category Column
    int leftColX = 16;
    int leftColY = 58;
    int leftColW = 160;
    int catBtnH = 32;

    for (size_t i = 0; i < categories.size(); ++i)
    {
        auto catRect = juce::Rectangle<float>((float)leftColX, (float)(leftColY + (int)i * (catBtnH + 4)), (float)leftColW, (float)catBtnH);
        bool isSelected = ((int)i == selectedCategoryIndex);

        if (isSelected)
        {
            g.setColour(juce::Colour(0xff0095c8));
            g.fillRoundedRectangle(catRect, 5.0f);
            g.setColour(juce::Colours::white);
        }
        else
        {
            g.setColour(juce::Colour(0xff21252d));
            g.fillRoundedRectangle(catRect, 5.0f);
            g.setColour(juce::Colour(0xff323742));
            g.drawRoundedRectangle(catRect, 5.0f, 1.0f);
            g.setColour(categories[i].isSupported ? juce::Colour(0xffc5cad4) : juce::Colour(0xff606673));
        }

        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(categories[i].name, catRect.reduced(10, 0), juce::Justification::centredLeft, true);

        // Small status disc on the right of category
        float discR = 8.0f;
        auto discRect = juce::Rectangle<float>(catRect.getRight() - 24.0f, catRect.getCentreY() - discR, discR * 2, discR * 2);
        bool catActive = false;
        if (i == 0) catActive = currentFilter.channels[0].processing || currentFilter.channels[0].level;
        else if (i == 1) catActive = currentFilter.buses[0].processing || currentFilter.buses[0].level;
        else if (i == 2) catActive = currentFilter.fxUnits;
        else if (i == 3) catActive = currentFilter.channels[0].directOut || currentFilter.channels[0].iemWedge;
        else if (i == 8) catActive = currentFilter.midi;
        else if (i == 10) catActive = currentFilter.channels[0].processing;
        else if (i == 11) catActive = currentFilter.otherDevice || currentFilter.otherPlayback;

        drawDisc(g, discRect, catActive && categories[i].isSupported, false, categories[i].isSupported);
    }

    // 4. Matrix Area Background
    int gridLeft = leftColX + leftColW + 20;
    int gridTop = 58;
    int gridW = getWidth() - gridLeft - 16;
    int gridH = getHeight() - gridTop - 16;
    auto matrixBounds = juce::Rectangle<float>((float)gridLeft, (float)gridTop, (float)gridW, (float)gridH);

    g.setColour(juce::Colour(0xff121418));
    g.fillRoundedRectangle(matrixBounds, 6.0f);
    g.setColour(juce::Colour(0xff282c35));
    g.drawRoundedRectangle(matrixBounds, 6.0f, 1.0f);

    // 5. Matrix Column Headers
    int colW = 68;
    int rowH = 46;
    int matrixGridTop = 180;

    for (size_t c = 0; c < columns.size(); ++c)
    {
        float cx = (float)(gridLeft + 180 + (int)c * colW);

        // Column background strip
        auto colStrip = juce::Rectangle<float>(cx, (float)gridTop + 8, (float)colW - 4, (float)gridH - 16);
        g.setColour(juce::Colour(0xff16191f));
        g.fillRoundedRectangle(colStrip, 4.0f);

        // Column '+' button
        auto plusRect = juce::Rectangle<float>(cx + (colW - 24) * 0.5f, (float)(matrixGridTop - 120), 24.0f, 18.0f);
        g.setColour(juce::Colour(0xff252a33));
        g.fillRoundedRectangle(plusRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("+", plusRect, juce::Justification::centred, false);

        // Column Label (vertical/angled text)
        g.setColour(columns[c].isSupported ? juce::Colour(0xffd0d5dd) : juce::Colour(0xff555c69));
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));

        // Draw rotated column text
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(-1.570796f, cx + colW * 0.5f, (float)(matrixGridTop - 45)));
        g.drawText(columns[c].name, (int)(cx + colW * 0.5f - 45), (int)(matrixGridTop - 55), 90, 20, juce::Justification::centred, false);
    }

    // 6. Master ALL Header
    float masterAllX = (float)gridLeft + 20.0f;
    float masterAllY = (float)matrixGridTop - 34.0f;
    auto masterAllRect = juce::Rectangle<float>(masterAllX, masterAllY, 30.0f, 30.0f);

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText("ALL", (int)masterAllX - 35, (int)masterAllY + 5, 30, 20, juce::Justification::centredRight, false);
    drawDisc(g, masterAllRect, currentFilter.isAnyAllowed(), hoveredTargetIndex == 0, true);

    // 7. Matrix Rows & Discs
    for (size_t r = 0; r < rows.size(); ++r)
    {
        float ry = (float)(matrixGridTop + (int)r * rowH);

        // Row background strip
        auto rowRect = juce::Rectangle<float>((float)gridLeft + 8, ry, (float)gridW - 16, (float)rowH - 4);
        g.setColour((r % 2 == 0) ? juce::Colour(0xff191c22) : juce::Colour(0xff15181e));
        g.fillRoundedRectangle(rowRect, 4.0f);

        // Row '+' button
        auto plusRect = juce::Rectangle<float>((float)gridLeft + 12.0f, ry + 11.0f, 22.0f, 20.0f);
        g.setColour(juce::Colour(0xff252a33));
        g.fillRoundedRectangle(plusRect, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("+", plusRect, juce::Justification::centred, false);

        // Row Label
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(rows[r].label, (int)gridLeft + 42, (int)ry + 11, 130, 20, juce::Justification::centredLeft, true);

        // Cell discs
        float discDiam = 26.0f;
        for (size_t c = 0; c < columns.size(); ++c)
        {
            float cx = (float)(gridLeft + 180 + (int)c * colW);
            float dx = cx + (colW - discDiam) * 0.5f;
            float dy = ry + (rowH - discDiam) * 0.5f;
            auto discRect = juce::Rectangle<float>(dx, dy, discDiam, discDiam);

            bool cellActive = false;
            bool cellSupported = columns[c].isSupported;

            // Evaluate state
            if (r == 0) // Inputs 1-4
            {
                switch (c)
                {
                    case 0: cellActive = currentFilter.channels[0].processing; break;
                    case 1: cellActive = currentFilter.channels[0].mute; break;
                    case 2: cellActive = currentFilter.channels[0].level; break;
                    case 3: cellActive = currentFilter.channels[0].mainOn; break;
                    case 4: cellActive = currentFilter.channels[0].pan; break;
                    case 5: cellActive = currentFilter.channels[0].routing; break;
                    case 6: cellActive = currentFilter.channels[0].directOut; break;
                    case 7: cellActive = currentFilter.channels[0].iemWedge; break;
                    case 8: cellActive = false; cellSupported = false; break; // DCA & Grp
                    case 9: cellActive = currentFilter.channels[0].nameAndColour; break;
                    default: break;
                }
            }
            else if (r == 1) // Stream Bus
            {
                switch (c)
                {
                    case 0: cellActive = currentFilter.buses[0].processing; break;
                    case 1: cellActive = currentFilter.buses[0].mute; break;
                    case 2: cellActive = currentFilter.buses[0].level; break;
                    case 4: cellActive = currentFilter.buses[0].pan; break;
                    default: cellSupported = false; break;
                }
            }
            else if (r == 2) // Monitor Bus
            {
                switch (c)
                {
                    case 0: cellActive = currentFilter.buses[1].processing; break;
                    case 1: cellActive = currentFilter.buses[1].mute; break;
                    case 2: cellActive = currentFilter.buses[1].level; break;
                    case 4: cellActive = currentFilter.buses[1].pan; break;
                    default: cellSupported = false; break;
                }
            }
            else if (r == 3) // FX Units
            {
                if (c == 0) cellActive = currentFilter.fxUnits;
                else cellSupported = false;
            }
            else if (r == 4) // MIDI Mapping
            {
                if (c == 0 || c == 5) cellActive = currentFilter.midi;
                else cellSupported = false;
            }
            else if (r == 5) // Device Preferences
            {
                if (c == 0 || c == 5) cellActive = currentFilter.otherDevice;
                else cellSupported = false;
            }

            drawDisc(g, discRect, cellActive, false, cellSupported);
        }
    }
}

void RecallFilterComponent::mouseMove(const juce::MouseEvent& e)
{
    int oldHover = hoveredTargetIndex;
    hoveredTargetIndex = -1;

    for (size_t i = 0; i < hitTargets.size(); ++i)
    {
        if (hitTargets[i].bounds.contains(e.position))
        {
            hoveredTargetIndex = (int)i;
            break;
        }
    }

    if (oldHover != hoveredTargetIndex)
    {
        repaint();
    }
}

void RecallFilterComponent::mouseDown(const juce::MouseEvent& e)
{
    // Check left category clicks
    int leftColX = 16;
    int leftColY = 58;
    int leftColW = 160;
    int catBtnH = 32;

    for (size_t i = 0; i < categories.size(); ++i)
    {
        auto catRect = juce::Rectangle<float>((float)leftColX, (float)(leftColY + (int)i * (catBtnH + 4)), (float)leftColW, (float)catBtnH);
        if (catRect.contains(e.position))
        {
            selectedCategoryIndex = (int)i;
            // Toggle corresponding category filter state
            if (i == 0) // Inputs
                currentFilter.toggleInputsRowBlock();
            else if (i == 1) // Mixes
            {
                bool cur = currentFilter.buses[0].processing;
                for (auto& b : currentFilter.buses) { b.processing = !cur; b.mute = !cur; b.level = !cur; b.pan = !cur; }
            }
            else if (i == 2) currentFilter.fxUnits = !currentFilter.fxUnits;
            else if (i == 3) // FX Sends
            {
                bool cur = currentFilter.channels[0].directOut;
                for (auto& ch : currentFilter.channels) { ch.directOut = !cur; ch.iemWedge = !cur; }
            }
            else if (i == 8) currentFilter.midi = !currentFilter.midi;
            else if (i == 10) // Dyn8 / Vocal DSP
            {
                bool cur = currentFilter.channels[0].processing;
                for (auto& ch : currentFilter.channels) ch.processing = !cur;
                for (auto& b : currentFilter.buses) b.processing = !cur;
            }
            else if (i == 11) // Other
            {
                currentFilter.otherDevice = !currentFilter.otherDevice;
                currentFilter.otherPlayback = !currentFilter.otherPlayback;
            }

            repaint();
            if (onFilterChanged) onFilterChanged();
            return;
        }
    }

    // Check Matrix hit targets
    for (const auto& target : hitTargets)
    {
        if (target.bounds.contains(e.position))
        {
            if (target.isMasterAll)
            {
                if (currentFilter.isAnyAllowed())
                    blockAll();
                else
                    allowAll();
                return;
            }

            if (target.isColumnPlus)
            {
                currentFilter.toggleColumnInputs(target.col);
                repaint();
                if (onFilterChanged) onFilterChanged();
                return;
            }

            if (target.isRowPlus)
            {
                if (target.row == 0)
                    currentFilter.toggleInputsRowBlock();
                else if (target.row == 1)
                {
                    bool cur = currentFilter.buses[0].processing;
                    currentFilter.buses[0].processing = !cur;
                    currentFilter.buses[0].mute = !cur;
                    currentFilter.buses[0].level = !cur;
                    currentFilter.buses[0].pan = !cur;
                }
                else if (target.row == 2)
                {
                    bool cur = currentFilter.buses[1].processing;
                    currentFilter.buses[1].processing = !cur;
                    currentFilter.buses[1].mute = !cur;
                    currentFilter.buses[1].level = !cur;
                    currentFilter.buses[1].pan = !cur;
                }
                else if (target.row == 3) currentFilter.fxUnits = !currentFilter.fxUnits;
                else if (target.row == 4) currentFilter.midi = !currentFilter.midi;
                else if (target.row == 5) currentFilter.otherDevice = !currentFilter.otherDevice;

                repaint();
                if (onFilterChanged) onFilterChanged();
                return;
            }

            // Cell disc toggle
            int r = target.row;
            int c = target.col;
            if (r == 0) // Inputs 1-4
            {
                switch (c)
                {
                    case 0: currentFilter.toggleColumnInputs(0); break;
                    case 1: currentFilter.toggleColumnInputs(1); break;
                    case 2: currentFilter.toggleColumnInputs(2); break;
                    case 3: currentFilter.toggleColumnInputs(3); break;
                    case 4: currentFilter.toggleColumnInputs(4); break;
                    case 5: currentFilter.toggleColumnInputs(5); break;
                    case 6: currentFilter.toggleColumnInputs(6); break;
                    case 7: currentFilter.toggleColumnInputs(7); break;
                    case 9: currentFilter.toggleColumnInputs(9); break;
                    default: break;
                }
            }
            else if (r == 1) // Stream Bus
            {
                if (c == 0) currentFilter.buses[0].processing = !currentFilter.buses[0].processing;
                else if (c == 1) currentFilter.buses[0].mute = !currentFilter.buses[0].mute;
                else if (c == 2) currentFilter.buses[0].level = !currentFilter.buses[0].level;
                else if (c == 4) currentFilter.buses[0].pan = !currentFilter.buses[0].pan;
            }
            else if (r == 2) // Monitor Bus
            {
                if (c == 0) currentFilter.buses[1].processing = !currentFilter.buses[1].processing;
                else if (c == 1) currentFilter.buses[1].mute = !currentFilter.buses[1].mute;
                else if (c == 2) currentFilter.buses[1].level = !currentFilter.buses[1].level;
                else if (c == 4) currentFilter.buses[1].pan = !currentFilter.buses[1].pan;
            }
            else if (r == 3 && c == 0) currentFilter.fxUnits = !currentFilter.fxUnits;
            else if (r == 4) currentFilter.midi = !currentFilter.midi;
            else if (r == 5) currentFilter.otherDevice = !currentFilter.otherDevice;

            repaint();
            if (onFilterChanged) onFilterChanged();
            return;
        }
    }
}

} // namespace LiveBus
