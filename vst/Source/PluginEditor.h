#pragma once
#include "PluginProcessor.h"
#include "UpdateBanner.h"

namespace col
{
    // Estética de CrazyHead: violeta profundo, texto crema y amarillo (el de la pista A de la app)
    const juce::Colour bg (0xff170a38), panel (0xff24104f), panel2 (0xff331a6b), line (0xff4a2896), text (0xfffff4e0),
                       muted (0xffb9a3e8), accent (0xffffd028), pink (0xffff3d7a), glow (0xff4a1fb0), dark (0xff0e0524);
    inline juce::Colour chop (int k)
    {
        static const juce::uint32 p[] = { 0xffffd028, 0xffff8a00, 0xffff3d7a, 0xffb57bff, 0xffff5a2a, 0xffffb83d, 0xffff7ad9, 0xff8a5cff };
        return juce::Colour (p[k & 7]);
    }
}

class CrazyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrazyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, bool bipolar = false);
    void resized() override;
    juce::Slider slider; juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

struct Tog : public juce::ToggleButton
{
    Tog (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label);
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

struct Combo : public juce::ComboBox
{
    Combo (juce::AudioProcessorValueTreeState& s, const juce::String& id);
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Vista general: todo el audio cargado y la región (arrastra los bordes o el cuerpo)
class OverviewView : public juce::Component
{
public:
    explicit OverviewView (CrazyHeadProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    CrazyHeadProcessor& proc; std::vector<float> peaks; int mode = 0; double a = 0, b = 0, grab = 0;
    double xToT (float x) const { return juce::jlimit (0.0, proc.sourceDuration(), (double) x / getWidth() * proc.sourceDuration()); }
};

// Región ampliada con los trozos: arrastra una línea para moverla, doble clic para añadir, clic derecho para quitar
class ChopView : public juce::Component
{
public:
    explicit ChopView (CrazyHeadProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void resized() override { refresh(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh();
    std::function<void (int)> onSelect;
    int selected = 0, lit = -1;
private:
    CrazyHeadProcessor& proc; std::vector<std::pair<float, float>> pk; int drag = -1; double dragT = 0;
    double xToT (float x) const { return juce::jlimit (0.0, proc.regionEnd() - proc.regionStart(), (double) x / getWidth() * (proc.regionEnd() - proc.regionStart())); }
};

class PatternView : public juce::Component
{
public:
    explicit PatternView (CrazyHeadProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<int()> selectedChop;
private:
    CrazyHeadProcessor& proc;
};

class CrazyHeadEditor : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit CrazyHeadEditor (CrazyHeadProcessor&);
    ~CrazyHeadEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    shupdate::UpdateBanner& banner() { return updateBanner; }

private:
    void timerCallback() override;
    void chooseFile();
    void sync();
    void section (juce::Graphics&, juce::Rectangle<int>, const juce::String&);

    CrazyHeadProcessor& proc; CrazyLookAndFeel laf; juce::AudioProcessorValueTreeState& st;
    OverviewView overview; ChopView chopView; PatternView pattern;
    juce::TextButton loadBtn { "Cargar audio" }, autoBtn { "Trocear" }, g4 { "4" }, g8 { "8" }, g16 { "16" }, regenBtn { "Regenerar" }, diceBtn { "Dado" }, clearPins { "Quitar fijos" }, playBtn { "Play" };
    juce::Slider sensSl; juce::Label fileLbl, infoLbl, seedLbl, syncLbl, sensLbl;
    juce::OwnedArray<juce::Component> comps; std::vector<juce::Rectangle<int>> secRects; std::vector<juce::String> secNames;
    std::vector<Knob*> kn; std::vector<Tog*> tg; std::vector<Combo*> cb;
    juce::OwnedArray<juce::Label> labels;
    std::unique_ptr<juce::FileChooser> chooser;
    shupdate::UpdateBanner updateBanner;
    int lastVer = -1, lastStep = -2, lastChopCount = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrazyHeadEditor)
};
