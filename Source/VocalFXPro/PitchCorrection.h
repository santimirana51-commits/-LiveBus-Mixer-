#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include <array>

/**
 * Real-time AutoTune Scale Quantizer and Pitch Shifter for vocals.
 * Supports Chromatic, Major, Minor, Harmonic Minor, and Melodic Minor scales.
 * Includes natural smoothing vs hard-tune instant note snapping.
 */
class VocalPitchCorrection
{
public:
    enum ScaleType
    {
        ScaleMajor = 0,
        ScaleMinor = 1,
        ScaleChromatic = 2,
        ScaleHarmonicMinor = 3,
        ScaleMelodicMinor = 4
    };

    VocalPitchCorrection() = default;

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxDelaySamples = juce::jmax(1024, (int)(sr * 0.1)); // 100 ms max window
        delayRingL.assign((size_t)maxDelaySamples, 0.0f);
        delayRingR.assign((size_t)maxDelaySamples, 0.0f);
        writePos = 0;
        phase1 = 0.0f;
        phase2 = 0.5f;
        currentPitchShiftSemitones = 0.0f;
        targetPitchShiftSemitones = 0.0f;
        smoothedShift = 0.0f;
    }

    void reset()
    {
        std::fill(delayRingL.begin(), delayRingL.end(), 0.0f);
        std::fill(delayRingR.begin(), delayRingR.end(), 0.0f);
        writePos = 0;
        phase1 = 0.0f;
        phase2 = 0.5f;
        currentPitchShiftSemitones = 0.0f;
        targetPitchShiftSemitones = 0.0f;
        smoothedShift = 0.0f;
    }

    /**
     * Converts a frequency (Hz) to a fractional MIDI note number.
     * A4 = 440 Hz = MIDI 69
     */
    static float hzToMidiNote(float hz) noexcept
    {
        if (hz <= 10.0f) return 0.0f;
        return 69.0f + 12.0f * std::log2(hz / 440.0f);
    }

    /**
     * Converts a MIDI note number back to frequency in Hz.
     */
    static float midiNoteToHz(float midiNote) noexcept
    {
        return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
    }

    /**
     * Finds the closest valid MIDI note in the given musical Key (0=C, 1=C#, ...)
     * and Scale (Major, Minor, Chromatic, etc.).
     */
    static float findClosestScaleNote(float midiNote, int rootKey, ScaleType scale) noexcept
    {
        if (scale == ScaleChromatic)
            return std::round(midiNote);

        // Scale pitch classes relative to root key
        // Major: 0, 2, 4, 5, 7, 9, 11
        // Minor (Natural): 0, 2, 3, 5, 7, 8, 10
        // Harmonic Minor: 0, 2, 3, 5, 7, 8, 11
        // Melodic Minor: 0, 2, 3, 5, 7, 9, 11
        static const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };
        static const int harmMinor[]  = { 0, 2, 3, 5, 7, 8, 11 };
        static const int melMinor[]   = { 0, 2, 3, 5, 7, 9, 11 };

        const int* scaleIntervals = majorScale;
        int numNotes = 7;

        if (scale == ScaleMinor) scaleIntervals = minorScale;
        else if (scale == ScaleHarmonicMinor) scaleIntervals = harmMinor;
        else if (scale == ScaleMelodicMinor) scaleIntervals = melMinor;

        // Search closest pitch class within octave
        float minDistance = 999.0f;
        float bestNote = std::round(midiNote);

        int baseOctave = (int)std::floor(midiNote / 12.0f) * 12;

        for (int octOffset = -12; octOffset <= 12; octOffset += 12)
        {
            for (int i = 0; i < numNotes; ++i)
            {
                float candidateNote = (float)(baseOctave + octOffset + rootKey + scaleIntervals[i]);
                float dist = std::abs(candidateNote - midiNote);
                if (dist < minDistance)
                {
                    minDistance = dist;
                    bestNote = candidateNote;
                }
            }
        }

        return bestNote;
    }

    /**
     * Compute pitch correction and process buffer through pitch-shifter.
     * @param buffer In/Out audio buffer
     * @param numSamples Samples in buffer
     * @param detectedHz Frequency detected by pitch detector
     * @param rootKey 0=C, 1=C#, 2=D, ... 11=B
     * @param scale ScaleType enum
     * @param retuneSpeed 0.0 (slow natural) to 1.0 (instant hard tune)
     * @param isHardTune true to snap without transition delay
     * @param humanize 0.0 to 1.0 (retains slight natural micro-intonation)
     * @param manualShiftSemitones Additional manual pitch transposition (-12 to +12)
     * @param outTargetHz Out pointer for GUI visualizer
     */
    void process(juce::AudioBuffer<float>& buffer, int numSamples,
                 float detectedHz, int rootKey, ScaleType scale,
                 float retuneSpeed, bool isHardTune,
                 float humanize = 0.0f, float manualShiftSemitones = 0.0f,
                 float* outTargetHz = nullptr)
    {
        if (numSamples <= 0) return;

        float targetHz = detectedHz;
        if (detectedHz > 30.0f)
        {
            float inputMidi = hzToMidiNote(detectedHz);
            float targetMidi = findClosestScaleNote(inputMidi, rootKey, scale);
            targetHz = midiNoteToHz(targetMidi);

            float diffSemitones = targetMidi - inputMidi;
            // Humanize reduces over-correction by preserving subtle natural micro-pitch variations
            if (humanize > 0.0f)
            {
                float humanScale = 1.0f - juce::jlimit(0.0f, 0.85f, humanize * 0.4f);
                diffSemitones *= humanScale;
            }

            // Combine AutoTune correction with optional manual pitch shift
            float totalShift = diffSemitones + manualShiftSemitones;
            targetPitchShiftSemitones = juce::jlimit(-12.0f, 12.0f, totalShift);
        }
        else
        {
            targetPitchShiftSemitones = juce::jlimit(-12.0f, 12.0f, manualShiftSemitones);
        }

        if (outTargetHz != nullptr)
            *outTargetHz = targetHz;

        // Smoothing coefficient based on retune speed and mode
        // retuneSpeed: 0.0 -> smooth (tau ~ 80ms), 1.0 -> hard tune (tau ~ 0ms)
        float smoothCoeff;
        if (isHardTune || retuneSpeed >= 0.95f)
        {
            smoothCoeff = 0.95f; // Rapid snapping
        }
        else
        {
            float alpha = juce::jmap(retuneSpeed, 0.0f, 1.0f, 0.03f, 0.5f);
            smoothCoeff = alpha;
        }

        // Window size for pitch shifter grain (around 25-35 ms for vocal clarity)
        const float grainPeriodSamples = (float)(sr * 0.030); // ~30ms grain
        const int ringSize = (int)delayRingL.size();
        const int numChannels = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            // Smooth toward target shift
            smoothedShift = (1.0f - smoothCoeff) * smoothedShift + smoothCoeff * targetPitchShiftSemitones;

            // Pitch ratio = 2^(semitones / 12)
            float pitchRatio = std::pow(2.0f, smoothedShift / 12.0f);
            float deltaPhase = (1.0f - pitchRatio) / grainPeriodSamples;

            // Advance dual grain pointers
            phase1 += deltaPhase;
            while (phase1 >= 1.0f) phase1 -= 1.0f;
            while (phase1 < 0.0f)  phase1 += 1.0f;

            phase2 = phase1 + 0.5f;
            if (phase2 >= 1.0f) phase2 -= 1.0f;

            // Triangular / Hann crossfade window
            float win1 = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase1));
            float win2 = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase2));

            float delay1 = phase1 * grainPeriodSamples;
            float delay2 = phase2 * grainPeriodSamples;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& ring = (ch == 0) ? delayRingL : delayRingR;
                float inSample = buffer.getSample(ch, i);

                // Write to ring buffer
                ring[(size_t)writePos] = inSample;

                // Read tap 1
                float readPos1 = (float)writePos - delay1;
                while (readPos1 < 0.0f) readPos1 += (float)ringSize;
                int r1_int = (int)readPos1;
                float r1_frac = readPos1 - (float)r1_int;
                float tap1 = ring[(size_t)(r1_int % ringSize)] * (1.0f - r1_frac)
                           + ring[(size_t)((r1_int + 1) % ringSize)] * r1_frac;

                // Read tap 2
                float readPos2 = (float)writePos - delay2;
                while (readPos2 < 0.0f) readPos2 += (float)ringSize;
                int r2_int = (int)readPos2;
                float r2_frac = readPos2 - (float)r2_int;
                float tap2 = ring[(size_t)(r2_int % ringSize)] * (1.0f - r2_frac)
                           + ring[(size_t)((r2_int + 1) % ringSize)] * r2_frac;

                float shifted = tap1 * win1 + tap2 * win2;
                buffer.setSample(ch, i, shifted);
            }

            writePos = (writePos + 1) % ringSize;
        }
    }

private:
    double sr = 44100.0;
    int maxDelaySamples = 4410;
    std::vector<float> delayRingL;
    std::vector<float> delayRingR;
    int writePos = 0;
    float phase1 = 0.0f;
    float phase2 = 0.5f;
    float currentPitchShiftSemitones = 0.0f;
    float targetPitchShiftSemitones = 0.0f;
    float smoothedShift = 0.0f;
};
