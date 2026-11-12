# alpha-comp — Démo console du Core (sans JUCE)
add_executable(alpha_comp_demo ConsoleDemo.cpp)
target_link_libraries(alpha_comp_demo PRIVATE alpha_comp_core)
