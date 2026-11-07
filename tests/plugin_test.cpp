// plugin_test.cpp - prova del processore (blocchi audio, MIDI CC, stato) e screenshot dell'editor
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    auto* rp = dynamic_cast<RisaccaAudioProcessor*> (p.get());
    const double sr = 48000.0; const int bs = 256;
    p->setPlayConfigDetails (2, 2, sr, bs);
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    p->prepareToPlay (sr, bs);
    std::printf ("prepareToPlay: %.0f ms, latenza %d campioni\n", juce::Time::getMillisecondCounterHiRes() - t0, p->getLatencySamples());

    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    double peak = 0, sum = 0; long cnt = 0;
    t0 = juce::Time::getMillisecondCounterHiRes();
    const int blocks = (int) (2.0 * sr / bs);
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < bs; ++i)
        {
            const double t = (b * bs + i) / sr;
            const float x = (float) (0.25 * std::sin (2 * juce::MathConstants<double>::pi * 110.0 * t) * std::exp (-2.0 * std::fmod (t, 1.0)));
            buf.setSample (0, i, x); buf.setSample (1, i, x);
        }
        midi.clear();
        if (b == blocks / 2) midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 20), 0);   // Noisegate via KeyLab
        p->processBlock (buf, midi);
        for (int i = 0; i < bs; ++i) { const double y = buf.getSample (0, i); peak = std::max (peak, std::abs (y)); sum += y * y; ++cnt; }
    }
    const double el = juce::Time::getMillisecondCounterHiRes() - t0;
    std::printf ("2 s audio in %.0f ms (%.2fx tempo reale), picco %.3f rms %.3f, noisegate=%.3f\n", el, 2000.0 / el, peak,
                 std::sqrt (sum / cnt), rp->apvts.getRawParameterValue ("noisegate")->load());

    juce::MemoryBlock st; p->getStateInformation (st);
    p->setStateInformation (st.getData(), (int) st.getSize());
    std::printf ("stato: %zu byte\n", st.getSize());

    if (argc > 1)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        ed->setSize (860, 540);
        for (int pass = 0; pass < 2; ++pass)
        {
            if (pass == 1)
                for (auto* c : ed->getChildren())
                    if (auto* face = dynamic_cast<PedalFace*> (c)) { face->showBackPanel (true); face->tick(); }

            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::String (argv[1]) + (pass == 0 ? "_front.png" : "_back.png"));
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("screenshot: %s\n", f.getFullPathName().toRawUTF8());
        }
        // dettaglio ingrandito: levetta spenta/accesa, footswitch a riposo/premuto
        {
            auto* rp = dynamic_cast<RisaccaAudioProcessor*> (p.get());
            juce::Image img (juce::Image::ARGB, 4 * 200, 220, true);
            juce::Graphics g (img);
            g.fillAll (juce::Colour (0xffe8e6df));
            for (int i = 0; i < 4; ++i)
            {
                const bool foot = i >= 2;
                PedalToggle t (*rp, foot ? pid::engaged : pid::preclip,
                               foot ? PedalToggle::footswitch : PedalToggle::toggle);
                if (foot) t.setBounds (0, 0, 66, 66); else t.setBounds (0, 0, 40, 62);
                if (! foot) t.setToggleState (i == 1, juce::dontSendNotification);
                juce::Graphics::ScopedSaveState ss (g);
                g.addTransform (juce::AffineTransform::scale (3.0f).translated ((float) (i * 200 + (foot ? 1 : 40)), 14.0f));
                t.paintButton (g, false, foot && i == 3);
            }
            juce::File f (juce::String (argv[1]) + "_detail.png");
            f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        }
    }
    p->releaseResources();
    return 0;
}
