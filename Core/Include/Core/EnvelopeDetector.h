#pragma once

// ============================================================================
// EnvelopeDetector.h — Détection du niveau (Peak / RMS / Lookahead)
// ----------------------------------------------------------------------------
// 100 % C++ pur, zéro JUCE. Un détecteur par canal (le DSP fait le lien
// stéréo en prenant le max des enveloppes).
//
//  - Peak : l'enveloppe suit |x| avec un one-pole dont le coefficient
//           dépend du sens : attack si le signal monte, release s'il descend.
//  - RMS  : le carré du signal est d'abord intégré (fenêtre ~10 ms),
//           puis sqrt() donne une valeur RMS qui suit ensuite le même
//           follower attack/release. Le RMS est donc plus bas que le Peak
//           sur un sinus (facteur 1/sqrt(2)).
//  - Lookahead : ligne à retard (ring buffer). processSample() renvoie
//           l'enveloppe calculée sur le signal NON retardé et écrit dans
//           delayedSample le signal retardé de `lookaheadMs`. Le DSP
//           applique le gain au signal retardé : le transitoire n'est
//           jamais manqué. La latence introduite = getLookaheadSamples().
//
// Coefficients one-pole : a = exp(-1 / (sampleRate * tau)), tau = temps en s.
// L'enveloppe atteint ~63 % de sa cible en `tau` (définition standard).
//
// Threading : les setters recalculent les coefficients (et peuvent
// redimensionner le ring buffer pour le lookahead). Ils doivent être
// appelés depuis le même thread que processSample(), ou avant le
// traitement audio. CompressorDSP gère le pont UI -> audio via des
// atomiques et applique les paramètres au début de chaque bloc, sur le
// thread audio : c'est donc sans risque dans le plugin.
// ============================================================================

#include <cstddef>
#include <vector>

namespace AlphaComp::Core {

enum class DetectorMode
{
    Peak = 0,
    RMS  = 1
};

class EnvelopeDetector
{
public:
    EnvelopeDetector();

    void prepare(double sampleRate);
    void reset();

    void setMode(DetectorMode mode) noexcept;
    void setAttackTimeMs(float ms);
    void setReleaseTimeMs(float ms);
    void setLookaheadTimeMs(float ms);

    DetectorMode getMode() const noexcept         { return mode_; }
    float getAttackTimeMs() const noexcept        { return attackMs_; }
    float getReleaseTimeMs() const noexcept       { return releaseMs_; }
    float getLookaheadTimeMs() const noexcept     { return lookaheadMs_; }
    double getSampleRate() const noexcept         { return sampleRate_; }
    int getLookaheadSamples() const noexcept
    {
        return static_cast<int>(delayLength_);
    }

    // Traite un échantillon. Retourne l'enveloppe linéaire (>= 0).
    // delayedSample reçoit l'échantillon retardé (lookahead).
    float processSample(float input, float& delayedSample) noexcept;

    float getEnvelope() const noexcept { return envelope_; }
    float getEnvelopeDb() const noexcept;

private:
    void updateCoefficients() noexcept;
    void updateDelayLine();

    double sampleRate_ = 48000.0;
    DetectorMode mode_ = DetectorMode::Peak;
    float attackMs_ = 10.0f;
    float releaseMs_ = 100.0f;
    float lookaheadMs_ = 0.0f;

    float attackCoeff_ = 0.0f;
    float releaseCoeff_ = 0.0f;
    float rmsCoeff_ = 0.0f;

    float envelope_ = 0.0f;
    float smoothedSquare_ = 0.0f;

    std::vector<float> delayLine_;
    std::size_t delayWritePos_ = 0;
    std::size_t delayLength_ = 0;
};

} // namespace AlphaComp::Core
