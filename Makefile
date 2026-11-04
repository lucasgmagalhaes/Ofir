# Top-level build for the Ofir repository.
# See docs/design/toolchain.md, section 7.

BUILD  ?= build
PYTHON ?= python3

.PHONY: all stage0 stage1 stage2 verify runtime unit runner-selftest test test-std test-stage1 compare-stages check-format check-codes check-sizes check-docs check-leaks check-asan seed check-seed bench bench-programs bench-compare clean

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
compare-stages: stage1
	$(PYTHON) tools/compare_stages.py --compiler $(BUILD)/stage0/ofir --stage1 $(BUILD)/stage1/ofir --fuzz 200

# Stage 1, built by stage 0.
stage1: stage0 runtime
	mkdir -p $(BUILD)/stage1
	OFIR_STD=$(abspath std) $(BUILD)/stage0/ofir build compiler/main.of -o $(BUILD)/stage1/ofir

# Stage 2: the compiler built by stage 1 (roadmap 4.13).
STAGE_ENV = OFIR_STD=$(abspath std) OFIR_RUNTIME=$(abspath $(BUILD))/libofir_rt.a
stage2: stage1
	mkdir -p $(BUILD)/stage2
	$(STAGE_ENV) $(BUILD)/stage1/ofir build compiler/main.of -o $(BUILD)/stage2/ofir

# Stage 2 builds itself (stage 3): both write the same LLVM IR for compiler/.
verify: stage2
	mkdir -p $(BUILD)/stage3
	$(STAGE_ENV) $(BUILD)/stage2/ofir build compiler/main.of -o $(BUILD)/stage3/ofir
	$(STAGE_ENV) $(BUILD)/stage2/ofir build compiler/main.of --emit=llvm -o $(BUILD)/stage2/self
	$(STAGE_ENV) $(BUILD)/stage3/ofir build compiler/main.of --emit=llvm -o $(BUILD)/stage3/self
	cmp $(BUILD)/stage2/self.ll $(BUILD)/stage3/self.ll
	@echo "verify: stage 2 and stage 3 generate the same LLVM IR for compiler/"

# The seed of a release (docs/bootstrap.md): the LLVM IR that the compiler
# writes for itself, once stage 2 and stage 3 agree on it.
VERSION := $(shell sed -n 's/^const VERSION: str = "\(.*\)"/\1/p' compiler/main.of)
SEED    ?= $(BUILD)/seed/ofir-$(VERSION).ll
seed: verify
	mkdir -p $(BUILD)/seed
	cp $(BUILD)/stage2/self.ll $(BUILD)/seed/ofir-$(VERSION).ll
	cd $(BUILD)/seed && (sha256sum ofir-$(VERSION).ll 2>/dev/null || \
	    shasum -a 256 ofir-$(VERSION).ll) > ofir-$(VERSION).ll.sha256
	@cat $(BUILD)/seed/ofir-$(VERSION).ll.sha256

# The fast path of the bootstrap: clang builds a compiler from a seed
# (SEED=path), which must build the current compiler and pass the tests.
SEED_CLANG = $(if $(OFIR_CLANG),$(OFIR_CLANG),clang)
check-seed: runtime
	mkdir -p $(BUILD)/from-seed
	$(SEED_CLANG) -O2 -Wno-override-module $(SEED) $(BUILD)/libofir_rt.a -lm \
	    -o $(BUILD)/from-seed/seed
	$(STAGE_ENV) $(BUILD)/from-seed/seed build compiler/main.of -o $(BUILD)/from-seed/ofir
	$(BUILD)/from-seed/ofir version
	$(STAGE_ENV) $(PYTHON) tests/runner/run_tests.py --compiler $(BUILD)/from-seed/ofir --stage 1

# Every test with stage 1 as the compiler (roadmap 4.8, 4.9).
test-stage1: stage1
	OFIR_STD=$(abspath std) OFIR_RUNTIME=$(abspath $(BUILD))/libofir_rt.a \
	    $(PYTHON) tests/runner/run_tests.py --compiler $(BUILD)/stage1/ofir --stage 1
	OFIR_STD=$(abspath std) OFIR_RUNTIME=$(abspath $(BUILD))/libofir_rt.a \
	    $(PYTHON) tools/run_std_tests.py --compiler $(BUILD)/stage1/ofir

test: stage0 runtime unit runner-selftest check-codes check-sizes check-docs check-format test-std compare-stages test-stage1 verify
	$(PYTHON) tests/runner/run_tests.py --compiler $(BUILD)/stage0/ofir

# Performance (docs/design/performance.md)
bench:
	$(MAKE) -C bootstrap release BUILD=$(abspath $(BUILD))/release
	$(PYTHON) bench/run_bench.py --compiler $(BUILD)/release/ofir

BENCH_PROGRAM_RUNS ?= 5
bench-programs: runtime
	$(MAKE) -C bootstrap release BUILD=$(abspath $(BUILD))/release
	$(STAGE_ENV) $(PYTHON) bench/run_programs.py --compiler $(BUILD)/release/ofir \
	    --runs $(BENCH_PROGRAM_RUNS) --json $(BUILD)/generated-code-results.json

BASE ?= origin/main
bench-compare:
	tools/bench_compare.sh $(BASE)

clean:
	rm -rf $(BUILD)
