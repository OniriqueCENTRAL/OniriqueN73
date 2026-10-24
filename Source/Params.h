#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "N73Engine.h"

//==============================================================================
//  Paramètres du N73 : identifiants stables (ne jamais les renommer, les projets les retrouvent par là).
//
//  Façade du 1073 : gain (LINE et MIC, chacun sa plage, comme sur le plugin UAD), MIC Z, HIGH, MID (fréquence + gain),
//  LOW (fréquence + gain), HIGH PASS, EQL, PHASE.
//  Présentation UA : PAD (entrée micro), fader de console (LEVEL : avant l'ampli de sortie, au-dessus de 0 dB il le
//  fait saturer), OUTPUT (volume propre après le module), POWER.
//  Réglages modernes (roue crantée) : CAL, LEVEL MATCH, qualité.
//==============================================================================
namespace params
{
inline const juce::String gain = "gain", micZ = "micZ", pad = "pad", high = "high", midFreq = "midFreq", mid = "mid",
                          lowFreq = "lowFreq", low = "low", hpf = "hpf", eql = "eql", phase = "phase", output = "output",
                          fader = "fader", power = "power", cal = "cal", levelMatch = "levelMatch", quality = "quality";

inline juce::StringArray gainNames()
{
    juce::StringArray s;
    for (const auto& gp : n73::gainPositions)
        s.add (gp.kind == n73::GainPosition::Off ? juce::String ("OFF")
             : gp.kind == n73::GainPosition::Mic ? "MIC " + juce::String (gp.gainDb) + " dB"
                                                  : "LINE " + juce::String (gp.gainDb > 0 ? "+" : "") + juce::String (gp.gainDb) + " dB");
    return s;
}

inline juce::String dbText (float v, int decimals = 1)
{
    if (std::abs (v) < 0.05f) v = 0.0f;
    return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + " dB";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const String& id, const String& name, const StringArray& items, int def, bool automatable = true)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def,
                   AudioParameterChoiceAttributes().withAutomatable (automatable)));
    };
    auto toggle = [&] (const String& id, const String& name, bool def, const String& on, const String& off, bool automatable = true)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def,
                   AudioParameterBoolAttributes().withAutomatable (automatable)
                       .withStringFromValueFunction ([on, off] (bool b, int) { return b ? on : off; })));
    };
    auto knob = [&] (const String& id, const String& name, float lo, float hi, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f), def,
                   AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return dbText (v); })
                       .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("+-.0123456789").getFloatValue(); })));
    };

    // build local de démo (jamais publié) : réglages de départ parlants pour le visuel de la boutique
   #if ONIRIQUE_UI_TEST
    const bool demo = SystemStats::getEnvironmentVariable ("ONIRIQUE_UI_TEST", {}).contains ("demo");
   #else
    const bool demo = false;
   #endif
    choice (gain, "Gain", gainNames(), demo ? 9 : 4);   // LINE 0 : gain unité
    choice (micZ, "Mic Z", { "LO 300", "HI 1200" }, 1);
    toggle (pad, "Pad", false, "-20 dB", "0 dB");
    knob (high, "High 12k", -16.0f, 16.0f, demo ? 4.0f : 0.0f);
    choice (midFreq, "Mid Freq", { "OFF", "0.36 kHz", "0.7 kHz", "1.6 kHz", "3.2 kHz", "4.8 kHz", "7.2 kHz" }, demo ? 4 : 0);
    knob (mid, "Mid", -18.0f, 18.0f, demo ? 3.0f : 0.0f);
    choice (lowFreq, "Low Freq", { "OFF", "35 Hz", "60 Hz", "110 Hz", "220 Hz" }, demo ? 3 : 0);
    knob (low, "Low", -16.0f, 16.0f, demo ? -2.0f : 0.0f);
    choice (hpf, "High Pass", { "OFF", "50 Hz", "80 Hz", "160 Hz", "300 Hz" }, demo ? 2 : 0);
    toggle (eql, "EQL", true, "IN", "OUT");
    toggle (phase, "Phase", false, "180", "0");
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
               AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return dbText (v); })));
    {
        // fader de console : -80 = ∞ ; la moitié haute de la course couvre -10 … +10 dB
        NormalisableRange<float> r (-80.0f, 10.0f, 0.01f);
        r.setSkewForCentre (-12.0f);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { fader, 1 }, "Fader", r, 0.0f,
                   AudioParameterFloatAttributes().withLabel ("dB")
                       .withStringFromValueFunction ([] (float v, int) { return v <= -79.9f ? String ("-inf") : dbText (v); })));
    }
    toggle (power, "Power", true, "ON", "OFF");
    // +26 dBu : l'alignement des plugins UAD, par défaut (les anciens projets gardent leur index)
    choice (cal, "Calibration", { "0 dBFS = +18 dBu", "0 dBFS = +20 dBu", "0 dBFS = +22 dBu", "0 dBFS = +24 dBu", "0 dBFS = +26 dBu" }, 4, false);
    toggle (levelMatch, "Level Match", false, "ON", "OFF");
    choice (quality, "Quality", { "Normal", "High" }, 0, false);
    return l;
}

/** Lecture des paramètres (thread audio : seulement des chargements atomiques). */
struct Reader
{
    explicit Reader (juce::AudioProcessorValueTreeState& s)
    {
        auto get = [&s] (const juce::String& id) { return s.getRawParameterValue (id); };
        gain = get (params::gain); micZ = get (params::micZ); pad = get (params::pad); high = get (params::high);
        midFreq = get (params::midFreq); mid = get (params::mid); lowFreq = get (params::lowFreq); low = get (params::low);
        hpf = get (params::hpf); eql = get (params::eql); phase = get (params::phase); output = get (params::output);
        fader = get (params::fader); power = get (params::power); cal = get (params::cal); levelMatch = get (params::levelMatch);
        quality = get (params::quality);
    }

    n73::EngineParams read() const
    {
        n73::EngineParams p;
        auto& m = p.module;
        m.gainIndex = juce::jlimit (0, n73::numGainPositions - 1, (int) gain->load());
        m.micZHigh = micZ->load() > 0.5f;
        m.pad = pad->load() > 0.5f;
        m.highPos = n73::eq::potFromKnob (high->load() / 16.0, n73::eq::hfZero);
        m.midPos = n73::eq::potFromKnob (mid->load() / 18.0, n73::eq::midZero);
        m.lowPos = n73::eq::potFromKnob (low->load() / 16.0, n73::eq::lfZero);
        m.midFreq = juce::jlimit (0, 6, (int) midFreq->load());
        m.lowFreq = juce::jlimit (0, 4, (int) lowFreq->load());
        m.hpf = juce::jlimit (0, 4, (int) hpf->load());
        m.eqIn = eql->load() > 0.5f;
        m.phase = phase->load() > 0.5f;
        m.levelDb = fader->load();         // fader : avant l'ampli de sortie
        p.outputDb = output->load();       // OUTPUT : après le module (volume propre)
        m.calDbu = 18.0 + 2.0 * juce::jlimit (0, 4, (int) cal->load());
        p.levelMatch = levelMatch->load() > 0.5f;
        p.power = power->load() > 0.5f;
        p.highQuality = quality->load() > 0.5f;
        return p;
    }

    std::atomic<float>* gain {}, *micZ {}, *pad {}, *high {}, *midFreq {}, *mid {}, *lowFreq {}, *low {}, *hpf {}, *eql {}, *phase {},
                       *output {}, *fader {}, *power {}, *cal {}, *levelMatch {}, *quality {};
};

/** Paramètres d'un preset ou d'une mémoire A / B (pas les réglages de session : CAL, qualité). */
inline bool isSoundParameter (const juce::String& id) { return id != cal && id != quality; }
} // namespace params
