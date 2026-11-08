// plugin_test.cpp - prova del processore (blocchi audio, MIDI CC, stato) e screenshot dell'editor
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && juce::String (argv[1]) == "scenario")
    {
        // uso reale: parametri cambiati durante il suono, chitarra realistica sul canale 1
        const double amp = argc > 2 ? std::atof (argv[2]) : 0.05;
        std::unique_ptr<juce::AudioProcessor> q (createPluginFilter());
        auto* rq = dynamic_cast<RisaccaAudioProcessor*> (q.get());
        q->setPlayConfigDetails (2, 2, 48000.0, 256);
        q->prepareToPlay (48000.0, 256);
        auto set = [&] (const char* id, float v) { rq->apvts.getParameter (id)->setValueNotifyingHost (v); };
        juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
        double t = 0;
        auto run = [&] (const char* label, double secs, bool guitar)
        {
            double sum = 0; long n = 0; double pk = 0;
            for (int blk = 0; blk < (int) (secs * 48000 / 256); ++blk)
            {
                for (int i = 0; i < 256; ++i, t += 1.0 / 48000.0)
                {
                    const double tt = std::fmod (t, 0.5), ph = std::fmod (tt * 146.8, 1.0);
                    const double x = guitar ? amp * std::exp (-tt * 4.0) * (std::sin (6.2831853 * ph) + 0.3 * std::sin (12.566 * ph)) : 0.0;
                    b.setSample (0, i, (float) x); b.setSample (1, i, 0.0f);
                }
                q->processBlock (b, m);
                for (int i = 0; i < 256; ++i) { const double y = b.getSample (0, i); sum += y * y; ++n; pk = std::max (pk, std::abs (y)); }
            }
            std::printf ("%-44s rms %6.1f dBFS  picco %.3f\n", label, 20 * std::log10 (std::sqrt (sum / n) + 1e-12), pk);
        };
        run ("di serie, silenzio", 2, false);
        run ("di serie, chitarra", 3, true);
        set ("selfOscSw", 1); run ("self-osc su, silenzio", 3, false);
        run ("self-osc su, chitarra", 3, true);
        set ("selfOscSw", 0); run ("self-osc giu, chitarra (subito)", 3, true);
        run ("self-osc giu, chitarra (dopo 3 s)", 3, true);
        run ("self-osc giu, silenzio", 2, false);
        // griglia Noisegate x Swash con self-osc giu
        for (float g : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            std::printf ("noisegate %.2f:", g);
            for (float sw : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                set ("noisegate", g); set ("swash", sw);
                double sum = 0; long n = 0;
                for (int blk = 0; blk < (int) (1.5 * 48000 / 256); ++blk)
                {
                    for (int i = 0; i < 256; ++i, t += 1.0 / 48000.0)
                    {
                        const double tt = std::fmod (t, 0.5), ph = std::fmod (tt * 146.8, 1.0);
                        b.setSample (0, i, (float) (amp * std::exp (-tt * 4.0) * std::sin (6.2831853 * ph))); b.setSample (1, i, 0.0f);
                    }
                    q->processBlock (b, m);
                    if (blk > 90) for (int i = 0; i < 256; ++i) { sum += b.getSample (0, i) * b.getSample (0, i); ++n; }
                }
                std::printf ("  sw%.2f %6.1f", sw, 20 * std::log10 (std::sqrt (sum / n) + 1e-12));
            }
            std::printf ("\n");
        }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "input")
    {
        // chitarra solo sul canale 1 (come un ingresso mono della scheda audio), parametri di serie
        for (double amp : { 0.0, 0.01, 0.03, 0.1, 0.3 })
        {
            std::unique_ptr<juce::AudioProcessor> q (createPluginFilter());
            q->setPlayConfigDetails (2, 2, 48000.0, 256);
            q->prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
            double sum = 0; long n = 0;
            for (int blk = 0; blk < (int) (3.0 * 48000 / 256); ++blk)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const double t = (blk * 256 + i) / 48000.0, tt = std::fmod (t, 1.0);
                    const double ph = std::fmod (tt * 110.0, 1.0);
                    b.setSample (0, i, (float) (amp * std::exp (-tt * 3.0) * (2.0 * ph - 1.0)));
                    b.setSample (1, i, 0.0f);
                }
                q->processBlock (b, m);
                if (blk * 256 > 48000) for (int i = 0; i < 256; ++i) { sum += b.getSample (0, i) * b.getSample (0, i); ++n; }
            }
            std::printf ("ingresso picco %.3f (%.0f dBFS): uscita rms %.4f (%.1f dBFS)\n", amp, 20 * std::log10 (std::max (amp, 1e-9)),
                         std::sqrt (sum / n), 20 * std::log10 (std::sqrt (sum / n) + 1e-12));
        }
        return 0;
    }
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

            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, argc > 2 ? (float) std::atof (argv[2]) : 1.0f);
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
