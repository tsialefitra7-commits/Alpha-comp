# alpha-comp — UI (JUCE, style Pro-C 2). Nécessite JUCE (voir CMake racine).
add_library(alpha_comp_ui STATIC
    Source/ProC2Theme.cpp
    Source/KnobRotary.cpp
    Source/CompressionCurve.cpp
    Source/GainReductionMeter.cpp
)

target_include_directories(alpha_comp_ui PUBLIC Include)

# JUCE en « headers-only » : les sources des modules JUCE sont compilées une
# seule fois, dans la cible plugin (voir helper alphacomp_juce_headers_only).
alphacomp_juce_headers_only(juce::juce_gui_basics ALPHACOMP_JUCE_GUI_HDRS)
alphacomp_juce_headers_only(juce::juce_audio_processors ALPHACOMP_JUCE_AP_HDRS)

target_link_libraries(alpha_comp_ui PUBLIC
    alpha_comp_core
    ${ALPHACOMP_JUCE_GUI_HDRS}
    ${ALPHACOMP_JUCE_AP_HDRS}
)

if(MSVC)
    target_compile_options(alpha_comp_ui PRIVATE /W4)
else()
    target_compile_options(alpha_comp_ui PRIVATE -Wall -Wextra)
endif()
