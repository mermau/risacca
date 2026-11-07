#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PedalLookAndFeel.h"

// Manopola del pedale: Slider rotativo con menu MIDI Learn sul tasto destro
class PedalKnob : public juce::Slider
{
public:
    PedalKnob (RisaccaAudioProcessor& p, int paramIndex, int style);
    void mouseDown (const juce::MouseEvent& e) override;
    const int index;
private:
    RisaccaAudioProcessor& proc;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Interruttore a levetta (o pulsante a pedale / LED del pannello CV)
class PedalToggle : public juce::Button
{
public:
    enum Kind { toggle, footswitch, panelButton };
    PedalToggle (RisaccaAudioProcessor& p, int paramIndex, Kind k, const juce::String& text = {});
    void paintButton (juce::Graphics& g, bool over, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void clicked() override { shownState = getToggleState(); }
    // footswitch premuto via MIDI/host: lo mostra abbassato per qualche frame
    void tickPress();
    const int index;
    bool learning = false;
private:
    void paintFootswitch (juce::Graphics& g, bool down);
    void paintLever (juce::Graphics& g);
    bool shownState = false;
    int pressFrames = 0;
    RisaccaAudioProcessor& proc;
    Kind kind;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Pannello posteriore: CV dell'Eurorack, mod nascoste, impostazioni del motore, MIDI
class BackPanel : public juce::Component
{
public:
    explicit BackPanel (RisaccaAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    std::function<void()> onClose;
    std::vector<PedalKnob*> knobs() { return { moodKnob.get(), swashCvKnob.get(), inKnob.get(), outKnob.get() }; }
    std::vector<PedalToggle*> toggles() { return { moodPatch.get(), modLed.get() }; }
    void refresh();
private:
    RisaccaAudioProcessor& proc;
    std::unique_ptr<PedalKnob> moodKnob, swashCvKnob, inKnob, outKnob;
    std::unique_ptr<PedalToggle> moodPatch, modLed;
    juce::ComboBox toneBox, sourceBox, qualityBox;
    juce::Slider seedSlider;
    juce::TextButton newUnit { "Nuovo esemplare" }, power { "Spegni e riaccendi" },
                     keylab { "Mappa KeyLab" }, clearCC { "Cancella CC" }, close { "Chiudi" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> toneAtt, sourceAtt, qualityAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedAtt;
    juce::Label cpuLabel;
};

// Frontale del pedale (coordinate fisse 860 x 540, scalato dall'editor)
class PedalFace : public juce::Component
{
public:
    explicit PedalFace (RisaccaAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    void tick();
    void showBackPanel (bool show);
    static constexpr int W = 860, H = 540;
private:
    RisaccaAudioProcessor& proc;
    std::vector<std::unique_ptr<PedalKnob>> knobs;
    std::vector<std::unique_ptr<PedalToggle>> toggles;
    std::unique_ptr<PedalToggle> foot;
    juce::TextButton cvButton { "CV  /  MOD" };
    BackPanel back;
    juce::Image texture;
    float ledOn = 0, ledLfo = 0, ledMood = 0;
    int lastLearn = -2, lastVersion = -1;
    juce::Point<float> pt (float px, float py) const;   // coordinate della foto -> frontale
};

class RisaccaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RisaccaEditor (RisaccaAudioProcessor& p);
    ~RisaccaEditor() override;
    void paint (juce::Graphics& g) override;
    void resized() override;
private:
    void timerCallback() override;
    RisaccaAudioProcessor& proc;
    look::PedalLookAndFeel lnf;
    PedalFace face;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RisaccaEditor)
};
