#include "Core/GainComputer.h"

#include <algorithm>
#include <cmath>

namespace AlphaComp::Core {

float GainComputer::dbToLinear(float db) noexcept
{
    return std::pow(10.0f, db * 0.05f);
}

float GainComputer::linearToDb(float linear, float floorDb) noexcept
{
    if (linear <= 1.0e-12f)
        return floorDb;

    const float db = 20.0f * std::log10(linear);
    return db < floorDb ? floorDb : db;
}

float GainComputer::computeGainReductionDb(float inputDb,
                                           float thresholdDb,
                                           float ratio,
                                           float kneeDb) noexcept
{
    const float r = std::max(ratio, 1.0f);
    if (r <= 1.0f)
        return 0.0f; // ratio 1:1 = pas de compression

    const float knee = std::max(kneeDb, 0.0f);
    const float over = inputDb - thresholdDb;

    if (knee <= 1.0e-6f)
    {
        // Knee dur (hard knee).
        if (over <= 0.0f)
            return 0.0f;
        return over * (1.0f / r - 1.0f);
    }

    const float halfKnee = knee * 0.5f;

    if (over < -halfKnee)
        return 0.0f;

    if (over > halfKnee)
        return over * (1.0f / r - 1.0f);

    // Zone du knee : parabole continue (valeur + dérivée) avec les deux zones.
    const float t = over + halfKnee; // 0 .. knee
    return (1.0f / r - 1.0f) * (t * t) / (2.0f * knee);
}

float GainComputer::computeOutputDb(float inputDb,
                                    float thresholdDb,
                                    float ratio,
                                    float kneeDb) noexcept
{
    return inputDb + computeGainReductionDb(inputDb, thresholdDb, ratio, kneeDb);
}

float GainComputer::computeOutputDb(float inputDb,
                                    float thresholdDb,
                                    float ratio,
                                    float kneeDb,
                                    float makeupDb) noexcept
{
    return computeOutputDb(inputDb, thresholdDb, ratio, kneeDb) + makeupDb;
}

float GainComputer::sanitizeThresholdDb(float v) noexcept
{
    return std::clamp(v, kMinThresholdDb, kMaxThresholdDb);
}

float GainComputer::sanitizeRatio(float v) noexcept
{
    return std::clamp(v, kMinRatio, kMaxRatio);
}

float GainComputer::sanitizeKneeDb(float v) noexcept
{
    return std::clamp(v, kMinKneeDb, kMaxKneeDb);
}

float GainComputer::sanitizeMakeupDb(float v) noexcept
{
    return std::clamp(v, kMinMakeupDb, kMaxMakeupDb);
}

} // namespace AlphaComp::Core
