#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "ui/OniriquePresets.h"

//==============================================================================
//  Presets d'usine du N73, rangés par usage pour le navigateur de la barre du haut (les tags). Ce sont des points de
//  départ, pas des réglages « du 1073 ». Valeurs réelles ; un réglage non cité reprend sa valeur par défaut.
//  Index du commutateur : LINE +20 = 0, +15 = 1, +10 = 2, +5 = 3, 0 = 4 … MIC 20 = 8, MIC 25 = 9, MIC 30 = 10 …
//  midFreq : 0 OFF, 1 0,36, 2 0,7, 3 1,6, 4 3,2, 5 4,8, 6 7,2 kHz ; lowFreq : 0 OFF, 1 35, 2 60, 3 110, 4 220 Hz ;
//  hpf : 0 OFF, 1 50, 2 80, 3 160, 4 300 Hz.
//  Niveaux pensés pour une source vers -18 dBFS avec la calibration par défaut (0 dBFS = +26 dBu) : le gain règle le
//  grain, OUTPUT ramène le volume.
//==============================================================================
namespace n73presets
{
enum Tags { vocal = 1, drums = 2, bass = 4, instruments = 8, mix = 16, drive = 32 };

inline juce::StringArray tagNames() { return { "Vocal", "Drums", "Bass", "Instruments", "Mix", "Drive" }; }

inline std::vector<onirique::PresetBank::Stock> stockPresets()
{
    using namespace params;
    auto n = [] (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); };
    return {
        { "INIT", 0, {} },
        { n ("VOCAL \xc2\xb7 WARM LEAD"), vocal,
          { { gain, 1.0f }, { hpf, 2.0f }, { lowFreq, 3.0f }, { low, -2.0f }, { midFreq, 4.0f }, { mid, 3.0f }, { high, 4.0f }, { output, -15.0f } } },
        { n ("VOCAL \xc2\xb7 AIRY POP"), vocal,
          { { gain, 3.0f }, { hpf, 2.0f }, { lowFreq, 3.0f }, { low, -2.0f }, { midFreq, 5.0f }, { mid, 2.0f }, { high, 5.0f }, { output, -5.0f } } },
        { n ("VOCAL \xc2\xb7 RAP PRESENCE"), vocal | drive,
          { { gain, 1.0f }, { hpf, 2.0f }, { lowFreq, 2.0f }, { low, 2.0f }, { midFreq, 4.0f }, { mid, 4.0f }, { high, 3.0f }, { output, -15.0f } } },
        { n ("VOCAL \xc2\xb7 DRIVEN"), vocal | drive, { { gain, 9.0f }, { hpf, 2.0f }, { high, 3.0f }, { output, -24.0f } } },
        { n ("VOCAL \xc2\xb7 RADIO GRIT"), vocal | drive,
          { { gain, 10.0f }, { hpf, 4.0f }, { midFreq, 4.0f }, { mid, 6.0f }, { high, -4.0f }, { output, -24.0f } } },
        { n ("DRUMS \xc2\xb7 BUS LINE"), drums | mix, { { gain, 2.0f }, { lowFreq, 2.0f }, { low, 3.0f }, { high, 4.0f }, { output, -10.0f } } },
        { n ("DRUMS \xc2\xb7 KICK WEIGHT"), drums,
          { { gain, 2.0f }, { lowFreq, 2.0f }, { low, 6.0f }, { midFreq, 2.0f }, { mid, -4.0f }, { high, 2.0f }, { output, -10.0f } } },
        { n ("DRUMS \xc2\xb7 SNARE CRACK"), drums | drive,
          { { gain, 1.0f }, { hpf, 2.0f }, { midFreq, 3.0f }, { mid, 3.0f }, { high, 5.0f }, { output, -15.0f } } },
        { n ("DRUMS \xc2\xb7 ROOM SMASH"), drums | drive, { { gain, 8.0f }, { lowFreq, 3.0f }, { low, 3.0f }, { high, 4.0f }, { output, -20.0f } } },
        { n ("BASS \xc2\xb7 GRIT"), bass | drive,
          { { gain, 8.0f }, { lowFreq, 2.0f }, { low, 4.0f }, { midFreq, 2.0f }, { mid, 2.0f }, { output, -20.0f } } },
        { n ("BASS \xc2\xb7 WARM DI"), bass,
          { { gain, 2.0f }, { lowFreq, 2.0f }, { low, 3.0f }, { midFreq, 2.0f }, { mid, 2.0f }, { high, -3.0f }, { output, -10.0f } } },
        { n ("GUITAR \xc2\xb7 CRUNCH"), instruments | drive,
          { { gain, 9.0f }, { hpf, 2.0f }, { midFreq, 4.0f }, { mid, 3.0f }, { high, 2.0f }, { output, -24.0f } } },
        { n ("ACOUSTIC \xc2\xb7 AIR"), instruments,
          { { gain, 3.0f }, { hpf, 2.0f }, { midFreq, 3.0f }, { mid, -2.0f }, { high, 5.0f }, { output, -5.0f } } },
        { n ("KEYS \xc2\xb7 WARMTH"), instruments,
          { { gain, 2.0f }, { lowFreq, 3.0f }, { low, 2.0f }, { midFreq, 5.0f }, { mid, -2.0f }, { high, 3.0f }, { output, -10.0f } } },
        { n ("MIX \xc2\xb7 GLUE COLOUR"), mix, { { gain, 3.0f }, { lowFreq, 1.0f }, { low, 2.0f }, { high, 2.0f }, { output, -5.0f } } },
        { n ("MIX \xc2\xb7 CONSOLE TONE"), mix, { { gain, 2.0f }, { output, -10.0f } } },
        { n ("LINE \xc2\xb7 CLEAN"), mix, { { gain, 4.0f } } },
    };
}

/** Les presets enregistrés avec l'ancien en-tête (fichiers .n73preset du même dossier) passent dans le navigateur,
    une fois : chacun est recopié au format de la nouvelle barre, l'original reste en place. */
inline void importOldUserPresets (const onirique::PresetBank& bank)
{
    for (const auto& file : bank.getFolder().findChildFiles (juce::File::findFiles, false, "*.n73preset"))
    {
        const auto target = file.withFileExtension (bank.getExtension());
        if (target.existsAsFile())
            continue;

        if (const auto xml = juce::XmlDocument::parse (file); xml != nullptr && xml->hasTagName ("N73Preset"))
        {
            juce::ValueTree snapshot ("Snapshot");
            for (auto* p : xml->getChildWithTagNameIterator ("Param"))
                if (const auto id = p->getStringAttribute ("id"); ! params::notStored().contains (id))
                    snapshot.setProperty (id, (float) p->getDoubleAttribute ("value"), nullptr);

            if (const auto out = snapshot.createXml())
                out->writeTo (target);
        }
    }
}
} // namespace n73presets
