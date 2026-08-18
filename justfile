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

# Full local CI: formatting, warning-clean build, unit tests, examples, release install smoke
ci: check-format
    cmake --preset ci
    cmake --build --preset ci -j
    ctest --preset ci
    ./build-ci/src/chronicle validate --scenario examples/minimal
    ./build-ci/src/chronicle validate --scenario examples/broken_wheel
    ./build-ci/src/chronicle inspect --scenario examples/minimal > /dev/null
    cmake --preset release
    cmake --build --preset release -j
    cmake --install build-release --prefix build-install-smoke
    (cd build-install-smoke && printf 'quit\n' | ./bin/chronicle > /dev/null)
    cmake -E remove_directory build-install-smoke

# Verify formatting without modifying the worktree
check-format:
    clang-format --dry-run --Werror src/main.cpp src/chronicle/*.hpp src/chronicle/*.cpp \
        src/chronicle/*/*.hpp src/chronicle/*/*.cpp tests/*.hpp tests/*.cpp \
        tests/integration/*.cpp

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
    cmake -E remove_directory build
    cmake -E remove_directory build-ci
    cmake -E remove_directory build-install-smoke
    cmake -E remove_directory build-linux-ci
    cmake -E remove_directory build-release
