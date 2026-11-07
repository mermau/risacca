#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wshadow", "-Wsign-conversion", "-Wfloat-equal", "-Wconversion", "-Wshorten-64-to-32", "-Wimplicit-int-float-conversion", "-Wfloat-conversion")
#include "../engine/SwashEngine.h"
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#include "MidiCCMap.h"

// RISACCA - emulazione a livello di circuito del 4ms Noise Swash (Standard / Max Tweaker)
class RisaccaAudioProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    RisaccaAudioProcessor();
    ~RisaccaAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    MidiCCMap ccMap;

    juce::RangedAudioParameter* getParam (int index) const { return params[(size_t) index]; }

    // indicatori per la GUI (aggiornati a ogni blocco)
    struct Meters { std::atomic<float> on { 0 }, lfo { 0 }, mood { 0 }, preclip { 0 }, vLow { 0 },
                    swashPos { 0 }, swashNeg { 0 }, cpu { 0 }; };
    Meters meters;

    void requestPowerCycle() { powerCycle.store (true); }  // stacca e riattacca l'alimentazione
    int getOversampling() const { return factor; }

private:
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void rebuild();                         // ricostruisce motore e sovracampionamento
    void applyCC (int cc, int value);
    risacca::SwashControls readControls() const;

    risacca::SwashEngine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> mono, dryDelay, dryOut;
    int dryWrite = 0, latency = 0;
    int factor = 4;
    double baseRate = 0.0;
    int maxBlock = 0;
    int preparedFactor = 0, preparedSeed = -1;
    double preparedRate = 0.0;
    bool prepared = false;
    float wetGain = 1.0f;                   // crossfade dell'interruttore a pedale
    std::atomic<bool> powerCycle { false };

    std::array<juce::RangedAudioParameter*, pid::numMappable> params {};
    std::array<std::atomic<float>*, pid::numMappable> raw {};
    std::atomic<float>* rawQuality = nullptr;
    std::atomic<float>* rawSeed = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RisaccaAudioProcessor)
};
