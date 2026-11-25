// Pruebas del motor (sin JUCE): sincronía, bloques variables, MIDI, golpes fijos, efectos aleatorios, estabilidad y CPU.
#include "../vst/Source/core/CrazyEngine.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace ch;
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf ("  FALLO: %s\n", msg); ++failures; } else std::printf ("  ok: %s\n", msg); } while (0)
static const double SR = 48000;

// Loop de 2 compases a 120 BPM (4 s): cada corchea suena un tono corto de frecuencia distinta
static std::shared_ptr<Loop> makeLoop (int nChops = 8)
{
    const int N = (int) (SR * 4); std::vector<float> x ((size_t) N, 0.0f);
    for (int b = 0; b < 8; ++b)
    {
        const size_t t0 = (size_t) (b * SR / 2);
        for (int i = 0; i < (int) (SR * .25) && t0 + (size_t) i < (size_t) N; ++i) x[t0 + (size_t) i] += (float) (.8 * std::sin (2 * M_PI * (110.0 * (b + 1)) * i / SR) * std::exp (-i / SR * 8));
    }
    return Loop::make (x.data(), nullptr, (size_t) N, SR, gridChop (4.0, nChops));
}
static std::vector<float> render (Engine& e, int total, int block, bool playing, double bpm, std::vector<float>* R = nullptr, double ppq0 = 0)
{
    std::vector<float> L ((size_t) total, 0.0f), rr ((size_t) total, 0.0f); double ppq = ppq0;
    for (int pos = 0; pos < total; pos += block) { const int n = std::min (block, total - pos); e.process (&L[(size_t) pos], &rr[(size_t) pos], n, playing, ppq); ppq += n / SR * bpm / 60; }
    if (R) *R = rr; return L;
}
static int firstAbove (const std::vector<float>& x, float th, size_t from = 0) { for (size_t i = from; i < x.size(); ++i) if (std::fabs (x[i]) > th) return (int) i; return -1; }
static double rmsOf (const std::vector<float>& x, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b && i < x.size(); ++i) e += (double) x[i] * x[i]; return std::sqrt (e / std::max<size_t> (1, b - a)); }

int main()
{
    std::printf ("== Bloques variables ==\n");
    {
        Params p; p.dens = 80; p.ratch = 40; p.dbl = 30; p.rev = 20; p.pitP = 50; p.hum = 0; p.mut = 30; p.rOn = true; p.rProb = 100; p.tgFlt = true; p.tgDly = true; p.dlOn = true; p.rv = 30; p.rBack = true;
        auto lp = makeLoop(); auto go = [&] (int blk) { Engine e; e.prepare (SR); e.setParams (p); e.setLoop (lp); e.setSeed (777); e.regenerate (false); return render (e, (int) (SR * 9), blk, true, 120); };
        auto a = go (64), b = go (480), c = go (1000); double dm = 0; for (size_t i = 0; i < a.size(); ++i) dm = std::max ({ dm, (double) std::fabs (a[i] - b[i]), (double) std::fabs (a[i] - c[i]) });
        CHECK (dm < 1e-4, "misma salida con bloques de 64, 480 y 1000 muestras");
        CHECK (rmsOf (a, 0, a.size()) > .01, "suena");
    }
    std::printf ("== Sincronía con el DAW ==\n");
    {
        Params p; p.dens = 0; p.rev = 0; p.ratch = 0; p.dbl = 0; p.vmin = p.vmax = 100; Engine e; e.prepare (SR); e.setParams (p); e.setLoop (makeLoop());
        e.setPin (0, 0); e.setPin (8, 3);
        auto y = render (e, (int) (SR * 4), 256, true, 120);
        const int i0 = firstAbove (y, .02f);
        CHECK (i0 >= 0 && i0 < 90, "el golpe fijo del paso 1 suena en la muestra 0 (±1,8 ms) del compás");
        const int stepSamples = (int) (SR * 60 / 120 / 4), i8 = firstAbove (y, .02f, (size_t) (8 * stepSamples - 200));
        CHECK (i8 >= 8 * stepSamples - 5 && i8 < 8 * stepSamples + 90, "el golpe fijo del paso 9 cae en su pulso");
        Engine e2; e2.prepare (SR); e2.setParams (p); e2.setLoop (makeLoop()); e2.setPin (0, 0);
        auto z = render (e2, (int) (SR * 2), 512, false, 120);
        CHECK (rmsOf (z, 0, z.size()) == 0, "con el transporte parado no suena el patrón");
        Engine e3; e3.prepare (SR); e3.setParams (p); e3.setLoop (makeLoop()); e3.setPin (0, 0);
        auto w = render (e3, (int) (SR * 1), 512, true, 120, nullptr, 3.5);   // empieza en el pulso 3.5 → el paso 1 del compás 1 está en 0.5 beat
        CHECK (firstAbove (w, .02f) > (int) (SR * 60 / 120 * .5) - 100, "empezar a mitad de compás espera al siguiente paso");
    }
    std::printf ("== Notas MIDI ==\n");
    {
        Params p; p.patOn = false; Engine e; e.prepare (SR); e.setParams (p); e.setLoop (makeLoop());
        std::vector<float> L (4800, 0.0f), R (4800, 0.0f); e.noteOn (1000, 36 + 2, 1.0f); e.process (L.data(), R.data(), 4800, false, 0);
        const int i0 = firstAbove (L, .01f);
        CHECK (i0 >= 1000 && i0 < 1000 + 60, "la nota 38 empieza en su muestra");
        double e1 = rmsOf (L, 1000, 3000); CHECK (e1 > .02, "toca el trozo 3");
        Engine e2; e2.prepare (SR); e2.setParams (p); e2.setLoop (makeLoop(4)); std::vector<float> A (2000), B (2000); e2.noteOn (0, 36 + 9, 1.0f); e2.process (A.data(), B.data(), 2000, false, 0);
        CHECK (rmsOf (A, 0, 2000) == 0, "una nota sin trozo no suena");
    }
    std::printf ("== Golpes fijos y regeneración ==\n");
    {
        Params p; p.dens = 100; Engine e; e.prepare (SR); e.setParams (p); e.setLoop (makeLoop()); e.setPin (4, 5); e.setPin (20, 5);
        CHECK (e.displayStep (4) == 5 && e.displayStep (20) == 5, "el golpe fijo aparece en el patrón (compases 1 y 2)");
        e.regenerate (true, 4242); CHECK (e.displayStep (4) == 5, "sigue ahí tras regenerar con otra semilla");
        const uint32_t sd = e.seed(); e.regenerate (false); CHECK (e.seed() == sd, "regenerar sin semilla nueva conserva la semilla");
    }
    std::printf ("== Efectos aleatorios ==\n");
    {
        Params p; p.dens = 100; p.rev = 0; p.rOn = false; p.vmin = p.vmax = 100;
        auto run = [&] (Params q) { Engine e; e.prepare (SR); e.setParams (q); e.setLoop (makeLoop()); e.setSeed (99); e.regenerate (false); return render (e, (int) (SR * 8), 512, true, 120); };
        auto a = run (p); Params q = p; q.rOn = true; q.rProb = 100; q.rAmt = 100; q.tgFlt = true; auto b = run (q);
        double d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::fabs (a[i] - b[i]);
        CHECK (d / (double) a.size() > 1e-3, "el filtro aleatorio cambia el sonido");
        Params q2 = p; q2.rOn = true; q2.rProb = 100; q2.rAmt = 100; q2.tgFlt = false; q2.tgGate = true; auto c = run (q2);
        d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::fabs (a[i] - c[i]);
        CHECK (d / (double) a.size() > 1e-3, "el stutter aleatorio cambia el sonido");
        Params q3 = p; q3.rOn = true; q3.rProb = 0; q3.tgFlt = true; auto z = run (q3);
        Params ref = p; ref.flOn = true; ref.flF = 100; ref.flQ = 10; auto a2 = run (ref);   // filtro abierto en reposo, como fuerza la app
        double d3 = 0; for (size_t i = 0; i < a2.size(); ++i) d3 += std::fabs (a2[i] - z[i]);
        CHECK (d3 / (double) a2.size() < 1e-6, "con probabilidad 0 el sonido es el del filtro abierto en reposo (sin cambios)");
        // determinista: misma semilla → mismo resultado
        auto b2 = run (q); double dd = 0; for (size_t i = 0; i < b.size(); ++i) dd = std::max (dd, (double) std::fabs (b[i] - b2[i])); CHECK (dd == 0, "mismo resultado con la misma semilla");
    }
    std::printf ("== Estabilidad y CPU ==\n");
    {
        uint32_t s = 5; auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (double) s / 4294967296.0; };
        bool ok = true; double peak = 0;
        for (int t = 0; t < 12; ++t)
        {
            Params p; p.dens = (int) (rnd() * 100); p.ratch = (int) (rnd() * 100); p.dbl = (int) (rnd() * 100); p.rev = (int) (rnd() * 100); p.pitP = 100; p.pitR = 12; p.hum = (int) (rnd() * 30); p.mut = (int) (rnd() * 100);
            p.grid = (int[]) { 4, 8, 16, 32 }[t % 4]; p.tune = (int) (rnd() * 24) - 12; p.fine = (int) (rnd() * 200) - 100; p.follow = t % 2; p.mono = t % 3 != 0; p.vol = 100; p.pan = (rnd() * 2 - 1) * 100;
            p.flOn = true; p.flType = t % 2; p.flF = (int) (rnd() * 100); p.flQ = 100; p.btOn = t % 2; p.bit = 4 + (int) (rnd() * 8); p.drOn = true; p.dr = 100; p.dlOn = true; p.dlDiv = t % 5; p.dlFb = 70; p.dlMix = 100; p.rv = 100; p.rvSz = (int) (rnd() * 100);
            p.rOn = true; p.rTrig = t % 5; p.rProb = 100; p.rAmt = 100; p.tgFlt = p.tgDly = p.tgRev = p.tgBit = p.tgDrv = p.tgPan = p.tgGate = true; p.swing = 50 + (int) (rnd() * 25);
            Engine e; e.prepare (SR); e.setParams (p); e.setLoop (makeLoop (1 + (int) (rnd() * 15))); e.setSeed ((uint32_t) (rnd() * 1e9)); e.regenerate (false);
            std::vector<float> R; auto L = render (e, (int) (SR * 6), 333 + t * 17, true, 60 + rnd() * 180, &R);
            for (size_t i = 0; i < L.size(); ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ok = false; peak = std::max ({ peak, (double) std::fabs (L[i]), (double) std::fabs (R[i]) }); }
        }
        CHECK (ok, "12 ajustes extremos: sin NaN ni infinitos");
        CHECK (peak <= 1.0001, "la salida nunca pasa de 0 dBFS (limitador suave)");
        Params p; p.dens = 100; p.ratch = 60; p.flOn = p.btOn = p.drOn = p.dlOn = true; p.rv = 50; p.rOn = true; p.rProb = 100; p.tgFlt = p.tgDly = p.tgRev = p.tgBit = p.tgDrv = true;
        Engine e; e.prepare (SR); e.setParams (p); e.setLoop (makeLoop()); std::vector<float> L (512), R (512); double ppq = 0;
        auto t0 = std::chrono::steady_clock::now(); const int blocks = (int) (SR * 20 / 512);
        for (int b = 0; b < blocks; ++b) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); e.process (L.data(), R.data(), 512, true, ppq); ppq += 512 / SR * 2; }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  CPU: %.2f %% de un núcleo con todo activado (20 s de audio en %.3f s)\n", sec / 20.0 * 100, sec);
        CHECK (sec / 20.0 < .15, "CPU por debajo del 15 % de un núcleo");
    }
    std::printf (failures ? "\n%d FALLOS\n" : "\nTodo correcto\n", failures);
    return failures ? 1 : 0;
}
