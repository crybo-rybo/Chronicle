# Release Checklist

Use this checklist before tagging `v1.0.0`.

## Required

- [ ] Confirm Linux and macOS CI are green for the release commit.
- [ ] Check formatting:

  ```bash
  scripts/format.sh --check
  ```

- [ ] Run the warning-as-error release build:

  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release -DCHRONICLE_WERROR=ON
  cmake --build build --parallel
  ```

- [ ] Run the default non-model test suite:

  ```bash
  ctest --test-dir build --output-on-failure
  ```

- [ ] Validate shipped scenarios:

  ```bash
  ./build/src/chronicle validate --scenario data
  ./build/src/chronicle validate --scenario examples/minimal_scenario
  ./build/src/chronicle validate --scenario examples/lighthouse_veil
  ```

- [ ] Smoke-test sample playthroughs:

  ```bash
  printf 'help\nquit\n' | ./build/src/chronicle --scenario data
  printf 'take ledger\neast\nquit\n' | ./build/src/chronicle --scenario examples/minimal_scenario
  ```

- [ ] Review README, authoring guide, schema reference, JSON schemas, and changelog/release notes for version and command drift.
- [ ] Confirm no generated planning files remain:

  ```bash
  test ! -d docs/superpowers || find docs/superpowers -maxdepth 1 -type f
  ```

- [ ] Confirm the worktree is clean:

  ```bash
  git status --short --branch
  ```

## Optional Model-Gated Checks

- [ ] Run model-backed integration tests when a local GGUF model is available:

  ```bash
  cmake -B build-integration -DCMAKE_BUILD_TYPE=Release -DCHRONICLE_INTEGRATION_TESTS=ON
  cmake --build build-integration --parallel
  ZOO_INTEGRATION_MODEL=/path/to/model.gguf ctest --test-dir build-integration --output-on-failure -R NpcConversationIntegrationTest
  ```

- [ ] Run one AddressSanitizer smoke build/session where practical:

  ```bash
  cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCHRONICLE_BUILD_TESTS=OFF -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
  cmake --build build-asan --target chronicle --parallel
  printf 'help\nquit\n' | ./build-asan/src/chronicle --scenario data
  ```
