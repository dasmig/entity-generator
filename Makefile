CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Wno-c2y-extensions -I.
BUILD    := build
PYTHON   := python3

HEADERS := dasmig/entitygen.hpp dasmig/ext/stats_observer.hpp dasmig/core.hpp $(wildcard dasmig/core/*.hpp)

EXAMPLE_SRC := examples/example.cpp
EXAMPLE_BIN := $(BUILD)/example

TEST_SRC := tests/tests.cpp tests/catch_amalgamated.cpp
TEST_BIN := $(BUILD)/test_runner

BENCH_SRC   := bench/bench.cpp
BENCH_FLAGS := -O2 -DNDEBUG

COV_FLAGS := -fprofile-instr-generate -fcoverage-mapping
COV_DIR   := coverage

.PHONY: all clean lint test entt-test bench bench-gate coverage docs

all: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_SRC) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(EXAMPLE_SRC)

$(TEST_BIN): $(TEST_SRC) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SRC)

test: $(TEST_BIN)
	./$(TEST_BIN)

# The EnTT adapter: its tests and the village demo, built against an EnTT
# checkout (ENTT_DIR is its src folder). Not part of `test`, which needs no
# third-party code.
ENTT_DIR ?=
entt-test: $(HEADERS) dasmig/ext/entt_adapter.hpp | $(BUILD)
	@test -n "$(ENTT_DIR)" || { echo "set ENTT_DIR to the src folder of an EnTT checkout"; exit 1; }
	$(CXX) $(CXXFLAGS) -isystem $(ENTT_DIR) -o $(BUILD)/entt_test_runner tests/test_entt_adapter.cpp tests/catch_amalgamated.cpp
	./$(BUILD)/entt_test_runner
	$(CXX) $(CXXFLAGS) -isystem $(ENTT_DIR) -o $(BUILD)/entt_demo examples/entt_demo.cpp
	./$(BUILD)/entt_demo

# Benchmarks (bench/): an optimized build run on each tier, and again with
# the lite tier compiled in where the library has data. Results in
# build/bench.tsv.
bench: $(BENCH_SRC) bench/bench.hpp $(HEADERS) | $(BUILD)
	rm -f $(BUILD)/bench.tsv
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) -o $(BUILD)/bench $(BENCH_SRC)
	./$(BUILD)/bench --tier lite --out $(BUILD)/bench.tsv
	./$(BUILD)/bench --tier full --out $(BUILD)/bench.tsv

# The instruction-count gate: the benchmark in --gate mode under cachegrind,
# compared with bench/gate_baseline.txt (scripts/gate.py). Needs Valgrind.
bench-gate: $(BENCH_SRC) bench/bench.hpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) -o $(BUILD)/bench_gate $(BENCH_SRC)
	$(PYTHON) scripts/gate.py $(BUILD)/bench_gate

coverage: $(TEST_SRC) $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(COV_FLAGS) -o $(BUILD)/test_cov $(TEST_SRC)
	cd $(BUILD) && ./test_cov
	llvm-profdata merge -sparse $(BUILD)/default.profraw -o $(BUILD)/tests.profdata
	llvm-cov report $(BUILD)/test_cov --instr-profile=$(BUILD)/tests.profdata --sources $(HEADERS)
	llvm-cov show $(BUILD)/test_cov --instr-profile=$(BUILD)/tests.profdata --sources $(HEADERS) --format=html -o $(COV_DIR)
	@echo "Coverage report: $(COV_DIR)/index.html"

$(BUILD):
	@mkdir -p $(BUILD)

docs:
	doxygen Doxyfile
	@echo "API docs: doc/api/html/index.html"

lint:
	clang-tidy $(EXAMPLE_SRC) -- $(CXXFLAGS)

clean:
	rm -rf $(BUILD) $(COV_DIR) doc/api
