#include "PluginProcessor.h"
#include "PluginEditor.h"

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using PID = juce::ParameterID;

static const char* kDlyNames[] = { "1/4", "1/8.", "1/8", "1/16", "1/4T" };

Layout CrazyHeadProcessor::createLayout()
{
    Layout l;
    auto I = [&l] (const char* id, const char* nm, int lo, int hi, int df) { l.add (std::make_unique<juce::AudioParameterInt> (PID { id, 1 }, nm, lo, hi, df)); };
    auto B = [&l] (const char* id, const char* nm, bool df) { l.add (std::make_unique<juce::AudioParameterBool> (PID { id, 1 }, nm, df)); };
    auto C = [&l] (const char* id, const char* nm, juce::StringArray s, int df) { l.add (std::make_unique<juce::AudioParameterChoice> (PID { id, 1 }, nm, s, df)); };
    // generador
    I ("dens", "Densidad", 0, 100, 60);       I ("sync", "Síncopa", 0, 100, 40);     I ("fid", "Fidelidad", 0, 100, 60);
    I ("ratch", "Trinados", 0, 100, 15);      I ("dbl", "Doble golpe", 0, 100, 10);  I ("vmin", "Vel. mín.", 1, 127, 60);
    I ("vmax", "Vel. máx.", 1, 127, 115);     I ("glen", "Acortar", 0, 100, 0);      I ("rev", "Reverse", 0, 100, 8);
    I ("pitp", "Pitch prob.", 0, 100, 0);     I ("pitr", "Pitch rango", 1, 12, 5);   I ("hum", "Humanize ms", 0, 30, 0);
    I ("mut", "Mutación", 0, 100, 0);
    C ("grid", "Rejilla", { "1/4", "1/8", "1/16", "1/32" }, 2);
    I ("len", "Compases del patrón", 1, 4, 2); I ("bars", "Compases del loop", 1, 8, 2); I ("swing", "Swing", 50, 75, 50);
    I ("pinprob", "Prob. fijos", 0, 100, 100);
    B ("paton", "Patrón", true); B ("mono", "Mono", true); B ("follow", "Seguir tempo", true);
    I ("tune", "Tono", -12, 12, 0); I ("fine", "Fino", -100, 100, 0);
    I ("vol", "Volumen", 0, 100, 80); I ("pan", "Pan", -100, 100, 0);
    // efectos
    B ("flon", "Filtro", false); C ("fltype", "Tipo", { "Paso bajo", "Paso alto" }, 0); I ("flf", "Frecuencia", 0, 100, 70); I ("flq", "Resonancia", 0, 100, 20);
    B ("bton", "Bitcrush", false); I ("bit", "Bits", 4, 12, 8); B ("dron", "Drive", false); I ("dr", "Cantidad", 0, 100, 30);
    B ("dlon", "Delay", false); C ("dldiv", "División", juce::StringArray (kDlyNames, 5), 1); I ("dlfb", "Realim.", 0, 70, 40); I ("dlmix", "Mezcla", 0, 100, 35);
    I ("rvmix", "Reverb", 0, 100, 0); I ("rvsz", "Tamaño", 0, 100, 50);
    // efectos aleatorios
    B ("rfxon", "Aleatorios", false); C ("rtrig", "Dispara", { "Cada golpe", "Cada 1/16", "Cada 1/8", "Cada 1/4", "Cada compás" }, 0);
    I ("rprob", "Probabilidad", 0, 100, 60); I ("ramt", "Cantidad", 0, 100, 60); I ("rglide", "Glide", 0, 100, 40);
    B ("rback", "Vuelve", true); B ("rbyslice", "Por chop", true); B ("rvary", "Varía", false);
    B ("tgflt", "Filtro", true); B ("tgdly", "Delay", false); B ("tgrev", "Reverb", false); B ("tgbit", "Bit", false);
    B ("tgdrv", "Drive", false); B ("tgpan", "Pan", false); B ("tggate", "Stutter", false);
    l.add (std::make_unique<juce::AudioParameterInt> (PID { "tempo", 1 }, "Tempo (sin DAW)", 30, 300, 120));
    return l;
}

CrazyHeadProcessor::CrazyHeadProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CrazyHead", createLayout())
{
    for (auto& a : pinsUI) a.store (-1);
    for (auto& a : poolUI) a.store (true);
    juce::Random r; const uint32_t s = (uint32_t) (r.nextInt() & 0x3fffffff) | 1u;
    seedWanted.store (s); seedChanged.store (true);
}

bool CrazyHeadProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void CrazyHeadProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate; engine.prepare (sampleRate);
    seedChanged.store (true); if (seedWanted.load() == 0) seedWanted.store (seedShown.load());
    if (! keepAlive.empty()) { const juce::SpinLock::ScopedLockType sl (loopLock); pendingLoop = keepAlive.back(); hasPending.store (true); }
}

ch::Params CrazyHeadProcessor::snapshot() const
{
    ch::Params q;
    q.dens = (int) p ("dens"); q.sync = (int) p ("sync"); q.fid = (int) p ("fid"); q.ratch = (int) p ("ratch"); q.dbl = (int) p ("dbl");
    q.vmin = (int) p ("vmin"); q.vmax = std::max ((int) p ("vmax"), q.vmin); q.glen = (int) p ("glen"); q.rev = (int) p ("rev");
    q.pitP = (int) p ("pitp"); q.pitR = (int) p ("pitr"); q.hum = (int) p ("hum"); q.mut = (int) p ("mut");
    static const int grids[] = { 4, 8, 16, 32 }; q.grid = grids[(int) p ("grid") & 3];
    q.len = (int) p ("len"); q.loopBars = (int) p ("bars"); q.swing = (int) p ("swing"); q.pinProb = (int) p ("pinprob");
    q.patOn = p ("paton") > .5f; q.mono = p ("mono") > .5f; q.follow = p ("follow") > .5f;
    q.tune = (int) p ("tune"); q.fine = (int) p ("fine"); q.vol = p ("vol"); q.pan = p ("pan");
    q.flOn = p ("flon") > .5f; q.flType = (int) p ("fltype"); q.flF = (int) p ("flf"); q.flQ = (int) p ("flq");
    q.btOn = p ("bton") > .5f; q.bit = (int) p ("bit"); q.drOn = p ("dron") > .5f; q.dr = (int) p ("dr");
    q.dlOn = p ("dlon") > .5f; q.dlDiv = (int) p ("dldiv"); q.dlFb = (int) p ("dlfb"); q.dlMix = (int) p ("dlmix");
    q.rv = (int) p ("rvmix"); q.rvSz = (int) p ("rvsz");
    q.rOn = p ("rfxon") > .5f; q.rTrig = (int) p ("rtrig"); q.rProb = (int) p ("rprob"); q.rAmt = (int) p ("ramt"); q.rGlide = (int) p ("rglide");
    q.rBack = p ("rback") > .5f; q.rBySlice = p ("rbyslice") > .5f; q.rVary = p ("rvary") > .5f;
    q.tgFlt = p ("tgflt") > .5f; q.tgDly = p ("tgdly") > .5f; q.tgRev = p ("tgrev") > .5f; q.tgBit = p ("tgbit") > .5f;
    q.tgDrv = p ("tgdrv") > .5f; q.tgPan = p ("tgpan") > .5f; q.tgGate = p ("tggate") > .5f;
    return q;
}

void CrazyHeadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    buffer.clear();

    // transporte del anfitrión (o interno)
    bool playing = false; double ppq = 0, bpm = 0; bool host = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; host = true; }
            playing = pos->getIsPlaying();
        }
    hostSync.store (host);
    if (bpm < 20 || bpm > 400) bpm = p ("tempo");
    if (! host)
    {
        playing = internalPlay.load(); ppq = internalPpq;
        if (playing) internalPpq += (double) n / sr * bpm / 60.0; else internalPpq = 0;
    }
    playingNow.store (playing); bpmNow.store (bpm);

    // loop nuevo
    if (hasPending.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (loopLock);
        if (sl.isLocked()) { engine.setLoop (pendingLoop); hasPending.store (false); }
    }
    auto prm = snapshot(); prm.bpm = bpm;
    engine.setParams (prm);
    if (seedChanged.exchange (false)) { engine.regenerate (true, seedWanted.load()); seedShown.store (engine.seed()); }
    bool pinsChanged = false;
    for (int k = 0; k < 16; ++k) if (pinsUI[(size_t) k].load() != engine.pin (k)) { engine.setPin (k, pinsUI[(size_t) k].load()); pinsChanged = true; }
    for (int k = 0; k < ch::kMaxSeg; ++k) if (poolUI[(size_t) k].load() != engine.pool (k)) { engine.setPool (k, poolUI[(size_t) k].load()); pinsChanged = true; }
    (void) pinsChanged;
    if (const int c = cmdRegen.exchange (0)) { engine.regenerate (c == 2, cmdSeed.load()); seedShown.store (engine.seed()); }
    if (cmdDice.exchange (0)) engine.doDice (cmdSeed.load() * 2654435761u + 7u);
    if (const int a = audition.exchange (-1); a >= 0) engine.noteOn (0, 36 + a, .9f);

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) engine.noteOn (juce::jlimit (0, juce::jmax (0, n - 1), m.samplePosition), msg.getNoteNumber(), msg.getFloatVelocity());
    }

    juce::AudioBuffer<float> tmp;                       // el motor es estéreo; si el bus es mono se mezcla
    float* L = buffer.getWritePointer (0);
    if (nch >= 2) engine.process (L, buffer.getWritePointer (1), n, playing, ppq);
    else { std::vector<float> r ((size_t) n, 0.0f); engine.process (L, r.data(), n, playing, ppq); for (int i = 0; i < n; ++i) L[i] = (L[i] + r[(size_t) i]) * .5f; }
}

// ---------------- audio y troceado ----------------
bool CrazyHeadProcessor::loadBuffer (const juce::AudioBuffer<float>& b, double sampleRate, const juce::String& name)
{
    if (b.getNumSamples() < 64 || sampleRate < 8000) return false;
    const int n = std::min (b.getNumSamples(), (int) (kMaxSeconds * sampleRate));
    notice = n < b.getNumSamples() ? juce::String (juce::CharPointer_UTF8 ("Audio recortado a 60 s (m\xc3\xa1ximo del plugin).")) : juce::String();
    srcL.assign (b.getReadPointer (0), b.getReadPointer (0) + n);
    srcR.assign (b.getReadPointer (b.getNumChannels() > 1 ? 1 : 0), b.getReadPointer (b.getNumChannels() > 1 ? 1 : 0) + n);
    srcSr = sampleRate; sourceName = name; reg0 = 0; reg1 = (double) n / sampleRate;
    markers.clear();
    // compases del loop: el valor más cercano a un tempo entre 70 y 160 con el tempo del proyecto
    autoChopNow();
    return true;
}

bool CrazyHeadProcessor::loadFile (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (! rd || rd->lengthInSamples < 64) return false;
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (kMaxSeconds * rd->sampleRate));
    juce::AudioBuffer<float> b (std::max (1, (int) std::min<unsigned> (2u, rd->numChannels)), n);
    if (! rd->read (&b, 0, n, 0, true, true)) return false;
    if (rd->lengthInSamples > n) { loadBuffer (b, rd->sampleRate, f.getFileName()); notice = juce::String (juce::CharPointer_UTF8 ("Audio recortado a 60 s (m\xc3\xa1ximo del plugin).")); return true; }
    return loadBuffer (b, rd->sampleRate, f.getFileName());
}

void CrazyHeadProcessor::setRegion (double a, double b)
{
    const double d = sourceDuration(); if (d <= 0) return;
    reg0 = juce::jlimit (0.0, d - 0.05, a); reg1 = juce::jlimit (reg0 + 0.05, d, b);
    autoChopNow();
}

void CrazyHeadProcessor::buildLoop()
{
    if (! hasAudio()) return;
    const size_t a = (size_t) std::floor (reg0 * srcSr), b = std::min (srcL.size(), (size_t) std::floor (reg1 * srcSr));
    if (b <= a + 32) return;
    auto lp = ch::Loop::make (srcL.data() + a, srcR.data() + a, b - a, srcSr, markers);
    keepAlive.push_back (lp); if (keepAlive.size() > 4) keepAlive.erase (keepAlive.begin());
    { const juce::SpinLock::ScopedLockType sl (loopLock); pendingLoop = lp; hasPending.store (true); }
    loopVersion.fetch_add (1);
}

void CrazyHeadProcessor::autoChopNow()
{
    if (! hasAudio()) return;
    const size_t a = (size_t) std::floor (reg0 * srcSr), b = std::min (srcL.size(), (size_t) std::floor (reg1 * srcSr));
    if (b <= a + 32) return;
    markers = ch::autoChop (srcL.data() + a, srcR.data() + a, b - a, srcSr, sens, gapMs, std::max (2, std::min (maxCuts, ch::kMaxSeg)));
    buildLoop();
}

void CrazyHeadProcessor::gridChopNow (int nn)
{
    if (! hasAudio()) return;
    markers = ch::gridChop (reg1 - reg0, nn); buildLoop();
}

void CrazyHeadProcessor::addMarker (double t)
{
    if (! hasAudio() || (int) markers.size() >= ch::kMaxSeg || t <= .005 || t >= reg1 - reg0 - .005) return;
    for (double m : markers) if (std::fabs (m - t) < .01) return;
    markers.push_back (t); std::sort (markers.begin(), markers.end()); buildLoop();
}

void CrazyHeadProcessor::moveMarker (int idx, double t)
{
    if (idx <= 0 || idx >= (int) markers.size()) return;
    const double lo = markers[(size_t) idx - 1] + .005, hi = idx + 1 < (int) markers.size() ? markers[(size_t) idx + 1] - .005 : reg1 - reg0 - .005;
    markers[(size_t) idx] = juce::jlimit (lo, hi, t); buildLoop();
}

void CrazyHeadProcessor::removeMarker (int idx)
{
    if (idx <= 0 || idx >= (int) markers.size()) return;
    markers.erase (markers.begin() + idx); buildLoop();
}

void CrazyHeadProcessor::requestRegen (bool newSeed)
{
    if (newSeed) cmdSeed.store ((uint32_t) (juce::Random::getSystemRandom().nextInt() & 0x3fffffff) | 1u);
    cmdRegen.store (newSeed ? 2 : 1);
}
void CrazyHeadProcessor::requestDice()
{
    cmdSeed.store ((uint32_t) (juce::Random::getSystemRandom().nextInt() & 0x3fffffff) | 1u); cmdDice.store (1);
}

// ---------------- estado ----------------
// Formato: «CHVST1» + tamaño del XML (int32) + XML + audio (frames, canales, tasa, PCM 16 bits entrelazado)
void CrazyHeadProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml();
    xml->setAttribute ("version", kVersion);
    xml->setAttribute ("seed", (int) seedShown.load());
    xml->setAttribute ("reg0", reg0); xml->setAttribute ("reg1", reg1);
    xml->setAttribute ("sens", sens); xml->setAttribute ("gap", gapMs); xml->setAttribute ("maxcuts", maxCuts);
    xml->setAttribute ("name", sourceName);
    juce::StringArray mk; for (double m : markers) mk.add (juce::String (m, 6)); xml->setAttribute ("markers", mk.joinIntoString (","));
    juce::String pins, pool; for (int k = 0; k < 16; ++k) { pins << (k ? "," : "") << pinsUI[(size_t) k].load(); pool << (poolUI[(size_t) k].load() ? '1' : '0'); }
    xml->setAttribute ("pins", pins); xml->setAttribute ("pool", pool);
    const auto xs = xml->toString (juce::XmlElement::TextFormat().singleLine());
    juce::MemoryOutputStream o (dest, false);
    o.write ("CHVST1", 6);
    const auto u = xs.toUTF8(); const int xl = (int) u.sizeInBytes() - 1;
    o.writeInt (xl); o.write (u.getAddress(), (size_t) xl);
    const int frames = (int) srcL.size();
    o.writeInt (frames); o.writeInt (frames ? 2 : 0); o.writeInt ((int) srcSr);
    for (int i = 0; i < frames; ++i)
    {
        o.writeShort ((short) juce::jlimit (-32768, 32767, (int) std::lround (srcL[(size_t) i] * 32767.0f)));
        o.writeShort ((short) juce::jlimit (-32768, 32767, (int) std::lround (srcR[(size_t) i] * 32767.0f)));
    }
}

void CrazyHeadProcessor::setStateInformation (const void* data, int size)
{
    if (size < 14 || std::memcmp (data, "CHVST1", 6) != 0) return;
    juce::MemoryInputStream in (data, (size_t) size, false); in.setPosition (6);
    const int xl = in.readInt(); if (xl <= 0 || xl > size) return;
    juce::MemoryBlock xb; in.readIntoMemoryBlock (xb, xl);
    auto xml = juce::XmlDocument::parse (xb.toString());
    const int frames = in.readInt(), nch = in.readInt(); const int rate = in.readInt();
    if (! xml) return;
    if (frames > 0 && nch == 2 && rate >= 8000 && (juce::int64) frames * 4 <= (juce::int64) size)
    {
        srcL.resize ((size_t) frames); srcR.resize ((size_t) frames); srcSr = rate;
        for (int i = 0; i < frames; ++i) { srcL[(size_t) i] = (float) in.readShort() / 32767.0f; srcR[(size_t) i] = (float) in.readShort() / 32767.0f; }
    }
    else { srcL.clear(); srcR.clear(); }
    if (auto tree = juce::ValueTree::fromXml (*xml); tree.isValid()) apvts.replaceState (tree);
    sourceName = xml->getStringAttribute ("name");
    sens = xml->getIntAttribute ("sens", 55); gapMs = xml->getIntAttribute ("gap", 70); maxCuts = xml->getIntAttribute ("maxcuts", 16);
    reg0 = xml->getDoubleAttribute ("reg0", 0); reg1 = xml->getDoubleAttribute ("reg1", sourceDuration());
    markers.clear();
    for (auto& s : juce::StringArray::fromTokens (xml->getStringAttribute ("markers"), ",", "")) if (s.isNotEmpty()) markers.push_back (s.getDoubleValue());
    if (markers.empty()) markers.push_back (0.0);
    auto pins = juce::StringArray::fromTokens (xml->getStringAttribute ("pins"), ",", "");
    for (int k = 0; k < 16; ++k) pinsUI[(size_t) k].store (k < pins.size() ? pins[k].getIntValue() : -1);
    const auto pool = xml->getStringAttribute ("pool");
    for (int k = 0; k < 16; ++k) poolUI[(size_t) k].store (k >= pool.length() || pool[k] == '1');
    seedWanted.store ((uint32_t) std::max (1, xml->getIntAttribute ("seed", 1))); seedChanged.store (true);
    if (hasAudio()) { reg0 = juce::jlimit (0.0, sourceDuration(), reg0); reg1 = juce::jlimit (reg0 + .05, sourceDuration(), reg1); buildLoop(); }
}

juce::AudioProcessorEditor* CrazyHeadProcessor::createEditor() { return new CrazyHeadEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CrazyHeadProcessor(); }
