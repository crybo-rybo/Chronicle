# Chronicle development recipes. `just` alone lists them.

default:
    @just --list

# Configure + build (Debug, unit tests on)
build:
    cmake --preset dev
    cmake --build --preset dev -j

# Unit tests (no model required)
test: build
    ctest --preset dev

# Full local CI: build, unit tests, validate bundled examples
ci: test validate

# Validate the bundled example cartridges with the built console
validate: build
    ./build/src/chronicle validate --scenario examples/minimal
    ./build/src/chronicle validate --scenario examples/broken_wheel
    ./build/src/chronicle inspect --scenario examples/minimal > /dev/null
    @echo OK

# Live Ollama playthrough tests (needs a local model; override with CHRONICLE_MODEL)
integration:
    cmake --preset integration
    cmake --build --preset integration -j
    ctest --preset integration

# Play the minimal example (stub unless CHRONICLE_BASE_URL/CHRONICLE_MODEL are set)
play scenario="examples/minimal": build
    ./build/src/chronicle --scenario {{scenario}}

# Harness smoke demo, no cartridge
tiny: build
    ./build/src/chronicle --tiny

# Release build
release:
    cmake --preset release
    cmake --build --preset release -j

# clang-format over Chronicle sources
format:
    clang-format -i src/main.cpp src/chronicle/*.hpp src/chronicle/*.cpp \
        src/chronicle/*/*.hpp src/chronicle/*/*.cpp tests/*.hpp tests/*.cpp \
        tests/integration/*.cpp

clean:
    rm -rf build build-release
