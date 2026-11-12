#pragma once

// ============================================================================
// GainComputer.h — Courbe de transfert statique du compresseur
// ----------------------------------------------------------------------------
// 100 % C++ pur, zéro JUCE. Ce module ne fait que des mathématiques :
//   entrée (dB) -> réduction de gain (dB, <= 0) -> sortie (dB).
//
// Formule (knee doux, type McNally / RBJ) :
//   over = inputDb - thresholdDb
//   si over < -knee/2        : GR = 0
//   si over > +knee/2        : GR = over * (1/ratio - 1)          (knee dur)
//   sinon (zone du knee)     : GR = (1/ratio - 1) * (over + knee/2)^2 / (2*knee)
//
// La formule est continue en valeur et en dérivée aux bords du knee.
// L'UI (CompressionCurve) utilise exactement ces fonctions pour dessiner
// la courbe : ce que l'on voit est ce que le DSP fait.
// ============================================================================

namespace AlphaComp::Core {

class GainComputer
{
public:
    // Plages valides (utilisées aussi par le DSP et l'APVTS).
    static constexpr float kMinThresholdDb = -60.0f;
    static constexpr float kMaxThresholdDb =  20.0f;
    static constexpr float kMinRatio       =   1.0f;
    static constexpr float kMaxRatio       =  20.0f;
    static constexpr float kMinKneeDb      =   0.0f;
    static constexpr float kMaxKneeDb      =  40.0f;
    static constexpr float kMinMakeupDb    = -24.0f;
    static constexpr float kMaxMakeupDb    =  24.0f;

    static float dbToLinear(float db) noexcept;
    static float linearToDb(float linear, float floorDb = -120.0f) noexcept;

    // Réduction de gain en dB (toujours <= 0). 0 = aucune compression.
    static float computeGainReductionDb(float inputDb,
                                        float thresholdDb,
                                        float ratio,
                                        float kneeDb) noexcept;

    // Sortie en dB = entrée + GR (+ makeup optionnel).
    static float computeOutputDb(float inputDb,
                                 float thresholdDb,
                                 float ratio,
                                 float kneeDb) noexcept;

    static float computeOutputDb(float inputDb,
                                 float thresholdDb,
                                 float ratio,
                                 float kneeDb,
                                 float makeupDb) noexcept;

    // Helpers de validation (clamp) partagés par le DSP.
    static float sanitizeThresholdDb(float v) noexcept;
    static float sanitizeRatio(float v) noexcept;
    static float sanitizeKneeDb(float v) noexcept;
    static float sanitizeMakeupDb(float v) noexcept;
};

} // namespace AlphaComp::Core
