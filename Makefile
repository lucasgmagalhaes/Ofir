# Top-level build for the Ofir repository.
# See docs/design/toolchain.md, section 7.

BUILD  ?= build
PYTHON ?= python3

.PHONY: all stage0 stage1 runtime unit runner-selftest test test-std test-stage1 compare-stages check-format check-codes check-sizes check-docs check-leaks check-asan bench bench-compare clean

all: stage0 runtime

stage0:
	$(MAKE) -C bootstrap BUILD=$(abspath $(BUILD))/stage0

runtime:
	$(MAKE) -C runtime BUILD=$(abspath $(BUILD))

unit:
	$(MAKE) -C bootstrap test BUILD=$(abspath $(BUILD))/stage0

runner-selftest:
	$(PYTHON) tests/runner/selftest/run_selftest.py

check-codes:
	$(PYTHON) tools/check_error_codes.py

check-sizes:
	$(PYTHON) tools/check_sizes.py

check-docs:
	$(PYTHON) tools/check_docs.py

# No inline function or statement bodies (docs/style-guide.md, `fmt.inlineBlocks`).
check-format:
	$(PYTHON) tools/check_format.py

# Every run test and example under valgrind: no leaks, no invalid accesses.
check-leaks: stage0 runtime
	$(PYTHON) tools/check_leaks.py --compiler $(BUILD)/stage0/ofir

# The `@test` functions of std and of the stage 1 compiler (spec 12.2).
test-std: stage0 runtime
	$(PYTHON) tools/run_std_tests.py --compiler $(BUILD)/stage0/ofir

# Every run and panic test and example under ASan and UBSan (needs gcc).
check-asan: stage0 runtime
	$(PYTHON) tools/check_asan.py --compiler $(BUILD)/stage0/ofir

# Stage 1 (compiler/, built by stage 0) gives the same output as stage 0.
compare-stages: stage0 runtime
	$(PYTHON) tools/compare_stages.py --compiler $(BUILD)/stage0/ofir --fuzz 200

# Stage 1, built by stage 0.
stage1: stage0 runtime
	mkdir -p $(BUILD)/stage1
	OFIR_STD=$(abspath std) $(BUILD)/stage0/ofir build compiler/main.of -o $(BUILD)/stage1/ofir

# Every test with stage 1 as the compiler (roadmap 4.8, 4.9).
test-stage1: stage1
	OFIR_STD=$(abspath std) OFIR_RUNTIME=$(abspath $(BUILD))/libofir_rt.a \
	    $(PYTHON) tests/runner/run_tests.py --compiler $(BUILD)/stage1/ofir --stage 1
	OFIR_STD=$(abspath std) OFIR_RUNTIME=$(abspath $(BUILD))/libofir_rt.a \
	    $(PYTHON) tools/run_std_tests.py --compiler $(BUILD)/stage1/ofir

test: stage0 runtime unit runner-selftest check-codes check-sizes check-docs check-format test-std compare-stages test-stage1
	$(PYTHON) tests/runner/run_tests.py --compiler $(BUILD)/stage0/ofir

# Performance (docs/design/performance.md)
bench:
	$(MAKE) -C bootstrap release BUILD=$(abspath $(BUILD))/release
	$(PYTHON) bench/run_bench.py --compiler $(BUILD)/release/ofir

BASE ?= origin/main
bench-compare:
	tools/bench_compare.sh $(BASE)

clean:
	rm -rf $(BUILD)
