# alpha-comp — Tests du Core (aucune dépendance externe)
add_executable(alpha_comp_tests CoreTests.cpp)
target_link_libraries(alpha_comp_tests PRIVATE alpha_comp_core)

add_test(NAME alpha_comp_tests COMMAND alpha_comp_tests)
