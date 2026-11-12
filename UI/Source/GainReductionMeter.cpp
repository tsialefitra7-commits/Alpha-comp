#include "UI/GainReductionMeter.h"

#include "UI/ProC2Theme.h"

#include <algorithm>
#include <cmath>

namespace AlphaComp::UI {

GainReductionMeter::GainReductionMeter() = default;

void GainReductionMeter::setGainReductionDb(float grDb)
{
    targetGrDb_ = std::clamp(grDb, -kMaxGrDb, 0.0f);

    // Lissage ballistique : fluide à 60 fps, sans à-coups.
    displayedGrDb_ += (targetGrDb_ - displayedGrDb_) * 0.35f;
    if (std::abs(displayedGrDb_) < 0.05f)
        displayedGrDb_ = 0.0f;

    // Pic : monte instantanément, redescend lentement.
    if (displayedGrDb_ < peakGrDb_)
        peakGrDb_ = displayedGrDb_;
    else
        peakGrDb_ += (displayedGrDb_ - peakGrDb_) * 0.03f;

    repaint();
}

void GainReductionMeter::setBypassed(bool bypass)
{
    bypassed_ = bypass;
    repaint();
}

void GainReductionMeter::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    g.setColour(ProC2Theme::panelBackground());
    g.fillRoundedRectangle(bounds.toFloat(), ProC2Theme::kCornerRadius);
    g.setColour(ProC2Theme::gridLine());
    g.drawRoundedRectangle(bounds.toFloat(), ProC2Theme::kCornerRadius, 1.0f);

    // Titre.
    g.setFont(ProC2Theme::getFont(11.0f, true));
    g.setColour(ProC2Theme::textSecondary());
    g.drawText("GR", bounds.getX(), bounds.getY() + 6, bounds.getWidth(), 14,
               juce::Justification::centred);

    // Piste LED : centrée, labels d'échelle à droite.
    const float trackWidth = 18.0f;
    const float trackX = bounds.getCentreX() - trackWidth * 0.5f - 8.0f;
    const float trackY = static_cast<float>(bounds.getY()) + 28.0f;
    const float trackHeight = static_cast<float>(bounds.getHeight()) - 28.0f - 30.0f;

    g.setColour(juce::Colour(0xff101114));
    g.fillRoundedRectangle(trackX, trackY, trackWidth, trackHeight, 3.0f);

    constexpr int kNumSegments = 28;
    const float segmentHeight = trackHeight / static_cast<float>(kNumSegments);
    const int litCount = static_cast<int>(std::round(
        (-displayedGrDb_ / kMaxGrDb) * static_cast<float>(kNumSegments)));

    const juce::Colour green = ProC2Theme::accentGreen();
    const juce::Colour orange = ProC2Theme::accentOrange();
    const juce::Colour red = ProC2Theme::accentRed();

    for (int i = 0; i < kNumSegments; ++i)
    {
        const float y = trackY + trackHeight - segmentHeight * static_cast<float>(i + 1);
        const juce::Rectangle<float> segment(trackX + 2.0f, y + 1.0f,
                                             trackWidth - 4.0f, segmentHeight - 2.0f);

        if (i < litCount && !bypassed_)
        {
            const float t = static_cast<float>(i) / static_cast<float>(kNumSegments - 1);
            const juce::Colour colour = (t < 0.5f)
                ? green.interpolatedWith(orange, t * 2.0f)
                : orange.interpolatedWith(red, (t - 0.5f) * 2.0f);
            g.setColour(colour);
        }
        else
        {
            g.setColour(juce::Colour(0xff26272b));
        }
        g.fillRoundedRectangle(segment, 1.5f);
    }

    // Marqueur de pic.
    if (!bypassed_ && peakGrDb_ < -0.1f)
    {
        const float peakY = trackY + trackHeight
                          - (std::abs(peakGrDb_) / kMaxGrDb) * trackHeight;
        g.setColour(ProC2Theme::textPrimary());
        g.fillRect(trackX - 3.0f, peakY - 1.0f, trackWidth + 6.0f, 2.0f);
    }

    // Échelle : 0, 6, 12, 24 dB.
    g.setFont(ProC2Theme::getFont(9.0f));
    g.setColour(ProC2Theme::textSecondary());
    for (float db : { 0.0f, 6.0f, 12.0f, 24.0f })
    {
        const float y = trackY + (db / kMaxGrDb) * trackHeight;
        g.drawText(juce::String(static_cast<int>(db)),
                   static_cast<int>(trackX + trackWidth + 4.0f),
                   static_cast<int>(y) - 6, 22, 12, juce::Justification::centredLeft);
    }

    // Lecture numérique.
    g.setFont(ProC2Theme::getFont(11.0f));
    g.setColour(bypassed_ ? ProC2Theme::textSecondary() : ProC2Theme::textPrimary());
    const juce::String readout = bypassed_ ? juce::String("BYPASS")
                                           : juce::String(displayedGrDb_, 1) + " dB";
    g.drawText(readout, bounds.getX(), bounds.getBottom() - 22, bounds.getWidth(), 16,
               juce::Justification::centred);
}

} // namespace AlphaComp::UI
