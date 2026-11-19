# Transient Sculptor

An adaptive, frequency-dependent transient shaper for macOS, built with
[JUCE](https://juce.com) on top of the [Pamplejuce](https://github.com/sudara/pamplejuce) template.

Transient Sculptor is inspired by [oeksound spiff](https://oeksound.com/manuals/spiff/):
instead of compressing the whole signal, it detects transient material and applies
gain reduction (or boost) only to the frequency ranges that contain it, leaving the
sustained part of the sound intact. Originally aimed at vocal cleanup — clicks, mouth
noises, harsh consonants — it works on drums, acoustic instruments and full mixes too.

## Formats

| Format | Platform |
| --- | --- |
| AU / AUv3 | macOS |
| VST3 | macOS |
| CLAP | macOS |
| Standalone | macOS |

Target: macOS 10.14+, universal binary (arm64 + x86_64) in CI.

## Getting started

```bash
# Fetch JUCE and other submodules (once)
git submodule update --init --recursive

# Configure
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build Builds --config Debug

# Test
ctest --test-dir Builds --verbose --output-on-failure
```

Builds are Debug by default for development. Use `-DCMAKE_BUILD_TYPE=Release` (or
`RelWithDebInfo`) when you need to check CPU usage / audio performance.

## Project layout

- `source/` — plugin processor and editor
- `tests/` — Catch2 tests (`ctest` runs them)
- `benchmarks/` — Catch2 benchmarks
- `docs/Spiff-Analysis.md` — breakdown of spiff's parameters and behaviour (design reference)
- `cmake/` — Pamplejuce CMake modules
- `modules/` — git submodules (melatonin_inspector, clap-juce-extensions)
- `JUCE/` — JUCE framework (git submodule)
- `packaging/` — macOS installer resources

## CI

`.github/workflows/build_and_test.yml` builds, tests and validates the plugin with
[pluginval](https://github.com/Tracktion/pluginval) on `macos-14`, then packages an
unsigned `.pkg`. Code signing/notarization and the Linux/Windows matrix entries are
commented out until they are needed — see the comments in the workflow and in
`CLAUDE.md`.

## Status

Early development: the template has been personalized, the DSP and UI for the adaptive
transient shaping are next. See `docs/Spiff-Analysis.md` for the feature plan.
