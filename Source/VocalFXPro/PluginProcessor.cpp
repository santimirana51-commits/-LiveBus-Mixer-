#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createParameters()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APA = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto addF = [&l](const char* id, const char* name, float lo, float hi, float def, float step = 0.0f) {
        l.add(std::make_unique<APF>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>(lo, hi, step), def));
    };
    auto addB = [&l](const char* id, const char* name, bool def) {
        l.add(std::make_unique<APB>(juce::ParameterID{id, 1}, name, def));
    };
    addF("input", "Input", -24.f, 24.f, 0.f);
    addF("output", "Output", -24.f, 24.f, 0.f);
    addF("mix", "Dry Wet", 0.f, 1.f, 1.f);
    addF("retune", "Retune Speed", 0.f, 1.f, 0.5f);
    addF("human", "Humanize", 0.f, 1.f, 0.25f);
    addB("autotune_on", "AutoTune Enable", true);
    l.add(std::make_unique<APA>(juce::ParameterID{"tune_mode", 1}, "Tune Mode",
        juce::StringArray{"Natural", "Hard Tune"}, 0));
    l.add(std::make_unique<APA>(juce::ParameterID{"key", 1}, "Key",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
    l.add(std::make_unique<APA>(juce::ParameterID{"scale", 1}, "Scale",
        juce::StringArray{"Major", "Minor", "Chromatic", "Harmonic Minor", "Melodic Minor"}, 0));
    addB("reverb_on", "Reverb Enable", true);
    addF("room", "Room Size", 0.f, 1.f, 0.35f);
    addF("decay", "Decay", 0.1f, 0.95f, 0.55f);
    addF("reverbMix", "Reverb Mix", 0.f, 1.f, 0.18f);
    addF("damping", "Damping", 0.f, 1.f, 0.45f);
    addB("delay_on", "Delay Enable", true);
    addF("delayTime", "Delay Time", 10.f, 1000.f, 280.f);
    addF("feedback", "Feedback", 0.f, 0.88f, 0.25f);
    addF("width", "Stereo Width", 0.f, 1.f, 0.75f);
    addF("delayMix", "Delay Mix", 0.f, 1.f, 0.12f);
    addB("eq_on", "EQ Enable", true);
    addF("low", "Low", -12.f, 12.f, 0.f);
    addF("mid", "Mid", -12.f, 12.f, 0.f);
    addF("high", "High", -12.f, 12.f, 0.f);
    addB("comp_on", "Comp Enable", true);
    addF("threshold", "Comp Threshold", -48.f, 0.f, -18.f);
    addF("ratio", "Comp Ratio", 1.f, 12.f, 3.f);
    addF("attack", "Comp Attack", 1.f, 100.f, 12.f);
    addF("release", "Comp Release", 10.f, 500.f, 110.f);
    addB("deess_on", "De-Esser Enable", true);
    addF("deess", "De-esser", 0.f, 1.f, 0.2f);
    addF("sat", "Saturation", 0.f, 1.f, 0.08f);
    addF("dist", "Distortion", 0.f, 1.f, 0.f);
    addF("robot", "Robot Voice", 0.f, 1.f, 0.f);
    addF("pitch", "Pitch Shift", -12.f, 12.f, 0.f);
    addF("drywet", "Global Mix", 0.f, 1.f, 1.f);
    return l;
}

VocalFXProAudioProcessor::VocalFXProAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  parameters(*this, nullptr, "PARAMETERS", createParameters())
{
}

bool VocalFXProAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void VocalFXProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    spec = { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() };
    lowFilter.prepare(spec); midFilter.prepare(spec); highFilter.prepare(spec);
    deessSidechainFilter.prepare(spec);
    compressor.prepare(spec); delay.prepare(spec); delay.setMaximumDelayInSamples((int)sampleRate);
    dryBuffer.setSize((int)getTotalNumOutputChannels(), samplesPerBlock);
    detectorBuffer.setSize((int)getTotalNumOutputChannels(), samplesPerBlock);
    reverb.prepare(spec);
    pitchDetector.prepare(sampleRate, samplesPerBlock);
    pitchCorrection.prepare(sampleRate, samplesPerBlock);

    // Prepare de-esser sidechain bandpass filter around 6.5 kHz (sibilance center)
    *deessSidechainFilter.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, 6500.0f, 2.0f);

    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = p("room")->load(); rp.damping = p("damping")->load();
    rp.wetLevel = p("reverbMix")->load(); rp.dryLevel = 1.0f - rp.wetLevel;
    rp.width = p("width")->load(); rp.freezeMode = 0.0f;
    reverb.setParameters(rp);
    compressor.setThreshold(p("threshold")->load());
    compressor.setRatio(p("ratio")->load());
    compressor.setAttack(p("attack")->load());
    compressor.setRelease(p("release")->load());
    for (int ch=0; ch<getTotalNumOutputChannels(); ++ch) delay.reset();
}

void VocalFXProAudioProcessor::releaseResources() {}

void VocalFXProAudioProcessor::updateFilters()
{
    *lowFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 160.0f, 0.707f, juce::Decibels::decibelsToGain(p("low")->load()));
    *midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 1200.0f, 0.9f, juce::Decibels::decibelsToGain(p("mid")->load()));
    *highFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 6500.0f, 0.707f, juce::Decibels::decibelsToGain(p("high")->load()));
}

void VocalFXProAudioProcessor::applyPitchAndRobot(juce::AudioBuffer<float>& b, int n)
{
    // Musical metallic ring-mod robot voice effect (carrier ~ 55 Hz fundamental buzz)
    const float robot = juce::jlimit(0.0f, 1.0f, p("robot")->load());
    if (robot <= 0.001f) return;

    const float carrierFreq = 65.41f; // C2 musical robot drone
    const float phaseInc = juce::MathConstants<float>::twoPi * carrierFreq / (float)sr;

    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* x = b.getWritePointer(ch);
        float phase = ch == 0 ? pitchPhaseL : pitchPhaseR;

        for (int i = 0; i < n; ++i)
        {
            // Rich harmonic carrier: fundamental + 2nd harmonic
            const float carrier = 0.7f * std::sin(phase) + 0.3f * std::sin(2.0f * phase);
            const float wet = x[i] * carrier * 1.5f;
            x[i] = x[i] * (1.0f - robot) + wet * robot;

            phase += phaseInc;
            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }

        if (ch == 0) pitchPhaseL = phase;
        else pitchPhaseR = phase;
    }
}

void VocalFXProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), channels = buffer.getNumChannels();
    if (dryBuffer.getNumSamples() < n) return; // no allocation in audio thread
    dryBuffer.makeCopyOf(buffer, true);
    buffer.applyGain(juce::Decibels::decibelsToGain(p("input")->load()));
    float inMax = 0.0f;
    for (int ch=0; ch<channels; ++ch)
        inMax = juce::jmax(inMax, buffer.getMagnitude(ch, 0, n));
    inputPeak.store(inMax);

    const bool eqEnabled = p("eq_on")->load() > 0.5f;
    if (eqEnabled)
    {
        updateFilters();
        juce::dsp::AudioBlock<float> block(buffer);
        auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
        lowFilter.process(ctx); midFilter.process(ctx); highFilter.process(ctx);
    }

    // Broadband compressor
    const bool compEnabled = p("comp_on")->load() > 0.5f;
    if (compEnabled)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
        compressor.setThreshold(p("threshold")->load());
        compressor.setRatio(p("ratio")->load());
        compressor.setAttack(p("attack")->load());
        compressor.setRelease(p("release")->load());
        compressor.process(ctx);
    }

    // De-esser approximation: dynamic attenuation of a high-frequency band when sibilance rises.
    // Sibilance-targeted De-Esser: sidechain bandpass (6.5 kHz) detection
    const bool deessEnabled = p("deess_on")->load() > 0.5f;
    const float deess = deessEnabled ? p("deess")->load() : 0.0f;
    if (deess > 0.001f && detectorBuffer.getNumSamples() >= n)
    {
        for (int ch = 0; ch < channels; ++ch)
            detectorBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

        juce::dsp::AudioBlock<float> sideBlock(detectorBuffer);
        auto sideCtx = juce::dsp::ProcessContextReplacing<float>(sideBlock);
        deessSidechainFilter.process(sideCtx);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* x = buffer.getWritePointer(ch);
            const auto* sibilance = detectorBuffer.getReadPointer(ch);

            for (int i = 0; i < n; ++i)
            {
                float sibLevel = std::abs(sibilance[i]);
                env = 0.995f * env + 0.005f * sibLevel;

                // When concentrated high-frequency sibilance exceeds threshold, attenuate
                if (sibLevel > env * 1.6f && sibLevel > 0.015f)
                {
                    float reduction = juce::jlimit(0.2f, 1.0f, 1.0f - (deess * 0.7f));
                    x[i] *= reduction;
                }
            }
        }
    }

    const float sat = p("sat")->load(), dist = p("dist")->load();
    if (sat > 0.001f || dist > 0.001f)
    {
        const float drive = 1.0f + sat * 5.0f + dist * 18.0f;
        for (int ch=0; ch<channels; ++ch) {
            auto* x = buffer.getWritePointer(ch);
            for (int i=0; i<n; ++i) {
                const float shaped = std::tanh(x[i] * drive);
                x[i] = juce::jmap(juce::jlimit(0.0f, 1.0f, sat + dist), x[i], shaped);
            }
        }
    }

    const bool autoTuneEnabled = p("autotune_on")->load() > 0.5f;
    if (autoTuneEnabled)
    {
        // 1. Monophonic Vocal Pitch Detection (Autocorrelation + NSDF)
        const float* monoPtr = buffer.getReadPointer(0);
        float detectedHz = pitchDetector.processBlock(monoPtr, n);
        currentDetectedHz.store(detectedHz);

        // 2. Scale & Key Quantization + Pitch Shift
        int rootKey = (int)p("key")->load();
        int scaleIdx = (int)p("scale")->load();
        auto scaleType = (VocalPitchCorrection::ScaleType)juce::jlimit(0, 4, scaleIdx);
        float retuneSpeed = p("retune")->load();
        float humanize = p("human")->load();
        float manualShift = p("pitch")->load();
        bool isHardTune = ((int)p("tune_mode")->load()) == 1;

        float targetHz = detectedHz;
        pitchCorrection.process(buffer, n, detectedHz, rootKey, scaleType, retuneSpeed, isHardTune, humanize, manualShift, &targetHz);
        currentTargetHz.store(targetHz);
    }
    else
    {
        // Even if AutoTune is bypassed, if manual pitch shift is set, apply pitch shift
        float manualShift = p("pitch")->load();
        if (std::abs(manualShift) > 0.05f)
        {
            float targetHz = 0.0f;
            pitchCorrection.process(buffer, n, 0.0f, 0, VocalPitchCorrection::ScaleChromatic, 1.0f, true, 0.0f, manualShift, &targetHz);
        }
        currentDetectedHz.store(0.0f);
        currentTargetHz.store(0.0f);
    }

    // Optional Robot Voice Modulation stage
    const float robot = p("robot")->load();
    if (robot > 0.001f)
        applyPitchAndRobot(buffer, n);

    // Delay line with feedback
    const bool delayEnabled = p("delay_on")->load() > 0.5f;
    if (delayEnabled)
    {
        const float dms = p("delayTime")->load();
        const int dSamp = juce::jlimit(1, (int)sr - 1, (int)(dms * 0.001 * sr));
        const float fb = p("feedback")->load(), dmix = p("delayMix")->load();
        for (int i=0; i<n; ++i) {
            for (int ch=0; ch<channels; ++ch) {
                const float in = buffer.getSample(ch, i);
                delay.setDelay((float)juce::jlimit(1, (int)sr - 1, dSamp + (ch == 1 ? (int)(p("width")->load()*0.012f*sr) : 0)));
                const float delayed = delay.popSample(ch);
                delay.pushSample(ch, in + delayed * fb);
                buffer.setSample(ch, i, in * (1.0f - dmix) + delayed * dmix);
            }
        }
    }

    const bool reverbEnabled = p("reverb_on")->load() > 0.5f;
    if (reverbEnabled)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = p("room")->load(); rp.damping = p("damping")->load();
        rp.wetLevel = p("reverbMix")->load(); rp.dryLevel = 1.0f - rp.wetLevel;
        rp.width = p("width")->load(); rp.freezeMode = 0.0f;
        reverb.setParameters(rp);
        reverb.process(ctx);
    }

    const float mix = p("drywet")->load();
    for (int ch=0; ch<channels; ++ch) {
        auto* x = buffer.getWritePointer(ch);
        const auto* dry = dryBuffer.getReadPointer(ch);
        for (int i=0; i<n; ++i) x[i] = dry[i] * (1.0f - mix) + x[i] * mix;
    }
    buffer.applyGain(juce::Decibels::decibelsToGain(p("output")->load()));
    float outMax = 0.0f;
    for (int ch=0; ch<channels; ++ch)
    {
        auto* x = buffer.getWritePointer(ch);
        for (int i=0; i<n; ++i)
        {
            if (std::isnan(x[i]) || std::isinf(x[i]))
                x[i] = 0.0f;
            else
                x[i] = juce::jlimit(-4.0f, 4.0f, x[i]); // Hard protection against blowout
        }
        outMax = juce::jmax(outMax, buffer.getMagnitude(ch, 0, n));
    }
    outputPeak.store(outMax);
}

juce::AudioProcessorEditor* VocalFXProAudioProcessor::createEditor() { return new VocalFXProAudioProcessorEditor(*this); }

void VocalFXProAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}
void VocalFXProAudioProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

void VocalFXProAudioProcessor::setCurrentProgram(int index)
{
    currentProgram = juce::jlimit(0, 4, index);

    // Apply factory preset parameters to APVTS so host preset selection updates DSP
    switch (currentProgram)
    {
        case 0: // Clean Vocal
            if (auto* p = parameters.getParameter("autotune_on")) p->setValueNotifyingHost(0.0f);
            if (auto* p = parameters.getParameter("retune")) p->setValueNotifyingHost(0.3f);
            if (auto* p = parameters.getParameter("tune_mode")) p->setValueNotifyingHost(0.0f);
            if (auto* p = parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("threshold")) p->setValueNotifyingHost(parameters.getParameterRange("threshold").convertTo0to1(-16.0f));
            if (auto* p = parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.12f);
            if (auto* p = parameters.getParameter("delay_on")) p->setValueNotifyingHost(0.0f);
            if (auto* p = parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            break;

        case 1: // Pop Vocal
            if (auto* p = parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("retune")) p->setValueNotifyingHost(0.65f);
            if (auto* p = parameters.getParameter("tune_mode")) p->setValueNotifyingHost(0.0f);
            if (auto* p = parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("threshold")) p->setValueNotifyingHost(parameters.getParameterRange("threshold").convertTo0to1(-20.0f));
            if (auto* p = parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.22f);
            if (auto* p = parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.15f);
            if (auto* p = parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            break;

        case 2: // Rap Vocal
            if (auto* p = parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("retune")) p->setValueNotifyingHost(0.85f);
            if (auto* p = parameters.getParameter("tune_mode")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("threshold")) p->setValueNotifyingHost(parameters.getParameterRange("threshold").convertTo0to1(-24.0f));
            if (auto* p = parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.10f);
            if (auto* p = parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.10f);
            if (auto* p = parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            break;

        case 3: // AutoTune
            if (auto* p = parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("retune")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("tune_mode")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("threshold")) p->setValueNotifyingHost(parameters.getParameterRange("threshold").convertTo0to1(-18.0f));
            if (auto* p = parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.18f);
            if (auto* p = parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.12f);
            if (auto* p = parameters.getParameter("robot")) p->setValueNotifyingHost(0.0f);
            break;

        case 4: // Robot Voice
            if (auto* p = parameters.getParameter("autotune_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("retune")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("tune_mode")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("comp_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("threshold")) p->setValueNotifyingHost(parameters.getParameterRange("threshold").convertTo0to1(-22.0f));
            if (auto* p = parameters.getParameter("reverb_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("reverbMix")) p->setValueNotifyingHost(0.25f);
            if (auto* p = parameters.getParameter("delay_on")) p->setValueNotifyingHost(1.0f);
            if (auto* p = parameters.getParameter("delayMix")) p->setValueNotifyingHost(0.20f);
            if (auto* p = parameters.getParameter("robot")) p->setValueNotifyingHost(0.85f);
            break;

        default:
            break;
    }
}
const juce::String VocalFXProAudioProcessor::getProgramName(int index)
{
    const char* names[] = {"Clean Vocal", "Pop Vocal", "Rap Vocal", "AutoTune", "Robot Voice"};
    return names[juce::jlimit(0, 4, index)];
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalFXProAudioProcessor(); }
