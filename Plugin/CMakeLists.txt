# alpha-comp — Plugin JUCE (VST3 / AU / Standalone). Nécessite JUCE.
# Formats configurables, ex. -DALPHACOMP_PLUGIN_FORMATS=VST3
# (utile sur les systèmes minimaux sans ALSA : le Standalone exige
# juce_audio_devices, le VST3 non).
set(ALPHACOMP_PLUGIN_FORMATS "VST3 AU Standalone" CACHE STRING "Formats de plugin JUCE")

juce_add_plugin(alpha-comp
    VERSION 1.0.0
    COMPANY_NAME "AlphaComp"
    PLUGIN_MANUFACTURER_CODE "AlCo"
    PLUGIN_CODE "AlCp"
    FORMATS ${ALPHACOMP_PLUGIN_FORMATS}
    PRODUCT_NAME "alpha-comp"
    DESCRIPTION "alpha-comp — Compresseur inspiré du FabFilter Pro-C 2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Dynamics"
    AU_SANDBOX_SAFE TRUE
)

target_sources(alpha-comp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_link_libraries(alpha-comp PRIVATE
    alpha_comp_ui
    alpha_comp_core
    juce::juce_audio_processors
    juce::juce_gui_basics
)

# juce_audio_utils (audio devices / ALSA) n'est requis que pour le Standalone.
if(ALPHACOMP_PLUGIN_FORMATS MATCHES "Standalone")
    target_link_libraries(alpha-comp PRIVATE juce::juce_audio_utils)
endif()

target_compile_definitions(alpha-comp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# JUCE 8 exige C++20 (le projet racine l'impose déjà, on le rappelle ici).
set_target_properties(alpha-comp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
