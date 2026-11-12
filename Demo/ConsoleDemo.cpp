// ============================================================================
// ConsoleDemo.cpp — Démo console du Core (sans JUCE, sans DAW)
// ----------------------------------------------------------------------------
// Prouve que le DSP fonctionne seul : compresse un sinus stéréo, affiche
// le niveau avant/après, la réduction de gain finale et la courbe de
// transfert. Utile pour valider le Core sur une machine sans JUCE.
// ============================================================================

#include "Core/CompressorDSP.h"
#include "Core/GainComputer.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace AlphaComp::Core;

namespace
{
float rms(const std::vector<float>& channel)
{
    double sum = 0.0;
    for (float s : channel)
        sum += static_cast<double>(s) * s;
    return static_cast<float>(std::sqrt(sum / static_cast<double>(channel.size())));
}
} // namespace

int main()
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 48000; // 1 s

    CompressorDSP dsp;
    dsp.prepare(kSampleRate, 2);

    CompressorParams params;
    params.thresholdDb = -18.0f;
    params.ratio = 4.0f;
    params.attackMs = 10.0f;
    params.releaseMs = 100.0f;
    params.kneeDb = 12.0f;
    params.makeupDb = 3.0f;
    params.mix = 1.0f;
    params.detectorMode = DetectorMode::Peak;
    params.lookaheadMs = 0.0f;
    params.bypass = false;
    dsp.setParams(params);

    std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
    for (int i = 0; i < kSamples; ++i)
    {
        const float s = 0.9f * static_cast<float>(
            std::sin(2.0 * 3.141592653589793 * 440.0 * i / kSampleRate));
        buffer[0][static_cast<std::size_t>(i)] = s;
        buffer[1][static_cast<std::size_t>(i)] = s;
    }

    const float rmsIn = rms(buffer[0]);
    std::vector<float*> ptrs{ buffer[0].data(), buffer[1].data() };
    dsp.processBlock(ptrs.data(), kSamples, 2);
    const float rmsOut = rms(buffer[0]);
    const auto meter = dsp.getMeterState();

    std::printf("alpha-comp — démo console du Core\n");
    std::printf("  Paramètres : seuil %.1f dB, ratio %.1f:1, attack %.0f ms, "
                "release %.0f ms, knee %.0f dB, makeup %.1f dB\n",
                params.thresholdDb, params.ratio, params.attackMs,
                params.releaseMs, params.kneeDb, params.makeupDb);
    std::printf("  Sinus 440 Hz, amplitude 0.9, 1 s stéréo @ 48 kHz\n");
    std::printf("  RMS entrée : %.4f (%.1f dB)\n", rmsIn, GainComputer::linearToDb(rmsIn));
    std::printf("  RMS sortie : %.4f (%.1f dB)\n", rmsOut, GainComputer::linearToDb(rmsOut));
    std::printf("  Réduction de gain finale : %.2f dB\n", meter.gainReductionDb);
    std::printf("  Meter bridge : in %.1f dB -> out %.1f dB\n", meter.inputDb, meter.outputDb);

    std::printf("\n  Courbe de transfert (entrée -> sortie, makeup inclus) :\n");
    for (float inDb = -40.0f; inDb <= 0.0f; inDb += 5.0f)
        std::printf("    %6.1f dB -> %6.1f dB\n", inDb, dsp.getCurveOutputDb(inDb));

    return 0;
}
