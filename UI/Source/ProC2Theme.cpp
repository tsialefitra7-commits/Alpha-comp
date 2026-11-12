#include "UI/ProC2Theme.h"

#include <cmath>

namespace AlphaComp::UI {

juce::Colour ProC2Theme::background()      { return juce::Colour(0xff17181a); }
juce::Colour ProC2Theme::panelBackground() { return juce::Colour(0xff1f2023); }
juce::Colour ProC2Theme::curveBackground() { return juce::Colour(0xff101114); }
juce::Colour ProC2Theme::gridLine()        { return juce::Colour(0xff2a2c30); }
juce::Colour ProC2Theme::textPrimary()     { return juce::Colour(0xffe8e9eb); }
juce::Colour ProC2Theme::textSecondary()   { return juce::Colour(0xff9a9da3); }
juce::Colour ProC2Theme::accentBlue()      { return juce::Colour(0xff4da3ff); }
juce::Colour ProC2Theme::accentOrange()    { return juce::Colour(0xffff9f43); }
juce::Colour ProC2Theme::accentRed()       { return juce::Colour(0xffff5a5a); }
juce::Colour ProC2Theme::accentGreen()     { return juce::Colour(0xff4ade80); }
juce::Colour ProC2Theme::diagonalLine()    { return juce::Colour(0xff6a6d73); }
juce::Colour ProC2Theme::curveLine()       { return juce::Colour(0xff7cc4ff); }

juce::Font ProC2Theme::getFont(float size, bool bold)
{
    // Font::boldened() existe en JUCE 7 et JUCE 8 (l'ancien enum Font::bold
    // est déprécié en JUCE 8) : on reste compatible avec les deux.
    juce::Font font(size);
    return bold ? font.boldened() : font;
}

void ProC2Theme::drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(panelBackground());
    g.fillRoundedRectangle(bounds.toFloat(), kCornerRadius);
    g.setColour(gridLine());
    g.drawRoundedRectangle(bounds.toFloat(), kCornerRadius, 1.0f);
}

// Les angles reçus sont en RADIANS, mesurés depuis 12 h dans le sens
// horaire (convention JUCE : Slider passe startAngleRadians/endAngleRadians,
// et Path::addCentredArc attend exactement cette convention).
// Note : Graphics::drawArc() n'existe plus en JUCE 8 — on dessine les arcs
// via Path::addCentredArc() + Graphics::strokePath() (JUCE 7 et 8).
void ProC2LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minAngleDegrees,
                                        float maxAngleDegrees, juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float radius = juce::jmin(static_cast<float>(width), static_cast<float>(height)) * 0.5f - 2.0f;
    if (radius <= 0.0f)
        return;

    const float startAngle = minAngleDegrees;
    const float endAngle = maxAngleDegrees;
    const float currentAngle = minAngleDegrees + sliderPos * (maxAngleDegrees - minAngleDegrees);

    const juce::Rectangle<float> arcBounds(centreX - radius, centreY - radius,
                                           radius * 2.0f, radius * 2.0f);

    // Piste de fond.
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                                startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff2a2c30));
    g.strokePath(backgroundArc, juce::PathStrokeType(3.0f));

    // Arc actif (bleu doux).
    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                               startAngle, currentAngle, true);
        g.setColour(ProC2Theme::accentBlue());
        g.strokePath(valueArc, juce::PathStrokeType(3.0f));
    }

    // Corps du knob.
    g.setColour(juce::Colour(0xff2e3036));
    g.fillEllipse(arcBounds.reduced(4.0f));
    g.setColour(juce::Colour(0xff3a3d44));
    g.drawEllipse(arcBounds.reduced(4.0f), 1.0f);

    // Pointeur clair (angle depuis 12 h : x = sin, y = -cos).
    const float pointerLength = radius - 6.0f;
    const float px = centreX + pointerLength * std::sin(currentAngle);
    const float py = centreY - pointerLength * std::cos(currentAngle);
    g.setColour(juce::Colour(0xffe8e9eb));
    g.drawLine(centreX, centreY, px, py, 2.0f);

    // Point central.
    g.fillEllipse(centreX - 2.0f, centreY - 2.0f, 4.0f, 4.0f);
}

} // namespace AlphaComp::UI
