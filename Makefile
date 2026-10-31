# Top-level build for the Astra repository.
# See docs/design/toolchain.md, section 7.

BUILD  ?= build
PYTHON ?= python3

.PHONY: all stage0 runtime unit runner-selftest test check-codes check-sizes clean

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

test: stage0 runtime unit runner-selftest check-codes check-sizes
	$(PYTHON) tests/runner/run_tests.py --compiler $(BUILD)/stage0/astra

clean:
	rm -rf $(BUILD)
