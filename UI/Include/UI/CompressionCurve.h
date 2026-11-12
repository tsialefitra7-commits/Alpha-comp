#pragma once

// ============================================================================
// CompressionCurve.h — La signature visuelle du Pro-C 2
// ----------------------------------------------------------------------------
// Graphique 2D temps réel :
//   Axe X : niveau d'entrée  (dB)   Axe Y : niveau de sortie (dB)
// Dessine :
//   - la diagonale 1:1 (aucune compression),
//   - la courbe de transfert calculée par le Core (GainComputer + makeup),
//     donc EXACTEMENT celle appliquée par le DSP,
//   - la zone de réduction de gain (entre diagonale et courbe, orange),
//   - le remplissage sous la courbe (bleu, dégradé d'alpha),
//   - la ligne de seuil + la bande du knee,
//   - l'historique du signal (points entrée->sortie qui s'estompent) et
//     le point "live" lumineux : où se situe le signal par rapport au seuil.
//
// Interaction (comme le Pro-C 2) : glisser la courbe horizontalement
// change le Threshold, verticalement change le Ratio. Les changements
// passent par l'APVTS (setValueNotifyingHost) pour rester automatisables,
// ou directement par le DSP si aucun APVTS n'est fourni.
//
// Animation : Timer à ~60 fps qui lit le Meter Bridge du DSP
// (getMeterState(), atomique, sans lock) et repeint.
// ============================================================================

#include "Core/CompressorDSP.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <deque>

namespace juce
{
class AudioProcessorValueTreeState;
}

namespace AlphaComp::UI {

class CompressionCurve : public juce::Component, private juce::Timer
{
public:
    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb = 20.0f;

    // apvts peut être nullptr (aperçu sans automatisation) : le drag
    // écrit alors directement dans le DSP.
    // Les IDs de paramètres attendus dans l'APVTS : "threshold", "ratio".
    CompressionCurve(Core::CompressorDSP& dsp,
                     juce::AudioProcessorValueTreeState* apvts = nullptr);
    ~CompressionCurve() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    static float dbToX(float db, float x0, float width) noexcept;
    static float xToDb(float x, float x0, float width) noexcept;
    static float dbToY(float db, float y0, float height) noexcept;
    static float yToDb(float y, float y0, float height) noexcept;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    void applyThresholdRatio(float thresholdDb, float ratio);

    Core::CompressorDSP& dsp_;
    juce::AudioProcessorValueTreeState* apvts_;

    struct HistoryPoint
    {
        float inputDb;
        float outputDb;
        float age; // en ticks (~60/s), 0 = tout juste
    };
    std::deque<HistoryPoint> history_;
    static constexpr std::size_t kMaxHistory = 600;

    bool dragging_ = false;
    float dragStartThresholdDb_ = -18.0f;
    float dragStartRatio_ = 4.0f;
    juce::Point<float> dragStartPos_;

    float lastEnvelopeDb_ = -120.0f;
    float lastOutputDb_ = -120.0f;
};

} // namespace AlphaComp::UI
