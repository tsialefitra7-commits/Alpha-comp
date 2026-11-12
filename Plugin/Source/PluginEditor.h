#pragma once

// ============================================================================
// PluginEditor.h — Assemble l'interface visuelle Pro-C 2
// ----------------------------------------------------------------------------
// Disposition (fenêtre ~780x460) :
//   Haut   : titre "alpha-comp" | sélecteur Detector (Peak/RMS) | Bypass
//   Centre : CompressionCurve (grande) + GainReductionMeter (à droite)
//   Bas    : rangée de knobs — Threshold, Ratio, Attack, Release,
//            Knee, Makeup, Mix, Lookahead (valeur exacte sous chaque knob)
// Timer à 60 fps : pousse la GR du Meter Bridge dans le vumètre.
// (La courbe a son propre Timer à 60 fps.)
// ============================================================================

#include "PluginProcessor.h"

#include "UI/CompressionCurve.h"
#include "UI/GainReductionMeter.h"
#include "UI/KnobRotary.h"
#include "UI/ProC2Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

class AlphaCompEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AlphaCompEditor(AlphaCompProcessor& processor);
    ~AlphaCompEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // Ordre important : processor_ avant curve_ (curve_ utilise processor_).
    AlphaCompProcessor& processor_;
    AlphaComp::UI::CompressionCurve curve_;
    AlphaComp::UI::GainReductionMeter grMeter_;

    AlphaComp::UI::KnobRotary thresholdKnob_;
    AlphaComp::UI::KnobRotary ratioKnob_;
    AlphaComp::UI::KnobRotary attackKnob_;
    AlphaComp::UI::KnobRotary releaseKnob_;
    AlphaComp::UI::KnobRotary kneeKnob_;
    AlphaComp::UI::KnobRotary makeupKnob_;
    AlphaComp::UI::KnobRotary mixKnob_;
    AlphaComp::UI::KnobRotary lookaheadKnob_;

    juce::ComboBox detectorCombo_;
    juce::ToggleButton bypassButton_;

    AlphaComp::UI::ProC2LookAndFeel lookAndFeel_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> makeupAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detectorAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlphaCompEditor)
};
