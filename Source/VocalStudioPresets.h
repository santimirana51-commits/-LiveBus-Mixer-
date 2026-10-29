#pragma once
#include <JuceHeader.h>
#include <array>
#include <string>

namespace LiveBus
{
/**
 * Factory and User Preset structure for RNNoise Vocal Studio VST.
 * Encapsulates full DSP parameter sets:
 * - RNNoise (VAD threshold, grace period, wet mix)
 * - Input Trim & HPF
 * - Noise Gate
 * - 8-Band EQ
 * - De-Esser
 * - Compressor
 * - Vocal Enhancement (Warmth, Body, Clarity, Presence, Air)
 * - Limiter & Output Gain
 */
struct VocalStudioPreset
{
    const char* name;
    const char* category;
    const char* description;

    // RNNoise
    bool rnnoiseBypass{false};
    bool enableVAD{false};
    float vadThreshold{0.60f};
    float gracePeriodMs{200.0f};
    float wetPercent{100.0f};

    // Input Trim & HPF
    float inputGainDb{0.0f};
    bool hpfBypass{false};
    float hpfFreq{80.0f};
    int hpfSlope{1}; // 1 = 12dB/oct, 2 = 24dB/oct

    // Noise Gate
    bool gateBypass{false};
    float gateThresholdDb{-42.0f};
    float gateAttackMs{1.0f};
    float gateHoldMs{20.0f};
    float gateReleaseMs{100.0f};
    float gateFloorDb{-60.0f};

    // 8-Band Parametric EQ
    bool eqBypass{false};
    struct BandSettings
    {
        float freq;
        float gainDb;
        float q;
        int type; // 0=Bell, 1=LowShelf, 2=HighShelf, 3=LowPass, 4=HighPass, 5=Notch
        bool enabled;
    };
    std::array<BandSettings, 8> eqBands;

    // De-Esser
    bool deEsserBypass{false};
    float deEsserThresholdDb{-16.0f};
    float deEsserFreq{5500.0f};
    float deEsserRangeDb{-8.0f};
    int deEsserType{0}; // 0 = Bell, 1 = High-Shelf

    // Compressor
    bool compBypass{false};
    float compThresholdDb{-24.0f};
    float compRatio{3.0f};
    float compAttackMs{10.0f};
    float compReleaseMs{120.0f};
    int compKnee{1}; // 0=Soft, 1=Normal, 2=Hard
    float compMakeupDb{3.0f};
    float compMixPercent{100.0f};
    bool compAutoRelease{false};

    // Vocal Enhancement (0 to 100%)
    bool enhanceBypass{false};
    float warmth{0.0f};
    float body{0.0f};
    float clarity{0.0f};
    float presence{0.0f};
    float air{0.0f};

    // Output Limiter & Gain
    bool limiterBypass{false};
    float limiterCeilingDb{-1.0f};
    float limiterReleaseMs{50.0f};
    float outputGainDb{0.0f};
};

class VocalStudioPresetManager
{
public:
    static constexpr int NumFactoryPresets = 13;

    static const VocalStudioPreset& getFactoryPreset(int index) noexcept
    {
        static const std::array<VocalStudioPreset, NumFactoryPresets> factoryPresets = {{
            // 0: Clean Broadcast
            {
                "Clean Broadcast", "Broadcast", "Pristine, authoritative speech for radio and podcasts with transparent AI cleanup",
                false, false, 0.60f, 200.0f, 100.0f, // RNNoise
                0.0f, false, 80.0f, 1,               // HPF 80Hz 12dB
                false, -46.0f, 1.0f, 30.0f, 120.0f, -50.0f, // Gate
                false, {{
                    { 80.0f,    0.0f, 0.71f, 4, true },  // HPF
                    { 180.0f,  -1.5f, 1.2f,  0, true },  // Mud cut
                    { 300.0f,   0.0f, 1.0f,  0, true },
                    { 1000.0f,  0.0f, 1.0f,  0, true },
                    { 2800.0f,  2.0f, 1.2f,  0, true },  // Articulation
                    { 5500.0f, -1.0f, 1.5f,  0, true },  // Harshness taming
                    { 10000.0f, 2.5f, 0.71f, 2, true },  // High shelf
                    { 18000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -18.0f, 6000.0f, -6.0f, 0,    // De-Esser Bell
                false, -20.0f, 3.5f, 15.0f, 100.0f, 1, 3.0f, 100.0f, true, // Comp Auto-Release
                false, 15.0f, 10.0f, 30.0f, 25.0f, 20.0f, // Enhancer
                false, -1.0f, 40.0f, 0.0f             // Limiter
            },

            // 1: Studio Lead Vocal
            {
                "Studio Lead Vocal", "Studio", "Full dynamic range with polished sheen, warmth and aggressive presence for singing",
                false, false, 0.60f, 150.0f, 90.0f,
                0.0f, false, 75.0f, 1,
                true, -50.0f, 2.0f, 20.0f, 100.0f, -60.0f, // Gate bypassed for natural tails
                false, {{
                    { 75.0f,    0.0f, 0.71f, 4, true },
                    { 240.0f,  -2.0f, 1.4f,  0, true },  // Boxiness cut
                    { 500.0f,   1.0f, 1.0f,  0, true },  // Chest body
                    { 1500.0f,  0.5f, 1.2f,  0, true },
                    { 3500.0f,  3.0f, 1.0f,  0, true },  // Lead clarity
                    { 7000.0f, -1.5f, 2.0f,  0, true },  // De-ess dip
                    { 12000.0f, 4.0f, 0.71f, 2, true },  // Air shelf
                    { 19000.0f, 0.0f, 0.71f, 3, true }
                }},
                false, -16.0f, 5800.0f, -9.0f, 0,
                false, -24.0f, 4.0f, 8.0f, 140.0f, 0, 4.0f, 85.0f, false, // Parallel comp 85%
                false, 35.0f, 25.0f, 40.0f, 35.0f, 45.0f,
                false, -0.8f, 50.0f, 0.5f
            },

            // 2: Warm Intimate Voice
            {
                "Warm Intimate Voice", "Acoustic", "Close-mic warmth, gentle compression and smooth tops without sibilance",
                false, false, 0.60f, 250.0f, 95.0f,
                0.0f, false, 60.0f, 1,
                true, -55.0f, 5.0f, 50.0f, 200.0f, -60.0f,
                false, {{
                    { 60.0f,    0.0f, 0.71f, 4, true },
                    { 280.0f,   2.5f, 0.9f,  0, true },  // Warm low-mid
                    { 600.0f,   0.5f, 1.0f,  0, true },
                    { 1800.0f, -1.0f, 1.1f,  0, true },
                    { 3200.0f,  1.0f, 1.0f,  0, true },
                    { 6500.0f, -2.0f, 1.5f,  0, true },
                    { 11000.0f, 1.5f, 0.71f, 2, true },
                    { 18000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -20.0f, 5200.0f, -10.0f, 0,
                false, -18.0f, 2.2f, 25.0f, 180.0f, 0, 2.0f, 100.0f, true,
                false, 50.0f, 40.0f, 15.0f, 10.0f, 20.0f,
                false, -1.5f, 60.0f, 0.0f
            },

            // 3: Crisp Live Streamer
            {
                "Crisp Live Streamer", "Streaming", "Punchy speech tuned for live game streaming, cuts over loud backgrounds",
                false, true, 0.65f, 180.0f, 100.0f,
                0.0f, false, 95.0f, 2, // 24dB HPF
                false, -40.0f, 1.0f, 20.0f, 90.0f, -55.0f,
                false, {{
                    { 95.0f,    0.0f, 0.71f, 4, true },
                    { 220.0f,  -2.5f, 1.5f,  0, true },  // Boom cut
                    { 450.0f,  -1.0f, 1.0f,  0, true },
                    { 1200.0f,  1.0f, 1.0f,  0, true },
                    { 3200.0f,  3.5f, 1.2f,  0, true },  // High clarity
                    { 5800.0f, -1.0f, 1.8f,  0, true },
                    { 9500.0f,  3.0f, 0.71f, 2, true },
                    { 16000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -17.0f, 6200.0f, -8.0f, 0,
                false, -22.0f, 4.5f, 10.0f, 80.0f, 1, 3.5f, 100.0f, false,
                false, 10.0f, 15.0f, 45.0f, 40.0f, 30.0f,
                false, -0.5f, 35.0f, 0.5f
            },

            // 4: Gaming & Discord Mic
            {
                "Gaming & Discord Mic", "Gaming", "Tight gating & maximum AI suppression for mechanical keyboards and fan noise",
                false, true, 0.72f, 150.0f, 100.0f, // High VAD
                0.0f, false, 110.0f, 2,
                false, -38.0f, 0.5f, 15.0f, 75.0f, -65.0f, // Fast gate
                false, {{
                    { 110.0f,   0.0f, 0.71f, 4, true },
                    { 250.0f,  -3.0f, 1.6f,  0, true },
                    { 500.0f,   0.0f, 1.0f,  0, true },
                    { 1400.0f,  1.5f, 1.0f,  0, true },
                    { 3000.0f,  2.5f, 1.2f,  0, true },
                    { 6000.0f, -1.0f, 1.4f,  0, true },
                    { 8500.0f,  1.5f, 0.71f, 2, true },
                    { 15000.0f, 0.0f, 0.71f, 3, true }
                }},
                false, -15.0f, 6000.0f, -6.0f, 0,
                false, -18.0f, 3.0f, 12.0f, 90.0f, 2, 2.0f, 100.0f, false,
                false, 0.0f, 5.0f, 35.0f, 30.0f, 10.0f,
                false, -1.0f, 40.0f, 0.0f
            },

            // 5: Heavy Noise Eliminator
            {
                "Heavy Noise Eliminator", "Restoration", "Extreme suppression for noisy AC, outdoor noise, and severe background hum",
                false, true, 0.80f, 120.0f, 100.0f, // Aggressive RNNoise & VAD
                0.0f, false, 120.0f, 2,
                false, -35.0f, 1.0f, 10.0f, 60.0f, -70.0f,
                false, {{
                    { 120.0f,   0.0f, 0.71f, 4, true },
                    { 220.0f,  -4.0f, 2.0f,  0, true },
                    { 450.0f,  -1.5f, 1.2f,  0, true },
                    { 1000.0f,  0.0f, 1.0f,  0, true },
                    { 2800.0f,  2.0f, 1.2f,  0, true },
                    { 5500.0f, -2.0f, 1.5f,  0, true },
                    { 8000.0f,  1.0f, 0.71f, 2, true },
                    { 14000.0f, 0.0f, 0.71f, 3, true }
                }},
                false, -14.0f, 5600.0f, -8.0f, 0,
                false, -16.0f, 3.5f, 15.0f, 80.0f, 1, 2.0f, 100.0f, false,
                false, 0.0f, 10.0f, 30.0f, 20.0f, 0.0f,
                false, -1.0f, 40.0f, 0.0f
            },

            // 6: Podcast Dual Host
            {
                "Podcast Dual Host", "Podcast", "Evened dynamic levels so softer and louder speakers maintain matched loudness",
                false, false, 0.60f, 220.0f, 100.0f,
                0.0f, false, 85.0f, 1,
                false, -44.0f, 1.5f, 25.0f, 130.0f, -48.0f,
                false, {{
                    { 85.0f,    0.0f, 0.71f, 4, true },
                    { 200.0f,  -1.5f, 1.2f,  0, true },
                    { 400.0f,   0.5f, 1.0f,  0, true },
                    { 1200.0f,  0.5f, 1.0f,  0, true },
                    { 2600.0f,  1.8f, 1.1f,  0, true },
                    { 6000.0f, -1.2f, 1.6f,  0, true },
                    { 10500.0f, 2.0f, 0.71f, 2, true },
                    { 18000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -18.0f, 5800.0f, -7.0f, 0,
                false, -26.0f, 5.0f, 12.0f, 120.0f, 1, 4.5f, 100.0f, true, // Deep compression to even levels
                false, 20.0f, 20.0f, 25.0f, 25.0f, 25.0f,
                false, -1.0f, 45.0f, 0.0f
            },

            // 7: Modern Pop Vocal
            {
                "Modern Pop Vocal", "Music", "Ultra-bright airy high-end, tight punchy mid-range, and controlled dynamics",
                false, false, 0.55f, 120.0f, 85.0f,
                0.0f, false, 85.0f, 1,
                true, -50.0f, 2.0f, 15.0f, 90.0f, -60.0f,
                false, {{
                    { 85.0f,    0.0f, 0.71f, 4, true },
                    { 300.0f,  -3.0f, 1.8f,  0, true },
                    { 800.0f,  -1.0f, 1.2f,  0, true },
                    { 2200.0f,  1.5f, 1.0f,  0, true },
                    { 4500.0f,  3.0f, 1.2f,  0, true },
                    { 7200.0f, -2.0f, 2.2f,  0, true },
                    { 13000.0f, 5.0f, 0.71f, 2, true },  // Pop air
                    { 20000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -20.0f, 6500.0f, -12.0f, 1, // Shelf De-Esser
                false, -25.0f, 4.5f, 5.0f, 100.0f, 0, 5.0f, 80.0f, false, // Fast parallel
                false, 25.0f, 20.0f, 45.0f, 50.0f, 65.0f, // Maximum Air & Presence
                false, -0.5f, 30.0f, 1.0f
            },

            // 8: Rap & Hip-Hop Vocals
            {
                "Rap & Hip-Hop Vocals", "Music", "Heavy punch, focused transients, chest impact and upfront aggressive presence",
                false, false, 0.60f, 150.0f, 90.0f,
                0.0f, false, 70.0f, 1,
                true, -48.0f, 1.0f, 20.0f, 80.0f, -60.0f,
                false, {{
                    { 70.0f,    0.0f, 0.71f, 4, true },
                    { 180.0f,   2.0f, 1.2f,  0, true },  // Low-end power
                    { 400.0f,  -2.0f, 1.5f,  0, true },  // Clean mud
                    { 1500.0f,  1.0f, 1.0f,  0, true },
                    { 3000.0f,  4.0f, 1.2f,  0, true },  // Upfront spit
                    { 6000.0f, -1.5f, 2.0f,  0, true },
                    { 11000.0f, 2.5f, 0.71f, 2, true },
                    { 18000.0f, 0.0f, 0.71f, 3, true }
                }},
                false, -17.0f, 5800.0f, -9.0f, 0,
                false, -22.0f, 6.0f, 8.0f, 80.0f, 2, 4.0f, 90.0f, false, // Heavy 6:1 ratio
                false, 40.0f, 35.0f, 50.0f, 45.0f, 30.0f,
                false, -0.5f, 35.0f, 0.5f
            },

            // 9: Voiceover Commercial
            {
                "Voiceover Commercial", "Commercial", "Authoritative 'Movie Trailer' vocal weight with hyper-polished clarity",
                false, false, 0.60f, 200.0f, 95.0f,
                0.0f, false, 65.0f, 1,
                false, -46.0f, 1.0f, 30.0f, 110.0f, -55.0f,
                false, {{
                    { 65.0f,    0.0f, 0.71f, 4, true },
                    { 150.0f,   2.5f, 1.0f,  0, true },  // Sub-chest boom
                    { 350.0f,  -2.0f, 1.4f,  0, true },
                    { 1000.0f,  0.5f, 1.0f,  0, true },
                    { 2800.0f,  3.0f, 1.1f,  0, true },
                    { 5500.0f, -1.0f, 1.5f,  0, true },
                    { 12000.0f, 3.5f, 0.71f, 2, true },
                    { 18000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -19.0f, 5500.0f, -8.0f, 0,
                false, -20.0f, 3.8f, 15.0f, 110.0f, 0, 3.5f, 100.0f, true,
                false, 45.0f, 30.0f, 35.0f, 35.0f, 40.0f,
                false, -0.8f, 45.0f, 0.0f
            },

            // 10: Smooth Acoustic Singer
            {
                "Smooth Acoustic Singer", "Acoustic", "Organic vocal feel preserving dynamic breathing and harmonic sweetness",
                false, false, 0.55f, 200.0f, 80.0f,
                0.0f, false, 75.0f, 1,
                true, -55.0f, 5.0f, 40.0f, 200.0f, -60.0f, // Gate off
                false, {{
                    { 75.0f,    0.0f, 0.71f, 4, true },
                    { 250.0f,   1.0f, 1.0f,  0, true },
                    { 500.0f,   0.0f, 1.0f,  0, true },
                    { 1200.0f, -0.5f, 1.0f,  0, true },
                    { 2500.0f,  1.5f, 1.0f,  0, true },
                    { 6000.0f, -1.0f, 1.5f,  0, true },
                    { 12500.0f, 2.5f, 0.71f, 2, true },
                    { 19000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -18.0f, 6000.0f, -6.0f, 0,
                false, -16.0f, 2.0f, 30.0f, 200.0f, 0, 1.5f, 100.0f, true, // Very gentle
                false, 30.0f, 20.0f, 20.0f, 20.0f, 35.0f,
                false, -1.5f, 60.0f, 0.0f
            },

            // 11: Vintage Tube Color
            {
                "Vintage Tube Color", "Vintage", "Warm saturation, thick mid-body and rolled-off silky high-end like classic gear",
                false, false, 0.60f, 200.0f, 90.0f,
                0.0f, false, 60.0f, 1,
                true, -50.0f, 2.0f, 30.0f, 150.0f, -60.0f,
                false, {{
                    { 60.0f,    0.0f, 0.71f, 4, true },
                    { 220.0f,   3.0f, 0.9f,  0, true },  // Thick low-mid
                    { 600.0f,   1.5f, 1.1f,  0, true },
                    { 1500.0f,  1.0f, 1.0f,  0, true },
                    { 3000.0f,  1.5f, 1.2f,  0, true },
                    { 5000.0f, -1.5f, 1.4f,  0, true },
                    { 9000.0f, -1.0f, 0.71f, 2, true },  // Vintage roll-off
                    { 16000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -16.0f, 5200.0f, -8.0f, 0,
                false, -18.0f, 3.2f, 20.0f, 150.0f, 0, 3.0f, 100.0f, false,
                false, 60.0f, 50.0f, 15.0f, 10.0f, 10.0f, // Maximum Warmth & Body
                false, -1.0f, 50.0f, 0.0f
            },

            // 12: Natural Speech Clarity
            {
                "Natural Speech Clarity", "General", "Balanced, uncolored transparent enhancement ideal for meetings and lectures",
                false, false, 0.60f, 200.0f, 100.0f,
                0.0f, false, 80.0f, 1,
                false, -45.0f, 1.5f, 20.0f, 100.0f, -50.0f,
                false, {{
                    { 80.0f,    0.0f, 0.71f, 4, true },
                    { 200.0f,  -1.0f, 1.0f,  0, true },
                    { 500.0f,   0.0f, 1.0f,  0, true },
                    { 1200.0f,  0.0f, 1.0f,  0, true },
                    { 2500.0f,  1.5f, 1.0f,  0, true },
                    { 5500.0f, -0.5f, 1.4f,  0, true },
                    { 10000.0f, 1.5f, 0.71f, 2, true },
                    { 18000.0f, 0.0f, 0.71f, 3, false }
                }},
                false, -16.0f, 5800.0f, -6.0f, 0,
                false, -18.0f, 2.5f, 15.0f, 120.0f, 1, 2.0f, 100.0f, true,
                false, 15.0f, 15.0f, 25.0f, 20.0f, 20.0f,
                false, -1.0f, 45.0f, 0.0f
            }
        }};

        const size_t safeIdx = static_cast<size_t>(juce::jlimit(0, NumFactoryPresets - 1, index));
        return factoryPresets[safeIdx];
    }

    /**
     * Applies a preset definition directly to the plugin's AudioProcessorValueTreeState.
     */
    static void applyPreset(juce::AudioProcessorValueTreeState& state, const VocalStudioPreset& p)
    {
        auto setVal = [&](const char* id, float val)
        {
            if (auto* param = state.getParameter(id))
                param->setValueNotifyingHost(param->convertTo0to1(val));
        };

        auto setBool = [&](const char* id, bool val)
        {
            if (auto* param = state.getParameter(id))
                param->setValueNotifyingHost(val ? 1.0f : 0.0f);
        };

        setBool("master_bypass", false);

        // RNNoise
        setBool("rnnoise_bypass", p.rnnoiseBypass);
        setBool("enable_vad", p.enableVAD);
        setVal("vad_threshold", p.vadThreshold);
        setVal("grace_period_ms", p.gracePeriodMs);
        setVal("wet_percent", p.wetPercent);

        // Input Gain & HPF
        setVal("input_gain_db", p.inputGainDb);
        setBool("hpf_bypass", p.hpfBypass);
        setVal("hpf_freq", p.hpfFreq);
        setVal("hpf_slope", static_cast<float>(p.hpfSlope));

        // Gate
        setBool("gate_bypass", p.gateBypass);
        setVal("gate_threshold", p.gateThresholdDb);
        setVal("gate_attack", p.gateAttackMs);
        setVal("gate_hold", p.gateHoldMs);
        setVal("gate_release", p.gateReleaseMs);
        setVal("gate_floor", p.gateFloorDb);

        // 8-Band EQ
        setBool("eq_bypass", p.eqBypass);
        for (int b = 0; b < 8; ++b)
        {
            const juce::String prefix = "eq_band_" + juce::String(b + 1) + "_";
            setVal((prefix + "freq").toRawUTF8(), p.eqBands[(size_t)b].freq);
            setVal((prefix + "gain").toRawUTF8(), p.eqBands[(size_t)b].gainDb);
            setVal((prefix + "q").toRawUTF8(), p.eqBands[(size_t)b].q);
            setVal((prefix + "type").toRawUTF8(), static_cast<float>(p.eqBands[(size_t)b].type));
            setBool((prefix + "enabled").toRawUTF8(), p.eqBands[(size_t)b].enabled);
        }

        // De-Esser
        setBool("deesser_bypass", p.deEsserBypass);
        setVal("deesser_threshold", p.deEsserThresholdDb);
        setVal("deesser_freq", p.deEsserFreq);
        setVal("deesser_range", p.deEsserRangeDb);
        setVal("deesser_type", static_cast<float>(p.deEsserType));

        // Compressor
        setBool("comp_bypass", p.compBypass);
        setVal("comp_threshold", p.compThresholdDb);
        setVal("comp_ratio", p.compRatio);
        setVal("comp_attack", p.compAttackMs);
        setVal("comp_release", p.compReleaseMs);
        setVal("comp_knee", static_cast<float>(p.compKnee));
        setVal("comp_makeup", p.compMakeupDb);
        setVal("comp_mix", p.compMixPercent);
        setBool("comp_autorelease", p.compAutoRelease);

        // Vocal Enhancement
        setBool("enhance_bypass", p.enhanceBypass);
        setVal("enhance_warmth", p.warmth);
        setVal("enhance_body", p.body);
        setVal("enhance_clarity", p.clarity);
        setVal("enhance_presence", p.presence);
        setVal("enhance_air", p.air);

        // Limiter & Output Gain
        setBool("limiter_bypass", p.limiterBypass);
        setVal("limiter_ceiling", p.limiterCeilingDb);
        setVal("limiter_release", p.limiterReleaseMs);
        setVal("output_gain_db", p.outputGainDb);
    }

    /**
     * Serializes current state to an XML string.
     */
    static juce::String exportPresetToXmlString(juce::AudioProcessorValueTreeState& state, const juce::String& presetName, const juce::String& category = "User")
    {
        if (auto xml = state.copyState().createXml())
        {
            xml->setAttribute("presetName", presetName);
            xml->setAttribute("category", category);
            xml->setAttribute("pluginVersion", "2.0.0");
            return xml->toString();
        }
        return {};
    }

    /**
     * Loads XML string state into the value tree state.
     */
    static bool importPresetFromXmlString(juce::AudioProcessorValueTreeState& state, const juce::String& xmlString, juce::String& outPresetName)
    {
        auto xml = juce::XmlDocument::parse(xmlString);
        if (xml == nullptr)
            return false;

        outPresetName = xml->getStringAttribute("presetName", "User Preset");
        const auto vt = juce::ValueTree::fromXml(*xml);
        if (vt.isValid() && vt.hasType(state.state.getType()))
        {
            state.replaceState(vt);
            return true;
        }
        return false;
    }
};

} // namespace LiveBus
