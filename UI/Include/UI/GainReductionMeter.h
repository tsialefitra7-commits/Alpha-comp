#pragma once

// ============================================================================
// GainReductionMeter.h — Barre verticale de réduction de gain (style LED)
// ----------------------------------------------------------------------------
// Plage 0 .. -24 dB, segments LED vert -> orange -> rouge, lissage interne
// (ballistique) pour un mouvement fluide à 60 fps, marqueur de pic qui
// redescend lentement, et lecture numérique en dB.
// L'éditeur appelle setGainReductionDb() depuis son Timer (60 fps) avec
// la valeur lue dans le Meter Bridge du DSP.
// ============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

namespace AlphaComp::UI {

class GainReductionMeter : public juce::Component
{
public:
    static constexpr float kMaxGrDb = 24.0f;

    GainReductionMeter();

    void setGainReductionDb(float grDb); // <= 0
    void setBypassed(bool bypass);

    float getGainReductionDb() const noexcept { return displayedGrDb_; }

    void paint(juce::Graphics& g) override;

private:
    float targetGrDb_ = 0.0f;
    float displayedGrDb_ = 0.0f;
    float peakGrDb_ = 0.0f;
    bool bypassed_ = false;
};

} // namespace AlphaComp::UI
