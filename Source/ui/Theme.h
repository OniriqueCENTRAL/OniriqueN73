#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <map>
#include "BinaryData.h"
#include "OniriqueLicenseData.h"

//==============================================================================
//  Charte du N73 (design validé dans Claude Design) : couleurs, polices, images des boutons.
//==============================================================================
namespace ui
{
inline const juce::Colour bg { 0xff09080a }, blood { 0xff3d0910 }, bone { 0xfff2ecec }, haze { 0xffaea3a6 }, dim { 0xff8a7f82 },
                          ember { 0xffe5384a }, paint { 0xfff4f0e8 }, rule { 0x24ffd6da };

enum class Face { display, body, mono };

/** Polices partagées par les interfaces ouvertes : créées à l'ouverture de la première, libérées à la fermeture de la
    dernière (rien de statique n'est détruit au déchargement du plugin). Thread de l'interface uniquement. */
struct FontScope
{
    FontScope()
    {
        if (users()++ == 0)
        {
            auto& f = faces();
            f[0] = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::Unbounded800_ttf, OniriqueLicenseData::Unbounded800_ttfSize);
            f[1] = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::HankenGrotesk600_ttf, OniriqueLicenseData::HankenGrotesk600_ttfSize);
            f[2] = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::JetBrainsMono600_ttf, OniriqueLicenseData::JetBrainsMono600_ttfSize);
        }
    }
    ~FontScope()
    {
        if (--users() == 0) for (auto& t : faces()) t = nullptr;
    }
    static int& users() { static int n = 0; return n; }
    static std::array<juce::Typeface::Ptr, 3>& faces() { static std::array<juce::Typeface::Ptr, 3> f; return f; }
};

inline juce::Font font (Face f, float h)
{
    const auto& tf = FontScope::faces()[(size_t) f];
    h = std::max (3.0f, h);
    if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (f == Face::display ? h * 0.86f : h));
    return juce::Font (juce::FontOptions (f == Face::mono ? "Consolas" : "Segoe UI", h, juce::Font::bold));
}

/** Images des boutons, décodées à la demande et gardées par l'éditeur (jamais d'image statique). */
class Assets
{
public:
    juce::Image get (const juce::String& name)
    {
        if (auto it = cache.find (name); it != cache.end()) return it->second;
        int size = 0;
        const auto resource = (name + "_png").replaceCharacter ('.', '_');
        juce::Image img;
        if (const char* data = BinaryData::getNamedResource (resource.toRawUTF8(), size))
            img = juce::ImageFileFormat::loadFrom (data, (size_t) size);
        cache[name] = img;
        return img;
    }

private:
    std::map<juce::String, juce::Image> cache;
};

/** Angles : 0 = en haut, sens horaire (degrés) → point sur un cercle. */
inline juce::Point<float> polar (juce::Point<float> c, float r, float deg)
{
    const float a = juce::degreesToRadians (deg);
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

/** Le texte centré sur un point. */
inline void textAt (juce::Graphics& g, const juce::String& t, juce::Point<float> c, float size, juce::Colour col, Face f = Face::body)
{
    g.setColour (col);
    g.setFont (font (f, size));
    g.drawText (t, juce::Rectangle<float> (c.x - 60.0f, c.y - size, 120.0f, size * 2.0f), juce::Justification::centred, false);
}

/** Un trait de repère sur un bouton (de r0 à r1 dans la direction deg), avec un liseré sombre pour le détacher. */
inline void pointer (juce::Graphics& g, juce::Point<float> c, float deg, float r0, float r1, float w, juce::Colour col)
{
    const auto a = polar (c, r0, deg), b = polar (c, r1, deg);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine ({ a, b }, w + 1.4f);
    g.setColour (col);
    g.drawLine ({ a, b }, w);
}
} // namespace ui
