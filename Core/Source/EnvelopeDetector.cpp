#include "Core/EnvelopeDetector.h"

#include "Core/GainComputer.h"

#include <algorithm>
#include <cmath>

namespace AlphaComp::Core {

namespace
{
// Coefficient one-pole : y = a*y + (1-a)*x, a = exp(-1/(sr*tau)).
float onePoleCoeff(double sampleRate, float timeMs) noexcept
{
    if (timeMs <= 0.0f || sampleRate <= 0.0)
        return 0.0f;

    const double tau = static_cast<double>(timeMs) * 0.001;
    const double a = std::exp(-1.0 / (sampleRate * tau));
    return static_cast<float>(std::clamp(a, 0.0, 0.999999));
}
} // namespace

EnvelopeDetector::EnvelopeDetector()
{
    updateCoefficients();
}

void EnvelopeDetector::prepare(double sampleRate)
{
    if (sampleRate > 0.0)
        sampleRate_ = sampleRate;

    updateCoefficients();
    updateDelayLine();
    reset();
}

void EnvelopeDetector::reset()
{
    envelope_ = 0.0f;
    smoothedSquare_ = 0.0f;
    std::fill(delayLine_.begin(), delayLine_.end(), 0.0f);
    delayWritePos_ = 0;
}

void EnvelopeDetector::setMode(DetectorMode mode) noexcept
{
    mode_ = mode;
}

void EnvelopeDetector::setAttackTimeMs(float ms)
{
    attackMs_ = std::clamp(ms, 0.01f, 1000.0f);
    updateCoefficients();
}

void EnvelopeDetector::setReleaseTimeMs(float ms)
{
    releaseMs_ = std::clamp(ms, 0.1f, 2000.0f);
    updateCoefficients();
}

void EnvelopeDetector::setLookaheadTimeMs(float ms)
{
    lookaheadMs_ = std::clamp(ms, 0.0f, 20.0f);
    updateDelayLine();
}

void EnvelopeDetector::updateCoefficients() noexcept
{
    attackCoeff_ = onePoleCoeff(sampleRate_, attackMs_);
    releaseCoeff_ = onePoleCoeff(sampleRate_, releaseMs_);
    // Fenêtre d'intégration RMS fixe (~10 ms), indépendante de l'attack.
    rmsCoeff_ = onePoleCoeff(sampleRate_, 10.0f);
}

void EnvelopeDetector::updateDelayLine()
{
    const std::size_t len = static_cast<std::size_t>(
        std::llround(static_cast<double>(lookaheadMs_) * 0.001 * sampleRate_));

    if (len != delayLength_)
    {
        delayLine_.assign(len, 0.0f);
        delayLength_ = len;
        delayWritePos_ = 0;
    }
}

float EnvelopeDetector::processSample(float input, float& delayedSample) noexcept
{
    // --- Lookahead : on retarde l'audio, pas la détection. ---
    if (delayLength_ == 0)
    {
        delayedSample = input;
    }
    else
    {
        delayedSample = delayLine_[delayWritePos_];
        delayLine_[delayWritePos_] = input;
        delayWritePos_ = (delayWritePos_ + 1) % delayLength_;
    }

    // --- Cible du détecteur. ---
    float target = 0.0f;
    if (mode_ == DetectorMode::RMS)
    {
        smoothedSquare_ += (1.0f - rmsCoeff_) * (input * input - smoothedSquare_);
        target = std::sqrt(std::max(smoothedSquare_, 0.0f));
    }
    else
    {
        target = std::abs(input);
    }

    // --- Follower attack/release. ---
    const float coeff = (target > envelope_) ? attackCoeff_ : releaseCoeff_;
    envelope_ = coeff * envelope_ + (1.0f - coeff) * target;

    if (envelope_ < 1.0e-12f)
        envelope_ = 0.0f;

    return envelope_;
}

float EnvelopeDetector::getEnvelopeDb() const noexcept
{
    return GainComputer::linearToDb(envelope_);
}

} // namespace AlphaComp::Core
