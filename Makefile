# Goggles Build System (legacy entrypoint)
# This Makefile mirrors Pixi tasks for compatibility (Pixi remains the source of truth).

.PHONY: \
	all help _check-pixi \
	build build-all-presets test start init format clean distclean \
	app

PRESET ?= $(preset)
FLAGS ?= $(flags)
PIXI ?= pixi

preset ?= debug
flags ?=

_check-pixi:
	@command -v $(PIXI) >/dev/null 2>&1 || { \
		echo "error: '$(PIXI)' not found"; \
		echo "hint: install Pixi and run 'pixi run init' once"; \
		exit 127; \
	}

# Pixi task mirrors
all: build

build: _check-pixi
	@$(PIXI) run build -p "$(PRESET)"

build-all-presets: _check-pixi
	@$(PIXI) run build-all-presets $(FLAGS)

test: _check-pixi
	@$(PIXI) run test -p "$(PRESET)"

start: _check-pixi
	@$(PIXI) run start -p "$(PRESET)"

init: _check-pixi
	@$(PIXI) run init

format: _check-pixi
	@$(PIXI) run format

clean: _check-pixi
	@$(PIXI) run clean -p "$(PRESET)"

distclean: _check-pixi
	@$(PIXI) run distclean

help:
	@echo "Goggles Makefile (legacy)"
	@echo "This Makefile is a compatibility wrapper around Pixi tasks (Pixi is the source of truth)."
	@echo ""
	@echo "Usage:"
	@echo "  make build [PRESET=debug]"
	@echo "  make build-all-presets [FLAGS=\"--clean\"]"
	@echo "  make test [PRESET=debug]"
	@echo "  make start [PRESET=debug]"
	@echo "  make init"
	@echo "  make format"
	@echo "  make clean [PRESET=debug]"
	@echo "  make distclean"
	@echo ""
	@echo "Back-compat aliases:"
	@echo "  make app      == make build"
	@echo ""
	@echo "Make variables:"
	@echo "  PRESET=...  (or preset=...)  CMake preset name (default: debug)"
	@echo "  FLAGS=...   (or flags=...)   Extra flags for build-all-presets"
	@echo "  PIXI=...                 Pixi binary name/path (default: pixi)"
	@echo ""
	@echo "Pixi equivalents:"
	@echo "  make build PRESET=X              -> pixi run build -p X"
	@echo "  make build-all-presets FLAGS=... -> pixi run build-all-presets ..."
	@echo "  make test PRESET=X               -> pixi run test -p X"
	@echo "  make start PRESET=X              -> pixi run start -p X"
	@echo "  make clean PRESET=X              -> pixi run clean -p X"
	@echo "  make distclean                   -> pixi run distclean"

# Back-compat aliases
app: build
