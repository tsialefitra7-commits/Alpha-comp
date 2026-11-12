#pragma once

// ============================================================================
// KnobRotary.h — Bouton rotatif épuré et précis (style FabFilter)
// ----------------------------------------------------------------------------
// Composant composite : Slider rotatif + label du paramètre + valeur exacte
// affichée en permanence sous le knob (comme le Pro-C 2 affiche la valeur
// au survol / au drag). Double-clic = retour à la valeur par défaut.
// Le Slider est exposé via getSlider() pour créer un SliderAttachment
// (APVTS) depuis le PluginEditor.
// ============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

namespace AlphaComp::UI {

class KnobRotary : public juce::Component, private juce::Slider::Listener
{
public:
    KnobRotary(const juce::String& label, const juce::String& unit,
               double minValue, double maxValue, double defaultValue,
               double interval = 0.0, double skewFactor = 1.0);

    ~KnobRotary() override;

    juce::Slider& getSlider() noexcept { return slider_; }
    const juce::Slider& getSlider() const noexcept { return slider_; }

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void sliderValueChanged(juce::Slider* slider) override;
    void updateValueLabel();

    juce::Slider slider_;
    juce::Label label_;
    juce::Label valueLabel_;
    juce::String unit_;
};

} // namespace AlphaComp::UI
