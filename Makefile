CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -I.
BUILD    := build

EXAMPLE_SRC := examples/example.cpp
EXAMPLE_BIN := $(BUILD)/example

.PHONY: all clean lint

all: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_SRC) dasmig/entitygen.hpp dasmig/random.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(EXAMPLE_SRC)

$(BUILD):
	@mkdir -p $(BUILD)

lint:
	clang-tidy $(EXAMPLE_SRC) -- $(CXXFLAGS)

clean:
	rm -rf $(BUILD)
