# Strategy 5: Conda-forge Distribution

## Status: Not started

Packaging PyAPSI for conda-forge would provide an alternative distribution channel for conda users (data science, HPC). This document outlines the approach and prerequisites.

## Why conda-forge

- **No C++ compiler required**: Conda-forge provides pre-built packages for SEAL, Kuku, and other C++ dependencies
- **Native platform builds**: CI runners compile for their target architecture, avoiding cross-architecture emulation issues
- **Separate dependency management**: C++ dependencies are managed as conda packages, not bundled or built via vcpkg

## Prerequisites

### Dependency availability on conda-forge

| Dependency | Package name | Status |
|------------|-------------|--------|
| SEAL | `libseal` or `seal` | Needs verification |
| Kuku | `kuku` | Needs verification |
| log4cplus | `log4cplus` | Likely available |
| cppzmq | `cppzmq` | Available |
| zeromq | `zeromq` | Available |
| flatbuffers | `flatbuffers` | Available |
| jsoncpp | `jsoncpp` | Available |
| Microsoft GSL | `ms-gsl` | Available |

If SEAL and/or Kuku are not on conda-forge, feedstocks for those packages would need to be created first.

## Approach

### 1. Create conda recipe

```
pyapsi-feedstock/
├── recipe/
│   ├── meta.yaml
│   ├── build.sh
│   └── bld.bat
├── LICENSE
└── README.md
```

### 2. Handle CMake dependency resolution

The current `setup.py` uses vcpkg for dependency management. For conda builds, CMake needs to find dependencies via `find_package` from the conda environment. This would require either:

- Detecting `CONDA_BUILD` in `setup.py` and skipping the vcpkg toolchain
- Adding a `USE_SYSTEM_DEPS` CMake option

### 3. Submit to conda-forge

1. Fork `conda-forge/staged-recipes`
2. Create PR with recipe in `recipes/pyapsi/`
3. CI builds on Linux, macOS, and Windows
4. After merge, `pyapsi-feedstock` repo is auto-created

### 4. Auto-version updates

The conda-forge bot detects new PyPI releases and auto-opens PRs with updated version and SHA256.

## Release workflow

```
1. Bump version in setup.py/pyproject.toml
2. Push tag → GitHub Actions builds and publishes to PyPI
3. conda-forge bot detects new PyPI version → opens PR on feedstock
4. Feedstock CI builds conda packages for all platforms
5. PR auto-merges → packages available on conda-forge
```

## Estimated effort

| Phase | Effort | Risk |
|-------|--------|------|
| Verify dependency availability | 1–2 days | Medium — SEAL/Kuku may need new feedstocks |
| Create recipe | 1–2 days | Low–Medium — CMake integration needs care |
| Submit to conda-forge | 1–2 days (plus review time) | Medium — review can take weeks |

## Comparison with sdist

| Aspect | sdist (current) | conda-forge (future) |
|--------|----------------|---------------------|
| Target users | pip users | conda users |
| Install command | `pip install apsi` | `conda install -c conda-forge pyapsi` |
| Build time | 5–15 min (local) | Pre-built (fast) |
| CPU optimization | Native | Per-platform CI build |
| C++ deps management | vcpkg (self-bootstrapping) | conda-forge packages |
| Maintenance | Low | Medium (feedstock upkeep) |
