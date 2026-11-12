#include "UI/CompressionCurve.h"

#include "UI/ProC2Theme.h"

#include "Core/GainComputer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cmath>

namespace AlphaComp::UI {

CompressionCurve::CompressionCurve(Core::CompressorDSP& dsp,
                                   juce::AudioProcessorValueTreeState* apvts)
    : dsp_(dsp), apvts_(apvts)
{
    startTimer(16); // ~60 fps
}

CompressionCurve::~CompressionCurve()
{
    stopTimer();
}

float CompressionCurve::dbToX(float db, float x0, float width) noexcept
{
    return x0 + (db - kMinDb) / (kMaxDb - kMinDb) * width;
}

float CompressionCurve::xToDb(float x, float x0, float width) noexcept
{
    if (width <= 0.0f)
        return kMinDb;
    return kMinDb + (x - x0) / width * (kMaxDb - kMinDb);
}

float CompressionCurve::dbToY(float db, float y0, float height) noexcept
{
    return y0 + height - (db - kMinDb) / (kMaxDb - kMinDb) * height;
}

float CompressionCurve::yToDb(float y, float y0, float height) noexcept
{
    if (height <= 0.0f)
        return kMinDb;
    return kMinDb + (y0 + height - y) / height * (kMaxDb - kMinDb);
}

juce::Rectangle<float> CompressionCurve::plotArea() const
{
    const auto bounds = getLocalBounds().toFloat();
    // Marges : gauche pour les labels Y, bas pour les labels X.
    return juce::Rectangle<float>(bounds.getX() + 38.0f, bounds.getY() + 10.0f,
                                  bounds.getWidth() - 48.0f, bounds.getHeight() - 34.0f);
}

void CompressionCurve::resized()
{
    juce::ignoreUnused(plotArea());
}

void CompressionCurve::timerCallback()
{
    // Meter Bridge : snapshot atomique, jamais bloquant pour l'audio.
    const auto meter = dsp_.getMeterState();
    lastEnvelopeDb_ = meter.envelopeDb;
    lastOutputDb_ = meter.outputDb;

    for (auto& point : history_)
        point.age += 1.0f;

    while (history_.size() >= kMaxHistory)
        history_.pop_front();

    if (lastEnvelopeDb_ > kMinDb + 0.5f && lastEnvelopeDb_ <= kMaxDb)
        history_.push_back({ lastEnvelopeDb_, lastOutputDb_, 0.0f });

    repaint();
}

void CompressionCurve::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const auto plot = plotArea();
    const auto params = dsp_.getParams();

    // Fond.
    g.setColour(ProC2Theme::curveBackground());
    g.fillRoundedRectangle(bounds.toFloat(), ProC2Theme::kCornerRadius);
    g.setColour(ProC2Theme::gridLine());
    g.drawRoundedRectangle(bounds.toFloat(), ProC2Theme::kCornerRadius, 1.0f);

    g.saveState();
    // reduceClipRegion() : présent en JUCE 7 et 8 (beginClipPath() a disparu en JUCE 8).
    g.reduceClipRegion(plot.toNearestInt());

    // Grille + labels tous les 12 dB (mineurs tous les 6 dB).
    g.setFont(ProC2Theme::getFont(9.0f));
    for (float db = kMinDb; db <= kMaxDb + 0.01f; db += 6.0f)
    {
        const bool major = std::fmod(db - kMinDb, 12.0f) < 0.01f;
        const float x = dbToX(db, plot.getX(), plot.getWidth());
        const float y = dbToY(db, plot.getY(), plot.getHeight());

        g.setColour(ProC2Theme::gridLine().withAlpha(major ? 0.9f : 0.45f));
        g.drawVerticalLine(static_cast<int>(x), static_cast<int>(plot.getY()),
                           static_cast<int>(plot.getBottom()));
        g.drawHorizontalLine(static_cast<int>(y), static_cast<int>(plot.getX()),
                             static_cast<int>(plot.getRight()));

        if (major)
        {
            g.setColour(ProC2Theme::textSecondary());
            g.drawText(juce::String(static_cast<int>(db)),
                       static_cast<int>(x) - 14, static_cast<int>(plot.getBottom()) + 3, 28, 12,
                       juce::Justification::centred);
            g.drawText(juce::String(static_cast<int>(db)),
                       static_cast<int>(plot.getX()) - 36, static_cast<int>(y) - 6, 32, 12,
                       juce::Justification::centredRight);
        }
    }

    // Bande du knee (autour du seuil).
    if (params.kneeDb > 0.0f && !params.bypass)
    {
        const float kneeLeft = dbToX(params.thresholdDb - params.kneeDb * 0.5f,
                                     plot.getX(), plot.getWidth());
        const float kneeRight = dbToX(params.thresholdDb + params.kneeDb * 0.5f,
                                      plot.getX(), plot.getWidth());
        g.setColour(ProC2Theme::accentBlue().withAlpha(0.06f));
        g.fillRect(kneeLeft, plot.getY(), kneeRight - kneeLeft, plot.getHeight());
    }

    // Courbe de transfert calculée par le Core (même formule que le DSP).
    juce::Path curvePath;
    juce::Path fillPath;
    juce::Path grPath;
    constexpr float kStepDb = 0.5f;
    bool first = true;
    for (float db = kMinDb; db <= kMaxDb + 0.01f; db += kStepDb)
    {
        const float outDb = Core::CompressorDSP::curveOutputDb(db, params);
        const float x = dbToX(db, plot.getX(), plot.getWidth());
        const float y = dbToY(outDb, plot.getY(), plot.getHeight());
        const float yDiag = dbToY(db, plot.getY(), plot.getHeight());

        if (first)
        {
            curvePath.startNewSubPath(x, y);
            fillPath.startNewSubPath(x, plot.getBottom());
            fillPath.lineTo(x, y);
            grPath.startNewSubPath(x, yDiag);
            grPath.lineTo(x, y);
            first = false;
        }
        else
        {
            curvePath.lineTo(x, y);
            fillPath.lineTo(x, y);
            grPath.lineTo(x, y);
        }
    }
    fillPath.lineTo(plot.getRight(), plot.getBottom());
    fillPath.closeSubPath();

    // Zone de réduction de gain : entre la diagonale et la courbe.
    if (!params.bypass)
    {
        juce::Path grFull = grPath;
        for (float db = kMaxDb; db >= kMinDb - 0.01f; db -= kStepDb)
        {
            const float x = dbToX(db, plot.getX(), plot.getWidth());
            const float yDiag = dbToY(db, plot.getY(), plot.getHeight());
            grFull.lineTo(x, yDiag);
        }
        grFull.closeSubPath();
        g.setColour(ProC2Theme::accentOrange().withAlpha(0.13f));
        g.fillPath(grFull);
    }

    // Remplissage sous la courbe.
    g.setColour(ProC2Theme::accentBlue().withAlpha(0.16f));
    g.fillPath(fillPath);

    // Diagonale 1:1.
    g.setColour(ProC2Theme::diagonalLine().withAlpha(0.55f));
    g.drawLine(plot.getX(), dbToY(kMinDb, plot.getY(), plot.getHeight()),
               plot.getRight(), dbToY(kMaxDb, plot.getY(), plot.getHeight()), 1.0f);

    // Courbe elle-même.
    g.setColour(ProC2Theme::curveLine());
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Ligne de seuil + marqueur sur la courbe.
    if (!params.bypass)
    {
        const float thresholdX = dbToX(params.thresholdDb, plot.getX(), plot.getWidth());
        const float thresholdOutDb = Core::CompressorDSP::curveOutputDb(params.thresholdDb, params);
        const float thresholdY = dbToY(thresholdOutDb, plot.getY(), plot.getHeight());

        g.setColour(ProC2Theme::textPrimary().withAlpha(0.35f));
        g.drawVerticalLine(static_cast<int>(thresholdX), static_cast<int>(plot.getY()),
                           static_cast<int>(plot.getBottom()));

        // Halo + point lumineux au seuil.
        g.setColour(ProC2Theme::accentBlue().withAlpha(0.25f));
        g.fillEllipse(thresholdX - 7.0f, thresholdY - 7.0f, 14.0f, 14.0f);
        g.setColour(ProC2Theme::textPrimary());
        g.fillEllipse(thresholdX - 3.0f, thresholdY - 3.0f, 6.0f, 6.0f);
    }

    // Historique du signal : points qui s'estompent.
    for (const auto& point : history_)
    {
        const float alpha = std::clamp(1.0f - point.age / static_cast<float>(kMaxHistory),
                                       0.0f, 1.0f);
        if (alpha <= 0.01f)
            continue;
        const float x = dbToX(point.inputDb, plot.getX(), plot.getWidth());
        const float y = dbToY(point.outputDb, plot.getY(), plot.getHeight());
        g.setColour(ProC2Theme::accentBlue().withAlpha(alpha * 0.55f));
        g.fillEllipse(x - 2.0f, y - 2.0f, 4.0f, 4.0f);
    }

    // Point "live" : position actuelle du signal.
    if (lastEnvelopeDb_ > kMinDb + 0.5f && lastEnvelopeDb_ <= kMaxDb)
    {
        const float x = dbToX(lastEnvelopeDb_, plot.getX(), plot.getWidth());
        const float y = dbToY(lastOutputDb_, plot.getY(), plot.getHeight());
        g.setColour(ProC2Theme::accentBlue().withAlpha(0.30f));
        g.fillEllipse(x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        g.setColour(ProC2Theme::accentBlue().withAlpha(0.65f));
        g.fillEllipse(x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(x - 2.0f, y - 2.0f, 4.0f, 4.0f);
    }

    g.restoreState();

    // Labels d'axes.
    g.setFont(ProC2Theme::getFont(10.0f));
    g.setColour(ProC2Theme::textSecondary());
    g.drawText("ENTREE (dB)", static_cast<int>(plot.getRight()) - 110,
               static_cast<int>(plot.getBottom()) + 10, 100, 14, juce::Justification::centredRight);
    g.drawText("SORTIE (dB)", static_cast<int>(plot.getX()) - 36,
               static_cast<int>(plot.getY()) - 2, 32, 12, juce::Justification::centredRight);

    if (params.bypass)
    {
        g.setFont(ProC2Theme::getFont(13.0f, true));
        g.setColour(ProC2Theme::accentOrange());
        g.drawText("BYPASS", bounds.getX(), bounds.getY() + 8, bounds.getWidth(), 18,
                   juce::Justification::centred);
    }
}

void CompressionCurve::mouseDown(const juce::MouseEvent& e)
{
    dragging_ = true;
    dragStartPos_ = e.getPosition().toFloat();
    const auto params = dsp_.getParams();
    dragStartThresholdDb_ = params.thresholdDb;
    dragStartRatio_ = params.ratio;
}

void CompressionCurve::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging_)
        return;

    const auto plot = plotArea();
    const float dx = e.getPosition().x - dragStartPos_.x;
    const float dy = e.getPosition().y - dragStartPos_.y;

    // Horizontal : seuil (1 px = plage dB / largeur du plot).
    float newThreshold = dragStartThresholdDb_;
    if (plot.getWidth() > 0.0f)
        newThreshold += dx * (kMaxDb - kMinDb) / plot.getWidth();

    // Vertical : ratio (exponentiel, plus naturel : glisser haut = plus de ratio).
    const float newRatio = dragStartRatio_ * std::exp(-dy * 0.01f);

    applyThresholdRatio(newThreshold, newRatio);
}

void CompressionCurve::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    dragging_ = false;
}

void CompressionCurve::applyThresholdRatio(float thresholdDb, float ratio)
{
    thresholdDb = Core::GainComputer::sanitizeThresholdDb(thresholdDb);
    ratio = Core::GainComputer::sanitizeRatio(ratio);

    if (apvts_ != nullptr)
    {
        // Conversion dénormalisé -> normalisé via la plage du paramètre :
        // évite de dépendre du type renvoyé par getParameter()
        // (AudioProcessorParameter* en JUCE 7, RangedAudioParameter* en JUCE 8).
        if (auto* thresholdParam = apvts_->getParameter("threshold"))
        {
            const auto range = apvts_->getParameterRange("threshold");
            thresholdParam->setValueNotifyingHost(range.convertTo0to1(thresholdDb));
        }
        if (auto* ratioParam = apvts_->getParameter("ratio"))
        {
            const auto range = apvts_->getParameterRange("ratio");
            ratioParam->setValueNotifyingHost(range.convertTo0to1(ratio));
        }
    }
    else
    {
        dsp_.setThresholdDb(thresholdDb);
        dsp_.setRatio(ratio);
    }
}

} // namespace AlphaComp::UI
