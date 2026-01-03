.PHONY: format lint lint-fix lint-verbose clean-lint help

SOURCES := $(shell find src include -type f \( -name "*.c" -o -name "*.h" \))

format:
	@clang-format -i $(SOURCES)

lint:
	@echo "Running clang-tidy on $(words $(SOURCES)) files..."
	@clang-tidy -p build $(SOURCES) --quiet 2>&1 | grep -v "warnings generated" || true

lint-verbose:
	@echo "Running clang-tidy (verbose mode)..."
	@for file in $(SOURCES); do \
		echo "Analyzing: $$file"; \
		clang-tidy -p build "$$file"; \
	done

lint-fix:
	@echo "Running clang-tidy with auto-fix..."
	@clang-tidy -p build $(SOURCES) --fix-errors

lint-file:
	@test -n "$(FILE)" || (echo "Usage: make lint-file FILE=path/to/file.c" && exit 1)
	@clang-tidy -p build $(FILE)

compile-db:
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

clean-lint:
	@rm -rf build/compile_commands.json

help:
	@echo "Available targets:"
	@echo "  make format        - Run clang-format"
	@echo "  make lint          - Run clang-tidy (quiet mode, recommended)"
	@echo "  make lint-verbose  - Run clang-tidy with detailed output"
	@echo "  make lint-fix      - Auto-fix issues where possible"
	@echo "  make lint-file FILE=src/file.c - Check single file"
	@echo "  make compile-db    - Generate compile_commands.json"
	@echo "  make clean-lint    - Remove compile_commands.json"
