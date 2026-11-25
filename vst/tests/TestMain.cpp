// Pruebas del plugin (sin DAW): carga y troceado, reproducción sincronizada, notas MIDI, estado, ventana (captura PNG).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

static constexpr double SR = 48000.0;
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::cout << "  FALLO: " << msg << "\n"; ++failures; } else std::cout << "  ok: " << msg << "\n"; } while (0)

struct FakeHead : public juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setBpm (bpm); i.setPpqPosition (ppq); i.setIsPlaying (playing); return i;
    }
};

static juce::AudioBuffer<float> loop (double seconds)
{
    const int n = (int) (SR * seconds); juce::AudioBuffer<float> b (2, n); b.clear();
    for (int k = 0; k < 8; ++k)
        for (int i = 0; i < (int) (SR * .2); ++i)
        {
            const int t = (int) (k * seconds / 8 * SR) + i; if (t >= n) break;
            const float v = (float) (.8 * std::sin (2 * juce::MathConstants<double>::pi * (110.0 * (k + 1)) * i / SR) * std::exp (-i / SR * 10));
            b.addSample (0, t, v); b.addSample (1, t, v);
        }
    return b;
}

static double run (CrazyHeadProcessor& p, FakeHead& h, int blocks, int bs, juce::MidiBuffer* firstMidi = nullptr)
{
    juce::AudioBuffer<float> buf (2, bs); double e = 0;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer m; if (b == 0 && firstMidi) m = *firstMidi;
        buf.clear(); p.processBlock (buf, m);
        for (int i = 0; i < bs; ++i) e += (double) buf.getSample (0, i) * buf.getSample (0, i);
        h.ppq += bs / SR * h.bpm / 60.0;
    }
    return std::sqrt (e / (blocks * bs));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::cout << "== Carga y troceado ==\n";
    CrazyHeadProcessor a; FakeHead head; a.setPlayHead (&head); a.prepareToPlay (SR, 256);
    CHECK (a.loadBuffer (loop (4.0), SR, "prueba.wav"), "carga un audio de 4 s");
    CHECK (a.chops().size() >= 4 && a.chops().size() <= 16, "lo trocea (entre 4 y 16 trozos)");
    a.gridChopNow (8); CHECK (a.chops().size() == 8, "troceado por rejilla en 8");
    a.addMarker (1.3); CHECK (a.chops().size() == 9, "añadir una marca");
    a.removeMarker (3); CHECK (a.chops().size() == 8, "quitar una marca");
    a.setRegion (0.5, 3.5); CHECK (std::fabs ((a.regionEnd() - a.regionStart()) - 3.0) < 1e-9, "elegir una región");
    a.setRegion (0, 4);

    std::cout << "== Reproducción ==\n";
    a.setPin (0, 0); a.setPin (8, 3);
    auto* dens = a.apvts.getParameter ("dens"); dens->setValueNotifyingHost (0.0f);
    run (a, head, 4, 256);      // deja que llegue el loop nuevo al hilo de audio
    head.ppq = 0; const double r = run (a, head, 400, 256);
    CHECK (r > .01, "suena con el transporte en marcha");
    head.playing = false; head.ppq = 0; const double r2 = run (a, head, 200, 256);
    CHECK (r2 < .05, "se calla con el transporte parado (queda solo la cola)");
    head.playing = false; juce::MidiBuffer mb; mb.addEvent (juce::MidiMessage::noteOn (1, 38, 1.0f), 0);
    const double r3 = run (a, head, 40, 256, &mb);
    CHECK (r3 > .02, "una nota MIDI toca su trozo");

    std::cout << "== Estado ==\n";
    a.apvts.getParameter ("dens")->setValueNotifyingHost (a.apvts.getParameter ("dens")->convertTo0to1 (77));
    a.apvts.getParameter ("flon")->setValueNotifyingHost (1.0f);
    juce::MemoryBlock mbk; a.getStateInformation (mbk);
    CrazyHeadProcessor b; b.prepareToPlay (SR, 256); b.setStateInformation (mbk.getData(), (int) mbk.getSize());
    CHECK ((int) b.p ("dens") == 77 && b.p ("flon") > .5f, "los mandos se restauran");
    CHECK (b.hasAudio() && std::fabs (b.sourceDuration() - 4.0) < 1e-3, "el audio se guarda dentro del proyecto");
    CHECK (b.chops().size() == a.chops().size(), "los trozos se restauran");
    CHECK (b.getPin (0) == 0 && b.getPin (8) == 3 && b.getPin (1) == -1, "los golpes fijos se restauran");
    CHECK (b.sourceName == "prueba.wav", "el nombre del audio se restaura");
    b.setStateInformation ("basura", 6);
    CHECK (b.hasAudio(), "un estado inválido no rompe nada");
    FakeHead h2; b.setPlayHead (&h2); run (b, h2, 8, 256); head.ppq = 0;
    CHECK (run (b, h2, 400, 256) > .01, "el proyecto restaurado suena");

    std::cout << "== Ventana ==\n";
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        ed->setSize (980, 850);
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        CHECK (img.getWidth() == 980 && img.getHeight() == 850, "se dibuja la ventana");
        auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("crazyhead-ventana.png");
        juce::FileOutputStream fo (out); if (fo.openedOk()) { fo.setPosition (0); fo.truncate(); juce::PNGImageFormat().writeImageToStream (img, fo); }
        CHECK (out.existsAsFile() && out.getSize() > 5000, "captura crazyhead-ventana.png");
    }
    std::cout << (failures ? "\nFALLOS\n" : "\nTodo correcto\n");
    return failures ? 1 : 0;
}
