# Chronicle

An LLM-driven text adventure game written in C++23. Chronicle uses local language models via [Zoo-Keeper](https://github.com/crybo-rybo/zoo-keeper) to power dynamic NPC conversations, emergent narrative, and world simulation — all running on your own hardware.

## Prerequisites

- CMake 3.25+
- C++23-capable compiler (GCC 13+, Clang 17+, or Apple Clang via Xcode 15+)
## Building

```bash
git clone https://github.com/crybo-rybo/chronicle.git
cd chronicle
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Running

```bash
./build/src/chronicle
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Project Structure

```
src/           C++ source code (engine, entities, AI, rendering, persistence)
data/          Scenario data files (JSON)
tests/         Unit and integration tests
extern/        Third-party dependencies (Zoo-Keeper submodule)
docs/          Design documents and specifications
```
