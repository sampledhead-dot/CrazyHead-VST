// CrazyHead · generador de patrones (sin JUCE). Port 1:1 de CrazyHead (index.html): mulberry32, genCell, pinCell,
// regen, pinApply, dice, cycleMut, detección de golpes y troceado. Cualquier cambio aquí exige el mismo cambio en la
// app web y pasar tests/parity.js.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace ch
{
constexpr int kMaxSeg = 16, kMaxSteps = 64, kMaxHits = 6;

inline double clampd (double x, double a, double b) { return x < a ? a : (x > b ? b : x); }
inline double jsRound (double x) { return std::floor (x + 0.5); }   // Math.round de JavaScript

// mulberry32 (misma secuencia que la app)
struct Rng
{
    uint32_t a;
    explicit Rng (uint32_t s = 1) : a (s) {}
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

struct Seg { double a = 0, b = 0; };

struct Hit
{
    double o = 0;      // desfase dentro del paso (0..1)
    int s = 0;         // chop
    double v = 1, d = 1;
    int r = 0, p = 0, t = 0, n = 1, l = 0;
};

struct Cell
{
    int n = 0;
    Hit h[kMaxHits];
    bool empty() const { return n == 0; }
};

struct GenParams
{
    int dens = 60, sync = 40, fid = 60, ratch = 15, dbl = 10, vmin = 60, vmax = 115, glen = 0, rev = 8, pitP = 0, pitR = 5, hum = 0, mut = 0;
    int grid = 16;        // 4, 8, 16 o 32
    int loopBars = 2;     // compases que dura el loop original
    int len = 2;          // compases del patrón (1..4)
    uint32_t seed = 1;
    int pinProb = 100;    // 100 = siempre
    uint32_t pinBars = 0; // 0 = todos los compases
    bool pool[kMaxSeg];
    int pins[16];
    GenParams() { for (auto& p : pool) p = true; for (auto& p : pins) p = -1; }
};

inline int origSlice (const GenParams& D, const Seg* sg, int nsg, double dur, int i)
{
    const int cyc = D.loopBars * 16;
    const double pos = ((double) (i % cyc) / (double) cyc) * dur;
    int s = 0;
    for (int k = 0; k < nsg; ++k) if (sg[k].a <= pos + 0.005) s = k;
    return s;
}

inline bool pinCell (const GenParams& D, const Seg* sg, int nsg, int i, Cell& out)
{
    const int s = D.pins[i % 16];
    if (! (s >= 0) || s >= nsg) return false;
    if (D.pinBars && ! ((D.pinBars >> (uint32_t) std::min (31, i / 16)) & 1u)) return false;
    if (D.pinProb < 100)
    {
        const uint32_t sd = (uint32_t) ((uint64_t) (int64_t) (int32_t) D.seed + (uint64_t) i * 2654435761ULL) ^ 0x5bd1e995u;
        Rng r (sd);
        if (r.next() * 100.0 >= D.pinProb) return false;
    }
    out.n = 1; out.h[0] = Hit();
    out.h[0].s = s;
    out.h[0].v = clampd ((D.vmax ? D.vmax : 115) / 127.0 * (((i % 16) % 4 == 0) ? 1.0 : .92), .05, 1);
    return true;
}

inline bool genCell (const GenParams& D, const Seg* sg, int nsg, double dur, int i, Rng& rng, bool force, Cell& out)
{
    const double r0 = rng.next();
    out.n = 0;
    if (nsg <= 0) return false;
    int pool[kMaxSeg], np = 0;
    for (int k = 0; k < nsg; ++k) if (D.pool[k]) pool[np++] = k;
    if (! np) return false;
    const int b = i % 16, grid = D.grid;
    if (! force && ((grid == 4 && b % 4) || (grid == 8 && b % 2))) return false;
    const double sy = D.sync / 100.0;
    const double w = b % 4 == 0 ? 1 - .35 * sy : (b % 4 == 2 ? .6 + .4 * sy : .3 + .7 * sy);
    double p = clampd (D.dens / 100.0 * w * 1.35, 0, 1);
    if (b == 0) p = std::max (p, .9 * std::min (1.0, D.dens / 40.0));
    if (! force && r0 >= p) return false;
    auto pick = [&] { return pool[(int) std::floor (rng.next() * np)]; };
    const int o = origSlice (D, sg, nsg, dur, i);
    bool inPool = false; for (int k = 0; k < np; ++k) if (pool[k] == o) inPool = true;
    const int s0 = (inPool && rng.next() < D.fid / 100.0) ? o : pick();
    double v = (D.vmin + rng.next() * (D.vmax - D.vmin)) / 127.0;
    if (b % 4 == 0) v *= 1.08;
    v = clampd (v, .05, 1);
    const double d = 1 - rng.next() * D.glen / 100.0 * .85;
    const int r = rng.next() < D.rev / 100.0 ? 1 : 0;
    int pt = 0;
    if (rng.next() < D.pitP / 100.0) pt = (int) jsRound ((rng.next() * 2 - 1) * D.pitR);
    const int t = D.hum ? (int) jsRound ((rng.next() * 2 - 1) * D.hum) : 0;
    const int rp = grid == 32 ? std::max (D.ratch, 50) : D.ratch;
    if (rng.next() < rp / 100.0)
    {
        int n; if (rng.next() < .6) n = 2; else n = rng.next() < .65 ? 3 : 4;
        for (int k = 0; k < n; ++k)
        {
            Hit h; h.o = (double) k / n; h.s = s0; h.v = clampd (v * (1 - k * .12), .05, 1); h.d = d; h.r = r; h.p = pt; h.t = t; h.n = n;
            out.h[out.n++] = h;
        }
    }
    else { Hit h; h.o = 0; h.s = s0; h.v = v; h.d = d; h.r = r; h.p = pt; h.t = t; h.n = 1; out.h[out.n++] = h; }
    if (rng.next() < D.dbl / 100.0 && np > 1)
    {
        int s2 = pick();
        if (s2 == s0) { int ix = 0; for (int k = 0; k < np; ++k) if (pool[k] == s0) ix = k; s2 = pool[(ix + 1) % np]; }
        Hit h; h.o = 0; h.s = s2; h.v = v * .7; h.d = d; h.r = 0; h.p = 0; h.t = t; h.n = 1; h.l = 1;
        out.h[out.n++] = h;
    }
    return true;
}

struct Pattern
{
    Cell cell[kMaxSteps];
    int steps = 32;
    uint32_t cyc = 0;
};

inline void regen (const GenParams& D, const Seg* sg, int nsg, double dur, Pattern& pat)
{
    pat.steps = std::min (kMaxSteps, D.len * 16); pat.cyc = 0;
    for (int i = 0; i < kMaxSteps; ++i) pat.cell[i].n = 0;
    Rng rng (D.seed);
    for (int i = 0; i < pat.steps; ++i)
    {
        Cell c, pc;
        genCell (D, sg, nsg, dur, i, rng, false, c);
        pat.cell[i] = pinCell (D, sg, nsg, i, pc) ? pc : c;
    }
}

inline void dice (const GenParams& D, const Seg* sg, int nsg, double dur, Pattern& pat, uint32_t seed)
{
    Rng rng (seed);
    for (int i = 0; i < pat.steps; ++i)
    {
        Cell pc;
        if (rng.next() < .25 && ! pinCell (D, sg, nsg, i, pc)) { Cell c; genCell (D, sg, nsg, dur, i, rng, false, c); pat.cell[i] = c; }
    }
}

inline void cycleMut (const GenParams& D, const Seg* sg, int nsg, double dur, Pattern& pat)
{
    pat.cyc++;
    if (D.mut > 0 && nsg > 0)
    {
        Rng rng ((uint32_t) (D.seed + pat.cyc * 977u));
        const double f = D.mut / 100.0 * .9;
        for (int i = 0; i < pat.steps; ++i)
        {
            Cell pc;
            if (rng.next() < f && ! pinCell (D, sg, nsg, i, pc)) { Cell c; genCell (D, sg, nsg, dur, i, rng, false, c); pat.cell[i] = c; }
        }
    }
}

// ---------- detección de golpes (flujo de energía en dos bandas) ----------
inline std::vector<double> detectOnsets (const float* L, const float* R, size_t n, double sr, double sens, double gapMs)
{
    std::vector<double> out;
    const size_t hop = std::max<size_t> (64, (size_t) jsRound (sr * .003)), nf = n / hop;
    if (nf < 8) return out;
    std::vector<float> eL (nf), eH (nf), flux (nf, 0.0f);
    double lp = 0; const double a = 1 - std::exp (-2 * M_PI * 250 / sr);
    for (size_t f = 0; f < nf; ++f)
    {
        double sl = 0, sh = 0;
        for (size_t j = f * hop, e = j + hop; j < e; ++j)
        {
            double x = R ? (L[j] + R[j]) / 2.0 : (double) L[j];
            lp += a * (x - lp); sl += std::fabs (lp); sh += std::fabs (x - lp);
        }
        eL[f] = (float) std::log (1 + 80 * sl / (double) hop); eH[f] = (float) std::log (1 + 80 * sh / (double) hop);
    }
    for (size_t f = 9; f + 1 < nf; ++f)
    {
        double rl = 0, rh = 0;
        for (size_t q = f - 8; q <= f - 3; ++q) { rl += eL[q]; rh += eH[q]; }
        rl /= 6; rh /= 6;
        flux[f] = (float) (std::max (0.0, std::max ((double) eL[f], (double) eL[f + 1]) - rl) + std::max (0.0, std::max ((double) eH[f], (double) eH[f + 1]) - rh) * 1.2);
    }
    std::vector<double> pre (nf + 1, 0.0);
    for (size_t f = 0; f < nf; ++f) pre[f + 1] = pre[f] + flux[f];
    const long W = std::max (4L, (long) jsRound (.25 * sr / (double) hop));
    const double factor = 1.15 + (100 - sens) / 100 * 4.5, floorv = .02 + (100 - sens) / 100 * .05;
    const long gap = std::max (1L, (long) jsRound (gapMs / 1000 * sr / (double) hop));
    struct K { long f; double v; };
    std::vector<K> kept;
    for (long f = 1; f < (long) nf - 1; ++f)
    {
        const long lo = std::max (0L, f - W), hi = std::min ((long) nf, f + W + 1);
        const double mean = (pre[(size_t) hi] - pre[(size_t) lo]) / (double) (hi - lo);
        if (flux[(size_t) f] > mean * factor + floorv && flux[(size_t) f] >= flux[(size_t) f - 1] && flux[(size_t) f] >= flux[(size_t) f + 1])
        {
            if (! kept.empty() && f - kept.back().f < gap) { if (flux[(size_t) f] > kept.back().v) { kept.back().f = f; kept.back().v = flux[(size_t) f]; } }
            else kept.push_back ({ f, (double) flux[(size_t) f] });
        }
    }
    for (auto& k : kept)
    {
        long f = k.f;
        while (f > 1 && flux[(size_t) f - 1] > .3 * k.v && k.f - f < 5) --f;
        out.push_back (std::max (0.0, (double) f * (double) hop / sr - .002));
    }
    return out;
}

// Marcadores (segundos, el primero siempre 0). maxCuts = máximo de trozos.
inline std::vector<double> autoChop (const float* L, const float* R, size_t n, double sr, double sens, double gap, int maxCuts)
{
    std::vector<double> on;
    for (int i = 0; i < 14; ++i)
    {
        auto all = detectOnsets (L, R, n, sr, sens, gap);
        on.clear(); for (double t : all) if (t > .015) on.push_back (t);
        if ((int) on.size() + 1 <= maxCuts) break;
        gap = gap * 1.3 + 8;
    }
    if ((int) on.size() + 1 > maxCuts)
    {
        std::vector<double> th;
        for (int i = 0; i < maxCuts - 1; ++i) th.push_back (on[(size_t) std::floor ((double) i * (double) on.size() / (double) (maxCuts - 1))]);
        on = th;
    }
    std::vector<double> m { 0.0 };
    m.insert (m.end(), on.begin(), on.end());
    return m;
}

inline std::vector<double> gridChop (double dur, int N)
{
    std::vector<double> m; N = std::max (1, std::min (N, kMaxSeg));
    for (int k = 0; k < N; ++k) m.push_back (dur * k / N);
    return m;
}
} // namespace ch
