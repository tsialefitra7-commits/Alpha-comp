// ============================================================================
// CoreTests.cpp — Tests du Core DSP (sans JUCE, sans framework externe)
// ----------------------------------------------------------------------------
// Compilation : voir Tests/CMakeLists.txt ou `make test` à la racine.
// Code de sortie 0 = tous les tests passent, 1 = au moins un échec.
// ============================================================================

#include "Core/CompressorDSP.h"
#include "Core/EnvelopeDetector.h"
#include "Core/GainComputer.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int g_failures = 0;
int g_checks = 0;

void check(bool condition, const char* message)
{
    ++g_checks;
    if (!condition)
    {
        ++g_failures;
        std::printf("  FAIL: %s\n", message);
    }
}

bool near(float a, float b, float tolerance)
{
    return std::abs(a - b) <= tolerance;
}

using namespace AlphaComp::Core;

// Génère un sinus stéréo dans un buffer [ch][sample].
void fillSine(std::vector<std::vector<float>>& buffer, float amplitude, float freqHz, double sampleRate)
{
    const int numSamples = static_cast<int>(buffer[0].size());
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = amplitude * static_cast<float>(std::sin(2.0 * 3.141592653589793 * freqHz * i / sampleRate));
        for (auto& channel : buffer)
            channel[static_cast<std::size_t>(i)] = s;
    }
}

float rms(const std::vector<float>& channel)
{
    double sum = 0.0;
    for (float s : channel)
        sum += static_cast<double>(s) * s;
    return static_cast<float>(std::sqrt(sum / static_cast<double>(channel.size())));
}

std::vector<float*> pointersOf(std::vector<std::vector<float>>& buffer)
{
    std::vector<float*> ptrs;
    ptrs.reserve(buffer.size());
    for (auto& channel : buffer)
        ptrs.push_back(channel.data());
    return ptrs;
}

void testGainComputer()
{
    std::printf("[GainComputer]\n");

    check(near(GainComputer::dbToLinear(0.0f), 1.0f, 1e-5f), "dbToLinear(0) == 1");
    check(near(GainComputer::linearToDb(1.0f), 0.0f, 1e-4f), "linearToDb(1) == 0");
    check(near(GainComputer::linearToDb(GainComputer::dbToLinear(-12.0f)), -12.0f, 1e-3f),
          "aller-retour dB <-> linéaire");
    check(near(GainComputer::linearToDb(0.0f), -120.0f, 1e-6f), "linearToDb(0) == floor");

    // Sous le seuil : aucune réduction.
    check(near(GainComputer::computeGainReductionDb(-30.0f, -18.0f, 4.0f, 0.0f), 0.0f, 1e-6f),
          "sous le seuil (knee dur) : GR == 0");
    check(near(GainComputer::computeOutputDb(-30.0f, -18.0f, 4.0f, 0.0f), -30.0f, 1e-6f),
          "sous le seuil : sortie == entrée");

    // Au-dessus du seuil, knee dur : GR = over * (1/ratio - 1).
    // over = 12 dB, ratio 4 -> GR = -9 dB, sortie = -15 dB.
    check(near(GainComputer::computeGainReductionDb(-6.0f, -18.0f, 4.0f, 0.0f), -9.0f, 1e-4f),
          "au-dessus du seuil : GR == -9 dB");
    check(near(GainComputer::computeOutputDb(-6.0f, -18.0f, 4.0f, 0.0f), -15.0f, 1e-4f),
          "au-dessus du seuil : sortie == -15 dB");

    // Ratio 1:1 : jamais de compression.
    check(near(GainComputer::computeGainReductionDb(0.0f, -60.0f, 1.0f, 0.0f), 0.0f, 1e-6f),
          "ratio 1:1 : GR == 0");

    // Makeup gain.
    check(near(GainComputer::computeOutputDb(-6.0f, -18.0f, 4.0f, 0.0f, 6.0f), -9.0f, 1e-4f),
          "makeup +6 dB décale la sortie de +6 dB");

    // Knee doux : continuité aux bords et monotonie.
    const float threshold = -18.0f, ratio = 4.0f, knee = 20.0f;
    check(near(GainComputer::computeGainReductionDb(threshold - knee * 0.5f, threshold, ratio, knee),
               0.0f, 1e-5f),
          "knee : GR == 0 au bord inférieur");
    const float hardAtTop = (knee * 0.5f) * (1.0f / ratio - 1.0f);
    check(near(GainComputer::computeGainReductionDb(threshold + knee * 0.5f, threshold, ratio, knee),
               hardAtTop, 1e-4f),
          "knee : continuité avec la zone dure au bord supérieur");

    float prevOut = -1.0e9f;
    bool monotonic = true;
    for (float in = -60.0f; in <= 20.0f; in += 1.0f)
    {
        const float out = GainComputer::computeOutputDb(in, threshold, ratio, knee);
        if (out < prevOut - 1e-4f)
            monotonic = false;
        prevOut = out;
    }
    check(monotonic, "knee doux : la courbe de sortie est monotone croissante");

    // Le knee doux commence à réduire AVANT le seuil (le knee dur ne réduit pas).
    const float grSoftBelow = GainComputer::computeGainReductionDb(-20.0f, threshold, ratio, knee);
    const float grHardBelow = GainComputer::computeGainReductionDb(-20.0f, threshold, ratio, 0.0f);
    check(grSoftBelow < 0.0f && grHardBelow == 0.0f,
          "knee doux : réduction avant le seuil, knee dur : aucune");
    // Au-dessus de threshold + knee/2, knee doux et knee dur coïncident.
    check(near(GainComputer::computeGainReductionDb(-5.0f, threshold, ratio, knee),
               GainComputer::computeGainReductionDb(-5.0f, threshold, ratio, 0.0f), 1e-5f),
          "knee doux : identique au knee dur au-dessus de threshold + knee/2");
}

void testEnvelopeDetector()
{
    std::printf("[EnvelopeDetector]\n");
    constexpr double kSampleRate = 48000.0;

    // Peak : une entrée constante finit par être suivie.
    {
        EnvelopeDetector detector;
        detector.prepare(kSampleRate);
        detector.setMode(DetectorMode::Peak);
        detector.setAttackTimeMs(1.0f);
        detector.setReleaseTimeMs(100.0f);
        float delayed = 0.0f;
        float env = 0.0f;
        for (int i = 0; i < 4800; ++i)
            env = detector.processSample(0.5f, delayed);
        check(near(env, 0.5f, 0.01f), "Peak : l'enveloppe converge vers l'entrée constante");
        check(near(detector.getEnvelopeDb(), GainComputer::linearToDb(0.5f), 0.2f),
              "Peak : getEnvelopeDb() cohérent");
    }

    // Attack lente vs rapide : après 1 ms, l'attack rapide est plus haute.
    {
        EnvelopeDetector fast, slow;
        fast.prepare(kSampleRate);
        slow.prepare(kSampleRate);
        fast.setAttackTimeMs(0.1f);
        slow.setAttackTimeMs(50.0f);
        float d1 = 0.0f, d2 = 0.0f, envFast = 0.0f, envSlow = 0.0f;
        for (int i = 0; i < 48; ++i) // 1 ms à 48 kHz
        {
            envFast = fast.processSample(1.0f, d1);
            envSlow = slow.processSample(1.0f, d2);
        }
        check(envFast > envSlow, "Attack : une attack rapide monte plus vite qu'une attack lente");
    }

    // RMS : sur un sinus d'amplitude 0.8, RMS ~= 0.8/sqrt(2) ~= 0.566 < Peak.
    {
        EnvelopeDetector detector;
        detector.prepare(kSampleRate);
        detector.setMode(DetectorMode::RMS);
        detector.setAttackTimeMs(1.0f);
        detector.setReleaseTimeMs(100.0f);
        float delayed = 0.0f;
        float env = 0.0f;
        for (int i = 0; i < 48000; ++i)
        {
            const float s = 0.8f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 100.0 * i / kSampleRate));
            env = detector.processSample(s, delayed);
        }
        check(env > 0.40f && env < 0.70f, "RMS : enveloppe proche de amplitude/sqrt(2)");
        check(env < 0.8f, "RMS : inférieur au Peak (0.8)");
    }

    // Lookahead : une impulsion ressort retardée de exactement N échantillons.
    {
        EnvelopeDetector detector;
        detector.prepare(kSampleRate);
        detector.setLookaheadTimeMs(10.0f); // 480 échantillons à 48 kHz
        check(detector.getLookaheadSamples() == 480, "Lookahead : 10 ms à 48 kHz == 480 échantillons");

        bool earlyLeak = false;
        bool arrivedAt480 = false;
        for (int i = 0; i < 1000; ++i)
        {
            float delayed = 0.0f;
            detector.processSample(i == 0 ? 1.0f : 0.0f, delayed);
            if (i < 480 && delayed != 0.0f)
                earlyLeak = true;
            if (i == 480 && near(delayed, 1.0f, 1e-6f))
                arrivedAt480 = true;
        }
        check(!earlyLeak, "Lookahead : rien ne sort avant le délai");
        check(arrivedAt480, "Lookahead : l'impulsion sort à l'échantillon 480");
    }

    // Silence : enveloppe nulle, dB au plancher.
    {
        EnvelopeDetector detector;
        detector.prepare(kSampleRate);
        float delayed = 0.0f;
        float env = 0.0f;
        for (int i = 0; i < 100; ++i)
            env = detector.processSample(0.0f, delayed);
        check(env == 0.0f, "Silence : enveloppe == 0");
        check(near(detector.getEnvelopeDb(), -120.0f, 1e-6f), "Silence : enveloppe à -120 dB");
    }
}

void testCompressorDSP()
{
    std::printf("[CompressorDSP]\n");
    constexpr double kSampleRate = 48000.0;
    constexpr int kSamples = 24000; // 0,5 s

    CompressorParams params;
    params.thresholdDb = -18.0f;
    params.ratio = 4.0f;
    params.attackMs = 1.0f;
    params.releaseMs = 100.0f;
    params.kneeDb = 0.0f;
    params.makeupDb = 0.0f;
    params.mix = 1.0f;
    params.detectorMode = DetectorMode::Peak;
    params.lookaheadMs = 0.0f;
    params.bypass = false;

    // Silence -> silence, GR nulle.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        dsp.setParams(params);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples, 0.0f));
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(rms(buffer[0]) == 0.0f, "Silence : sortie silencieuse");
        check(near(dsp.getGainReductionDb(), 0.0f, 1e-6f), "Silence : GR == 0");
    }

    // Signal fort (0,9) -> compression nette.
    float loudRmsOut = 0.0f;
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        dsp.setParams(params);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
        fillSine(buffer, 0.9f, 100.0f, kSampleRate);
        const float rmsIn = rms(buffer[0]);
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        loudRmsOut = rms(buffer[0]);
        check(loudRmsOut < rmsIn * 0.6f, "Signal fort : le niveau de sortie est réduit");
        const float gr = dsp.getGainReductionDb();
        check(gr < -8.0f && gr > -16.0f, "Signal fort : GR entre -16 et -8 dB");
        const auto meter = dsp.getMeterState();
        check(meter.gainReductionDb < -5.0f, "Meter bridge : gainReductionDb négative");
        check(meter.inputDb > -10.0f, "Meter bridge : inputDb proche du pic");
        check(meter.outputDb < meter.inputDb, "Meter bridge : outputDb < inputDb");
    }

    // Signal faible (0,05 ~= -26 dB) -> quasi aucune compression.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        dsp.setParams(params);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
        fillSine(buffer, 0.05f, 100.0f, kSampleRate);
        const float rmsIn = rms(buffer[0]);
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(near(rms(buffer[0]), rmsIn, rmsIn * 0.05f), "Signal faible : sortie ~= entrée");
        check(dsp.getGainReductionDb() > -1.0f, "Signal faible : GR quasi nulle");
    }

    // Makeup gain : +6 dB remonte le niveau.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        auto p = params;
        p.makeupDb = 6.0f;
        dsp.setParams(p);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
        fillSine(buffer, 0.9f, 100.0f, kSampleRate);
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(rms(buffer[0]) > loudRmsOut * 1.5f, "Makeup +6 dB : sortie plus forte que sans makeup");
    }

    // Mix 0 % : sortie = entrée (dry), même avec un signal fort.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        auto p = params;
        p.mix = 0.0f;
        dsp.setParams(p);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
        fillSine(buffer, 0.9f, 100.0f, kSampleRate);
        const float rmsIn = rms(buffer[0]);
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(near(rms(buffer[0]), rmsIn, rmsIn * 0.02f), "Mix 0 % : sortie == entrée");
    }

    // Bypass : sortie = entrée.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        auto p = params;
        p.bypass = true;
        dsp.setParams(p);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
        fillSine(buffer, 0.9f, 100.0f, kSampleRate);
        const float rmsIn = rms(buffer[0]);
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(near(rms(buffer[0]), rmsIn, rmsIn * 0.02f), "Bypass : sortie == entrée");
        check(near(dsp.getGainReductionDb(), 0.0f, 1e-6f), "Bypass : GR == 0");
    }

    // Lien stéréo : canal gauche fort, canal droit faible -> les deux sont réduits.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        dsp.setParams(params);
        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples));
        fillSine(buffer, 0.9f, 100.0f, kSampleRate);
        for (int i = 0; i < kSamples; ++i)
            buffer[1][static_cast<std::size_t>(i)] *= 0.1f; // droite 10x plus faible
        const float rmsRightIn = rms(buffer[1]);
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(rms(buffer[1]) < rmsRightIn * 0.7f,
              "Lien stéréo : le canal faible est réduit par le canal fort");
    }

    // Courbe statique du DSP == GainComputer + makeup.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        auto p = params;
        p.kneeDb = 12.0f;
        p.makeupDb = 3.0f;
        dsp.setParams(p);
        const float expected = GainComputer::computeOutputDb(-6.0f, p.thresholdDb, p.ratio,
                                                             p.kneeDb, p.makeupDb);
        check(near(dsp.getCurveOutputDb(-6.0f), expected, 1e-5f),
              "getCurveOutputDb() == GainComputer + makeup");
        check(near(CompressorDSP::curveOutputDb(-6.0f, p), expected, 1e-5f),
              "curveOutputDb() statique cohérente");
    }

    // Sanitization des paramètres.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        CompressorParams wild;
        wild.thresholdDb = 100.0f;
        wild.ratio = 100.0f;
        wild.mix = 5.0f;
        dsp.setParams(wild);
        const auto readBack = dsp.getParams();
        check(near(readBack.thresholdDb, GainComputer::kMaxThresholdDb, 1e-6f),
              "Sanitize : threshold clampé à +20 dB");
        check(near(readBack.ratio, GainComputer::kMaxRatio, 1e-6f),
              "Sanitize : ratio clampé à 20");
        check(near(readBack.mix, 1.0f, 1e-6f), "Sanitize : mix clampé à 1");
    }

    // Lookahead : latence déclarée + impulsion retardée mais compressée.
    {
        CompressorDSP dsp;
        dsp.prepare(kSampleRate, 2);
        auto p = params;
        p.lookaheadMs = 5.0f; // 240 échantillons
        dsp.setParams(p);
        check(dsp.getLatencySamples() == 240, "Lookahead 5 ms : latence == 240 échantillons");

        std::vector<std::vector<float>> buffer(2, std::vector<float>(kSamples, 0.0f));
        buffer[0][0] = 1.0f;
        buffer[1][0] = 1.0f;
        auto ptrs = pointersOf(buffer);
        dsp.processBlock(ptrs.data(), kSamples, 2);
        check(buffer[0][0] == 0.0f, "Lookahead : l'impulsion ne sort pas à l'échantillon 0");
        check(buffer[0][240] != 0.0f, "Lookahead : l'impulsion sort à l'échantillon 240");
    }
}

} // namespace

int main()
{
    std::printf("alpha-comp — Core tests\n");
    testGainComputer();
    testEnvelopeDetector();
    testCompressorDSP();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0)
        std::printf("ALL TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
