// CrazyHead · motor de audio de una pista (sin JUCE): loop troceado, secuenciador sincronizado con el DAW,
// golpes por MIDI, filtro, bitcrush, drive, delay, reverb y efectos aleatorios (rfx) como en la app web.
#pragma once
#include "CrazyGen.h"
#include <array>
#include <atomic>
#include <memory>

namespace ch
{
struct Loop
{
    double sr = 44100, dur = 0;
    int nseg = 0; Seg seg[kMaxSeg];
    std::vector<float> L, R, Lr, Rr;          // Lr/Rr: copia invertida (para reverse)
    size_t size() const { return L.size(); }

    static std::shared_ptr<Loop> make (const float* l, const float* r, size_t n, double sampleRate, const std::vector<double>& markers)
    {
        auto lp = std::make_shared<Loop>();
        lp->sr = sampleRate; lp->dur = (double) n / sampleRate;
        lp->L.assign (l, l + n); lp->R.assign (r ? r : l, (r ? r : l) + n);
        lp->Lr.assign (lp->L.rbegin(), lp->L.rend()); lp->Rr.assign (lp->R.rbegin(), lp->R.rend());
        std::vector<double> m = markers; if (m.empty() || m[0] > 0) m.insert (m.begin(), 0.0);
        std::sort (m.begin(), m.end());
        lp->nseg = (int) std::min<size_t> (m.size(), (size_t) kMaxSeg);
        for (int k = 0; k < lp->nseg; ++k) { lp->seg[k].a = m[(size_t) k]; lp->seg[k].b = k + 1 < lp->nseg ? m[(size_t) k + 1] : lp->dur; }
        return lp;
    }
};

struct Params
{
    int dens = 60, sync = 40, fid = 60, ratch = 15, dbl = 10, vmin = 60, vmax = 115, glen = 0, rev = 8, pitP = 0, pitR = 5, hum = 0, mut = 0;
    int grid = 16, len = 2, loopBars = 2, swing = 50, pinProb = 100;
    uint32_t pinBars = 0;
    bool patOn = true, mono = true, follow = true;
    int tune = 0, fine = 0; double vol = 80, pan = 0;
    bool flOn = false; int flType = 0, flF = 70, flQ = 20;      // flType: 0 paso bajo, 1 paso alto
    bool btOn = false; int bit = 8; bool drOn = false; int dr = 30;
    bool dlOn = false; int dlDiv = 1, dlFb = 40, dlMix = 35;    // dlDiv: 0=1/4 1=1/8. 2=1/8 3=1/16 4=1/4T
    int rv = 0, rvSz = 50;
    bool rOn = false; int rTrig = 0; int rProb = 60, rAmt = 60, rGlide = 40;   // rTrig: 0 golpe, 1 cada 16, 2 cada 8, 3 cada 4, 4 compás
    bool rBack = true, rBySlice = true, rVary = false;
    bool tgFlt = true, tgDly = false, tgRev = false, tgBit = false, tgDrv = false, tgPan = false, tgGate = false;
    double bpm = 120;
    bool genEqual (const Params& o) const
    {
        return dens == o.dens && sync == o.sync && fid == o.fid && ratch == o.ratch && dbl == o.dbl && vmin == o.vmin && vmax == o.vmax && glen == o.glen
            && rev == o.rev && pitP == o.pitP && pitR == o.pitR && hum == o.hum && mut == o.mut && grid == o.grid && len == o.len && loopBars == o.loopBars
            && pinProb == o.pinProb && pinBars == o.pinBars;
    }
};

inline double divMul (int d) { static const double m[] = { 1.0, .75, .5, .25, 2.0 / 3.0 }; return m[std::max (0, std::min (4, d))]; }

struct Sm
{
    double v = 0, t = 0, k = 1;
    void set (double target, double tc, double sr) { t = target; k = tc <= 0 ? 1 : 1 - std::exp (-1.0 / (tc * sr)); }
    void jump (double x) { v = t = x; k = 1; }
    double tick() { v += (t - v) * k; return v; }
};

class Engine
{
public:
    enum { FF, FQ, DS, RS, PN, GD, GB, GR, NSM };

    Engine() { for (auto& d : disp) d.store (-1); }

    void prepare (double sampleRate)
    {
        sr = sampleRate; now = 0; nq = 0;
        for (auto& v : voices) v.on = false;
        const size_t dl = (size_t) (sr * 3.2); for (auto& d : dly) d.assign (dl, 0.0f); dpos = 0;
        const int base[4] = { 1117, 1481, 1759, 2087 };
        for (int i = 0; i < 4; ++i) { rvLen[i] = (int) (base[i] * sr / 44100.0); rv[i].assign ((size_t) rvLen[i], 0.0f); rvPos[i] = 0; rvLp[i] = 0; }
        for (auto& f : fl) f = Bq();
        flC = 0; applyBase (true); active = false; lastStep = -1;
    }

    void setLoop (std::shared_ptr<const Loop> l) { loopHold = l; loop = l.get(); for (auto& v : voices) v.on = false; regenerate (false); }
    const Loop* getLoop() const { return loop; }

    void setParams (const Params& np)
    {
        const bool g = ! np.genEqual (P);
        const bool fx = np.flOn != P.flOn || np.flType != P.flType || np.flF != P.flF || np.flQ != P.flQ || np.btOn != P.btOn || np.bit != P.bit || np.drOn != P.drOn || np.dr != P.dr
                     || np.dlOn != P.dlOn || np.dlDiv != P.dlDiv || np.dlFb != P.dlFb || np.dlMix != P.dlMix || np.rv != P.rv || np.rvSz != P.rvSz || np.vol != P.vol || np.pan != P.pan
                     || np.bpm != P.bpm || np.rOn != P.rOn || np.tgFlt != P.tgFlt || np.tgBit != P.tgBit || np.tgDrv != P.tgDrv;
        P = np; G.dens = P.dens; G.sync = P.sync; G.fid = P.fid; G.ratch = P.ratch; G.dbl = P.dbl; G.vmin = P.vmin; G.vmax = P.vmax; G.glen = P.glen; G.rev = P.rev;
        G.pitP = P.pitP; G.pitR = P.pitR; G.hum = P.hum; G.mut = P.mut; G.grid = P.grid; G.len = P.len; G.loopBars = P.loopBars; G.pinProb = P.pinProb; G.pinBars = P.pinBars;
        if (g) regenerate (false);
        if (fx) applyBase (false);
    }
    const Params& params() const { return P; }

    // ---- patrón ----
    void setSeed (uint32_t s) { G.seed = s; }
    uint32_t seed() const { return G.seed; }
    void setPin (int step16, int chop) { G.pins[step16 & 15] = chop; regenerate (false); }
    int pin (int step16) const { return G.pins[step16 & 15]; }
    void setPool (int chop, bool on) { if (chop >= 0 && chop < kMaxSeg) { G.pool[chop] = on; regenerate (false); } }
    bool pool (int chop) const { return G.pool[chop]; }
    void clearPins() { for (auto& p : G.pins) p = -1; regenerate (false); }
    void regenerate (bool newSeed, uint32_t seedIfNew = 0)
    {
        if (newSeed) G.seed = seedIfNew;
        if (! loop) { for (auto& d : disp) d.store (-1); pat.steps = std::min (kMaxSteps, P.len * 16); for (auto& c : pat.cell) c.n = 0; return; }
        regen (G, loop->seg, loop->nseg, loop->dur, pat); publish();
    }
    void doDice (uint32_t seed) { if (! loop) return; dice (G, loop->seg, loop->nseg, loop->dur, pat, seed); publish(); }
    const Pattern& pattern() const { return pat; }
    int displayStep (int i) const { return disp[(size_t) i].load(); }
    int currentStep() const { return curStep.load(); }
    float peak() const { return pk.load(); }

    // ---- notas MIDI (chop = nota − 36) ----
    void noteOn (int sampleOffset, int note, float vel)
    {
        const int s = note - 36; if (! loop || s < 0 || s >= loop->nseg) return;
        Ev e; e.at = now + sampleOffset; e.type = 0; e.h.s = s; e.h.v = clampd (vel, .05, 1); e.sd = 60.0 / P.bpm / 4; push (e);
    }

    void process (float* outL, float* outR, int n, bool playing, double ppq)
    {
        if (playing && P.patOn && loop && pat.steps > 0)
        {
            const double p1 = ppq + (double) n / sr * P.bpm / 60.0;
            for (long long gs = (long long) std::ceil (ppq * 4 - 1e-7); (double) gs / 4.0 < p1 - 1e-7; ++gs)
            {
                if (gs < 0) continue;
                const long long at = now + (long long) std::llround (((double) gs / 4.0 - ppq) * 60.0 / P.bpm * sr);
                scheduleStep (gs, at, 60.0 / P.bpm / 4);
            }
        }
        int pos = 0; float mx = 0;
        while (pos < n)
        {
            while (nq && q[0].at <= now + pos) apply (pop());
            int next = n; if (nq) next = (int) std::min<long long> (n, q[0].at - now);
            if (next <= pos) next = pos + 1;
            render (outL + pos, outR + pos, next - pos);
            pos = next;
        }
        for (int i = 0; i < n; ++i) mx = std::max (mx, std::max (std::fabs (outL[i]), std::fabs (outR[i])));
        pk.store (std::max (mx, pk.load() * .92f));
        now += n;
    }

private:
    struct Ev { long long at = 0; int type = 0; Hit h; double sd = 0; int id = 0; double val = 0, tc = 0; double val2 = 0; };
    struct Voice
    {
        bool on = false; const float* L = nullptr; const float* R = nullptr; size_t len = 0;
        double pos = 0, inc = 1, pkv = 1; long total = 0, done = 0, att = 0, rel = 0;
        bool choke = false; long chokeN = 1, chokeLeft = 0; double chokeG = 1;
    };
    struct Bq { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    double sr = 44100; long long now = 0;
    Params P; GenParams G; Pattern pat; std::atomic<int> disp[kMaxSteps]; std::atomic<int> curStep { 0 }; std::atomic<float> pk { 0 };
    const Loop* loop = nullptr; std::shared_ptr<const Loop> loopHold;
    std::array<Voice, 24> voices;
    Ev q[512]; int nq = 0;
    Sm sm[NSM]; bool active = false; long long lastStep = -1;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; Bq fl[2]; int flC = 0; bool flOnNow = false;
    std::vector<float> dly[2]; size_t dpos = 0; double dTime = 0, dTimeT = 0;
    std::vector<float> rv[4]; int rvLen[4] {}; int rvPos[4] {}; double rvLp[4] {}; double rvG[4] { .8, .8, .8, .8 };
    bool rgateNext = false; int rgateC = 1; double rgateDepth = .5;

    void publish() { for (int i = 0; i < kMaxSteps; ++i) disp[(size_t) i].store (i < pat.steps && pat.cell[i].n ? pat.cell[i].h[0].s : -1); }

    void push (const Ev& e)
    {
        if (nq >= 512) return;
        int i = nq++; while (i > 0 && q[i - 1].at > e.at) { q[i] = q[i - 1]; --i; } q[i] = e;
    }
    Ev pop() { Ev e = q[0]; for (int i = 1; i < nq; ++i) q[i - 1] = q[i]; --nq; return e; }

    // valores base (como eff() de la app)
    double baseFreq() const { return (P.flOn ? 60 * std::pow (16000.0 / 60.0, P.flF / 100.0) : 16000.0); }
    double baseQ() const { return P.flOn ? .5 + P.flQ / 100.0 * 13 : (P.rOn && P.tgFlt ? .5 + 10 / 100.0 * 13 : .5); }
    double baseDs() const { return P.dlOn ? P.dlMix / 100.0 : 0; }
    double baseRs() const { return P.rv / 100.0 * .8; }
    double btMix() const { return P.btOn ? 1 : 0; }
    double drMix() const { return P.drOn ? 1 : 0; }
    void applyBase (bool jump)
    {
        const double both = (btMix() > 0 && drMix() > 0) ? .7 : 1;
        const double tg[NSM] = { baseFreq(), baseQ(), baseDs(), baseRs(), clampd (P.pan / 100.0, -1, 1), (1 - btMix()) * (1 - drMix()), btMix() * both, drMix() * both };
        const double tc[NSM] = { .03, .03, .03, .03, .02, .02, .02, .02 };
        for (int i = 0; i < NSM; ++i) { if (jump) sm[i].jump (tg[i]); else sm[i].set (tg[i], tc[i], sr); }
        dTimeT = clampd (60.0 / P.bpm * divMul (P.dlDiv), .01, 2.9);
        if (jump) dTime = dTimeT;
        const double rt = .4 + P.rvSz / 100.0 * 2.2;
        for (int i = 0; i < 4; ++i) rvG[i] = std::pow (10.0, -3.0 * rvLen[i] / (rt * sr));
    }

    void scheduleStep (long long gs, long long at, double sd)
    {
        const int Lp = pat.steps, i = (int) (gs % Lp);
        if (i == 0 && gs > 0) { cycleMut (G, loop->seg, loop->nseg, loop->dur, pat); publish(); }
        curStep.store (i);
        const Cell& cell = pat.cell[i];
        const double swv = P.swing;
        const long long ts = at + (long long) std::llround ((i & 1) ? (2 * swv / 100.0 - 1) * sd * sr : 0.0);
        rgateNext = false;
        rfxStep (i, cell, ts, sd);
        if (cell.empty()) return;
        if (rgateNext)
        {
            const Hit& h = cell.h[0]; const int c = rgateC; const double y = clampd ((rgateDepth - .4) / .6, 0, 1), frac = 1 - .75 * y;
            for (int k = 0; k < c; ++k)
            {
                Ev e; e.at = ts + (long long) std::llround ((double) k / c * sd * sr); e.type = 0; e.sd = sd;
                e.h.o = 0; e.h.s = h.s; e.h.v = clampd (h.v * (k ? .92 : 1), .05, 1); e.h.d = frac; e.h.r = h.r; e.h.p = h.p; e.h.t = 0; e.h.n = c; push (e);
            }
            return;
        }
        for (int k = 0; k < cell.n; ++k)
        {
            Ev e; e.at = ts + (long long) std::llround ((cell.h[k].o * sd + cell.h[k].t / 1000.0) * sr); e.type = 0; e.h = cell.h[k]; e.sd = sd; push (e);
        }
    }

    void setP (int id, double v, double tc, long long at, bool send)   // programa un cambio de parámetro (y su vuelta)
    {
        Ev e; e.at = at; e.type = 1; e.id = id; e.val = v; e.tc = tc; push (e); (void) send;
    }
    double baseOf (int id) const
    {
        switch (id) { case FF: return baseFreq(); case FQ: return baseQ(); case DS: return baseDs(); case RS: return baseRs(); case PN: return clampd (P.pan / 100.0, -1, 1); }
        return 0;
    }
    void autoP (int id, double v, long long t, double tc, bool back, double sd, double hold)
    {
        setP (id, v, tc, t, false);
        const bool send = id == DS || id == RS;
        if (back || send) setP (id, -1e9 /* = valor base al llegar */, send ? .05 : .02, t + (long long) std::llround (sd * (hold > 0 ? hold : .9) * sr), send);
    }
    void autoMix (bool bit, double mix, long long t, double tc, bool back, double sd)
    {
        const double other = bit ? drMix() : btMix(), baseM = bit ? btMix() : drMix();
        Ev e; e.at = t; e.type = 2; e.id = bit ? 1 : 0; e.val = mix * (other > 0 ? .7 : 1); e.val2 = (1 - mix) * (1 - other); e.tc = tc; push (e);
        if (back) { Ev b; b.at = t + (long long) std::llround (sd * .9 * sr); b.type = 2; b.id = bit ? 1 : 0; b.val = baseM * (other > 0 ? .7 : 1); b.val2 = (1 - baseM) * (1 - other); b.tc = .02; push (b); }
    }
    void rfxStep (int li, const Cell& cell, long long t, double sd)
    {
        if (! P.rOn) return;
        bool fire = false;
        switch (P.rTrig) { case 0: fire = ! cell.empty(); break; case 1: fire = true; break; case 2: fire = li % 2 == 0; break; case 3: fire = li % 4 == 0; break; case 4: fire = li % 16 == 0; break; }
        if (! fire) return;
        const int s0 = ! cell.empty() ? cell.h[0].s : -1;
        const uint64_t sd0 = P.rBySlice && s0 >= 0 ? (uint64_t) G.seed * 13u + (uint64_t) s0 * 104729u
                                                    : (uint64_t) G.seed * 31u + (uint64_t) li * 7919u + (P.rVary ? (uint64_t) pat.cyc * 4099u : 0u);
        Rng rng ((uint32_t) sd0);
        const double p = rng.next(), r1 = rng.next(), r2 = rng.next(), r3 = rng.next(), r4 = rng.next(), r5 = rng.next(), r6 = rng.next(), r7 = rng.next(), r8 = rng.next(), r9 = rng.next();
        (void) r3;
        if (p * 100 >= P.rProb) return;
        const double am = P.rAmt / 100.0, tc = std::max (.004, P.rGlide / 100.0 * sd * .5);
        const bool back = P.rBack;
        if (P.tgFlt)
        {
            const bool hp = P.flOn && P.flType == 1, norm = 0; (void) norm;
            const double nm = hp ? r1 * am * .8 : 1 - r1 * am * .9;
            autoP (FF, 60 * std::pow (16000.0 / 60.0, nm), t, tc, back, sd, 0);
            autoP (FQ, .7 + r9 * am * 7, t, tc, back, sd, 0);
        }
        const double hold = P.rTrig == 4 ? 15 : P.rTrig == 3 ? 3.6 : P.rTrig == 2 ? 1.8 : .95;
        if (P.tgDly) autoP (DS, r2 > .35 ? (r2 - .35) / .65 * am * .6 : 0, t, tc, back, sd, hold);
        if (P.tgRev) autoP (RS, r4 > .35 ? (r4 - .35) / .65 * am * .8 : 0, t, tc, back, sd, hold);
        if (P.tgBit) autoMix (true, r5 > 1 - am ? 1 : 0, t, tc, back, sd);
        if (P.tgDrv) autoMix (false, r6 > 1 - am ? 1 : 0, t, tc, back, sd);
        if (P.tgPan) autoP (PN, (r7 * 2 - 1) * am, t, tc, back, sd, 0);
        if (P.tgGate) { rgateNext = true; rgateC = r8 > .5 ? 2 : 1; rgateDepth = .5 + .5 * am; }
    }

    void apply (const Ev& e)
    {
        if (e.type == 0) hit (e.h, e.sd);
        else if (e.type == 1)
        {
            const double v = e.val < -1e8 ? baseOf (e.id) : e.val;
            sm[e.id].set (v, e.tc, sr);
        }
        else if (e.type == 2)
        {
            sm[e.id ? GB : GR].set (e.val, e.tc, sr); sm[GD].set (e.val2, e.tc, sr);
        }
    }

    void hit (const Hit& h, double stepDur)
    {
        if (! loop || h.s < 0 || h.s >= loop->nseg) return;
        const Seg& sg = loop->seg[h.s];
        const double ob = P.loopBars * 4 * 60.0 / loop->dur;
        const double rate = std::pow (2.0, ((h.p) + P.tune + P.fine / 100.0) / 12.0) * (P.follow ? P.bpm / ob : 1);
        if (! (rate > .05 && rate < 16)) return;
        double dur = (sg.b - sg.a) / rate * h.d;
        if (h.n > 1) dur = std::min (dur, stepDur / h.n * .95);
        dur = std::max (dur, .02);
        if (P.mono && ! h.l) for (auto& v : voices) if (v.on && ! v.choke) { v.choke = true; v.chokeN = std::max (1L, (long) (.004 * sr)); v.chokeLeft = v.chokeN; v.chokeG = envOf (v); }
        Voice* vp = nullptr; for (auto& v : voices) if (! v.on) { vp = &v; break; }
        if (! vp) { long best = 1L << 60; for (auto& v : voices) { const long rem = v.total - v.done; if (rem < best) { best = rem; vp = &v; } } }
        Voice& v = *vp;
        v = Voice(); v.on = true;
        v.L = h.r ? loop->Lr.data() : loop->L.data(); v.R = h.r ? loop->Rr.data() : loop->R.data(); v.len = loop->size();
        const double off = h.r ? loop->dur - sg.b : sg.a;
        v.pos = off * loop->sr; v.inc = rate * loop->sr / sr; v.pkv = clampd (h.v, .02, 1);
        v.total = std::max (1L, (long) std::llround (dur * sr)); v.att = std::max (1L, (long) (.002 * sr));
        v.rel = std::max (1L, (long) (std::min (.012, dur * .5) * sr));
    }
    double envOf (const Voice& v) const
    {
        double e = v.done < v.att ? (double) v.done / (double) v.att : 1.0;
        if (v.done > v.total - v.rel) e = std::min (e, (double) (v.total - v.done) / (double) v.rel);
        return std::max (0.0, e) * v.pkv;
    }

    static double bitq (double x, double L) { return std::floor (x * L + .5) / L; }

    void setBiquad (double f, double qdb, bool hp)
    {
        f = clampd (f, 20, std::min (22000.0, sr * .45));
        const double w = 2 * M_PI * f / sr, cw = std::cos (w), sw = std::sin (w), Q = std::pow (10.0, qdb / 20.0), al = sw / (2 * Q);
        double bb0, bb1, bb2;
        if (! hp) { bb0 = (1 - cw) / 2; bb1 = 1 - cw; bb2 = bb0; } else { bb0 = (1 + cw) / 2; bb1 = -(1 + cw); bb2 = bb0; }
        const double a0 = 1 + al;
        b0 = bb0 / a0; b1 = bb1 / a0; b2 = bb2 / a0; a1 = -2 * cw / a0; a2 = (1 - al) / a0;
    }

    void render (float* oL, float* oR, int n)
    {
        const double vol = std::pow (clampd (P.vol / 100.0, 0, 1), 1.5);
        const double lvl = std::pow (2.0, 0.0);
        const bool useFl = P.flOn || (P.rOn && P.tgFlt);
        const double dfb = clampd (P.dlFb / 100.0, 0, .7);
        const double bitL = std::pow (2.0, P.bit - 1.0), kk = 1 + std::round (P.dr / 4.0) * 4 * .14, tk = std::tanh (kk);
        const bool hp = P.flOn && P.flType == 1;
        const bool useDl = P.dlOn || (P.rOn && P.tgDly), useRv = P.rv > 0 || (P.rOn && P.tgRev);
        (void) lvl;
        for (int i = 0; i < n; ++i)
        {
            double xl = 0, xr = 0;
            for (auto& v : voices)
            {
                if (! v.on) continue;
                const size_t ip = (size_t) v.pos;
                if (v.done >= v.total || ip + 1 >= v.len) { v.on = false; continue; }
                const double fr = v.pos - (double) ip;
                double g = envOf (v);
                if (v.choke) { g *= v.chokeG * (double) v.chokeLeft / (double) v.chokeN; if (--v.chokeLeft <= 0) { v.on = false; continue; } }
                xl += (v.L[ip] + (v.L[ip + 1] - v.L[ip]) * fr) * g; xr += (v.R[ip] + (v.R[ip + 1] - v.R[ip]) * fr) * g;
                v.pos += v.inc; ++v.done;
            }
            const double ff = sm[FF].tick(), fq = sm[FQ].tick(), ds = sm[DS].tick(), rs = sm[RS].tick(), pn = sm[PN].tick(), gd = sm[GD].tick(), gb = sm[GB].tick(), gr = sm[GR].tick();
            if (useFl)
            {
                if ((flC++ & 15) == 0) setBiquad (ff, fq, hp);
                double* ch[2] = { &xl, &xr };
                for (int c = 0; c < 2; ++c)
                {
                    Bq& f = fl[c]; const double x = *ch[c], y = b0 * x + b1 * f.x1 + b2 * f.x2 - a1 * f.y1 - a2 * f.y2;
                    f.x2 = f.x1; f.x1 = x; f.y2 = f.y1; f.y1 = y; *ch[c] = y;
                }
            }
            double yl = xl, yr = xr;
            if (gb > 1e-4 || gr > 1e-4 || gd < .9999)
            {
                yl = xl * gd + (gb > 1e-4 ? bitq (xl, bitL) * gb : 0) + (gr > 1e-4 ? std::tanh (kk * xl) / tk * gr : 0);
                yr = xr * gd + (gb > 1e-4 ? bitq (xr, bitL) * gb : 0) + (gr > 1e-4 ? std::tanh (kk * xr) / tk * gr : 0);
            }
            double wl = 0, wr = 0;
            if (useDl)
            {
                dTime += (dTimeT - dTime) * .0004;
                const size_t sz = dly[0].size(); double rp = (double) dpos - dTime * sr; while (rp < 0) rp += (double) sz;
                const size_t r0 = (size_t) rp % sz, r1 = (r0 + 1) % sz; const double fr = rp - std::floor (rp);
                const double dl = dly[0][r0] + (dly[0][r1] - dly[0][r0]) * fr, dr = dly[1][r0] + (dly[1][r1] - dly[1][r0]) * fr;
                dly[0][dpos] = (float) (yl * ds + dl * dfb); dly[1][dpos] = (float) (yr * ds + dr * dfb); dpos = (dpos + 1) % sz;
                wl += dl; wr += dr;
            }
            if (useRv)
            {
                const double in = (yl + yr) * .5 * rs; double o[4];
                for (int k = 0; k < 4; ++k) o[k] = rv[k][(size_t) rvPos[k]];
                const double m[4] = { (o[0] + o[1] + o[2] + o[3]) * .5, (o[0] - o[1] + o[2] - o[3]) * .5, (o[0] + o[1] - o[2] - o[3]) * .5, (o[0] - o[1] - o[2] + o[3]) * .5 };
                for (int k = 0; k < 4; ++k)
                {
                    rvLp[k] += .55 * (m[k] * rvG[k] - rvLp[k]);
                    rv[k][(size_t) rvPos[k]] = (float) (rvLp[k] + in * (k & 1 ? -1 : 1)); if (++rvPos[k] >= rvLen[k]) rvPos[k] = 0;
                }
                wl += o[0] + o[2] * .7; wr += o[1] + o[3] * .7;
            }
            const double gl = std::min (1.0, std::sqrt (2.0) * std::cos ((pn + 1) * M_PI / 4)), grr = std::min (1.0, std::sqrt (2.0) * std::sin ((pn + 1) * M_PI / 4));
            double ol = (yl * gl + wl) * vol, orr = (yr * grr + wr) * vol;
            oL[i] += (float) soft (ol); oR[i] += (float) soft (orr);
        }
    }
    static double soft (double x) { const double a = std::fabs (x); if (a <= .9) return x; const double y = .9 + .1 * std::tanh ((a - .9) / .1); return x < 0 ? -y : y; }
};
} // namespace ch
