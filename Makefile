# Developer entry points for Core and the independent UI adapters.
# LOGGER=production uses the pinned m-mdy-m/log.c dependency (default).
# LOGGER=stub is for offline development/tests only and is never release-safe.
BUILD_DIR ?= build
CMAKE ?= cmake
CMAKE_BUILD_TYPE ?= Release
LOGGER ?= production
GUI ?= OFF
JOBS ?= 0

ifeq ($(LOGGER),stub)
LOG_STUB := ON
DEPS_TARGET :=
else ifeq ($(LOGGER),production)
LOG_STUB := OFF
DEPS_TARGET := deps
else
$(error LOGGER must be either production or stub)
endif

ifeq ($(JOBS),0)
PARALLEL_FLAG := --parallel
else
PARALLEL_FLAG := --parallel $(JOBS)
endif

.PHONY: all deps configure build build-core build-tui run-tui tui \
        build-gui configure-gui run-gui gui test integration boundary lint structure \
        check package clean help

all: build

help:
	@printf '%s\n' \
	  'make build              Build Core + TUI preview + Headless (GUI=ON to add SDL3)' \
	  'make run-tui            Build and launch the terminal preview' \
	  'make build-gui          Build the SDL3 desktop preview (requires SDL3 >= 3.2)' \
	  'make run-gui            Build and launch the SDL3 desktop preview' \
	  'make test               Build and run all enabled tests' \
	  'make check              Run the repository CI checks' \
	  'LOGGER=stub             Offline test logger only; never use for release' \
	  'GUI=ON                  Include GUI-SDL in the normal build'

deps:
	bash scripts/install-deps.sh

configure: $(DEPS_TARGET)
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)" \
		-DCMAKE_PREFIX_PATH="$(CURDIR)/.deps/log" \
		-DCDIN_FRONTEND_USE_LOG_STUB=$(LOG_STUB) \
		-DCDIN_FRONTEND_BUILD_UI_TUI=ON \
		-DCDIN_FRONTEND_BUILD_UI_SDL=$(GUI) \
		-DCDIN_FRONTEND_BUILD_UI_HEADLESS=ON \
		-DCDIN_FRONTEND_BUILD_APPS=ON

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CMAKE_BUILD_TYPE)" $(PARALLEL_FLAG)

build-core: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CMAKE_BUILD_TYPE)" --target cdin_frontend_core $(PARALLEL_FLAG)

build-tui: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CMAKE_BUILD_TYPE)" \
		--target cdin_frontend_tui_preview $(PARALLEL_FLAG)

run-tui: build-tui
	"$(BUILD_DIR)/cdin_frontend_tui_preview"

tui: run-tui

build-gui: configure-gui
	$(CMAKE) --build "$(BUILD_DIR)-gui" --config "$(CMAKE_BUILD_TYPE)" \
		--target cdin_frontend_gui_sdl_preview $(PARALLEL_FLAG)

configure-gui: $(DEPS_TARGET)
	$(CMAKE) -S . -B "$(BUILD_DIR)-gui" -DCMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)" \
		-DCMAKE_PREFIX_PATH="$(CURDIR)/.deps/log" \
		-DCDIN_FRONTEND_USE_LOG_STUB=$(LOG_STUB) \
		-DCDIN_FRONTEND_BUILD_UI_TUI=OFF \
		-DCDIN_FRONTEND_BUILD_UI_SDL=ON \
		-DCDIN_FRONTEND_BUILD_UI_HEADLESS=OFF \
		-DCDIN_FRONTEND_BUILD_APPS=ON

run-gui: build-gui
	"$(BUILD_DIR)-gui/cdin_frontend_gui_sdl_preview"

gui: run-gui

test: build
	ctest --test-dir "$(BUILD_DIR)" -C "$(CMAKE_BUILD_TYPE)" --output-on-failure

integration: test

boundary:
	bash scripts/check-boundaries.sh

lint:
	bash scripts/lint-docs.sh

structure:
	psx check

check:
	bash scripts/ci-check.sh

package: check
	bash scripts/package-release.sh

clean:
	rm -rf "$(BUILD_DIR)" "$(BUILD_DIR)-gui" build-cmake build-revision dist
