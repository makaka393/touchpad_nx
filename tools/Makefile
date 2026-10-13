# Host-side tools. `make` builds the previewer; `make packs` draws every layout of every pack
# in ../packs into build/ as PNG; `make docs` refreshes the pictures in ../docs.
# The PPM -> PNG step needs ImageMagick.
CXX      ?= g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -isystem ../third_party
CORE     := $(wildcard ../core/*.cpp)

.PHONY: all packs docs clean
all: build/preview

build/preview: preview.cpp $(CORE) $(wildcard ../core/*.hpp) $(wildcard ../packs/*.ini)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) preview.cpp $(CORE) -o $@

packs: build/preview
	@for pack in ../packs/*.ini; do \
		name=$$(basename $$pack .ini); \
		for layout in $$(grep -o '^\[[a-z0-9_-]*\]' $$pack | tr -d '[]' | grep -v -E '^(pack|state)$$'); do \
			./build/preview build/$$name-$$layout.ppm pack $$pack $$layout >/dev/null || exit 1; \
			magick build/$$name-$$layout.ppm build/$$name-$$layout.png && rm build/$$name-$$layout.ppm; \
			echo "build/$$name-$$layout.png"; \
		done; \
	done

# One picture: $(call shot,<name>,<preview arguments>)
shot = ./build/preview build/$(1).ppm $(2) >/dev/null && magick build/$(1).ppm ../docs/$(1).png && rm build/$(1).ppm

docs: build/preview
	@$(call shot,preview-idle,play)
	@$(call shot,preview-menu,menu)
	@$(call shot,preview-editor,editor)
	@$(call shot,mk8d-menu,pack ../packs/mk8d.ini menu)
	@$(call shot,mk8d-race,pack ../packs/mk8d.ini race)
	@$(call shot,lm3-play,pack ../packs/lm3.ini play)
	@$(call shot,lm3-menu,pack ../packs/lm3.ini menu)
	@$(call shot,burnout-drive,pack ../packs/burnout.ini drive)
	@$(call shot,icons-1,pack icons.ini sheet1)
	@$(call shot,icons-2,pack icons.ini sheet2)
	@$(call shot,icons-3,pack icons.ini sheet3)
	@echo "docs/*.png refreshed"

clean:
	rm -rf build
