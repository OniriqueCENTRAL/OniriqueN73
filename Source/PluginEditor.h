#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Controls.h"
#if N73_LICENCE
 #include "License/LicenseOverlay.h"
#endif

//==============================================================================
//  L'interface du N73, telle que validée dans Claude Design : module 1073 à gauche (GAIN, HIGH, MID, LOW,
//  HIGH PASS, EQL / PHASE, témoin), panneau de droite (MIC Z, PAD, fader, OUTPUT, POWER), bandeau du haut
//  (presets, A / B, réglages). Taille de base 600 × 1120, redimensionnable sans déformation.
//==============================================================================
class N73Editor : public juce::AudioProcessorEditor
{
public:
    explicit N73Editor (N73Processor&);
    ~N73Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 600, baseHeight = 1120;

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
    void showPresetMenu();
    void showSettingsMenu();
    void refreshHeader();
    float scale() const { return (float) getWidth() / (float) baseWidth; }

    N73Processor& processor;
    ui::FontScope fonts;
    ui::MenuLook menuLook;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    ui::Assets assets;
    ui::ValueBubble bubble;
    juce::Image background, logo;

    ui::Knob gain, high, mid, low, hpf, output;
    ui::BatToggle micZ, pad, power;
    ui::KeyButton eql, phase;
    ui::ConsoleFader fader;
    ui::DriveLamp lamp;
    HeaderButton prevPreset, presetName, nextPreset, slotA, slotB, settings;

    struct Refresher : juce::Timer
    {
        explicit Refresher (N73Editor& e) : ed (e) {}
        void timerCallback() override { ed.refreshHeader(); }
        N73Editor& ed;
    } refresher { *this };
    juce::String shownPreset;
    int shownSlot = -1;
    bool sizeReady = false;   // la taille d'ouverture est posée : on peut la mémoriser

#if N73_LICENCE
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (N73Editor)
};
