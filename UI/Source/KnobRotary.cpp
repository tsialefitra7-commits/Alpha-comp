#include "UI/KnobRotary.h"

#include "UI/ProC2Theme.h"

#include <cmath>

namespace AlphaComp::UI {

KnobRotary::KnobRotary(const juce::String& label, const juce::String& unit,
                       double minValue, double maxValue, double defaultValue,
                       double interval, double skewFactor)
    : unit_(unit)
{
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setRange(minValue, maxValue, interval);
    if (skewFactor != 1.0)
        slider_.setSkewFactor(skewFactor);
    slider_.setValue(defaultValue);
#if JUCE_VERSION >= 0x080000
    // JUCE 8 : setDoubleClickReturnValue(bool, double, ...)
    slider_.setDoubleClickReturnValue(true, defaultValue);
#else
    // JUCE 7 : setDoubleClickReturnValue(double)
    slider_.setDoubleClickReturnValue(defaultValue);
#endif
    slider_.addListener(this);

    label_.setText(label, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(ProC2Theme::getFont(11.0f));
    label_.setColour(juce::Label::textColourId, ProC2Theme::textSecondary());

    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setFont(ProC2Theme::getFont(11.0f));
    valueLabel_.setColour(juce::Label::textColourId, ProC2Theme::textPrimary());

    addAndMakeVisible(slider_);
    addAndMakeVisible(label_);
    addAndMakeVisible(valueLabel_);

    updateValueLabel();
}

KnobRotary::~KnobRotary()
{
    slider_.removeListener(this);
}

void KnobRotary::resized()
{
    auto bounds = getLocalBounds();
    label_.setBounds(bounds.removeFromTop(14));
    valueLabel_.setBounds(bounds.removeFromBottom(14));
    slider_.setBounds(bounds.reduced(2));
}

void KnobRotary::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
    // Le fond est transparent : le panneau est dessiné par l'éditeur.
}

void KnobRotary::sliderValueChanged(juce::Slider* slider)
{
    juce::ignoreUnused(slider);
    updateValueLabel();
}

void KnobRotary::updateValueLabel()
{
    const double value = slider_.getValue();
    const double interval = slider_.getInterval();

    juce::String text;
    if (interval >= 1.0)
        text = juce::String(static_cast<int>(std::round(value)));
    else if (interval >= 0.1)
        text = juce::String(value, 1);
    else
        text = juce::String(value, 2);

    if (unit_.isNotEmpty())
        text += " " + unit_;

    valueLabel_.setText(text, juce::dontSendNotification);
}

} // namespace AlphaComp::UI
