# Shortcuts. The library builds on its own; the app arrives with M2.
BUILD ?= build

.PHONY: lib test golden lint format fuzz

lib:
	cmake -S libs/pixelfx-core -B $(BUILD)/lib -G Ninja -DPFX_WERROR=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build $(BUILD)/lib

test: lib
	ctest --test-dir $(BUILD)/lib --output-on-failure

golden: lib
	PFX_GOLDEN_UPDATE=1 $(BUILD)/lib/tests/pfx_tests -tc="golden*"

lint: lib
	find libs/pixelfx-core -path '*/build*' -prune -o \( -name '*.h' -o -name '*.cpp' \) -print | xargs clang-format --dry-run --Werror
	clang-tidy -p $(BUILD)/lib --config-file=.clang-tidy $$(find libs/pixelfx-core/src libs/pixelfx-core/tools -name '*.cpp')

format:
	find libs/pixelfx-core -path '*/build*' -prune -o \( -name '*.h' -o -name '*.cpp' \) -print | xargs clang-format -i

# Fuzz the document parser and the image loader for FUZZ_SECONDS each (clang, sanitizers).
# Seeds come from the examples and the golden images; crashes land in $(BUILD)/fuzz/crashes.
FUZZ_SECONDS ?= 120
fuzz:
	cmake -S libs/pixelfx-core -B $(BUILD)/fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo -DPFX_BUILD_FUZZERS=ON -DPFX_BUILD_TESTS=OFF -DPFX_BUILD_TOOLS=OFF
	cmake --build $(BUILD)/fuzz
	mkdir -p $(BUILD)/fuzz/crashes $(BUILD)/fuzz/corpus-document $(BUILD)/fuzz/corpus-load
	cp libs/pixelfx-core/examples/*.pfx* $(BUILD)/fuzz/corpus-document/
	cp libs/pixelfx-core/tests/golden/exposure.png $(BUILD)/fuzz/corpus-load/
	$(BUILD)/fuzz/fuzz/fuzz_document -max_total_time=$(FUZZ_SECONDS) -artifact_prefix=$(BUILD)/fuzz/crashes/ $(BUILD)/fuzz/corpus-document
	$(BUILD)/fuzz/fuzz/fuzz_load -max_total_time=$(FUZZ_SECONDS) -max_len=65536 -artifact_prefix=$(BUILD)/fuzz/crashes/ $(BUILD)/fuzz/corpus-load
