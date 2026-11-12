#pragma once

// ============================================================================
// ProC2Theme.h — Design System "alpha-comp" (inspiré FabFilter Pro-C 2)
// ----------------------------------------------------------------------------
//  - Fonds gris anthracite très sombres, presque mats.
//  - Accents bleu doux (courbe / arc actif) et orange (réduction de gain).
//  - Police fine et lisible, petits labels en majuscules grises.
//  - ProC2LookAndFeel : arc rotatif fin, corps sombre, pointeur clair —
//    le look épuré des knobs FabFilter (valeur exacte affichée sous le knob).
// ============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

namespace AlphaComp::UI {

class ProC2Theme
{
public:
    static constexpr float kCornerRadius = 6.0f;

    static juce::Colour background();
    static juce::Colour panelBackground();
    static juce::Colour curveBackground();
    static juce::Colour gridLine();
    static juce::Colour textPrimary();
    static juce::Colour textSecondary();
    static juce::Colour accentBlue();
    static juce::Colour accentOrange();
    static juce::Colour accentRed();
    static juce::Colour accentGreen();
    static juce::Colour diagonalLine();
    static juce::Colour curveLine();

    static juce::Font getFont(float size, bool bold = false);

    // Panneau arrondi sombre avec fine bordure (fond des sections).
    static void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds);
};

// Look & Feel des sliders rotatifs : arc fin bleu, corps anthracite.
class ProC2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minAngleDegrees, float maxAngleDegrees,
                          juce::Slider& slider) override;
};

} // namespace AlphaComp::UI
