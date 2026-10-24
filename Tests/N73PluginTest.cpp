// Tests fonctionnels du plugin (processeur réel, sans hôte) : état, A / B, presets, automation, bypass, mono, latence.
#include "PluginProcessor.h"
#include <iostream>
#include <random>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok     " : "  ÉCHEC  ") << what << std::endl;
    if (! ok) ++failures;
}

std::unique_ptr<N73Processor> make (int channels = 2)
{
    std::unique_ptr<N73Processor> p (dynamic_cast<N73Processor*> (createPluginFilter()));
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout l; l.inputBuses.add (set); l.outputBuses.add (set);
    p->setBusesLayout (l);
    p->prepareToPlay (48000.0, 256);
    return p;
}

void setReal (N73Processor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
float real (N73Processor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::cout << "== Tests fonctionnels du plugin ==" << std::endl;

    // état : réglages → sauvegarde → nouvelle instance → restauration
    {
        auto a = make();
        setReal (*a, params::gain, 19); setReal (*a, params::high, 5.5f); setReal (*a, params::midFreq, 4); setReal (*a, params::mid, -7);
        setReal (*a, params::lowFreq, 2); setReal (*a, params::low, 3); setReal (*a, params::hpf, 3); setReal (*a, params::output, -8);
        setReal (*a, params::fader, -3); setReal (*a, params::micZ, 0); setReal (*a, params::pad, 1); setReal (*a, params::cal, 2);
        juce::MemoryBlock mb; a->getStateInformation (mb);
        auto b = make();
        b->setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = true;
        for (auto* prm : a->getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (std::abs (rp->getValue() - b->apvts.getParameter (rp->getParameterID())->getValue()) > 1.0e-5f)
                { same = false; std::cout << "    différent : " << rp->getParameterID() << std::endl; }
        check (same, "sauvegarde puis restauration de tous les paramètres");
    }

    // A / B
    {
        auto p = make();
        setReal (*p, params::high, 6);
        p->selectSlot (1);
        setReal (*p, params::high, -6);
        p->selectSlot (0);
        const bool backA = std::abs (real (*p, params::high) - 6.0f) < 0.02f;
        p->selectSlot (1);
        const bool backB = std::abs (real (*p, params::high) + 6.0f) < 0.02f;
        check (backA && backB, "A / B : chaque mémoire retrouve ses réglages");
        p->copyActiveToOther(); p->selectSlot (0);
        check (std::abs (real (*p, params::high) + 6.0f) < 0.02f, "A / B : copie vers l'autre mémoire");
    }

    // presets d'usine : chargement et traitement
    {
        auto p = make();
        juce::AudioBuffer<float> buf (2, 256); juce::MidiBuffer midi;
        bool ok = true;
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
        {
            p->presets.loadFactory (i);
            ok &= p->presets.currentName() == factoryPresets()[(size_t) i].name;
            for (int b = 0; b < 40; ++b)
            {
                for (int c = 0; c < 2; ++c) for (int s = 0; s < 256; ++s) buf.setSample (c, s, 0.3f * std::sin (0.05f * (float) (b * 256 + s)));
                p->processBlock (buf, midi);
                ok &= allFinite (buf);
            }
        }
        check (ok, "presets d'usine : chargement et traitement (" + juce::String ((int) factoryPresets().size()) + ")");
    }

    // automation au hasard pendant la lecture, blocs de tailles variables
    {
        auto p = make();
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        juce::MidiBuffer midi; bool ok = true; float peak = 0;
        for (int b = 0; b < 2000; ++b)
        {
            const int n = 1 + (int) (u (rng) * 255);
            juce::AudioBuffer<float> buf (2, n);
            for (auto* prm : p->getParameters()) if (u (rng) < 0.05f) prm->setValueNotifyingHost (u (rng));
            for (int c = 0; c < 2; ++c) for (int s = 0; s < n; ++s) buf.setSample (c, s, (u (rng) * 2 - 1) * 0.5f);
            p->processBlock (buf, midi);
            ok &= allFinite (buf);
            peak = std::max (peak, buf.getMagnitude (0, n));
        }
        check (ok, "automation de tous les paramètres pendant la lecture (pic " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS)");
    }

    // bypass de l'hôte : sortie = entrée retardée de la latence, sans saut
    {
        auto p = make();
        juce::MidiBuffer midi;
        const int lat = p->getLatencySamples();
        std::vector<float> in, out;
        for (int b = 0; b < 60; ++b)
        {
            juce::AudioBuffer<float> buf (2, 256);
            for (int s = 0; s < 256; ++s) { const float v = 0.25f * std::sin (0.031f * (float) (b * 256 + s)); buf.setSample (0, s, v); buf.setSample (1, s, v); in.push_back (v); }
            p->processBlockBypassed (buf, midi);
            for (int s = 0; s < 256; ++s) out.push_back (buf.getSample (0, s));
        }
        float err = 0;
        for (size_t i = 40 * 256; i < out.size(); ++i) err = std::max (err, std::abs (out[i] - in[i - (size_t) lat]));
        check (err < 1.0e-5f, "bypass : entrée retardée de " + juce::String (lat) + " échantillons (écart " + juce::String (err) + ")");
    }

    // mono
    {
        auto p = make (1);
        juce::AudioBuffer<float> buf (1, 256); juce::MidiBuffer midi; bool ok = true;
        for (int b = 0; b < 100; ++b)
        {
            for (int s = 0; s < 256; ++s) buf.setSample (0, s, 0.3f * std::sin (0.02f * (float) (b * 256 + s)));
            p->processBlock (buf, midi);
            ok &= allFinite (buf) && buf.getMagnitude (0, 256) > 0.01f;
        }
        check (ok, "mono : traitement");
    }

    // latence déclarée
    {
        auto p = make();
        check (p->getLatencySamples() == 4, "latence déclarée à 48 kHz : " + juce::String (p->getLatencySamples()) + " échantillons");
        p->prepareToPlay (96000.0, 256);
        check (p->getLatencySamples() == 0, "latence déclarée à 96 kHz : " + juce::String (p->getLatencySamples()) + " échantillon");
    }

    std::cout << (failures == 0 ? "TOUT EST BON" : juce::String (failures) + " échec(s)") << std::endl;
    return failures == 0 ? 0 : 1;
}
