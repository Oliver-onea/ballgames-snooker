ROOT := $(CURDIR)
MAMBA := $(ROOT)/.tools/bin/micromamba
MAMBA_ROOT := $(ROOT)/.micromamba
CPP_ENV := $(ROOT)/.cpp-env
BUILD_DIR := $(ROOT)/build/sdl
SDL_EXE := $(BUILD_DIR)/snooker_sdl
MAMBA_RUN := MAMBA_ROOT_PREFIX="$(MAMBA_ROOT)" "$(MAMBA)" run -p "$(CPP_ENV)"
DEFAULT_SDL_CXX := $(if $(wildcard $(CPP_ENV)/bin/x86_64-conda-linux-gnu-g++),x86_64-conda-linux-gnu-g++,)
SDL_CXX ?= $(DEFAULT_SDL_CXX)
CXX_CONFIG := $(if $(strip $(SDL_CXX)),-DCMAKE_CXX_COMPILER="$(SDL_CXX)",)

.PHONY: all build run smoke pygame web env clean distclean help

all: build

build: env
	$(MAMBA_RUN) cmake -S native/sdl -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=Release $(CXX_CONFIG)
	$(MAMBA_RUN) cmake --build "$(BUILD_DIR)" --parallel

run: build
	$(MAMBA_RUN) "$(SDL_EXE)"

smoke: build
	SDL_VIDEODRIVER=dummy $(MAMBA_RUN) "$(SDL_EXE)" --smoke-test

pygame:
	./scripts/run-pygame.sh

web:
	@printf 'Open %s/web/index.html in a browser.\n' "$(ROOT)"

env:
	@./scripts/bootstrap-cpp-env.sh

clean:
	rm -rf "$(BUILD_DIR)"

distclean: clean
	rm -rf "$(ROOT)/build" "$(ROOT)/.cpp-env" "$(ROOT)/.micromamba" "$(ROOT)/.tools" "$(ROOT)/.venv"

help:
	@printf 'make          Build native C++/SDL2 version\n'
	@printf 'make run      Build and run native C++/SDL2 version\n'
	@printf 'make smoke    Build and run headless SDL smoke test\n'
	@printf 'make pygame   Run Python/Pygame fallback\n'
	@printf 'make web      Print browser fallback path\n'
	@printf 'make clean    Remove C++ build output\n'
