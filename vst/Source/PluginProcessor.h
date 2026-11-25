#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "core/CrazyEngine.h"

class CrazyHeadProcessor : public juce::AudioProcessor
{
public:
    static constexpr const char* kVersion = "1.0.0";   // igual que VERSION en CMakeLists.txt
    static constexpr double kMaxSeconds = 60.0;        // máximo de audio que se guarda en el proyecto del DAW

    CrazyHeadProcessor();
    ~CrazyHeadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CrazyHead"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- audio y troceado (hilo de la interfaz) ----
    bool loadFile (const juce::File& f);                 // false si no se puede leer
    bool loadBuffer (const juce::AudioBuffer<float>& b, double sampleRate, const juce::String& name);
    bool hasAudio() const { return srcL.size() > 1; }
    double sourceDuration() const { return srcSr > 0 ? (double) srcL.size() / srcSr : 0.0; }
    double sourceRate() const { return srcSr; }
    const std::vector<float>& sourceLeft() const { return srcL; }
    const std::vector<float>& sourceRight() const { return srcR; }
    juce::String sourceName;
    juce::String notice;                                  // aviso para la ventana (p. ej. «recortado a 60 s»)

    double regionStart() const { return reg0; }
    double regionEnd() const { return reg1; }
    void setRegion (double a, double b);                  // segundos del audio original
    const std::vector<double>& chops() const { return markers; }   // segundos relativos a la región; el primero es 0
    void autoChopNow();
    void gridChopNow (int n);
    void addMarker (double t);
    void moveMarker (int idx, double t);
    void removeMarker (int idx);
    int sens = 55, gapMs = 70;                            // detección de golpes
    int maxCuts = 16;
    std::atomic<int> loopVersion { 0 };                   // sube con cada cambio de loop/chops (la ventana se redibuja)

    // ---- patrón (se aplican en el hilo de audio) ----
    void requestRegen (bool newSeed);
    void requestDice();
    void setPin (int step, int chop) { pinsUI[(size_t) (step & 15)].store (chop); }
    int getPin (int step) const { return pinsUI[(size_t) (step & 15)].load(); }
    void setPool (int chop, bool on) { poolUI[(size_t) chop].store (on); }
    bool getPool (int chop) const { return poolUI[(size_t) chop].load(); }
    int patternStep (int i) const { return engine.displayStep (i); }
    int currentStep() const { return playingNow.load() ? engine.currentStep() : -1; }
    uint32_t currentSeed() const { return seedShown.load(); }
    float outputPeak() const { return engine.peak(); }
    void auditionChop (int c) { audition.store (c); }

    // transporte interno (solo cuando el anfitrión no da posición, p. ej. la app independiente)
    std::atomic<bool> internalPlay { false };
    std::atomic<bool> hostSync { false };                 // ¿el anfitrión da posición?
    std::atomic<bool> playingNow { false };
    std::atomic<double> bpmNow { 120.0 };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

private:
    void buildLoop();                                     // región + marcadores → engine
    ch::Params snapshot() const;

    ch::Engine engine;
    std::vector<float> srcL, srcR; double srcSr = 44100;
    double reg0 = 0, reg1 = 0;
    std::vector<double> markers;
    std::vector<std::shared_ptr<ch::Loop>> keepAlive;    // evita liberar un loop en el hilo de audio
    juce::SpinLock loopLock; std::shared_ptr<const ch::Loop> pendingLoop; std::atomic<bool> hasPending { false };
    std::array<std::atomic<int>, 16> pinsUI; std::array<std::atomic<bool>, 16> poolUI;
    std::atomic<int> cmdRegen { 0 }; std::atomic<uint32_t> cmdSeed { 1 }; std::atomic<int> cmdDice { 0 }; std::atomic<int> audition { -1 };
    std::atomic<uint32_t> seedShown { 1 }, seedWanted { 0 }; std::atomic<bool> seedChanged { false };
    double sr = 44100, internalPpq = 0;
    std::atomic<int> nextSeedSalt { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrazyHeadProcessor)
};
