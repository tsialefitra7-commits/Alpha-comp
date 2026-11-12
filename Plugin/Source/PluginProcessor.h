#pragma once

// ============================================================================
// PluginProcessor.h — Adaptateur DAW <-> Core <-> UI
// ----------------------------------------------------------------------------
// Reçoit l'audio du DAW, alimente le Core (CompressorDSP, C++ pur) et
// transmet les métriques à l'UI via le Meter Bridge du Core.
// Les paramètres vivent dans l'APVTS (automatisables) ; à chaque bloc,
// processBlock() les relit et les pousse dans le Core (atomiques).
// ============================================================================

#include "Core/CompressorDSP.h"

#include <juce_audio_processors/juce_audio_processors.h>

class AlphaCompProcessor : public juce::AudioProcessor
{
public:
    // IDs de paramètres (doivent correspondre à CompressionCurve :
    // "threshold" et "ratio" sont utilisés par le drag sur la courbe).
    static constexpr const char* kThresholdParamID = "threshold";
    static constexpr const char* kRatioParamID = "ratio";
    static constexpr const char* kAttackParamID = "attack";
    static constexpr const char* kReleaseParamID = "release";
    static constexpr const char* kKneeParamID = "knee";
    static constexpr const char* kMakeupParamID = "makeup";
    static constexpr const char* kMixParamID = "mix";
    static constexpr const char* kDetectorParamID = "detector";
    static constexpr const char* kLookaheadParamID = "lookahead";
    static constexpr const char* kBypassParamID = "bypass";

    AlphaCompProcessor();
    ~AlphaCompProcessor() override;

    const juce::String getName() const override { return "alpha-comp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& newName) override
    {
        juce::ignoreUnused(index, newName);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    AlphaComp::Core::CompressorDSP& getCompressorDSP() noexcept { return dsp_; }
    const AlphaComp::Core::CompressorDSP& getCompressorDSP() const noexcept { return dsp_; }
    AlphaComp::Core::MeterState getMeterState() const { return dsp_.getMeterState(); }

private:
    void updateDSPParams();

    AlphaComp::Core::CompressorDSP dsp_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlphaCompProcessor)
};

// Point d'entrée attendu par les formats JUCE (VST3/AU/Standalone).
juce::AudioProcessor* createPluginFilter();
