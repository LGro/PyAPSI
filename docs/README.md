# Distribution Strategy Plans

This directory contains implementation plans for distributing PyAPSI across platforms.

## Plans

- [Strategy 4: Hybrid — Source Distribution + Pre-built Wheels](./strategy-4-hybrid-wheels-sdist.md) — **IMPLEMENTED**
- [Strategy 5: Conda-forge Distribution](./strategy-5-conda-forge.md) — Not started

## Quick Comparison

| | Strategy 4 (Hybrid) | Strategy 5 (Conda-forge) |
|---|---|---|
| **Target users** | pip users (broadest) | conda users (data science, HPC) |
| **Install command** | `pip install apsi` | `conda install -c conda-forge pyapsi` |
| **CPU optimization** | sdist only (`--no-binary`) | Always (per-platform CI build) |
| **Build system** | vcpkg (self-bootstrapping) | conda-forge packages |
| **Effort** | ~3-4 days | ~2 weeks + review time |
| **Prerequisite work** | APSI git submodule (done) | Dependency feedstocks (SEAL, Kuku) |
| **Can be done independently** | Yes (done) | Partially (needs APSI on conda-forge first) |

## Recommended Order

1. **Strategy 4** — DONE. Self-bootstrapping vcpkg works immediately for pip users.
2. **Strategy 5** — Future. Once APSI and its dependencies are available on conda-forge.

## Current State

Strategy 4 has been implemented with the following changes:

- `setup.py`: Self-bootstrapping vcpkg logic
- `CMakeLists.txt`: Simplified, uses vcpkg toolchain
- `pyproject.toml`: cibuildwheel configuration for Linux/macOS/Windows
- `MANIFEST.in`: sdist completeness
- `.github/workflows/build-wheels.yml`: Wheel + sdist CI
- `.github/workflows/cicd.yaml`: Multi-platform testing
- `external/apsi`: Git submodule pointing to APSI v0.12.0
- `README.md`: Updated installation instructions
