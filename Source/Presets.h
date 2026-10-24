#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <vector>
#include "Params.h"

//==============================================================================
//  Presets : usine (ici) + utilisateur (fichiers .n73preset dans %APPDATA%/Onirique/N73/Presets).
//  Un preset contient les paramètres du son ; jamais CAL ni la qualité (réglages de session).
//  Les presets d'usine sont des points de départ, pas des réglages « du 1073 ».
//==============================================================================
struct N73Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // valeurs réelles (dB, index de choix, 0 / 1)
};

inline const std::vector<N73Preset>& factoryPresets()
{
    // index du commutateur : LINE +20 = 0 … LINE 0 = 4 … MIC 20 = 8 … MIC 50 = 14, MIC 55 = 16 … MIC 80 = 21
    static const std::vector<N73Preset> list = {
        { "INIT", {} },
        { "VOCAL · WARM LEAD", { { params::gain, 18.0f }, { params::hpf, 2.0f }, { params::lowFreq, 3.0f }, { params::low, -2.0f },
                                 { params::midFreq, 4.0f }, { params::mid, 3.0f }, { params::high, 4.0f }, { params::output, -5.0f } } },
        { "VOCAL · DRIVEN",    { { params::gain, 20.0f }, { params::hpf, 2.0f }, { params::high, 3.0f }, { params::output, -15.0f } } },
        { "BASS · GRIT",       { { params::gain, 19.0f }, { params::lowFreq, 2.0f }, { params::low, 4.0f }, { params::midFreq, 2.0f },
                                 { params::mid, 2.0f }, { params::output, -10.0f } } },
        { "DRUMS · BUS LINE",  { { params::gain, 2.0f }, { params::lowFreq, 2.0f }, { params::low, 3.0f }, { params::high, 4.0f },
                                 { params::output, -10.0f } } },
        { "ACOUSTIC · AIR",    { { params::gain, 17.0f }, { params::hpf, 2.0f }, { params::midFreq, 3.0f }, { params::mid, -2.0f }, { params::high, 5.0f } } },
        { "LINE · CLEAN",      { { params::gain, 4.0f } } },
    };
    return list;
}

class N73PresetManager
{
public:
    explicit N73PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    static juce::File folder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Onirique").getChildFile ("N73").getChildFile ("Presets");
    }

    juce::Array<juce::File> userPresets() const
    {
        auto files = folder().findChildFiles (juce::File::findFiles, false, "*.n73preset");
        files.sort();
        return files;
    }

    juce::String currentName() const { return apvts.state.getProperty ("presetName", "INIT").toString(); }

    /** valeurs de tous les paramètres du son (valeurs réelles). */
    std::map<juce::String, float> capture() const
    {
        std::map<juce::String, float> m;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (params::isSoundParameter (rp->getParameterID()))
                    m[rp->getParameterID()] = rp->convertFrom0to1 (rp->getValue());
        return m;
    }

    /** applique des valeurs ; un paramètre absent reprend sa valeur par défaut. */
    void apply (const std::map<juce::String, float>& values)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto id = rp->getParameterID();
                if (! params::isSoundParameter (id)) continue;
                const auto it = values.find (id);
                const float v01 = it != values.end() ? rp->convertTo0to1 (it->second) : rp->getDefaultValue();
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (v01);
                rp->endChangeGesture();
            }
    }

    void loadFactory (int index)
    {
        const auto& p = factoryPresets()[(size_t) index];
        std::map<juce::String, float> m (p.values.begin(), p.values.end());
        apply (m);
        apvts.state.setProperty ("presetName", p.name, nullptr);
    }

    bool loadFile (const juce::File& f)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("N73Preset")) return false;
        std::map<juce::String, float> m;
        for (auto* e : xml->getChildWithTagNameIterator ("Param"))
            m[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        apply (m);
        apvts.state.setProperty ("presetName", f.getFileNameWithoutExtension(), nullptr);
        return true;
    }

    bool save (const juce::String& name)
    {
        const auto safe = juce::File::createLegalFileName (name.trim());
        if (safe.isEmpty()) return false;
        folder().createDirectory();
        juce::XmlElement root ("N73Preset");
        root.setAttribute ("version", 1);
        for (const auto& [id, v] : capture())
        {
            auto* e = root.createNewChildElement ("Param");
            e->setAttribute ("id", id);
            e->setAttribute ("value", (double) v);
        }
        const bool ok = root.writeTo (folder().getChildFile (safe + ".n73preset"));
        if (ok) apvts.state.setProperty ("presetName", safe, nullptr);
        return ok;
    }

    /** suivant / précédent dans la liste usine puis utilisateur. */
    void step (int delta)
    {
        const auto users = userPresets();
        const int total = (int) factoryPresets().size() + users.size();
        int cur = 0;
        const auto name = currentName();
        for (int i = 0; i < (int) factoryPresets().size(); ++i) if (factoryPresets()[(size_t) i].name == name) cur = i;
        for (int i = 0; i < users.size(); ++i) if (users[i].getFileNameWithoutExtension() == name) cur = (int) factoryPresets().size() + i;
        const int next = ((cur + delta) % total + total) % total;
        if (next < (int) factoryPresets().size()) loadFactory (next);
        else loadFile (users[next - (int) factoryPresets().size()]);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};
