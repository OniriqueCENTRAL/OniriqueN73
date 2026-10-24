#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../dsp/N73Core.h"

namespace ui
{
//==============================================================================
/** Bulle de valeur, affichée par l'éditeur au-dessus des contrôles pendant un réglage. */
class ValueBubble : public juce::Component
{
public:
    ValueBubble() { setInterceptsMouseClicks (false, false); setAlwaysOnTop (true); }

    void show (juce::Component& from, const juce::String& t, juce::Point<float> anchorInFrom, float scale)
    {
        text = t;
        s = scale;
        const auto p = getParentComponent()->getLocalPoint (&from, anchorInFrom);
        const float w = juce::GlyphArrangement::getStringWidth (font (Face::mono, 12.0f * s), text) + 22.0f * s, h = 26.0f * s;
        setBounds (juce::Rectangle<float> (w, h).withCentre ({ p.x, p.y - h }).toNearestInt());
        setVisible (true);
        repaint();
    }
    void hide() { setVisible (false); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xf0121012));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (ember.withAlpha (0.55f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (bone);
        g.setFont (font (Face::mono, 12.0f * s));
        g.drawText (text, r, juce::Justification::centred, false);
    }

private:
    juce::String text;
    float s = 1.0f;
};

//==============================================================================
/** Les boutons de la façade du 1073 (images 3D + repères et étiquettes dessinés à l'échelle).
    Gain    : commutateur 22 positions (levier rouge)
    High    : potentiomètre (capuchon gris sur jupe sombre)
    MidLow  : bague crème = fréquence (commutateur), capuchon gris = gain (potentiomètre)
    Hpf     : commutateur 5 positions (levier bleu nuit)
    Output  : potentiomètre (bouton noir)
    Coordonnées de base : celles du design (module de 262 × 980), multipliées par l'échelle de l'éditeur. */
class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { Gain, High, MidLow, Hpf, Output };

    // index : position mise en braise (-1 : jamais) ; micInput / lineInput : les textes MIC et LINE du gain
    struct Label { juce::String text; float dx, dy, size; int index; };
    static constexpr int micInput = -2, lineInput = -3;

    /** gain : dernière position de la plage MIC ou LINE, reprise quand on clique sur MIC ou LINE. */
    std::function<int (bool mic)> inputMemory;

    Knob (Kind k, Assets& a, ValueBubble& b, juce::RangedAudioParameter& mainParam, juce::RangedAudioParameter* ringParam,
          juce::String accessibleName)
        : kind (k), assets (a), bubble (b), main (mainParam), ring (ringParam),
          mainAttach (mainParam, [this] (float v) { mainValue = v; repaint(); }),
          ringAttach (ringParam != nullptr ? std::make_unique<juce::ParameterAttachment> (*ringParam, [this] (float v) { ringValue = v; repaint(); })
                                           : nullptr)
    {
        setTitle (accessibleName);
        setWantsKeyboardFocus (true);
        mainAttach.sendInitialUpdate();
        if (ringAttach != nullptr) ringAttach->sendInitialUpdate();
        setupGeometry();
    }

    /** centre du bouton, en coordonnées de base du composant. */
    void setLayout (juce::Point<float> centreBase, float scale)
    {
        s = scale;
        centre = centreBase * s;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const auto c = centre;
        // repères
        for (size_t i = 0; i < dotAngles.size(); ++i)
        {
            const bool sel = (int) i == selectedDot();
            const auto p = polar (c, dotRadius * s, dotAngles[i]);
            const float d = (sel ? 7.0f : 6.0f) * s;
            if (sel)
            {
                g.setColour (ember.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (d * 2.2f, d * 2.2f).withCentre (p));
            }
            g.setColour (sel ? ember : bone);
            g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
        }
        for (const auto& l : labels)
        {
            const bool lit = l.index >= 0 ? l.index == highlightedLabelIndex()
                                          : (l.index == micInput && ! lineSelected()) || (l.index == lineInput && lineSelected());
            textAt (g, l.text, c + juce::Point<float> (l.dx, l.dy) * s, l.size * s, lit ? ember : bone);
        }

        // le bouton
        switch (kind)
        {
            case Kind::Gain:  image (g, "gain_" + juce::String (index()).paddedLeft ('0', 2), 164.0f); break;
            case Kind::Hpf:   image (g, "hpf_" + juce::String (index()), 148.0f); break;
            case Kind::High:
                image (g, "high_base", 136.0f);
                image (g, "high_cap_" + capFrame (capAngle()), 136.0f);
                pointer (g, c, capAngle(), 24.5f * s, 34.0f * s, 2.6f * s, ui::paint);
                break;
            case Kind::MidLow:
                image (g, "ring", 148.0f);
                pointer (g, c, ringAngle(), 42.0f * s, 52.5f * s, 3.2f * s, ember);
                image (g, "cap_" + capFrame (capAngle()), 148.0f);
                pointer (g, c, capAngle(), 25.5f * s, 37.0f * s, 2.6f * s, ui::paint);
                break;
            case Kind::Output:
                image (g, "out_" + capFrame (capAngle()), 100.0f);
                pointer (g, c, capAngle(), 9.0f * s, 25.0f * s, 2.4f * s, ui::paint);
                break;
        }
        if (hasKeyboardFocus (false))
        {
            g.setColour (ember.withAlpha (0.5f));
            g.drawEllipse (juce::Rectangle<float> (knobRadius * 2.3f * s, knobRadius * 2.3f * s).withCentre (c), 1.2f * s);
        }
    }

    bool hitTest (int x, int y) override
    {
        const auto p = juce::Point<float> ((float) x, (float) y);
        if (p.getDistanceFrom (centre) <= knobRadius * 1.15f * s) return true;
        return labelAt (p) >= 0 || inputAt (p) != 0;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        // gain : clic sur MIC ou LINE, le bouton reprend la dernière position de cette entrée (comme sur le plugin UAD)
        if (const int input = inputAt (e.position); input != 0)
        {
            target = Target::Main;
            const bool mic = input == micInput;
            if (lineSelected() == mic)
                setIndex (target, inputMemory ? inputMemory (mic) : (mic ? n73::defaultMicIndex : n73::defaultLineIndex));
            dragging = false;
            showValue();
            return;
        }
        // clic sur une étiquette de position : on y va directement (pour le gain, y compris dans l'autre plage)
        const int li = labelAt (e.position);
        if (li >= 0 && e.position.getDistanceFrom (centre) > knobRadius * s)
        {
            target = isRingLabel ? Target::Ring : Target::Main;
            setIndex (target, li);
            dragging = false;
            showValue();
            return;
        }
        target = (kind == Kind::MidLow && e.position.getDistanceFrom (centre) > capRadius * s) ? Target::Ring : Target::Main;
        param (target).beginChangeGesture();
        dragging = true;
        dragStart = e.position.y - e.position.x * 0.3f;
        startValue = value (target);
        accum = 0.0f;
        showValue();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const float travel = (dragStart - (e.position.y - e.position.x * 0.3f)) / s;
        if (isSwitch (target))
        {
            const int steps = (int) std::round (travel / 16.0f);
            setIndexLive (target, withinInput (target, (int) std::round (startValue), (int) std::round (startValue) + steps));
        }
        else
        {
            const auto range = param (target).getNormalisableRange().getRange();
            const float perPx = range.getLength() / (e.mods.isShiftDown() ? 900.0f : 220.0f);
            setLive (target, juce::jlimit (range.getStart(), range.getEnd(), startValue + travel * perPx));
        }
        showValue();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging) param (target).endChangeGesture();
        dragging = false;
        bubble.hide();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        target = (kind == Kind::MidLow && e.position.getDistanceFrom (centre) > capRadius * s) ? Target::Ring : Target::Main;
        auto& p = param (target);
        if (kind == Kind::Gain && target == Target::Main)
        {
            // gain : position de départ de l'entrée en cours (LINE 0 ou MIC 20), sans changer d'entrée
            setIndex (target, lineSelected() ? n73::defaultLineIndex : n73::defaultMicIndex);
            return;
        }
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.getDefaultValue());
        p.endChangeGesture();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        target = (kind == Kind::MidLow && e.position.getDistanceFrom (centre) > capRadius * s) ? Target::Ring : Target::Main;
        const float d = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
        if (d == 0.0f) return;
        nudge (target, d > 0 ? 1 : -1, e.mods.isShiftDown());
        showValue();
        bubbleTimer.startTimer (900);
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const bool up = k.isKeyCode (juce::KeyPress::upKey) || k.isKeyCode (juce::KeyPress::rightKey);
        const bool down = k.isKeyCode (juce::KeyPress::downKey) || k.isKeyCode (juce::KeyPress::leftKey);
        if (! up && ! down) return false;
        // MID / LOW : Alt agit sur la bague (fréquence)
        target = (kind == Kind::MidLow && k.getModifiers().isAltDown()) ? Target::Ring : Target::Main;
        nudge (target, up ? 1 : -1, k.getModifiers().isShiftDown());
        return true;
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        struct Value : public juce::AccessibilityRangedNumericValueInterface
        {
            explicit Value (Knob& k) : knob (k) {}
            bool isReadOnly() const override { return false; }
            double getCurrentValue() const override { return knob.value (Target::Main); }
            void setValue (double v) override { knob.setOnce (Target::Main, (float) v); }
            AccessibleValueRange getRange() const override
            {
                const auto r = knob.main.getNormalisableRange();
                return { { r.start, r.end }, knob.isSwitch (Target::Main) ? 1.0 : 0.1 };
            }
            Knob& knob;
        };
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider, juce::AccessibilityActions{},
                                                             juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
    }

private:
    enum class Target { Main, Ring };

    juce::RangedAudioParameter& param (Target t) { return t == Target::Ring && ring != nullptr ? *ring : main; }
    float value (Target t) const { return t == Target::Ring ? ringValue : mainValue; }
    bool isSwitch (Target t) const { return t == Target::Ring || kind == Kind::Gain || kind == Kind::Hpf; }
    int index() const { return (int) std::round (mainValue); }
    int ringIndex() const { return (int) std::round (ringValue); }

    void setLive (Target t, float v)
    {
        auto& p = param (t);
        p.setValueNotifyingHost (p.convertTo0to1 (v));
    }
    void setOnce (Target t, float v)
    {
        auto& p = param (t);
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (v));
        p.endChangeGesture();
    }
    void setIndexLive (Target t, int i)
    {
        const auto r = param (t).getNormalisableRange();
        setLive (t, (float) juce::jlimit ((int) r.start, (int) r.end, i));
    }
    void setIndex (Target t, int i)
    {
        const auto r = param (t).getNormalisableRange();
        setOnce (t, (float) juce::jlimit ((int) r.start, (int) r.end, i));
    }
    void nudge (Target t, int dir, bool fine)
    {
        if (isSwitch (t)) { const int i = (int) std::round (value (t)); setIndex (t, withinInput (t, i, i + dir)); return; }
        const auto r = param (t).getNormalisableRange();
        setOnce (t, juce::jlimit (r.start, r.end, value (t) + (float) dir * (fine ? 0.1f : 0.5f)));
    }

    void showValue()
    {
        auto& p = param (target);
        juce::String t = p.getCurrentValueAsText();
        if (kind == Kind::MidLow) t = (target == Target::Ring ? "FREQ " : "GAIN ") + t;
        bubble.show (*this, t, centre.translated (0.0f, -knobRadius * 1.05f * s), s);
    }

    // angles (0 = en haut, sens horaire), d'après le design
    float capAngle() const
    {
        if (kind == Kind::Output) return mainValue < 0.0f ? 135.0f * mainValue / 24.0f : 135.0f * mainValue / 12.0f;
        const float range = kind == Kind::MidLow && isMid ? 18.0f : 16.0f;
        return 180.0f + 118.0f * mainValue / range;
    }
    float ringAngle() const { return 90.0f + 30.0f * (float) ringIndex(); }
    static juce::String capFrame (float deg)
    {
        float m = std::fmod (deg, 18.0f);
        if (m < 0.0f) m += 18.0f;
        return juce::String ((int) std::round (m / 1.5f) % 12).paddedLeft ('0', 2);
    }

    int selectedDot() const
    {
        if (kind == Kind::Gain || kind == Kind::Hpf) return index();
        return -1;   // le gain d'EQ n'est montré que par son trait blanc
    }
    int highlightedLabelIndex() const
    {
        if (kind == Kind::Gain || kind == Kind::Hpf) return index();
        if (kind == Kind::MidLow) return ringIndex();
        return -1;
    }

    void image (juce::Graphics& g, const juce::String& name, float boxBase)
    {
        const auto img = assets.get (name);
        if (img.isValid())
            g.drawImage (img, juce::Rectangle<float> (boxBase * s, boxBase * s).withCentre (centre));
    }

    // gain : LINE et MIC sont deux plages ; tourner le bouton ne fait pas passer de l'une à l'autre
    bool lineSelected() const { return kind == Kind::Gain && n73::isLineIndex (index()); }
    int withinInput (Target t, int from, int to) const
    {
        if (kind != Kind::Gain || t != Target::Main) return to;
        return n73::isLineIndex (from) ? juce::jlimit (0, n73::lastLineIndex, to)
                                       : juce::jlimit (n73::lastLineIndex + 1, n73::numGainPositions - 1, to);
    }

    /** micInput ou lineInput si p est sur le texte MIC ou LINE du gain, 0 sinon. */
    int inputAt (juce::Point<float> p) const
    {
        for (const auto& l : labels)
            if ((l.index == micInput || l.index == lineInput)
                && p.getDistanceFrom (centre + juce::Point<float> (l.dx, l.dy) * s) < 18.0f * s)
                return l.index;
        return 0;
    }

    int labelAt (juce::Point<float> p) const
    {
        for (const auto& l : labels)
            if (l.index >= 0 && p.getDistanceFrom (centre + juce::Point<float> (l.dx, l.dy) * s) < 12.0f * s)
            {
                const_cast<Knob*> (this)->isRingLabel = kind == Kind::MidLow;
                return l.index;
            }
        return -1;
    }

    void setupGeometry()
    {
        auto spread = [] (int n, float a0, float a1)
        {
            std::vector<float> v;
            for (int i = 0; i < n; ++i) v.push_back (a0 + (a1 - a0) * (float) i / (float) (n - 1));
            return v;
        };
        auto eqDots = [&] (int n)
        {
            auto v = spread (n, 62.0f, 298.0f);
            v.erase (v.begin() + n / 2);   // le « 0 » imprimé remplace le cran du milieu
            return v;
        };
        switch (kind)
        {
            case Kind::Gain:
                for (int i = 0; i < 22; ++i) dotAngles.push_back (45.0f + 15.0f * (float) i);
                dotRadius = 64.5f; knobRadius = 58.0f;
                labels = { { "dB", 4, -98, 12, -1 }, { juce::CharPointer_UTF8 ("\xe2\x88\x92" "80"), -4, -78, 12, 21 }, { "70", -38, -69, 12, 19 },
                           { "60", -71, -36, 12, 17 }, { "OFF", -85, -1, 11, 15 }, { "50", -78, 16, 12, 14 }, { "40", -59, 48, 12, 12 },
                           { "MIC.", -73, 74, 12, micInput }, { "30", -18, 74, 12, 10 }, { juce::CharPointer_UTF8 ("\xe2\x88\x92" "20"), 21, 74, 12, 8 },
                           { "OFF", 53, 66, 11, 7 }, { "+10", 65, 47, 12, 6 }, { "0", 74, 19, 12, 4 }, { "10", 76, -20, 12, 2 },
                           { juce::CharPointer_UTF8 ("\xe2\x88\x92" "20"), 59, -54, 12, 0 }, { "LINE", 61, -78, 12, lineInput } };
                break;
            case Kind::High:
                dotAngles = eqDots (17); dotRadius = 63.0f; knobRadius = 48.0f;
                labels = { { "+", -56, -46, 16, -1 }, { juce::CharPointer_UTF8 ("\xe2\x88\x92"), 55, -46, 16, -1 }, { "0", 0, 66, 12, -1 } };
                break;
            case Kind::MidLow:
                dotRadius = 63.0f; knobRadius = 54.0f; capRadius = 39.0f;
                break;
            case Kind::Hpf:
                for (int i = 0; i < 5; ++i) dotAngles.push_back (90.0f + 30.0f * (float) i);
                dotRadius = 66.0f; knobRadius = 54.0f;
                labels = { { "OFF", 86, 1, 11, 0 }, { "50", 74, 38, 12, 1 }, { "80", 38, 74, 12, 2 }, { "160", 1, 81, 12, 3 }, { "300", -37, 74, 12, 4 } };
                break;
            case Kind::Output:
                knobRadius = 28.0f;
                labels = { { "0", 0, -38, 13, -1 }, { juce::CharPointer_UTF8 ("\xe2\x88\x92" "24"), -34, 40, 13, -1 }, { "12 dB", 42, 40, 13, -1 } };
                break;
        }
    }

public:
    /** MID ou LOW (à appeler juste après la construction d'un bouton MidLow). */
    void setMidLow (bool mid)
    {
        isMid = mid;
        dotAngles.clear();
        auto v = std::vector<float>();
        const int n = mid ? 19 : 17;
        for (int i = 0; i < n; ++i) if (i != n / 2) v.push_back (62.0f + 236.0f * (float) i / (float) (n - 1));
        dotAngles = v;
        const juce::String minus = juce::CharPointer_UTF8 ("\xe2\x88\x92");
        if (mid)
            labels = { { "+", -56, -46, 16, -1 }, { minus, 55, -46, 16, -1 }, { "0", 0, 65, 11, -1 },
                       { "OFF", 85, 2, 11, 0 }, { juce::CharPointer_UTF8 ("0\xc2\xb7" "36"), 80, 38, 11, 1 }, { juce::CharPointer_UTF8 ("0\xc2\xb7" "7"), 38, 72, 11, 2 },
                       { juce::CharPointer_UTF8 ("1\xc2\xb7" "6"), 1, 81, 11, 3 }, { juce::CharPointer_UTF8 ("3\xc2\xb7" "2"), -19, 72, 11, 4 },
                       { juce::CharPointer_UTF8 ("4\xc2\xb7" "8"), -67, 34, 11, 5 }, { juce::CharPointer_UTF8 ("7\xc2\xb7" "2"), -75, 2, 11, 6 } };
        else
            labels = { { "+", -56, -46, 16, -1 }, { minus, 55, -46, 16, -1 }, { "0", 0, 64, 11, -1 },
                       { "OFF", 85, 2, 11, 0 }, { "35", 72, 36, 11, 1 }, { "60", 38, 71, 11, 2 }, { "110", 1, 79, 11, 3 }, { "220", -35, 71, 11, 4 } };
        repaint();
    }

private:
    Kind kind;
    Assets& assets;
    ValueBubble& bubble;
    juce::RangedAudioParameter& main;
    juce::RangedAudioParameter* ring;
    juce::ParameterAttachment mainAttach;
    std::unique_ptr<juce::ParameterAttachment> ringAttach;
    float mainValue = 0.0f, ringValue = 0.0f, s = 1.0f;
    juce::Point<float> centre;
    std::vector<float> dotAngles;
    std::vector<Label> labels;
    float dotRadius = 63.0f, knobRadius = 50.0f, capRadius = 39.0f;
    bool isMid = true, dragging = false, isRingLabel = false;
    Target target = Target::Main;
    float dragStart = 0.0f, startValue = 0.0f, accum = 0.0f;

    struct BubbleTimer : juce::Timer
    {
        explicit BubbleTimer (ValueBubble& b) : bubble (b) {}
        void timerCallback() override { stopTimer(); bubble.hide(); }
        ValueBubble& bubble;
    } bubbleTimer { bubble };
};

//==============================================================================
/** Inverseur à levier (MIC Z, PAD, POWER) : texte à gauche, fente, levier chromé, texte à droite, nom dessous. */
class BatToggle : public juce::Component
{
public:
    BatToggle (juce::RangedAudioParameter& p, juce::String leftText, juce::String rightText, juce::String nameText)
        : param (p), left (std::move (leftText)), right (std::move (rightText)), name (std::move (nameText)),
          attach (p, [this] (float v) { on = v > 0.5f; repaint(); })
    {
        setTitle (name);
        setWantsKeyboardFocus (true);
        attach.sendInitialUpdate();
    }

    void setScale (float scale) { s = scale; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const float cx = (float) getWidth() * 0.5f, rowY = 16.0f * s;   // marge au-dessus : le levier dépasse de la fente
        const auto slot = juce::Rectangle<float> (50.0f * s, 20.0f * s).withCentre ({ cx, rowY });
        textAt (g, left, { slot.getX() - 12.0f * s - 18.0f * s, rowY }, 13.0f * s, on ? dim : ember);
        textAt (g, right, { slot.getRight() + 12.0f * s + 18.0f * s, rowY }, 13.0f * s, on ? ember : dim);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050405), 0, slot.getY(), juce::Colour (0xff1b181b), 0, slot.getBottom(), false));
        g.fillRoundedRectangle (slot, slot.getHeight() * 0.5f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (slot, slot.getHeight() * 0.5f, 1.0f * s);
        const float bx = on ? slot.getRight() - 20.0f * s : slot.getX() + 2.0f * s;
        const auto bat = juce::Rectangle<float> (bx, slot.getY() - 3.0f * s, 18.0f * s, 24.0f * s);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bat.translated (3.0f * s, 4.0f * s), 9.0f * s);
        juce::ColourGradient chrome (juce::Colour (0xff8e8789), bat.getX(), 0, juce::Colour (0xff6d6567), bat.getRight(), 0, false);
        chrome.addColour (0.35, juce::Colours::white);
        chrome.addColour (0.55, juce::Colour (0xffd9d2d4));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (bat, 9.0f * s);
        textAt (g, name, { cx, rowY + 22.0f * s }, 12.0f * s, bone);
        if (hasKeyboardFocus (false)) { g.setColour (ember.withAlpha (0.5f)); g.drawRoundedRectangle (slot.expanded (3.0f * s), 12.0f * s, 1.0f * s); }
    }

    void mouseDown (const juce::MouseEvent&) override { if (! isEnabled()) return; grabKeyboardFocus(); flip(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.isKeyCode (juce::KeyPress::spaceKey) || k.isKeyCode (juce::KeyPress::returnKey)) { flip(); return true; }
        return false;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    // désactivé (MIC Z et PAD sur l'entrée LINE, comme sur le plugin UAD) : estompé, ne bascule plus
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.35f); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::toggleButton,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::toggle, [this] { flip(); }));
    }

private:
    void flip()
    {
        if (! isEnabled()) return;
        param.beginChangeGesture();
        param.setValueNotifyingHost (on ? 0.0f : 1.0f);
        param.endChangeGesture();
    }

    juce::RangedAudioParameter& param;
    juce::String left, right, name;
    juce::ParameterAttachment attach;
    bool on = false;
    float s = 1.0f;
};

//==============================================================================
/** Touche crème (EQL, PHASE) : allumée en braise quand elle est enfoncée. */
class KeyButton : public juce::Component
{
public:
    KeyButton (juce::RangedAudioParameter& p, juce::String t, bool leftSide)
        : param (p), text (std::move (t)), isLeft (leftSide), attach (p, [this] (float v) { on = v > 0.5f; repaint(); })
    {
        setTitle (text);
        setWantsKeyboardFocus (true);
        attach.sendInitialUpdate();
    }
    void setScale (float scale) { s = scale; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (on)
        {
            g.setColour (ember.withAlpha (0.35f));
            g.fillRoundedRectangle (r, 6.0f * s);
        }
        r = r.reduced (4.0f * s);
        juce::ColourGradient face (on ? juce::Colour (0xfffff3e6) : juce::Colour (0xffd9d2c6), 0, r.getY(),
                                   on ? juce::Colour (0xfff2dccb) : juce::Colour (0xffb8b0a3), 0, r.getBottom(), false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (r, 2.0f * s);
        if (on)
        {
            g.setColour (ember.withAlpha (0.25f));
            g.drawRoundedRectangle (r.reduced (2.0f * s), 2.0f * s, 3.0f * s);
        }
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 2.0f * s, 1.0f * s);
        g.setColour (on ? juce::Colour (0xff1a1416) : juce::Colour (0xff2b2426));
        g.setFont (font (Face::body, 13.0f * s));
        g.drawText (text, r, juce::Justification::centred, false);
        if (hasKeyboardFocus (false)) { g.setColour (ember); g.drawRoundedRectangle (r.expanded (1.0f * s), 3.0f * s, 1.0f * s); }
    }
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); flip(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.isKeyCode (juce::KeyPress::spaceKey) || k.isKeyCode (juce::KeyPress::returnKey)) { flip(); return true; }
        return false;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::toggleButton,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::toggle, [this] { flip(); }));
    }

private:
    void flip()
    {
        param.beginChangeGesture();
        param.setValueNotifyingHost (on ? 0.0f : 1.0f);
        param.endChangeGesture();
    }
    juce::RangedAudioParameter& param;
    juce::String text;
    bool isLeft;
    juce::ParameterAttachment attach;
    bool on = false;
    float s = 1.0f;
};

//==============================================================================
/** Fader de console : graduations 10 … ∞ du design, poignée noire striée, repère braise. */
class ConsoleFader : public juce::Component
{
public:
    ConsoleFader (juce::RangedAudioParameter& p, ValueBubble& b)
        : param (p), bubble (b), attach (p, [this] (float v) { db = v; repaint(); })
    {
        setTitle ("Fader");
        setWantsKeyboardFocus (true);
        attach.sendInitialUpdate();
    }
    void setScale (float scale) { s = scale; repaint(); }

    // graduations (dB → hauteur, coordonnées de base de la section de 662)
    static constexpr float marks[9][2] = { { 10, 115 }, { 5, 166 }, { 0, 216 }, { -5, 277 }, { -10, 326 }, { -20, 381 },
                                           { -30, 434 }, { -40, 483 }, { -80, 545 } };
    static float yFor (float v)
    {
        for (int i = 0; i < 8; ++i)
            if (v <= marks[i][0] && v >= marks[i + 1][0])
                return marks[i][1] + (marks[i][0] - v) / (marks[i][0] - marks[i + 1][0]) * (marks[i + 1][1] - marks[i][1]);
        return v > 10 ? marks[0][1] : marks[8][1];
    }
    static float dbFor (float y)
    {
        if (y <= marks[0][1]) return 10.0f;
        for (int i = 0; i < 8; ++i)
            if (y >= marks[i][1] && y <= marks[i + 1][1])
                return marks[i][0] + (y - marks[i][1]) / (marks[i + 1][1] - marks[i][1]) * (marks[i + 1][0] - marks[i][0]);
        return -80.0f;
    }

    void paint (juce::Graphics& g) override
    {
        const float cx = (float) getWidth() * 0.5f;
        // fente
        const auto track = juce::Rectangle<float> (cx - 4.0f * s, 44.0f * s, 8.0f * s, (662.0f - 88.0f) * s);
        g.setColour (juce::Colour (0xff050405));
        g.fillRoundedRectangle (track, 4.0f * s);
        g.setColour (juce::Colour (0x12fff4f5));
        g.drawLine (track.getX(), track.getBottom() + 1.0f * s, track.getRight(), track.getBottom() + 1.0f * s, 1.0f * s);
        // poignée
        const auto cap = juce::Rectangle<float> (66.0f * s, 92.0f * s).withCentre ({ cx, yFor (db) * s });
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (cap.translated (0, 12.0f * s).expanded (2.0f * s), 6.0f * s);
        juce::ColourGradient body (juce::Colour (0xff121012), cap.getX(), 0, juce::Colour (0xff121012), cap.getRight(), 0, false);
        body.addColour (0.3, juce::Colour (0xff2c272b));
        body.addColour (0.5, juce::Colour (0xff3b3539));
        body.addColour (0.7, juce::Colour (0xff2c272b));
        g.setGradientFill (body);
        g.fillRoundedRectangle (cap, 4.0f * s);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (cap, 4.0f * s, 1.0f * s);
        for (int i = 0; i < 8; ++i)
        {
            const float y = cap.getY() + (8.0f + (float) i * 10.0f + (i >= 4 ? 8.0f : 0.0f)) * s;
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (cap.getX() + 3.0f * s, y, cap.getWidth() - 6.0f * s, 2.0f * s);
            g.setColour (juce::Colour (0x14fff4f5));
            g.fillRect (cap.getX() + 3.0f * s, y + 2.0f * s, cap.getWidth() - 6.0f * s, 1.0f * s);
        }
        g.setColour (ember.withAlpha (0.5f));
        g.fillRect (cap.getX() + 1.0f * s, cap.getY() + 42.5f * s, cap.getWidth() - 2.0f * s, 6.0f * s);
        g.setColour (ember);
        g.fillRect (cap.getX() + 3.0f * s, cap.getY() + 44.0f * s, cap.getWidth() - 6.0f * s, 3.0f * s);
        if (hasKeyboardFocus (false)) { g.setColour (ember.withAlpha (0.6f)); g.drawRoundedRectangle (cap.expanded (3.0f * s), 6.0f * s, 1.0f * s); }
    }

    bool hitTest (int x, int) override { return std::abs ((float) x - (float) getWidth() * 0.5f) < 40.0f * s; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        param.beginChangeGesture();
        grabOffset = e.position.y / s - yFor (db);
        if (std::abs (grabOffset) > 46.0f) grabOffset = 0.0f;   // clic hors de la poignée : elle vient sous la souris
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        set (dbFor (e.position.y / s - grabOffset));
        bubble.show (*this, param.getCurrentValueAsText(), { (float) getWidth() * 0.5f, (yFor (db) - 50.0f) * s }, s);
    }
    void mouseUp (const juce::MouseEvent&) override { param.endChangeGesture(); bubble.hide(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { param.beginChangeGesture(); set (0.0f); param.endChangeGesture(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float d = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
        param.beginChangeGesture(); set (db + (d > 0 ? 0.5f : -0.5f)); param.endChangeGesture();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int dir = k.isKeyCode (juce::KeyPress::upKey) ? 1 : k.isKeyCode (juce::KeyPress::downKey) ? -1 : 0;
        if (dir == 0) return false;
        param.beginChangeGesture(); set (db + (float) dir * (k.getModifiers().isShiftDown() ? 0.1f : 0.5f)); param.endChangeGesture();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        struct Value : public juce::AccessibilityRangedNumericValueInterface
        {
            explicit Value (ConsoleFader& f) : fader (f) {}
            bool isReadOnly() const override { return false; }
            double getCurrentValue() const override { return fader.db; }
            void setValue (double v) override { fader.param.beginChangeGesture(); fader.set ((float) v); fader.param.endChangeGesture(); }
            AccessibleValueRange getRange() const override { return { { -80.0, 10.0 }, 0.5 }; }
            ConsoleFader& fader;
        };
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider, juce::AccessibilityActions{},
                                                             juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
    }

private:
    void set (float v) { param.setValueNotifyingHost (param.convertTo0to1 (juce::jlimit (-80.0f, 10.0f, v))); }

    juce::RangedAudioParameter& param;
    ValueBubble& bubble;
    juce::ParameterAttachment attach;
    float db = 0.0f, s = 1.0f, grabOffset = 0.0f;
};

} // namespace ui

namespace ui
{
//==============================================================================
/** Apparence des menus et de la boîte d'enregistrement (membre de l'éditeur : jamais statique). */
struct MenuLook : juce::LookAndFeel_V4
{
    MenuLook()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff121012));
        setColour (juce::PopupMenu::textColourId, bone);
        setColour (juce::PopupMenu::headerTextColourId, haze);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ember.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff121012));
        setColour (juce::AlertWindow::textColourId, bone);
        setColour (juce::AlertWindow::outlineColourId, ember.withAlpha (0.4f));
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1c181b));
        setColour (juce::TextEditor::textColourId, bone);
        setColour (juce::TextEditor::outlineColourId, rule);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2427));
        setColour (juce::TextButton::textColourOffId, bone);
    }
    juce::Font getPopupMenuFont() override { return font (Face::body, 15.0f); }
};
} // namespace ui
