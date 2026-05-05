# Scripts

Small wrappers for common local development tasks.

```bash
scripts/build.sh          # configure and build with the debug preset
scripts/format.sh         # format tracked C++ files
scripts/format.sh --check # verify tracked C++ formatting
scripts/test.sh           # build and run the default non-model tests
```

All scripts run from the repository root regardless of the caller's current
directory. Set `CHRONICLE_PRESET=debug-logging` or pass `--preset debug-logging`
to use the logging preset.
