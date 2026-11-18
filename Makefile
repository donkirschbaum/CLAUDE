#!/usr/bin/make -f
# Anu - top-level Makefile
#   make            -> native build (Linux / macOS host)
#   make windows    -> cross-compile 64-bit Windows from Linux (mingw-w64)
#   make macos      -> universal (x86_64 + arm64) build on macOS
# Output goes to ./bin

all: plugins

plugins:
	$(MAKE) -C plugins/Anu

windows:
	$(MAKE) -C plugins/Anu mingw64

macos:
	$(MAKE) -C plugins/Anu macos-universal-10.15

test:
	$(CXX) -std=c++17 -O2 -o build/render_test tests/render_test.cpp src/engine.cpp src/anu_core.cpp
	$(CXX) -std=c++17 -O2 -o build/hostsync_test tests/hostsync_test.cpp src/engine.cpp src/anu_core.cpp
	./build/render_test build/test.wav
	./build/hostsync_test

clean:
	$(MAKE) clean -C plugins/Anu
	rm -rf bin build dpf/build

.PHONY: all plugins windows macos test clean
