#include "PluginEditor.h"

namespace
{
const juce::Colour ink (0xff141414);
const juce::Colour accent (0xffd04a2b);

void showMidiMenu (RisaccaAudioProcessor& p, int index, juce::Component* target)
{
    juce::PopupMenu m;
    const int cc = p.ccMap.ccForParam (index);
    m.addSectionHeader (juce::String (pid::all[(size_t) index].name)
                        + (cc >= 0 ? "  -  CC " + juce::String (cc) : "  -  nessun CC"));
    m.addItem (1, "MIDI Learn (muovi un controllo della tastiera)");
    m.addItem (2, "Rimuovi assegnazione CC", cc >= 0);
    m.addSeparator();
    m.addItem (3, "Annulla MIDI Learn in corso", p.ccMap.learnParam.load() >= 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                     [&p, index] (int r)
                     {
                         if (r == 1) p.ccMap.learnParam.store (index);
                         else if (r == 2) p.ccMap.clearParam (index);
                         else if (r == 3) p.ccMap.learnParam.store (-1);
                     });
}

juce::String tipFor (RisaccaAudioProcessor& p, int index)
{
    const int cc = p.ccMap.ccForParam (index);
    return juce::String (pid::all[(size_t) index].name)
           + (cc >= 0 ? "   [CC " + juce::String (cc) + "]" : "")
           + "\nTasto destro: MIDI Learn";
}
} // namespace

// ============================================================================ PedalKnob
PedalKnob::PedalKnob (RisaccaAudioProcessor& p, int paramIndex, int style)
    : index (paramIndex), proc (p)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    setMouseDragSensitivity (240);
    setVelocityBasedMode (false);
    getProperties().set ("style", style);
    setPopupDisplayEnabled (true, false, nullptr);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, pid::all[(size_t) paramIndex].id, *this);
    setTooltip (tipFor (p, paramIndex));
}

void PedalKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMidiMenu (proc, index, this); return; }
    juce::Slider::mouseDown (e);
}

// ============================================================================ PedalToggle
PedalToggle::PedalToggle (RisaccaAudioProcessor& p, int paramIndex, Kind k, const juce::String& text)
    : juce::Button (text), index (paramIndex), proc (p), kind (k)
{
    setClickingTogglesState (true);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, pid::all[(size_t) paramIndex].id, *this);
    setTooltip (tipFor (p, paramIndex));
}

void PedalToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMidiMenu (proc, index, this); return; }
    juce::Button::mouseDown (e);
}

void PedalToggle::tickPress()
{
    if (kind != footswitch) return;
    if (getToggleState() != shownState)          // cambiato da MIDI o dall'host, non dal mouse
    {
        shownState = getToggleState();
        pressFrames = 5;                         // ~170 ms abbassato a 30 Hz
        repaint();
    }
    else if (pressFrames > 0 && --pressFrames == 0)
        repaint();
}

namespace
{
juce::ColourGradient chromeGrad (juce::Point<float> c, float r, float darken = 0.0f)
{
    juce::ColourGradient cg (juce::Colours::white.darker (darken), c.x - 0.35f * r, c.y - 0.45f * r,
                             juce::Colour (0xff4f5358).darker (darken), c.x + 0.9f * r, c.y + 0.9f * r, true);
    cg.addColour (0.35, juce::Colour (0xffd8dadd).darker (darken));
    cg.addColour (0.75, juce::Colour (0xff85898f).darker (darken));
    return cg;
}
juce::Rectangle<float> disc (juce::Point<float> p, float r) { return juce::Rectangle<float> (2 * r, 2 * r).withCentre (p); }
}

void PedalToggle::paintFootswitch (juce::Graphics& g, bool down)
{
    // Vista dall'alto, luce fissa in alto a sinistra. Premendo, il pulsante scende dentro
    // la ghiera: si accorciano la parete laterale visibile e l'ombra che proietta nel foro.
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;

    Path hex;   // dado esagonale
    for (int i = 0; i < 6; ++i)
    {
        const auto p = c.getPointOnCircumference (R * 0.96f, (float) i * MathConstants<float>::pi / 3.0f);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillPath (hex, AffineTransform::translation (1.5f, 3.0f));
    g.setGradientFill (chromeGrad (c, R, 0.25f));
    g.fillPath (hex);
    g.setColour (Colours::black.withAlpha (0.3f));
    g.strokePath (hex, PathStrokeType (1.0f));

    const float ring = R * 0.68f, hole = R * 0.55f, rp = R * 0.47f;   // ghiera e foro
    g.setGradientFill (chromeGrad (c, ring, 0.3f));
    g.fillEllipse (disc (c, ring));
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawEllipse (disc (c, ring), 1.0f);
    g.setGradientFill (ColourGradient (Colour (0xff050506), c.x - hole * 0.6f, c.y - hole * 0.6f,
                                       Colour (0xff2a2c30), c.x + hole * 0.7f, c.y + hole * 0.8f, false));
    g.fillEllipse (disc (c, hole));

    const float h = R * (down ? 0.03f : 0.11f);   // altezza del pulsante sopra la ghiera
    const auto top = c.translated (0.0f, -h);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addEllipse (disc (c, hole));
        g.reduceClipRegion (clip);
        const auto sc = c.translated (h * 0.9f, h * 1.2f);
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (Colours::black.withAlpha (0.16f));
            g.fillEllipse (disc (sc, rp + (float) i * R * 0.012f));
        }
    }
    {
        Path wall;   // parete laterale del cilindro
        wall.addEllipse (disc (c, rp));
        wall.addRectangle (c.x - rp, top.y, 2 * rp, c.y - top.y);
        g.setGradientFill (ColourGradient (Colour (0xff9a9ea4), c.x - rp, c.y, Colour (0xff3b3e43), c.x + rp, c.y, false));
        g.fillPath (wall);
    }
    ColourGradient face (Colour (0xfffbfbfc), top.x - rp * 0.45f, top.y - rp * 0.5f,
                         Colour (0xff5f6369), top.x + rp * 0.9f, top.y + rp * 0.95f, true);
    face.addColour (0.30, Colour (0xffe2e4e7));
    face.addColour (0.70, Colour (0xff9da1a7));
    g.setGradientFill (face);
    g.fillEllipse (disc (top, rp));
    g.setColour (Colours::white.withAlpha (0.55f));
    g.drawEllipse (disc (top, rp - 0.8f), 1.0f);
    g.setColour (Colours::black.withAlpha (0.25f));
    {
        Path rim; rim.addCentredArc (top.x, top.y, rp - 0.5f, rp - 0.5f, 0.0f,
                                     MathConstants<float>::pi * 0.65f, MathConstants<float>::pi * 1.35f, true);
        g.strokePath (rim, PathStrokeType (1.2f));
    }
    g.setColour (Colours::white.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (rp * 0.55f, rp * 0.32f).withCentre (top.translated (-rp * 0.32f, -rp * 0.38f)));
}

void PedalToggle::paintLever (juce::Graphics& g)
{
    // Deviatore a levetta visto dall'alto: la levetta sta quasi in piedi e si inclina di circa
    // 25 gradi verso l'alto (acceso) o verso il basso (spento), come su un pedale vero.
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const bool on = getToggleState();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.22f;

    Path hex;   // dado esagonale
    for (int i = 0; i < 6; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / 6.0f + 0.52f;
        const auto p = c.getPointOnCircumference (r * 1.35f, a + MathConstants<float>::halfPi);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillPath (hex, AffineTransform::translation (1.5f, 2.0f));
    g.setGradientFill (ColourGradient (Colour (0xffeef0f2), c.x - r, c.y - r, Colour (0xff80868d), c.x + r, c.y + r, false));
    g.fillPath (hex);
    g.setColour (Colour (0xff5d6268));
    g.strokePath (hex, PathStrokeType (0.8f));
    // boccola filettata con il foro
    g.setGradientFill (chromeGrad (c, r * 0.78f, 0.35f));
    g.fillEllipse (disc (c, r * 0.78f));
    g.setColour (Colour (0xff1c1d20));
    g.fillEllipse (disc (c, r * 0.5f));

    const float tilt = degreesToRadians (25.0f);
    const float len = b.getHeight() * 0.44f;              // lunghezza reale della levetta
    const float reach = len * std::sin (tilt);            // quanto si sposta vista dall'alto
    const float height = len * std::cos (tilt);           // quanto sta in alto: allunga l'ombra
    const float dir = on ? -1.0f : 1.0f;
    const Point<float> tip (c.x, c.y + dir * reach);
    const float wBase = r * 0.40f, wTip = r * 0.62f;      // la punta è più vicina all'osservatore

    // ombra proiettata sul pannello (luce in alto a sinistra)
    const Point<float> sh (height * 0.12f, height * 0.16f);
    {
        Path s;
        s.startNewSubPath (c.x - wBase * 0.5f, c.y);
        s.lineTo (c.x + wBase * 0.5f, c.y);
        s.lineTo (tip.x + sh.x + wTip * 0.5f, tip.y + sh.y);
        s.lineTo (tip.x + sh.x - wTip * 0.5f, tip.y + sh.y);
        s.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.22f));
        g.fillPath (s);
        g.fillEllipse (disc (tip + sh, r * 0.5f));
    }
    // stelo rastremato
    Path shaft;
    shaft.startNewSubPath (c.x - wBase * 0.5f, c.y);
    shaft.lineTo (c.x + wBase * 0.5f, c.y);
    shaft.lineTo (tip.x + wTip * 0.5f, tip.y);
    shaft.lineTo (tip.x - wTip * 0.5f, tip.y);
    shaft.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xfff7f8f9), c.x - wTip * 0.5f, c.y, Colour (0xff767c83), c.x + wTip * 0.5f, c.y, false));
    g.fillPath (shaft);
    // pomello all'estremità
    const float rb = r * 0.5f;
    ColourGradient ball (Colour (0xffffffff), tip.x - rb * 0.4f, tip.y - rb * 0.45f,
                         Colour (0xff6c727a), tip.x + rb, tip.y + rb, true);
    ball.addColour (0.4, Colour (0xffd5d9dd));
    g.setGradientFill (ball);
    g.fillEllipse (disc (tip, rb));
    g.setColour (Colour (0xff5f656c));
    g.drawEllipse (disc (tip, rb), 0.7f);
}

void PedalToggle::paintButton (juce::Graphics& g, bool, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const bool on = getToggleState();
    if (kind == toggle)
        paintLever (g);
    else if (kind == footswitch)
        paintFootswitch (g, down || pressFrames > 0);
    else // pulsante del pannello posteriore
    {
        auto rr = b.reduced (1.0f);
        g.setColour (on ? accent : juce::Colour (0xff2e2e34));
        g.fillRoundedRectangle (rr, 4.0f);
        g.setColour (juce::Colour (0xff66666e));
        g.drawRoundedRectangle (rr, 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (look::font (12.5f));
        g.drawFittedText (getButtonText(), rr.toNearestInt(), juce::Justification::centred, 2);
    }
    if (learning)
    {
        g.setColour (juce::Colour (0xff3fa9ff));
        g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 2.5f);
    }
}

// ============================================================================ BackPanel
BackPanel::BackPanel (RisaccaAudioProcessor& p) : proc (p)
{
    moodKnob = std::make_unique<PedalKnob> (p, pid::moodCV, look::panel);
    swashCvKnob = std::make_unique<PedalKnob> (p, pid::swashCV, look::panel);
    inKnob = std::make_unique<PedalKnob> (p, pid::inLevel, look::panel);
    outKnob = std::make_unique<PedalKnob> (p, pid::outLevel, look::panel);
    moodPatch = std::make_unique<PedalToggle> (p, pid::moodPatched, PedalToggle::panelButton, "Cavo inserito");
    modLed = std::make_unique<PedalToggle> (p, pid::modLed, PedalToggle::panelButton, "D2 = LED rosso");
    for (auto* k : { moodKnob.get(), swashCvKnob.get(), inKnob.get(), outKnob.get() }) addAndMakeVisible (k);
    addAndMakeVisible (*moodPatch); addAndMakeVisible (*modLed);

    toneBox.addItemList ({ "Standard (Bass/Treble)", "Eurorack, C5 100n", "Eurorack, C5 10n" }, 1);
    sourceBox.addItemList ({ "Linea", "Chitarra", "Scollegato" }, 1);
    qualityBox.addItemList ({ "Standard (4x)", "Alta (8x)" }, 1);
    for (auto* cb : { &toneBox, &sourceBox, &qualityBox }) addAndMakeVisible (cb);
    toneAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "modTone", toneBox);
    sourceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "source", sourceBox);
    qualityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, pid::qualityId, qualityBox);

    seedSlider.setSliderStyle (juce::Slider::LinearBar);
    seedSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 22);
    seedSlider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.6f));
    addAndMakeVisible (seedSlider);
    seedAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, pid::seedId, seedSlider);

    newUnit.onClick = [this]
    {
        auto* prm = proc.apvts.getParameter (pid::seedId);
        const int v = juce::Random::getSystemRandom().nextInt ({ 1, 1000 });
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
    };
    power.onClick = [this] { proc.requestPowerCycle(); };
    keylab.onClick = [this] { proc.ccMap.resetToKeyLabDefaults(); };
    clearCC.onClick = [this] { proc.ccMap.clearAll(); };
    close.onClick = [this] { if (onClose) onClose(); };
    for (auto* b : { &newUnit, &power, &keylab, &clearCC, &close }) addAndMakeVisible (b);
    cpuLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    cpuLabel.setFont (look::font (12.0f, false));
    addAndMakeVisible (cpuLabel);
}

void BackPanel::refresh()
{
    cpuLabel.setText ("Carico: " + juce::String (juce::roundToInt (proc.meters.cpu.load() * 100.0f)) + "% di un core   |   "
                      "interno " + juce::String (proc.getOversampling()) + "x   |   linea Low "
                      + juce::String (proc.meters.vLow.load(), 2) + " V", juce::dontSendNotification);
}

void BackPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    auto r = getLocalBounds().toFloat().reduced (24.0f);
    juce::ColourGradient pg (juce::Colour (0xff2a2a2f), r.getX(), r.getY(), juce::Colour (0xff16161a), r.getX(), r.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colour (0xff4a4a52));
    g.drawRoundedRectangle (r, 10.0f, 1.5f);
    // viti
    for (auto pnt : { r.getTopLeft() + juce::Point<float> (14, 14), r.getTopRight() + juce::Point<float> (-14, 14),
                      r.getBottomLeft() + juce::Point<float> (14, -14), r.getBottomRight() + juce::Point<float> (-14, -14) })
    {
        g.setColour (juce::Colour (0xff8a8f96)); g.fillEllipse (pnt.x - 5, pnt.y - 5, 10, 10);
        g.setColour (juce::Colour (0xff3a3d42)); g.drawLine (pnt.x - 3, pnt.y, pnt.x + 3, pnt.y, 1.4f);
    }
    g.setColour (juce::Colours::white);
    g.setFont (look::font (20.0f));
    g.drawText ("PANNELLO CV / MOD", (int) r.getX() + 30, (int) r.getY() + 14, 400, 26, juce::Justification::left);

    auto section = [&] (int x, int y, int w, int h, const juce::String& title, const juce::String& sub)
    {
        g.setColour (juce::Colour (0xff3a3a42));
        g.drawRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f, 1.0f);
        g.setColour (accent);
        g.setFont (look::font (13.0f));
        g.drawText (title, x + 10, y + 6, w - 20, 18, juce::Justification::left);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (look::font (11.0f, false));
        g.drawFittedText (sub, x + 10, y + h - 34, w - 20, 30, juce::Justification::topLeft, 2);
    };
    section (54, 80, 170, 220, "MOOD  CV In", "Jack Mood (Eurorack): 0..15 V sul nodo dopo C4");
    section (234, 80, 170, 220, "SWASH  CV In +/-", "+ : vactrol Tone-Gait   - : vactrol sullo Swash");
    section (414, 80, 190, 220, "MOD", "Varianti dello schema Eurorack");
    section (614, 80, 192, 220, "INGRESSO / USCITA", "0 dBFS = 1 V di picco");
    section (54, 310, 550, 150, "MOTORE", "Esemplare = tolleranze dei componenti e offset degli op-amp");
    section (614, 310, 192, 150, "MIDI", "Tasto destro su un controllo: MIDI Learn");

    // LED dei CV
    auto led = [&] (float cx, float cy, juce::Colour col, float v)
    {
        g.setColour (juce::Colour (0xff111111)); g.fillEllipse (cx - 7, cy - 7, 14, 14);
        g.setColour (col.withAlpha (0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, v))); g.fillEllipse (cx - 5, cy - 5, 10, 10);
    };
    // valori sotto le manopole
    auto cap = [&] (int idx, int cx, int cy, const juce::String& name)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (look::font (12.0f));
        g.drawText (name, cx - 60, cy, 120, 16, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (look::font (11.5f, false));
        g.drawText (proc.getParam (idx)->getCurrentValueAsText(), cx - 60, cy + 15, 120, 16, juce::Justification::centred);
    };
    cap (pid::moodCV, 139, 232, "Mood");
    cap (pid::swashCV, 319, 232, "Swash CV");
    cap (pid::inLevel, 664, 184, "Ingresso");
    cap (pid::outLevel, 752, 184, "Uscita");
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (look::font (12.0f));
    g.drawText ("Tone", 430, 150, 158, 16, juce::Justification::left);
    g.drawText ("Sorgente", 630, 222, 160, 16, juce::Justification::left);
    led (139.0f, 112.0f, juce::Colour (0xff3a7bff), proc.meters.mood.load());
    led (300.0f, 112.0f, juce::Colour (0xffff9a1a), proc.meters.swashPos.load());
    led (338.0f, 112.0f, juce::Colour (0xff35d04a), proc.meters.swashNeg.load());
}

void BackPanel::resized()
{
    moodPatch->setBounds (74, 104, 130, 26);
    moodKnob->setBounds (99, 140, 80, 80);
    swashCvKnob->setBounds (279, 140, 80, 80);
    modLed->setBounds (430, 110, 158, 28);
    toneBox.setBounds (430, 168, 158, 26);
    inKnob->setBounds (629, 112, 70, 70);
    outKnob->setBounds (717, 112, 70, 70);
    sourceBox.setBounds (630, 240, 160, 24);
    qualityBox.setBounds (70, 340, 170, 26);
    seedSlider.setBounds (260, 340, 180, 26);
    newUnit.setBounds (450, 340, 140, 26);
    power.setBounds (70, 380, 170, 26);
    cpuLabel.setBounds (260, 380, 340, 26);
    keylab.setBounds (630, 336, 160, 26);
    clearCC.setBounds (630, 370, 160, 26);
    close.setBounds (720, 30, 90, 26);
}

// ============================================================================ PedalFace
namespace
{
struct KnobDef { int index; int style; float px, py, d; const char* label; float lx, ly; };
// posizioni prese dalla foto del pedale (800 x 475), etichette come sul frontale
const KnobDef knobDefs[] = {
    { pid::noisegate, look::chromeBig,  215,  45, 76, "Noisegate",   215,  96 },
    { pid::swash,     look::chromeBig,  385,  45, 76, "Swash",       395,  96 },
    { pid::bass,      look::blackSmall, 580,  45, 48, "Bass",        585,  84 },
    { pid::clean,     look::blackSmall,  95, 112, 52, "Clean blend",  92, 154 },
    { pid::pregain,   look::chromeMid,  300, 128, 62, "Pregain",     300, 172 },
    { pid::treble,    look::blackSmall, 490, 128, 48, "Treble",      490, 166 },
    { pid::lfoSpeed,  look::chicken,    700, 112, 64, "LFO speed",   690, 160 },
    { pid::selfOsc,   look::chromeDome, 105, 205, 80, "Self-osc",    105, 253 },
    { pid::noiseVol,  look::blackSmall, 650, 193, 54, "Noise vol",   655, 233 },
    { pid::low,       look::blackLarge, 420, 245, 88, "Low",         420, 302 },
    { pid::volume,    look::blackSmall, 270, 265, 56, "Volume",      270, 305 },
    { pid::postgain,  look::chromeMid,  580, 265, 62, "Postgain",    590, 307 },
};
struct ToggleDef { int index; float px, py; const char* label; float lx, ly; };
const ToggleDef toggleDefs[] = {
    { pid::preclip,   300,  45, "Preclip",      300,  78 },
    { pid::postclip,  480,  45, "Postclip",     480,  78 },
    { pid::selfOscSw,  75, 292, "",               0,   0 },
    { pid::tame,      185, 292, "Tame/trouble", 205, 322 },
    { pid::lfoSw,     735, 292, "LFO",          706, 252 },
};
} // namespace

juce::Point<float> PedalFace::pt (float px, float py) const
{
    return { 20.0f + px * 1.025f, 20.0f + py * 1.053f };
}

PedalFace::PedalFace (RisaccaAudioProcessor& p) : proc (p), back (p)
{
    for (const auto& k : knobDefs)
    {
        auto kn = std::make_unique<PedalKnob> (p, k.index, k.style);
        addAndMakeVisible (*kn);
        knobs.push_back (std::move (kn));
    }
    for (const auto& t : toggleDefs)
    {
        auto tg = std::make_unique<PedalToggle> (p, t.index, PedalToggle::toggle);
        addAndMakeVisible (*tg);
        toggles.push_back (std::move (tg));
    }
    foot = std::make_unique<PedalToggle> (p, pid::engaged, PedalToggle::footswitch);
    addAndMakeVisible (*foot);

    cvButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2b30));
    cvButton.onClick = [this] { back.setVisible (true); back.toFront (true); };
    cvButton.setTooltip ("Pannello posteriore: CV dell'Eurorack, mod, ingresso/uscita, motore, MIDI");
    addAndMakeVisible (cvButton);
    back.onClose = [this] { back.setVisible (false); };
    addChildComponent (back);

    // grana della vernice a polvere
    texture = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Random rnd (4242);
    for (int i = 0; i < 26000; ++i)
    {
        const int x = rnd.nextInt (W), y = rnd.nextInt (H);
        const float a = 0.02f + 0.05f * rnd.nextFloat();
        texture.setPixelAt (x, y, (rnd.nextBool() ? juce::Colours::black : juce::Colours::white).withAlpha (a));
    }
    setSize (W, H);
}

void PedalFace::resized()
{
    if (foot == nullptr) return;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = knobDefs[i];
        const auto c = pt (k.px, k.py);
        knobs[i]->getProperties().set ("diam", k.d);
        knobs[i]->setBounds (juce::Rectangle<int> ((int) k.d + 34, (int) k.d + 34).withCentre (c.toInt()));
    }
    for (size_t i = 0; i < toggles.size(); ++i)
    {
        const auto c = pt (toggleDefs[i].px, toggleDefs[i].py);
        toggles[i]->setBounds (juce::Rectangle<int> (40, 62).withCentre (c.toInt()));
    }
    foot->setBounds (juce::Rectangle<int> (66, 66).withCentre (pt (305, 385).toInt()));
    cvButton.setBounds (712, 56, 112, 24);
    back.setBounds (getLocalBounds());
}

void PedalFace::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1e));
    auto box = getLocalBounds().toFloat().reduced (12.0f);
    // ombra
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (box.translated (4.0f, 6.0f), 24.0f);
    // contenitore verniciato bianco
    juce::ColourGradient body (juce::Colour (0xfff4f2ec), box.getX(), box.getY(), juce::Colour (0xffd9d5ca), box.getRight(), box.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (box, 24.0f);
    g.drawImageAt (texture, 0, 0);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawRoundedRectangle (box.reduced (2.0f), 22.0f, 2.0f);
    g.setColour (juce::Colour (0xff8f8a7e));
    g.drawRoundedRectangle (box, 24.0f, 1.5f);

    // jack sul bordo superiore (decorazione)
    auto jack = [&] (float x, const char* label)
    {
        g.setColour (juce::Colour (0xffa8adb3)); g.fillEllipse (x - 11, 15, 22, 22);
        g.setColour (juce::Colour (0xff2a2a2a)); g.fillEllipse (x - 6, 20, 12, 12);
        g.setColour (ink.withAlpha (0.7f)); g.setFont (look::font (10.0f));
        g.drawText (label, (int) x - 30, 38, 60, 12, juce::Justification::centred);
    };
    jack (60.0f, "OUT"); jack (690.0f, "9V DC"); jack (800.0f, "IN");

    g.setColour (ink);
    g.setFont (look::font (16.0f));
    for (const auto& k : knobDefs)
    {
        const auto c = pt (k.lx, k.ly);
        g.drawText (k.label, juce::Rectangle<int> (120, 20).withCentre (c.toInt()), juce::Justification::centred);
    }
    g.setFont (look::font (14.0f));
    for (const auto& t : toggleDefs)
        if (t.label[0] != 0)
            g.drawText (t.label, juce::Rectangle<int> (120, 18).withCentre (pt (t.lx, t.ly).toInt()), juce::Justification::centred);
    g.setFont (look::font (16.0f));
    g.drawText ("Bypass", juce::Rectangle<int> (100, 20).withCentre (pt (215, 407).toInt()), juce::Justification::centred);

    // LED: acceso (rosso), Mood (blu), LFO (rosso)
    auto led = [&] (juce::Point<float> c, juce::Colour col, float v)
    {
        g.setColour (juce::Colour (0xff2a2a2a)); g.fillEllipse (c.x - 6.5f, c.y - 6.5f, 13, 13);
        const float a = 0.18f + 0.82f * juce::jlimit (0.0f, 1.0f, v);
        g.setColour (col.withAlpha (a)); g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9, 9);
        if (v > 0.05f)
        {
            juce::ColourGradient glow (col.withAlpha (0.45f * v), c.x, c.y, col.withAlpha (0.0f), c.x + 16, c.y, true);
            g.setGradientFill (glow); g.fillEllipse (c.x - 16, c.y - 16, 32, 32);
        }
    };
    led (pt (148, 293), juce::Colour (0xffff2a1a), ledOn);
    led (pt (395, 293), juce::Colour (0xff3a7bff), ledMood);
    led (pt (662, 292), juce::Colour (0xffff2a1a), ledLfo);

    // nome
    g.setColour (ink);
    g.setFont (look::font (60.0f));
    g.drawText ("risacca", juce::Rectangle<int> (420, 70).withCentre (pt (590, 398).toInt()), juce::Justification::centredLeft);
    g.setFont (look::font (13.0f, false));
    g.drawText ("Collateral Effects  -  noise swash circuit", juce::Rectangle<int> (420, 18).withCentre (pt (590, 438).toInt()),
                juce::Justification::centredLeft);
}

void PedalFace::paintOverChildren (juce::Graphics& g)
{
    const int learn = proc.ccMap.learnParam.load();
    if (learn >= 0)
    {
        auto r = juce::Rectangle<int> (420, 28).withCentre ({ W / 2, H - 34 });
        g.setColour (juce::Colour (0xee10325a));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (look::font (13.0f));
        g.drawText ("MIDI Learn: " + juce::String (pid::all[(size_t) learn].name) + " - muovi un controllo della KeyLab",
                    r, juce::Justification::centred);
    }
}

void PedalFace::showBackPanel (bool show)
{
    back.setVisible (show);
    if (show) back.toFront (false);
}

void PedalFace::tick()
{
    if (foot != nullptr) foot->tickPress();
    const float on = proc.meters.on.load(), lfo = proc.meters.lfo.load(), mood = proc.meters.mood.load();
    if (std::abs (on - ledOn) > 0.01f || std::abs (lfo - ledLfo) > 0.02f || std::abs (mood - ledMood) > 0.02f)
    {
        ledOn = on; ledLfo = lfo; ledMood = mood;
        repaint();
    }
    const int learn = proc.ccMap.learnParam.load();
    const int ver = proc.ccMap.version.load();
    if (learn != lastLearn || ver != lastVersion)
    {
        lastLearn = learn; lastVersion = ver;
        auto allKnobs = back.knobs();
        for (auto& k : knobs) allKnobs.push_back (k.get());
        for (auto* k : allKnobs)
        {
            k->getProperties().set ("learn", k->index == learn);
            k->setTooltip (tipFor (proc, k->index));
            k->repaint();
        }
        auto allToggles = back.toggles();
        for (auto& t : toggles) allToggles.push_back (t.get());
        allToggles.push_back (foot.get());
        for (auto* t : allToggles)
        {
            t->learning = (t->index == learn);
            t->setTooltip (tipFor (proc, t->index));
            t->repaint();
        }
        repaint();
    }
    if (back.isVisible()) { back.refresh(); back.repaint(); }
}

// ============================================================================ Editor
RisaccaEditor::RisaccaEditor (RisaccaAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), face (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (face);
    setResizable (true, true);
    setResizeLimits (PedalFace::W / 2, PedalFace::H / 2, PedalFace::W * 2, PedalFace::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) PedalFace::W / PedalFace::H);
    setSize (PedalFace::W, PedalFace::H);
    startTimerHz (30);
}

RisaccaEditor::~RisaccaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RisaccaEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1b1b1e)); }

void RisaccaEditor::resized()
{
    const float s = (float) getWidth() / (float) PedalFace::W;
    face.setTransform (juce::AffineTransform::scale (s));
    face.setBounds (0, 0, PedalFace::W, PedalFace::H);
}

void RisaccaEditor::timerCallback() { face.tick(); }
