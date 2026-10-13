# Host-side unit tests for the platform-independent core.
CXX      ?= g++
CXXFLAGS := -std=c++20 -O1 -g -Wall -Wextra -Werror -isystem ../third_party -fsanitize=address,undefined -fno-omit-frame-pointer
CORE     := $(wildcard ../core/*.cpp)
TESTS    := $(wildcard test_*.cpp) main.cpp

.PHONY: test clean
test: build/run_tests
	./build/run_tests

build/run_tests: $(TESTS) $(CORE) $(wildcard ../core/*.hpp) $(wildcard ../packs/*.ini) tinytest.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(TESTS) $(CORE) -o $@

clean:
	rm -rf build
