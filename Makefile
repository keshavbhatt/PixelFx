# Shortcuts. The library builds on its own; the app arrives with M2.
BUILD ?= build

.PHONY: lib test golden lint format

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
