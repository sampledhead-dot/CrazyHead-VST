#include "PluginEditor.h"

static juce::String U (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

//==============================================================================
CrazyLookAndFeel::CrazyLookAndFeel()
{
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::dark);
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::dark);
    setColour (juce::Slider::thumbColourId, col::accent);
}

void CrazyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float r = (float) juce::jmin (w, h) * 0.5f - 3.0f, cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    juce::Path bgArc; bgArc.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
    g.setColour (col::line); g.strokePath (bgArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path arc; arc.addCentredArc (cx, cy, r, r, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (col::accent); g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float kr = r * 0.66f;
    g.setColour (col::dark); g.fillEllipse (cx - kr, cy - kr + 2.0f, kr * 2, kr * 2);
    g.setColour (col::panel2); g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (col::line); g.drawEllipse (cx - kr, cy - kr, kr * 2, kr * 2, 1.2f);
    juce::Path ptr; ptr.addRoundedRectangle (-1.5f, -kr + 2.0f, 3.0f, kr * 0.85f, 1.5f);
    g.setColour (col::accent); g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void CrazyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::accent : (over ? col::line : col::panel2));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? col::accent : col::line); g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? col::dark : col::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (2, 0), juce::Justification::centred, true);
}

void CrazyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    auto c = on ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (over && ! on) c = c.brighter (0.15f);
    if (down) c = c.darker (0.15f);
    g.setColour (c); g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? col::accent : col::line); g.drawRoundedRectangle (r, 7.0f, 1.2f);
}

Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, bool bipolar)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 13);
    slider.setColour (juce::Slider::textBoxTextColourId, col::muted);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("bipolar", bipolar);
    name.setText (label.toUpperCase(), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setColour (juce::Label::textColourId, col::text);
    name.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider); addAndMakeVisible (name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}
void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (12));
    slider.setBounds (r);
}

Tog::Tog (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label)
{
    setButtonText (label); att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, *this);
}
Combo::Combo (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) addItemList (p->choices, 1);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, *this);
}

//==============================================================================
void OverviewView::refresh()
{
    peaks.clear();
    const auto& L = proc.sourceLeft(); const auto& R = proc.sourceRight();
    if (L.size() > 1 && getWidth() > 4)
    {
        const int nb = getWidth(); peaks.assign ((size_t) nb, 0.0f);
        const size_t per = std::max<size_t> (1, L.size() / (size_t) nb);
        for (int i = 0; i < nb; ++i)
        {
            float m = 0; const size_t s0 = (size_t) i * per, s1 = std::min (L.size(), s0 + per), st = std::max<size_t> (1, per / 64);
            for (size_t k = s0; k < s1; k += st) m = std::max (m, std::max (std::fabs (L[k]), std::fabs (R[k])));
            peaks[(size_t) i] = m;
        }
    }
    repaint();
}
void OverviewView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::dark); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (.5f), 6.0f, 1.0f);
    const double d = proc.sourceDuration();
    if (d <= 0) { g.setColour (col::muted); g.setFont (juce::FontOptions (13.0f)); g.drawText (U ("Suelta aqu\xc3\xad un audio o pulsa «Cargar audio»"), getLocalBounds(), juce::Justification::centred); return; }
    const float cy = r.getCentreY(), H = r.getHeight() * .46f;
    g.setColour (col::muted.withAlpha (.55f));
    for (size_t i = 0; i < peaks.size(); ++i) g.fillRect ((float) i, cy - peaks[i] * H, 1.0f, std::max (1.0f, peaks[i] * H * 2));
    const double ra = mode ? a : proc.regionStart(), rb = mode ? b : proc.regionEnd();
    const float x0 = (float) (ra / d * getWidth()), x1 = (float) (rb / d * getWidth());
    g.setColour (col::accent.withAlpha (.16f)); g.fillRect (x0, 1.0f, x1 - x0, r.getHeight() - 2);
    g.setColour (col::accent); g.fillRect (x0 - 1.5f, 1.0f, 3.0f, r.getHeight() - 2); g.fillRect (x1 - 1.5f, 1.0f, 3.0f, r.getHeight() - 2);
}
void OverviewView::mouseDown (const juce::MouseEvent& e)
{
    const double d = proc.sourceDuration(); if (d <= 0) return;
    a = proc.regionStart(); b = proc.regionEnd(); const double t = xToT (e.position.x), tol = 8.0 / getWidth() * d;
    if (std::fabs (t - a) < tol) mode = 1; else if (std::fabs (t - b) < tol) mode = 2;
    else if (t > a && t < b) { mode = 3; grab = t - a; }
    else { mode = 2; a = t; b = std::min (d, t + .05); }
    repaint();
}
void OverviewView::mouseDrag (const juce::MouseEvent& e)
{
    const double d = proc.sourceDuration(); if (! mode || d <= 0) return;
    const double t = xToT (e.position.x);
    if (mode == 1) a = juce::jlimit (0.0, b - .05, t);
    else if (mode == 2) { if (t < a) { b = a; a = t; mode = 1; } else b = juce::jmax (a + .05, t); }
    else { const double l = b - a; a = juce::jlimit (0.0, d - l, t - grab); b = a + l; }
    repaint();
}
void OverviewView::mouseUp (const juce::MouseEvent&)
{
    if (mode) { proc.setRegion (a, b); mode = 0; repaint(); }
}

//==============================================================================
void ChopView::refresh()
{
    pk.clear();
    const auto& L = proc.sourceLeft(); const auto& R = proc.sourceRight(); const double sr = proc.sourceRate();
    if (L.size() > 1 && getWidth() > 4)
    {
        const size_t a = (size_t) (proc.regionStart() * sr), b = std::min (L.size(), (size_t) (proc.regionEnd() * sr));
        const int nb = getWidth(); pk.assign ((size_t) nb, { 0.0f, 0.0f });
        if (b > a + 1)
            for (int i = 0; i < nb; ++i)
            {
                const size_t s0 = a + (size_t) ((double) i / nb * (double) (b - a)), s1 = std::max (s0 + 1, a + (size_t) ((double) (i + 1) / nb * (double) (b - a)));
                const size_t st = std::max<size_t> (1, (s1 - s0) / 48); float mn = 0, mx = 0;
                for (size_t k = s0; k < std::min (s1, L.size()); k += st) { const float v = (L[k] + R[k]) * .5f; mn = std::min (mn, v); mx = std::max (mx, v); }
                pk[(size_t) i] = { mn, mx };
            }
    }
    if (selected >= (int) proc.chops().size()) selected = 0;
    repaint();
}
void ChopView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::dark); g.fillRoundedRectangle (r, 6.0f);
    const auto& m = proc.chops(); const double len = proc.regionEnd() - proc.regionStart();
    if (m.empty() || len <= 0) { g.setColour (col::line); g.drawRoundedRectangle (r.reduced (.5f), 6.0f, 1.0f); return; }
    const int n = (int) m.size(); const float W = (float) getWidth(), top = 22.0f;
    auto X = [&] (double t) { return (float) (t / len * W); };
    for (int k = 0; k < n; ++k)
    {
        double t0 = m[(size_t) k]; if (drag == k) t0 = dragT;
        double t1 = k + 1 < n ? m[(size_t) k + 1] : len; if (drag == k + 1) t1 = dragT;
        const float x0 = X (t0), x1 = X (t1); const bool on = proc.getPool (k), isLit = k == lit;
        auto c = col::chop (k);
        g.setColour (c.withAlpha (isLit ? .34f : (on ? .13f : .04f))); g.fillRect (x0, top, x1 - x0, r.getHeight() - top);
        g.setColour (c.withAlpha (on ? .95f : .35f)); g.fillRect (x0, 0.0f, std::max (2.0f, x1 - x0 - 2.0f), top - 3);
        g.setColour (col::dark); g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String (k + 1), (int) x0, 0, (int) std::max (8.0f, x1 - x0), (int) top - 3, juce::Justification::centred, false);
        if (k == selected) { g.setColour (juce::Colours::white); g.drawRect (x0, 0.0f, std::max (2.0f, x1 - x0 - 2.0f), top - 3, 2.0f); }
    }
    const float cy = top + (r.getHeight() - top) * .5f, H = (r.getHeight() - top) * .46f;
    g.setColour (col::text.withAlpha (.9f));
    for (size_t i = 0; i < pk.size(); ++i) g.fillRect ((float) i, cy - pk[i].second * H, 1.0f, std::max (1.0f, (pk[i].second - pk[i].first) * H));
    for (int k = 1; k < n; ++k)
    {
        const float x = X (drag == k ? dragT : m[(size_t) k]);
        g.setColour (col::chop (k)); g.fillRect (x - 1.0f, top, 2.0f, r.getHeight() - top);
    }
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (.5f), 6.0f, 1.0f);
}
void ChopView::mouseDown (const juce::MouseEvent& e)
{
    const auto& m = proc.chops(); const double len = proc.regionEnd() - proc.regionStart(); if (m.empty() || len <= 0) return;
    const int n = (int) m.size(); const double t = xToT (e.position.x);
    int k = 0; for (int i = 0; i < n; ++i) if (m[(size_t) i] <= t) k = i;
    if (e.position.y < 22)
    {
        if (e.mods.isShiftDown() || e.mods.isPopupMenu()) { proc.setPool (k, ! proc.getPool (k)); repaint(); return; }
        selected = k; proc.auditionChop (k); if (onSelect) onSelect (k); repaint(); return;
    }
    const double tol = 7.0 / getWidth() * len; int best = -1; double bd = tol;
    for (int i = 1; i < n; ++i) { const double dd = std::fabs (m[(size_t) i] - t); if (dd < bd) { bd = dd; best = i; } }
    if (best > 0 && e.mods.isPopupMenu()) { proc.removeMarker (best); refresh(); return; }
    if (best > 0) { drag = best; dragT = m[(size_t) best]; repaint(); }
}
void ChopView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag > 0) { dragT = xToT (e.position.x); repaint(); }
}
void ChopView::mouseUp (const juce::MouseEvent&)
{
    if (drag > 0) { const int k = drag; drag = -1; proc.moveMarker (k, dragT); refresh(); }
}
void ChopView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.position.y >= 22) { proc.addMarker (xToT (e.position.x)); refresh(); }
}

//==============================================================================
void PatternView::paint (juce::Graphics& g)
{
    const int rows = std::max (1, (int) proc.p ("len")); const float W = (float) getWidth(), cw = W / 16.0f, rh = 22.0f;
    const int cur = proc.currentStep(), n = (int) proc.chops().size();
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < 16; ++c)
        {
            const int i = r * 16 + c, s = proc.patternStep (i);
            juce::Rectangle<float> cell (c * cw + 1.5f, r * rh + 1.5f, cw - 3.0f, rh - 3.0f);
            g.setColour (s >= 0 ? col::chop (s) : (c % 4 == 0 ? col::panel2 : col::panel)); g.fillRoundedRectangle (cell, 4.0f);
            if (s >= 0) { g.setColour (col::dark); g.drawText (juce::String (s + 1), cell, juce::Justification::centred, false); }
            if (i == cur) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (cell.expanded (1.0f), 4.0f, 2.0f); }
        }
    const float py = rows * rh + 6;
    g.setColour (col::muted); g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (U ("GOLPES FIJOS · elige un trozo arriba y toca el paso donde debe sonar siempre"), 0, (int) py, getWidth(), 12, juce::Justification::centredLeft, false);
    for (int c = 0; c < 16; ++c)
    {
        const int pn = proc.getPin (c); juce::Rectangle<float> cell (c * cw + 1.5f, py + 14, cw - 3.0f, 24.0f);
        const bool ok = pn >= 0 && pn < n;
        g.setColour (ok ? col::chop (pn) : (c % 4 == 0 ? col::panel2 : col::panel)); g.fillRoundedRectangle (cell, 5.0f);
        g.setColour (ok ? col::dark : col::muted); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (ok ? juce::String (pn + 1) : (c % 4 == 0 ? juce::String (c / 4 + 1) : juce::String (".")), cell, juce::Justification::centred, false);
    }
}
void PatternView::mouseDown (const juce::MouseEvent& e)
{
    const int rows = std::max (1, (int) proc.p ("len")); const float py = rows * 22.0f + 6 + 14;
    if (e.position.y < py || e.position.y > py + 24) return;
    const int c = juce::jlimit (0, 15, (int) (e.position.x / ((float) getWidth() / 16.0f)));
    const int sel = selectedChop ? selectedChop() : 0;
    if (e.mods.isPopupMenu()) proc.setPin (c, -1);
    else proc.setPin (c, proc.getPin (c) == sel ? -1 : sel);
    repaint();
}

//==============================================================================
CrazyHeadEditor::CrazyHeadEditor (CrazyHeadProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), st (p.apvts), overview (p), chopView (p), pattern (p),
      updateBanner ("CrazyHead-VST", "CrazyHead", CrazyHeadProcessor::kVersion, col::panel, col::text, col::accent)
{
    setLookAndFeel (&laf);
    setSize (980, 850);
    auto addC = [this] (juce::Component* c) { addAndMakeVisible (c); };
    for (auto* c : { (juce::Component*) &overview, (juce::Component*) &chopView, (juce::Component*) &pattern }) addC (c);
    for (auto* b : { &loadBtn, &autoBtn, &g4, &g8, &g16, &regenBtn, &diceBtn, &clearPins, &playBtn }) addC (b);
    pattern.selectedChop = [this] { return chopView.selected; };
    loadBtn.onClick = [this] { chooseFile(); };
    autoBtn.onClick = [this] { proc.autoChopNow(); };
    g4.onClick = [this] { proc.gridChopNow (4); }; g8.onClick = [this] { proc.gridChopNow (8); }; g16.onClick = [this] { proc.gridChopNow (16); };
    regenBtn.onClick = [this] { proc.requestRegen (true); };
    diceBtn.onClick = [this] { proc.requestDice(); };
    clearPins.onClick = [this] { for (int k = 0; k < 16; ++k) proc.setPin (k, -1); pattern.repaint(); };
    playBtn.setClickingTogglesState (true); playBtn.onClick = [this] { proc.internalPlay.store (playBtn.getToggleState()); };
    regenBtn.setTooltip (U ("Nuevo patr\xc3\xb3n al azar (nueva semilla)")); diceBtn.setTooltip (U ("Cambia solo algunos pasos"));
    sensSl.setSliderStyle (juce::Slider::LinearHorizontal); sensSl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); sensSl.setRange (0, 100, 1); sensSl.setValue (proc.sens, juce::dontSendNotification);
    sensSl.onDragEnd = [this] { proc.sens = (int) sensSl.getValue(); proc.autoChopNow(); };
    sensSl.setColour (juce::Slider::trackColourId, col::accent); sensSl.setColour (juce::Slider::backgroundColourId, col::line);
    addC (&sensSl);
    for (auto* l : { &fileLbl, &infoLbl, &seedLbl, &syncLbl, &sensLbl }) { addC (l); l->setColour (juce::Label::textColourId, col::muted); l->setFont (juce::FontOptions (12.0f)); }
    fileLbl.setColour (juce::Label::textColourId, col::text); sensLbl.setText ("Sensibilidad", juce::dontSendNotification);
    chopView.onSelect = [this] (int) { pattern.repaint(); };

    // mandos y botones (se colocan en resized() por su id)
    auto K = [this] (const juce::String& id, const juce::String& label, bool bip = false) { auto* k = new Knob (st, id, label, bip); k->setComponentID (id); comps.add (k); addAndMakeVisible (k); };
    auto T = [this] (const juce::String& id, const juce::String& label) { auto* t = new Tog (st, id, label); t->setComponentID (id); comps.add (t); addAndMakeVisible (t); };
    auto C = [this] (const juce::String& id) { auto* c = new Combo (st, id); c->setComponentID (id); comps.add (c); addAndMakeVisible (c); };
    for (const auto& a : std::initializer_list<std::pair<const char*, const char*>> { { "dens", "Densidad" }, { "sync", "S\xc3\xadncopa" }, { "fid", "Fidelidad" }, { "ratch", "Trinados" }, { "dbl", "Doble" },
        { "glen", "Acortar" }, { "rev", "Reverse" }, { "pitp", "Pitch prob" }, { "pitr", "Pitch rango" }, { "hum", "Humanize" }, { "mut", "Mutaci\xc3\xb3n" }, { "vmin", "Vel min" }, { "vmax", "Vel max" },
        { "len", "Patr. comp." }, { "bars", "Loop comp." }, { "swing", "Swing" }, { "pinprob", "Prob fijos" }, { "flf", "Frec" }, { "flq", "Reson" }, { "bit", "Bits" }, { "dr", "Drive" },
        { "dlfb", "Realim" }, { "dlmix", "Mezcla" }, { "rvmix", "Reverb" }, { "rvsz", "Tama\xc3\xb1o" }, { "rprob", "Prob" }, { "ramt", "Cantidad" }, { "rglide", "Glide" } })
        K (a.first, juce::String (juce::CharPointer_UTF8 (a.second)));
    K ("tune", "Tono", true); K ("fine", "Fino", true); K ("vol", "Volumen"); K ("pan", "Pan", true);
    for (const auto& a : std::initializer_list<std::pair<const char*, const char*>> { { "paton", "Patr\xc3\xb3n" }, { "mono", "Mono" }, { "follow", "Seguir tempo" }, { "flon", "Filtro" }, { "bton", "Bitcrush" }, { "dron", "Drive" },
        { "dlon", "Delay" }, { "rfxon", "ALEATORIOS" }, { "rback", "Vuelve" }, { "rbyslice", "Por trozo" }, { "rvary", "Var\xc3\xad" "a" }, { "tgflt", "Filtro" }, { "tgdly", "Delay" }, { "tgrev", "Reverb" },
        { "tgbit", "Bit" }, { "tgdrv", "Drive" }, { "tgpan", "Pan" }, { "tggate", "Stutter" } })
        T (a.first, juce::String (juce::CharPointer_UTF8 (a.second)));
    C ("grid"); C ("fltype"); C ("dldiv"); C ("rtrig");
    addAndMakeVisible (updateBanner);
    startTimerHz (30);
    resized(); sync();
}
CrazyHeadEditor::~CrazyHeadEditor() { setLookAndFeel (nullptr); }

void CrazyHeadEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> (U ("Elige un audio (m\xc3\xa1x. 60 s)"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult(); if (f == juce::File()) return;
        filesDropped ({ f.getFullPathName() }, 0, 0);
    });
}
void CrazyHeadEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    if (! proc.loadFile (juce::File (files[0]))) { fileLbl.setText (U ("No se pudo leer ese archivo"), juce::dontSendNotification); return; }
    // compases del loop: el n\xc3\xbamero de compases que deja el tempo entre 70 y 170
    const double d = proc.regionEnd() - proc.regionStart(); int best = 2; double bd = 1e9;
    for (int b : { 1, 2, 4, 8 }) { const double bpm = b * 4 * 60.0 / d, e = std::fabs (std::log (bpm / 110.0)); if (e < bd) { bd = e; best = b; } }
    if (auto* prm = st.getParameter ("bars")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) best));
    chopView.selected = 0; lastVer = -1; sync();
}

void CrazyHeadEditor::sync()
{
    const int v = proc.loopVersion.load();
    if (v != lastVer) { lastVer = v; overview.refresh(); chopView.refresh(); pattern.repaint(); }
    fileLbl.setText (proc.hasAudio() ? proc.sourceName : juce::String(), juce::dontSendNotification);
    if (proc.hasAudio())
    {
        const double d = proc.regionEnd() - proc.regionStart();
        infoLbl.setText (juce::String ((int) proc.chops().size()) + U (" trozos \xc2\xb7 regi\xc3\xb3n ") + juce::String (d, 2) + " s" + (proc.notice.isNotEmpty() ? " \xc2\xb7 " + proc.notice : juce::String()), juce::dontSendNotification);
    }
    else infoLbl.setText ({}, juce::dontSendNotification);
    seedLbl.setText (U ("semilla ") + juce::String ((juce::int64) proc.currentSeed()), juce::dontSendNotification);
    const bool host = proc.hostSync.load();
    playBtn.setVisible (! host); playBtn.setButtonText (playBtn.getToggleState() ? "Stop" : "Play");
    syncLbl.setText (host ? U ("Sincronizado con el DAW \xc2\xb7 ") + juce::String (proc.bpmNow.load(), 1) + " BPM"
                          : U ("Sin DAW: tempo propio \xc2\xb7 ") + juce::String (proc.bpmNow.load(), 0) + " BPM", juce::dontSendNotification);
}

void CrazyHeadEditor::timerCallback()
{
    sync();
    const int cur = proc.currentStep();
    if (cur != lastStep)
    {
        lastStep = cur; pattern.repaint();
        chopView.lit = cur >= 0 ? proc.patternStep (cur) : -1; chopView.repaint();
    }
}

void CrazyHeadEditor::section (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t)
{
    g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r.toFloat().reduced (.5f), 10.0f, 1.2f);
    g.setColour (col::accent); g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (t, r.getX() + 12, r.getY() + 5, r.getWidth() - 24, 16, juce::Justification::centredLeft, false);
}

void CrazyHeadEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (col::glow, getWidth() * .5f, -40.0f, col::bg, getWidth() * .5f, getHeight() * .62f, true));
    g.fillAll();
    // título tipo grafiti: sombra oscura, contorno rosa y relleno amarillo
    const auto f = juce::Font (juce::FontOptions (38.0f, juce::Font::bold | juce::Font::italic));
    g.setFont (f);
    g.setColour (col::dark); g.drawText ("CRAZYHEAD", 19, 9, 360, 44, juce::Justification::centredLeft, false);
    g.setColour (col::pink); g.drawText ("CRAZYHEAD", 17, 7, 360, 44, juce::Justification::centredLeft, false);
    g.setColour (col::accent); g.drawText ("CRAZYHEAD", 16, 6, 360, 44, juce::Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (juce::FontOptions (12.0f));
    g.drawText (U ("sampler \xc2\xb7 generador al azar \xc2\xb7 By Sampled Head \xc2\xb7 v") + CrazyHeadProcessor::kVersion, 300, 24, 420, 18, juce::Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("REJILLA", 672, 60, 60, 14, juce::Justification::centredLeft, false);
    for (size_t i = 0; i < secRects.size(); ++i) section (g, secRects[i], secNames[i]);
}

void CrazyHeadEditor::resized()
{
    auto put = [this] (const juce::String& id, int x, int y, int w, int h)
    {
        for (auto* c : comps) if (c->getComponentID() == id) { c->setBounds (x, y, w, h); return; }
    };
    secRects.clear(); secNames.clear();
    auto sec = [this] (int x, int y, int w, int h, const juce::String& t) { secRects.push_back ({ x, y, w, h }); secNames.push_back (t); };
    const int W = getWidth();
    syncLbl.setBounds (W - 316, 12, 300, 18); syncLbl.setJustificationType (juce::Justification::centredRight);
    playBtn.setBounds (W - 96, 32, 80, 26);
    // fila de audio
    loadBtn.setBounds (16, 62, 120, 30); fileLbl.setBounds (142, 62, 200, 30);
    autoBtn.setBounds (346, 62, 80, 30); sensLbl.setBounds (432, 62, 84, 30); sensSl.setBounds (516, 62, 110, 30);
    g4.setBounds (730, 62, 40, 30); g8.setBounds (774, 62, 40, 30); g16.setBounds (818, 62, 40, 30);
    infoLbl.setBounds (866, 62, 100, 30); infoLbl.setJustificationType (juce::Justification::centredRight);
    overview.setBounds (16, 100, W - 32, 40);
    chopView.setBounds (16, 146, W - 32, 150);
    pattern.setBounds (16, 306, 640, 124);
    regenBtn.setBounds (680, 306, 140, 30); diceBtn.setBounds (826, 306, 138, 30);
    clearPins.setBounds (680, 342, 140, 28); seedLbl.setBounds (826, 342, 138, 28);
    put ("grid", 680, 380, 140, 28);
    // generador
    const int cw = 62, ch_ = 74;
    sec (16, 438, 450, 176, "GENERADOR AL AZAR");
    const char* g1[] = { "dens", "sync", "fid", "ratch", "dbl", "glen", "rev" }; const char* g2[] = { "pitp", "pitr", "hum", "mut", "vmin", "vmax" };
    for (int i = 0; i < 7; ++i) put (g1[i], 26 + i * cw, 462, cw - 2, ch_);
    for (int i = 0; i < 6; ++i) put (g2[i], 26 + i * cw, 538, cw - 2, ch_);
    // tempo y salida
    sec (474, 438, 490, 176, "LOOP Y SALIDA");
    const char* t1[] = { "len", "bars", "swing", "pinprob", "tune", "fine", "vol", "pan" };
    for (int i = 0; i < 8; ++i) put (t1[i], 484 + i * 60, 462, 58, ch_);
    put ("paton", 484, 546, 90, 26); put ("mono", 580, 546, 90, 26); put ("follow", 676, 546, 120, 26);
    // efectos
    sec (16, 622, 590, 214, "EFECTOS");
    put ("flon", 26, 648, 70, 26); put ("fltype", 100, 648, 100, 26); put ("bton", 210, 648, 82, 26); put ("dron", 296, 648, 70, 26); put ("dlon", 372, 648, 66, 26); put ("dldiv", 442, 648, 74, 26);
    const char* f1[] = { "flf", "flq", "bit", "dr", "dlfb", "dlmix", "rvmix", "rvsz" };
    for (int i = 0; i < 8; ++i) put (f1[i], 26 + i * 70, 686, 66, 80);
    // aleatorios
    sec (614, 622, 350, 214, U ("EFECTOS ALEATORIOS (cambian solos con el patr\xc3\xb3n)"));
    put ("rfxon", 624, 648, 110, 26); put ("rtrig", 740, 648, 214, 26);
    put ("rprob", 624, 682, 66, 80); put ("ramt", 694, 682, 66, 80); put ("rglide", 764, 682, 66, 80);
    put ("rback", 840, 686, 114, 22); put ("rbyslice", 840, 712, 114, 22); put ("rvary", 840, 738, 114, 22);
    const char* tg[] = { "tgflt", "tgdly", "tgrev", "tgbit", "tgdrv", "tgpan", "tggate" };
    for (int i = 0; i < 7; ++i) put (tg[i], 624 + (i % 4) * 86, 772 + (i / 4) * 30, 82, 26);
    updateBanner.setBounds (16, getHeight() - 36, W - 32, 28);
    if (! updateBanner.isVisible()) updateBanner.setBounds (0, 0, 0, 0);
    overview.refresh(); chopView.refresh();
}
