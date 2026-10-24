#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace n73
{
//==============================================================================
//  Circuit linéaire (résistances, condensateurs, bobines, amplis op idéaux) → filtre numérique en espace d'état.
//
//  Analyse nodale modifiée ; chaque condensateur et chaque bobine est remplacé par son modèle compagnon
//  trapézoïdal (la transformée bilinéaire du circuit), précompensé à la fréquence fWarp où la réponse numérique
//  est exacte. Les tensions de nœuds sont éliminées : il ne reste que les mémoires des composants réactifs.
//
//      y[n] = C·s[n-1] + D·u[n]
//      s[n] = A·s[n-1] + B·u[n]
//
//  Nœud 0 = masse ; nœud 1 = entrée, tenue par une source de tension idéale. Aucune allocation : tout est dans des
//  tableaux de taille fixe, la construction peut se faire sur le thread audio (quelques microsecondes).
//==============================================================================
template <int MaxStates>
struct StateSpace
{
    int n = 0;
    std::array<double, MaxStates * MaxStates> A {};
    std::array<double, MaxStates> B {}, C {};
    double D = 1.0;
};

template <int MaxNodes, int MaxElems, int MaxOps, int MaxStates>
class Netlist
{
public:
    void clear() { numNodes = 1; numElems = numOps = numStates = 0; out = 1; }

    /** a, b : numéros de nœuds (0 = masse). Les numéros se créent au fil de l'eau. */
    void r (int a, int b, double ohms)      { add (Kind::R, a, b, ohms); }
    void c (int a, int b, double farads)    { add (Kind::C, a, b, farads); }
    void l (int a, int b, double henries)   { add (Kind::L, a, b, henries); }
    void opAmp (int plus, int minus, int output)
    {
        ops[(size_t) numOps++] = { plus, minus, output };
        numNodes = std::max ({ numNodes, plus, minus, output });
    }
    void output (int node) { out = node; }

    /** Discrétise le circuit à la fréquence d'échantillonnage fs. Renvoie false si le système est singulier. */
    bool discretize (double fs, double fWarp, StateSpace<MaxStates>& ss) const
    {
        constexpr int MaxU = MaxNodes + MaxOps + 1;
        const int N = numNodes, U = N + numOps + 1;
        const double w0 = 2.0 * 3.14159265358979323846 * fWarp;
        const double K = w0 / std::tan (w0 / (2.0 * fs));   // 2/T précompensé

        std::array<double, MaxU * MaxU> M {};
        std::array<double, MaxU * (MaxStates + 1)> R {};    // [b | S]
        const int cols = numStates + 1;
        auto m = [&] (int i, int j) -> double& { return M[(size_t) (i * U + j)]; };
        auto rhs = [&] (int i, int j) -> double& { return R[(size_t) (i * cols + j)]; };
        auto stamp = [&] (int a, int b, double g)
        {
            if (a > 0) m (a - 1, a - 1) += g;
            if (b > 0) m (b - 1, b - 1) += g;
            if (a > 0 && b > 0) { m (a - 1, b - 1) -= g; m (b - 1, a - 1) -= g; }
        };

        std::array<double, MaxStates> g2 {}, sgn {};
        std::array<int, MaxStates> na {}, nb {};
        int k = 0;
        for (int e = 0; e < numElems; ++e)
        {
            const auto& el = elems[(size_t) e];
            if (el.kind == Kind::R) { stamp (el.a, el.b, 1.0 / el.v); continue; }
            const bool cap = el.kind == Kind::C;
            const double g = cap ? K * el.v : 1.0 / (K * el.v);
            stamp (el.a, el.b, g);
            // mémoire injectée : condensateur i = g·v - s  →  +s en a ; bobine i = g·v + s  →  -s en a
            const double dir = cap ? 1.0 : -1.0;
            if (el.a > 0) rhs (el.a - 1, k + 1) += dir;
            if (el.b > 0) rhs (el.b - 1, k + 1) -= dir;
            g2[(size_t) k] = 2.0 * g;
            sgn[(size_t) k] = cap ? -1.0 : 1.0;
            na[(size_t) k] = el.a; nb[(size_t) k] = el.b;
            ++k;
        }
        for (int o = 0; o < numOps; ++o)
        {
            const auto& op = ops[(size_t) o];
            const int row = N + o;
            if (op.out > 0) m (op.out - 1, row) += 1.0;
            if (op.plus > 0) m (row, op.plus - 1) += 1.0;
            if (op.minus > 0) m (row, op.minus - 1) -= 1.0;
        }
        const int src = N + numOps;
        m (0, src) += 1.0; m (src, 0) += 1.0;   // nœud 1 = entrée
        rhs (src, 0) = 1.0;

        // élimination de Gauss avec pivot partiel : M·Z = [b | S]
        for (int c = 0; c < U; ++c)
        {
            int piv = c;
            double best = std::abs (m (c, c));
            for (int r = c + 1; r < U; ++r)
                if (std::abs (m (r, c)) > best) { best = std::abs (m (r, c)); piv = r; }
            if (best < 1.0e-300) return false;
            if (piv != c)
            {
                for (int q = 0; q < U; ++q) std::swap (m (c, q), m (piv, q));
                for (int q = 0; q < cols; ++q) std::swap (rhs (c, q), rhs (piv, q));
            }
            const double inv = 1.0 / m (c, c);
            for (int r = c + 1; r < U; ++r)
            {
                const double f = m (r, c) * inv;
                if (f == 0.0) continue;
                for (int q = c; q < U; ++q) m (r, q) -= f * m (c, q);
                for (int q = 0; q < cols; ++q) rhs (r, q) -= f * rhs (c, q);
            }
        }
        for (int r = U - 1; r >= 0; --r)
        {
            const double inv = 1.0 / m (r, r);
            for (int q = 0; q < cols; ++q)
            {
                double acc = rhs (r, q);
                for (int j = r + 1; j < U; ++j) acc -= m (r, j) * rhs (j, q);
                rhs (r, q) = acc * inv;
            }
        }
        // rhs contient maintenant Z = M^-1·[b | S] (tensions de nœuds pour u = 1 et pour chaque mémoire = 1)
        auto v = [&] (int node, int col) { return node > 0 ? rhs (node - 1, col) : 0.0; };
        ss.n = numStates;
        ss.A.fill (0.0); ss.B.fill (0.0); ss.C.fill (0.0);   // zéros au-delà de n : le calcul se fait à taille fixe
        ss.D = v (out, 0);
        for (int j = 0; j < numStates; ++j)
        {
            ss.C[(size_t) j] = v (out, j + 1);
            const double vb = v (na[(size_t) j], 0) - v (nb[(size_t) j], 0);
            ss.B[(size_t) j] = g2[(size_t) j] * vb;
            for (int i = 0; i < numStates; ++i)
            {
                const double va = v (na[(size_t) j], i + 1) - v (nb[(size_t) j], i + 1);
                ss.A[(size_t) (j * MaxStates + i)] = g2[(size_t) j] * va + (i == j ? sgn[(size_t) j] : 0.0);
            }
        }
        return true;
    }

    int states() const { return numStates; }

private:
    enum class Kind { R, C, L };
    struct Elem { Kind kind; int a, b; double v; };
    struct Op { int plus, minus, out; };

    void add (Kind kind, int a, int b, double v)
    {
        elems[(size_t) numElems++] = { kind, a, b, v };
        numNodes = std::max ({ numNodes, a, b });
        if (kind != Kind::R) ++numStates;
    }

    std::array<Elem, MaxElems> elems {};
    std::array<Op, MaxOps> ops {};
    int numNodes = 1, numElems = 0, numOps = 0, numStates = 0, out = 1;
};

//==============================================================================
/** Exécute un StateSpace : une mémoire par composant réactif, en double. */
template <int MaxStates>
struct StateSpaceRunner
{
    std::array<double, MaxStates> s {};

    void reset() { s.fill (0.0); }

    inline double process (const StateSpace<MaxStates>& ss, double u) noexcept
    {
        // taille fixe (les coefficients au-delà de n sont nuls) : boucles que le compilateur déroule et vectorise
        double y = ss.D * u;
        for (int i = 0; i < MaxStates; ++i) y += ss.C[(size_t) i] * s[(size_t) i];
        std::array<double, MaxStates> t;
        for (int j = 0; j < MaxStates; ++j)
        {
            const double* a = ss.A.data() + j * MaxStates;
            double acc = ss.B[(size_t) j] * u;
            for (int i = 0; i < MaxStates; ++i) acc += a[i] * s[(size_t) i];
            t[(size_t) j] = acc;
        }
        s = t;
        return y;
    }
};
} // namespace n73
