#include "PluginProcessor.h"
#include "PluginEditor.h"

RisaccaAudioProcessor::RisaccaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < pid::numMappable; ++i)
    {
        params[(size_t) i] = apvts.getParameter (pid::all[(size_t) i].id);
        raw[(size_t) i] = apvts.getRawParameterValue (pid::all[(size_t) i].id);
    }
    rawQuality = apvts.getRawParameterValue (pid::qualityId);
    rawSeed = apvts.getRawParameterValue (pid::seedId);
    apvts.addParameterListener (pid::qualityId, this);
    apvts.addParameterListener (pid::seedId, this);
}

RisaccaAudioProcessor::~RisaccaAudioProcessor()
{
    apvts.removeParameterListener (pid::qualityId, this);
    apvts.removeParameterListener (pid::seedId, this);
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout RisaccaAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : pid::all)
    {
        const juce::ParameterID id { p.id, 1 };
        const juce::String sid (p.id);
        switch (p.kind)
        {
            case 0:
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, p.name,
                    juce::NormalisableRange<float> (0.0f, 1.0f), p.defaultValue,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return juce::String (v * 10.0f, 1); })));
                break;
            case 1:
            {
                auto attr = juce::AudioParameterBoolAttributes();
                if (sid == "tame")
                    attr = attr.withStringFromValueFunction ([] (bool v, int) { return juce::String (v ? "Tame" : "Trouble"); });
                else if (sid == "engaged")
                    attr = attr.withStringFromValueFunction ([] (bool v, int) { return juce::String (v ? "On" : "Bypass"); });
                layout.add (std::make_unique<juce::AudioParameterBool> (id, p.name, p.defaultValue > 0.5f, attr));
                break;
            }
            case 2:
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, p.name,
                    juce::NormalisableRange<float> (p.minValue, p.maxValue), p.defaultValue,
                    juce::AudioParameterFloatAttributes().withLabel ("V").withStringFromValueFunction (
                        [] (float v, int) { return juce::String (v, 2) + " V"; })));
                break;
            case 3:
                if (sid == "modTone")
                    layout.add (std::make_unique<juce::AudioParameterChoice> (id, p.name,
                        juce::StringArray { "Standard (Bass/Treble)", "Eurorack, C5 100n", "Eurorack, C5 10n" }, 0));
                else
                    layout.add (std::make_unique<juce::AudioParameterChoice> (id, p.name,
                        juce::StringArray { "Linea", "Chitarra", "Scollegato" }, (int) p.defaultValue));
                break;
            default:
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, p.name,
                    juce::NormalisableRange<float> (p.minValue, p.maxValue, 0.1f), p.defaultValue,
                    juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
                        [] (float v, int) { return juce::String (v, 1) + " dB"; })));
                break;
        }
    }
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid::qualityId, 1 }, "Quality",
        juce::StringArray { "Standard (4x)", "Alta (8x)" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { pid::seedId, 1 }, "Unit", 1, 999, 1,
        juce::AudioParameterIntAttributes().withAutomatable (false)));
    return layout;
}

bool RisaccaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return true;
}

risacca::SwashControls RisaccaAudioProcessor::readControls() const
{
    auto v = [this] (int i) { return (double) raw[(size_t) i]->load(); };
    auto b = [this] (int i) { return raw[(size_t) i]->load() > 0.5f; };
    risacca::SwashControls c;
    c.gate = v (pid::noisegate);    c.swash = v (pid::swash);      c.bass = v (pid::bass);
    c.clean = v (pid::clean);       c.pregain = v (pid::pregain);  c.treble = v (pid::treble);
    c.lfoSpeed = v (pid::lfoSpeed); c.selfOsc = v (pid::selfOsc);  c.low = v (pid::low);
    c.master = v (pid::volume);     c.postgain = v (pid::postgain); c.noiseVol = v (pid::noiseVol);
    c.preclip = b (pid::preclip);   c.postclip = b (pid::postclip); c.selfOscOn = b (pid::selfOscSw);
    c.tame = b (pid::tame);         c.lfoOn = b (pid::lfoSw);       c.engaged = b (pid::engaged);
    c.moodPatched = b (pid::moodPatched);
    c.moodV = v (pid::moodCV);      c.swashCV = v (pid::swashCV);
    c.modLedPreclip = b (pid::modLed);
    c.modTone = juce::jlimit (0, 2, juce::roundToInt (v (pid::modTone)));
    c.source = juce::jlimit (0, 2, juce::roundToInt (v (pid::source)));
    return c;
}

void RisaccaAudioProcessor::rebuild()
{
    // frequenza interna ~192 kHz (Standard) o ~384 kHz (Alta)
    const double target = (rawQuality->load() > 0.5f) ? 384000.0 : 192000.0;
    int f = 1;
    while (baseRate * f < target * 0.9 && f < 16) f *= 2;
    factor = f;
    const int seed = juce::jlimit (1, 999, juce::roundToInt (rawSeed->load()));

    int log2f = 0;
    while ((1 << log2f) < factor) ++log2f;
    oversampler.reset();
    if (log2f > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, (size_t) log2f, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) maxBlock);
        latency = (int) std::round (oversampler->getLatencyInSamples());
    }
    else latency = 0;
    setLatencySamples (latency);
    dryDelay.setSize (1, juce::jmax (1, latency + maxBlock + 1));
    dryDelay.clear();
    dryWrite = 0;

    engine.prepare (baseRate * factor, (uint32_t) seed);
    engine.setControls (readControls());
    engine.settle (0.3);
    preparedFactor = factor; preparedSeed = seed; preparedRate = baseRate;
    prepared = true;
}

void RisaccaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool blockGrew = samplesPerBlock > maxBlock;
    maxBlock = juce::jmax (maxBlock, samplesPerBlock);
    baseRate = sampleRate;
    mono.setSize (1, maxBlock);
    dryOut.setSize (1, maxBlock);
    const double target = (rawQuality->load() > 0.5f) ? 384000.0 : 192000.0;
    int f = 1;
    while (baseRate * f < target * 0.9 && f < 16) f *= 2;
    const int seed = juce::jlimit (1, 999, juce::roundToInt (rawSeed->load()));
    // il pedale resta "acceso" se nulla e' cambiato (niente nuovo transitorio d'accensione)
    if (! prepared || blockGrew || f != preparedFactor || seed != preparedSeed || sampleRate != preparedRate)
        rebuild();
}

void RisaccaAudioProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void RisaccaAudioProcessor::handleAsyncUpdate()
{
    if (! prepared) return;
    const double target = (rawQuality->load() > 0.5f) ? 384000.0 : 192000.0;
    int f = 1;
    while (baseRate * f < target * 0.9 && f < 16) f *= 2;
    const int seed = juce::jlimit (1, 999, juce::roundToInt (rawSeed->load()));
    if (f == preparedFactor && seed == preparedSeed) return;
    suspendProcessing (true);
    rebuild();
    suspendProcessing (false);
}

void RisaccaAudioProcessor::applyCC (int cc, int value)
{
    if (ccMap.learnParam.load() >= 0 && MidiCCMap::isLearnable (cc))
        ccMap.assign (cc, ccMap.learnParam.exchange (-1));
    const int p = ccMap.paramForCC (cc);
    if (p < 0) return;
    auto* param = params[(size_t) p];
    const float norm = pid::all[(size_t) p].kind == 1 ? (value >= 64 ? 1.0f : 0.0f) : (float) value / 127.0f;
    if (std::abs (param->getValue() - norm) > 1.0e-4f)
        param->setValueNotifyingHost (norm);
}

void RisaccaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController()) applyCC (m.getControllerNumber(), m.getControllerValue());
    }
    midi.clear();

    if (powerCycle.exchange (false)) engine.reset();
    const auto ctl = readControls();
    engine.setControls (ctl);

    const float inGain = juce::Decibels::decibelsToGain (raw[pid::inLevel]->load());   // 0 dBFS = 1 V di picco
    const float outGain = juce::Decibels::decibelsToGain (raw[pid::outLevel]->load()); // 1 V = 0 dBFS
    const float leak = (! ctl.engaged && ! ctl.tame) ? 0.01f : 0.0f;  // Trouble: il circuito "perde" anche in bypass
    const float fadeStep = (float) (1.0 / (0.008 * baseRate));
    float lfoMax = 0.0f, moodMax = 0.0f, preMax = 0.0f;

    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - start);
        float* m = mono.getWritePointer (0);
        for (int i = 0; i < len; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < nIn; ++ch) s += buffer.getReadPointer (ch)[start + i];
            m[i] = nIn > 0 ? s / (float) nIn : 0.0f;
        }
        // copia "dry" ritardata quanto il sovracampionamento (per il bypass)
        float* dl = dryDelay.getWritePointer (0);
        const int dlLen = dryDelay.getNumSamples();
        float* dryBuf = dryOut.getWritePointer (0);

        juce::dsp::AudioBlock<float> block (mono.getArrayOfWritePointers(), 1, (size_t) len);
        for (int i = 0; i < len; ++i)
        {
            dl[dryWrite] = m[i];
            int rd = dryWrite - latency; if (rd < 0) rd += dlLen;
            dryBuf[i] = dl[rd];
            if (++dryWrite >= dlLen) dryWrite = 0;
        }

        if (oversampler)
        {
            auto up = oversampler->processSamplesUp (block);
            float* u = up.getChannelPointer (0);
            const int nu = (int) up.getNumSamples();
            for (int i = 0; i < nu; ++i)
            {
                u[i] = (float) engine.process ((double) (u[i] * inGain));
                const auto& mt = engine.meters();
                lfoMax = juce::jmax (lfoMax, (float) mt.ledLfo);
                moodMax = juce::jmax (moodMax, (float) mt.ledMood);
                preMax = juce::jmax (preMax, (float) mt.ledPreclip);
            }
            oversampler->processSamplesDown (block);
        }
        else
        {
            for (int i = 0; i < len; ++i) m[i] = (float) engine.process ((double) (m[i] * inGain));
        }

        const float target = ctl.engaged ? 1.0f : 0.0f;
        for (int i = 0; i < len; ++i)
        {
            if (wetGain < target) wetGain = juce::jmin (target, wetGain + fadeStep);
            else if (wetGain > target) wetGain = juce::jmax (target, wetGain - fadeStep);
            const float wet = m[i] * outGain;
            const float dry = dryBuf[i];
            const float y = wetGain * wet + (1.0f - wetGain) * (dry + leak * wet);
            for (int ch = 0; ch < nOut; ++ch) buffer.getWritePointer (ch)[start + i] = y;
        }
    }

    const auto& mt = engine.meters();
    meters.on.store (ctl.engaged ? 1.0f : 0.0f);
    meters.lfo.store (lfoMax);
    meters.mood.store (moodMax);
    meters.preclip.store (preMax);
    meters.vLow.store ((float) mt.vLow);
    meters.swashPos.store ((float) mt.ledSwashPos);
    meters.swashNeg.store ((float) mt.ledSwashNeg);
    const double used = juce::Time::getMillisecondCounterHiRes() - t0;
    const double avail = 1000.0 * n / juce::jmax (1.0, baseRate);
    meters.cpu.store ((float) (0.9 * meters.cpu.load() + 0.1 * used / avail));
}

void RisaccaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("MIDIMAP"); old.isValid()) state.removeChild (old, nullptr);
    state.appendChild (ccMap.toValueTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void RisaccaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        if (auto map = state.getChildWithName ("MIDIMAP"); map.isValid())
        {
            ccMap.fromValueTree (map);
            state.removeChild (map, nullptr);
        }
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* RisaccaAudioProcessor::createEditor() { return new RisaccaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RisaccaAudioProcessor(); }
