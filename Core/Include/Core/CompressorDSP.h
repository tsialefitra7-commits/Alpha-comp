#pragma once

// ============================================================================
// CompressorDSP.h — Processeur de compression temps réel (stéréo liée)
// ----------------------------------------------------------------------------
// 100 % C++ pur, zéro JUCE. C'est le cœur audio du plugin.
//
// Chaîne par échantillon (lien stéréo : un seul gain pour tous les canaux) :
//   1. Chaque canal passe dans son EnvelopeDetector (Peak/RMS + lookahead).
//      L'enveloppe liée = max des enveloppes des canaux.
//   2. GainComputer convertit cette enveloppe (dB) en réduction de gain cible.
//   3. Lissage du gain : attack si la GR devient plus négative,
//      release si elle remonte (évite la distorsion de modulation).
//   4. Gain appliqué au signal RETARDÉ (lookahead) + makeup gain.
//   5. Mix parallèle : out = dry*(1-mix) + wet*mix (dry = signal retardé,
//      pour rester aligné avec le wet quand le lookahead est actif).
//
// Meter Bridge (thread-safe) : le DSP écrit en permanence la réduction de
// gain, le niveau d'entrée/sortie dans des std::atomic<float>. L'UI lit
// un snapshot via getMeterState() à 60 fps sans lock ni allocation,
// donc sans jamais bloquer le thread audio.
//
// Paramètres : stockés en atomiques. setParams() peut être appelé depuis
// n'importe quel thread (UI) ; les valeurs sont relues et appliquées aux
// détecteurs au début de chaque bloc, sur le thread audio.
// ============================================================================

#include "Core/EnvelopeDetector.h"

#include <atomic>
#include <vector>

namespace AlphaComp::Core {

struct CompressorParams
{
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float kneeDb = 12.0f;
    float makeupDb = 0.0f;
    float mix = 1.0f; // 0..1 (1 = 100 % wet)
    DetectorMode detectorMode = DetectorMode::Peak;
    float lookaheadMs = 0.0f;
    bool bypass = false;
};

// Snapshot lu par l'UI (CompressionCurve + GainReductionMeter).
struct MeterState
{
    float gainReductionDb = 0.0f;  // <= 0, valeur lissée réellement appliquée
    float inputDb = -120.0f;       // enveloppe d'entrée liée (dB)
    float outputDb = -120.0f;      // enveloppe de sortie (chemin wet, dB)
    float envelopeDb = -120.0f;    // = inputDb (alias explicite pour la courbe)
    float makeupDb = 0.0f;
};

class CompressorDSP
{
public:
    static constexpr float kMinAttackMs = 0.01f;
    static constexpr float kMaxAttackMs = 300.0f;
    static constexpr float kMinReleaseMs = 10.0f;
    static constexpr float kMaxReleaseMs = 1000.0f;
    static constexpr float kMinMix = 0.0f;
    static constexpr float kMaxMix = 1.0f;
    static constexpr float kMinLookaheadMs = 0.0f;
    static constexpr float kMaxLookaheadMs = 20.0f;

    CompressorDSP();

    void prepare(double sampleRate, int maxChannels);
    void reset();

    // --- Paramètres (thread-safe, appelables depuis l'UI) ---
    void setParams(const CompressorParams& params);
    CompressorParams getParams() const;
    static CompressorParams sanitizeParams(CompressorParams params);

    void setThresholdDb(float v);
    void setRatio(float v);
    void setAttackTimeMs(float v);
    void setReleaseTimeMs(float v);
    void setKneeDb(float v);
    void setMakeupDb(float v);
    void setMix(float v);
    void setDetectorMode(DetectorMode mode);
    void setLookaheadTimeMs(float v);
    void setBypass(bool bypass);

    // --- Traitement ---
    // channelData[ch][sample], in-place. numChannels doit être <= maxChannels
    // passé à prepare() (les canaux supplémentaires éventuels sont ignorés).
    void processBlock(float* const* channelData, int numSamples, int numChannels);

    // --- Meter Bridge (thread-safe, sans allocation) ---
    MeterState getMeterState() const;
    float getGainReductionDb() const noexcept;

    // --- Courbe de transfert (utilisée par l'UI, identique au DSP) ---
    float getCurveOutputDb(float inputDb) const;
    static float curveOutputDb(float inputDb, const CompressorParams& params);

    double getSampleRate() const noexcept { return sampleRate_; }
    int getMaxChannels() const noexcept   { return maxChannels_; }

    // Latence introduite par le lookahead (échantillons). Le plugin doit
    // la déclarer via AudioProcessor::setLatencySamples() dans prepareToPlay().
    int getLatencySamples() const noexcept;

private:
    void applyParamsToDetectors(const CompressorParams& params);

    double sampleRate_ = 48000.0;
    int maxChannels_ = 2;
    std::vector<EnvelopeDetector> detectors_;
    std::vector<float> delayedBuffer_;
    float currentGainReductionDb_ = 0.0f;

    // Paramètres atomiques (écrits par l'UI, lus par l'audio).
    std::atomic<float> thresholdDb_{-18.0f};
    std::atomic<float> ratio_{4.0f};
    std::atomic<float> attackMs_{10.0f};
    std::atomic<float> releaseMs_{100.0f};
    std::atomic<float> kneeDb_{12.0f};
    std::atomic<float> makeupDb_{0.0f};
    std::atomic<float> mix_{1.0f};
    std::atomic<float> lookaheadMs_{0.0f};
    std::atomic<int> detectorMode_{0};
    std::atomic<bool> bypass_{false};

    // Meter bridge atomique (écrit par l'audio, lu par l'UI).
    mutable std::atomic<float> meterGainReductionDb_{0.0f};
    mutable std::atomic<float> meterInputDb_{-120.0f};
    mutable std::atomic<float> meterOutputDb_{-120.0f};
    mutable std::atomic<float> meterEnvelopeDb_{-120.0f};
};

} // namespace AlphaComp::Core
