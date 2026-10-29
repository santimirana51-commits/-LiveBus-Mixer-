#include "RNNoiseVST3Plugin.h"
#include "StudioVocalPluginEditor.h"
#include <cmath>

RNNoiseVST3Plugin::RNNoiseVST3Plugin()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "RNNoiseVocalStudio", createParameterLayout()),
      worker(std::make_unique<LiveBus::AsyncAudioWorker>(suppressor))
{
    masterBypassParam = parameters.getRawParameterValue("master_bypass");

    // RNNoise
    rnnoiseBypassParam = parameters.getRawParameterValue("rnnoise_bypass");
    enableVADParam = parameters.getRawParameterValue("enable_vad");
    vadThresholdParam = parameters.getRawParameterValue("vad_threshold");
    gracePeriodParam = parameters.getRawParameterValue("grace_period_ms");
    wetLevelParam = parameters.getRawParameterValue("wet_percent");

    // Input Trim & HPF
    inputGainParam = parameters.getRawParameterValue("input_gain_db");
    hpfBypassParam = parameters.getRawParameterValue("hpf_bypass");
    hpfFreqParam = parameters.getRawParameterValue("hpf_freq");
    hpfSlopeParam = parameters.getRawParameterValue("hpf_slope");

    // Noise Gate
    gateBypassParam = parameters.getRawParameterValue("gate_bypass");
    gateThresholdParam = parameters.getRawParameterValue("gate_threshold");
    gateAttackParam = parameters.getRawParameterValue("gate_attack");
    gateHoldParam = parameters.getRawParameterValue("gate_hold");
    gateReleaseParam = parameters.getRawParameterValue("gate_release");
    gateFloorParam = parameters.getRawParameterValue("gate_floor");

    // 8-Band Parametric EQ
    eqBypassParam = parameters.getRawParameterValue("eq_bypass");
    for (int b = 0; b < LiveBus::numEQBands; ++b)
    {
        const juce::String prefix = "eq_band_" + juce::String(b + 1) + "_";
        eqBandParams[(size_t)b].freq = parameters.getRawParameterValue(prefix + "freq");
        eqBandParams[(size_t)b].gain = parameters.getRawParameterValue(prefix + "gain");
        eqBandParams[(size_t)b].q = parameters.getRawParameterValue(prefix + "q");
        eqBandParams[(size_t)b].type = parameters.getRawParameterValue(prefix + "type");
        eqBandParams[(size_t)b].enabled = parameters.getRawParameterValue(prefix + "enabled");
    }

    // De-Esser
    deEsserBypassParam = parameters.getRawParameterValue("deesser_bypass");
    deEsserThresholdParam = parameters.getRawParameterValue("deesser_threshold");
    deEsserFreqParam = parameters.getRawParameterValue("deesser_freq");
    deEsserRangeParam = parameters.getRawParameterValue("deesser_range");
    deEsserTypeParam = parameters.getRawParameterValue("deesser_type");

    // Compressor
    compBypassParam = parameters.getRawParameterValue("comp_bypass");
    compThresholdParam = parameters.getRawParameterValue("comp_threshold");
    compRatioParam = parameters.getRawParameterValue("comp_ratio");
    compAttackParam = parameters.getRawParameterValue("comp_attack");
    compReleaseParam = parameters.getRawParameterValue("comp_release");
    compKneeParam = parameters.getRawParameterValue("comp_knee");
    compMakeupParam = parameters.getRawParameterValue("comp_makeup");
    compMixParam = parameters.getRawParameterValue("comp_mix");
    compAutoReleaseParam = parameters.getRawParameterValue("comp_autorelease");

    // Vocal Enhancement
    enhanceBypassParam = parameters.getRawParameterValue("enhance_bypass");
    warmthParam = parameters.getRawParameterValue("enhance_warmth");
    bodyParam = parameters.getRawParameterValue("enhance_body");
    clarityParam = parameters.getRawParameterValue("enhance_clarity");
    presenceParam = parameters.getRawParameterValue("enhance_presence");
    airParam = parameters.getRawParameterValue("enhance_air");

    // Limiter & Output Gain
    limiterBypassParam = parameters.getRawParameterValue("limiter_bypass");
    limiterCeilingParam = parameters.getRawParameterValue("limiter_ceiling");
    limiterReleaseParam = parameters.getRawParameterValue("limiter_release");
    outputGainParam = parameters.getRawParameterValue("output_gain_db");

    // Enable master vocal processor
    vocalProcessor.params.masterEnabled.store(true, std::memory_order_relaxed);
}

RNNoiseVST3Plugin::~RNNoiseVST3Plugin()
{
    releaseResources();
}

juce::AudioProcessorValueTreeState::ParameterLayout RNNoiseVST3Plugin::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Master Bypass
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"master_bypass", 1}, "Master Bypass", false));

    // --- RNNoise Core Engine ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"rnnoise_bypass", 1}, "RNNoise Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"enable_vad", 1}, "Enable VAD", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vad_threshold", 1}, "VAD Threshold",
        juce::NormalisableRange<float>{0.0f, 1.0f, 0.01f}, 0.60f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grace_period_ms", 1}, "VAD Grace Period",
        juce::NormalisableRange<float>{0.0f, 2000.0f, 10.0f}, 200.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"wet_percent", 1}, "RNNoise Wet Mix",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- Input Trim & HPF ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"input_gain_db", 1}, "Input Gain",
        juce::NormalisableRange<float>{-18.0f, 18.0f, 0.1f}, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"hpf_bypass", 1}, "HPF Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpf_freq", 1}, "HPF Frequency",
        juce::NormalisableRange<float>{20.0f, 500.0f, 1.0f, 0.4f}, 80.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"hpf_slope", 1}, "HPF Slope",
        juce::StringArray{"6 dB/Oct", "12 dB/Oct", "18 dB/Oct", "24 dB/Oct"}, 1));

    // --- Noise Gate ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"gate_bypass", 1}, "Gate Bypass", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate_threshold", 1}, "Gate Threshold",
        juce::NormalisableRange<float>{-80.0f, 0.0f, 0.5f}, -45.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate_attack", 1}, "Gate Attack",
        juce::NormalisableRange<float>{0.1f, 100.0f, 0.1f, 0.4f}, 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate_hold", 1}, "Gate Hold",
        juce::NormalisableRange<float>{0.0f, 500.0f, 1.0f}, 25.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate_release", 1}, "Gate Release",
        juce::NormalisableRange<float>{10.0f, 1000.0f, 1.0f, 0.4f}, 120.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gate_floor", 1}, "Gate Floor",
        juce::NormalisableRange<float>{-80.0f, 0.0f, 1.0f}, -60.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // --- 8-Band Parametric EQ ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"eq_bypass", 1}, "EQ Bypass", false));

    const float defaultFreqs[LiveBus::numEQBands] = { 80.0f, 160.0f, 350.0f, 800.0f, 1800.0f, 3500.0f, 8000.0f, 14000.0f };
    const int defaultTypes[LiveBus::numEQBands] = { 0, 1, 2, 2, 2, 2, 3, 4 };

    for (int b = 0; b < LiveBus::numEQBands; ++b)
    {
        const juce::String prefix = "eq_band_" + juce::String(b + 1) + "_";
        const juce::String bandName = "Band " + juce::String(b + 1) + " ";

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "freq", 1}, bandName + "Freq",
            juce::NormalisableRange<float>{20.0f, 20000.0f, 1.0f, 0.35f}, defaultFreqs[b],
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "gain", 1}, bandName + "Gain",
            juce::NormalisableRange<float>{-18.0f, 18.0f, 0.1f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "q", 1}, bandName + "Q",
            juce::NormalisableRange<float>{0.1f, 10.0f, 0.05f, 0.5f}, 0.707f));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "type", 1}, bandName + "Type",
            juce::StringArray{"High-Pass", "Low Shelf", "Bell", "High Shelf", "Low-Pass"}, defaultTypes[b]));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "enabled", 1}, bandName + "Enable", true));
    }

    // --- De-Esser ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"deesser_bypass", 1}, "De-Esser Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"deesser_threshold", 1}, "De-Esser Threshold",
        juce::NormalisableRange<float>{-40.0f, 0.0f, 0.5f}, -18.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"deesser_freq", 1}, "De-Esser Freq",
        juce::NormalisableRange<float>{1000.0f, 12000.0f, 10.0f, 0.5f}, 5500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"deesser_range", 1}, "De-Esser Range",
        juce::NormalisableRange<float>{-24.0f, 0.0f, 0.5f}, -8.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"deesser_type", 1}, "De-Esser Mode",
        juce::StringArray{"Bell Filter", "High-Shelf"}, 0));

    // --- Compressor ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"comp_bypass", 1}, "Compressor Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_threshold", 1}, "Comp Threshold",
        juce::NormalisableRange<float>{-60.0f, 0.0f, 0.5f}, -24.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_ratio", 1}, "Comp Ratio",
        juce::NormalisableRange<float>{1.0f, 20.0f, 0.1f, 0.4f}, 3.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_attack", 1}, "Comp Attack",
        juce::NormalisableRange<float>{0.1f, 100.0f, 0.1f, 0.4f}, 12.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_release", 1}, "Comp Release",
        juce::NormalisableRange<float>{10.0f, 1000.0f, 1.0f, 0.4f}, 120.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"comp_knee", 1}, "Comp Knee",
        juce::StringArray{"Soft", "Normal", "Hard"}, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_makeup", 1}, "Comp Makeup",
        juce::NormalisableRange<float>{-12.0f, 24.0f, 0.5f}, 3.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"comp_mix", 1}, "Comp Wet Mix",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"comp_autorelease", 1}, "Comp Auto-Release", false));

    // --- Vocal Enhancement ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"enhance_bypass", 1}, "Enhancer Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"enhance_warmth", 1}, "Warmth",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 25.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"enhance_body", 1}, "Body",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 15.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"enhance_clarity", 1}, "Clarity",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"enhance_presence", 1}, "Presence",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"enhance_air", 1}, "Air",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 20.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // --- Limiter & Output Protection ---
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"limiter_bypass", 1}, "Limiter Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"limiter_ceiling", 1}, "Limiter Ceiling",
        juce::NormalisableRange<float>{-20.0f, 0.0f, 0.1f}, -1.0f,
        juce::AudioParameterFloatAttributes().withLabel("dBFS")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"limiter_release", 1}, "Limiter Release",
        juce::NormalisableRange<float>{1.0f, 500.0f, 1.0f, 0.4f}, 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output_gain_db", 1}, "Output Gain",
        juce::NormalisableRange<float>{-18.0f, 18.0f, 0.1f}, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return layout;
}

void RNNoiseVST3Plugin::syncParametersFromTreeState() noexcept
{
    // 1. RNNoise controls
    const bool rnnoiseBypassed = (rnnoiseBypassParam != nullptr && rnnoiseBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    suppressor.setVADEnabled(enableVADParam != nullptr && enableVADParam->load(std::memory_order_relaxed) >= 0.5f);
    if (vadThresholdParam != nullptr)
        suppressor.setVADThreshold(vadThresholdParam->load(std::memory_order_relaxed));
    if (gracePeriodParam != nullptr)
        suppressor.setVADGracePeriodMs(gracePeriodParam->load(std::memory_order_relaxed));
    if (wetLevelParam != nullptr)
        suppressor.setWetLevel(rnnoiseBypassed ? 0.0f : (wetLevelParam->load(std::memory_order_relaxed) * 0.01f));

    // 2. Input Gain & HPF
    if (inputGainParam != nullptr)
        vocalProcessor.params.inputGainDb.store(inputGainParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (hpfBypassParam != nullptr)
        vocalProcessor.params.hpfBypassed.store(hpfBypassParam->load(std::memory_order_relaxed) >= 0.5f, std::memory_order_relaxed);
    if (hpfFreqParam != nullptr)
        vocalProcessor.params.hpfFreq.store(hpfFreqParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (hpfSlopeParam != nullptr)
        vocalProcessor.params.hpfSlope.store(static_cast<int>(hpfSlopeParam->load(std::memory_order_relaxed)), std::memory_order_relaxed);

    // 3. Noise Gate
    if (gateBypassParam != nullptr)
        vocalProcessor.params.gateBypassed.store(gateBypassParam->load(std::memory_order_relaxed) >= 0.5f, std::memory_order_relaxed);
    if (gateThresholdParam != nullptr)
    {
        const float openThresh = gateThresholdParam->load(std::memory_order_relaxed);
        vocalProcessor.params.gateOpenThresholdDb.store(openThresh, std::memory_order_relaxed);
        vocalProcessor.params.gateCloseThresholdDb.store(openThresh - 6.0f, std::memory_order_relaxed);
    }
    if (gateAttackParam != nullptr)
        vocalProcessor.params.gateAttackMs.store(gateAttackParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (gateHoldParam != nullptr)
        vocalProcessor.params.gateHoldMs.store(gateHoldParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (gateReleaseParam != nullptr)
        vocalProcessor.params.gateReleaseMs.store(gateReleaseParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (gateFloorParam != nullptr)
        vocalProcessor.params.gateFloorDb.store(gateFloorParam->load(std::memory_order_relaxed), std::memory_order_relaxed);

    // 4. 8-Band Parametric EQ
    auto& eq = vocalProcessor.getEQ();
    if (eqBypassParam != nullptr)
    {
        const bool eqBypass = eqBypassParam->load(std::memory_order_relaxed) >= 0.5f;
        vocalProcessor.params.eqBypassed.store(eqBypass, std::memory_order_relaxed);
        eq.params.masterEnabled.store(!eqBypass, std::memory_order_relaxed);
    }
    for (int b = 0; b < LiveBus::numEQBands; ++b)
    {
        auto& bandParams = eqBandParams[(size_t)b];
        auto& eqBand = eq.params.bands[(size_t)b];
        if (bandParams.freq != nullptr)
            eqBand.freq.store(bandParams.freq->load(std::memory_order_relaxed), std::memory_order_relaxed);
        if (bandParams.gain != nullptr)
            eqBand.gainDb.store(bandParams.gain->load(std::memory_order_relaxed), std::memory_order_relaxed);
        if (bandParams.q != nullptr)
            eqBand.q.store(bandParams.q->load(std::memory_order_relaxed), std::memory_order_relaxed);
        if (bandParams.type != nullptr)
            eqBand.type.store(static_cast<LiveBus::EQFilterType>(static_cast<int>(bandParams.type->load(std::memory_order_relaxed))), std::memory_order_relaxed);
        if (bandParams.enabled != nullptr)
            eqBand.enabled.store(bandParams.enabled->load(std::memory_order_relaxed) >= 0.5f, std::memory_order_relaxed);
    }

    // 5. De-Esser
    if (deEsserBypassParam != nullptr)
        vocalProcessor.params.deEsserBypassed.store(deEsserBypassParam->load(std::memory_order_relaxed) >= 0.5f, std::memory_order_relaxed);
    if (deEsserThresholdParam != nullptr)
        vocalProcessor.params.deEsserThresholdDb.store(deEsserThresholdParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (deEsserFreqParam != nullptr)
        vocalProcessor.params.deEsserFreq.store(deEsserFreqParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (deEsserRangeParam != nullptr)
        vocalProcessor.params.deEsserRangeDb.store(deEsserRangeParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (deEsserTypeParam != nullptr)
        vocalProcessor.params.deEsserType.store(static_cast<LiveBus::VocalDSPParameters::DeEsserType>(static_cast<int>(deEsserTypeParam->load(std::memory_order_relaxed))), std::memory_order_relaxed);

    // 6. Compressor
    if (compBypassParam != nullptr)
        vocalProcessor.params.compBypassed.store(compBypassParam->load(std::memory_order_relaxed) >= 0.5f, std::memory_order_relaxed);
    if (compThresholdParam != nullptr)
        vocalProcessor.params.compThresholdDb.store(compThresholdParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (compRatioParam != nullptr)
        vocalProcessor.params.compRatio.store(compRatioParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (compAttackParam != nullptr)
        vocalProcessor.params.compAttackMs.store(compAttackParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (compReleaseParam != nullptr)
        vocalProcessor.params.compReleaseMs.store(compReleaseParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (compKneeParam != nullptr)
        vocalProcessor.params.compKnee.store(static_cast<LiveBus::VocalDSPParameters::KneeType>(static_cast<int>(compKneeParam->load(std::memory_order_relaxed))), std::memory_order_relaxed);
    if (compMakeupParam != nullptr)
        vocalProcessor.params.compMakeupDb.store(compMakeupParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (compMixParam != nullptr)
        vocalProcessor.params.compMixPercent.store(compMixParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (compAutoReleaseParam != nullptr)
        vocalProcessor.params.compReleaseMode.store(compAutoReleaseParam->load(std::memory_order_relaxed) >= 0.5f ? LiveBus::VocalDSPParameters::ReleaseAuto : LiveBus::VocalDSPParameters::ReleaseManual, std::memory_order_relaxed);

    // 7. Vocal Enhancement
    if (enhanceBypassParam != nullptr)
        enhancer.setBypass(enhanceBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (warmthParam != nullptr)
        enhancer.setWarmth(warmthParam->load(std::memory_order_relaxed));
    if (bodyParam != nullptr)
        enhancer.setBody(bodyParam->load(std::memory_order_relaxed));
    if (clarityParam != nullptr)
        enhancer.setClarity(clarityParam->load(std::memory_order_relaxed));
    if (presenceParam != nullptr)
        enhancer.setPresence(presenceParam->load(std::memory_order_relaxed));
    if (airParam != nullptr)
        enhancer.setAir(airParam->load(std::memory_order_relaxed));

    // 8. Limiter & Output Gain
    if (limiterBypassParam != nullptr)
        vocalProcessor.params.limiterBypassed.store(limiterBypassParam->load(std::memory_order_relaxed) >= 0.5f, std::memory_order_relaxed);
    if (limiterCeilingParam != nullptr)
        vocalProcessor.params.limiterCeilingDb.store(limiterCeilingParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (limiterReleaseParam != nullptr)
        vocalProcessor.params.limiterReleaseMs.store(limiterReleaseParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (outputGainParam != nullptr)
        vocalProcessor.params.outputGainDb.store(outputGainParam->load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void RNNoiseVST3Plugin::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    releaseResources();
    currentSampleRate = sampleRate;
    maximumBlockSize = juce::jmax(0, maximumExpectedSamplesPerBlock);

    // Prepare VocalDSPProcessor and VocalEnhancerDSP
    vocalProcessor.prepare(sampleRate, maximumBlockSize);
    enhancer.prepare(sampleRate, maximumBlockSize);

    // RNNoise requires exactly 48 kHz
    if (std::abs(sampleRate - LiveBus::RNNoiseNoiseSuppressor::RequiredSampleRate) < 0.5 &&
        maximumBlockSize > 0 &&
        maximumBlockSize <= static_cast<int>(LiveBus::AsyncAudioWorker::MaxSamples))
    {
        workerStarted.store(worker->start(), std::memory_order_release);
    }

    updateLatency();
}

void RNNoiseVST3Plugin::releaseResources()
{
    if (worker != nullptr)
        worker->stop();
    workerStarted.store(false, std::memory_order_release);
    vocalProcessor.reset();
    enhancer.reset();
}

bool RNNoiseVST3Plugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output &&
        (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void RNNoiseVST3Plugin::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto channelCount = buffer.getNumChannels();
    const auto sampleCount = buffer.getNumSamples();
    if (channelCount <= 0 || channelCount > static_cast<int>(LiveBus::AsyncAudioWorker::MaxChannels) ||
        sampleCount <= 0 || sampleCount > static_cast<int>(LiveBus::AsyncAudioWorker::MaxSamples))
        return;

    // Synchronize all parameter changes lock-free for real-time safety
    syncParametersFromTreeState();

    // Check Master Bypass
    if (masterBypassParam != nullptr && masterBypassParam->load(std::memory_order_relaxed) >= 0.5f)
        return;

    // Stage 1: RNNoise AI Neural Noise Suppression (if 48 kHz and worker active)
    const bool rnnoiseBypassed = (rnnoiseBypassParam != nullptr && rnnoiseBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (!rnnoiseBypassed && workerStarted.load(std::memory_order_acquire) &&
        std::abs(currentSampleRate.load(std::memory_order_acquire) - LiveBus::RNNoiseNoiseSuppressor::RequiredSampleRate) < 0.5)
    {
        std::array<const float*, LiveBus::AsyncAudioWorker::MaxChannels> inputs{};
        std::array<float*, LiveBus::AsyncAudioWorker::MaxChannels> outputs{};
        for (int channel = 0; channel < channelCount; ++channel)
        {
            inputs[static_cast<size_t>(channel)] = buffer.getReadPointer(channel);
            outputs[static_cast<size_t>(channel)] = buffer.getWritePointer(channel);
        }

        worker->processBlock(inputs.data(), outputs.data(), static_cast<size_t>(channelCount),
                             static_cast<size_t>(sampleCount), true);
    }

    // Stage 2: Vocal DSP Processor (Input Gain -> HPF -> Noise Gate -> 8-Band EQ -> De-Esser -> Compressor -> Limiter)
    vocalProcessor.process(buffer, sampleCount);

    // Stage 3: Vocal Enhancer DSP (Clarity, Presence, Warmth, Body, Air)
    enhancer.process(buffer, sampleCount);
}

juce::AudioProcessorEditor* RNNoiseVST3Plugin::createEditor()
{
    return new LiveBus::StudioVocalPluginEditor(*this);
}

int RNNoiseVST3Plugin::getNumPrograms()
{
    return LiveBus::VocalStudioPresetManager::NumFactoryPresets;
}

int RNNoiseVST3Plugin::getCurrentProgram()
{
    return currentProgramIndex.load(std::memory_order_relaxed);
}

void RNNoiseVST3Plugin::setCurrentProgram(int index)
{
    if (index >= 0 && index < LiveBus::VocalStudioPresetManager::NumFactoryPresets)
    {
        currentProgramIndex.store(index, std::memory_order_relaxed);
        loadFactoryPreset(index);
    }
}

const juce::String RNNoiseVST3Plugin::getProgramName(int index)
{
    if (index >= 0 && index < LiveBus::VocalStudioPresetManager::NumFactoryPresets)
        return LiveBus::VocalStudioPresetManager::getFactoryPreset(index).name;
    return {};
}

void RNNoiseVST3Plugin::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void RNNoiseVST3Plugin::loadFactoryPreset(int index)
{
    if (index >= 0 && index < LiveBus::VocalStudioPresetManager::NumFactoryPresets)
    {
        const auto& preset = LiveBus::VocalStudioPresetManager::getFactoryPreset(index);
        LiveBus::VocalStudioPresetManager::applyPreset(parameters, preset);
        currentProgramIndex.store(index, std::memory_order_relaxed);
    }
}

juce::String RNNoiseVST3Plugin::exportPresetXml(const juce::String& presetName)
{
    return LiveBus::VocalStudioPresetManager::exportPresetToXmlString(parameters, presetName);
}

bool RNNoiseVST3Plugin::importPresetXml(const juce::String& xmlString, juce::String& loadedName)
{
    return LiveBus::VocalStudioPresetManager::importPresetFromXmlString(parameters, xmlString, loadedName);
}

void RNNoiseVST3Plugin::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (auto xml = parameters.copyState().createXml())
    {
        xml->setAttribute("currentProgram", currentProgramIndex.load(std::memory_order_relaxed));
        xml->setAttribute("pluginName", "LiveBus RNNoise Vocal Studio");
        copyXmlToBinary(*xml, destinationData);
    }
}

void RNNoiseVST3Plugin::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasAttribute("currentProgram"))
            currentProgramIndex.store(xml->getIntAttribute("currentProgram", 0), std::memory_order_relaxed);

        const auto state = juce::ValueTree::fromXml(*xml);
        if (state.hasType(parameters.state.getType()))
            parameters.replaceState(state);
    }
}

void RNNoiseVST3Plugin::updateLatency()
{
    const bool supported = workerStarted.load(std::memory_order_acquire) && maximumBlockSize > 0 &&
        std::abs(currentSampleRate.load(std::memory_order_acquire) -
                 LiveBus::RNNoiseNoiseSuppressor::RequiredSampleRate) < 0.5;
    setLatencySamples(supported
        ? static_cast<int>(LiveBus::AsyncAudioWorker::LatencyBlocks) * maximumBlockSize +
              static_cast<int>(suppressor.getLatencySamples())
        : 0);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RNNoiseVST3Plugin();
}
