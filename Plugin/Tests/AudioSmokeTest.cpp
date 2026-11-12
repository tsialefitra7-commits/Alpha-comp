// Test de fumée du chemin audio du plugin (sans DAW, sans GUI) :
// instancie AlphaCompProcessor, pousse un sinus stéréo dans processBlock()
// et vérifie que le son passe.
//
// Régression couverte : les bus audio désactivés par défaut (constructeur
// AudioProcessor sans BusesProperties) => silence dans les DAW.
//
// Construit avec -DALPHACOMP_BUILD_TESTS=ON quand JUCE est disponible.

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>

namespace
{
int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "OK" : "ECHEC", what);
    if (!ok)
        ++failures;
}

double rmsOf(const juce::AudioBuffer<float>& b)
{
    double acc = 0.0;
    const int n = b.getNumChannels() * b.getNumSamples();
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            acc += (double) b.getSample(ch, i) * (double) b.getSample(ch, i);
    return n > 0 ? std::sqrt(acc / n) : 0.0;
}
} // namespace

int main()
{
    std::printf("alpha-comp — test de fumee du chemin audio\n");

    AlphaCompProcessor proc;
    check(proc.getTotalNumInputChannels() == 2, "bus d'entree stereo actif");
    check(proc.getTotalNumOutputChannels() == 2, "bus de sortie stereo actif");

    proc.prepareToPlay(48000.0, 512);

    const int kBlockSize = 512;
    const int kBlocks = 40;
    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;

    // Rend kBlocks d'un sinus 440 Hz d'amplitude amp, retourne le RMS global.
    auto runBlocks = [&](double& phase, float amp, double& peakDiffVsInput)
    {
        double acc = 0.0;
        peakDiffVsInput = 0.0;
        for (int b = 0; b < kBlocks; ++b)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float s = amp * (float) std::sin(phase);
                phase += 2.0 * juce::MathConstants<double>::pi * 440.0 / 48000.0;
                for (int ch = 0; ch < 2; ++ch)
                    buf.setSample(ch, i, s);
            }
            const double before = rmsOf(buf);
            proc.processBlock(buf, midi);
            const double after = rmsOf(buf);
            acc += after * after;
            peakDiffVsInput = std::max(peakDiffVsInput, std::abs(after - before));
        }
        return std::sqrt(acc / kBlocks);
    };

    // 1. Paramètres par défaut (compression active, mix 100 %) : le son passe
    //    et la compression réduit le niveau.
    double phase = 0.0;
    double diff = 0.0;
    const double rmsIn = 0.5 / std::sqrt(2.0); // RMS d'un sinus d'amplitude 0.5
    const double rmsOut = runBlocks(phase, 0.5f, diff);
    std::printf("       RMS entree=%.4f sortie=%.4f\n", rmsIn, rmsOut);
    check(rmsOut > 0.05, "signal audible en sortie (parametres par defaut)");
    check(rmsOut < rmsIn, "compression appliquee (sortie < entree)");

    // 2. Bypass : la sortie doit egaliser l'entree (lookahead 0 par defaut).
    proc.apvts.getParameter("bypass")->setValueNotifyingHost(1.0f);
    proc.apvts.getParameter("lookahead")->setValueNotifyingHost(0.0f);
    phase = 0.0;
    const double rmsBypass = runBlocks(phase, 0.5f, diff);
    std::printf("       RMS bypass=%.4f (entree=%.4f)\n", rmsBypass, rmsIn);
    check(std::abs(rmsBypass - rmsIn) < 0.01, "bypass = signal d'entree");
    check(diff < 0.01, "bypass transparent echantillon par echantillon");

    // 3. Mix 0 % (tout sec, hors bypass) : signal d'entree.
    proc.apvts.getParameter("bypass")->setValueNotifyingHost(0.0f);
    proc.apvts.getParameter("mix")->setValueNotifyingHost(0.0f);
    phase = 0.0;
    const double rmsDry = runBlocks(phase, 0.5f, diff);
    check(std::abs(rmsDry - rmsIn) < 0.01, "mix 0% = signal d'entree");

    // 4. Lookahead 5 ms : le son passe (retardé) et la compression agit.
    proc.apvts.getParameter("mix")->setValueNotifyingHost(1.0f);
    proc.apvts.getParameter("lookahead")->setValueNotifyingHost(1.0f); // 20 ms max
    phase = 0.0;
    const double rmsLa = runBlocks(phase, 0.5f, diff);
    std::printf("       RMS lookahead=%.4f\n", rmsLa);
    check(rmsLa > 0.05, "signal audible avec lookahead");

    std::printf(failures == 0 ? "\nTOUT EST OK\n" : "\n%d ECHEC(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
