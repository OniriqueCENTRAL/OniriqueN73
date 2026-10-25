#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/OniriqueBar.h"
#if N73_LICENCE
 #include "License/LicenseOverlay.h"
#endif

//==============================================================================
//  L'interface du N73, telle que validée dans Claude Design : module 1073 à gauche (GAIN, HIGH, MID, LOW,
//  HIGH PASS, EQL / PHASE), panneau de droite (MIC Z, PAD, fader, OUTPUT, POWER), et en haut la barre des plugins
//  ONIRIQUE (le titre, les presets, IN, A / B, le menu ⋯) avec le bouton des réglages. Taille de base 600 × 1128,
//  redimensionnable sans déformation.
//==============================================================================
class N73Editor : public juce::AudioProcessorEditor
{
public:
    explicit N73Editor (N73Processor&);
    ~N73Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 600, baseHeight = 1128;
    static constexpr float topOffset = 8.0f;   // tout descend sous la barre du haut (84 points, deux rangées)

private:
    /** petit bouton du bandeau, dessiné par une fonction. */
    struct HeaderButton : juce::Component
    {
        std::function<void (juce::Graphics&, juce::Rectangle<float>, bool)> draw;
        std::function<void()> onClick;
        void paint (juce::Graphics& g) override { if (draw) draw (g, getLocalBounds().toFloat(), isMouseOver (true)); }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override { if (onClick && contains (e.getPosition())) onClick(); }
    };

    juce::RangedAudioParameter& p (const juce::String& id) { return *processor.apvts.getParameter (id); }
    void renderBackground();
    void showSettingsMenu();
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) const;   // le titre, dans la barre (en points)
    float scale() const { return (float) getWidth() / (float) baseWidth; }

    N73Processor& processor;
    ui::FontScope fonts;
    ui::MenuLook menuLook;
    ui::Assets assets;
    ui::ValueBubble bubble;
    juce::Image background, logo;

    ui::Knob gain, high, mid, low, hpf, output;
    ui::BatToggle micZ, pad, power;
    ui::KeyButton eql, phase;
    ui::ConsoleFader fader;
    // MIC Z et PAD n'existent que pour l'entrée micro : désactivés quand le gain est sur LINE (comme sur le plugin UAD)
    juce::ParameterAttachment inputWatch;
    onirique::nuit::Bar bar;
    HeaderButton settings;   // réglages modernes (CAL, LEVEL MATCH, qualité), à côté du menu de la barre
    bool sizeReady = false;   // la taille d'ouverture est posée : on peut la mémoriser

#if N73_LICENCE
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (N73Editor)
};
