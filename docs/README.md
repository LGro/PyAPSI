# Distribution Strategy Plans

This directory contains implementation plans for distributing PyAPSI across platforms.

## Plans

- [Strategy 4: Hybrid — Source Distribution + Pre-built Wheels](./strategy-4-hybrid-wheels-sdist.md) — **PARTIALLY IMPLEMENTED (sdist only)**
- [Strategy 5: Conda-forge Distribution](./strategy-5-conda-forge.md) — Not started

## Current State

### Implemented: Source Distribution (sdist)

The sdist workflow is active and:
- Builds a source tarball on every push/PR
- Tests installation from the tarball on Python 3.14 (Ubuntu + macOS)
- Publishes to PyPI on tags only after successful install tests

Users install with:
```bash
pip install apsi --no-binary apsi
```

This triggers a full local build with native CPU optimizations. Build time is ~5-15 minutes.

### Deferred: Pre-built Wheels

Wheel building was attempted but blocked by several issues. The work is documented below for future reference.

#### Issues encountered

1. **flatc segfault in manylinux containers**: The flatbuffers compiler (`flatc`) from vcpkg segfaults when running in manylinux Docker containers, particularly on ARM64 hosts running x86_64 emulation. This happens during APSI's CMake configuration when it tries to generate C++ headers from `.fbs` schema files.

2. **SEAL version mismatch**: vcpkg provides SEAL 4.3.0, but APSI's CMakeLists.txt requires `find_package(SEAL 4.1)`. Workaround: patch the version check at build time.

3. **pybind11 CMake compatibility**: pybind11 v2.9.2 is incompatible with CMake 3.31+. Fixed by upgrading to v2.13.6.

4. **vcpkg bootstrap permissions**: Zip extraction doesn't preserve execute permissions on Unix. Fixed with `os.chmod()`.

5. **macOS `/opt/vcpkg` permissions**: CI runners can't write to `/opt/`. Fixed by using `/tmp/vcpkg`.

6. **Windows vcpkg pre-installed**: Windows runners already have vcpkg at `C:\vcpkg`. Fixed by checking for existence before cloning.

#### Potential solutions for future wheel builds

1. **Pre-generate flatbuffers headers**: Commit the `*_generated.h` files to the APSI submodule or generate them in a separate build step before the manylinux container runs. This eliminates the need for `flatc` during the wheel build.

2. **Use a custom manylinux image**: Pre-build vcpkg + all dependencies + flatbuffers headers into a custom Docker image, avoiding the bootstrap and flatc issues entirely.

3. **Build wheels on native runners**: Avoid cross-architecture emulation by using native x86_64 runners for Linux wheels.

4. **Consider scikit-build-core**: A modern alternative to setuptools + CMake that handles many of these edge cases better.

#### Files to restore for wheel building

The following files contain wheel-building configuration that can be reactivated:

- `pyproject.toml`: Contains `[tool.cibuildwheel]` sections (currently kept but not used)
- `setup.py`: Contains self-bootstrapping vcpkg logic (works for sdist installs)
- `.github/workflows/build-wheels.yml`: Currently sdist-only, can be extended back to wheels

## Quick Comparison

| | Strategy 4 (Hybrid) | Strategy 5 (Conda-forge) |
|---|---|---|
| **Target users** | pip users (broadest) | conda users (data science, HPC) |
| **Install command** | `pip install apsi --no-binary apsi` | `conda install -c conda-forge pyapsi` |
| **CPU optimization** | Always (builds locally) | Always (per-platform CI build) |
| **Build system** | vcpkg (self-bootstrapping) | conda-forge packages |
| **Effort** | sdist: done, wheels: blocked | ~2 weeks + review time |
| **Prerequisite work** | APSI git submodule (done) | Dependency feedstocks (SEAL, Kuku) |

## Recommended Order

1. **Strategy 4 sdist** — DONE. Works for all platforms with native optimizations.
2. **Strategy 4 wheels** — Deferred until flatc/pre-generation issues are resolved.
3. **Strategy 5** — Future. Once APSI and its dependencies are available on conda-forge.
