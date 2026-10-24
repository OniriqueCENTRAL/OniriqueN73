// Banc de mesure du moteur N73 : chaque mesure est réellement calculée ici, rien n'est simulé à la main.
//
//   N73Bench [section …]   sections : eq, gain, freq, thd, clip, alias, latency, cpu, robust, clicks, all
//
// Niveaux : CAL +18 (0 dBFS = +18 dBu sinus efficace) ; « dBu » = niveau de sortie du module.
#include <juce_dsp/juce_dsp.h>
#include "N73Engine.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <sstream>

using namespace n73;

namespace
{
constexpr double pi = 3.14159265358979323846;

double dbuToFs (double dbu, double cal = 18.0) { return std::pow (10.0, (dbu - cal) / 20.0); }   // amplitude crête relative
double fsToDbu (double a, double cal = 18.0)   { return 20.0 * std::log10 (std::max (1.0e-30, a)) + cal; }

EngineParams defaults()
{
    EngineParams p;
    p.module.gainIndex = 4;   // LINE 0
    return p;
}

int lineIndex (int gainDb) { for (int i = 0; i < numGainPositions; ++i) if (gainPositions[i].kind == GainPosition::Line && gainPositions[i].gainDb == gainDb) return i; return 4; }
int micIndex (int gainDb)  { for (int i = 0; i < numGainPositions; ++i) if (gainPositions[i].kind == GainPosition::Mic && gainPositions[i].gainDb == gainDb) return i; return 17; }
double potFor (double db, double range, double zero = 0.5) { return eq::potFromKnob (db / range, zero); }

/** Fait passer un sinus et renvoie le signal de sortie (canal 0). */
std::vector<float> runSine (const EngineParams& p, double fs, double freq, double amp, double seconds, int block = 256)
{
    Engine e;
    e.prepare (fs, block, 2);
    const int total = (int) (seconds * fs);
    std::vector<float> out ((size_t) total);
    juce::AudioBuffer<float> buf (2, block);
    double ph = 0.0;
    const double inc = 2.0 * pi * freq / fs;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const float s = (float) (amp * std::sin (ph));
            ph += inc; if (ph > 2.0 * pi) ph -= 2.0 * pi;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        e.process (buf, p);
        for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = buf.getSample (0, i);
    }
    return out;
}

/** Amplitude de la composante à freq (corrélation sur un nombre entier de périodes, fenêtre de Hann). */
double amplitudeAt (const std::vector<float>& x, size_t start, size_t len, double fs, double freq)
{
    double re = 0, im = 0, wsum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) len);
        const double ph = 2.0 * pi * freq * (double) (start + i) / fs;
        re += w * x[start + i] * std::cos (ph);
        im += w * x[start + i] * std::sin (ph);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

struct Harmonics { double h1 = 0; double h[10] {}; double thd = 0; };

Harmonics analyse (const std::vector<float>& y, double fs, double f)
{
    const size_t len = (size_t) (fs * 0.5), start = y.size() - len;
    Harmonics r;
    r.h1 = amplitudeAt (y, start, len, fs, f);
    double sum = 0;
    for (int k = 2; k <= 10; ++k)
    {
        if (k * f >= fs * 0.5) break;
        r.h[k - 1] = amplitudeAt (y, start, len, fs, k * f) / std::max (1.0e-30, r.h1);
        sum += r.h[k - 1] * r.h[k - 1];
    }
    r.thd = std::sqrt (sum);
    return r;
}

std::string pct (double v) { std::ostringstream o; o << std::fixed << std::setprecision (v < 0.001 ? 5 : 4) << v * 100.0 << " %"; return o.str(); }

//==============================================================================
void testEq()
{
    std::cout << "\n== EQ : réseaux discrétisés (96 kHz) contre la simulation analogique de référence ==\n";
    const double fs = 96000.0;
    eq::Net net; eq::SS ss;
    auto show = [&] (const char* name, std::initializer_list<double> fl, double ref)
    {
        std::cout << "  " << std::left << std::setw (26) << name << std::right;
        for (double f : fl) std::cout << std::setw (7) << std::fixed << std::setprecision (1) << 20.0 * std::log10 (std::abs (eq::response (ss, f, fs)) / ref);
        std::cout << "\n";
    };
    std::cout << "  " << std::setw (26) << "" ; for (double f : { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }) std::cout << std::setw (7) << (int) f; std::cout << "\n";
    eq::buildLfHf (net, 0.5, 0.5, 100e-9); net.discretize (fs, 12000, ss);
    const double ref = std::abs (eq::response (ss, 1000.0, fs));
    std::cout << "  gain absolu au centre : 1 kHz " << 20.0 * std::log10 (ref) << " dB, 100 Hz " << 20.0 * std::log10 (std::abs (eq::response (ss, 100.0, fs))) << " dB, 10 kHz " << 20.0 * std::log10 (std::abs (eq::response (ss, 10000.0, fs))) << " dB\n";
    { eq::SS m; eq::buildMid (net, 0.5, -1); net.discretize (fs, 1000, m); std::cout << "  MID OFF : 1 kHz " << 20.0 * std::log10 (std::abs (eq::response (m, 1000.0, fs))) << " dB\n"; eq::buildLfHf (net, 0.5, 0.5, 100e-9); net.discretize (fs, 12000, ss); }
    {
        // position du potentiomètre HIGH la plus plate (200 Hz – 20 kHz), LOW au centre
        double bestPos = 0.5, bestErr = 1e9;
        for (double hp = 0.40; hp <= 0.60; hp += 0.001)
        {
            eq::buildLfHf (net, hp, 0.5, 100e-9); net.discretize (fs, 12000, ss);
            double err = 0;
            for (double f = 200; f <= 20000; f *= 1.1) err = std::max (err, std::abs (20.0 * std::log10 (std::abs (eq::response (ss, f, fs)))));
            if (err < bestErr) { bestErr = err; bestPos = hp; }
        }
        std::cout << "  HIGH le plus plat : position " << bestPos << " (écart max " << bestErr << " dB)\n";
        double bestL = 0.5; bestErr = 1e9;
        for (double lp = 0.40; lp <= 0.60; lp += 0.001)
        {
            eq::buildLfHf (net, bestPos, lp, 100e-9); net.discretize (fs, 12000, ss);
            double err = 0;
            for (double f = 20; f <= 20000; f *= 1.1) err = std::max (err, std::abs (20.0 * std::log10 (std::abs (eq::response (ss, f, fs)))));
            if (err < bestErr) { bestErr = err; bestL = lp; }
        }
        std::cout << "  puis LOW le plus plat : position " << bestL << " (écart max 20 Hz – 20 kHz " << bestErr << " dB)\n";
        eq::buildLfHf (net, 0.5, 0.5, 100e-9); net.discretize (fs, 12000, ss);
    }
    show ("plat (potards au centre)", { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }, ref);
    eq::buildLfHf (net, 0.5, 0.0, 100e-9); net.discretize (fs, 12000, ss);
    show ("LOW 60 Hz, maximum", { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }, ref);
    eq::buildLfHf (net, 0.5, 1.0, 100e-9); net.discretize (fs, 12000, ss);
    show ("LOW 60 Hz, minimum", { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }, ref);
    eq::buildLfHf (net, 0.0, 0.5, 100e-9); net.discretize (fs, 12000, ss);
    show ("HIGH, maximum", { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }, ref);
    eq::buildLfHf (net, 1.0, 0.5, 100e-9); net.discretize (fs, 12000, ss);
    show ("HIGH, minimum", { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }, ref);
    eq::buildLfHf (net, 0.5, 0.5, 0.0); net.discretize (fs, 12000, ss);
    show ("LOW OFF (potard au centre)", { 20., 40., 100., 200., 350., 1000., 2000., 5000., 10000., 20000. }, ref);

    std::cout << "\n  MID : gain au centre, extrémités du potentiomètre (fiche AMS : ±18 dB)\n";
    const double centres[6] = { 360, 700, 1600, 3200, 4800, 7200 };
    for (int i = 0; i < 6; ++i)
    {
        eq::buildMid (net, 0.5, -1); net.discretize (fs, centres[i], ss);
        const double r0 = std::abs (eq::response (ss, 1000.0, fs));
        double best = 0, bf = 0, worst = 0, wf = 0;
        for (double f = centres[i] * 0.5; f < centres[i] * 1.8; f *= 1.002)
        {
            eq::buildMid (net, 0.0, i); net.discretize (fs, centres[i], ss);
            const double gb = 20.0 * std::log10 (std::abs (eq::response (ss, f, fs)) / r0);
            if (gb > best) { best = gb; bf = f; }
            eq::buildMid (net, 1.0, i); net.discretize (fs, centres[i], ss);
            const double gc = 20.0 * std::log10 (std::abs (eq::response (ss, f, fs)) / r0);
            if (gc < worst) { worst = gc; wf = f; }
        }
        std::cout << "    " << std::setw (5) << (int) centres[i] << " Hz : +" << std::setprecision (1) << best << " dB à " << (int) bf
                  << " Hz, " << worst << " dB à " << (int) wf << " Hz\n";
    }

    std::cout << "\n  HIGH PASS : -3 dB (source 1k2, charge 5k1)\n";
    for (int i = 0; i < 4; ++i)
    {
        eq::buildHpf (net, i); net.discretize (fs, 100.0, ss);
        const double r0 = std::abs (eq::response (ss, 5000.0, fs));
        double f3 = 0;
        for (double f = 10; f < 2000; f *= 1.002) if (20.0 * std::log10 (std::abs (eq::response (ss, f, fs)) / r0) > -3.0) { f3 = f; break; }
        std::cout << "    position " << i + 1 << " : -3 dB à " << (int) f3 << " Hz ; à f/2 : "
                  << 20.0 * std::log10 (std::abs (eq::response (ss, f3 / 2, fs)) / r0) << " dB\n";
    }
}

void testGain()
{
    std::cout << "\n== Gain par position (1 kHz, petit signal, EQ en circuit à plat, 48 kHz) ==\n";
    for (int i = 0; i < numGainPositions; ++i)
    {
        const auto& gp = gainPositions[i];
        if (gp.kind == GainPosition::Off) { std::cout << "  OFF\n"; continue; }
        auto p = defaults();
        p.module.gainIndex = i;
        // entrée : 60 dB sous la sortie visée de 0 dBu
        const double inDbu = (gp.kind == GainPosition::Mic ? micReferenceDb : 0.0) - gp.gainDb;
        const double amp = dbuToFs (inDbu);
        auto y = runSine (p, 48000.0, 1000.0, amp, 0.6);
        const double g = 20.0 * std::log10 (analyse (y, 48000.0, 1000.0).h1 / amp) + (gp.kind == GainPosition::Mic ? micReferenceDb : 0.0);
        std::cout << "  " << (gp.kind == GainPosition::Mic ? "MIC " : "LINE ") << std::setw (3) << gp.gainDb << " dB : mesuré "
                  << std::fixed << std::setprecision (2) << g << " dB\n";
    }
    auto p = defaults();
    p.module.gainIndex = micIndex (40);
    for (bool hi : { true, false })
    {
        p.module.micZHigh = hi;
        const double amp = dbuToFs (micReferenceDb - 40.0 - 20.0);   // petit signal : sortie vers -20 dBu
        auto y = runSine (p, 48000.0, 1000.0, amp, 0.6);
        std::cout << "  MIC 40, MIC Z " << (hi ? "HI (1200 Ω)" : "LO (300 Ω) ") << " : " << 20.0 * std::log10 (analyse (y, 48000.0, 1000.0).h1 / amp) << " dB\n";
    }
}

void testFreq()
{
    std::cout << "\n== Réponse en fréquence du préampli (EQ hors circuit, HPF OFF, -30 dBu en sortie) ==\n";
    const double freqs[] = { 5, 6, 7, 10, 15, 20, 30, 50, 100, 1000, 5000, 10000, 15000, 20000 };
    for (double fs : { 48000.0, 96000.0 })
        for (int mode = 0; mode < 2; ++mode)
        {
            auto p = defaults();
            p.module.eqIn = false;
            p.module.gainIndex = mode == 0 ? lineIndex (0) : micIndex (50);
            const double inDbu = mode == 0 ? -30.0 : -30.0 + micReferenceDb - 50.0;
            const double amp = dbuToFs (inDbu);
            std::cout << "  " << (mode == 0 ? "LINE 0  " : "MIC 50  ") << (int) (fs / 1000) << " kHz :";
            double ref = 0;
            for (double f : freqs)
            {
                auto y = runSine (p, fs, f, amp, f < 20 ? 3.0 : 1.0);
                const double a = analyse (y, fs, f).h1;
                if (f == 1000) ref = a;
                (void) a;
            }
            for (double f : freqs)
            {
                auto y = runSine (p, fs, f, amp, f < 20 ? 3.0 : 1.0);
                std::cout << "  " << f << ":" << std::setprecision (2) << 20.0 * std::log10 (analyse (y, fs, f).h1 / ref);
            }
            std::cout << "\n";
        }
}

void testThd()
{
    std::cout << "\n== Distorsion selon le niveau de sortie (LINE 0, EQ à plat, 96 kHz) ==\n";
    for (double f : { 1000.0, 100.0, 50.0, 20.0 })
    {
        std::cout << "  " << f << " Hz\n";
        for (double out : { 4.0, 10.0, 16.0, 20.0, 24.0, 26.0, 28.0, 30.0 })
        {
            auto p = defaults();
            const auto y = runSine (p, 96000.0, f, dbuToFs (out), f < 50 ? 2.5 : 1.5);
            const auto h = analyse (y, 96000.0, f);
            std::cout << "    +" << std::setw (2) << (int) out << " dBu : DHT " << std::setw (11) << pct (h.thd)
                      << "  H2 " << std::setw (11) << pct (h.h[1]) << "  H3 " << std::setw (11) << pct (h.h[2])
                      << "  H4 " << std::setw (11) << pct (h.h[3]) << "  H5 " << std::setw (11) << pct (h.h[4])
                      << "  (sortie " << std::setprecision (1) << fsToDbu (h.h1) << " dBu)\n";
        }
    }
    std::cout << "\n  MIC 80, entrée à -60 dBu (sortie visée +20 dBu), 1 kHz :";
    auto p = defaults(); p.module.gainIndex = micIndex (80);
    auto y = runSine (p, 96000.0, 1000.0, dbuToFs (-60.0 + micReferenceDb), 1.5);
    auto h = analyse (y, 96000.0, 1000.0);
    std::cout << " DHT " << pct (h.thd) << " H2 " << pct (h.h[1]) << " H3 " << pct (h.h[2]) << "\n";
}

void testClip()
{
    std::cout << "\n== Écrêtage : niveau de sortie à 1 % et 3 % de DHT (LINE 0, 1 kHz et 50 Hz) ==\n";
    for (double f : { 1000.0, 50.0 })
    {
        double at1 = 0, at3 = 0;
        for (double out = 14.0; out <= 34.0; out += 0.5)
        {
            auto p = defaults();
            const auto h = analyse (runSine (p, 96000.0, f, dbuToFs (out), 1.0), 96000.0, f);
            if (at1 == 0 && h.thd >= 0.01) at1 = out;
            if (at3 == 0 && h.thd >= 0.03) { at3 = out; break; }
        }
        std::cout << "  " << f << " Hz : 1 % à +" << at1 << " dBu, 3 % à +" << at3 << " dBu\n";
    }
}

void testAlias()
{
    std::cout << "\n== Repliement : 9 973 Hz poussé fort (MIC 80, sortie visée +30 dBu), pire raie non harmonique 20 Hz – 20 kHz ==\n";
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (bool hq : { false, true })
        {
            auto p = defaults(); p.module.gainIndex = micIndex (80); p.highQuality = hq;
            const double f = 9973.0;
            auto y = runSine (p, fs, f, dbuToFs (-50.0 + micReferenceDb), 1.0);
            const size_t N = 32768, start = y.size() - N;
            juce::dsp::FFT fft (15);
            std::vector<float> data (2 * N, 0.0f);
            for (size_t i = 0; i < N; ++i)
            {
                const double w = 0.35875 - 0.48829 * std::cos (2 * pi * i / N) + 0.14128 * std::cos (4 * pi * i / N) - 0.01168 * std::cos (6 * pi * i / N);
                data[i] = (float) (y[start + i] * w);
            }
            fft.performFrequencyOnlyForwardTransform (data.data());
            const double binHz = fs / (double) N;
            double fund = 0, worst = 0, worstF = 0;
            for (size_t b = 1; b < N / 2; ++b)
            {
                const double fb = b * binHz;
                if (std::abs (fb - f) < 4 * binHz) { fund = std::max (fund, (double) data[b]); continue; }
                if (fb < 20.0 || fb > 20000.0) continue;
                bool harmonic = false;
                for (int k = 2; k * f < fs / 2; ++k) if (std::abs (fb - k * f) < 4 * binHz) harmonic = true;
                if (harmonic) continue;
                if (data[b] > worst) { worst = data[b]; worstF = fb; }
            }
            Engine e; e.prepare (fs, 256, 2);
            std::cout << "  " << fs / 1000 << " kHz, " << (hq ? "qualité haute" : "normal       ") << " (travail " << e.workingRate (hq) / 1000
                      << " kHz) : " << std::setprecision (1) << 20.0 * std::log10 (worst / fund) << " dB à " << (int) worstF << " Hz\n";
        }
}

void testLatency()
{
    std::cout << "\n== Latence (petit signal) ==\n";
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (bool hq : { false, true })
        {
            auto p = defaults(); p.module.eqIn = false; p.highQuality = hq;
            Engine e; e.prepare (fs, 512, 2);
            // retard de groupe à 1 kHz : phase du sinus de sortie contre l'entrée
            auto y = runSine (p, fs, 1000.0, dbuToFs (-20.0), 1.0, 512);
            const size_t len = (size_t) fs / 2, start = y.size() - len;
            double re = 0, im = 0;
            for (size_t i = 0; i < len; ++i)
            {
                const double ph = 2.0 * pi * 1000.0 * (double) (start + i) / fs;
                re += y[start + i] * std::cos (ph); im += y[start + i] * std::sin (ph);
            }
            // sortie ≈ A·sin(ph - φ) → re ≈ -A/2·sin φ·len, im ≈ A/2·cos φ·len
            const double phi = std::atan2 (-re, im);
            std::cout << "  " << fs / 1000 << " kHz " << (hq ? "qualité haute" : "normal       ") << " : latence déclarée " << e.latencySamples (hq)
                      << " éch., retard de phase à 1 kHz " << std::setprecision (2) << phi / (2 * pi) * fs / 1000.0 << " éch.\n";
        }
}

void testCpu()
{
    std::cout << "\n== CPU : 20 s stéréo, blocs de 128, réglages typiques (EQ actif, HPF 80, MIC 60) ==\n";
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (bool hq : { false, true })
        {
            auto p = defaults();
            p.module.gainIndex = micIndex (60); p.module.hpf = 2; p.module.lowFreq = 3; p.module.midFreq = 4;
            p.module.highPos = potFor (4, 16, eq::hfZero); p.module.midPos = potFor (3, 18); p.module.lowPos = potFor (-2, 16, eq::lfZero);
            p.highQuality = hq;
            Engine e; e.prepare (fs, 128, 2);
            juce::AudioBuffer<float> buf (2, 128);
            // bruit à -80 dBFS : avec 60 dB de gain, la sortie est vers -20 dBFS (un niveau de travail normal)
            std::mt19937 rng (1); std::normal_distribution<float> nd (0.0f, 1.0e-4f);
            const int blocks = (int) (20.0 * fs / 128);
            double secs = 0;
            for (int b = 0; b < blocks; ++b)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) buf.setSample (c, i, nd (rng));
                const auto t0 = std::chrono::high_resolution_clock::now();
                e.process (buf, p);
                secs += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
            }
            std::cout << "  " << fs / 1000 << " kHz " << (hq ? "qualité haute" : "normal       ") << " : " << std::setprecision (2)
                      << 100.0 * secs / 20.0 << " % d'un cœur (" << std::setprecision (3) << secs * 1.0e6 / blocks << " µs par bloc)\n";
        }
    // coût pendant un mouvement continu des potentiomètres (recalcul des réseaux)
    {
        auto p = defaults(); p.module.lowFreq = 2; p.module.midFreq = 3;
        Engine e; e.prepare (48000.0, 128, 2);
        juce::AudioBuffer<float> buf (2, 128);
        const int blocks = (int) (10.0 * 48000.0 / 128);
        double secs = 0;
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            p.module.highPos = 0.5 + 0.5 * std::sin (b * 0.01);
            p.module.lowPos = 0.5 + 0.5 * std::sin (b * 0.013);
            p.module.midPos = 0.5 + 0.5 * std::sin (b * 0.017);
            const auto t0 = std::chrono::high_resolution_clock::now();
            e.process (buf, p);
            secs += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        }
        std::cout << "  48 kHz, trois potards en mouvement permanent : " << std::setprecision (2) << 100.0 * secs / 10.0 << " % d'un cœur\n";
    }
    // pire cas : tous les étages saturés (MIC 30, bruit à -20 dBFS)
    {
        auto p = defaults(); p.module.gainIndex = micIndex (30);
        Engine e; e.prepare (48000.0, 128, 2);
        juce::AudioBuffer<float> buf (2, 128);
        std::mt19937 rng (1); std::normal_distribution<float> nd (0.0f, 0.1f);
        const int blocks = (int) (10.0 * 48000.0 / 128);
        double secs = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) buf.setSample (c, i, nd (rng));
            const auto t0 = std::chrono::high_resolution_clock::now();
            e.process (buf, p);
            secs += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        }
        std::cout << "  48 kHz, saturation forte (MIC 30, bruit à -20 dBFS) : " << std::setprecision (2) << 100.0 * secs / 10.0 << " % d'un cœur\n";
    }
}

void testRobust()
{
    std::cout << "\n== Robustesse : fréquences, tailles de blocs, réglages au hasard, NaN / inf en entrée ==\n";
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    int failures = 0, runs = 0;
    double worstAll = 0;
    for (double fs : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (int block : { 1, 7, 64, 333, 512, 2048 })
        {
            Engine e; e.prepare (fs, block, 2);
            juce::AudioBuffer<float> buf (2, block);
            double maxAbs = 0;
            bool bad = false;
            const int blocks = std::max (20, (int) (0.5 * fs / block));
            for (int b = 0; b < blocks; ++b)
            {
                EngineParams p;
                p.module.gainIndex = (int) (u (rng) * numGainPositions) % numGainPositions;
                p.module.micZHigh = u (rng) > 0.5; p.module.pad = u (rng) > 0.8;
                p.module.highPos = u (rng); p.module.midPos = u (rng); p.module.lowPos = u (rng);
                p.module.midFreq = (int) (u (rng) * 7) % 7; p.module.lowFreq = (int) (u (rng) * 5) % 5; p.module.hpf = (int) (u (rng) * 5) % 5;
                p.module.eqIn = u (rng) > 0.2; p.module.phase = u (rng) > 0.5;
                p.module.levelDb = -80 + 90 * u (rng);
                p.levelMatch = u (rng) > 0.7; p.power = u (rng) > 0.1; p.highQuality = u (rng) > 0.7;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < block; ++i)
                    {
                        float s = (float) ((u (rng) * 2 - 1) * 2.0);   // jusqu'à +6 dBFS de bruit
                        if (b == blocks / 2 && i == 0) s = std::numeric_limits<float>::quiet_NaN();
                        if (b == blocks / 2 + 1 && i == 0) s = std::numeric_limits<float>::infinity();
                        buf.setSample (c, i, s);
                    }
                // NaN / inf : l'hôte les envoie parfois ; le processeur les remplace par 0 avant le moteur
                for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) if (! std::isfinite (buf.getSample (c, i))) buf.setSample (c, i, 0.0f);
                e.process (buf, p);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < block; ++i)
                    {
                        const float s = buf.getSample (c, i);
                        if (! std::isfinite (s)) bad = true;
                        maxAbs = std::max (maxAbs, (double) std::abs (s));
                    }
            }
            ++runs; worstAll = std::max (worstAll, maxAbs);
            if (bad || maxAbs > 1000.0) { ++failures; std::cout << "  ÉCHEC " << fs << " Hz, blocs de " << block << " : max " << maxAbs << (bad ? " (non fini)" : "") << "\n"; }
        }
    std::cout << "  " << runs - failures << " / " << runs << " configurations sans valeur non finie ni emballement (pic le plus haut "
              << std::setprecision (1) << 20.0 * std::log10 (worstAll) << " dBFS, réglages extrêmes compris)\n";
}

void testClicks()
{
    std::cout << "\n== Clics : sinus 1 kHz à 0 dBu, changements de commutateurs toutes les 100 ms ==\n";
    const double fs = 48000.0;
    auto run = [&] (bool switching)
    {
        Engine e; e.prepare (fs, 128, 2);
        juce::AudioBuffer<float> buf (2, 128);
        double ph = 0, worst = 0;
        auto p = defaults();
        p.module.gainIndex = micIndex (60);
        const int blocks = (int) (4.0 * fs / 128);
        std::vector<float> prev (3, 0.0f);
        int step = 0;
        for (int b = 0; b < blocks; ++b)
        {
            if (switching && b % 37 == 0)
            {
                ++step;
                p.module.gainIndex = micIndex (55 + 5 * (step % 3));
                p.module.lowFreq = step % 5; p.module.midFreq = step % 7; p.module.hpf = step % 5;
                p.module.micZHigh = (step % 2) == 0;
            }
            for (int i = 0; i < 128; ++i)
            {
                const float s = (float) (dbuToFs (0.0 + micReferenceDb - 60.0) * std::sin (ph));
                ph += 2 * pi * 1000.0 / fs;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            e.process (buf, p);
            for (int i = 0; i < 128; ++i)
            {
                const float y = buf.getSample (0, i);
                const double d2 = std::abs (y - 2 * prev[1] + prev[0]);
                if (b > 40) worst = std::max (worst, d2);
                prev[0] = prev[1]; prev[1] = y;
            }
        }
        return worst;
    };
    const double still = run (false), moving = run (true);
    std::cout << std::setprecision (5) << "  dérivée seconde max, sans changement : " << still << " ; avec changements : " << moving
              << " (rapport " << std::setprecision (2) << moving / still << ")\n";
}
void testDrive()
{
    std::cout << "\n== Saturation selon le gain : sinus à -18 dBFS efficace, 0 dBFS = +26 dBu (comme UAD), 48 kHz ==\n";
    for (double f : { 100.0, 1000.0 })
    {
        std::cout << "  " << f << " Hz\n";
        for (int i : { 4, 3, 2, 1, 0, 8, 9, 10, 11, 12 })
        {
            auto p = defaults();
            p.module.gainIndex = i;
            p.module.calDbu = 26.0;
            const double amp = std::pow (10.0, -18.0 / 20.0) * std::sqrt (2.0);
            const auto y = runSine (p, 48000.0, f, amp, 1.0);
            const auto h = analyse (y, 48000.0, f);
            const auto& gp = gainPositions[i];
            std::cout << "    " << (gp.kind == GainPosition::Mic ? "MIC  " : "LINE ") << std::setw (3) << gp.gainDb << " : DHT "
                      << std::setw (10) << pct (h.thd) << "  H2 " << std::setw (10) << pct (h.h[1]) << "  H3 " << std::setw (10) << pct (h.h[2])
                      << "  sortie " << std::setprecision (1) << 20.0 * std::log10 (h.h1 / std::sqrt (2.0)) << " dBFS\n";
        }
    }
    std::cout << "\n  fader (LEVEL) avant l'ampli de sortie : LINE 0, sinus 1 kHz à -8 dBFS efficace (+18 dBu)\n";
    for (double level : { 0.0, 5.0, 10.0 })
    {
        auto p = defaults();
        p.module.calDbu = 26.0;
        p.module.levelDb = level;
        const auto y = runSine (p, 48000.0, 1000.0, std::pow (10.0, -8.0 / 20.0) * std::sqrt (2.0), 1.0);
        const auto h = analyse (y, 48000.0, 1000.0);
        std::cout << "    fader " << std::setprecision (0) << std::setw (3) << level << " dB : DHT " << std::setw (10) << pct (h.thd)
                  << "  sortie " << std::setprecision (1) << 20.0 * std::log10 (h.h1 / std::sqrt (2.0)) << " dBFS\n";
    }
}

void testProfile()
{
    std::cout << "\n== Coût de chaque composant (ns par échantillon, 96 kHz, signal musical à 0 dBu) ==\n";
    const int N = 2000000;
    std::vector<double> x ((size_t) N);
    std::mt19937 rng (3); std::normal_distribution<double> nd (0.0, 1.0);
    double lp = 0; for (auto& v : x) { lp += 0.05 * (nd (rng) - lp); v = lp * 4.0; }
    auto timeIt = [&] (const char* name, auto&& fn)
    {
        double acc = 0;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i) acc += fn (x[(size_t) i]);
        const double ns = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count() * 1.0e9 / N;
        std::cout << "  " << std::left << std::setw (34) << name << std::right << std::setprecision (1) << ns << " ns" << (acc == 12345.0 ? "!" : "") << "\n";
    };
    Transformer t; t.prepare (96000); t.setParams (model::lineTransformer());
    timeIt ("transformateur (ligne)", [&] (double v) { return t.process (v); });
    Transformer to; to.prepare (96000); to.setParams (model::outputTransformer());
    timeIt ("transformateur (sortie, polarisé)", [&] (double v) { return to.process (v); });
    AmpStage a; a.prepare (96000); a.setParams (model::nvStage (7.86));
    timeIt ("étage BA283 NV", [&] (double v) { return a.process (v * 0.1); });
    OutputStage o; o.prepare (96000); OutputStage::Params op; op.xf = model::outputTransformer(); o.setParams (op);
    timeIt ("étage de sortie + LO1166", [&] (double v) { return o.process (v * 0.2); });
    eq::Net net; eq::SS s7, s3; eq::buildLfHf (net, 0.4, 0.6, 100e-9); net.discretize (96000, 12000, s7);
    eq::buildMid (net, 0.3, 3); net.discretize (96000, 3200, s3);
    StateSpaceRunner<eq::MaxStates> r7, r3;
    timeIt ("réseau HIGH / LOW (7 mémoires)", [&] (double v) { return r7.process (s7, v); });
    timeIt ("réseau MID (3 mémoires)", [&] (double v) { return r3.process (s3, v); });
    Biquad b; b.setLowPass (60000, 1.08, 96000, 20000);
    timeIt ("biquad", [&] (double v) { return b.process (v); });
    Module m; m.prepare (96000); ModuleConfig c; c.gainIndex = 4; c.hpf = 2; c.lowFreq = 2; c.midFreq = 3; m.configure (c);
    timeIt ("module complet, un canal", [&] (double v) { return m.process (0, v * 0.1); });
}
} // namespace

int main (int argc, char** argv)
{
    std::cout << std::fixed;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.isEmpty()) args.add ("all");
    auto want = [&] (const char* s) { return args.contains (s) || args.contains ("all"); };
    if (want ("eq")) testEq();
    if (want ("gain")) testGain();
    if (want ("freq")) testFreq();
    if (want ("thd")) testThd();
    if (want ("clip")) testClip();
    if (want ("alias")) testAlias();
    if (want ("latency")) testLatency();
    if (want ("cpu")) testCpu();
    if (want ("robust")) testRobust();
    if (want ("clicks")) testClicks();
    if (want ("drive")) testDrive();
    if (args.contains ("prof")) testProfile();
    return 0;
}
