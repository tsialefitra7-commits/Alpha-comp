# alpha-comp — Core (DSP pur C++, zéro JUCE)
add_library(alpha_comp_core STATIC
    Source/GainComputer.cpp
    Source/EnvelopeDetector.cpp
    Source/CompressorDSP.cpp
)

target_include_directories(alpha_comp_core PUBLIC Include)
target_compile_features(alpha_comp_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(alpha_comp_core PRIVATE /W4)
else()
    target_compile_options(alpha_comp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
