# Goggles Build System
# Wrapper for common CMake workflows

.PHONY: all app layer layer32 layer64 dev clean distclean help

PRESET ?= debug
MANIFEST_DIR := $(HOME)/.local/share/vulkan/implicit_layer.d

# Default: build main project
all: app

# Build main project (includes 64-bit layer)
app:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

# Build both layer architectures
layer: layer64 layer32

# 64-bit layer (from main build)
layer64: app

# 32-bit layer only
layer32:
	cmake --preset $(PRESET)-i686
	cmake --build --preset $(PRESET)-i686

# Development: app + both layers + install manifests
dev: app layer32
	@mkdir -p $(MANIFEST_DIR)
	@cp build/$(PRESET)/share/vulkan/implicit_layer.d/*.json $(MANIFEST_DIR)/
	@cp build/$(PRESET)-i686/share/vulkan/implicit_layer.d/*.json $(MANIFEST_DIR)/
	@echo "Manifests installed to $(MANIFEST_DIR)"

# Clean build directories for current preset
clean:
	rm -rf build/$(PRESET) build/$(PRESET)-i686

# Clean all build directories
distclean:
	rm -rf build

# Show available targets
help:
	@echo "Goggles Build System"
	@echo ""
	@echo "Targets:"
	@echo "  app       Build main project (default)"
	@echo "  layer     Build both 64-bit and 32-bit layers"
	@echo "  layer32   Build 32-bit layer only"
	@echo "  layer64   Build 64-bit layer (alias for app)"
	@echo "  dev       Build app + both layers, install manifests"
	@echo "  clean     Clean current preset build directories"
	@echo "  distclean Clean all build directories"
	@echo ""
	@echo "Variables:"
	@echo "  PRESET=debug|release  Build preset (default: debug)"
	@echo ""
	@echo "Examples:"
	@echo "  make dev              # Development setup"
	@echo "  make PRESET=release   # Release build"
	@echo "  make layer32          # Just 32-bit layer"
