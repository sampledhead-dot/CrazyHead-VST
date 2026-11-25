// Paridad del generador: lee casos (un caso por línea) y escribe el patrón tras regen, 3 mutaciones y un dado.
#include "../vst/Source/core/CrazyGen.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
using namespace ch;
static void dump (const Pattern& p, std::string& out)
{
    char b[200];
    for (int i = 0; i < p.steps; ++i)
    {
        out += std::to_string (i) + ":";
        for (int k = 0; k < p.cell[i].n; ++k)
        {
            const Hit& h = p.cell[i].h[k];
            snprintf (b, sizeof b, "[s%d o%.9g v%.9g d%.9g r%d p%d t%d n%d l%d]", h.s, h.o, h.v, h.d, h.r, h.p, h.t, h.n, h.l);
            out += b;
        }
        out += "\n";
    }
}
int main (int argc, char** argv)
{
    std::ifstream f (argv[1]); std::string line;
    while (std::getline (f, line))
    {
        std::istringstream is (line); GenParams D; double dur; int nseg; unsigned pb, seed;
        is >> D.dens >> D.sync >> D.fid >> D.ratch >> D.dbl >> D.vmin >> D.vmax >> D.glen >> D.rev >> D.pitP >> D.pitR >> D.hum >> D.mut >> D.grid >> D.loopBars >> D.len >> seed >> D.pinProb >> pb >> dur >> nseg;
        D.seed = seed; D.pinBars = pb;
        Seg sg[kMaxSeg]; std::vector<double> m (nseg); for (auto& x : m) is >> x;
        for (int k = 0; k < nseg; ++k) { sg[k].a = m[k]; sg[k].b = k + 1 < nseg ? m[k + 1] : dur; }
        std::string pool; is >> pool; for (int k = 0; k < kMaxSeg; ++k) D.pool[k] = pool[k] == '1';
        for (int k = 0; k < 16; ++k) is >> D.pins[k];
        Pattern pat; std::string out;
        regen (D, sg, nseg, dur, pat); dump (pat, out); out += "--mut\n";
        for (int k = 0; k < 3; ++k) cycleMut (D, sg, nseg, dur, pat);
        dump (pat, out); out += "--dice\n";
        dice (D, sg, nseg, dur, pat, 123456u); dump (pat, out);
        std::printf ("%s=====\n", out.c_str());
    }
}
