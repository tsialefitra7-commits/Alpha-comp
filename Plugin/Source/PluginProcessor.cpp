#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <memory>
#include <utility>

namespace
{
// JUCE 7 : Layout = vector<unique_ptr<...>> (push_back)
// JUCE 8 : ParameterLayout (add) — Layout n'existe plus.
// Le type de retour déduit (auto) + le lambda addParam rendent ce code
// compatible avec les deux versions.
auto createParameterLayout()
{
#if JUCE_VERSION >= 0x080000
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
#else
    juce::AudioProcessorValueTreeState::Layout layout;
#endif

    auto addParam = [&layout](auto&& param)
    {
#if JUCE_VERSION >= 0x080000
        layout.add(std::forward<decltype(param)>(param));
#else
        layout.push_back(std::forward<decltype(param)>(param));
#endif
    };

    // Les plages et skews doivent correspondre aux KnobRotary de l'éditeur
    // (un SliderAttachment exige slider.min/max == paramètre.min/max).
    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kThresholdParamID), "Threshold",
        juce::NormalisableRange<float>(-60.0f, 20.0f, 0.1f), -18.0f));

    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kRatioParamID), "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 4.0f));

    // Attack/Release : skew pour plus de résolution en bas de plage.
    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kAttackParamID), "Attack",
        juce::NormalisableRange<float>(0.01f, 300.0f, 0.01f, 0.4f), 10.0f));

    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kReleaseParamID), "Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 0.1f, 0.5f), 100.0f));

    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kKneeParamID), "Knee",
        juce::NormalisableRange<float>(0.0f, 40.0f, 0.1f), 12.0f));

    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kMakeupParamID), "Makeup",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kMixParamID), "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    addParam(std::make_unique<juce::AudioParameterChoice>(
        juce::String(AlphaCompProcessor::kDetectorParamID), "Detector",
        juce::StringArray({ "Peak", "RMS" }), 0));

    addParam(std::make_unique<juce::AudioParameterFloat>(
        juce::String(AlphaCompProcessor::kLookaheadParamID), "Lookahead",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 0.0f));

    addParam(std::make_unique<juce::AudioParameterBool>(
        juce::String(AlphaCompProcessor::kBypassParamID), "Bypass", false));

    return layout;
}
} // namespace

AlphaCompProcessor::AlphaCompProcessor()
    // Bus stéréo EXPLICITES et ACTIVÉS par défaut : le constructeur par
    // défaut d'AudioProcessor crée des bus « disabled by default », ce que
    // les DAW respectent (aucun routage audio => silence).
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "alpha-comp", createParameterLayout())
{
    updateDSPParams();
}

AlphaCompProcessor::~AlphaCompProcessor() = default;

bool AlphaCompProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void AlphaCompProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    dsp_.prepare(sampleRate, getTotalNumOutputChannels());
    // Le lookahead introduit une latence : on la déclare au DAW.
    setLatencySamples(dsp_.getLatencySamples());
}

void AlphaCompProcessor::releaseResources()
{
    dsp_.reset();
}

void AlphaCompProcessor::updateDSPParams()
{
    AlphaComp::Core::CompressorParams params;
    params.thresholdDb = apvts.getRawParameterValue(kThresholdParamID)->load();
    params.ratio = apvts.getRawParameterValue(kRatioParamID)->load();
    params.attackMs = apvts.getRawParameterValue(kAttackParamID)->load();
    params.releaseMs = apvts.getRawParameterValue(kReleaseParamID)->load();
    params.kneeDb = apvts.getRawParameterValue(kKneeParamID)->load();
    params.makeupDb = apvts.getRawParameterValue(kMakeupParamID)->load();
    // Mix : le paramètre est en % (0..100), le Core attend 0..1.
    params.mix = apvts.getRawParameterValue(kMixParamID)->load() / 100.0f;
    params.detectorMode = static_cast<AlphaComp::Core::DetectorMode>(
        static_cast<int>(apvts.getRawParameterValue(kDetectorParamID)->load()));
    params.lookaheadMs = apvts.getRawParameterValue(kLookaheadParamID)->load();
    params.bypass = apvts.getRawParameterValue(kBypassParamID)->load() > 0.5f;
    dsp_.setParams(params);
}

void AlphaCompProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Pont APVTS -> Core (atomiques, sans allocation, thread audio).
    updateDSPParams();

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    float* const* channelData = buffer.getArrayOfWritePointers();
    dsp_.processBlock(channelData, numSamples, numChannels);
}

juce::AudioProcessorEditor* AlphaCompProcessor::createEditor()
{
    return new AlphaCompEditor(*this);
}

void AlphaCompProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // copyState().createXml() : compatible JUCE 7 et 8
    // (APVTS::copyXml() a été supprimé en JUCE 8).
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void AlphaCompProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // getXmlFromBinary() renvoie XmlElement* (JUCE 7) ou unique_ptr (JUCE 8) :
    // on normalise via unique_ptr dans les deux cas.
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        updateDSPParams();
    }
}

juce::AudioProcessor* createPluginFilter()
{
    return new AlphaCompProcessor();
}
