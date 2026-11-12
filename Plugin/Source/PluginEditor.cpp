#include "PluginEditor.h"

#include <memory>

AlphaCompEditor::AlphaCompEditor(AlphaCompProcessor& processor)
    : AudioProcessorEditor(processor),
      processor_(processor),
      curve_(processor.getCompressorDSP(), &processor.apvts),
      thresholdKnob_("THRESHOLD", "dB", -60.0, 20.0, -18.0, 0.1),
      ratioKnob_("RATIO", ":1", 1.0, 20.0, 4.0, 0.1),
      attackKnob_("ATTACK", "ms", 0.01, 300.0, 10.0, 0.01, 0.4),
      releaseKnob_("RELEASE", "ms", 10.0, 1000.0, 100.0, 0.1, 0.5),
      kneeKnob_("KNEE", "dB", 0.0, 40.0, 12.0, 0.1),
      makeupKnob_("MAKEUP", "dB", -24.0, 24.0, 0.0, 0.1),
      mixKnob_("MIX", "%", 0.0, 100.0, 100.0, 1.0),
      lookaheadKnob_("LOOKAHEAD", "ms", 0.0, 20.0, 0.0, 0.1)
{
    setSize(780, 460);
    setResizable(true, true);
    setResizeLimits(560, 380, 1200, 700);

    // Look & Feel Pro-C 2 sur les knobs, le combo et le bouton.
    thresholdKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    ratioKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    attackKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    releaseKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    kneeKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    makeupKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    mixKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    lookaheadKnob_.getSlider().setLookAndFeel(&lookAndFeel_);
    detectorCombo_.setLookAndFeel(&lookAndFeel_);
    bypassButton_.setLookAndFeel(&lookAndFeel_);

    detectorCombo_.addItem("Peak", 1);
    detectorCombo_.addItem("RMS", 2);
    detectorCombo_.setSelectedId(1, juce::dontSendNotification);
    detectorCombo_.setJustificationType(juce::Justification::centredLeft);

    bypassButton_.setButtonText("Bypass");
    bypassButton_.setClickingTogglesState(true);

    addAndMakeVisible(curve_);
    addAndMakeVisible(grMeter_);
    addAndMakeVisible(thresholdKnob_);
    addAndMakeVisible(ratioKnob_);
    addAndMakeVisible(attackKnob_);
    addAndMakeVisible(releaseKnob_);
    addAndMakeVisible(kneeKnob_);
    addAndMakeVisible(makeupKnob_);
    addAndMakeVisible(mixKnob_);
    addAndMakeVisible(lookaheadKnob_);
    addAndMakeVisible(detectorCombo_);
    addAndMakeVisible(bypassButton_);

    // Attachments APVTS (automatisables dans le DAW).
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    thresholdAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kThresholdParamID, thresholdKnob_.getSlider());
    ratioAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kRatioParamID, ratioKnob_.getSlider());
    attackAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kAttackParamID, attackKnob_.getSlider());
    releaseAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kReleaseParamID, releaseKnob_.getSlider());
    kneeAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kKneeParamID, kneeKnob_.getSlider());
    makeupAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kMakeupParamID, makeupKnob_.getSlider());
    mixAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kMixParamID, mixKnob_.getSlider());
    lookaheadAttachment_ = std::make_unique<Attachment>(
        processor_.apvts, AlphaCompProcessor::kLookaheadParamID, lookaheadKnob_.getSlider());

    detectorAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts, AlphaCompProcessor::kDetectorParamID, detectorCombo_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor_.apvts, AlphaCompProcessor::kBypassParamID, bypassButton_);

    startTimer(16); // ~60 fps pour le vumètre GR
}

AlphaCompEditor::~AlphaCompEditor()
{
    stopTimer();
    thresholdKnob_.getSlider().setLookAndFeel(nullptr);
    ratioKnob_.getSlider().setLookAndFeel(nullptr);
    attackKnob_.getSlider().setLookAndFeel(nullptr);
    releaseKnob_.getSlider().setLookAndFeel(nullptr);
    kneeKnob_.getSlider().setLookAndFeel(nullptr);
    makeupKnob_.getSlider().setLookAndFeel(nullptr);
    mixKnob_.getSlider().setLookAndFeel(nullptr);
    lookaheadKnob_.getSlider().setLookAndFeel(nullptr);
    detectorCombo_.setLookAndFeel(nullptr);
    bypassButton_.setLookAndFeel(nullptr);
}

void AlphaCompEditor::paint(juce::Graphics& g)
{
    g.fillAll(AlphaComp::UI::ProC2Theme::background());

    // Titre.
    g.setFont(AlphaComp::UI::ProC2Theme::getFont(20.0f, true));
    g.setColour(AlphaComp::UI::ProC2Theme::textPrimary());
    g.drawText("alpha-comp", 16, 8, 220, 26, juce::Justification::centredLeft);

    g.setFont(AlphaComp::UI::ProC2Theme::getFont(10.0f));
    g.setColour(AlphaComp::UI::ProC2Theme::textSecondary());
    g.drawText("COMPRESSOR  •  v1.0.0", 16, 34, 220, 14, juce::Justification::centredLeft);

    // Séparateur sous la barre de titre.
    g.setColour(AlphaComp::UI::ProC2Theme::gridLine());
    g.drawHorizontalLine(52, 12.0f, static_cast<float>(getWidth()) - 12.0f);
}

void AlphaCompEditor::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    bounds.removeFromTop(48); // barre de titre

    // Rangée de knobs en bas.
    auto knobsArea = bounds.removeFromBottom(92);
    bounds.removeFromBottom(8);

    // Courbe (gauche, large) + vumètre GR (droite).
    grMeter_.setBounds(bounds.removeFromRight(72));
    bounds.removeFromRight(8);
    curve_.setBounds(bounds);

    // 8 knobs répartis uniformément.
    const int numKnobs = 8;
    const int knobWidth = knobsArea.getWidth() / numKnobs;
    AlphaComp::UI::KnobRotary* knobs[numKnobs] = { &thresholdKnob_, &ratioKnob_, &attackKnob_, &releaseKnob_,
                                        &kneeKnob_, &makeupKnob_, &mixKnob_, &lookaheadKnob_ };
    for (int i = 0; i < numKnobs; ++i)
    {
        auto knobBounds = knobsArea.removeFromLeft(knobWidth);
        knobs[i]->setBounds(knobBounds.reduced(4));
    }

    // Contrôles de la barre de titre (droite).
    auto titleBounds = getLocalBounds().reduced(12);
    titleBounds.removeFromBottom(getHeight() - 52);
    bypassButton_.setBounds(titleBounds.removeFromRight(90));
    titleBounds.removeFromRight(8);
    detectorCombo_.setBounds(titleBounds.removeFromRight(110));
}

void AlphaCompEditor::timerCallback()
{
    // Meter Bridge -> vumètre (60 fps, sans lock).
    const auto meter = processor_.getMeterState();
    grMeter_.setGainReductionDb(meter.gainReductionDb);
    const bool bypassed = processor_.apvts.getRawParameterValue(
                              AlphaCompProcessor::kBypassParamID)->load() > 0.5f;
    grMeter_.setBypassed(bypassed);
}
