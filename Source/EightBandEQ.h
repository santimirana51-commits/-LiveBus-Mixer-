#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <string>

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

        // Analyzer mode
        void setAnalyzerEnabled(bool enabled) { analyzerEnabled.store(enabled, std::memory_order_relaxed); }
        bool isAnalyzerEnabled() const { return analyzerEnabled.load(std::memory_order_relaxed); }

        void setAnalyzerPost(bool isPost) { analyzerPost.store(isPost, std::memory_order_relaxed); }
        bool isAnalyzerPost() const { return analyzerPost.load(std::memory_order_relaxed); }

    private:
        double currentSampleRate = 48000.0;
        int currentBlockSize = 512;

        // 2 audio channels (Stereo) x 8 cascaded IIR filters
        juce::dsp::IIR::Filter<float> filters[2][numEQBands];

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
