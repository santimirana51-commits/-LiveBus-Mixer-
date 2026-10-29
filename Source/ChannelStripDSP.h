#pragma once
#include <JuceHeader.h>
#include "EightBandEQ.h"
#include "VocalDSPProcessor.h"
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>

static constexpr int numPuzuBands = 15;
static constexpr int numMonitorGraphicEQBands = 31;
static constexpr int numMainOutputEQBands = 24;

inline constexpr std::array<float, numMainOutputEQBands> mainOutputEQFrequencies{
    20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
    200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f,
    1600.0f, 2500.0f, 4000.0f, 6300.0f, 10000.0f, 16000.0f
};

inline constexpr std::array<float, numMonitorGraphicEQBands> monitorGraphicEQFrequencies{
    20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
    200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f,
    1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f,
    10000.0f, 12500.0f, 16000.0f, 20000.0f
};

inline const float defaultPuzuFreqs[15] = {
    25.3f, 40.1f, 63.7f, 101.0f, 184.0f, 260.0f, 405.0f, 764.0f,
    1000.0f, 1620.0f, 2520.0f, 4000.0f, 5140.0f, 10100.0f, 16000.0f
};

inline const float defaultPuzuQs[15] = {
    1.19f, 1.19f, 1.19f, 1.19f, 3.00f, 1.19f, 1.19f, 2.00f,
    1.19f, 1.19f, 1.19f, 1.19f, 2.70f, 1.19f, 1.19f
};

enum class PuzuFilterType : int { HighPass, LowShelf, Bell, HighShelf, LowPass, Notch };

struct PuzuBandParam
{
    std::atomic<float> freq{1000.0f};
    std::atomic<float> q{1.19f};
    std::atomic<float> gainDb{0.0f};
    std::atomic<PuzuFilterType> type{PuzuFilterType::Bell};
    std::atomic<bool> enabled{true};

    PuzuBandParam() = default;
    PuzuBandParam(float f, float qVal, float g = 0.0f)
        : freq(f), q(qVal), gainDb(g), enabled(true) {}
};

struct PuzuBandState
{
    float freq = 1000.0f;
    float q = 1.19f;
    float gainDb = 0.0f;
    PuzuFilterType type = PuzuFilterType::Bell;
    bool enabled = true;
};

struct PuzuEQState
{
    bool enabled = false;
    std::array<PuzuBandState, 15> bands{};
};

struct MonitorGraphicEQ31State
{
    bool enabled = false;
    std::array<float, numMonitorGraphicEQBands> gainDb{};
};

enum class MainOutputEQFilterType : int
{
    HighPass,
    LowShelf,
    Bell,
    HighShelf,
    Notch,
    LowPass
};

enum class MainOutputEQDetector : int
{
    Peak,
    RMS
};

struct MainOutputEQ24BandState
{
    float freq = 1000.0f;
    float q = 0.707f;
    float gainDb = 0.0f;
    MainOutputEQFilterType type = MainOutputEQFilterType::Bell;
    bool enabled = true;
    bool dynamicEnabled = false;
    float dynamicThresholdDb = -24.0f;
    float dynamicAttackMs = 10.0f;
    float dynamicReleaseMs = 100.0f;
    float dynamicRangeDb = 6.0f;
    MainOutputEQDetector dynamicDetector = MainOutputEQDetector::Peak;
};

struct MainOutputEQ24State
{
    bool enabled = false;
    std::array<MainOutputEQ24BandState, numMainOutputEQBands> bands{};
};

struct BusDSPParameters
{
    // Master enables
    std::atomic<bool> hpfEnabled{false};
    std::atomic<bool> eqEnabled{false};
    std::atomic<bool> puzuEnabled{false};
    std::atomic<bool> mainOutputEQEnabled{false};
    std::atomic<int> mainOutputEQSoloBand{-1};
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQFreq;
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQGainDb;
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQQ;
    std::array<std::atomic<int>, numMainOutputEQBands> mainOutputEQType;
    std::array<std::atomic<bool>, numMainOutputEQBands> mainOutputEQBandEnabled;
    std::array<std::atomic<bool>, numMainOutputEQBands> mainOutputEQDynamicEnabled;
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQDynamicThresholdDb;
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQDynamicAttackMs;
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQDynamicReleaseMs;
    std::array<std::atomic<float>, numMainOutputEQBands> mainOutputEQDynamicRangeDb;
    std::array<std::atomic<int>, numMainOutputEQBands> mainOutputEQDynamicDetector;
    std::atomic<bool> gateEnabled{false};
    std::atomic<bool> compEnabled{false};
    std::atomic<bool> limiterEnabled{false};
    std::atomic<bool> reverbEnabled{false};
    std::atomic<bool> delayEnabled{false};

    // HPF (High-Pass / Low-Cut)
    std::atomic<float> hpfFreq{80.0f};           // 20.0 to 1000.0 Hz
    std::atomic<int> hpfSlope{1};                // 0=6dB, 1=12dB, 2=18dB, 3=24dB/Oct
    std::atomic<int> hpfType{0};                 // 0=Butterworth, 1=Bessel, 2=Linkwitz-Riley

    // LPF (Low-Pass / High-Cut)
    std::atomic<bool> lpfEnabled{false};
    std::atomic<float> lpfFreq{20600.0f};
    std::atomic<int> lpfSlope{0}; // 0=OFF, 1=6dB, 2=12dB, 3=18dB, 4=24dB/Oct
    std::atomic<int> lpfType{0};

    // 15-Band Studio Parametric Equalizer (PUZU Style)
    std::array<PuzuBandParam, 15> puzuBands;
    std::atomic<bool> monitorGraphicEQEnabled{false};
    std::array<std::atomic<float>, numMonitorGraphicEQBands> monitorGraphicEQGainDb;

    // Legacy 4-Band Studio Parametric Equalizer compatibility fields
    std::atomic<float> eqLowFreq{100.0f};
    std::atomic<float> eqLowGainDb{0.0f};
    std::atomic<float> eqMid1Freq{500.0f};
    std::atomic<float> eqMid1GainDb{0.0f};
    std::atomic<float> eqMid1Q{0.707f};
    std::atomic<float> eqFreq{2500.0f};
    std::atomic<float> eqQ{0.707f};
    std::atomic<float> eqGainDb{0.0f};
    std::atomic<float> eqHighFreq{10000.0f};
    std::atomic<float> eqHighGainDb{0.0f};

    // Noise Gate
    std::atomic<float> gateThresholdDb{-45.0f};  // -80.0 to 0.0 dB
    std::atomic<float> gateRatio{4.0f};          // 1.0 to 20.0 (expander downward slope)
    std::atomic<float> gateAttackMs{2.0f};       // 0.1 to 100.0 ms
    std::atomic<float> gateHoldMs{20.0f};        // 0.0 to 500.0 ms
    std::atomic<float> gateReleaseMs{100.0f};    // 10.0 to 1000.0 ms
    std::atomic<float> gateFloorDb{-60.0f};      // -80.0 to 0.0 dB (-80 is mute)
    std::atomic<float> gateHysteresisDb{4.0f};   // 1.0 to 12.0 dB

    // Compressor
    std::atomic<float> compThresholdDb{-18.0f};  // -60.0 to 0.0 dB
    std::atomic<float> compRatio{3.0f};          // 1.0 to 20.0
    std::atomic<float> compAttackMs{10.0f};      // 0.1 to 100.0 ms
    std::atomic<float> compReleaseMs{100.0f};    // 10.0 to 1000.0 ms
    std::atomic<int> compKnee{1};                // 0=Soft(10dB), 1=Normal(5dB), 2=Hard(0dB)
    std::atomic<float> compMakeupDb{0.0f};       // -12.0 to +24.0 dB
    std::atomic<float> compMixPercent{100.0f};   // 0.0 to 100.0% (parallel compression)

    // Limiter (Brickwall peak ceiling)
    std::atomic<float> limiterCeilingDb{-1.0f};  // -20.0 to 0.0 dBFS
    std::atomic<float> limiterDbu{-30.0f};
    std::atomic<float> limiterAttackMs{0.3f};
    std::atomic<float> limiterReleaseMs{60.0f};  // 1.0 to 500.0 ms

    // Time Alignment Delay & Phase
    std::atomic<float> timeDelayMs{0.0f};
    std::atomic<float> timeDelayDistanceCm{0.0f};
    std::atomic<bool> phaseInverted{false};

    // Reverb
    std::atomic<float> reverbRoomSize{0.5f};     // 0.0 to 1.0
    std::atomic<float> reverbDamping{0.5f};      // 0.0 to 1.0
    std::atomic<float> reverbWetLevel{0.33f};    // 0.0 to 1.0
    std::atomic<float> reverbDryLevel{1.0f};     // 0.0 to 1.0
    std::atomic<float> reverbWidth{1.0f};        // 0.0 to 1.0

    // Creative Delay
    std::atomic<float> delayTimeMs{350.0f};      // 10.0 to 1500.0 ms
    std::atomic<float> delayFeedback{0.35f};     // 0.0 to 0.90 (clamped <= 0.90)
    std::atomic<float> delayWetLevel{0.25f};     // 0.0 to 1.0
    std::atomic<float> delayDryLevel{1.0f};      // 0.0 to 1.0
    std::atomic<bool> delayPingPong{false};      // Mono vs Ping-Pong Stereo

    // Master bus gain & pan
    std::atomic<float> masterGain{1.0f};
    std::atomic<float> masterPan{0.0f};          // -1.0 to +1.0
    std::atomic<bool> masterMute{false};
    std::atomic<bool> masterDim{false};

    BusDSPParameters()
    {
        setToDefault();
    }

    void setToDefault() noexcept
    {
        hpfEnabled.store(false, std::memory_order_relaxed);
        eqEnabled.store(false, std::memory_order_relaxed);
        puzuEnabled.store(false, std::memory_order_relaxed);
        mainOutputEQEnabled.store(false, std::memory_order_relaxed);
        mainOutputEQSoloBand.store(-1, std::memory_order_relaxed);
        gateEnabled.store(false, std::memory_order_relaxed);
        compEnabled.store(false, std::memory_order_relaxed);
        limiterEnabled.store(false, std::memory_order_relaxed);
        reverbEnabled.store(false, std::memory_order_relaxed);
        delayEnabled.store(false, std::memory_order_relaxed);
        hpfFreq.store(80.0f, std::memory_order_relaxed);
        hpfSlope.store(1, std::memory_order_relaxed);
        hpfType.store(0, std::memory_order_relaxed);
        lpfEnabled.store(false, std::memory_order_relaxed);
        lpfFreq.store(20600.0f, std::memory_order_relaxed);
        lpfSlope.store(0, std::memory_order_relaxed);
        lpfType.store(0, std::memory_order_relaxed);
        for (int i = 0; i < 15; ++i)
        {
            puzuBands[(size_t)i].freq.store(defaultPuzuFreqs[i], std::memory_order_relaxed);
            puzuBands[(size_t)i].q.store(defaultPuzuQs[i], std::memory_order_relaxed);
            puzuBands[(size_t)i].gainDb.store(0.0f, std::memory_order_relaxed);
            puzuBands[(size_t)i].type.store(PuzuFilterType::Bell, std::memory_order_relaxed);
            puzuBands[(size_t)i].enabled.store(true, std::memory_order_relaxed);
        }
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            mainOutputEQFreq[(size_t)band].store(
                mainOutputEQFrequencies[(size_t)band], std::memory_order_relaxed);
            mainOutputEQGainDb[(size_t)band].store(0.0f, std::memory_order_relaxed);
            mainOutputEQQ[(size_t)band].store(0.707f, std::memory_order_relaxed);
            mainOutputEQType[(size_t)band].store((int)MainOutputEQFilterType::Bell,
                                                 std::memory_order_relaxed);
            mainOutputEQBandEnabled[(size_t)band].store(true,
                                                        std::memory_order_relaxed);
            mainOutputEQDynamicEnabled[(size_t)band].store(false,
                                                           std::memory_order_relaxed);
            mainOutputEQDynamicThresholdDb[(size_t)band].store(-24.0f,
                                                                std::memory_order_relaxed);
            mainOutputEQDynamicAttackMs[(size_t)band].store(10.0f,
                                                             std::memory_order_relaxed);
            mainOutputEQDynamicReleaseMs[(size_t)band].store(100.0f,
                                                              std::memory_order_relaxed);
            mainOutputEQDynamicRangeDb[(size_t)band].store(6.0f,
                                                            std::memory_order_relaxed);
            mainOutputEQDynamicDetector[(size_t)band].store(
                (int)MainOutputEQDetector::Peak, std::memory_order_relaxed);
        }
        monitorGraphicEQEnabled.store(false, std::memory_order_relaxed);
        for (auto& gainDb : monitorGraphicEQGainDb)
            gainDb.store(0.0f, std::memory_order_relaxed);
        eqLowFreq.store(100.0f, std::memory_order_relaxed);
        eqLowGainDb.store(0.0f, std::memory_order_relaxed);
        eqMid1Freq.store(500.0f, std::memory_order_relaxed);
        eqMid1GainDb.store(0.0f, std::memory_order_relaxed);
        eqMid1Q.store(0.707f, std::memory_order_relaxed);
        eqFreq.store(2500.0f, std::memory_order_relaxed);
        eqQ.store(0.707f, std::memory_order_relaxed);
        eqGainDb.store(0.0f, std::memory_order_relaxed);
        eqHighFreq.store(10000.0f, std::memory_order_relaxed);
        eqHighGainDb.store(0.0f, std::memory_order_relaxed);
        gateThresholdDb.store(-45.0f, std::memory_order_relaxed);
        gateRatio.store(4.0f, std::memory_order_relaxed);
        gateAttackMs.store(2.0f, std::memory_order_relaxed);
        gateHoldMs.store(20.0f, std::memory_order_relaxed);
        gateReleaseMs.store(100.0f, std::memory_order_relaxed);
        gateFloorDb.store(-60.0f, std::memory_order_relaxed);
        gateHysteresisDb.store(4.0f, std::memory_order_relaxed);
        compThresholdDb.store(-18.0f, std::memory_order_relaxed);
        compRatio.store(3.0f, std::memory_order_relaxed);
        compAttackMs.store(10.0f, std::memory_order_relaxed);
        compReleaseMs.store(100.0f, std::memory_order_relaxed);
        compKnee.store(1, std::memory_order_relaxed);
        compMakeupDb.store(0.0f, std::memory_order_relaxed);
        compMixPercent.store(100.0f, std::memory_order_relaxed);
        limiterCeilingDb.store(-1.0f, std::memory_order_relaxed);
        limiterDbu.store(-30.0f, std::memory_order_relaxed);
        limiterAttackMs.store(0.3f, std::memory_order_relaxed);
        limiterReleaseMs.store(60.0f, std::memory_order_relaxed);
        timeDelayMs.store(0.0f, std::memory_order_relaxed);
        timeDelayDistanceCm.store(0.0f, std::memory_order_relaxed);
        phaseInverted.store(false, std::memory_order_relaxed);
        reverbRoomSize.store(0.5f, std::memory_order_relaxed);
        reverbDamping.store(0.5f, std::memory_order_relaxed);
        reverbWetLevel.store(0.33f, std::memory_order_relaxed);
        reverbDryLevel.store(1.0f, std::memory_order_relaxed);
        reverbWidth.store(1.0f, std::memory_order_relaxed);
        delayTimeMs.store(350.0f, std::memory_order_relaxed);
        delayFeedback.store(0.35f, std::memory_order_relaxed);
        delayWetLevel.store(0.25f, std::memory_order_relaxed);
        delayDryLevel.store(1.0f, std::memory_order_relaxed);
        delayPingPong.store(false, std::memory_order_relaxed);
        masterGain.store(1.0f, std::memory_order_relaxed);
        masterPan.store(0.0f, std::memory_order_relaxed);
        masterMute.store(false, std::memory_order_relaxed);
        masterDim.store(false, std::memory_order_relaxed);
    }
};

class BusDSPProcessor
{
public:
    BusDSPProcessor()
    {
        eightBandEQ.params.masterEnabled.store(false, std::memory_order_relaxed);
        for (int channel = 0; channel < 2; ++channel)
        {
            mainOutputEQSoloFilters[channel].coefficients =
                juce::dsp::IIR::Coefficients<float>::makeBandPass(
                    48000.0, 1000.0f, 0.707f);
            mainOutputEQSoloFilters[channel].reset();
            for (int band = 0; band < numMainOutputEQBands; ++band)
            {
                mainOutputEQDynamicFilters[channel][band].coefficients =
                    juce::dsp::IIR::Coefficients<float>::makeBandPass(
                        48000.0, mainOutputEQFrequencies[(size_t)band], 0.707f);
                mainOutputEQDynamicFilters[channel][band].reset();
            }
        }
        std::array<juce::ReferenceCountedObjectPtr<
                       juce::dsp::IIR::Coefficients<float>>,
                   numMainOutputEQBands> initialMainCoefficients{};
        for (auto& coefficient : initialMainCoefficients)
            coefficient = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                48000.0, 1000.0f, 0.707f, 1.0f);
        mainOutputEQFilters.prepare(initialMainCoefficients);
    }

    void prepare(double sampleRate, int maxBlockSize)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        currentBlockSize = juce::jmax(1, maxBlockSize);
        puzuMix = 0.0f;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate;
        spec.maximumBlockSize = (juce::uint32)currentBlockSize;
        spec.numChannels = 2;

        for (int ch = 0; ch < 2; ++ch)
        {
            hpfFilterStage1[ch].reset();
            hpfFilterStage2[ch].reset();
            lpfFilterStage1[ch].reset();
            lpfFilterStage2[ch].reset();

            for (int b = 0; b < numMainOutputEQBands; ++b)
            {
                mainOutputEQDynamicFilters[ch][b].coefficients =
                    juce::dsp::IIR::Coefficients<float>::makeBandPass(
                        currentSampleRate,
                        params.mainOutputEQFreq[(size_t)b].load(std::memory_order_relaxed),
                        params.mainOutputEQQ[(size_t)b].load(std::memory_order_relaxed));
                mainOutputEQDynamicFilters[ch][b].reset();
            }

            legacyEqLowFilter[ch].reset();
            legacyEqMid1Filter[ch].reset();
            legacyEqMid2Filter[ch].reset();
            legacyEqHighFilter[ch].reset();
        }
        std::array<juce::ReferenceCountedObjectPtr<
                       juce::dsp::IIR::Coefficients<float>>, numPuzuBands> puzuCoefficients{};
        for (int band = 0; band < numPuzuBands; ++band)
            puzuCoefficients[(size_t)band] =
                juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                    currentSampleRate, defaultPuzuFreqs[band],
                    defaultPuzuQs[band], 1.0f);
        puzuBandFilters.prepare(puzuCoefficients);

        std::array<juce::ReferenceCountedObjectPtr<
                       juce::dsp::IIR::Coefficients<float>>,
                   numMonitorGraphicEQBands> monitorCoefficients{};
        for (int band = 0; band < numMonitorGraphicEQBands; ++band)
            monitorCoefficients[(size_t)band] =
                juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                    currentSampleRate,
                    monitorGraphicEQFrequencies[(size_t)band], 4.318f, 1.0f);
        monitorGraphicEQFilters.prepare(monitorCoefficients);

        std::array<juce::ReferenceCountedObjectPtr<
                       juce::dsp::IIR::Coefficients<float>>,
                   numMainOutputEQBands> mainOutputCoefficients{};
        for (int band = 0; band < numMainOutputEQBands; ++band)
            mainOutputCoefficients[(size_t)band] =
                juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                    currentSampleRate,
                    mainOutputEQFrequencies[(size_t)band], 0.707f, 1.0f);
        mainOutputEQFilters.prepare(mainOutputCoefficients);
        lastHpfFreq = -1.0f;
        lastHpfSlope = -1;
        lastLpfFreq = -1.0f;
        lastLpfSlope = -1;

        reverb.prepare(spec);

        // Prepare delay buffer (max 2 seconds)
        int maxDelaySamples = (int)(currentSampleRate * 2.0);
        delayBuffer.setSize(2, maxDelaySamples);
        delayBuffer.clear();
        delayWritePos = 0;
        currentDelaySamples = params.delayTimeMs.load(std::memory_order_relaxed) * 0.001f * (float)currentSampleRate;

        // Time alignment buffer (max 100ms)
        int maxAlignSamples = (int)(currentSampleRate * 0.1);
        timeAlignBuffer.setSize(2, juce::jmax(16, maxAlignSamples));
        timeAlignBuffer.clear();
        timeAlignWritePos = 0;
        puzuPreAnalyzerFifo.reset();
        puzuPostAnalyzerFifo.reset();
        monitorGraphicEQPreAnalyzerFifo.reset();
        monitorGraphicEQPostAnalyzerFifo.reset();
        mainOutputEQPreAnalyzerFifo.reset();
        mainOutputEQPostAnalyzerFifo.reset();

        smoothedMasterGain.reset(currentSampleRate, 0.02); // 20ms ramp
        smoothedMasterGain.setCurrentAndTargetValue(params.masterGain.load(std::memory_order_relaxed));

        resetInternalEnvelopes();
        updateCoefficients();
        updateMonitorGraphicEQCoefficients();
        updateMainOutputEQCoefficients();

        eightBandEQ.prepare(currentSampleRate, currentBlockSize);
        vocalRack.setEQ(&eightBandEQ);
        vocalRack.prepare(currentSampleRate, currentBlockSize);
    }

    void reset()
    {
        puzuMix = 0.0f;
        mainOutputEQMix = 0.0f;
        mainOutputEQSoloMix = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            hpfFilterStage1[ch].reset();
            hpfFilterStage2[ch].reset();
            lpfFilterStage1[ch].reset();
            lpfFilterStage2[ch].reset();

            for (int b = 0; b < numMainOutputEQBands; ++b)
            {
                mainOutputEQDynamicFilters[ch][b].reset();
            }
            mainOutputEQSoloFilters[ch].reset();

            legacyEqLowFilter[ch].reset();
            legacyEqMid1Filter[ch].reset();
            legacyEqMid2Filter[ch].reset();
            legacyEqHighFilter[ch].reset();
        }
        puzuBandFilters.reset();
        monitorGraphicEQFilters.reset();
        mainOutputEQFilters.reset();
        reverb.reset();
        delayBuffer.clear();
        timeAlignBuffer.clear();
        delayWritePos = 0;
        timeAlignWritePos = 0;
        puzuPreAnalyzerFifo.reset();
        puzuPostAnalyzerFifo.reset();
        monitorGraphicEQPreAnalyzerFifo.reset();
        monitorGraphicEQPostAnalyzerFifo.reset();
        mainOutputEQPreAnalyzerFifo.reset();
        mainOutputEQPostAnalyzerFifo.reset();
        currentDelaySamples = params.delayTimeMs.load(std::memory_order_relaxed) *
                              0.001f * (float)currentSampleRate;
        targetDelaySamples = currentDelaySamples;
        smoothedMasterGain.setCurrentAndTargetValue(params.masterGain.load(std::memory_order_relaxed));
        resetInternalEnvelopes();
        eightBandEQ.reset();
        vocalRack.reset();

        peakL.store(0.0f);
        peakR.store(0.0f);
        clipping.store(false);
    }

    void updateCoefficients()
    {
        // Clamp delay feedback to 0.90 to strictly prevent runaway oscillation
        if (params.delayFeedback.load(std::memory_order_relaxed) > 0.90f)
            params.delayFeedback.store(0.90f, std::memory_order_relaxed);

        // 1. High Pass Filter (up to 24dB/Oct)
        float hpfF = juce::jlimit(20.0f, (float)(currentSampleRate * 0.49), params.hpfFreq.load(std::memory_order_relaxed));
        int hpfSlope = params.hpfSlope.load(std::memory_order_relaxed);
        auto hpfCoeff1 = (hpfSlope == 0)
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, hpfF)
            : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfF, (hpfSlope >= 3) ? 0.5412f : 0.7071f);
        auto hpfCoeff2 = (hpfSlope == 2)
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, hpfF)
            : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfF, 1.3065f);

        // 2. Low Pass Filter (up to 24dB/Oct)
        const float lpfMaxFreq = juce::jmin(22000.0f, (float)(currentSampleRate * 0.49));
        const float lpfF = juce::jlimit(juce::jmin(1000.0f, lpfMaxFreq), lpfMaxFreq,
                                        params.lpfFreq.load(std::memory_order_relaxed));
        const int lpfSlope = juce::jlimit(0, 4, params.lpfSlope.load(std::memory_order_relaxed));
        auto lpfCoeff1 = lpfSlope <= 1
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(currentSampleRate, lpfF)
            : juce::dsp::IIR::Coefficients<float>::makeLowPass(
                  currentSampleRate, lpfF, lpfSlope == 4 ? 0.5412f : 0.7071f);
        auto lpfCoeff2 = lpfSlope == 3
            ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(currentSampleRate, lpfF)
            : juce::dsp::IIR::Coefficients<float>::makeLowPass(
                  currentSampleRate, lpfF, lpfSlope == 4 ? 1.3065f : 0.7071f);

        auto puzuCoeffs = makePuzuCoefficients();

        // 4. Legacy 4-Band EQ (Low Shelf, Mid 1, Mid 2, High Shelf)
        float lowF = juce::jlimit(20.0f, 800.0f, params.eqLowFreq.load(std::memory_order_relaxed));
        float lowG = juce::Decibels::decibelsToGain(params.eqLowGainDb.load(std::memory_order_relaxed));
        auto lowCoeff = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, lowF, 0.707f, lowG);

        float mid1F = juce::jlimit(100.0f, 5000.0f, params.eqMid1Freq.load(std::memory_order_relaxed));
        float mid1Q = juce::jlimit(0.1f, 10.0f, params.eqMid1Q.load(std::memory_order_relaxed));
        float mid1G = juce::Decibels::decibelsToGain(params.eqMid1GainDb.load(std::memory_order_relaxed));
        auto mid1Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, mid1F, mid1Q, mid1G);

        float mid2F = juce::jlimit(200.0f, 16000.0f, params.eqFreq.load(std::memory_order_relaxed));
        float mid2Q = juce::jlimit(0.1f, 10.0f, params.eqQ.load(std::memory_order_relaxed));
        float mid2G = juce::Decibels::decibelsToGain(params.eqGainDb.load(std::memory_order_relaxed));
        auto mid2Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, mid2F, mid2Q, mid2G);

        float highF = juce::jlimit(2000.0f, 20000.0f, params.eqHighFreq.load(std::memory_order_relaxed));
        float highG = juce::Decibels::decibelsToGain(params.eqHighGainDb.load(std::memory_order_relaxed));
        auto highCoeff = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, highF, 0.707f, highG);

        for (int ch = 0; ch < 2; ++ch)
        {
            *hpfFilterStage1[ch].coefficients = *hpfCoeff1;
            *hpfFilterStage2[ch].coefficients = *hpfCoeff2;

            *lpfFilterStage1[ch].coefficients = *lpfCoeff1;
            *lpfFilterStage2[ch].coefficients = *lpfCoeff2;

            *legacyEqLowFilter[ch].coefficients = *lowCoeff;
            *legacyEqMid1Filter[ch].coefficients = *mid1Coeff;
            *legacyEqMid2Filter[ch].coefficients = *mid2Coeff;
            *legacyEqHighFilter[ch].coefficients = *highCoeff;
        }
        publishPuzuCoefficients(puzuCoeffs);

        lastHpfFreq = hpfF;
        lastHpfSlope = hpfSlope;
        lastLpfFreq = lpfF;
        lastLpfSlope = lpfSlope;

        // Reverb Parameters
        juce::dsp::Reverb::Parameters rParams;
        rParams.roomSize = juce::jlimit(0.0f, 1.0f, params.reverbRoomSize.load(std::memory_order_relaxed));
        rParams.damping = juce::jlimit(0.0f, 1.0f, params.reverbDamping.load(std::memory_order_relaxed));
        rParams.wetLevel = juce::jlimit(0.0f, 1.0f, params.reverbWetLevel.load(std::memory_order_relaxed));
        rParams.dryLevel = juce::jlimit(0.0f, 1.0f, params.reverbDryLevel.load(std::memory_order_relaxed));
        rParams.width = juce::jlimit(0.0f, 1.0f, params.reverbWidth.load(std::memory_order_relaxed));
        rParams.freezeMode = 0.0f;
        reverb.setParameters(rParams);
    }

    void updatePuzuCoefficients()
    {
        publishPuzuCoefficients(makePuzuCoefficients());
    }

    void updateMonitorGraphicEQCoefficients()
    {
        std::array<std::array<float, 5>, numMonitorGraphicEQBands> coefficients{};
        const float maxFrequency = juce::jmax(20.0f,
            (float)(currentSampleRate * 0.495));
        for (int band = 0; band < numMonitorGraphicEQBands; ++band)
        {
            const float frequency = juce::jmin(
                monitorGraphicEQFrequencies[(size_t)band], maxFrequency);
            const float gainDb = juce::jlimit(-12.0f, 12.0f,
                params.monitorGraphicEQGainDb[(size_t)band].load(std::memory_order_relaxed));
            const auto filter = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                currentSampleRate, frequency, 4.318f,
                juce::Decibels::decibelsToGain(gainDb));
            const auto* raw = filter->getRawCoefficients();
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                coefficients[(size_t)band][(size_t)coefficient] = raw[coefficient];

        }
        publishMonitorGraphicEQCoefficients(coefficients);
    }

    void updateMainOutputEQCoefficients()
    {
        std::array<std::array<float, 5>, numMainOutputEQBands> coefficients{};
        std::array<std::array<float, 5>, numMainOutputEQBands> dynamicCoefficients{};
        std::array<float, 5> soloCoefficients{};
        int soloBand = juce::jlimit(-1, numMainOutputEQBands - 1,
            params.mainOutputEQSoloBand.load(std::memory_order_relaxed));
        const float maxFrequency = juce::jmax(20.0f,
            (float)(currentSampleRate * 0.495));
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            const float frequency = std::isfinite(params.mainOutputEQFreq[(size_t)band].load(
                std::memory_order_relaxed))
                ? juce::jlimit(20.0f, maxFrequency,
                    params.mainOutputEQFreq[(size_t)band].load(std::memory_order_relaxed))
                : mainOutputEQFrequencies[(size_t)band];
            const float q = std::isfinite(params.mainOutputEQQ[(size_t)band].load(
                std::memory_order_relaxed))
                ? juce::jlimit(0.1f, 10.0f,
                    params.mainOutputEQQ[(size_t)band].load(std::memory_order_relaxed))
                : 0.707f;
            const float gainDb = std::isfinite(params.mainOutputEQGainDb[(size_t)band].load(
                std::memory_order_relaxed))
                ? juce::jlimit(-18.0f, 18.0f,
                    params.mainOutputEQGainDb[(size_t)band].load(std::memory_order_relaxed))
                : 0.0f;
            const float gain = juce::Decibels::decibelsToGain(gainDb);
            juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> filter;
            switch (params.mainOutputEQType[(size_t)band].load(std::memory_order_relaxed))
            {
                case (int)MainOutputEQFilterType::HighPass:
                    filter = juce::dsp::IIR::Coefficients<float>::makeHighPass(
                        currentSampleRate, frequency, q);
                    break;
                case (int)MainOutputEQFilterType::LowShelf:
                    filter = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                        currentSampleRate, frequency, q, gain);
                    break;
                case (int)MainOutputEQFilterType::HighShelf:
                    filter = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                        currentSampleRate, frequency, q, gain);
                    break;
                case (int)MainOutputEQFilterType::Notch:
                    filter = juce::dsp::IIR::Coefficients<float>::makeNotch(
                        currentSampleRate, frequency, q);
                    break;
                case (int)MainOutputEQFilterType::LowPass:
                    filter = juce::dsp::IIR::Coefficients<float>::makeLowPass(
                        currentSampleRate, frequency, q);
                    break;
                case (int)MainOutputEQFilterType::Bell:
                default:
                    filter = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                        currentSampleRate, frequency, q, gain);
                    break;
            }
            const auto* raw = filter->getRawCoefficients();
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                coefficients[(size_t)band][(size_t)coefficient] = raw[coefficient];

            const auto dynamicFilter =
                juce::dsp::IIR::Coefficients<float>::makeBandPass(
                    currentSampleRate, frequency, q);
            const auto* dynamicRaw = dynamicFilter->getRawCoefficients();
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                dynamicCoefficients[(size_t)band][(size_t)coefficient] =
                    dynamicRaw[coefficient];
        }
        if (soloBand >= 0)
        {
            const float frequency = juce::jlimit(20.0f, maxFrequency,
                params.mainOutputEQFreq[(size_t)soloBand].load(std::memory_order_relaxed));
            const float q = juce::jlimit(0.1f, 10.0f,
                params.mainOutputEQQ[(size_t)soloBand].load(std::memory_order_relaxed));
            const auto bandPass =
                juce::dsp::IIR::Coefficients<float>::makeBandPass(
                    currentSampleRate, frequency, q);
            const auto* raw = bandPass->getRawCoefficients();
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                soloCoefficients[(size_t)coefficient] = raw[coefficient];
        }
        publishMainOutputEQCoefficients(coefficients, dynamicCoefficients,
                                        soloCoefficients, soloBand);
    }

    void setMainOutputEQSoloBand(int band)
    {
        params.mainOutputEQSoloBand.store(
            juce::jlimit(-1, numMainOutputEQBands - 1, band),
            std::memory_order_relaxed);
        updateMainOutputEQCoefficients();
    }

    int getMainOutputEQSoloBand() const noexcept
    {
        return params.mainOutputEQSoloBand.load(std::memory_order_relaxed);
    }

    float getMainOutputEQSoloMagnitudeDb(float frequency) const noexcept
    {
        if (!std::isfinite(frequency) || frequency <= 0.0f)
            return -120.0f;
        std::array<float, 5> coefficients{};
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto sequenceBefore =
                mainOutputEQSequence.load(std::memory_order_acquire);
            if ((sequenceBefore & 1u) != 0u)
                continue;
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                coefficients[(size_t)coefficient] =
                    mainOutputEQSoloCoefficientSnapshot[(size_t)coefficient]
                        .load(std::memory_order_relaxed);
            if (sequenceBefore != mainOutputEQSequence.load(std::memory_order_acquire))
                continue;
            const double angle = -juce::MathConstants<double>::twoPi *
                                 (double)frequency / currentSampleRate;
            const double cosine1 = std::cos(angle), sine1 = std::sin(angle);
            const double cosine2 = std::cos(2.0 * angle), sine2 = std::sin(2.0 * angle);
            const double numeratorReal =
                coefficients[0] + coefficients[1] * cosine1 +
                coefficients[2] * cosine2;
            const double numeratorImag =
                coefficients[1] * sine1 + coefficients[2] * sine2;
            const double denominatorReal =
                1.0 + coefficients[3] * cosine1 + coefficients[4] * cosine2;
            const double denominatorImag =
                coefficients[3] * sine1 + coefficients[4] * sine2;
            const double denominatorMagnitude =
                denominatorReal * denominatorReal + denominatorImag * denominatorImag;
            if (denominatorMagnitude <= 0.0)
                return -120.0f;
            const double magnitude = std::sqrt(
                (numeratorReal * numeratorReal + numeratorImag * numeratorImag) /
                denominatorMagnitude);
            return (float)juce::Decibels::gainToDecibels((float)magnitude, -120.0f);
        }
        return -120.0f;
    }

    MainOutputEQ24State getMainOutputEQ24State() const noexcept
    {
        MainOutputEQ24State state;
        state.enabled = params.mainOutputEQEnabled.load(std::memory_order_relaxed);
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            auto& destination = state.bands[(size_t)band];
            destination.freq = params.mainOutputEQFreq[(size_t)band].load(std::memory_order_relaxed);
            destination.q = params.mainOutputEQQ[(size_t)band].load(std::memory_order_relaxed);
            destination.gainDb = params.mainOutputEQGainDb[(size_t)band].load(std::memory_order_relaxed);
            const int type = params.mainOutputEQType[(size_t)band].load(std::memory_order_relaxed);
            destination.type = type >= (int)MainOutputEQFilterType::HighPass &&
                               type <= (int)MainOutputEQFilterType::LowPass
                ? (MainOutputEQFilterType)type : MainOutputEQFilterType::Bell;
            destination.enabled =
                params.mainOutputEQBandEnabled[(size_t)band].load(std::memory_order_relaxed);
            destination.dynamicEnabled =
                params.mainOutputEQDynamicEnabled[(size_t)band].load(std::memory_order_relaxed);
            destination.dynamicThresholdDb =
                params.mainOutputEQDynamicThresholdDb[(size_t)band].load(std::memory_order_relaxed);
            destination.dynamicAttackMs =
                params.mainOutputEQDynamicAttackMs[(size_t)band].load(std::memory_order_relaxed);
            destination.dynamicReleaseMs =
                params.mainOutputEQDynamicReleaseMs[(size_t)band].load(std::memory_order_relaxed);
            destination.dynamicRangeDb =
                params.mainOutputEQDynamicRangeDb[(size_t)band].load(std::memory_order_relaxed);
            const int detector =
                params.mainOutputEQDynamicDetector[(size_t)band].load(std::memory_order_relaxed);
            destination.dynamicDetector = detector == (int)MainOutputEQDetector::RMS
                ? MainOutputEQDetector::RMS : MainOutputEQDetector::Peak;
        }
        return state;
    }

    void setMainOutputEQ24State(const MainOutputEQ24State& state)
    {
        params.mainOutputEQEnabled.store(state.enabled, std::memory_order_relaxed);
        const float maxFrequency = juce::jmax(20.0f, (float)(currentSampleRate * 0.495));
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            const auto& source = state.bands[(size_t)band];
            const int type = (int)source.type;
            params.mainOutputEQFreq[(size_t)band].store(
                std::isfinite(source.freq)
                    ? juce::jlimit(20.0f, maxFrequency, source.freq)
                    : mainOutputEQFrequencies[(size_t)band],
                std::memory_order_relaxed);
            params.mainOutputEQQ[(size_t)band].store(
                std::isfinite(source.q) ? juce::jlimit(0.1f, 10.0f, source.q) : 0.707f,
                std::memory_order_relaxed);
            params.mainOutputEQGainDb[(size_t)band].store(
                std::isfinite(source.gainDb)
                    ? juce::jlimit(-18.0f, 18.0f, source.gainDb) : 0.0f,
                std::memory_order_relaxed);
            params.mainOutputEQType[(size_t)band].store(
                type >= (int)MainOutputEQFilterType::HighPass &&
                        type <= (int)MainOutputEQFilterType::LowPass
                    ? type : (int)MainOutputEQFilterType::Bell,
                std::memory_order_relaxed);
            params.mainOutputEQBandEnabled[(size_t)band].store(
                source.enabled, std::memory_order_relaxed);
            params.mainOutputEQDynamicEnabled[(size_t)band].store(
                source.dynamicEnabled, std::memory_order_relaxed);
            params.mainOutputEQDynamicThresholdDb[(size_t)band].store(
                std::isfinite(source.dynamicThresholdDb)
                    ? juce::jlimit(-80.0f, 0.0f, source.dynamicThresholdDb) : -24.0f,
                std::memory_order_relaxed);
            params.mainOutputEQDynamicAttackMs[(size_t)band].store(
                std::isfinite(source.dynamicAttackMs)
                    ? juce::jlimit(0.1f, 100.0f, source.dynamicAttackMs) : 10.0f,
                std::memory_order_relaxed);
            params.mainOutputEQDynamicReleaseMs[(size_t)band].store(
                std::isfinite(source.dynamicReleaseMs)
                    ? juce::jlimit(5.0f, 1000.0f, source.dynamicReleaseMs) : 100.0f,
                std::memory_order_relaxed);
            params.mainOutputEQDynamicRangeDb[(size_t)band].store(
                std::isfinite(source.dynamicRangeDb)
                    ? juce::jlimit(0.0f, 18.0f, source.dynamicRangeDb) : 6.0f,
                std::memory_order_relaxed);
            params.mainOutputEQDynamicDetector[(size_t)band].store(
                source.dynamicDetector == MainOutputEQDetector::RMS
                    ? (int)MainOutputEQDetector::RMS : (int)MainOutputEQDetector::Peak,
                std::memory_order_relaxed);
        }
        updateMainOutputEQCoefficients();
    }

    float getMainOutputEQ24MagnitudeDb(float frequency) const noexcept
    {
        if (!std::isfinite(frequency) || frequency <= 0.0f)
            return 0.0f;
        std::array<std::array<float, 5>, numMainOutputEQBands> coefficients{};
        if (!readMainOutputEQCoefficientSnapshot(coefficients))
            return 0.0f;
        double magnitude = 1.0;
        const double angle = -juce::MathConstants<double>::twoPi *
                             (double)frequency / currentSampleRate;
        const double cos1 = std::cos(angle), sin1 = std::sin(angle);
        const double cos2 = std::cos(2.0 * angle), sin2 = std::sin(2.0 * angle);
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            if (!params.mainOutputEQBandEnabled[(size_t)band].load(
                    std::memory_order_relaxed))
                continue;
            const auto& c = coefficients[(size_t)band];
            const double numeratorReal = c[0] + c[1] * cos1 + c[2] * cos2;
            const double numeratorImag = c[1] * sin1 + c[2] * sin2;
            const double denominatorReal = 1.0 + c[3] * cos1 + c[4] * cos2;
            const double denominatorImag = c[3] * sin1 + c[4] * sin2;
            const double denominatorMagnitude = denominatorReal * denominatorReal +
                                                denominatorImag * denominatorImag;
            if (denominatorMagnitude > 0.0)
                magnitude *= std::sqrt((numeratorReal * numeratorReal +
                                        numeratorImag * numeratorImag) /
                                       denominatorMagnitude);
        }
        return (float)juce::Decibels::gainToDecibels((float)magnitude, -120.0f);
    }

    MonitorGraphicEQ31State getMonitorGraphicEQState() const noexcept
    {
        MonitorGraphicEQ31State state;
        state.enabled = params.monitorGraphicEQEnabled.load(std::memory_order_relaxed);
        for (int band = 0; band < numMonitorGraphicEQBands; ++band)
            state.gainDb[(size_t)band] =
                params.monitorGraphicEQGainDb[(size_t)band].load(std::memory_order_relaxed);
        return state;
    }
    void setMonitorGraphicEQAnalyzerEnabled(bool enabled) noexcept
    {
        monitorGraphicEQAnalyzerEnabled.store(enabled, std::memory_order_relaxed);
    }
    bool pullMonitorGraphicEQPreAnalyzer(float* destination, int numSamples)
    {
        return pullMonitorGraphicEQAnalyzer(
            monitorGraphicEQPreAnalyzerFifo, monitorGraphicEQPreAnalyzerLeftBuffer,
            monitorGraphicEQPreAnalyzerRightBuffer, destination, numSamples);
    }
    bool pullMonitorGraphicEQPostAnalyzer(float* destination, int numSamples)
    {
        return pullMonitorGraphicEQAnalyzer(
            monitorGraphicEQPostAnalyzerFifo, monitorGraphicEQPostAnalyzerLeftBuffer,
            monitorGraphicEQPostAnalyzerRightBuffer, destination, numSamples);
    }
    bool pullMonitorGraphicEQPreAnalyzerStereo(float* leftDestination,
                                               float* rightDestination,
                                               int numSamples)
    {
        return pullMonitorGraphicEQAnalyzerStereo(
            monitorGraphicEQPreAnalyzerFifo,
            monitorGraphicEQPreAnalyzerLeftBuffer,
            monitorGraphicEQPreAnalyzerRightBuffer,
            leftDestination, rightDestination, numSamples);
    }
    bool pullMonitorGraphicEQPostAnalyzerStereo(float* leftDestination,
                                                float* rightDestination,
                                                int numSamples)
    {
        return pullMonitorGraphicEQAnalyzerStereo(
            monitorGraphicEQPostAnalyzerFifo,
            monitorGraphicEQPostAnalyzerLeftBuffer,
            monitorGraphicEQPostAnalyzerRightBuffer,
            leftDestination, rightDestination, numSamples);
    }

    void setMonitorGraphicEQState(const MonitorGraphicEQ31State& state)
    {
        params.monitorGraphicEQEnabled.store(state.enabled, std::memory_order_relaxed);
        for (int band = 0; band < numMonitorGraphicEQBands; ++band)
        {
            const float gainDb = state.gainDb[(size_t)band];
            params.monitorGraphicEQGainDb[(size_t)band].store(
                std::isfinite(gainDb) ? juce::jlimit(-12.0f, 12.0f, gainDb) : 0.0f,
                std::memory_order_relaxed);
        }
        updateMonitorGraphicEQCoefficients();
    }

    float getMonitorGraphicEQMagnitudeDb(float frequency) const noexcept
    {
        if (!std::isfinite(frequency) || frequency <= 0.0f)
            return 0.0f;
        std::array<std::array<float, 5>, numMonitorGraphicEQBands> coefficients{};
        if (!readMonitorGraphicEQCoefficientSnapshot(coefficients))
            return 0.0f;
        double magnitude = 1.0;
        const double angle = -juce::MathConstants<double>::twoPi *
                             (double)frequency / currentSampleRate;
        const double cos1 = std::cos(angle);
        const double sin1 = std::sin(angle);
        const double cos2 = std::cos(2.0 * angle);
        const double sin2 = std::sin(2.0 * angle);
        for (const auto& c : coefficients)
        {
            const double numeratorReal = c[0] + c[1] * cos1 + c[2] * cos2;
            const double numeratorImag = c[1] * sin1 + c[2] * sin2;
            const double denominatorReal = 1.0 + c[3] * cos1 + c[4] * cos2;
            const double denominatorImag = c[3] * sin1 + c[4] * sin2;
            const double denominatorMagnitude = denominatorReal * denominatorReal +
                                                denominatorImag * denominatorImag;
            if (denominatorMagnitude > 0.0)
                magnitude *= std::sqrt((numeratorReal * numeratorReal +
                                        numeratorImag * numeratorImag) /
                                       denominatorMagnitude);
        }
        return (float)juce::Decibels::gainToDecibels((float)magnitude, -120.0f);
    }

    PuzuEQState getPuzuEQState() const noexcept
    {
        PuzuEQState state;
        state.enabled = params.puzuEnabled.load(std::memory_order_relaxed);
        for (int band = 0; band < numPuzuBands; ++band)
        {
            const auto& source = params.puzuBands[(size_t)band];
            auto& destination = state.bands[(size_t)band];
            destination.freq = source.freq.load(std::memory_order_relaxed);
            destination.q = source.q.load(std::memory_order_relaxed);
            destination.gainDb = source.gainDb.load(std::memory_order_relaxed);
            destination.type = source.type.load(std::memory_order_relaxed);
            destination.enabled = source.enabled.load(std::memory_order_relaxed);
        }
        return state;
    }

    void setPuzuEQState(const PuzuEQState& state)
    {
        params.puzuEnabled.store(state.enabled, std::memory_order_relaxed);
        const float maxFrequency = juce::jmax(20.0f,
            juce::jmin(20000.0f, (float)(currentSampleRate * 0.495)));
        for (int band = 0; band < numPuzuBands; ++band)
        {
            const auto& source = state.bands[(size_t)band];
            auto& destination = params.puzuBands[(size_t)band];
            const float fallbackFrequency = defaultPuzuFreqs[band];
            destination.freq.store(std::isfinite(source.freq)
                ? juce::jlimit(20.0f, maxFrequency, source.freq)
                : juce::jlimit(20.0f, maxFrequency, fallbackFrequency),
                std::memory_order_relaxed);
            destination.q.store(std::isfinite(source.q)
                ? juce::jlimit(0.1f, 10.0f, source.q)
                : defaultPuzuQs[band],
                std::memory_order_relaxed);
            destination.gainDb.store(std::isfinite(source.gainDb)
                ? juce::jlimit(-18.0f, 18.0f, source.gainDb)
                : 0.0f,
                std::memory_order_relaxed);
            const int filterType = (int)source.type;
            destination.type.store(filterType >= (int)PuzuFilterType::HighPass &&
                                   filterType <= (int)PuzuFilterType::Notch
                ? source.type : PuzuFilterType::Bell,
                std::memory_order_relaxed);
            destination.enabled.store(source.enabled, std::memory_order_relaxed);
        }
        updatePuzuCoefficients();
    }

    float getMagnitudeForFrequency(float frequency) const
    {
        float mag = 1.0f;

        // High Pass Filter magnitude
        if (params.hpfEnabled.load(std::memory_order_relaxed))
        {
            if (hpfFilterStage1[0].coefficients)
                mag *= (float)hpfFilterStage1[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (params.hpfSlope.load(std::memory_order_relaxed) >= 2 && hpfFilterStage2[0].coefficients)
                mag *= (float)hpfFilterStage2[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
        }

        // Low Pass Filter magnitude
        if (params.lpfEnabled.load(std::memory_order_relaxed) && params.lpfSlope.load(std::memory_order_relaxed) > 0)
        {
            if (lpfFilterStage1[0].coefficients)
                mag *= (float)lpfFilterStage1[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (params.lpfSlope.load(std::memory_order_relaxed) >= 3 && lpfFilterStage2[0].coefficients)
                mag *= (float)lpfFilterStage2[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
        }

        // 15-Band Parametric Equalizer magnitude
        if (params.puzuEnabled.load(std::memory_order_relaxed))
        {
            std::array<std::array<float, 5>, numPuzuBands> coefficients{};
            if (readPuzuCoefficientSnapshot(coefficients))
            for (int b = 0; b < numPuzuBands; ++b)
            {
                if (!params.puzuBands[b].enabled.load(std::memory_order_relaxed))
                    continue;

                const auto& c = coefficients[(size_t)b];
                const double angle = -juce::MathConstants<double>::twoPi *
                                     (double)frequency / currentSampleRate;
                const double cos1 = std::cos(angle);
                const double sin1 = std::sin(angle);
                const double cos2 = std::cos(2.0 * angle);
                const double sin2 = std::sin(2.0 * angle);
                const double numeratorReal = c[0] + c[1] * cos1 + c[2] * cos2;
                const double numeratorImag = c[1] * sin1 + c[2] * sin2;
                const double denominatorReal = 1.0 + c[3] * cos1 + c[4] * cos2;
                const double denominatorImag = c[3] * sin1 + c[4] * sin2;
                const double denominatorMagnitude = denominatorReal * denominatorReal +
                                                    denominatorImag * denominatorImag;
                if (denominatorMagnitude > 0.0)
                    mag *= (float)std::sqrt((numeratorReal * numeratorReal +
                                             numeratorImag * numeratorImag) /
                                            denominatorMagnitude);
            }
        }

        if (params.eqEnabled.load(std::memory_order_relaxed))
        {
            if (legacyEqLowFilter[0].coefficients)
                mag *= (float)legacyEqLowFilter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (legacyEqMid1Filter[0].coefficients)
                mag *= (float)legacyEqMid1Filter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (legacyEqMid2Filter[0].coefficients)
                mag *= (float)legacyEqMid2Filter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
            if (legacyEqHighFilter[0].coefficients)
                mag *= (float)legacyEqHighFilter[0].coefficients->getMagnitudeForFrequency(frequency, currentSampleRate);
        }

        return juce::Decibels::gainToDecibels(mag, -40.0f);
    }

    void process(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        juce::ScopedNoDenormals noDenormals;

        if (numSamples <= 0 || buffer.getNumChannels() < 1) return;

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // 0. Protect against non-finite values (NaN / Inf)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (!std::isfinite(data[i]))
                    data[i] = 0.0f;
            }
        }

        // Measure input peak level
        float maxIn = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float m = buffer.getMagnitude(ch, 0, numSamples);
            if (m > maxIn) maxIn = m;
        }
        float inDb = (maxIn > 0.00001f) ? juce::Decibels::gainToDecibels(maxIn, -100.0f) : -100.0f;
        inPeakDb.store(inDb, std::memory_order_relaxed);

        // Phase Inversion
        if (params.phaseInverted.load(std::memory_order_relaxed))
        {
            buffer.applyGain(-1.0f);
        }

        // Time Alignment Delay
        float alignMs = params.timeDelayMs.load(std::memory_order_relaxed);
        if (alignMs > 0.01f)
        {
            int alignSamples = juce::jlimit(0, timeAlignBuffer.getNumSamples() - 1, (int)(alignMs * 0.001f * currentSampleRate));
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* in = buffer.getReadPointer(ch);
                float* out = buffer.getWritePointer(ch);
                float* delayData = timeAlignBuffer.getWritePointer(ch);
                int bufSize = timeAlignBuffer.getNumSamples();

                for (int s = 0; s < numSamples; ++s)
                {
                    int readPos = timeAlignWritePos - alignSamples + s;
                    while (readPos < 0) readPos += bufSize;
                    while (readPos >= bufSize) readPos -= bufSize;

                    int writePos = (timeAlignWritePos + s) % bufSize;
                    delayData[writePos] = in[s];
                    out[s] = delayData[readPos];
                }
            }
            timeAlignWritePos = (timeAlignWritePos + numSamples) % timeAlignBuffer.getNumSamples();
        }

        // Check if Waves eMo D5 hardware vocal rack is explicitly enabled
        if (vocalRack.params.masterEnabled.load(std::memory_order_relaxed))
        {
            vocalRack.process(buffer, numSamples);
            processPuzuEQ(buffer, numSamples, numChannels);
            processMainOutputEQ(buffer, numSamples, numChannels);
            processMonitorGraphicEQ(buffer, numSamples, numChannels);
            gateGR.store(vocalRack.getGateGainReductionDb(), std::memory_order_relaxed);
            compGR.store(vocalRack.getCompGainReductionDb(), std::memory_order_relaxed);
            limiterGR.store(vocalRack.getLimiterGainReductionDb(), std::memory_order_relaxed);
        }
        else
        {
            // Standard LiveBus Mixer 7-Stage DSP Signal Chain:
            // 1. High-Pass Filter (HPF)
            if (params.hpfEnabled.load(std::memory_order_relaxed))
            {
                processHPF(buffer, numSamples);
            }

            // 2. Low-Pass Filter (LPF)
            if (params.lpfEnabled.load(std::memory_order_relaxed) &&
                params.lpfSlope.load(std::memory_order_relaxed) > 0)
            {
                processLPF(buffer, numSamples);
            }

            // 3. Noise Gate / Expander
            if (params.gateEnabled.load(std::memory_order_relaxed))
            {
                processGate(buffer, numSamples);
            }
            else
            {
                gateGR.store(0.0f, std::memory_order_relaxed);
                gateCurrentState.store(2, std::memory_order_relaxed); // Open when disabled
            }

            // 4. Independent 15-band PUZU and legacy 4-band EQ stages.
            processPuzuEQ(buffer, numSamples, numChannels);

            if (params.eqEnabled.load(std::memory_order_relaxed))
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    juce::dsp::AudioBlock<float> block(buffer);
                    auto singleBlock = block.getSingleChannelBlock((size_t)ch).getSubBlock(0, (size_t)numSamples);
                    juce::dsp::ProcessContextReplacing<float> context(singleBlock);

                    legacyEqLowFilter[ch].process(context);
                    legacyEqMid1Filter[ch].process(context);
                    legacyEqMid2Filter[ch].process(context);
                    legacyEqHighFilter[ch].process(context);
                }
            }

            eightBandEQ.process(buffer, numSamples);
            processMainOutputEQ(buffer, numSamples, numChannels);

            // 5. Dynamics Compressor
            if (params.compEnabled.load(std::memory_order_relaxed))
            {
                processCompressor(buffer, numSamples);
            }
            else
            {
                compGR.store(0.0f, std::memory_order_relaxed);
            }

            // 6. Studio Reverb
            if (params.reverbEnabled.load(std::memory_order_relaxed))
            {
                processReverb(buffer, numSamples);
            }

            // 7. Stereo Creative Delay
            if (params.delayEnabled.load(std::memory_order_relaxed))
            {
                processDelay(buffer, numSamples);
            }

            processMonitorGraphicEQ(buffer, numSamples, numChannels);

            // 8. Brickwall Peak Limiter
            if (params.limiterEnabled.load(std::memory_order_relaxed))
            {
                processLimiter(buffer, numSamples);
            }
            else
            {
                limiterGR.store(0.0f, std::memory_order_relaxed);
            }
        }

        // 8. Master Gain, Pan, and Mute
        if (params.masterMute.load(std::memory_order_relaxed))
        {
            buffer.clear(0, numSamples);
            peakL.store(0.0f, std::memory_order_relaxed);
            peakR.store(0.0f, std::memory_order_relaxed);
            outPeakDb.store(-100.0f, std::memory_order_relaxed);
            clipping.store(false, std::memory_order_relaxed);
            return;
        }

        float targetGain = params.masterGain.load(std::memory_order_relaxed);
        if (params.masterDim.load(std::memory_order_relaxed))
            targetGain *= juce::Decibels::decibelsToGain(-20.0f);
        smoothedMasterGain.setTargetValue(targetGain);

        float pan = juce::jlimit(-1.0f, 1.0f, params.masterPan.load(std::memory_order_relaxed));
        float panGainL = 1.0f;
        float panGainR = 1.0f;
        if (std::abs(pan) > 0.0001f)
        {
            float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // -3dB equal power
            panGainL = std::cos(panAngle) * 1.41421356f;
            panGainR = std::sin(panAngle) * 1.41421356f;
        }

        float* left = buffer.getWritePointer(0);
        float* right = (numChannels > 1) ? buffer.getWritePointer(1) : left;

        float maxL = 0.0f;
        float maxR = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float g = smoothedMasterGain.getNextValue();
            left[i] *= (g * panGainL);
            if (numChannels > 1)
                right[i] *= (g * panGainR);

            float absL = std::abs(left[i]);
            float absR = (numChannels > 1) ? std::abs(right[i]) : absL;
            if (absL > maxL) maxL = absL;
            if (absR > maxR) maxR = absR;
        }

        peakL.store(maxL, std::memory_order_relaxed);
        peakR.store(maxR, std::memory_order_relaxed);

        float outMax = juce::jmax(maxL, maxR);
        float outDb = (outMax > 0.00001f) ? juce::Decibels::gainToDecibels(outMax, -100.0f) : -100.0f;
        outPeakDb.store(outDb, std::memory_order_relaxed);

        bool clip = (maxL >= 0.999f || maxR >= 0.999f);
        if (clip) clipping.store(true, std::memory_order_relaxed);
    }

    // Real-time safe telemetry and metrics
    float getGateGainReductionDb() const noexcept { return gateGR.load(std::memory_order_relaxed); }
    float getCompGainReductionDb() const noexcept { return compGR.load(std::memory_order_relaxed); }
    float getLimiterGainReductionDb() const noexcept { return limiterGR.load(std::memory_order_relaxed); }
    int getGateState() const noexcept { return gateCurrentState.load(std::memory_order_relaxed); }
    float getInputPeakDb() const noexcept { return inPeakDb.load(std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept { return outPeakDb.load(std::memory_order_relaxed); }
    float getPeakL() const noexcept { return peakL.load(std::memory_order_relaxed); }
    float getPeakR() const noexcept { return peakR.load(std::memory_order_relaxed); }
    bool isClipping() const noexcept { return clipping.load(std::memory_order_relaxed); }
    void resetClipping() noexcept { clipping.store(false, std::memory_order_relaxed); }
    double getSampleRate() const noexcept { return currentSampleRate; }
    void setPuzuAnalyzerEnabled(bool enabled) noexcept
    {
        puzuAnalyzerEnabled.store(enabled, std::memory_order_relaxed);
    }
    bool pullPuzuPreAnalyzer(float* destination, int numSamples)
    {
        return pullPuzuAnalyzerMono(
            puzuPreAnalyzerFifo, puzuPreAnalyzerLeftBuffer,
            puzuPreAnalyzerRightBuffer, destination, numSamples);
    }
    bool pullPuzuPostAnalyzer(float* destination, int numSamples)
    {
        return pullPuzuAnalyzerMono(
            puzuPostAnalyzerFifo, puzuPostAnalyzerLeftBuffer,
            puzuPostAnalyzerRightBuffer, destination, numSamples);
    }
    bool pullPuzuPreAnalyzerStereo(float* leftDestination,
                                   float* rightDestination, int numSamples)
    {
        return pullPuzuAnalyzerStereo(
            puzuPreAnalyzerFifo, puzuPreAnalyzerLeftBuffer,
            puzuPreAnalyzerRightBuffer, leftDestination,
            rightDestination, numSamples);
    }
    bool pullPuzuPostAnalyzerStereo(float* leftDestination,
                                    float* rightDestination, int numSamples)
    {
        return pullPuzuAnalyzerStereo(
            puzuPostAnalyzerFifo, puzuPostAnalyzerLeftBuffer,
            puzuPostAnalyzerRightBuffer, leftDestination,
            rightDestination, numSamples);
    }
    void setMainOutputEQAnalyzerEnabled(bool enabled) noexcept
    {
        mainOutputEQAnalyzerEnabled.store(enabled, std::memory_order_relaxed);
    }
    bool pullMainOutputEQPreAnalyzer(float* destination, int numSamples)
    {
        return pullMainOutputEQAnalyzerMono(
            mainOutputEQPreAnalyzerFifo, mainOutputEQPreAnalyzerLeftBuffer,
            mainOutputEQPreAnalyzerRightBuffer, destination, numSamples);
    }
    bool pullMainOutputEQPostAnalyzer(float* destination, int numSamples)
    {
        return pullMainOutputEQAnalyzerMono(
            mainOutputEQPostAnalyzerFifo, mainOutputEQPostAnalyzerLeftBuffer,
            mainOutputEQPostAnalyzerRightBuffer, destination, numSamples);
    }
    bool pullMainOutputEQPreAnalyzerStereo(float* leftDestination,
                                           float* rightDestination,
                                           int numSamples)
    {
        return pullMainOutputEQAnalyzerStereo(
            mainOutputEQPreAnalyzerFifo, mainOutputEQPreAnalyzerLeftBuffer,
            mainOutputEQPreAnalyzerRightBuffer, leftDestination,
            rightDestination, numSamples);
    }
    bool pullMainOutputEQPostAnalyzerStereo(float* leftDestination,
                                            float* rightDestination,
                                            int numSamples)
    {
        return pullMainOutputEQAnalyzerStereo(
            mainOutputEQPostAnalyzerFifo, mainOutputEQPostAnalyzerLeftBuffer,
            mainOutputEQPostAnalyzerRightBuffer, leftDestination,
            rightDestination, numSamples);
    }

    BusDSPParameters params;
    LiveBus::EightBandEQProcessor eightBandEQ;
    LiveBus::VocalDSPProcessor vocalRack;

private:
    using PuzuCoefficientSet =
        std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>,
                   numPuzuBands>;

    void publishMainOutputEQCoefficients(
        const std::array<std::array<float, 5>, numMainOutputEQBands>& coefficients,
        const std::array<std::array<float, 5>, numMainOutputEQBands>& dynamicCoefficients,
        const std::array<float, 5>& soloCoefficients, int soloBand)
    {
        const std::lock_guard<std::mutex> lock(mainOutputEQWriterMutex);
        mainOutputEQSequence.fetch_add(1, std::memory_order_acq_rel);
        for (int band = 0; band < numMainOutputEQBands; ++band)
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                mainOutputEQCoefficientSnapshot[(size_t)band][(size_t)coefficient].store(
                    coefficients[(size_t)band][(size_t)coefficient],
                    std::memory_order_relaxed);
        for (int band = 0; band < numMainOutputEQBands; ++band)
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                mainOutputEQDynamicCoefficientSnapshot[(size_t)band][(size_t)coefficient]
                    .store(dynamicCoefficients[(size_t)band][(size_t)coefficient],
                           std::memory_order_relaxed);
        for (int coefficient = 0; coefficient < 5; ++coefficient)
            mainOutputEQSoloCoefficientSnapshot[(size_t)coefficient].store(
                soloCoefficients[(size_t)coefficient], std::memory_order_relaxed);
        mainOutputEQSoloBandSnapshot.store(soloBand, std::memory_order_relaxed);
        mainOutputEQSequence.fetch_add(1, std::memory_order_release);
    }

    bool readMainOutputEQCoefficientSnapshot(
        std::array<std::array<float, 5>, numMainOutputEQBands>& destination) const noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto sequenceBefore = mainOutputEQSequence.load(std::memory_order_acquire);
            if ((sequenceBefore & 1u) != 0u)
                continue;
            for (int band = 0; band < numMainOutputEQBands; ++band)
                for (int coefficient = 0; coefficient < 5; ++coefficient)
                    destination[(size_t)band][(size_t)coefficient] =
                        mainOutputEQCoefficientSnapshot[(size_t)band][(size_t)coefficient]
                            .load(std::memory_order_relaxed);
            if (sequenceBefore == mainOutputEQSequence.load(std::memory_order_acquire))
                return true;
        }
        return false;
    }

    void applyMainOutputEQCoefficientSnapshot() noexcept
    {
        const auto sequence = mainOutputEQSequence.load(std::memory_order_acquire);
        if (sequence == audioMainOutputEQSequence || (sequence & 1u) != 0u)
            return;
        std::array<std::array<float, 5>, numMainOutputEQBands> snapshot{};
        std::array<std::array<float, 5>, numMainOutputEQBands> dynamicSnapshot{};
        std::array<float, 5> soloSnapshot{};
        if (!readMainOutputEQCoefficientSnapshot(snapshot))
            return;
        for (int band = 0; band < numMainOutputEQBands; ++band)
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                dynamicSnapshot[(size_t)band][(size_t)coefficient] =
                    mainOutputEQDynamicCoefficientSnapshot[(size_t)band][(size_t)coefficient]
                        .load(std::memory_order_relaxed);
        for (int coefficient = 0; coefficient < 5; ++coefficient)
            soloSnapshot[(size_t)coefficient] =
                mainOutputEQSoloCoefficientSnapshot[(size_t)coefficient]
                    .load(std::memory_order_relaxed);
        const int nextSoloBand =
            mainOutputEQSoloBandSnapshot.load(std::memory_order_relaxed);
        if (mainOutputEQSequence.load(std::memory_order_acquire) != sequence)
            return;
        if (!mainOutputEQFilters.beginTransition(
                snapshot, juce::roundToInt(currentSampleRate * 0.01)))
            return;
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int band = 0; band < numMainOutputEQBands; ++band)
            {
                auto* dynamicDestination =
                    mainOutputEQDynamicFilters[channel][band]
                        .coefficients->coefficients.begin();
                for (int coefficient = 0; coefficient < 5; ++coefficient)
                    dynamicDestination[coefficient] =
                        dynamicSnapshot[(size_t)band][(size_t)coefficient];
            }
            auto* soloDestination =
                mainOutputEQSoloFilters[channel].coefficients->coefficients.begin();
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                soloDestination[coefficient] = soloSnapshot[(size_t)coefficient];
        }
        if (nextSoloBand != audioMainOutputEQSoloBand)
        {
            for (auto& filter : mainOutputEQSoloFilters)
                filter.reset();
            audioMainOutputEQSoloBand = nextSoloBand;
            if (nextSoloBand >= 0)
                mainOutputEQSoloMix = 0.0f;
        }
        audioMainOutputEQSequence = sequence;
    }

    void processMainOutputEQ(juce::AudioBuffer<float>& buffer, int numSamples,
                             int numChannels) noexcept
    {
        if (mainOutputEQAnalyzerEnabled.load(std::memory_order_relaxed))
            pushMainOutputEQAnalyzer(mainOutputEQPreAnalyzerFifo,
                                     mainOutputEQPreAnalyzerLeftBuffer,
                                     mainOutputEQPreAnalyzerRightBuffer,
                                     buffer, numSamples);
        applyMainOutputEQCoefficientSnapshot();
        const bool targetEnabled =
            params.mainOutputEQEnabled.load(std::memory_order_relaxed);
        const bool soloActive = audioMainOutputEQSoloBand >= 0;
        if (!targetEnabled && mainOutputEQMix <= 0.0f && !soloActive &&
            mainOutputEQSoloMix <= 0.0f)
        {
            if (mainOutputEQAnalyzerEnabled.load(std::memory_order_relaxed))
                pushMainOutputEQAnalyzer(mainOutputEQPostAnalyzerFifo,
                                         mainOutputEQPostAnalyzerLeftBuffer,
                                         mainOutputEQPostAnalyzerRightBuffer,
                                         buffer, numSamples);
            return;
        }
        if (targetEnabled && mainOutputEQMix <= 0.0f)
            mainOutputEQFilters.reset();

        std::array<bool, numMainOutputEQBands> bandEnabled{};
        std::array<bool, numMainOutputEQBands> dynamicEnabled{};
        std::array<MainOutputEQDetector, numMainOutputEQBands> dynamicDetector{};
        std::array<float, numMainOutputEQBands> thresholdDb{};
        std::array<float, numMainOutputEQBands> rangeDb{};
        std::array<float, numMainOutputEQBands> attackAlpha{};
        std::array<float, numMainOutputEQBands> releaseAlpha{};
        for (int band = 0; band < numMainOutputEQBands; ++band)
        {
            bandEnabled[(size_t)band] =
                params.mainOutputEQBandEnabled[(size_t)band].load(std::memory_order_relaxed);
            dynamicEnabled[(size_t)band] = targetEnabled && bandEnabled[(size_t)band] &&
                params.mainOutputEQDynamicEnabled[(size_t)band].load(std::memory_order_relaxed);
            const bool wasDynamic =
                audioMainOutputEQDynamicEnabled[(size_t)band];
            if (dynamicEnabled[(size_t)band] != wasDynamic)
            {
                for (int channel = 0; channel < numChannels; ++channel)
                    mainOutputEQDynamicFilters[channel][band].reset();
                mainOutputEQDynamicEnvelope[(size_t)band] = 0.0f;
                audioMainOutputEQDynamicEnabled[(size_t)band] =
                    dynamicEnabled[(size_t)band];
            }
            thresholdDb[(size_t)band] = juce::jlimit(-80.0f, 0.0f,
                params.mainOutputEQDynamicThresholdDb[(size_t)band]
                    .load(std::memory_order_relaxed));
            rangeDb[(size_t)band] = juce::jlimit(0.0f, 18.0f,
                params.mainOutputEQDynamicRangeDb[(size_t)band]
                    .load(std::memory_order_relaxed));
            const float attackMs = juce::jlimit(0.1f, 100.0f,
                params.mainOutputEQDynamicAttackMs[(size_t)band]
                    .load(std::memory_order_relaxed));
            const float releaseMs = juce::jlimit(5.0f, 1000.0f,
                params.mainOutputEQDynamicReleaseMs[(size_t)band]
                    .load(std::memory_order_relaxed));
            attackAlpha[(size_t)band] = 1.0f - std::exp(
                -1.0f / (float)(currentSampleRate * attackMs * 0.001));
            releaseAlpha[(size_t)band] = 1.0f - std::exp(
                -1.0f / (float)(currentSampleRate * releaseMs * 0.001));
            dynamicDetector[(size_t)band] =
                params.mainOutputEQDynamicDetector[(size_t)band]
                    .load(std::memory_order_relaxed) == (int)MainOutputEQDetector::RMS
                    ? MainOutputEQDetector::RMS : MainOutputEQDetector::Peak;
        }

        const float mixStep = 1.0f / juce::jmax(1.0f, (float)(currentSampleRate * 0.01));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            mainOutputEQMix = targetEnabled
                ? juce::jmin(1.0f, mainOutputEQMix + mixStep)
                : juce::jmax(0.0f, mainOutputEQMix - mixStep);
            mainOutputEQSoloMix = soloActive
                ? juce::jmin(1.0f, mainOutputEQSoloMix + mixStep)
                : juce::jmax(0.0f, mainOutputEQSoloMix - mixStep);
            std::array<float, 2> drySamples{};
            std::array<float, 2> wetSamples{};
            for (int channel = 0; channel < numChannels; ++channel)
            {
                drySamples[(size_t)channel] = buffer.getSample(channel, sample);
                const float wet = mainOutputEQFilters.processSample(
                    channel, drySamples[(size_t)channel], bandEnabled);
                wetSamples[(size_t)channel] = wet;
            }
            if (targetEnabled)
            {
                for (int band = 0; band < numMainOutputEQBands; ++band)
                {
                    if (!dynamicEnabled[(size_t)band])
                        continue;
                    std::array<float, 2> bandSignal{};
                    float detectorPeak = 0.0f;
                    float detectorEnergy = 0.0f;
                    for (int channel = 0; channel < numChannels; ++channel)
                    {
                        bandSignal[(size_t)channel] =
                            mainOutputEQDynamicFilters[channel][band]
                                .processSample(wetSamples[(size_t)channel]);
                        detectorPeak = juce::jmax(detectorPeak,
                            std::abs(bandSignal[(size_t)channel]));
                        detectorEnergy += bandSignal[(size_t)channel] *
                                          bandSignal[(size_t)channel];
                    }
                    const float detectorValue =
                        dynamicDetector[(size_t)band] == MainOutputEQDetector::RMS
                            ? std::sqrt(detectorEnergy / (float)numChannels)
                            : detectorPeak;
                    float& envelope = mainOutputEQDynamicEnvelope[(size_t)band];
                    const float alpha = detectorValue > envelope
                        ? attackAlpha[(size_t)band] : releaseAlpha[(size_t)band];
                    envelope += (detectorValue - envelope) * alpha;
                    const float envelopeDb = juce::Decibels::gainToDecibels(
                        envelope, -120.0f);
                    const float reductionDb = juce::jmin(rangeDb[(size_t)band],
                        juce::jmax(0.0f, envelopeDb - thresholdDb[(size_t)band]));
                    const float dynamicGain =
                        juce::Decibels::decibelsToGain(-reductionDb);
                    for (int channel = 0; channel < numChannels; ++channel)
                        wetSamples[(size_t)channel] +=
                            bandSignal[(size_t)channel] * (dynamicGain - 1.0f);
                }
            }
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float dry = drySamples[(size_t)channel];
                const float eqOutput = dry + (wetSamples[(size_t)channel] - dry) *
                                                mainOutputEQMix;
                const float soloOutput = soloActive
                    ? mainOutputEQSoloFilters[channel].processSample(dry) : 0.0f;
                buffer.setSample(channel, sample,
                    eqOutput + (soloOutput - eqOutput) * mainOutputEQSoloMix);
            }
            mainOutputEQFilters.advanceSample();
        }
        if (mainOutputEQAnalyzerEnabled.load(std::memory_order_relaxed))
            pushMainOutputEQAnalyzer(mainOutputEQPostAnalyzerFifo,
                                     mainOutputEQPostAnalyzerLeftBuffer,
                                     mainOutputEQPostAnalyzerRightBuffer,
                                     buffer, numSamples);
    }

    static constexpr int mainOutputEQAnalyzerFifoSize = 8192;

    void pushMainOutputEQAnalyzer(
        juce::AbstractFifo& fifo,
        std::array<float, mainOutputEQAnalyzerFifoSize>& leftBuffer,
        std::array<float, mainOutputEQAnalyzerFifoSize>& rightBuffer,
        const juce::AudioBuffer<float>& audio, int numSamples) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
        const int channels = juce::jmin(2, audio.getNumChannels());
        if (channels <= 0)
        {
            fifo.finishedWrite(0);
            return;
        }
        for (int i = 0; i < size1; ++i)
        {
            leftBuffer[(size_t)(start1 + i)] = audio.getSample(0, i);
            rightBuffer[(size_t)(start1 + i)] =
                channels > 1 ? audio.getSample(1, i) : audio.getSample(0, i);
        }
        for (int i = 0; i < size2; ++i)
        {
            const int sample = size1 + i;
            leftBuffer[(size_t)(start2 + i)] = audio.getSample(0, sample);
            rightBuffer[(size_t)(start2 + i)] =
                channels > 1 ? audio.getSample(1, sample)
                             : audio.getSample(0, sample);
        }
        fifo.finishedWrite(size1 + size2);
    }

    bool pullMainOutputEQAnalyzerStereo(
        juce::AbstractFifo& fifo,
        const std::array<float, mainOutputEQAnalyzerFifoSize>& leftBuffer,
        const std::array<float, mainOutputEQAnalyzerFifoSize>& rightBuffer,
        float* leftDestination, float* rightDestination, int numSamples)
    {
        if (leftDestination == nullptr || rightDestination == nullptr ||
            numSamples <= 0 || numSamples > mainOutputEQAnalyzerFifoSize ||
            fifo.getNumReady() < numSamples)
            return false;
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(numSamples, start1, size1, start2, size2);
        if (size1 > 0)
        {
            std::memcpy(leftDestination, leftBuffer.data() + start1,
                        (size_t)size1 * sizeof(float));
            std::memcpy(rightDestination, rightBuffer.data() + start1,
                        (size_t)size1 * sizeof(float));
        }
        if (size2 > 0)
        {
            std::memcpy(leftDestination + size1, leftBuffer.data() + start2,
                        (size_t)size2 * sizeof(float));
            std::memcpy(rightDestination + size1, rightBuffer.data() + start2,
                        (size_t)size2 * sizeof(float));
        }
        fifo.finishedRead(size1 + size2);
        return size1 + size2 == numSamples;
    }

    bool pullMainOutputEQAnalyzerMono(
        juce::AbstractFifo& fifo,
        const std::array<float, mainOutputEQAnalyzerFifoSize>& leftBuffer,
        const std::array<float, mainOutputEQAnalyzerFifoSize>& rightBuffer,
        float* destination, int numSamples)
    {
        if (destination == nullptr || numSamples <= 0 ||
            numSamples > mainOutputEQAnalyzerFifoSize ||
            fifo.getNumReady() < numSamples)
            return false;

        std::array<float, 1024> leftChunk{};
        std::array<float, 1024> rightChunk{};
        int samplesRemaining = numSamples;
        int destinationOffset = 0;
        while (samplesRemaining > 0)
        {
            const int chunkSize = juce::jmin(samplesRemaining,
                                              (int)leftChunk.size());
            if (!pullMainOutputEQAnalyzerStereo(
                    fifo, leftBuffer, rightBuffer, leftChunk.data(),
                    rightChunk.data(), chunkSize))
                return false;
            for (int sample = 0; sample < chunkSize; ++sample)
                destination[destinationOffset + sample] =
                    0.5f * (leftChunk[(size_t)sample] +
                            rightChunk[(size_t)sample]);
            samplesRemaining -= chunkSize;
            destinationOffset += chunkSize;
        }
        return true;
    }

    void publishMonitorGraphicEQCoefficients(
        const std::array<std::array<float, 5>, numMonitorGraphicEQBands>& coefficients)
    {
        const std::lock_guard<std::mutex> lock(monitorGraphicEQWriterMutex);
        monitorGraphicEQSequence.fetch_add(1, std::memory_order_acq_rel);
        for (int band = 0; band < numMonitorGraphicEQBands; ++band)
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                monitorGraphicEQCoefficientSnapshot[(size_t)band][(size_t)coefficient].store(
                    coefficients[(size_t)band][(size_t)coefficient],
                    std::memory_order_relaxed);
        monitorGraphicEQSequence.fetch_add(1, std::memory_order_release);
    }

    bool readMonitorGraphicEQCoefficientSnapshot(
        std::array<std::array<float, 5>, numMonitorGraphicEQBands>& destination) const noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto sequenceBefore =
                monitorGraphicEQSequence.load(std::memory_order_acquire);
            if ((sequenceBefore & 1u) != 0u)
                continue;
            for (int band = 0; band < numMonitorGraphicEQBands; ++band)
                for (int coefficient = 0; coefficient < 5; ++coefficient)
                    destination[(size_t)band][(size_t)coefficient] =
                        monitorGraphicEQCoefficientSnapshot[(size_t)band][(size_t)coefficient]
                            .load(std::memory_order_relaxed);
            if (sequenceBefore ==
                monitorGraphicEQSequence.load(std::memory_order_acquire))
                return true;
        }
        return false;
    }

    void applyMonitorGraphicEQCoefficientSnapshot() noexcept
    {
        const auto sequence =
            monitorGraphicEQSequence.load(std::memory_order_acquire);
        if (sequence == audioMonitorGraphicEQSequence || (sequence & 1u) != 0u)
            return;
        std::array<std::array<float, 5>, numMonitorGraphicEQBands> snapshot{};
        if (!readMonitorGraphicEQCoefficientSnapshot(snapshot) ||
            monitorGraphicEQSequence.load(std::memory_order_acquire) != sequence)
            return;
        if (!monitorGraphicEQFilters.beginTransition(
                snapshot, juce::roundToInt(currentSampleRate * 0.01)))
            return;
        audioMonitorGraphicEQSequence = sequence;
    }

    void processMonitorGraphicEQ(juce::AudioBuffer<float>& buffer, int numSamples,
                                 int numChannels) noexcept
    {
        const bool analyzerEnabled =
            monitorGraphicEQAnalyzerEnabled.load(std::memory_order_relaxed);
        if (analyzerEnabled)
            pushMonitorGraphicEQAnalyzer(
                monitorGraphicEQPreAnalyzerFifo,
                monitorGraphicEQPreAnalyzerLeftBuffer,
                monitorGraphicEQPreAnalyzerRightBuffer, buffer, numSamples);

        applyMonitorGraphicEQCoefficientSnapshot();
        const bool targetEnabled =
            params.monitorGraphicEQEnabled.load(std::memory_order_relaxed);
        if (!targetEnabled && monitorGraphicEQMix <= 0.0f)
        {
            if (analyzerEnabled)
                pushMonitorGraphicEQAnalyzer(
                    monitorGraphicEQPostAnalyzerFifo,
                    monitorGraphicEQPostAnalyzerLeftBuffer,
                    monitorGraphicEQPostAnalyzerRightBuffer, buffer, numSamples);
            return;
        }
        if (targetEnabled && monitorGraphicEQMix <= 0.0f)
            monitorGraphicEQFilters.reset();

        std::array<bool, numMonitorGraphicEQBands> bandEnabled{};
        bandEnabled.fill(true);
        const float mixStep =
            1.0f / juce::jmax(1.0f, (float)(currentSampleRate * 0.01));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            monitorGraphicEQMix = targetEnabled
                ? juce::jmin(1.0f, monitorGraphicEQMix + mixStep)
                : juce::jmax(0.0f, monitorGraphicEQMix - mixStep);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float dry = buffer.getSample(channel, sample);
                const float wet =
                    monitorGraphicEQFilters.processSample(channel, dry, bandEnabled);
                buffer.setSample(channel, sample,
                                 dry + (wet - dry) * monitorGraphicEQMix);
            }
            monitorGraphicEQFilters.advanceSample();
        }
        if (analyzerEnabled)
            pushMonitorGraphicEQAnalyzer(
                monitorGraphicEQPostAnalyzerFifo,
                monitorGraphicEQPostAnalyzerLeftBuffer,
                monitorGraphicEQPostAnalyzerRightBuffer, buffer, numSamples);
    }

    static constexpr int monitorGraphicEQAnalyzerFifoSize = 8192;

    void pushMonitorGraphicEQAnalyzer(
        juce::AbstractFifo& fifo,
        std::array<float, monitorGraphicEQAnalyzerFifoSize>& leftBuffer,
        std::array<float, monitorGraphicEQAnalyzerFifoSize>& rightBuffer,
        const juce::AudioBuffer<float>& audio, int numSamples) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
        const int channels = juce::jmin(2, audio.getNumChannels());
        if (channels <= 0)
        {
            fifo.finishedWrite(0);
            return;
        }
        for (int sample = 0; sample < size1; ++sample)
        {
            leftBuffer[(size_t)(start1 + sample)] = audio.getSample(0, sample);
            rightBuffer[(size_t)(start1 + sample)] = channels > 1
                ? audio.getSample(1, sample) : audio.getSample(0, sample);
        }
        for (int sample = 0; sample < size2; ++sample)
        {
            const int audioSample = size1 + sample;
            leftBuffer[(size_t)(start2 + sample)] = audio.getSample(0, audioSample);
            rightBuffer[(size_t)(start2 + sample)] = channels > 1
                ? audio.getSample(1, audioSample) : audio.getSample(0, audioSample);
        }
        fifo.finishedWrite(size1 + size2);
    }

    bool pullMonitorGraphicEQAnalyzer(
        juce::AbstractFifo& fifo,
        const std::array<float, monitorGraphicEQAnalyzerFifoSize>& leftBuffer,
        const std::array<float, monitorGraphicEQAnalyzerFifoSize>& rightBuffer,
        float* destination, int numSamples)
    {
        if (destination == nullptr || numSamples <= 0 ||
            numSamples > monitorGraphicEQAnalyzerFifoSize ||
            fifo.getNumReady() < numSamples)
            return false;
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(numSamples, start1, size1, start2, size2);
        const auto copyAndAverage = [&](int start, int count, int destinationOffset)
        {
            for (int sample = 0; sample < count; ++sample)
                destination[destinationOffset + sample] =
                    0.5f * (leftBuffer[(size_t)(start + sample)] +
                                         rightBuffer[(size_t)(start + sample)]);
        };
        copyAndAverage(start1, size1, 0);
        copyAndAverage(start2, size2, size1);
        fifo.finishedRead(size1 + size2);
        return size1 + size2 == numSamples;
    }

    bool pullMonitorGraphicEQAnalyzerStereo(
        juce::AbstractFifo& fifo,
        const std::array<float, monitorGraphicEQAnalyzerFifoSize>& leftBuffer,
        const std::array<float, monitorGraphicEQAnalyzerFifoSize>& rightBuffer,
        float* leftDestination, float* rightDestination, int numSamples)
    {
        if (leftDestination == nullptr || rightDestination == nullptr ||
            numSamples <= 0 || numSamples > monitorGraphicEQAnalyzerFifoSize ||
            fifo.getNumReady() < numSamples)
            return false;
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(numSamples, start1, size1, start2, size2);
        if (size1 > 0)
        {
            std::memcpy(leftDestination, leftBuffer.data() + start1,
                                     (size_t)size1 * sizeof(float));
            std::memcpy(rightDestination, rightBuffer.data() + start1,
                                     (size_t)size1 * sizeof(float));
        }
        if (size2 > 0)
        {
            std::memcpy(leftDestination + size1, leftBuffer.data() + start2,
                                     (size_t)size2 * sizeof(float));
            std::memcpy(rightDestination + size1, rightBuffer.data() + start2,
                                     (size_t)size2 * sizeof(float));
        }
        fifo.finishedRead(size1 + size2);
        return size1 + size2 == numSamples;
    }

    PuzuCoefficientSet makePuzuCoefficients() const
    {
        PuzuCoefficientSet coefficients;
        const float maxFrequency = juce::jmin(20000.0f, (float)(currentSampleRate * 0.495));
        for (int band = 0; band < numPuzuBands; ++band)
        {
            const float frequency = juce::jlimit(20.0f, maxFrequency,
                params.puzuBands[band].freq.load(std::memory_order_relaxed));
            const float q = juce::jlimit(0.1f, 10.0f,
                params.puzuBands[band].q.load(std::memory_order_relaxed));
            const float gain = juce::Decibels::decibelsToGain(juce::jlimit(-18.0f, 18.0f,
                params.puzuBands[band].gainDb.load(std::memory_order_relaxed)));
            switch (params.puzuBands[band].type.load(std::memory_order_relaxed))
            {
                case PuzuFilterType::HighPass:
                    coefficients[(size_t)band] =
                        juce::dsp::IIR::Coefficients<float>::makeHighPass(
                            currentSampleRate, frequency, q);
                    break;
                case PuzuFilterType::LowShelf:
                    coefficients[(size_t)band] =
                        juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                            currentSampleRate, frequency, q, gain);
                    break;
                case PuzuFilterType::HighShelf:
                    coefficients[(size_t)band] =
                        juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                            currentSampleRate, frequency, q, gain);
                    break;
                case PuzuFilterType::LowPass:
                    coefficients[(size_t)band] =
                        juce::dsp::IIR::Coefficients<float>::makeLowPass(
                            currentSampleRate, frequency, q);
                    break;
                case PuzuFilterType::Notch:
                    coefficients[(size_t)band] =
                        juce::dsp::IIR::Coefficients<float>::makeNotch(
                            currentSampleRate, frequency, q);
                    break;
                case PuzuFilterType::Bell:
                default:
                    coefficients[(size_t)band] =
                        juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                            currentSampleRate, frequency, q, gain);
                    break;
            }
        }
        return coefficients;
    }

    void processPuzuEQ(juce::AudioBuffer<float>& buffer, int numSamples, int numChannels)
    {
        if (puzuAnalyzerEnabled.load(std::memory_order_relaxed))
            pushPuzuAnalyzer(puzuPreAnalyzerFifo, puzuPreAnalyzerLeftBuffer,
                             puzuPreAnalyzerRightBuffer, buffer, numSamples);

        applyPuzuCoefficientSnapshot();
        const bool targetEnabled = params.puzuEnabled.load(std::memory_order_relaxed);
        if (targetEnabled && puzuMix <= 0.0f)
            puzuBandFilters.reset();

        if (!targetEnabled && puzuMix <= 0.0f)
        {
            for (int sample = 0; sample < numSamples; ++sample)
                puzuBandFilters.advanceSample();
            if (puzuAnalyzerEnabled.load(std::memory_order_relaxed))
                pushPuzuAnalyzer(puzuPostAnalyzerFifo, puzuPostAnalyzerLeftBuffer,
                                 puzuPostAnalyzerRightBuffer, buffer, numSamples);
            return;
        }

        std::array<bool, numPuzuBands> bandEnabled{};
        for (int band = 0; band < numPuzuBands; ++band)
            bandEnabled[(size_t)band] =
                params.puzuBands[band].enabled.load(std::memory_order_relaxed);

        const float mixStep = 1.0f / juce::jmax(1.0f, (float)(currentSampleRate * 0.01));
        for (int sample = 0; sample < numSamples; ++sample)
        {
            puzuMix = targetEnabled
                ? juce::jmin(1.0f, puzuMix + mixStep)
                : juce::jmax(0.0f, puzuMix - mixStep);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float dry = buffer.getSample(channel, sample);
                const float wet =
                    puzuBandFilters.processSample(channel, dry, bandEnabled);
                buffer.setSample(channel, sample, dry + (wet - dry) * puzuMix);
            }
            puzuBandFilters.advanceSample();
        }

        if (puzuAnalyzerEnabled.load(std::memory_order_relaxed))
            pushPuzuAnalyzer(puzuPostAnalyzerFifo, puzuPostAnalyzerLeftBuffer,
                             puzuPostAnalyzerRightBuffer, buffer, numSamples);
    }

    static constexpr int puzuAnalyzerFifoSize = 8192;

    void pushPuzuAnalyzer(juce::AbstractFifo& fifo,
                          std::array<float, puzuAnalyzerFifoSize>& leftBuffer,
                          std::array<float, puzuAnalyzerFifoSize>& rightBuffer,
                          const juce::AudioBuffer<float>& audio, int numSamples) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
        const int channels = juce::jmin(2, audio.getNumChannels());
        if (channels <= 0)
        {
            fifo.finishedWrite(0);
            return;
        }
        for (int i = 0; i < size1; ++i)
        {
            leftBuffer[(size_t)(start1 + i)] = audio.getSample(0, i);
            rightBuffer[(size_t)(start1 + i)] =
                channels > 1 ? audio.getSample(1, i) : audio.getSample(0, i);
        }
        for (int i = 0; i < size2; ++i)
        {
            const int sample = size1 + i;
            leftBuffer[(size_t)(start2 + i)] = audio.getSample(0, sample);
            rightBuffer[(size_t)(start2 + i)] =
                channels > 1 ? audio.getSample(1, sample)
                             : audio.getSample(0, sample);
        }
        fifo.finishedWrite(size1 + size2);
    }

    bool pullPuzuAnalyzerStereo(
        juce::AbstractFifo& fifo,
        const std::array<float, puzuAnalyzerFifoSize>& leftBuffer,
        const std::array<float, puzuAnalyzerFifoSize>& rightBuffer,
        float* leftDestination, float* rightDestination, int numSamples)
    {
        if (leftDestination == nullptr || rightDestination == nullptr ||
            numSamples <= 0 || numSamples > puzuAnalyzerFifoSize ||
            fifo.getNumReady() < numSamples)
            return false;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(numSamples, start1, size1, start2, size2);
        if (size1 > 0)
        {
            std::memcpy(leftDestination, leftBuffer.data() + start1,
                        (size_t)size1 * sizeof(float));
            std::memcpy(rightDestination, rightBuffer.data() + start1,
                        (size_t)size1 * sizeof(float));
        }
        if (size2 > 0)
        {
            std::memcpy(leftDestination + size1, leftBuffer.data() + start2,
                        (size_t)size2 * sizeof(float));
            std::memcpy(rightDestination + size1, rightBuffer.data() + start2,
                        (size_t)size2 * sizeof(float));
        }
        fifo.finishedRead(size1 + size2);
        return size1 + size2 == numSamples;
    }

    bool pullPuzuAnalyzerMono(
        juce::AbstractFifo& fifo,
        const std::array<float, puzuAnalyzerFifoSize>& leftBuffer,
        const std::array<float, puzuAnalyzerFifoSize>& rightBuffer,
        float* destination, int numSamples)
    {
        if (destination == nullptr || numSamples <= 0 ||
            numSamples > puzuAnalyzerFifoSize ||
            fifo.getNumReady() < numSamples)
            return false;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(numSamples, start1, size1, start2, size2);
        const auto copyAndAverage = [&](int start, int count, int destinationOffset)
        {
            for (int sample = 0; sample < count; ++sample)
                destination[destinationOffset + sample] =
                    0.5f * (leftBuffer[(size_t)(start + sample)] +
                            rightBuffer[(size_t)(start + sample)]);
        };
        copyAndAverage(start1, size1, 0);
        copyAndAverage(start2, size2, size1);
        fifo.finishedRead(size1 + size2);
        return size1 + size2 == numSamples;
    }

    void publishPuzuCoefficients(
        const std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>,
                         numPuzuBands>& coefficients)
    {
        const std::lock_guard<std::mutex> lock(puzuCoefficientWriterMutex);
        puzuCoefficientSequence.fetch_add(1, std::memory_order_acq_rel);
        for (int band = 0; band < numPuzuBands; ++band)
        {
            const auto* values = coefficients[(size_t)band]->getRawCoefficients();
            for (int coefficient = 0; coefficient < 5; ++coefficient)
                puzuCoefficientSnapshot[(size_t)band][(size_t)coefficient].store(
                    values[coefficient], std::memory_order_relaxed);
        }
        puzuCoefficientSequence.fetch_add(1, std::memory_order_release);
    }

    bool readPuzuCoefficientSnapshot(
        std::array<std::array<float, 5>, numPuzuBands>& destination) const noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto sequenceBefore = puzuCoefficientSequence.load(std::memory_order_acquire);
            if ((sequenceBefore & 1u) != 0u)
                continue;

            for (int band = 0; band < numPuzuBands; ++band)
                for (int coefficient = 0; coefficient < 5; ++coefficient)
                    destination[(size_t)band][(size_t)coefficient] =
                        puzuCoefficientSnapshot[(size_t)band][(size_t)coefficient].load(
                            std::memory_order_relaxed);

            const auto sequenceAfter = puzuCoefficientSequence.load(std::memory_order_acquire);
            if (sequenceBefore == sequenceAfter)
                return true;
        }
        return false;
    }

    void applyPuzuCoefficientSnapshot() noexcept
    {
        std::array<std::array<float, 5>, numPuzuBands> snapshot{};
        const auto sequence = puzuCoefficientSequence.load(std::memory_order_acquire);
        if (sequence == audioPuzuCoefficientSequence || (sequence & 1u) != 0u ||
            !readPuzuCoefficientSnapshot(snapshot))
            return;

        const auto confirmedSequence = puzuCoefficientSequence.load(std::memory_order_acquire);
        if (confirmedSequence != sequence)
            return;

        if (!puzuBandFilters.beginTransition(
                snapshot, juce::roundToInt(currentSampleRate * 0.01)))
            return;
        audioPuzuCoefficientSequence = sequence;
    }

    void resetInternalEnvelopes()
    {
        gateDetectorEnv = 0.0f;
        gateCurrentGain = 1.0f;
        gateHoldRemaining = 0;
        gateState = 2; // Open

        compCurrentGain = 1.0f;
        limiterCurrentGain = 1.0f;

        gateGR.store(0.0f, std::memory_order_relaxed);
        compGR.store(0.0f, std::memory_order_relaxed);
        limiterGR.store(0.0f, std::memory_order_relaxed);
        gateCurrentState.store(2, std::memory_order_relaxed);
        inPeakDb.store(-100.0f, std::memory_order_relaxed);
        outPeakDb.store(-100.0f, std::memory_order_relaxed);
    }

    void processHPF(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        float freq = juce::jlimit(20.0f, (float)(currentSampleRate * 0.49), params.hpfFreq.load(std::memory_order_relaxed));
        int slope = params.hpfSlope.load(std::memory_order_relaxed);

        if (std::abs(freq - lastHpfFreq) > 0.5f || slope != lastHpfSlope)
        {
            lastHpfFreq = freq;
            lastHpfSlope = slope;
            auto c1 = (slope == 0)
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, freq, (slope >= 3) ? 0.5412f : 0.7071f);
            auto c2 = (slope == 2)
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, freq, 1.3065f);

            for (int ch = 0; ch < 2; ++ch)
            {
                *hpfFilterStage1[ch].coefficients = *c1;
                *hpfFilterStage2[ch].coefficients = *c2;
            }
        }

        juce::dsp::AudioBlock<float> block(buffer);
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto singleBlock = block.getSingleChannelBlock((size_t)ch).getSubBlock(0, (size_t)numSamples);
            juce::dsp::ProcessContextReplacing<float> context(singleBlock);
            hpfFilterStage1[ch].process(context);
            if (slope >= 2)
                hpfFilterStage2[ch].process(context);
        }
    }

    void processLPF(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float maxFrequency = juce::jmin(22000.0f, (float)(currentSampleRate * 0.49));
        const float freq = juce::jlimit(juce::jmin(1000.0f, maxFrequency), maxFrequency,
                                        params.lpfFreq.load(std::memory_order_relaxed));
        const int slope = juce::jlimit(1, 4, params.lpfSlope.load(std::memory_order_relaxed));

        if (std::abs(freq - lastLpfFreq) > 0.5f || slope != lastLpfSlope)
        {
            lastLpfFreq = freq;
            lastLpfSlope = slope;
            const auto stage1 = slope == 1
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(currentSampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeLowPass(
                      currentSampleRate, freq, slope == 4 ? 0.5412f : 0.7071f);
            const auto stage2 = slope == 3
                ? juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(currentSampleRate, freq)
                : juce::dsp::IIR::Coefficients<float>::makeLowPass(
                      currentSampleRate, freq, slope == 4 ? 1.3065f : 0.7071f);

            for (int ch = 0; ch < 2; ++ch)
            {
                *lpfFilterStage1[ch].coefficients = *stage1;
                *lpfFilterStage2[ch].coefficients = *stage2;
            }
        }

        juce::dsp::AudioBlock<float> block(buffer);
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto singleBlock = block.getSingleChannelBlock((size_t)ch)
                                   .getSubBlock(0, (size_t)numSamples);
            juce::dsp::ProcessContextReplacing<float> context(singleBlock);
            lpfFilterStage1[ch].process(context);
            if (slope >= 3)
                lpfFilterStage2[ch].process(context);
        }
    }

    void processGate(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float threshDb = params.gateThresholdDb.load(std::memory_order_relaxed);
        const float openThreshLinear = juce::Decibels::decibelsToGain(threshDb);
        const float hystDb = juce::jlimit(1.0f, 12.0f, params.gateHysteresisDb.load(std::memory_order_relaxed));
        const float closeThreshLinear = juce::Decibels::decibelsToGain(threshDb - hystDb);
        const float floorDb = params.gateFloorDb.load(std::memory_order_relaxed);
        const float floorLinear = (floorDb <= -79.0f) ? 0.0f : juce::Decibels::decibelsToGain(floorDb);

        const float attMs = juce::jmax(0.1f, params.gateAttackMs.load(std::memory_order_relaxed));
        const float holdMs = juce::jmax(0.0f, params.gateHoldMs.load(std::memory_order_relaxed));
        const float relMs = juce::jmax(1.0f, params.gateReleaseMs.load(std::memory_order_relaxed));

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * attMs * 0.001));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * relMs * 0.001));
        const int holdSamples = (int)(currentSampleRate * holdMs * 0.001);

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        float minGainObserved = 1.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float sampleMax = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float a = std::abs(buffer.getSample(ch, s));
                if (a > sampleMax) sampleMax = a;
            }

            gateDetectorEnv = sampleMax;

            // State Machine: 0 = Closed, 1 = Hold, 2 = Open
            if (gateDetectorEnv >= openThreshLinear)
            {
                gateState = 2; // Open
                gateHoldRemaining = holdSamples;
            }
            else if (gateDetectorEnv < closeThreshLinear)
            {
                if (gateHoldRemaining > 0)
                {
                    gateState = 1; // Hold
                    gateHoldRemaining--;
                }
                else
                {
                    gateState = 0; // Closed
                }
            }

            float targetGain = (gateState == 2 || gateState == 1) ? 1.0f : floorLinear;

            if (targetGain > gateCurrentGain)
                gateCurrentGain += (targetGain - gateCurrentGain) * alphaAtt;
            else
                gateCurrentGain += (targetGain - gateCurrentGain) * alphaRel;

            if (gateCurrentGain < minGainObserved)
                minGainObserved = gateCurrentGain;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.setSample(ch, s, buffer.getSample(ch, s) * gateCurrentGain);
            }
        }

        gateCurrentState.store(gateState, std::memory_order_relaxed);
        float grDb = (gateCurrentGain > 0.00001f) ? -juce::Decibels::gainToDecibels(gateCurrentGain, -80.0f) : 80.0f;
        gateGR.store(juce::jmax(0.0f, grDb), std::memory_order_relaxed);
    }

    void processCompressor(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float threshDb = params.compThresholdDb.load(std::memory_order_relaxed);
        const float ratio = juce::jmax(1.0f, params.compRatio.load(std::memory_order_relaxed));
        const float attMs = juce::jmax(0.1f, params.compAttackMs.load(std::memory_order_relaxed));
        const float relMs = juce::jmax(1.0f, params.compReleaseMs.load(std::memory_order_relaxed));
        const int kneeType = params.compKnee.load(std::memory_order_relaxed);
        const float kneeWidthDb = (kneeType == 0) ? 10.0f : (kneeType == 1) ? 5.0f : 0.0f;
        const float makeupLinear = juce::Decibels::decibelsToGain(params.compMakeupDb.load(std::memory_order_relaxed));
        const float mixPercent = juce::jlimit(0.0f, 100.0f, params.compMixPercent.load(std::memory_order_relaxed)) * 0.01f;

        const float alphaAtt = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * attMs * 0.001));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * relMs * 0.001));

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        float maxGRDb = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float samplePeak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float a = std::abs(buffer.getSample(ch, s));
                if (a > samplePeak) samplePeak = a;
            }

            float inDb = (samplePeak > 0.00001f) ? juce::Decibels::gainToDecibels(samplePeak, -90.0f) : -90.0f;

            float targetGRDb = 0.0f;
            if (kneeWidthDb > 0.01f)
            {
                float halfKnee = kneeWidthDb * 0.5f;
                if (inDb <= threshDb - halfKnee)
                {
                    targetGRDb = 0.0f;
                }
                else if (inDb >= threshDb + halfKnee)
                {
                    targetGRDb = (inDb - threshDb) * (1.0f - 1.0f / ratio);
                }
                else
                {
                    float delta = inDb - threshDb + halfKnee;
                    float quad = (delta * delta) / (2.0f * kneeWidthDb);
                    targetGRDb = quad * (1.0f - 1.0f / ratio);
                }
            }
            else
            {
                if (inDb > threshDb)
                    targetGRDb = (inDb - threshDb) * (1.0f - 1.0f / ratio);
            }

            float targetGainLinear = juce::Decibels::decibelsToGain(-targetGRDb);

            if (targetGainLinear < compCurrentGain)
                compCurrentGain += (targetGainLinear - compCurrentGain) * alphaAtt;
            else
                compCurrentGain += (targetGainLinear - compCurrentGain) * alphaRel;

            float currentGRDb = -juce::Decibels::gainToDecibels(compCurrentGain, -90.0f);
            if (currentGRDb > maxGRDb) maxGRDb = currentGRDb;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float dry = buffer.getSample(ch, s);
                float wet = dry * compCurrentGain * makeupLinear;
                buffer.setSample(ch, s, dry * (1.0f - mixPercent) + wet * mixPercent);
            }
        }

        compGR.store(juce::jmax(0.0f, maxGRDb), std::memory_order_relaxed);
    }

    void processReverb(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        juce::dsp::Reverb::Parameters rParams;
        rParams.roomSize = juce::jlimit(0.0f, 1.0f, params.reverbRoomSize.load(std::memory_order_relaxed));
        rParams.damping = juce::jlimit(0.0f, 1.0f, params.reverbDamping.load(std::memory_order_relaxed));
        rParams.wetLevel = juce::jlimit(0.0f, 1.0f, params.reverbWetLevel.load(std::memory_order_relaxed));
        rParams.dryLevel = juce::jlimit(0.0f, 1.0f, params.reverbDryLevel.load(std::memory_order_relaxed));
        rParams.width = juce::jlimit(0.0f, 1.0f, params.reverbWidth.load(std::memory_order_relaxed));
        rParams.freezeMode = 0.0f;
        reverb.setParameters(rParams);

        juce::dsp::AudioBlock<float> block(buffer);
        auto subBlock = block.getSubBlock(0, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> context(subBlock);
        reverb.process(context);
    }

    void processDelay(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float delayMs = juce::jlimit(10.0f, 1500.0f, params.delayTimeMs.load(std::memory_order_relaxed));
        const float feedback = juce::jlimit(0.0f, 0.90f, params.delayFeedback.load(std::memory_order_relaxed));
        const float wet = juce::jlimit(0.0f, 1.0f, params.delayWetLevel.load(std::memory_order_relaxed));
        const float dry = juce::jlimit(0.0f, 1.0f, params.delayDryLevel.load(std::memory_order_relaxed));
        const bool pingPong = params.delayPingPong.load(std::memory_order_relaxed);

        const int bufSize = delayBuffer.getNumSamples();
        if (bufSize <= 0) return;

        targetDelaySamples = delayMs * 0.001f * (float)currentSampleRate;
        const float alphaSmooth = 0.005f;

        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        for (int s = 0; s < numSamples; ++s)
        {
            currentDelaySamples += (targetDelaySamples - currentDelaySamples) * alphaSmooth;
            float dSamples = juce::jlimit(1.0f, (float)(bufSize - 2), currentDelaySamples);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float readIndex = (float)delayWritePos - dSamples;
                while (readIndex < 0.0f) readIndex += (float)bufSize;
                while (readIndex >= (float)bufSize) readIndex -= (float)bufSize;

                int idx0 = (int)readIndex;
                int idx1 = (idx0 + 1) % bufSize;
                float frac = readIndex - (float)idx0;

                const float* dData = delayBuffer.getReadPointer(ch);
                float delayedSample = dData[idx0] * (1.0f - frac) + dData[idx1] * frac;

                float inSample = buffer.getSample(ch, s);

                int writeCh = (pingPong && numChannels == 2) ? (1 - ch) : ch;
                float* dWriteData = delayBuffer.getWritePointer(writeCh);
                dWriteData[delayWritePos] = inSample + (delayedSample * feedback);

                buffer.setSample(ch, s, inSample * dry + delayedSample * wet);
            }

            delayWritePos = (delayWritePos + 1) % bufSize;
        }
    }

    void processLimiter(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float ceilingDb = juce::jlimit(-20.0f, 0.0f, params.limiterCeilingDb.load(std::memory_order_relaxed));
        const float ceilingLinear = juce::Decibels::decibelsToGain(ceilingDb);
        const float relMs = juce::jmax(1.0f, params.limiterReleaseMs.load(std::memory_order_relaxed));
        const float alphaRel = 1.0f - std::exp(-1.0f / (float)(currentSampleRate * relMs * 0.001));

        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        float maxGRDb = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float a = std::abs(buffer.getSample(ch, s));
                if (a > peak) peak = a;
            }

            float targetGain = 1.0f;
            if (peak > ceilingLinear)
                targetGain = ceilingLinear / (peak + 0.00001f);

            if (targetGain < limiterCurrentGain)
                limiterCurrentGain = targetGain;
            else
                limiterCurrentGain += (targetGain - limiterCurrentGain) * alphaRel;

            float grDb = -juce::Decibels::gainToDecibels(limiterCurrentGain, -60.0f);
            if (grDb > maxGRDb) maxGRDb = grDb;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float outVal = buffer.getSample(ch, s) * limiterCurrentGain;
                if (outVal > ceilingLinear) outVal = ceilingLinear;
                if (outVal < -ceilingLinear) outVal = -ceilingLinear;
                buffer.setSample(ch, s, outVal);
            }
        }

        limiterGR.store(juce::jmax(0.0f, maxGRDb), std::memory_order_relaxed);
    }

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    float lastHpfFreq = -1.0f;
    int lastHpfSlope = -1;
    float lastLpfFreq = -1.0f;
    int lastLpfSlope = -1;

    juce::dsp::IIR::Filter<float> hpfFilterStage1[2];
    juce::dsp::IIR::Filter<float> hpfFilterStage2[2];
    juce::dsp::IIR::Filter<float> lpfFilterStage1[2];
    juce::dsp::IIR::Filter<float> lpfFilterStage2[2];

    CrossfadingEQFilterBank<numPuzuBands> puzuBandFilters;
    float puzuMix = 0.0f;
    CrossfadingEQFilterBank<numMainOutputEQBands> mainOutputEQFilters;
    juce::dsp::IIR::Filter<float> mainOutputEQDynamicFilters[2][numMainOutputEQBands];
    std::array<float, numMainOutputEQBands> mainOutputEQDynamicEnvelope{};
    std::array<bool, numMainOutputEQBands> audioMainOutputEQDynamicEnabled{};
    float mainOutputEQMix = 0.0f;
    std::array<std::array<std::atomic<float>, 5>, numMainOutputEQBands>
        mainOutputEQCoefficientSnapshot{};
    std::array<std::array<std::atomic<float>, 5>, numMainOutputEQBands>
        mainOutputEQDynamicCoefficientSnapshot{};
    std::array<std::atomic<float>, 5> mainOutputEQSoloCoefficientSnapshot{};
    std::atomic<int> mainOutputEQSoloBandSnapshot{-1};
    std::atomic<std::uint64_t> mainOutputEQSequence{0};
    std::uint64_t audioMainOutputEQSequence = 0;
    int audioMainOutputEQSoloBand = -1;
    float mainOutputEQSoloMix = 0.0f;
    juce::dsp::IIR::Filter<float> mainOutputEQSoloFilters[2];
    std::mutex mainOutputEQWriterMutex;
    CrossfadingEQFilterBank<numMonitorGraphicEQBands> monitorGraphicEQFilters;
    float monitorGraphicEQMix = 0.0f;
    std::array<std::array<std::atomic<float>, 5>, numMonitorGraphicEQBands>
        monitorGraphicEQCoefficientSnapshot{};
    std::atomic<std::uint64_t> monitorGraphicEQSequence{0};
    std::uint64_t audioMonitorGraphicEQSequence = 0;
    std::mutex monitorGraphicEQWriterMutex;
    std::array<std::array<std::atomic<float>, 5>, numPuzuBands> puzuCoefficientSnapshot{};
    std::atomic<std::uint64_t> puzuCoefficientSequence{0};
    std::uint64_t audioPuzuCoefficientSequence = 0;
    std::mutex puzuCoefficientWriterMutex;
    std::atomic<bool> puzuAnalyzerEnabled{false};
    juce::AbstractFifo puzuPreAnalyzerFifo{puzuAnalyzerFifoSize};
    juce::AbstractFifo puzuPostAnalyzerFifo{puzuAnalyzerFifoSize};
    std::array<float, puzuAnalyzerFifoSize> puzuPreAnalyzerLeftBuffer{};
    std::array<float, puzuAnalyzerFifoSize> puzuPreAnalyzerRightBuffer{};
    std::array<float, puzuAnalyzerFifoSize> puzuPostAnalyzerLeftBuffer{};
    std::array<float, puzuAnalyzerFifoSize> puzuPostAnalyzerRightBuffer{};
    std::atomic<bool> monitorGraphicEQAnalyzerEnabled{false};
    juce::AbstractFifo monitorGraphicEQPreAnalyzerFifo{
        monitorGraphicEQAnalyzerFifoSize};
    juce::AbstractFifo monitorGraphicEQPostAnalyzerFifo{
        monitorGraphicEQAnalyzerFifoSize};
    std::array<float, monitorGraphicEQAnalyzerFifoSize>
        monitorGraphicEQPreAnalyzerLeftBuffer{};
    std::array<float, monitorGraphicEQAnalyzerFifoSize>
        monitorGraphicEQPreAnalyzerRightBuffer{};
    std::array<float, monitorGraphicEQAnalyzerFifoSize>
        monitorGraphicEQPostAnalyzerLeftBuffer{};
    std::array<float, monitorGraphicEQAnalyzerFifoSize>
        monitorGraphicEQPostAnalyzerRightBuffer{};
    std::atomic<bool> mainOutputEQAnalyzerEnabled{false};
    juce::AbstractFifo mainOutputEQPreAnalyzerFifo{mainOutputEQAnalyzerFifoSize};
    juce::AbstractFifo mainOutputEQPostAnalyzerFifo{mainOutputEQAnalyzerFifoSize};
    std::array<float, mainOutputEQAnalyzerFifoSize> mainOutputEQPreAnalyzerLeftBuffer{};
    std::array<float, mainOutputEQAnalyzerFifoSize> mainOutputEQPreAnalyzerRightBuffer{};
    std::array<float, mainOutputEQAnalyzerFifoSize> mainOutputEQPostAnalyzerLeftBuffer{};
    std::array<float, mainOutputEQAnalyzerFifoSize> mainOutputEQPostAnalyzerRightBuffer{};

    juce::dsp::IIR::Filter<float> legacyEqLowFilter[2];
    juce::dsp::IIR::Filter<float> legacyEqMid1Filter[2];
    juce::dsp::IIR::Filter<float> legacyEqMid2Filter[2];
    juce::dsp::IIR::Filter<float> legacyEqHighFilter[2];

    // Real envelope states
    float gateDetectorEnv = 0.0f;
    float gateCurrentGain = 1.0f;
    int gateHoldRemaining = 0;
    int gateState = 2; // 0=Closed, 1=Hold, 2=Open

    float compCurrentGain = 1.0f;
    float limiterCurrentGain = 1.0f;

    juce::dsp::Reverb reverb;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    float currentDelaySamples = 0.0f;
    float targetDelaySamples = 0.0f;

    juce::AudioBuffer<float> timeAlignBuffer;
    int timeAlignWritePos = 0;

    juce::SmoothedValue<float> smoothedMasterGain;
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};
    std::atomic<bool> clipping{false};

    // Real-time telemetry atomics
    std::atomic<float> gateGR{0.0f};
    std::atomic<float> compGR{0.0f};
    std::atomic<float> limiterGR{0.0f};
    std::atomic<int> gateCurrentState{2};
    std::atomic<float> inPeakDb{-100.0f};
    std::atomic<float> outPeakDb{-100.0f};
};
