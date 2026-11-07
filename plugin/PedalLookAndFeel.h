#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Aspetto del pedale: manopole di tipo diverso come sul Noise Swash "Max Tweaker" a 12 manopole,
// interruttori a levetta, pulsante a pedale, LED. Tutto disegnato in vettoriale.
namespace look
{
enum KnobStyle { chromeBig = 0, chromeMid, chromeDome, blackLarge, blackSmall, chicken, panel };

inline juce::Font font (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline void knurl (juce::Graphics& g, juce::Point<float> c, float r, int n, juce::Colour col)
{
    g.setColour (col);
    for (int i = 0; i < n; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n;
        const float s = std::sin (a), co = std::cos (a);
        g.drawLine (c.x + s * r * 0.84f, c.y - co * r * 0.84f, c.x + s * r * 0.99f, c.y - co * r * 0.99f, 1.1f);
    }
}

inline void shadow (juce::Graphics& g, juce::Point<float> c, float r)
{
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.35f), c.x + r * 0.18f, c.y + r * 0.25f,
                             juce::Colours::transparentBlack, c.x + r * 0.18f, c.y + r * 1.45f, true);
    g.setGradientFill (sh);
    g.fillEllipse (c.x - r * 1.25f + r * 0.18f, c.y - r * 1.25f + r * 0.25f, r * 2.5f, r * 2.5f);
}

inline void chromeCap (juce::Graphics& g, juce::Point<float> c, float r)
{
    juce::ColourGradient cg (juce::Colour (0xfff4f6f8), c.x - r * 0.45f, c.y - r * 0.55f,
                             juce::Colour (0xff7d838a), c.x + r * 0.6f, c.y + r * 0.7f, true);
    cg.addColour (0.45, juce::Colour (0xffc9ced3));
    cg.addColour (0.75, juce::Colour (0xff9aa1a8));
    g.setGradientFill (cg);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    // riflessi concentrici del metallo tornito
    for (int i = 1; i < 6; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        const float rr = r * (float) i / 6.0f;
        g.drawEllipse (c.x - rr, c.y - rr, 2 * rr, 2 * rr, 0.7f);
    }
    g.setColour (juce::Colour (0xff5a5f66));
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.0f);
}

inline void pointer (juce::Graphics& g, juce::Point<float> c, float a, float r0, float r1, float w, juce::Colour col)
{
    const float s = std::sin (a), co = std::cos (a);
    g.setColour (col);
    g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * r1, c.y - co * r1, w);
}

inline void fluted (juce::Graphics& g, juce::Point<float> c, float r, int lobes, juce::Colour top, juce::Colour edge)
{
    juce::Path p;
    const int steps = lobes * 8;
    for (int i = 0; i <= steps; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) steps;
        const float rr = r * (0.93f + 0.07f * std::cos ((float) lobes * a));
        const float px = c.x + std::sin (a) * rr, py = c.y - std::cos (a) * rr;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    p.closeSubPath();
    juce::ColourGradient gr (top.brighter (0.25f), c.x - r * 0.4f, c.y - r * 0.6f, edge, c.x + r * 0.5f, c.y + r * 0.8f, true);
    g.setGradientFill (gr);
    g.fillPath (p);
    g.setColour (juce::Colours::black);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    // piano superiore
    const float rt = r * 0.72f;
    juce::ColourGradient tg (top.brighter (0.35f), c.x - rt * 0.5f, c.y - rt * 0.7f, top.darker (0.4f), c.x + rt, c.y + rt, true);
    g.setGradientFill (tg);
    g.fillEllipse (c.x - rt, c.y - rt, 2 * rt, 2 * rt);
}

class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202024));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffd04a2b));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2f));
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff55555c));
        setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2e2e34));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd04a2b));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff55555c));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff2a2a2f));
        setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee202024));
        setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xff888888));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xee202024));
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    }

    juce::Font getPopupMenuFont() override { return font (14.0f, false); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f, false); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.5f, true); }
    void drawBubble (juce::Graphics& g, juce::BubbleComponent& b, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override
    {
        juce::LookAndFeel_V4::drawBubble (g, b, tip, body);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const int style = (int) s.getProperties().getWithDefault ("style", 0);
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        const float diam = (float) (double) s.getProperties().getWithDefault ("diam", 0.0);
        const float r = diam > 0.0f ? diam * 0.5f : juce::jmin ((float) w, (float) h) * 0.5f - 3.0f;
        const float a = a0 + pos * (a1 - a0);
        const bool learn = (bool) s.getProperties().getWithDefault ("learn", false);

        switch (style)
        {
            case chromeBig:
            case chromeMid:
            {
                shadow (g, c, r);
                g.setColour (juce::Colour (0xff141414));
                g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
                knurl (g, c, r, style == chromeBig ? 40 : 32, juce::Colour (0xff3a3a3a));
                const float rc = r * 0.64f;
                chromeCap (g, c, rc);
                pointer (g, c, a, rc * 0.25f, rc * 0.92f, 2.2f, juce::Colour (0xff1a1a1a));
                pointer (g, c, a, r * 0.86f, r * 0.99f, 2.0f, juce::Colours::white.withAlpha (0.85f));
                break;
            }
            case chromeDome:
            {
                shadow (g, c, r);
                chromeCap (g, c, r);
                g.setColour (juce::Colour (0xff6c7178));
                g.drawEllipse (c.x - r * 0.98f, c.y - r * 0.98f, r * 1.96f, r * 1.96f, 2.0f);
                pointer (g, c, a, r * 0.2f, r * 0.9f, 2.6f, juce::Colour (0xff1c1c1c));
                break;
            }
            case blackLarge:
            case blackSmall:
            {
                shadow (g, c, r);
                fluted (g, c, r, style == blackLarge ? 14 : 10, juce::Colour (0xff2b2b2b), juce::Colour (0xff050505));
                pointer (g, c, a, r * 0.18f, r * 0.88f, style == blackLarge ? 3.0f : 2.4f, juce::Colours::white);
                break;
            }
            case chicken:
            {
                shadow (g, c, r);
                g.setColour (juce::Colour (0xff111111));
                g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
                juce::Path p;
                p.startNewSubPath (0.0f, -r);
                p.quadraticTo (r * 0.26f, -r * 0.5f, r * 0.42f, r * 0.25f);
                p.quadraticTo (0.0f, r * 0.62f, -r * 0.42f, r * 0.25f);
                p.quadraticTo (-r * 0.26f, -r * 0.5f, 0.0f, -r);
                p.closeSubPath();
                p.applyTransform (juce::AffineTransform::rotation (a).translated (c.x, c.y));
                juce::ColourGradient cg (juce::Colour (0xff4a4a4a), c.x - r * 0.3f, c.y - r * 0.6f,
                                         juce::Colour (0xff080808), c.x + r * 0.5f, c.y + r * 0.6f, true);
                g.setGradientFill (cg);
                g.fillPath (p);
                g.setColour (juce::Colours::black);
                g.strokePath (p, juce::PathStrokeType (1.0f));
                pointer (g, c, a, r * 0.45f, r * 0.93f, 2.2f, juce::Colours::white);
                break;
            }
            default: // panel (pannello posteriore)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
                g.setColour (juce::Colour (0xff3a3a40));
                g.strokePath (arc, juce::PathStrokeType (3.0f));
                const bool bipolar = s.getMinimum() < 0.0;
                const float aMid = bipolar ? (a0 + a1) * 0.5f : a0;
                juce::Path val;
                val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (aMid, a), juce::jmax (aMid, a), true);
                g.setColour (juce::Colour (0xffd04a2b));
                g.strokePath (val, juce::PathStrokeType (3.0f));
                const float rk = r * 0.74f;
                juce::ColourGradient kg (juce::Colour (0xff55555c), c.x - rk * 0.5f, c.y - rk * 0.6f,
                                         juce::Colour (0xff16161a), c.x + rk * 0.6f, c.y + rk * 0.8f, true);
                g.setGradientFill (kg);
                g.fillEllipse (c.x - rk, c.y - rk, 2 * rk, 2 * rk);
                pointer (g, c, a, rk * 0.3f, rk * 0.95f, 2.2f, juce::Colours::white);
                break;
            }
        }
        if (learn)
        {
            g.setColour (juce::Colour (0xff3fa9ff));
            g.drawEllipse (c.x - r - 3.0f, c.y - r - 3.0f, 2 * r + 6.0f, 2 * r + 6.0f, 2.5f);
        }
    }
};
} // namespace look
