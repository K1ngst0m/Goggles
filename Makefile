# Goggles Build System (legacy entrypoint)
# This Makefile mirrors Pixi tasks for compatibility (Pixi remains the source of truth).

.PHONY: \
	all help _check-pixi \
	build build-i686 build-all-presets install-manifests test dev start init format clean distclean \
	app layer layer32 layer64

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
	@$(PIXI) run build "$(PRESET)"

build-i686: _check-pixi
	@$(PIXI) run build-i686 "$(PRESET)"

build-all-presets: _check-pixi
	@$(PIXI) run build-all-presets $(FLAGS)

install-manifests: _check-pixi
	@$(PIXI) run install-manifests "$(PRESET)"

test: _check-pixi
	@$(PIXI) run test "$(PRESET)"

dev: _check-pixi
	@$(PIXI) run dev "$(PRESET)"

start: _check-pixi
	@$(PIXI) run start

init: _check-pixi
	@$(PIXI) run init

format: _check-pixi
	@$(PIXI) run format

clean: _check-pixi
	@$(PIXI) run clean "$(PRESET)"

distclean: _check-pixi
	@$(PIXI) run distclean

help:
	@echo "Goggles Makefile (legacy)"
	@echo "This Makefile is a compatibility wrapper around Pixi tasks (Pixi is the source of truth)."
	@echo ""
	@echo "Usage:"
	@echo "  make build [PRESET=debug]"
	@echo "  make build-i686 [PRESET=debug]"
	@echo "  make build-all-presets [FLAGS=\"--clean --no-i686\"]"
	@echo "  make test [PRESET=debug]"
	@echo "  make install-manifests [PRESET=debug]"
	@echo "  make dev [PRESET=debug]"
	@echo "  make start"
	@echo "  make init"
	@echo "  make format"
	@echo "  make clean [PRESET=debug]"
	@echo "  make distclean"
	@echo ""
	@echo "Back-compat aliases:"
	@echo "  make app      == make build"
	@echo "  make layer64  == make build"
	@echo "  make layer32  == make build-i686"
	@echo "  make layer    == make build && make build-i686"
	@echo ""
	@echo "Make variables:"
	@echo "  PRESET=...  (or preset=...)  CMake preset name (default: debug)"
	@echo "  FLAGS=...   (or flags=...)   Extra flags for build-all-presets"
	@echo "  PIXI=...                 Pixi binary name/path (default: pixi)"
	@echo ""
	@echo "Pixi equivalents:"
	@echo "  make build PRESET=X              -> pixi run build X"
	@echo "  make build-i686 PRESET=X         -> pixi run build-i686 X"
	@echo "  make build-all-presets FLAGS=... -> pixi run build-all-presets ..."
	@echo "  make dev PRESET=X                -> pixi run dev X"
	@echo "  make test PRESET=X               -> pixi run test X"
	@echo "  make install-manifests PRESET=X  -> pixi run install-manifests X"
	@echo "  make clean PRESET=X              -> pixi run clean X"
	@echo "  make distclean                   -> pixi run distclean"

# Back-compat aliases
app: build
layer32: build-i686
layer64: build
layer: build build-i686
