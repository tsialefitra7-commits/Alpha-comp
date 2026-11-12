# alpha-comp — Makefile de secours (Core uniquement, sans CMake ni JUCE)
#
#   make test   : compile et lance les tests du Core avec g++
#   make demo   : compile et lance la démo console du Core
#   make clean  : supprime build/
#
# Le plugin JUCE (UI/Plugin) nécessite CMake + JUCE : voir README.md.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -I Core/Include
CORE_SRC := Core/Source/GainComputer.cpp Core/Source/EnvelopeDetector.cpp Core/Source/CompressorDSP.cpp
BUILD    := build

.PHONY: test demo clean

test: $(BUILD)/alpha_comp_tests
	./$(BUILD)/alpha_comp_tests

demo: $(BUILD)/alpha_comp_demo
	./$(BUILD)/alpha_comp_demo

$(BUILD)/alpha_comp_tests: Tests/CoreTests.cpp $(CORE_SRC)
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ Tests/CoreTests.cpp $(CORE_SRC)

$(BUILD)/alpha_comp_demo: Demo/ConsoleDemo.cpp $(CORE_SRC)
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ Demo/ConsoleDemo.cpp $(CORE_SRC)

clean:
	rm -rf $(BUILD)
