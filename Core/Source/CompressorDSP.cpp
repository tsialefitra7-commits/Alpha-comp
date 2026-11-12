#include "Core/CompressorDSP.h"

#include "Core/GainComputer.h"

#include <algorithm>
#include <cmath>

namespace AlphaComp::Core {

namespace
{
float onePoleCoeff(double sampleRate, float timeMs) noexcept
{
    if (timeMs <= 0.0f || sampleRate <= 0.0)
        return 0.0f;

    const double tau = static_cast<double>(timeMs) * 0.001;
    const double a = std::exp(-1.0 / (sampleRate * tau));
    return static_cast<float>(std::clamp(a, 0.0, 0.999999));
}
} // namespace

CompressorDSP::CompressorDSP()
{
    setParams(CompressorParams{});
}

void CompressorDSP::prepare(double sampleRate, int maxChannels)
{
    if (sampleRate > 0.0)
        sampleRate_ = sampleRate;

    maxChannels_ = std::max(1, maxChannels);
    detectors_.assign(static_cast<std::size_t>(maxChannels_), EnvelopeDetector{});
    delayedBuffer_.assign(static_cast<std::size_t>(maxChannels_), 0.0f);

    for (auto& detector : detectors_)
        detector.prepare(sampleRate_);

    applyParamsToDetectors(getParams());
    reset();
}

void CompressorDSP::reset()
{
    for (auto& detector : detectors_)
        detector.reset();

    currentGainReductionDb_ = 0.0f;
    meterGainReductionDb_.store(0.0f);
    meterInputDb_.store(-120.0f);
    meterOutputDb_.store(-120.0f);
    meterEnvelopeDb_.store(-120.0f);
}

CompressorParams CompressorDSP::sanitizeParams(CompressorParams params)
{
    params.thresholdDb = GainComputer::sanitizeThresholdDb(params.thresholdDb);
    params.ratio = GainComputer::sanitizeRatio(params.ratio);
    params.attackMs = std::clamp(params.attackMs, kMinAttackMs, kMaxAttackMs);
    params.releaseMs = std::clamp(params.releaseMs, kMinReleaseMs, kMaxReleaseMs);
    params.kneeDb = GainComputer::sanitizeKneeDb(params.kneeDb);
    params.makeupDb = GainComputer::sanitizeMakeupDb(params.makeupDb);
    params.mix = std::clamp(params.mix, kMinMix, kMaxMix);
    params.lookaheadMs = std::clamp(params.lookaheadMs, kMinLookaheadMs, kMaxLookaheadMs);
    return params;
}

void CompressorDSP::setParams(const CompressorParams& params)
{
    const auto p = sanitizeParams(params);
    thresholdDb_.store(p.thresholdDb);
    ratio_.store(p.ratio);
    attackMs_.store(p.attackMs);
    releaseMs_.store(p.releaseMs);
    kneeDb_.store(p.kneeDb);
    makeupDb_.store(p.makeupDb);
    mix_.store(p.mix);
    lookaheadMs_.store(p.lookaheadMs);
    detectorMode_.store(static_cast<int>(p.detectorMode));
    bypass_.store(p.bypass);
}

CompressorParams CompressorDSP::getParams() const
{
    CompressorParams p;
    p.thresholdDb = thresholdDb_.load();
    p.ratio = ratio_.load();
    p.attackMs = attackMs_.load();
    p.releaseMs = releaseMs_.load();
    p.kneeDb = kneeDb_.load();
    p.makeupDb = makeupDb_.load();
    p.mix = mix_.load();
    p.lookaheadMs = lookaheadMs_.load();
    p.detectorMode = static_cast<DetectorMode>(detectorMode_.load());
    p.bypass = bypass_.load();
    return p;
}

void CompressorDSP::setThresholdDb(float v)     { auto p = getParams(); p.thresholdDb = v; setParams(p); }
void CompressorDSP::setRatio(float v)           { auto p = getParams(); p.ratio = v; setParams(p); }
void CompressorDSP::setAttackTimeMs(float v)    { auto p = getParams(); p.attackMs = v; setParams(p); }
void CompressorDSP::setReleaseTimeMs(float v)   { auto p = getParams(); p.releaseMs = v; setParams(p); }
void CompressorDSP::setKneeDb(float v)          { auto p = getParams(); p.kneeDb = v; setParams(p); }
void CompressorDSP::setMakeupDb(float v)        { auto p = getParams(); p.makeupDb = v; setParams(p); }
void CompressorDSP::setMix(float v)             { auto p = getParams(); p.mix = v; setParams(p); }
void CompressorDSP::setDetectorMode(DetectorMode m) { auto p = getParams(); p.detectorMode = m; setParams(p); }
void CompressorDSP::setLookaheadTimeMs(float v) { auto p = getParams(); p.lookaheadMs = v; setParams(p); }
void CompressorDSP::setBypass(bool bypass)      { auto p = getParams(); p.bypass = bypass; setParams(p); }

void CompressorDSP::applyParamsToDetectors(const CompressorParams& params)
{
    for (auto& detector : detectors_)
    {
        detector.setMode(params.detectorMode);
        detector.setAttackTimeMs(params.attackMs);
        detector.setReleaseTimeMs(params.releaseMs);
        detector.setLookaheadTimeMs(params.lookaheadMs);
    }
}

void CompressorDSP::processBlock(float* const* channelData, int numSamples, int numChannels)
{
    if (channelData == nullptr || numSamples <= 0 || numChannels <= 0 || detectors_.empty())
        return;

    const int chCount = std::min(numChannels, static_cast<int>(detectors_.size()));

    // Paramètres relus à chaque bloc sur le thread audio (pont UI -> audio).
    const auto params = getParams();
    applyParamsToDetectors(params);

    const float attackCoeff = onePoleCoeff(sampleRate_, params.attackMs);
    const float releaseCoeff = onePoleCoeff(sampleRate_, params.releaseMs);
    const float makeupLinear = GainComputer::dbToLinear(params.makeupDb);
    const float mix = params.bypass ? 0.0f : params.mix;
    const float wetGainBase = params.bypass ? 1.0f : makeupLinear;

    float lastInputDb = -120.0f;
    float lastOutputDb = -120.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Détection par canal + lien stéréo (max).
        float maxEnvelope = 0.0f;
        for (int ch = 0; ch < chCount; ++ch)
        {
            float delayed = 0.0f;
            const float env = detectors_[static_cast<std::size_t>(ch)]
                                  .processSample(channelData[ch][i], delayed);
            delayedBuffer_[static_cast<std::size_t>(ch)] = delayed;
            if (env > maxEnvelope)
                maxEnvelope = env;
        }

        // 2. Réduction de gain cible depuis l'enveloppe liée.
        const float inputDb = GainComputer::linearToDb(maxEnvelope);
        const float targetGr = params.bypass
            ? 0.0f
            : GainComputer::computeGainReductionDb(inputDb, params.thresholdDb,
                                                   params.ratio, params.kneeDb);

        // 3. Lissage attack/release du gain.
        const float coeff = (targetGr < currentGainReductionDb_) ? attackCoeff : releaseCoeff;
        currentGainReductionDb_ = coeff * currentGainReductionDb_ + (1.0f - coeff) * targetGr;

        // 4+5. Application au signal retardé + makeup + mix parallèle.
        const float wetGain = wetGainBase * GainComputer::dbToLinear(currentGainReductionDb_);
        for (int ch = 0; ch < chCount; ++ch)
        {
            const float dry = delayedBuffer_[static_cast<std::size_t>(ch)];
            const float wet = dry * wetGain;
            channelData[ch][i] = dry * (1.0f - mix) + wet * mix;
        }

        lastInputDb = inputDb;
        lastOutputDb = inputDb + currentGainReductionDb_
                     + (params.bypass ? 0.0f : params.makeupDb);
    }

    // Meter bridge : écriture atomique unique en fin de bloc.
    meterGainReductionDb_.store(currentGainReductionDb_);
    meterInputDb_.store(lastInputDb);
    meterEnvelopeDb_.store(lastInputDb);
    meterOutputDb_.store(lastOutputDb);
}

MeterState CompressorDSP::getMeterState() const
{
    MeterState state;
    state.gainReductionDb = meterGainReductionDb_.load();
    state.inputDb = meterInputDb_.load();
    state.outputDb = meterOutputDb_.load();
    state.envelopeDb = meterEnvelopeDb_.load();
    state.makeupDb = makeupDb_.load();
    return state;
}

float CompressorDSP::getGainReductionDb() const noexcept
{
    return meterGainReductionDb_.load();
}

float CompressorDSP::curveOutputDb(float inputDb, const CompressorParams& params)
{
    if (params.bypass)
        return inputDb;

    return GainComputer::computeOutputDb(inputDb, params.thresholdDb, params.ratio,
                                         params.kneeDb, params.makeupDb);
}

float CompressorDSP::getCurveOutputDb(float inputDb) const
{
    return curveOutputDb(inputDb, getParams());
}

int CompressorDSP::getLatencySamples() const noexcept
{
    const float ms = lookaheadMs_.load();
    if (ms <= 0.0f || sampleRate_ <= 0.0)
        return 0;

    return static_cast<int>(std::llround(static_cast<double>(ms) * 0.001 * sampleRate_));
}

} // namespace AlphaComp::Core
