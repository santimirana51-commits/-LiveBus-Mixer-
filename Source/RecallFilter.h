#pragma once
#include <JuceHeader.h>
#include <array>
#include "RoutingMatrix.h"

namespace LiveBus
{

/** Granular recall filter for an individual input channel strip. */
struct ChannelRecallFilter
{
    bool processing = true;     // 8-Band EQ & Vocal DSP Rack
    bool mute = true;           // Mute state
    bool level = true;          // Channel fader level (gain)
    bool mainOn = true;         // Main bus assignment (Stream Route)
    bool pan = true;            // Channel stereo pan
    bool routing = true;        // Bus routing flags (Stream & Monitor enable)
    bool directOut = true;      // Stream send level
    bool iemWedge = true;       // Monitor send level (In-Ear / Wedge)
    bool nameAndColour = true;  // Channel strip name & color
};

/** Granular recall filter for an individual master bus. */
struct BusRecallFilter
{
    bool processing = true;     // Bus DSP (HPF, 4-band, Gate, Comp, Limiter, Reverb, Delay) + 8-Band EQ + Vocal DSP
    bool mute = true;           // Master mute
    bool level = true;          // Master fader level
    bool pan = true;            // Master pan
};

/**
 * RecallFilter specifies which subset of a Scene is applied during recall.
 * Provides fine-grained matrix control modeled on professional broadcast consoles
 * (Allen & Heath dLive / DiGiCo style).
 */
struct RecallFilter
{
    std::array<ChannelRecallFilter, maxInputs> channels; // Inputs 1-16
    std::array<BusRecallFilter, 2> buses;       // 0 = Stream Bus, 1 = Monitor Bus
    bool fxUnits = true;                        // Out-of-process VST3 plugins
    bool midi = true;                           // MIDI CC / Note mapping
    bool otherDevice = false;                   // Audio device configuration (default FALSE for safety)
    bool otherPlayback = true;                  // Playback routing & level
    bool ducking = true;                        // Auto ducking settings & targets
    bool safety = true;                         // Audio safety & output limiter
    bool macros = true;                         // Macro parameters & assignments
    bool routingMatrix = true;                  // Routing matrix & per-channel patchbay

    /** Set every filter switch to either allowed (true) or blocked (false). */
    void setAll(bool allow)
    {
        for (auto& ch : channels)
        {
            ch.processing = allow;
            ch.mute = allow;
            ch.level = allow;
            ch.mainOn = allow;
            ch.pan = allow;
            ch.routing = allow;
            ch.directOut = allow;
            ch.iemWedge = allow;
            ch.nameAndColour = allow;
        }
        for (auto& b : buses)
        {
            b.processing = allow;
            b.mute = allow;
            b.level = allow;
            b.pan = allow;
        }
        fxUnits = allow;
        midi = allow;
        otherDevice = allow;
        otherPlayback = allow;
        ducking = allow;
        safety = allow;
        macros = allow;
        routingMatrix = allow;
    }

    /** Exclude all sections from recall. */
    void blockAll() { setAll(false); }

    /** Allow all supported sections for recall. */
    void allowAll() { setAll(true); }

    /** Returns true if at least one parameter is allowed for recall. */
    bool isAnyAllowed() const
    {
        for (const auto& ch : channels)
        {
            if (ch.processing || ch.mute || ch.level || ch.mainOn || ch.pan ||
                ch.routing || ch.directOut || ch.iemWedge || ch.nameAndColour)
                return true;
        }
        for (const auto& b : buses)
        {
            if (b.processing || b.mute || b.level || b.pan)
                return true;
        }
        return fxUnits || midi || otherDevice || otherPlayback || ducking || safety || macros || routingMatrix;
    }

    /** Toggle an entire column across all input channels. */
    void toggleColumnInputs(int colIndex)
    {
        // Columns: 0=Processing, 1=Mute, 2=Level, 3=Main On, 4=Pan, 5=Routing, 6=Direct Out, 7=IEM/Wedge, 8=DCA/Grp, 9=Name&Colour
        bool currentState = false;
        switch (colIndex)
        {
            case 0: currentState = channels[0].processing; break;
            case 1: currentState = channels[0].mute; break;
            case 2: currentState = channels[0].level; break;
            case 3: currentState = channels[0].mainOn; break;
            case 4: currentState = channels[0].pan; break;
            case 5: currentState = channels[0].routing; break;
            case 6: currentState = channels[0].directOut; break;
            case 7: currentState = channels[0].iemWedge; break;
            case 9: currentState = channels[0].nameAndColour; break;
            default: break;
        }
        bool newState = !currentState;
        for (auto& ch : channels)
        {
            switch (colIndex)
            {
                case 0: ch.processing = newState; break;
                case 1: ch.mute = newState; break;
                case 2: ch.level = newState; break;
                case 3: ch.mainOn = newState; break;
                case 4: ch.pan = newState; break;
                case 5: ch.routing = newState; break;
                case 6: ch.directOut = newState; break;
                case 7: ch.iemWedge = newState; break;
                case 9: ch.nameAndColour = newState; break;
                default: break;
            }
        }
    }

    /** Toggle all inputs row block. */
    void toggleInputsRowBlock()
    {
        bool cur = channels[0].processing;
        for (auto& ch : channels)
        {
            ch.processing = !cur;
            ch.mute = !cur;
            ch.level = !cur;
            ch.mainOn = !cur;
            ch.pan = !cur;
            ch.routing = !cur;
            ch.directOut = !cur;
            ch.iemWedge = !cur;
            ch.nameAndColour = !cur;
        }
    }
};

} // namespace LiveBus
