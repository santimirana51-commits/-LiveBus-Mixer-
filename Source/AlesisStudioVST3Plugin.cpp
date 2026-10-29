#include "AlesisStudioVST3Plugin.h"
#include "AlesisStudioPluginEditor.h"

namespace LiveBus
{

AlesisStudioVST3Plugin::AlesisStudioVST3Plugin()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AlesisStudioParameters", createParameterLayout())
{
    masterBypassParam = parameters.getRawParameterValue("master_bypass");

    // 1. Reverb
    verbBypassParam = parameters.getRawParameterValue("verb_bypass");
    verbTypeParam = parameters.getRawParameterValue("verb_type");
    verbDecayParam = parameters.getRawParameterValue("verb_decay");
    verbSizeParam = parameters.getRawParameterValue("verb_size");
    verbDampParam = parameters.getRawParameterValue("verb_damping");
    verbPreDelayParam = parameters.getRawParameterValue("verb_predelay");
    verbWetParam = parameters.getRawParameterValue("verb_wet");
    verbLowCutParam = parameters.getRawParameterValue("verb_lowcut");
    verbHighCutParam = parameters.getRawParameterValue("verb_highcut");

    // 2. Delay
    delayBypassParam = parameters.getRawParameterValue("delay_bypass");
    delayModeParam = parameters.getRawParameterValue("delay_mode");
    delayTimeParam = parameters.getRawParameterValue("delay_time");
    delayFeedbackParam = parameters.getRawParameterValue("delay_feedback");
    delayWetParam = parameters.getRawParameterValue("delay_wet");
    delayFilterParam = parameters.getRawParameterValue("delay_filter");

    // 3. EQ
    eqBypassParam = parameters.getRawParameterValue("eq_bypass");
    eqHpParam = parameters.getRawParameterValue("eq_hp");
    eqLowShelfParam = parameters.getRawParameterValue("eq_lowshelf");
    eqLowMidParam = parameters.getRawParameterValue("eq_lowmid");
    eqMidParam = parameters.getRawParameterValue("eq_mid");
    eqHighMidParam = parameters.getRawParameterValue("eq_highmid");
    eqHighShelfParam = parameters.getRawParameterValue("eq_highshelf");
    eqLpParam = parameters.getRawParameterValue("eq_lp");

    // 4. Compressor
    compBypassParam = parameters.getRawParameterValue("comp_bypass");
    compThreshParam = parameters.getRawParameterValue("comp_threshold");
    compRatioParam = parameters.getRawParameterValue("comp_ratio");
    compAttackParam = parameters.getRawParameterValue("comp_attack");
    compReleaseParam = parameters.getRawParameterValue("comp_release");
    compHardKneeParam = parameters.getRawParameterValue("comp_hardknee");
    compMakeupParam = parameters.getRawParameterValue("comp_makeup");
    compAutoGainParam = parameters.getRawParameterValue("comp_autogain");
    compMixParam = parameters.getRawParameterValue("comp_mix");

    // 5. Vocal Enhancements
    deessBypassParam = parameters.getRawParameterValue("deess_bypass");
    deessAmountParam = parameters.getRawParameterValue("deess_amount");
    gateBypassParam = parameters.getRawParameterValue("gate_bypass");
    gateThreshParam = parameters.getRawParameterValue("gate_thresh");
    satAmountParam = parameters.getRawParameterValue("sat_amount");
    exciterAmountParam = parameters.getRawParameterValue("exciter_amount");
    stereoWidthParam = parameters.getRawParameterValue("stereo_width");

    // 6. Creative FX
    creativeModeParam = parameters.getRawParameterValue("creative_mode");
    creativeMixParam = parameters.getRawParameterValue("creative_mix");

    // 7. AutoTune
    atBypassParam = parameters.getRawParameterValue("at_bypass");
    atAmountParam = parameters.getRawParameterValue("at_amount");
    atSpeedParam = parameters.getRawParameterValue("at_speed");
    atKeyParam = parameters.getRawParameterValue("at_key");
    atScaleParam = parameters.getRawParameterValue("at_scale");
}

AlesisStudioVST3Plugin::~AlesisStudioVST3Plugin()
{
    releaseResources();
}

juce::AudioProcessorValueTreeState::ParameterLayout AlesisStudioVST3Plugin::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Master Bypass
    l.add(std::make_unique<APB>(juce::ParameterID{"master_bypass", 1}, "Master Bypass", false));

    // --- 1. Reverb Module ---
    l.add(std::make_unique<APB>(juce::ParameterID{"verb_bypass", 1}, "Reverb Bypass", false));
    l.add(std::make_unique<APC>(juce::ParameterID{"verb_type", 1}, "Reverb Space",
        juce::StringArray{"Room", "Hall", "Plate", "Spring", "Studio", "Cathedral"}, 1));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_decay", 1}, "Reverb Decay",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 45.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_size", 1}, "Reverb Size",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 50.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_damping", 1}, "Reverb Damping",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 35.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_predelay", 1}, "Reverb Pre-Delay",
        juce::NormalisableRange<float>{0.0f, 300.0f, 1.0f}, 20.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_wet", 1}, "Reverb Wet Mix",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 18.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_lowcut", 1}, "Reverb Low Cut",
        juce::NormalisableRange<float>{20.0f, 1000.0f, 1.0f, 0.4f}, 120.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"verb_highcut", 1}, "Reverb High Cut",
        juce::NormalisableRange<float>{1000.0f, 20000.0f, 10.0f, 0.4f}, 9500.0f));

    // --- 2. Delay & Echo Module ---
    l.add(std::make_unique<APB>(juce::ParameterID{"delay_bypass", 1}, "Delay Bypass", false));
    l.add(std::make_unique<APC>(juce::ParameterID{"delay_mode", 1}, "Delay Mode",
        juce::StringArray{"Mono", "Stereo", "Ping-Pong", "Slapback"}, 1));
    l.add(std::make_unique<APF>(juce::ParameterID{"delay_time", 1}, "Delay Time",
        juce::NormalisableRange<float>{10.0f, 1200.0f, 1.0f, 0.5f}, 280.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"delay_feedback", 1}, "Delay Feedback",
        juce::NormalisableRange<float>{0.0f, 90.0f, 1.0f}, 25.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"delay_wet", 1}, "Delay Wet Mix",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 15.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"delay_filter", 1}, "Delay Tone Filter",
        juce::NormalisableRange<float>{500.0f, 18000.0f, 10.0f, 0.4f}, 6000.0f));

    // --- 3. Parametric EQ Module ---
    l.add(std::make_unique<APB>(juce::ParameterID{"eq_bypass", 1}, "EQ Bypass", false));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_hp", 1}, "EQ High-Pass",
        juce::NormalisableRange<float>{20.0f, 400.0f, 1.0f, 0.5f}, 80.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_lowshelf", 1}, "EQ Low Shelf Gain",
        juce::NormalisableRange<float>{-15.0f, 15.0f, 0.1f}, 1.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_lowmid", 1}, "EQ Low-Mid Gain",
        juce::NormalisableRange<float>{-15.0f, 15.0f, 0.1f}, 0.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_mid", 1}, "EQ Mid Gain",
        juce::NormalisableRange<float>{-15.0f, 15.0f, 0.1f}, 2.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_highmid", 1}, "EQ High-Mid Gain",
        juce::NormalisableRange<float>{-15.0f, 15.0f, 0.1f}, 0.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_highshelf", 1}, "EQ High Shelf Gain",
        juce::NormalisableRange<float>{-15.0f, 15.0f, 0.1f}, 2.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"eq_lp", 1}, "EQ Low-Pass",
        juce::NormalisableRange<float>{3000.0f, 22000.0f, 10.0f, 0.4f}, 20000.0f));

    // --- 4. Alesis 3630 Style Compressor ---
    l.add(std::make_unique<APB>(juce::ParameterID{"comp_bypass", 1}, "Compressor Bypass", false));
    l.add(std::make_unique<APF>(juce::ParameterID{"comp_threshold", 1}, "Comp Threshold",
        juce::NormalisableRange<float>{-60.0f, 0.0f, 0.1f}, -18.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"comp_ratio", 1}, "Comp Ratio",
        juce::NormalisableRange<float>{1.0f, 20.0f, 0.1f}, 3.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"comp_attack", 1}, "Comp Attack",
        juce::NormalisableRange<float>{0.1f, 150.0f, 0.1f, 0.4f}, 15.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"comp_release", 1}, "Comp Release",
        juce::NormalisableRange<float>{10.0f, 1500.0f, 1.0f, 0.4f}, 150.0f));
    l.add(std::make_unique<APB>(juce::ParameterID{"comp_hardknee", 1}, "Hard Knee Mode", false));
    l.add(std::make_unique<APF>(juce::ParameterID{"comp_makeup", 1}, "Makeup Gain",
        juce::NormalisableRange<float>{-12.0f, 24.0f, 0.1f}, 2.0f));
    l.add(std::make_unique<APB>(juce::ParameterID{"comp_autogain", 1}, "Auto Gain", false));
    l.add(std::make_unique<APF>(juce::ParameterID{"comp_mix", 1}, "Comp Mix",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 100.0f));

    // --- 5. Vocal Dynamics & Enhancement ---
    l.add(std::make_unique<APB>(juce::ParameterID{"deess_bypass", 1}, "De-Esser Bypass", false));
    l.add(std::make_unique<APF>(juce::ParameterID{"deess_amount", 1}, "De-Esser Amount",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 25.0f));
    l.add(std::make_unique<APB>(juce::ParameterID{"gate_bypass", 1}, "Noise Gate Bypass", true));
    l.add(std::make_unique<APF>(juce::ParameterID{"gate_thresh", 1}, "Gate Threshold",
        juce::NormalisableRange<float>{-80.0f, -20.0f, 1.0f}, -55.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"sat_amount", 1}, "Saturation Drive",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 5.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"exciter_amount", 1}, "Vocal Exciter",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 0.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"stereo_width", 1}, "Stereo Width",
        juce::NormalisableRange<float>{0.0f, 200.0f, 1.0f}, 100.0f));

    // --- 6. Creative FX ---
    l.add(std::make_unique<APC>(juce::ParameterID{"creative_mode", 1}, "Creative Effect",
        juce::StringArray{"Off", "Chorus", "Flanger", "Phaser", "Doubler", "Telephone", "Radio", "Robot Voice"}, 0));
    l.add(std::make_unique<APF>(juce::ParameterID{"creative_mix", 1}, "Creative Mix",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 0.0f));

    // --- 7. AutoTune & Pitch Correction ---
    l.add(std::make_unique<APB>(juce::ParameterID{"at_bypass", 1}, "AutoTune Bypass", false));
    l.add(std::make_unique<APF>(juce::ParameterID{"at_amount", 1}, "AutoTune Amount",
        juce::NormalisableRange<float>{0.0f, 100.0f, 1.0f}, 35.0f));
    l.add(std::make_unique<APF>(juce::ParameterID{"at_speed", 1}, "Retune Speed",
        juce::NormalisableRange<float>{0.0f, 150.0f, 1.0f}, 20.0f)); // ms
    l.add(std::make_unique<APC>(juce::ParameterID{"at_key", 1}, "AutoTune Key",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
    l.add(std::make_unique<APC>(juce::ParameterID{"at_scale", 1}, "AutoTune Scale",
        juce::StringArray{"Major", "Minor", "Chromatic", "Harmonic Minor", "Melodic Minor"}, 0));

    return l;
}

void AlesisStudioVST3Plugin::syncParameters() noexcept
{
    // Reverb
    if (verbBypassParam) alesisDSP.setReverbBypass(verbBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (verbTypeParam) alesisDSP.setReverbType(static_cast<int>(verbTypeParam->load(std::memory_order_relaxed)));
    if (verbDecayParam) alesisDSP.setReverbDecay(verbDecayParam->load(std::memory_order_relaxed));
    if (verbSizeParam) alesisDSP.setReverbSize(verbSizeParam->load(std::memory_order_relaxed));
    if (verbDampParam) alesisDSP.setReverbDamping(verbDampParam->load(std::memory_order_relaxed));
    if (verbPreDelayParam) alesisDSP.setReverbPreDelay(verbPreDelayParam->load(std::memory_order_relaxed));
    if (verbWetParam) alesisDSP.setReverbWet(verbWetParam->load(std::memory_order_relaxed));
    if (verbLowCutParam) alesisDSP.setReverbLowCut(verbLowCutParam->load(std::memory_order_relaxed));
    if (verbHighCutParam) alesisDSP.setReverbHighCut(verbHighCutParam->load(std::memory_order_relaxed));

    // Delay
    if (delayBypassParam) alesisDSP.setDelayBypass(delayBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (delayModeParam) alesisDSP.setDelayMode(static_cast<int>(delayModeParam->load(std::memory_order_relaxed)));
    if (delayTimeParam) alesisDSP.setDelayTime(delayTimeParam->load(std::memory_order_relaxed));
    if (delayFeedbackParam) alesisDSP.setDelayFeedback(delayFeedbackParam->load(std::memory_order_relaxed));
    if (delayWetParam) alesisDSP.setDelayWet(delayWetParam->load(std::memory_order_relaxed));
    if (delayFilterParam) alesisDSP.setDelayFilterCutoff(delayFilterParam->load(std::memory_order_relaxed));

    // EQ
    if (eqBypassParam) alesisDSP.setEQBypass(eqBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (eqHpParam) alesisDSP.setEQHighPass(eqHpParam->load(std::memory_order_relaxed));
    if (eqLowShelfParam) alesisDSP.setEQLowShelf(eqLowShelfParam->load(std::memory_order_relaxed));
    if (eqLowMidParam) alesisDSP.setEQLowMid(eqLowMidParam->load(std::memory_order_relaxed));
    if (eqMidParam) alesisDSP.setEQMid(eqMidParam->load(std::memory_order_relaxed));
    if (eqHighMidParam) alesisDSP.setEQHighMid(eqHighMidParam->load(std::memory_order_relaxed));
    if (eqHighShelfParam) alesisDSP.setEQHighShelf(eqHighShelfParam->load(std::memory_order_relaxed));
    if (eqLpParam) alesisDSP.setEQLowPass(eqLpParam->load(std::memory_order_relaxed));

    // Compressor
    if (compBypassParam) alesisDSP.setCompBypass(compBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (compThreshParam) alesisDSP.setCompThreshold(compThreshParam->load(std::memory_order_relaxed));
    if (compRatioParam) alesisDSP.setCompRatio(compRatioParam->load(std::memory_order_relaxed));
    if (compAttackParam) alesisDSP.setCompAttackMs(compAttackParam->load(std::memory_order_relaxed));
    if (compReleaseParam) alesisDSP.setCompReleaseMs(compReleaseParam->load(std::memory_order_relaxed));
    if (compHardKneeParam) alesisDSP.setCompHardKnee(compHardKneeParam->load(std::memory_order_relaxed) >= 0.5f);
    if (compMakeupParam) alesisDSP.setCompMakeupGain(compMakeupParam->load(std::memory_order_relaxed));
    if (compAutoGainParam) alesisDSP.setCompAutoGain(compAutoGainParam->load(std::memory_order_relaxed) >= 0.5f);
    if (compMixParam) alesisDSP.setCompMix(compMixParam->load(std::memory_order_relaxed));

    // Vocal Enhancements
    if (deessBypassParam) alesisDSP.setDeEsserBypass(deessBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (deessAmountParam) alesisDSP.setDeEsserAmount(deessAmountParam->load(std::memory_order_relaxed));
    if (gateBypassParam) alesisDSP.setNoiseGateBypass(gateBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (gateThreshParam) alesisDSP.setGateThreshold(gateThreshParam->load(std::memory_order_relaxed));
    if (satAmountParam) alesisDSP.setSaturation(satAmountParam->load(std::memory_order_relaxed));
    if (exciterAmountParam) alesisDSP.setExciter(exciterAmountParam->load(std::memory_order_relaxed));
    if (stereoWidthParam) alesisDSP.setStereoWidth(stereoWidthParam->load(std::memory_order_relaxed));

    // Creative FX
    if (creativeModeParam) alesisDSP.setCreativeMode(static_cast<int>(creativeModeParam->load(std::memory_order_relaxed)));
    if (creativeMixParam) alesisDSP.setCreativeMix(creativeMixParam->load(std::memory_order_relaxed));

    // AutoTune
    if (atBypassParam) alesisDSP.setAutoTuneBypass(atBypassParam->load(std::memory_order_relaxed) >= 0.5f);
    if (atAmountParam) alesisDSP.setAutoTuneAmount(atAmountParam->load(std::memory_order_relaxed));
    if (atSpeedParam) alesisDSP.setAutoTuneRetuneSpeedMs(atSpeedParam->load(std::memory_order_relaxed));
    if (atKeyParam) alesisDSP.setAutoTuneKey(static_cast<int>(atKeyParam->load(std::memory_order_relaxed)));
    if (atScaleParam) alesisDSP.setAutoTuneScale(static_cast<int>(atScaleParam->load(std::memory_order_relaxed)));
}

void AlesisStudioVST3Plugin::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    alesisDSP.prepare(sampleRate, maximumExpectedSamplesPerBlock);
}

void AlesisStudioVST3Plugin::releaseResources()
{
    alesisDSP.reset();
}

bool AlesisStudioVST3Plugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output &&
        (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void AlesisStudioVST3Plugin::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto channelCount = buffer.getNumChannels();
    const auto sampleCount = buffer.getNumSamples();
    if (channelCount <= 0 || sampleCount <= 0)
        return;

    syncParameters();

    if (masterBypassParam != nullptr && masterBypassParam->load(std::memory_order_relaxed) >= 0.5f)
        return;

    alesisDSP.process(buffer, sampleCount);
}

juce::AudioProcessorEditor* AlesisStudioVST3Plugin::createEditor()
{
    return new AlesisStudioPluginEditor(*this);
}

void AlesisStudioVST3Plugin::setCurrentProgram(int index)
{
    currentProgram = juce::jlimit(0, 4, index);

    auto setParamVal = [this](const char* id, float val) {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(parameters.getParameterRange(id).convertTo0to1(val));
    };
    auto setChoiceVal = [this](const char* id, int idx) {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(p->getAllValueStrings().size() > 1 ? (float)idx / (float)(p->getAllValueStrings().size() - 1) : 0.0f);
    };
    auto setBoolVal = [this](const char* id, bool b) {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(b ? 1.0f : 0.0f);
    };

    switch (currentProgram)
    {
        case 0: // 1. DANGDUT KOPLO LIVE
            setBoolVal("at_bypass", false);
            setParamVal("at_amount", 35.0f);
            setParamVal("at_speed", 20.0f); // 20 ms
            setBoolVal("comp_bypass", false);
            setParamVal("comp_ratio", 3.0f);
            setBoolVal("verb_bypass", false);
            setChoiceVal("verb_type", 1); // Hall
            setParamVal("verb_wet", 18.0f);
            setBoolVal("delay_bypass", false);
            setChoiceVal("delay_mode", 1); // Stereo
            setParamVal("delay_wet", 15.0f);
            setBoolVal("eq_bypass", false);
            setParamVal("eq_lowshelf", 1.0f);
            setParamVal("eq_mid", 2.0f);
            setParamVal("eq_highshelf", 2.0f);
            setBoolVal("deess_bypass", false);
            setParamVal("deess_amount", 20.0f);
            setChoiceVal("creative_mode", 0);
            break;

        case 1: // 2. POP VOCAL PRO
            setBoolVal("at_bypass", false);
            setParamVal("at_amount", 45.0f);
            setParamVal("at_speed", 12.0f); // 12 ms
            setBoolVal("comp_bypass", false);
            setParamVal("comp_ratio", 3.0f);
            setBoolVal("verb_bypass", false);
            setChoiceVal("verb_type", 2); // Plate
            setParamVal("verb_wet", 12.0f);
            setBoolVal("delay_bypass", false);
            setChoiceVal("delay_mode", 1); // Stereo
            setParamVal("delay_wet", 10.0f);
            setBoolVal("deess_bypass", false);
            setParamVal("deess_amount", 30.0f);
            setBoolVal("eq_bypass", false);
            setParamVal("eq_hp", 80.0f);
            setParamVal("eq_mid", 1.0f);
            setParamVal("eq_highshelf", 2.5f);
            setChoiceVal("creative_mode", 0);
            break;

        case 2: // 3. STUDIO CLEAN
            setBoolVal("at_bypass", false);
            setParamVal("at_amount", 15.0f);
            setBoolVal("comp_bypass", false);
            setParamVal("comp_ratio", 2.0f);
            setBoolVal("verb_bypass", false);
            setChoiceVal("verb_type", 0); // Room
            setParamVal("verb_wet", 7.0f);
            setBoolVal("delay_bypass", true);
            setParamVal("delay_wet", 0.0f);
            setBoolVal("eq_bypass", false);
            setParamVal("eq_hp", 90.0f); // High-pass 80-100 Hz
            setBoolVal("deess_bypass", false);
            setParamVal("deess_amount", 15.0f); // De-Esser aktif ringan
            setChoiceVal("creative_mode", 0);
            break;

        case 3: // 4. HARD TUNE
            setBoolVal("at_bypass", false);
            setParamVal("at_amount", 100.0f);
            setParamVal("at_speed", 2.0f); // 0-5 ms instant snap
            setBoolVal("comp_bypass", false);
            setParamVal("comp_ratio", 4.0f);
            setBoolVal("verb_bypass", false);
            setChoiceVal("verb_type", 2); // Plate
            setParamVal("verb_wet", 10.0f);
            setBoolVal("delay_bypass", false);
            setChoiceVal("delay_mode", 2); // Ping-Pong
            setParamVal("delay_wet", 12.0f);
            setBoolVal("deess_bypass", false);
            setParamVal("deess_amount", 25.0f);
            setChoiceVal("creative_mode", 0);
            break;

        case 4: // 5. VOCAL MEGA FX
            setBoolVal("at_bypass", false);
            setParamVal("at_amount", 50.0f);
            setBoolVal("comp_bypass", false);
            setParamVal("comp_ratio", 3.0f);
            setBoolVal("verb_bypass", false);
            setChoiceVal("verb_type", 1); // Hall
            setParamVal("verb_wet", 25.0f);
            setBoolVal("delay_bypass", false);
            setChoiceVal("delay_mode", 2); // Ping-Pong
            setParamVal("delay_wet", 20.0f);
            setChoiceVal("creative_mode", 4); // Doubler Aktif
            setParamVal("creative_mix", 25.0f);
            setBoolVal("deess_bypass", false);
            setParamVal("deess_amount", 20.0f);
            break;

        default:
            break;
    }
}

const juce::String AlesisStudioVST3Plugin::getProgramName(int index)
{
    const char* names[] = {
        "DANGDUT KOPLO LIVE",
        "POP VOCAL PRO",
        "STUDIO CLEAN",
        "HARD TUNE",
        "VOCAL MEGA FX"
    };
    return names[juce::jlimit(0, 4, index)];
}

void AlesisStudioVST3Plugin::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (auto xml = parameters.copyState().createXml())
    {
        xml->setAttribute("pluginName", "LiveBus Alesis Vintage Studio");
        copyXmlToBinary(*xml, destinationData);
    }
}

void AlesisStudioVST3Plugin::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml(*xml);
        if (state.hasType(parameters.state.getType()))
            parameters.replaceState(state);
    }
}

} // namespace LiveBus

#if !BUILD_LIVEBUS_COMBINED_HOST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LiveBus::AlesisStudioVST3Plugin();
}
#endif
