#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>

template <int NumBands>
class CrossfadingEQFilterBank
{
public:
    using CoefficientSet = std::array<std::array<float, 5>, NumBands>;
    using EnabledSet = std::array<bool, NumBands>;

    template <typename CoefficientPointers>
    void prepare(const CoefficientPointers& coefficients)
    {
        EnabledSet enabled{};
        enabled.fill(true);
        prepare(coefficients, enabled);
    }

    template <typename CoefficientPointers>
    void prepare(const CoefficientPointers& coefficients,
                 const EnabledSet& enabled)
    {
        for (int bank = 0; bank < 2; ++bank)
            for (int channel = 0; channel < 2; ++channel)
                for (int band = 0; band < NumBands; ++band)
                {
                    filters[(size_t)bank][(size_t)channel][(size_t)band]
                        .coefficients = new juce::dsp::IIR::Coefficients<float>(
                            *coefficients[(size_t)band]);
                    filters[(size_t)bank][(size_t)channel][(size_t)band].reset();
                }
        enabledBanks[0] = enabled;
        enabledBanks[1] = enabled;
        activeBank = 0;
        fadeProgress = fadeLength;
    }

    bool beginTransition(const CoefficientSet& coefficients,
                         int transitionSamples) noexcept
    {
        return beginTransition(coefficients, enabledBanks[(size_t)activeBank],
                               transitionSamples);
    }

    bool beginTransition(const CoefficientSet& coefficients,
                         const EnabledSet& enabled,
                         int transitionSamples) noexcept
    {
        if (isTransitioning())
            return false;
        const int nextBank = 1 - activeBank;
        for (int channel = 0; channel < 2; ++channel)
            for (int band = 0; band < NumBands; ++band)
            {
                auto& filter =
                    filters[(size_t)nextBank][(size_t)channel][(size_t)band];
                auto* destination = filter.coefficients->coefficients.begin();
                for (int coefficient = 0; coefficient < 5; ++coefficient)
                    destination[coefficient] =
                        coefficients[(size_t)band][(size_t)coefficient];
                filter.reset();
            }
        enabledBanks[(size_t)nextBank] = enabled;
        fadeLength = juce::jmax(1, transitionSamples);
        fadeProgress = 0;
        return true;
    }

    float processSample(int channel, float input,
                        const EnabledSet& enabled) noexcept
    {
        const auto processBank = [&](int bank)
        {
            float output = input;
            for (int band = 0; band < NumBands; ++band)
                if (enabled[(size_t)band])
                    output = filters[(size_t)bank][(size_t)channel]
                                     [(size_t)band].processSample(output);
            return output;
        };
        const float active = processBank(activeBank);
        if (!isTransitioning())
            return active;
        const float next = processBank(1 - activeBank);
        const float proportion = (float)fadeProgress / (float)fadeLength;
        return active + (next - active) * proportion;
    }

    float processSample(int channel, float input) noexcept
    {
        const auto processBank = [&](int bank)
        {
            float output = input;
            for (int band = 0; band < NumBands; ++band)
                if (enabledBanks[(size_t)bank][(size_t)band])
                    output = filters[(size_t)bank][(size_t)channel]
                                     [(size_t)band].processSample(output);
            return output;
        };
        const float active = processBank(activeBank);
        if (!isTransitioning())
            return active;
        const float next = processBank(1 - activeBank);
        const float proportion = (float)fadeProgress / (float)fadeLength;
        return active + (next - active) * proportion;
    }

    void advanceSample() noexcept
    {
        if (isTransitioning() && ++fadeProgress >= fadeLength)
            activeBank = 1 - activeBank;
    }

    bool isTransitioning() const noexcept { return fadeProgress < fadeLength; }

    void reset() noexcept
    {
        for (auto& bank : filters)
            for (auto& channel : bank)
                for (auto& filter : channel)
                    filter.reset();
        if (isTransitioning())
            activeBank = 1 - activeBank;
        fadeProgress = fadeLength;
    }

private:
    std::array<std::array<std::array<juce::dsp::IIR::Filter<float>, NumBands>, 2>, 2>
        filters{};
    std::array<EnabledSet, 2> enabledBanks{};
    int activeBank = 0;
    int fadeProgress = 1;
    int fadeLength = 1;
};

namespace LiveBus
{
    constexpr int numEQBands = 8;
    constexpr int fftOrder = 12;            // 2^12 = 4096 points
    constexpr int fftSize = 1 << fftOrder;  // 4096

    enum class EQFilterType : int
    {
        HighPass = 0,
        LowShelf,
        Bell,
        HighShelf,
        LowPass,
        NumTypes
    };

    inline juce::String getFilterTypeName(EQFilterType type)
    {
        switch (type)
        {
            case EQFilterType::HighPass:  return "High-Pass";
            case EQFilterType::LowShelf:  return "Low Shelf";
            case EQFilterType::Bell:      return "Bell";
            case EQFilterType::HighShelf: return "High Shelf";
            case EQFilterType::LowPass:   return "Low-Pass";
            default:                      return "Bell";
        }
    }

    inline EQFilterType getFilterTypeFromName(const juce::String& name)
    {
        if (name.equalsIgnoreCase("High-Pass") || name.equalsIgnoreCase("HighPass"))
            return EQFilterType::HighPass;
        if (name.equalsIgnoreCase("Low Shelf") || name.equalsIgnoreCase("LowShelf"))
            return EQFilterType::LowShelf;
        if (name.equalsIgnoreCase("High Shelf") || name.equalsIgnoreCase("HighShelf"))
            return EQFilterType::HighShelf;
        if (name.equalsIgnoreCase("Low-Pass") || name.equalsIgnoreCase("LowPass"))
            return EQFilterType::LowPass;
        return EQFilterType::Bell;
    }

    struct EQBandParameter
    {
        std::atomic<EQFilterType> type{EQFilterType::Bell};
        std::atomic<float> freq{1000.0f};      // 20 Hz .. 20000 Hz
        std::atomic<float> gainDb{0.0f};       // -18.0 dB .. +18.0 dB
        std::atomic<float> q{0.707f};          // 0.1 .. 10.0
        std::atomic<bool> enabled{true};       // Individual band bypass

        EQBandParameter() = default;
        EQBandParameter(EQFilterType t, float f, float g, float qVal, bool en = true)
            : type(t), freq(f), gainDb(g), q(qVal), enabled(en) {}

        EQBandParameter(const EQBandParameter& other)
        {
            type.store(other.type.load(std::memory_order_relaxed));
            freq.store(other.freq.load(std::memory_order_relaxed));
            gainDb.store(other.gainDb.load(std::memory_order_relaxed));
            q.store(other.q.load(std::memory_order_relaxed));
            enabled.store(other.enabled.load(std::memory_order_relaxed));
        }

        EQBandParameter& operator=(const EQBandParameter& other)
        {
            if (this != &other)
            {
                type.store(other.type.load(std::memory_order_relaxed));
                freq.store(other.freq.load(std::memory_order_relaxed));
                gainDb.store(other.gainDb.load(std::memory_order_relaxed));
                q.store(other.q.load(std::memory_order_relaxed));
                enabled.store(other.enabled.load(std::memory_order_relaxed));
            }
            return *this;
        }
    };

    struct EightBandEQParameters
    {
        std::atomic<bool> masterEnabled{true};
        std::array<EQBandParameter, numEQBands> bands;

        EightBandEQParameters()
        {
            setToDefault();
        }

        EightBandEQParameters(const EightBandEQParameters& other)
        {
            masterEnabled.store(other.masterEnabled.load(std::memory_order_relaxed));
            for (int i = 0; i < numEQBands; ++i)
                bands[i] = other.bands[i];
        }

        EightBandEQParameters& operator=(const EightBandEQParameters& other)
        {
            if (this != &other)
            {
                masterEnabled.store(other.masterEnabled.load(std::memory_order_relaxed));
                for (int i = 0; i < numEQBands; ++i)
                    bands[i] = other.bands[i];
            }
            return *this;
        }

        void setToDefault()
        {
            masterEnabled.store(true);

            // Default broadcast reference tuning matching visual mockup:
            // Band 1: High-Pass, 80 Hz, 0.0 dB, Q 0.71, On
            bands[0].type.store(EQFilterType::HighPass);
            bands[0].freq.store(80.0f);
            bands[0].gainDb.store(0.0f);
            bands[0].q.store(0.71f);
            bands[0].enabled.store(true);

            // Band 2: Low Shelf, 120 Hz, +4.5 dB, Q 0.71, On
            bands[1].type.store(EQFilterType::LowShelf);
            bands[1].freq.store(120.0f);
            bands[1].gainDb.store(4.5f);
            bands[1].q.store(0.71f);
            bands[1].enabled.store(true);

            // Band 3: Bell, 350 Hz, -6.0 dB, Q 1.20, On
            bands[2].type.store(EQFilterType::Bell);
            bands[2].freq.store(350.0f);
            bands[2].gainDb.store(-6.0f);
            bands[2].q.store(1.20f);
            bands[2].enabled.store(true);

            // Band 4: Bell, 1200 Hz, +5.5 dB, Q 1.00, On
            bands[3].type.store(EQFilterType::Bell);
            bands[3].freq.store(1200.0f);
            bands[3].gainDb.store(5.5f);
            bands[3].q.store(1.00f);
            bands[3].enabled.store(true);

            // Band 5: Bell, 2500 Hz, -4.0 dB, Q 1.20, On
            bands[4].type.store(EQFilterType::Bell);
            bands[4].freq.store(2500.0f);
            bands[4].gainDb.store(-4.0f);
            bands[4].q.store(1.20f);
            bands[4].enabled.store(true);

            // Band 6: Bell, 5000 Hz, +3.5 dB, Q 1.00, On
            bands[5].type.store(EQFilterType::Bell);
            bands[5].freq.store(5000.0f);
            bands[5].gainDb.store(3.5f);
            bands[5].q.store(1.00f);
            bands[5].enabled.store(true);

            // Band 7: Bell, 10000 Hz, +2.0 dB, Q 0.71, On
            bands[6].type.store(EQFilterType::Bell);
            bands[6].freq.store(10000.0f);
            bands[6].gainDb.store(2.0f);
            bands[6].q.store(0.71f);
            bands[6].enabled.store(true);

            // Band 8: Low-Pass, 16000 Hz, 0.0 dB, Q 0.71, On
            bands[7].type.store(EQFilterType::LowPass);
            bands[7].freq.store(16000.0f);
            bands[7].gainDb.store(0.0f);
            bands[7].q.store(0.71f);
            bands[7].enabled.store(true);
        }

        void setToFlat()
        {
            masterEnabled.store(true);
            for (int i = 0; i < numEQBands; ++i)
            {
                bands[i].gainDb.store(0.0f);
                bands[i].enabled.store(true);
            }
            bands[0].type.store(EQFilterType::HighPass);
            bands[0].freq.store(20.0f);
            bands[0].q.store(0.707f);

            bands[1].type.store(EQFilterType::LowShelf);
            bands[1].freq.store(100.0f);
            bands[1].q.store(0.707f);

            bands[2].type.store(EQFilterType::Bell);
            bands[2].freq.store(250.0f);
            bands[2].q.store(1.0f);

            bands[3].type.store(EQFilterType::Bell);
            bands[3].freq.store(1000.0f);
            bands[3].q.store(1.0f);

            bands[4].type.store(EQFilterType::Bell);
            bands[4].freq.store(3000.0f);
            bands[4].q.store(1.0f);

            bands[5].type.store(EQFilterType::Bell);
            bands[5].freq.store(6000.0f);
            bands[5].q.store(1.0f);

            bands[6].type.store(EQFilterType::HighShelf);
            bands[6].freq.store(10000.0f);
            bands[6].q.store(0.707f);

            bands[7].type.store(EQFilterType::LowPass);
            bands[7].freq.store(20000.0f);
            bands[7].q.store(0.707f);
        }
    };

    // Preset Data Model
    struct EQPreset
    {
        juce::String name;
        EightBandEQParameters params;
    };

    class EightBandEQProcessor
    {
    public:
        EightBandEQProcessor();
        ~EightBandEQProcessor() = default;

        void prepare(double sampleRate, int maxBlockSize);
        void reset();

        // Parameter validation helpers
        static float validateFrequency(float freq, double sampleRate);
        static float validateGain(float gainDb);
        static float validateQ(float q);

        // Coefficient update (called when parameters change or in prepare)
        void updateCoefficients();

        // Audio processing (real-time safe, 0 heap allocation, 0 lock wait)
        void process(juce::AudioBuffer<float>& buffer, int numSamples);

        // True frequency response magnitude calculation in dB at frequency 'f'
        float getMagnitudeForFrequency(float frequency) const;
        float getBandMagnitudeForFrequency(int bandIndex, float frequency) const;

        // Spectrum Analyzer FIFO access
        void pushToAnalyzer(const float* channelData, int numSamples);
        bool pullFromAnalyzer(float* destBuffer, int numSamples);

        // Parameter access
        EightBandEQParameters params;

        // Real-time meter readings
        float getMeterInL() const { return meterInL.load(std::memory_order_relaxed); }
        float getMeterInR() const { return meterInR.load(std::memory_order_relaxed); }
        float getMeterOutL() const { return meterOutL.load(std::memory_order_relaxed); }
        float getMeterOutR() const { return meterOutR.load(std::memory_order_relaxed); }
        double getSampleRate() const noexcept { return currentSampleRate; }

        // Analyzer mode
        void setAnalyzerEnabled(bool enabled) { analyzerEnabled.store(enabled, std::memory_order_relaxed); }
        bool isAnalyzerEnabled() const { return analyzerEnabled.load(std::memory_order_relaxed); }

        void setAnalyzerPost(bool isPost) { analyzerPost.store(isPost, std::memory_order_relaxed); }
        bool isAnalyzerPost() const { return analyzerPost.load(std::memory_order_relaxed); }

    private:
        void applyPendingCoefficients() noexcept;

        double currentSampleRate = 48000.0;
        int currentBlockSize = 512;

        using CoefficientPointers =
            std::array<juce::ReferenceCountedObjectPtr<
                           juce::dsp::IIR::Coefficients<float>>,
                       numEQBands>;
        CoefficientPointers currentCoefficients{};
        std::array<std::atomic<float>, numEQBands * 5> coefficientSnapshot{};
        std::array<std::atomic<bool>, numEQBands> enabledSnapshot{};
        std::atomic<uint64_t> coefficientSequence{0};
        uint64_t audioCoefficientSequence = 0;
        std::mutex coefficientWriterMutex;
        CrossfadingEQFilterBank<numEQBands> filterBank;
        bool prepared = false;
        float masterMix = 1.0f;

        // Real-time meters
        std::atomic<float> meterInL{0.0f};
        std::atomic<float> meterInR{0.0f};
        std::atomic<float> meterOutL{0.0f};
        std::atomic<float> meterOutR{0.0f};

        // Spectrum analyzer ring FIFO
        std::atomic<bool> analyzerEnabled{true};
        std::atomic<bool> analyzerPost{true};
        static constexpr int analyzerFifoSize = 8192;
        juce::AbstractFifo analyzerFifo{analyzerFifoSize};
        std::array<float, analyzerFifoSize> analyzerBuffer{};
    };
}
