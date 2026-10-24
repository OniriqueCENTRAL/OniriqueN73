#pragma once

#include "LinearCircuit.h"
#include <complex>

namespace n73
{
//==============================================================================
//  Les réseaux de l'égaliseur 1073, comme circuits électriques.
//
//  Topologies et valeurs : réseau HF/LF (PL10205), présence (BA211, bobines T1280 / T1530) et passe-haut
//  (PL10182, passif, chargé par 5k1 comme l'exige le manuel AMS Neve), relevés dans le projet libre NYAN-1073
//  (greyboxaudio, CC BY-SA 4.0) qui reprend le schéma d'origine ; valeurs des bobines d'après les fiches Sowter.
//  Ce sont des sources secondaires. La reconstitution a été vérifiée par simulation contre les courbes mesurées
//  de NYAN (écart < 1 dB, voir Tests/). Les amplis du module (BA284) sont remplacés par des amplis op idéaux.
//
//  Positions des potentiomètres : 0 = curseur côté entrée (accentuation maximale), 1 = côté sortie (atténuation).
//  potEnd : résistance de butée aux deux extrémités (estimée : voir docs/MODELE.md).
//==============================================================================
namespace eq
{
constexpr int MaxNodes = 14, MaxElems = 24, MaxOps = 1, MaxStates = 8;
using Net = Netlist<MaxNodes, MaxElems, MaxOps, MaxStates>;
using SS = StateSpace<MaxStates>;

// LOW : condensateurs du réseau grave selon la position (35, 60, 110, 220 Hz)
constexpr double lowCaps[4] = { 200e-9, 100e-9, 47e-9, 15e-9 };

// MID (présence) : bobine, sa résistance, condensateur — 0,36 / 0,7 / 1,6 / 3,2 / 4,8 / 7,2 kHz
struct MidValues { double L, rL, C; };
constexpr MidValues midValues[6] = {
    { 2.0,   85.0, 100e-9 },
    { 1.1,   59.0,  47e-9 },
    { 0.45,  35.0,  22e-9 },
    { 0.2,   32.0,  12.2e-9 },
    { 0.2,   32.0,   4.7e-9 },
    { 0.2,   32.0,   2.42e-9 },
};

// HIGH PASS : C (deux fois), bobine en dérivation et sa résistance — 50 / 80 / 160 / 300 Hz
struct HpfValues { double C, L, rL; };
constexpr HpfValues hpfValues[4] = {
    { 1.0e-6,  10.0, 1000.0 },
    { 470e-9,   7.0,  800.0 },
    { 220e-9,   3.0,  490.0 },
    { 122e-9,   1.3,  310.0 },
};
constexpr double hpfSource = 1200.0, hpfLoad = 5100.0;

// résistances de butée des potentiomètres (estimées, voir docs/MODELE.md)
constexpr double hfPot = 10e3, lfPot = 50e3, midPot = 10e3;
constexpr double hfEnd = 60.0, lfEnd = 900.0, midEnd = 180.0;

// position « 0 » des boutons : là où le réseau est le plus plat (le 470 p de l'ampli le déséquilibre un peu au
// centre exact). Mesuré au banc : ±0,4 dB de 20 Hz à 20 kHz au mieux, propriété du circuit.
constexpr double hfZero = 0.478, lfZero = 0.512, midZero = 0.5;

/** bouton → position du potentiomètre. k : −1 (atténuation maximale) … +1 (accentuation maximale). */
inline double potFromKnob (double k, double zero)
{
    k = std::clamp (k, -1.0, 1.0);
    return k >= 0.0 ? zero * (1.0 - k) : zero + (1.0 - zero) * (-k);
}

inline void potSplit (double pos, double total, double end, double& toInput, double& toOutput)
{
    pos = std::clamp (pos, 0.0, 1.0);
    toInput = end + pos * total;
    toOutput = end + (1.0 - pos) * total;
}

/** HIGH / LOW : réseau de type Baxandall autour d'un ampli inverseur. lowCap = 0 → bande LOW hors circuit. */
inline void buildLfHf (Net& n, double hfPos, double lfPos, double lowCap)
{
    n.clear();
    enum { IN = 1, N5, N3, W, N4, N6, O, X, N8, LW, N10, N12 };
    double a, b;
    // HIGH : entrée → 620 Ω → 22 n → potentiomètre 10 k → 22 n → 620 Ω → sortie ; curseur → 15 n → entrée inverseuse
    potSplit (hfPos, hfPot, hfEnd, a, b);
    n.r (IN, N5, 620.0);  n.c (N5, N3, 22e-9);
    n.r (N3, W, a);       n.r (W, N4, b);
    n.c (N4, N6, 22e-9);  n.r (N6, O, 620.0);
    n.c (W, X, 15e-9);
    // LOW : entrée → 6k8 → potentiomètre 50 k → 6k8 → sortie ; C de chaque côté vers un nœud commun (12 k à la masse,
    // 10 n vers le curseur) ; curseur → 12 k → entrée inverseuse
    if (lowCap > 0.0)
    {
        potSplit (lfPos, lfPot, lfEnd, a, b);
        n.r (IN, N8, 6800.0); n.r (N8, LW, a); n.r (LW, N10, b); n.r (N10, O, 6800.0);
        n.c (N8, N12, lowCap); n.c (N10, N12, lowCap); n.c (LW, N12, 10e-9); n.r (N12, 0, 12e3);
        n.r (LW, X, 12e3);
    }
    else
    {
        // bande hors circuit : il reste la contre-réaction résistive, gain unité (6k8 + 25 k / 6k8 + 25 k)
        n.r (IN, N8, 6800.0 + 0.5 * lfPot); n.r (N8, X, 12e3); n.r (N8, O, 6800.0 + 0.5 * lfPot);
    }
    n.c (X, O, 470e-12);
    n.opAmp (0, X, O);
    n.output (O);
}

/** MID : pot entre entrée et sortie de l'ampli, curseur → condensateur, bobine (avec sa résistance), 2k7 → entrée
    inverseuse. index < 0 → bande hors circuit (ampli seul, gain unité). */
inline void buildMid (Net& n, double pos, int index)
{
    n.clear();
    enum { IN = 1, A, O, X, W, B1, B2, B3 };
    n.r (IN, A, 6800.0); n.r (A, O, 6800.0); n.r (A, X, 8200.0); n.c (X, O, 180e-12);
    if (index >= 0)
    {
        const auto& v = midValues[index];
        double a, b;
        potSplit (pos, midPot, midEnd, a, b);
        n.r (IN, W, a); n.r (W, O, b);
        n.c (W, B1, v.C); n.r (B1, B2, v.rL); n.l (B2, B3, v.L); n.r (B3, X, 2700.0);
    }
    n.r (O, 0, 10e3);
    n.opAmp (0, X, O);
    n.output (O);
}

/** HIGH PASS passif : source 1k2 → C → (bobine + résistance vers la masse) → C → charge 5k1. */
inline void buildHpf (Net& n, int index)
{
    n.clear();
    enum { IN = 1, A, M, Lr, O };
    const auto& v = hpfValues[index];
    n.r (IN, A, hpfSource); n.c (A, M, v.C); n.l (M, Lr, v.L); n.r (Lr, 0, v.rL); n.c (M, O, v.C); n.r (O, 0, hpfLoad);
    n.output (O);
}

/** Gain du chemin passe-haut hors circuit (la même charge, sans le filtre) : 5k1 / (1k2 + 5k1). */
constexpr double hpfFlatGain = hpfLoad / (hpfSource + hpfLoad);

/** Réponse en fréquence d'un StateSpace numérique, à f (Hz), pour fs. */
inline std::complex<double> response (const SS& ss, double f, double fs)
{
    using C = std::complex<double>;
    const int n = ss.n;
    const C zi = std::polar (1.0, -2.0 * 3.14159265358979323846 * f / fs);   // z^-1
    // (I - A z^-1) x = B
    C M[MaxStates][MaxStates + 1];
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j) M[i][j] = (i == j ? C (1.0) : C (0.0)) - ss.A[(size_t) (i * MaxStates + j)] * zi;
        M[i][n] = ss.B[(size_t) i];
    }
    for (int c = 0; c < n; ++c)
    {
        int piv = c;
        for (int r = c + 1; r < n; ++r) if (std::abs (M[r][c]) > std::abs (M[piv][c])) piv = r;
        if (piv != c) for (int q = 0; q <= n; ++q) std::swap (M[c][q], M[piv][q]);
        for (int r = c + 1; r < n; ++r)
        {
            const C fac = M[r][c] / M[c][c];
            for (int q = c; q <= n; ++q) M[r][q] -= fac * M[c][q];
        }
    }
    C x[MaxStates];
    for (int r = n - 1; r >= 0; --r)
    {
        C acc = M[r][n];
        for (int j = r + 1; j < n; ++j) acc -= M[r][j] * x[j];
        x[r] = acc / M[r][r];
    }
    C y = ss.D;
    for (int i = 0; i < n; ++i) y += zi * ss.C[(size_t) i] * x[i];
    return y;
}
} // namespace eq
} // namespace n73
