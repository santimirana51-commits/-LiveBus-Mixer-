#pragma once
#include <JuceHeader.h>
#include "EightBandEQ.h"
#include <array>
#include <atomic>
#include <cmath>

namespace LiveBus
{
    /**
     * Professional Vocal & Dynamics DSP Rack Parameters.
     * All parameters are atomic and real-time safe for lock-free audio thread access.
     */
    struct VocalDSPParameters
    {
        // Global / Module Bypasses
        std::atomic<bool> masterEnabled{false};
        std::atomic<bool> hpfBypassed{false};
        std::atomic<bool> gateBypassed{false};
        std::atomic<bool> eqBypassed{false};
        std::atomic<bool> deEsserBypassed{false};
        std::atomic<bool> compBypassed{false};
        std::atomic<bool> limiterBypassed{false};

        // 1. High-Pass Filter / Input Leveler (LVL)
        std::atomic<float> inputGainDb{0.0f};       // -18.0 to +18.0 dB
        std::atomic<float> hpfFreq{80.0f};          // 20.0 to 500.0 Hz
        std::atomic<int> hpfSlope{1};               // 0=6dB, 1=12dB, 2=18dB, 3=24dB/Oct

        // 2. Noise Gate / Expander
        enum GateMode : int { ModeGate = 0, ModeExpander = 1 };
        std::atomic<int> gateMode{ModeGate};
        std::atomic<float> gateOpenThresholdDb{-42.0f};  // -80.0 to 0.0 dB
        std::atomic<float> gateCloseThresholdDb{-50.0f}; // -80.0 to 0.0 dB (hysteresis)
        std::atomic<float> gateAttackMs{1.0f};           // 0.1 to 100.0 ms
        std::atomic<float> gateHoldMs{20.0f};            // 0.0 to 500.0 ms
        std::atomic<float> gateReleaseMs{100.0f};        // 10.0 to 1000.0 ms
        std::atomic<float> gateFloorDb{-60.0f};          // -80.0 to 0.0 dB (-Inf at -80)
        std::atomic<int> gateKeySource{0};              // 0=Internal, 1=External
        std::atomic<float> gateKeyHpfFreq{70.0f};        // 20.0 to 1000.0 Hz
        std::atomic<float> gateKeyLpfFreq{4000.0f};      // 200.0 to 20000.0 Hz

        // 3. De-Esser
        enum DeEsserType : int { TypeBell = 0, TypeShelf = 1 };
        std::atomic<int> deEsserType{TypeBell};
        std::atomic<float> deEsserThresholdDb{-16.0f};   // -40.0 to 0.0 dB
        std::atomic<float> deEsserFreq{5500.0f};         // 1000.0 to 12000.0 Hz
        std::atomic<float> deEsserRangeDb{-8.0f};        // -24.0 to 0.0 dB (max attenuation)

        // 4. Compressor
        enum KneeType : int { KneeSoft = 0, KneeNormal = 1, KneeHard = 2 };
        enum ReleaseMode : int { ReleaseManual = 0, ReleaseAuto = 1 };
        std::atomic<float> compThresholdDb{-24.0f};      // -60.0 to 0.0 dB
        std::atomic<float> compRatio{3.0f};              // 1.0 to 20.0
        std::atomic<float> compAttackMs{10.0f};          // 0.1 to 100.0 ms
        std::atomic<float> compReleaseMs{120.0f};        // 10.0 to 1000.0 ms
        std::atomic<int> compKnee{KneeNormal};           // Soft/Normal/Hard
        std::atomic<int> compReleaseMode{ReleaseManual}; // Manual/Auto
        std::atomic<float> compMakeupDb{3.0f};           // -12.0 to +24.0 dB
        std::atomic<float> compMixPercent{100.0f};       // 0.0 to 100.0% (parallel compression)
        std::atomic<int> compKeySource{0};              // 0=Internal, 1=De-Esser, 2=External
        std::atomic<float> compKeyHpfFreq{60.0f};        // 20.0 to 500.0 Hz
        std::atomic<float> compKeyLpfFreq{15000.0f};     // 2000.0 to 20000.0 Hz

        // 5. Limiter & Output Protection
        std::atomic<float> limiterCeilingDb{-1.0f};      // -20.0 to 0.0 dBFS
        std::atomic<float> limiterReleaseMs{50.0f};      // 1.0 to 500.0 ms
        std::atomic<float> outputGainDb{0.0f};           // -18.0 to +18.0 dB
    };

    /**
     * Professional Vocal DSP Processor.
     * Implements a full studio-grade vocal chain:
     * Input Trim/HPF -> Noise Gate/Expander -> 8-Band Parametric EQ -> De-Esser -> Compressor -> Limiter -> Output.
     * Real-time safe: Zero memory allocation, zero locks on audio callback.
     */
    class VocalDSPProcessor
    {
    public:
        VocalDSPProcessor();
        ~VocalDSPProcessor() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void reset();

        // Audio processing (Real-time safe, lock-free)
        void process(juce::AudioBuffer<float>& buffer, int numSamples);

        // Real-time Gain Reduction & Level Meters (in dB, readable by UI message thread)
        float getGateGainReductionDb() const noexcept { return gateGR.load(std::memory_order_relaxed); }
        float getCompGainReductionDb() const noexcept { return compGR.load(std::memory_order_relaxed); }
        float getDeEsserGainReductionDb() const noexcept { return deEsserGR.load(std::memory_order_relaxed); }
        float getLimiterGainReductionDb() const noexcept { return limiterGR.load(std::memory_order_relaxed); }

        float getInputPeakDb() const noexcept { return inPeakDb.load(std::memory_order_relaxed); }
        float getOutputPeakDb() const noexcept { return outPeakDb.load(std::memory_order_relaxed); }
        bool isClipping() const noexcept { return clipFlag.load(std::memory_order_relaxed); }
        void resetClipping() noexcept { clipFlag.store(false, std::memory_order_relaxed); }

        // Dynamic Compression Curve Helper for UI (returns output dB for given input dB)
        float getCompressionTransferDb(float inDb) const;

        // Embedded or bound 8-Band Parametric Equalizer
        void setEQ(EightBandEQProcessor* eq) noexcept { eqProcessor = eq ? eq : &internalEQ; }
        EightBandEQProcessor& getEQ() noexcept { return *eqProcessor; }
        const EightBandEQProcessor& getEQ() const noexcept { return *eqProcessor; }

        VocalDSPParameters params;

    private:
        // Processing stages
        void processHPF(juce::AudioBuffer<float>& buffer, int numSamples);
        void processGate(juce::AudioBuffer<float>& buffer, int numSamples);
        void processDeEsser(juce::AudioBuffer<float>& buffer, int numSamples);
        void processCompressor(juce::AudioBuffer<float>& buffer, int numSamples);
        void processLimiter(juce::AudioBuffer<float>& buffer, int numSamples);

        // Filter design helpers
        void updateHPFCoefficients();
        void updateGateKeyCoefficients();
        void updateCompKeyCoefficients();
        void updateDeEsserCoefficients();

        double sampleRate = 48000.0;
        int blockSize = 512;

        // 8-Band Parametric Equalizer (bound or internal fallback)
        EightBandEQProcessor internalEQ;
        EightBandEQProcessor* eqProcessor = &internalEQ;

        // 1. HPF Filters (Stereo)
        juce::dsp::IIR::Filter<float> hpfStage1[2];
        juce::dsp::IIR::Filter<float> hpfStage2[2];
        float lastHpfFreq = -1.0f;
        int lastHpfSlope = -1;

        // 2. Noise Gate / Expander State
        enum GateState { StateClosed = 0, StateAttack, StateOpen, StateHold, StateRelease };
        GateState gateState[2] = { StateClosed, StateClosed };
        float gateGain[2] = { 0.001f, 0.001f };
        int gateHoldCount[2] = { 0, 0 };
        juce::dsp::IIR::Filter<float> gateKeyHpf[2];
        juce::dsp::IIR::Filter<float> gateKeyLpf[2];

        // 3. De-Esser State & Sidechain Filter
        juce::dsp::IIR::Filter<float> deEsserSidechainFilter[2];
        juce::dsp::IIR::Filter<float> deEsserBellFilter[2];
        float deEsserGainEnv[2] = { 1.0f, 1.0f };
        float lastDeEsserFreq = -1.0f;
        int lastDeEsserType = -1;

        // 4. Compressor State & Sidechain Filters
        juce::dsp::IIR::Filter<float> compKeyHpf[2];
        juce::dsp::IIR::Filter<float> compKeyLpf[2];
        float compGainEnv[2] = { 1.0f, 1.0f };
        float autoReleaseMs[2] = { 120.0f, 120.0f };
        juce::AudioBuffer<float> compDryBuffer;

        // 5. Limiter State
        float limiterEnvelope[2] = { 0.0f, 0.0f };

        // Detector Envelopes
        float gateKeyEnvelope[2] = { 0.0f, 0.0f };
        float compKeyEnvelope[2] = { 0.0f, 0.0f };
        float deEsserSidechainEnvelope[2] = { 0.0f, 0.0f };

        // Output Level smoothing
        juce::SmoothedValue<float> smoothedInputGain;
        juce::SmoothedValue<float> smoothedOutputGain;

        // Real-time metering atomics (read by UI message thread)
        std::atomic<float> gateGR{0.0f};
        std::atomic<float> compGR{0.0f};
        std::atomic<float> deEsserGR{0.0f};
        std::atomic<float> limiterGR{0.0f};
        std::atomic<float> inPeakDb{-100.0f};
        std::atomic<float> outPeakDb{-100.0f};
        std::atomic<bool> clipFlag{false};
    };
}
