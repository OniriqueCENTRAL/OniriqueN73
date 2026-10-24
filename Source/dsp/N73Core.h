#pragma once

#include "EqNetworks.h"
#include "AmpStage.h"
#include "OutputStage.h"
#include "Transformer.h"

namespace n73
{
//==============================================================================
//  Le commutateur de sensibilité (EK20033) : 22 positions, de la butée LINE −20 (gain +20) à la butée MIC −80.
//  L'ordre est celui de la rotation : LINE +20 … −10, OFF, MIC 20 … 50, OFF, MIC 55 … 80.
//==============================================================================
struct GainPosition
{
    enum Kind { Line, Off, Mic } kind;
    int gainDb;
};

constexpr int numGainPositions = 22;
constexpr GainPosition gainPositions[numGainPositions] = {
    { GainPosition::Line, 20 }, { GainPosition::Line, 15 }, { GainPosition::Line, 10 }, { GainPosition::Line, 5 },
    { GainPosition::Line, 0 },  { GainPosition::Line, -5 }, { GainPosition::Line, -10 }, { GainPosition::Off, 0 },
    { GainPosition::Mic, 20 },  { GainPosition::Mic, 25 },  { GainPosition::Mic, 30 },  { GainPosition::Mic, 35 },
    { GainPosition::Mic, 40 },  { GainPosition::Mic, 45 },  { GainPosition::Mic, 50 },  { GainPosition::Off, 0 },
    { GainPosition::Mic, 55 },  { GainPosition::Mic, 60 },  { GainPosition::Mic, 65 },  { GainPosition::Mic, 70 },
    { GainPosition::Mic, 75 },  { GainPosition::Mic, 80 },
};

/** Convention de niveau (moderne, documentée) : en MIC, le signal numérique est lu comme un signal de micro
    60 dB sous le niveau ligne — MIC 60 ≈ gain unité, au-delà le préampli est poussé. */
constexpr double micReferenceDb = 60.0;

//==============================================================================
/** Réglages d'un module, déjà convertis (positions de potentiomètres 0 … 1, 0 = accentuation maximale). */
struct ModuleConfig
{
    int gainIndex = 17;          // MIC 60
    bool micZHigh = true;        // 1200 Ω
    bool pad = false;
    double highPos = eq::hfZero, midPos = eq::midZero, lowPos = eq::lfZero;
    int midFreq = 0, lowFreq = 0, hpf = 0;   // 0 = OFF
    bool eqIn = true;
    bool phase = false;
    double outputDb = 0.0;
    double calDbu = 18.0;        // 0 dBFS = calDbu (sinus efficace)

    /** ce qui impose de reconstruire le module (fondu enchaîné) : tout sauf les potentiomètres et OUTPUT. */
    bool sameStructure (const ModuleConfig& o) const
    {
        return gainIndex == o.gainIndex && micZHigh == o.micZHigh && pad == o.pad && midFreq == o.midFreq
            && lowFreq == o.lowFreq && hpf == o.hpf && eqIn == o.eqIn && phase == o.phase && calDbu == o.calDbu;
    }
};

//==============================================================================
//  Valeurs du modèle (voir docs/MODELE.md pour l'origine de chacune)
//==============================================================================
namespace model
{
// BA283 NV : gain 1 + 2k2 / (390 ∥ 1k8 ∥ R_T) ; R_T = 330 Ω en position 45, 120 Ω en position 50 (EK20033)
inline double nvGain (double rBoost)
{
    double r = 390.0 * 1800.0 / (390.0 + 1800.0);
    if (rBoost > 0.0) r = r * rBoost / (r + rBoost);
    return 1.0 + 2200.0 / r;
}

constexpr double outGain = 9.87;        // BA283 AM + LO1166 (1:1,7), calé pour MIC 40 = 40 dB
constexpr double outZ = 75.0;           // impédance de sortie (fiche AMS)
constexpr double load = 600.0;          // charge (fiche AMS)

inline Transformer::Params micTransformer (bool high, int gainDb)
{
    Transformer::Params p;
    p.Rs = 150.0;                                             // micro de 150 Ω (hypothèse, documentée)
    p.RL = high ? (gainDb >= 55 ? 910.0 : 1300.0) : 300.0;   // impédances mesurées par SOS (1073N)
    p.Lm = high ? 10.0 : 2.5;                                 // primaires en série / en parallèle (÷4)
    p.lambdaS = high ? 0.06 : 0.03;                           // en parallèle, chaque enroulement voit toute la tension
    p.lambdaDC = 0.0;
    p.ratio = high ? 2.0 : 4.0;                               // 10468 : 1:2 (HI), 1:4 (LO)
    return p;
}

inline Transformer::Params lineTransformer()
{
    Transformer::Params p;
    p.Rs = 100.0; p.RL = 10000.0; p.Lm = 20.0; p.lambdaS = 0.12; p.lambdaDC = 0.0; p.ratio = 1.0;
    return p;
}

inline Transformer::Params outputTransformer()
{
    Transformer::Params p;
    p.Rs = outZ; p.RL = load; p.Lm = 15.0; p.lambdaS = 0.2; p.lambdaDC = 0.07; p.ratio = 1.0;
    return p;
}

inline AmpStage::Params nvStage (double gain)
{
    AmpStage::Params p;
    p.gain = gain;
    p.hd2 = 1.0e-4; p.hd3 = 2.0e-4; p.vRef = 10.0;
    p.vPos = 10.5; p.vNeg = 7.5;           // émetteur suiveur TR6 : côté bas limité par le courant dans la charge
    p.fEmph = 7000.0; p.emph = 3.0; p.fCouple = 1.0;
    return p;
}

inline AmpStage::Params eqAmp()
{
    AmpStage::Params p;
    p.gain = 1.0;
    p.hd2 = 1.0e-4; p.hd3 = 2.0e-4; p.vRef = 10.0;
    p.vPos = 10.5; p.vNeg = 7.5;
    p.fEmph = 7000.0; p.emph = 3.0; p.fCouple = 0.7;
    return p;
}
} // namespace model

//==============================================================================
/** Un canal du module, à la fréquence de travail (suréchantillonnée). Copiable tel quel (fondus enchaînés). */
class Channel
{
public:
    void prepare (double fs)
    {
        sampleRate = fs;
        t1.prepare (fs); t2.prepare (fs); nv.prepare (fs); eq1.prepare (fs); eq2.prepare (fs); out.prepare (fs);
        dc.setHighPass (1.0, fs);
        reset();
    }

    void reset()
    {
        t1.reset(); t2.reset(); nv.reset(); eq1.reset(); eq2.reset(); out.reset();
        dc.reset(); t1Hf.reset(); t2Hf.reset(); outHf.reset(); bump.reset();
        lfhf.reset(); mid.reset(); hpf.reset();
    }

    /** structure : appelée hors fondu, sur la copie inactive. */
    void configure (const ModuleConfig& c, double vpkFullScale)
    {
        const auto& gp = gainPositions[c.gainIndex];
        mic = gp.kind == GainPosition::Mic;
        off = gp.kind == GainPosition::Off;
        eqIn = c.eqIn; hpfOn = c.hpf > 0; phase = c.phase;
        vFs = vpkFullScale;
        inScale = (c.pad ? 0.1 : 1.0) * (mic ? std::pow (10.0, -micReferenceDb / 20.0) : 1.0);

        const double fs = sampleRate;
        double frontGain = 1.0, nvG = model::nvGain (0.0), g284 = 1.0;
        if (mic)
        {
            const auto tp = model::micTransformer (c.micZHigh, gp.gainDb);
            t1.setParams (tp);
            // le gain nominal est celui de la position HI (étiquettes du commutateur)
            const auto hi = model::micTransformer (true, gp.gainDb);
            frontGain = hi.ratio * hi.RL / (hi.RL + hi.Rs);
            if (gp.gainDb == 45) nvG = model::nvGain (330.0);
            if (gp.gainDb == 50) nvG = model::nvGain (120.0);
            t1Hf.setLowPass (60000.0, 1.08, fs, 20000.0);   // résonance du 10468 avec C1 180 p (+1,5 dB à 45 kHz, SOS)
        }
        else
        {
            const auto tp = model::lineTransformer();
            t2.setParams (tp);
            frontGain = tp.ratio * tp.RL / (tp.RL + tp.Rs);
            t2Hf.setLowPass (100000.0, 0.75, fs, 20000.0);
        }
        // gain nominal du reste de la chaîne (EQ à plat, chemin passe-haut, étage de sortie, charge)
        const double rest = nvG * eq::hpfFlatGain * model::outGain * model::load / (model::load + model::outZ);
        const double wanted = std::pow (10.0, gp.gainDb / 20.0);
        double trim = wanted / (frontGain * rest);
        if (mic && gp.gainDb >= 55) { g284 = trim; trim = 1.0; }   // BA284 : l'étage d'entrée en plus, au-delà de 50 dB
        preGain = g284 * trim;   // positions 20 à 35 : atténuateur avant la BA283 (trim < 1) ; ligne : atténuateur

        nv.setParams (model::nvStage (nvG));
        eq1.setParams (model::eqAmp());
        eq2.setParams (model::eqAmp());
        OutputStage::Params op;
        op.gain = model::outGain;
        op.load = model::load;
        op.xf = model::outputTransformer();
        out.setParams (op);
        bump.setHighPass (7.2, 0.9, fs, 15.0);            // bosse grave mesurée par SOS (+0,5 dB à 15 Hz, −3 dB à 6 Hz)
        outHf.setLowPass (120000.0, 0.7, fs, 20000.0);
        outLevel = std::pow (10.0, c.outputDb / 20.0);
    }

    void setOutputDb (double db) noexcept { outLevel = std::pow (10.0, db / 20.0); }

    inline double process (double x, const eq::SS& ssLfhf, const eq::SS& ssMid, const eq::SS& ssHpf) noexcept
    {
        double v = dc.process (x * vFs) * inScale;
        if (mic) v = t1Hf.process (t1.process (v));
        else     v = t2Hf.process (t2.process (v));
        v = nv.process (v * preGain);
        if (eqIn)
        {
            v = eq1.process (lfhf.process (ssLfhf, v));
            v = eq2.process (mid.process (ssMid, v));
        }
        v = hpfOn ? hpf.process (ssHpf, v) : v * eq::hpfFlatGain;
        v = out.process (v * outLevel);
        v = outHf.process (bump.process (v));
        if (phase) v = -v;
        if (off) v = 0.0;
        return v / vFs;
    }

    double drive() const noexcept { return out.lastDrive(); }

private:
    double sampleRate = 96000.0, vFs = 8.72, inScale = 1.0, preGain = 1.0, outLevel = 1.0;
    bool mic = true, off = false, eqIn = true, hpfOn = false, phase = false;
    Transformer t1, t2;
    AmpStage nv, eq1, eq2;
    OutputStage out;
    OnePole dc;
    Biquad t1Hf, t2Hf, outHf, bump;
    StateSpaceRunner<eq::MaxStates> lfhf, mid, hpf;
};

//==============================================================================
/** Un module complet : réseaux d'EQ discrétisés (partagés par les canaux) + deux canaux. Copiable. */
class Module
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double fs)
    {
        sampleRate = fs;
        for (auto& c : ch) c.prepare (fs);
        configure (cfg);
    }

    void reset() { for (auto& c : ch) c.reset(); }

    static double fullScaleVolts (double calDbu) { return 0.7745967 * std::sqrt (2.0) * std::pow (10.0, calDbu / 20.0); }

    void configure (const ModuleConfig& c)
    {
        cfg = c;
        for (auto& x : ch) x.configure (c, fullScaleVolts (c.calDbu));
        rebuildEq();
        rebuildHpf();
    }

    /** potentiomètres et OUTPUT : sans fondu (valeurs déjà lissées). */
    void setPots (double highPos, double midPos, double lowPos)
    {
        if (highPos == cfg.highPos && midPos == cfg.midPos && lowPos == cfg.lowPos) return;
        cfg.highPos = highPos; cfg.midPos = midPos; cfg.lowPos = lowPos;
        rebuildEq();
    }

    void setOutputDb (double db)
    {
        cfg.outputDb = db;
        for (auto& x : ch) x.setOutputDb (db);
    }

    inline double process (int channel, double x) noexcept { return ch[channel].process (x, ssLfhf, ssMid, ssHpf); }

    const ModuleConfig& config() const { return cfg; }
    double drive (int channel) const { return ch[channel].drive(); }

    // pour les mesures et l'affichage
    const eq::SS& lfhfNetwork() const { return ssLfhf; }
    const eq::SS& midNetwork() const { return ssMid; }
    const eq::SS& hpfNetwork() const { return ssHpf; }

private:
    void rebuildEq()
    {
        const double lowCap = cfg.lowFreq > 0 ? eq::lowCaps[cfg.lowFreq - 1] : 0.0;
        net.clear();
        eq::buildLfHf (net, cfg.highPos, cfg.lowPos, lowCap);
        net.discretize (sampleRate, 12000.0, ssLfhf);
        eq::buildMid (net, cfg.midPos, cfg.midFreq - 1);
        const double fMid = cfg.midFreq > 0 ? midCentres[cfg.midFreq - 1] : 1000.0;
        net.discretize (sampleRate, fMid, ssMid);
    }

    void rebuildHpf()
    {
        if (cfg.hpf <= 0) return;
        eq::buildHpf (net, cfg.hpf - 1);
        net.discretize (sampleRate, hpfCorners[cfg.hpf - 1], ssHpf);
    }

    static constexpr double midCentres[6] = { 360.0, 700.0, 1600.0, 3200.0, 4800.0, 7200.0 };
    static constexpr double hpfCorners[4] = { 50.0, 80.0, 160.0, 300.0 };

    double sampleRate = 96000.0;
    ModuleConfig cfg;
    eq::Net net;
    eq::SS ssLfhf, ssMid, ssHpf;
    Channel ch[maxChannels];
};
} // namespace n73
