#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/N73Core.h"

namespace n73
{
//==============================================================================
/** Ce que l'interface et l'automation demandent au moteur, lu une fois par bloc. */
struct EngineParams
{
    ModuleConfig module;
    double faderDb = 0.0;      // fader de console, propre (après le module)
    bool levelMatch = false;   // compense le gain nominal (comparaisons A/B à niveau égal)
    bool power = true;
    bool highQuality = false;  // suréchantillonnage ×4 au lieu de ×2 (×2 au lieu de ×1 à 88,2 kHz et plus)
};

//==============================================================================
//  Le moteur : suréchantillonnage, lissage des réglages, fondus enchaînés quand la structure change (commutateurs),
//  fader, compensation de niveau, POWER. Aucune allocation dans process().
//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        fs = sampleRate;
        channels = juce::jlimit (1, Module::maxChannels, numChannels);
        maxBlockSize = juce::jmax (1, maxBlock);
        for (int q = 0; q < 2; ++q)
        {
            const int factorLog2 = osFactorLog2 (q == 1);
            if (factorLog2 > 0)
            {
                os[q] = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, (size_t) factorLog2,
                            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
                os[q]->initProcessing ((size_t) maxBlockSize);
            }
            else os[q].reset();
        }
        dry.setSize (channels, maxBlockSize + 64);
        delayLine.setSize (channels, 64);
        delayLine.clear();
        delayPos = 0;
        configured = false;
        reset();
    }

    void reset()
    {
        for (auto& o : os) if (o != nullptr) o->reset();
        for (auto& m : modules) m.reset();
        fading = false;
        fadePos = 0;
        wet.setCurrentAndTargetValue (1.0f);
        delayLine.clear();
    }

    /** latence ajoutée (échantillons, à la fréquence de l'hôte) : celle des filtres du suréchantillonnage. */
    int latencySamples (bool highQuality) const
    {
        const auto& o = os[highQuality ? 1 : 0];
        return o != nullptr ? (int) std::lround (o->getLatencyInSamples()) : 0;
    }

    double workingRate (bool highQuality) const { return fs * (double) (1 << osFactorLog2 (highQuality)); }

    void process (juce::AudioBuffer<float>& buffer, const EngineParams& p)
    {
        juce::ScopedNoDenormals noDenormals;
        const int n = buffer.getNumSamples();
        const int nch = juce::jmin (channels, buffer.getNumChannels());
        if (n <= 0 || nch <= 0) return;

        const int q = p.highQuality ? 1 : 0;
        if (q != quality || ! configured)
        {
            // changement de qualité : nouvelle fréquence de travail, le module repart de zéro
            quality = q;
            const double rate = workingRate (p.highQuality);
            for (auto& m : modules) { m.prepare (rate); m.configure (p.module); }
            active = 0; fading = false; configured = true;
            potHigh.reset (rate, 0.03); potMid.reset (rate, 0.03); potLow.reset (rate, 0.03);
            potHigh.setCurrentAndTargetValue (p.module.highPos);
            potMid.setCurrentAndTargetValue (p.module.midPos);
            potLow.setCurrentAndTargetValue (p.module.lowPos);
            outDb.reset (rate, 0.03); outDb.setCurrentAndTargetValue (p.module.outputDb);
            fader.reset (fs, 0.03); fader.setCurrentAndTargetValue (dbToGain (p.faderDb));
            match.reset (fs, 0.05); match.setCurrentAndTargetValue (matchGain (p));
            wet.reset (fs, 0.02); wet.setCurrentAndTargetValue (p.power ? 1.0f : 0.0f);
            if (os[q] != nullptr) os[q]->reset();
        }

        // signal sec retardé de la latence du suréchantillonnage : POWER se fond sans filtrage en peigne
        const int lat = latencySamples (p.highQuality);
        for (int c = 0; c < nch; ++c)
        {
            auto* d = dry.getWritePointer (c);
            const auto* in = buffer.getReadPointer (c);
            auto* dl = delayLine.getWritePointer (c);
            int pos = delayPos;
            for (int i = 0; i < n; ++i)
            {
                dl[pos] = in[i];
                d[i] = dl[(pos - lat + 64) & 63];
                pos = (pos + 1) & 63;
            }
        }
        delayPos = (delayPos + n) & 63;

        wet.setTargetValue (p.power ? 1.0f : 0.0f);
        const bool wetSilent = ! wet.isSmoothing() && wet.getCurrentValue() == 0.0f;

        if (! wetSilent)
            processModule (buffer, nch, n, p);

        // fader, compensation, POWER
        fader.setTargetValue (dbToGain (p.faderDb));
        match.setTargetValue (matchGain (p));
        for (int i = 0; i < n; ++i)
        {
            const float g = fader.getNextValue() * match.getNextValue();
            const float w = wet.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                auto* s = buffer.getWritePointer (c);
                const float processed = wetSilent ? 0.0f : s[i] * g;
                s[i] = processed * w + dry.getReadPointer (c)[i] * (1.0f - w);
            }
        }
        for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    }

    /** dérive maximale de l'étage de sortie sur le dernier bloc (0 = repos, 1 = butée). */
    float drive() const noexcept { return lastDrive.load(); }

    const Module& currentModule() const { return modules[active]; }

private:
    static float dbToGain (double db) { return db <= -80.0 ? 0.0f : (float) std::pow (10.0, db / 20.0); }

    static float matchGain (const EngineParams& p)
    {
        if (! p.levelMatch) return 1.0f;
        const auto& gp = gainPositions[p.module.gainIndex];
        if (gp.kind == GainPosition::Off) return 1.0f;
        const double nominal = gp.kind == GainPosition::Mic ? gp.gainDb - micReferenceDb : gp.gainDb;
        const double comp = -(nominal + p.module.outputDb + (p.module.pad ? -20.0 : 0.0));
        return (float) std::pow (10.0, juce::jlimit (-30.0, 30.0, comp) / 20.0);   // au-delà, la comparaison n'a plus de sens
    }

    int osFactorLog2 (bool highQuality) const
    {
        // fréquence de travail ≥ 88,2 kHz : ×2 à 44,1 / 48 kHz, ×1 au-delà ; qualité haute : un cran de plus
        const int base = fs < 80000.0 ? 1 : 0;
        return juce::jmin (2, base + (highQuality ? 1 : 0));
    }

    void processModule (juce::AudioBuffer<float>& buffer, int nch, int n, const EngineParams& p)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
        auto& o = os[quality];
        auto up = o != nullptr ? o->processSamplesUp (block) : block;
        const int un = (int) up.getNumSamples();

        // structure : fondu enchaîné vers une copie reconfigurée
        if (! fading && ! modules[active].config().sameStructure (p.module))
        {
            const int next = 1 - active;
            modules[next] = modules[active];
            modules[next].configure (withPots (p.module, modules[active].config()));
            fading = true; fadePos = 0;
            fadeLen = juce::jmax (1, (int) (0.02 * workingRate (p.highQuality)));
        }

        potHigh.setTargetValue (p.module.highPos);
        potMid.setTargetValue (p.module.midPos);
        potLow.setTargetValue (p.module.lowPos);
        outDb.setTargetValue (p.module.outputDb);

        float peak = 0.0f;
        constexpr int sub = 32;
        for (int start = 0; start < un; start += sub)
        {
            const int len = juce::jmin (sub, un - start);
            // réglages continus, à la cadence de 32 échantillons
            const double h = potHigh.skip (len), m = potMid.skip (len), l = potLow.skip (len), od = outDb.skip (len);
            const int targets[2] = { active, 1 - active };
            for (int k = 0; k < (fading ? 2 : 1); ++k)
            {
                auto& mod = modules[targets[k]];
                mod.setPots (h, m, l);
                if (mod.config().outputDb != od) mod.setOutputDb (od);
            }
            for (int c = 0; c < nch; ++c)
            {
                auto* s = up.getChannelPointer ((size_t) c) + start;
                int fp = fadePos;
                for (int i = 0; i < len; ++i)
                {
                    double y = modules[active].process (c, (double) s[i]);
                    if (fading)
                    {
                        const double t = juce::jmin (1.0, (double) (fp + i) / (double) fadeLen);
                        const double y2 = modules[1 - active].process (c, (double) s[i]);
                        y = y + (y2 - y) * t;
                    }
                    if (! std::isfinite (y)) { y = 0.0; needsReset = true; }
                    s[i] = (float) y;
                }
                peak = juce::jmax (peak, (float) modules[fading ? 1 - active : active].drive (c));
            }
            if (fading)
            {
                fadePos += len;
                if (fadePos >= fadeLen) { active = 1 - active; fading = false; }
            }
        }
        if (o != nullptr) o->processSamplesDown (block);
        if (needsReset) { for (auto& mod : modules) mod.reset(); needsReset = false; }
        lastDrive.store (peak);
    }

    static ModuleConfig withPots (ModuleConfig c, const ModuleConfig& current)
    {
        c.highPos = current.highPos; c.midPos = current.midPos; c.lowPos = current.lowPos; c.outputDb = current.outputDb;
        return c;
    }

    double fs = 48000.0;
    int channels = 2, maxBlockSize = 512, quality = 0;
    bool configured = false, fading = false, needsReset = false;
    int active = 0, fadePos = 0, fadeLen = 1;
    Module modules[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2];
    juce::SmoothedValue<double> potHigh, potMid, potLow, outDb;
    juce::SmoothedValue<float> fader, match, wet;
    juce::AudioBuffer<float> dry, delayLine;
    int delayPos = 0;
    std::atomic<float> lastDrive { 0.0f };
};
} // namespace n73
