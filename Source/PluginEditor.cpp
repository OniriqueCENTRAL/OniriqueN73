#include "PluginEditor.h"
#include "OniriqueWindowSize.h"

namespace
{
const juce::String pluginName = "ONIRIQUE N73";

// centres des boutons (coordonnées de base de l'éditeur) : module en (37, 103), bouton à x = 131 dans le module
constexpr float moduleX = 37.0f, moduleY = 103.0f, panelX = 299.0f, panelW = 264.0f;
constexpr float knobX = moduleX + 131.0f;
constexpr float gainY = moduleY + 133.0f, highY = moduleY + 293.0f, midY = moduleY + 453.0f, lowY = moduleY + 615.0f, hpfY = moduleY + 775.0f;
constexpr float faderTop = moduleY + 120.0f, bottomTop = moduleY + 784.0f;
constexpr float panelCX = panelX + panelW * 0.5f;

juce::Path icon (int which)
{
    // symboles gravés à droite des boutons (26 × 14)
    juce::Path p;
    switch (which)
    {
        case 0: p.startNewSubPath (1, 7); p.lineTo (11, 7); p.cubicTo (15, 7, 16, 2, 25, 2);
                p.startNewSubPath (11, 7); p.cubicTo (15, 7, 16, 12, 25, 12); break;                         // HIGH
        case 1: p.startNewSubPath (1, 7); p.lineTo (8.5f, 7); p.startNewSubPath (17.5f, 7); p.lineTo (25, 7);
                p.addEllipse (8.5f, 2.5f, 9.0f, 9.0f); break;                                                  // MID
        case 2: p.startNewSubPath (25, 7); p.lineTo (15, 7); p.cubicTo (11, 7, 10, 2, 1, 2);
                p.startNewSubPath (15, 7); p.cubicTo (11, 7, 10, 12, 1, 12); break;                           // LOW
        default: p.startNewSubPath (1, 13); p.cubicTo (7, 13, 8, 2, 13, 2); p.lineTo (25, 2); break;          // HIGH PASS
    }
    return p;
}

void screw (juce::Graphics& g, juce::Point<float> c, float d, float rot)
{
    juce::ColourGradient grad (juce::Colour (0xff3a3438), c.x - d * 0.15f, c.y - d * 0.2f, juce::Colour (0xff0d0b0d), c.x + d * 0.5f, c.y + d * 0.5f, true);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
    g.setColour (juce::Colours::black);
    const auto a = ui::polar (c, d * 0.36f, rot + 90.0f), b = ui::polar (c, d * 0.36f, rot - 90.0f);
    g.drawLine ({ a, b }, d * 0.16f);
}

} // namespace

//==============================================================================
N73Editor::N73Editor (N73Processor& proc)
    : AudioProcessorEditor (proc), processor (proc),
      gain (ui::Knob::Kind::Gain, assets, bubble, p (params::gain), nullptr, "Gain"),
      high (ui::Knob::Kind::High, assets, bubble, p (params::high), nullptr, "High 12 kHz"),
      mid (ui::Knob::Kind::MidLow, assets, bubble, p (params::mid), &p (params::midFreq), "Mid"),
      low (ui::Knob::Kind::MidLow, assets, bubble, p (params::low), &p (params::lowFreq), "Low"),
      hpf (ui::Knob::Kind::Hpf, assets, bubble, p (params::hpf), nullptr, "High pass"),
      output (ui::Knob::Kind::Output, assets, bubble, p (params::output), nullptr, "Output"),
      micZ (p (params::micZ), "LO", "HI", "MIC Z"),
      pad (p (params::pad), "0", juce::CharPointer_UTF8 ("\xe2\x88\x92" "20"), "PAD"),
      power (p (params::power), "OFF", "ON", "POWER"),
      eql (p (params::eql), "EQL", true),
      phase (p (params::phase), "PHASE", false),
      fader (p (params::fader), bubble),
      lamp ([this] { return processor.getDrive(); })
{
    mid.setMidLow (true);
    low.setMidLow (false);
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource ("logo_png", size))
            logo = juce::ImageFileFormat::loadFrom (data, (size_t) size);
    }

    for (juce::Component* c : std::initializer_list<juce::Component*> { &gain, &high, &mid, &low, &hpf, &output, &micZ, &pad, &power, &eql, &phase,
                                &fader, &lamp, &prevPreset, &presetName, &nextPreset, &slotA, &slotB, &settings })
        addAndMakeVisible (c);
    addChildComponent (bubble);

    auto pill = [] (juce::Graphics& g, juce::Rectangle<float> r, bool hover)
    {
        g.setColour (juce::Colour (0xf0121012));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (hover ? ui::ember.withAlpha (0.5f) : ui::rule);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    };
    prevPreset.draw = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool hover)
    {
        g.setColour (hover ? ui::bone : ui::haze);
        juce::Path t; t.addTriangle (r.getCentreX() + 3 * scale(), r.getCentreY() - 4 * scale(), r.getCentreX() + 3 * scale(), r.getCentreY() + 4 * scale(),
                                     r.getCentreX() - 3 * scale(), r.getCentreY());
        g.fillPath (t);
    };
    nextPreset.draw = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool hover)
    {
        g.setColour (hover ? ui::bone : ui::haze);
        juce::Path t; t.addTriangle (r.getCentreX() - 3 * scale(), r.getCentreY() - 4 * scale(), r.getCentreX() - 3 * scale(), r.getCentreY() + 4 * scale(),
                                     r.getCentreX() + 3 * scale(), r.getCentreY());
        g.fillPath (t);
    };
    presetName.draw = [this] (juce::Graphics& g, juce::Rectangle<float> r, bool hover)
    {
        g.setColour (hover ? juce::Colours::white : ui::bone);
        g.setFont (ui::font (ui::Face::mono, 11.0f * scale()));
        g.drawFittedText (processor.presets.currentName(), r.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    };
    prevPreset.onClick = [this] { processor.presets.step (-1); refreshHeader(); };
    nextPreset.onClick = [this] { processor.presets.step (1); refreshHeader(); };
    presetName.onClick = [this] { showPresetMenu(); };

    auto slotDraw = [this] (int slot)
    {
        return [this, slot] (juce::Graphics& g, juce::Rectangle<float> r, bool hover)
        {
            const bool on = processor.getActiveSlot() == slot;
            if (on)
            {
                g.setGradientFill (juce::ColourGradient (ui::ember, r.getX(), r.getY(), juce::Colour (0xff8f1f2c), r.getRight(), r.getBottom(), false));
                g.fillRoundedRectangle (r.reduced (1.0f), r.getHeight() * 0.5f);
            }
            g.setColour (on ? juce::Colour (0xfffff4f5) : hover ? ui::bone : ui::haze);
            g.setFont (ui::font (ui::Face::display, 11.0f * scale()));
            g.drawText (slot == 0 ? "A" : "B", r, juce::Justification::centred, false);
        };
    };
    slotA.draw = slotDraw (0);
    slotB.draw = slotDraw (1);
    slotA.onClick = [this] { processor.selectSlot (0); refreshHeader(); slotA.repaint(); slotB.repaint(); };
    slotB.onClick = [this] { processor.selectSlot (1); refreshHeader(); slotA.repaint(); slotB.repaint(); };
    slotA.setTitle ("A"); slotB.setTitle ("B");
    settings.draw = [this, pill] (juce::Graphics& g, juce::Rectangle<float> r, bool hover)
    {
        pill (g, r, hover);
        const auto c = r.getCentre();
        const float s = scale();
        g.setColour (hover ? ui::bone : ui::haze);
        g.drawEllipse (juce::Rectangle<float> (4.8f * s, 4.8f * s).withCentre (c), 1.4f * s);
        for (int i = 0; i < 8; ++i)
            g.drawLine ({ ui::polar (c, 4.6f * s, 45.0f * (float) i), ui::polar (c, 6.6f * s, 45.0f * (float) i) }, 1.4f * s);
    };
    settings.onClick = [this] { showSettingsMenu(); };
    settings.setTitle ("Réglages");
    prevPreset.setTitle ("Preset précédent"); nextPreset.setTitle ("Preset suivant"); presetName.setTitle ("Presets");

#if N73_LICENCE
    licenseOverlay = std::make_unique<onirique::license::Overlay> (pluginName, processor.license);
    addAndMakeVisible (*licenseOverlay);
#endif

    setResizable (true, true);
    setResizeLimits (360, 672, 900, 1680);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    const auto size = onirique::window::openingSize (processor.apvts.state, pluginName, { 480, 896 }, { 360, 672 }, { 900, 1680 });
    setSize (size.x, juce::roundToInt ((float) size.x * (float) baseHeight / (float) baseWidth));
    refresher.startTimerHz (4);

    // essais locaux : ONIRIQUE_UI_PNG=fichier → l'interface s'écrit elle-même en PNG (rendu logiciel)
    if (auto* png = std::getenv ("ONIRIQUE_UI_PNG"))
    {
        const juce::String path (png);
        juce::Component::SafePointer<N73Editor> safe (this);
        juce::Timer::callAfterDelay (900, [safe, path]
        {
            if (safe == nullptr) return;
            juce::Image img (juce::Image::ARGB, safe->getWidth(), safe->getHeight(), true, juce::SoftwareImageType());
            { juce::Graphics g (img); safe->paintEntireComponent (g, true); }
            juce::File f (path); f.deleteFile();
            if (auto os = f.createOutputStream()) juce::PNGImageFormat().writeImageToStream (img, *os);
        });
    }
}

N73Editor::~N73Editor()
{
    onirique::window::rememberOnThisComputer (pluginName, getWidth(), getHeight());
}

//==============================================================================
void N73Editor::resized()
{
    const float s = scale();
    auto place = [s] (juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds (juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt());
    };
    auto placeKnob = [&] (ui::Knob& k, float cx, float cy, float half)
    {
        const auto r = juce::Rectangle<float> ((cx - half) * s, (cy - half) * s, 2.0f * half * s, 2.0f * half * s).toNearestInt();
        k.setBounds (r);
        k.setLayout (juce::Point<float> ((cx * s - (float) r.getX()) / s, (cy * s - (float) r.getY()) / s), s);
    };
    placeKnob (gain, knobX, gainY, 110.0f);
    placeKnob (high, knobX, highY, 100.0f);
    placeKnob (mid, knobX, midY, 100.0f);
    placeKnob (low, knobX, lowY, 100.0f);
    placeKnob (hpf, knobX, hpfY, 100.0f);
    placeKnob (output, panelCX, bottomTop + 58.0f, 70.0f);

    place (micZ, panelCX - 90.0f, moduleY + 11.0f, 180.0f, 44.0f);
    place (pad, panelCX - 90.0f, moduleY + 65.0f, 180.0f, 44.0f);
    place (power, panelCX - 90.0f, bottomTop + 142.0f, 180.0f, 44.0f);
    for (auto* t : { &micZ, &pad, &power }) t->setScale (s);
    place (eql, moduleX + 61.0f - 4.0f, moduleY + 877.0f - 4.0f, 78.0f, 42.0f);
    place (phase, moduleX + 131.0f - 4.0f, moduleY + 877.0f - 4.0f, 78.0f, 42.0f);
    eql.setScale (s); phase.setScale (s);
    place (fader, panelCX - 50.0f, faderTop, 100.0f, 662.0f);
    fader.setScale (s);
    place (lamp, moduleX + 131.0f - 25.0f, moduleY + 956.0f - 25.0f, 50.0f, 50.0f);

    place (prevPreset, 282.0f, 29.0f, 28.0f, 34.0f);
    place (presetName, 310.0f, 29.0f, 124.0f, 34.0f);
    place (nextPreset, 434.0f, 29.0f, 28.0f, 34.0f);
    place (slotA, 470.0f, 29.0f, 34.0f, 34.0f);
    place (slotB, 504.0f, 29.0f, 34.0f, 34.0f);
    place (settings, 546.0f, 29.0f, 34.0f, 34.0f);

#if N73_LICENCE
    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), s, { 176, 1099, 248, 20 });   // la marge sous la carte
#endif
    renderBackground();
    onirique::window::remember (processor.apvts.state, getWidth(), getHeight());
}

void N73Editor::refreshHeader()
{
    const auto name = processor.presets.currentName();
    if (name != shownPreset) { shownPreset = name; presetName.repaint(); }
    if (processor.getActiveSlot() != shownSlot) { shownSlot = processor.getActiveSlot(); slotA.repaint(); slotB.repaint(); }
}

//==============================================================================
void N73Editor::renderBackground()
{
    const float k = juce::jmax (1.0f, (float) juce::Component::getApproximateScaleFactorForComponent (this));
    background = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * k)),
                              juce::jmax (1, juce::roundToInt ((float) getHeight() * k)), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale() * k));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    // fond et lueurs
    g.fillAll (ui::bg);
    g.setGradientFill (juce::ColourGradient (ui::blood.withAlpha (0.6f), 90, 0, ui::blood.withAlpha (0.0f), 90 + 520, 0, true));
    g.fillRect (0, 0, baseWidth, baseHeight);
    g.setGradientFill (juce::ColourGradient (ui::blood.withAlpha (0.35f), 600, 1120, ui::blood.withAlpha (0.0f), 600 - 420, 1120, true));
    g.fillRect (0, 0, baseWidth, baseHeight);

    // bandeau : sigil, ONIRIQUE, N73
    if (logo.isValid()) g.drawImage (logo, juce::Rectangle<float> (20, 30, 36, 32), juce::RectanglePlacement::centred);
    {
        auto f = ui::font (ui::Face::display, 13.0f);
        f.setExtraKerningFactor (0.12f);
        g.setFont (f);
        g.setColour (ui::bone);
        g.drawText ("ONIRIQUE", juce::Rectangle<float> (64, 30, 120, 32), juce::Justification::centredLeft, false);
        const float w = juce::GlyphArrangement::getStringWidth (f, "ONIRIQUE");
        auto big = ui::font (ui::Face::display, 22.0f);
        big.setExtraKerningFactor (0.06f);
        g.setFont (big);
        g.setColour (ui::ember);
        g.drawText ("N73", juce::Rectangle<float> (64 + w + 10, 28, 90, 36), juce::Justification::centredLeft, false);
    }
    // pastilles du bandeau (fonds)
    auto pill = [&g] (juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xf0121012));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (ui::rule);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    };
    pill ({ 282, 29, 180, 34 });
    pill ({ 470, 29, 68, 34 });

    // carte
    const juce::Rectangle<float> card (20, 86, 560, 1014);
    {
        juce::Path pth; pth.addRoundedRectangle (card, 16.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 60, { 0, 30 }).drawForPath (g, pth);
        juce::ColourGradient cg (juce::Colour (0xff241b1d), card.getX(), card.getY(), juce::Colour (0xff0f0d0f), card.getRight(), card.getBottom(), false);
        cg.addColour (0.45, juce::Colour (0xff151214));
        g.setGradientFill (cg);
        g.fillPath (pth);
        g.setColour (juce::Colour (0x1affd6da));
        g.strokePath (pth, juce::PathStrokeType (1.0f));
        g.setColour (ui::ember.withAlpha (0.08f));
        g.drawRoundedRectangle (card.reduced (1.5f), 15.0f, 1.0f);
    }

    // module
    const juce::Rectangle<float> module (moduleX, moduleY, 262, 980);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1a1f), 0, module.getY(), juce::Colour (0xff151317), 0, module.getBottom(), false));
    g.fillRoundedRectangle (module, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (module, 4.0f, 1.0f);
    g.setColour (juce::Colour (0x12fff4f5));
    g.drawLine (module.getX() + 4, module.getY() + 1, module.getRight() - 4, module.getY() + 1, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (module.getRight(), module.getY() + 2, 2.0f, module.getHeight() - 4);
    for (auto sc : std::initializer_list<juce::Point<float>> { { 15 + 6.5f, 12 + 6.5f }, { 234 + 6.5f, 12 + 6.5f }, { 15 + 6.5f, 952 + 6.5f }, { 234 + 6.5f, 952 + 6.5f } })
        screw (g, sc + juce::Point<float> (moduleX, moduleY), 13.0f, sc.x < 100 ? 30.0f : -20.0f);
    if (logo.isValid())
    {
        g.setOpacity (0.9f);
        g.drawImage (logo, juce::Rectangle<float> (moduleX + 46, moduleY + 39, 32, 28), juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }
    // symboles et unités
    {
        const float iy[4] = { highY + 66, midY + 67, lowY + 66, hpfY + 71 };
        for (int i = 0; i < 4; ++i)
        {
            auto path = icon (i);
            path.applyTransform (juce::AffineTransform::translation (knobX + 73 - 13, iy[i] - 7));
            g.setColour (ui::bone);
            g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        ui::textAt (g, "kHz", { knobX - 82, midY + 67 }, 11, ui::bone);
        ui::textAt (g, "Hz", { knobX - 82, lowY + 68 }, 11, ui::bone);
        ui::textAt (g, "Hz", { knobX - 82, hpfY + 71 }, 11, ui::bone);
    }
    {
        g.setColour (ui::bone);
        g.setFont (ui::font (ui::Face::display, 11.0f));
        g.drawText ("N73", juce::Rectangle<float> (moduleX + 22, moduleY + 926, 60, 16), juce::Justification::centredLeft, false);
    }

    // panneau de droite
    const juce::Rectangle<float> panel (panelX, moduleY, panelW, 980);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2427), 0, panel.getY(), juce::Colour (0xff1c181b), 0, panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (panel, 4.0f, 1.0f);
    g.setColour (juce::Colour (0xff0b090b));
    g.fillRect (panel.getX() + 1, moduleY + 118, panel.getWidth() - 2, 2.0f);
    g.fillRect (panel.getX() + 1, bottomTop - 2, panel.getWidth() - 2, 2.0f);

    // fader : vis, graduations, crochets
    screw (g, { panelCX, faderTop + 23 }, 14.0f, 10.0f);
    screw (g, { panelCX, faderTop + 662 - 23 }, 14.0f, -35.0f);
    const char* texts[9] = { "10", "5", "0", "5", "10", "20", "30", "40", "" };
    for (int i = 0; i < 9; ++i)
    {
        const float y = faderTop + ui::ConsoleFader::marks[i][1];
        const juce::String t = i == 8 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : juce::String (texts[i]);
        const auto col = i == 2 ? ui::ember : ui::bone;
        ui::textAt (g, t, { panelX + 50, y }, 16, col);
        ui::textAt (g, t, { panelX + panelW - 50, y }, 16, col);
    }
    {
        const float y0 = faderTop + ui::ConsoleFader::yFor (-30.0f) - 12, y1 = faderTop + ui::ConsoleFader::yFor (-80.0f) + 12;
        g.setColour (ui::bone.withAlpha (0.55f));
        // « ] » à gauche (ouvert vers les chiffres), « [ » à droite, comme dans le design
        for (const auto [line, tick] : { std::pair<float, float> { panelX + 78.0f, panelX + 72.0f }, { panelX + panelW - 78.0f, panelX + panelW - 72.0f } })
        {
            g.drawLine (line, y0, line, y1, 1.0f);
            g.drawLine (line, y0, tick, y0, 1.0f);
            g.drawLine (line, y1, tick, y1, 1.0f);
        }
    }
    {
        auto f = ui::font (ui::Face::body, 12.0f);
        f.setExtraKerningFactor (0.14f);
        g.setFont (f);
        g.setColour (ui::bone);
        g.drawText ("OUTPUT", juce::Rectangle<float> (panelCX - 60, bottomTop + 116, 120, 16), juce::Justification::centred, false);
    }
}

void N73Editor::paint (juce::Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
    else
        g.fillAll (ui::bg);
}

//==============================================================================
void N73Editor::showPresetMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLook);
    m.addSectionHeader ("Presets ONIRIQUE");
    const auto& factory = factoryPresets();
    const auto current = processor.presets.currentName();
    for (int i = 0; i < (int) factory.size(); ++i)
        m.addItem (1 + i, factory[(size_t) i].name, true, factory[(size_t) i].name == current);
    const auto users = processor.presets.userPresets();
    if (! users.isEmpty())
    {
        m.addSectionHeader ("Mes presets");
        for (int i = 0; i < users.size(); ++i)
            m.addItem (100 + i, users[i].getFileNameWithoutExtension(), true, users[i].getFileNameWithoutExtension() == current);
    }
    m.addSeparator();
    m.addItem (1000, juce::String (juce::CharPointer_UTF8 ("Enregistrer le réglage actuel…")));
    m.addItem (1001, "Ouvrir le dossier des presets");
    juce::Component::SafePointer<N73Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName), [safe, users] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& ed = *safe;
        if (r >= 1 && r < 100) ed.processor.presets.loadFactory (r - 1);
        else if (r >= 100 && r < 1000) ed.processor.presets.loadFile (users[r - 100]);
        else if (r == 1001) { N73PresetManager::folder().createDirectory(); N73PresetManager::folder().startAsProcess(); }
        else if (r == 1000)
        {
            // la boîte appartient à l'éditeur : elle disparaît avec lui
            ed.saveDialog = std::make_unique<juce::AlertWindow> ("Enregistrer le preset", "Nom du preset :", juce::MessageBoxIconType::NoIcon, &ed);
            auto* w = ed.saveDialog.get();
            w->setLookAndFeel (&ed.menuLook);
            w->addTextEditor ("name", ed.processor.presets.currentName());
            w->addButton ("Enregistrer", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
            {
                if (safe == nullptr || safe->saveDialog == nullptr) return;
                if (result == 1) safe->processor.presets.save (safe->saveDialog->getTextEditorContents ("name"));
                safe->refreshHeader();
                juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->saveDialog.reset(); });
            }), false);
        }
        ed.refreshHeader();
    });
}

void N73Editor::showSettingsMenu()
{
    auto& ap = processor.apvts;
    auto choiceIndex = [&ap] (const juce::String& id) { return (int) ap.getRawParameterValue (id)->load(); };
    auto setParam = [&ap] (const juce::String& id, float realValue)
    {
        auto* prm = ap.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        prm->endChangeGesture();
    };

    juce::PopupMenu cal;
    for (int i = 0; i < 4; ++i) cal.addItem (10 + i, "0 dBFS = +" + juce::String (18 + 2 * i) + " dBu", true, choiceIndex (params::cal) == i);
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLook);
    m.addSectionHeader (juce::String (juce::CharPointer_UTF8 ("Réglages (modernes, hors matériel)")));
    m.addSubMenu ("Calibration", cal);
    m.addItem (20, juce::String (juce::CharPointer_UTF8 ("LEVEL MATCH — comparer à niveau égal")), true, choiceIndex (params::levelMatch) == 1);
    m.addItem (30, juce::String (juce::CharPointer_UTF8 ("Qualité normale (×2 à 44,1 / 48 kHz)")), true, choiceIndex (params::quality) == 0);
    m.addItem (31, juce::String (juce::CharPointer_UTF8 ("Qualité haute (×4 : moins de repliement, CPU ×2)")), true, choiceIndex (params::quality) == 1);
    m.addSeparator();
    m.addItem (40, processor.getActiveSlot() == 0 ? juce::String (juce::CharPointer_UTF8 ("Copier A → B")) : juce::String (juce::CharPointer_UTF8 ("Copier B → A")));
    m.addSeparator();
    const double rate = processor.getWorkingRate();
    m.addItem (99, juce::String (juce::CharPointer_UTF8 ("Latence : ")) + juce::String (processor.getLatency()) + juce::String (juce::CharPointer_UTF8 (" éch. · travail à "))
                   + juce::String (rate / 1000.0, 1) + " kHz", false, false);
    m.addItem (98, "ONIRIQUE N73 v" + juce::String (JucePlugin_VersionString), false, false);

    juce::Component::SafePointer<N73Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settings), [safe, setParam, choiceIndex] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 10 && r < 14) setParam (params::cal, (float) (r - 10));
        else if (r == 20) setParam (params::levelMatch, choiceIndex (params::levelMatch) == 1 ? 0.0f : 1.0f);
        else if (r == 30 || r == 31) setParam (params::quality, (float) (r - 30));
        else if (r == 40) safe->processor.copyActiveToOther();
    });
}
