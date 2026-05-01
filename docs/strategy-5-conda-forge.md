# Strategy 5: Conda-forge Distribution

## Overview

Package PyAPSI for conda-forge, leveraging its mature C++ build infrastructure. Conda-forge handles native dependencies (SEAL, Kuku, etc.) as separate conda packages, eliminating the need for vcpkg or vendored sources. Platform-specific builds are standard practice in conda-forge, and CI runners compile packages for their target architecture.

---

## Phase 1: Verify Dependency Availability on conda-forge

### 1.1 Check existing conda-forge packages

Search `conda-forge` for each dependency:

| Dependency | Package name | Status |
|------------|-------------|--------|
| SEAL | `libseal` or `seal` | Check availability |
| Kuku | `kuku` | Check availability |
| log4cplus | `log4cplus` | Likely available |
| cppzmq | `cppzmq` | Available |
| zeromq | `zeromq` | Available |
| flatbuffers | `flatbuffers` | Available |
| jsoncpp | `jsoncpp` | Available |
| Microsoft GSL | `ms-gsl` | Available |

Run:
```bash
conda search -c conda-forge <package-name>
```

### 1.2 Handle missing packages

If a dependency is not on conda-forge (most likely SEAL and Kuku):

**Option A — Submit to conda-forge** (recommended):
1. Create a `feedstock` repo for the missing package
2. Submit to `conda-forge/staged-recipes`
3. Wait for review and merge
4. Package becomes available to all conda-forge users

**Option B — Use a custom channel** (temporary):
Host the missing packages on your own Anaconda channel and reference it in the recipe. Not ideal for long-term maintenance.

**Option C — Bundle in the PyAPSI recipe**:
Use the source bundle approach within the PyAPSI recipe itself. Works but loses the benefit of shared dependency management.

### 1.3 Determine SEAL package naming

SEAL may be packaged as `libseal` (C++ library) or `seal`. Check the exact package name and version available. The conda-forge package must match the version that APSI expects.

**Acceptance criteria**:
- All dependencies are available on conda-forge, or feedstock PRs are submitted for missing ones
- Version compatibility between APSI and available dependency versions is confirmed

---

## Phase 2: Create the Conda Recipe

### 2.1 Set up feedstock repository

The conda-forge way is to create a feedstock via `staged-recipes`:

1. Fork `conda-forge/staged-recipes`
2. Create `recipes/pyapsi/` directory
3. Add `meta.yaml` and `build.sh` / `bld.bat`
4. Submit a PR to `staged-recipes`
5. After merge, a `pyapsi-feedstock` repo is auto-created

Alternatively, if you want to host the recipe yourself first (for testing):

```
pyapsi-feedstock/
├── recipe/
│   ├── meta.yaml
│   ├── build.sh
│   └── bld.bat
├── LICENSE
└── README.md
```

### 2.2 Write meta.yaml

```yaml
{% set name = "pyapsi" %}
{% set version = "0.1.3" %}

package:
  name: {{ name|lower }}
  version: {{ version }}

source:
  url: https://pypi.org/packages/source/{{ name[0] }}/{{ name }}/{{ name }}-{{ version }}.tar.gz
  sha256: <compute from sdist>
  # Or use git source:
  # git_url: https://github.com/LGro/PyAPSI.git
  # git_rev: v{{ version }}

build:
  number: 0
  script: {{ PYTHON }} -m pip install . -vv

requirements:
  build:
    - {{ compiler('c') }}
    - {{ compiler('cxx') }}
    - cmake >=3.13.4
    - make  # [unix]
    - ninja
    - python
    - cross-python_{{ target_platform }}  # [build_platform != target_platform]
    - pybind11
  host:
    - python
    - pip
    - pybind11
    - seal  # or libseal
    - kuku
    - log4cplus
    - cppzmq
    - zeromq
    - flatbuffers
    - jsoncpp
    - ms-gsl
  run:
    - python

test:
  imports:
    - apsi
  requires:
    - pytest
  source_files:
    - tests/
  commands:
    - pytest tests/

about:
  home: https://github.com/LGro/PyAPSI
  summary: Python wrapper for asymmetric private set intersection (APSI)
  license: MIT
  license_file: LICENSE

extra:
  recipe-maintainers:
    - LGro
```

### 2.3 Write build.sh (Linux/macOS)

```bash
#!/bin/bash
set -ex

# CMake needs to find conda-provided dependencies
export CMAKE_PREFIX_PATH=$PREFIX

# Build with pip (setup.py handles CMake)
$PYTHON -m pip install . --no-deps -vv
```

### 2.4 Write bld.bat (Windows)

```batch
set CMAKE_PREFIX_PATH=%LIBRARY_PREFIX%
%PYTHON% -m pip install . --no-deps -vv
if errorlevel 1 exit 1
```

### 2.5 Handle CMake dependency resolution

The key challenge: `setup.py` currently expects vcpkg. The conda build needs CMake to find dependencies via `find_package` from the conda environment.

Two approaches:

**Approach A — Patch setup.py for conda**:
Remove the vcpkg toolchain line and let CMake use `find_package`:

```python
# In setup.py, detect conda build environment
if os.environ.get('CONDA_BUILD'):
    # Don't pass vcpkg toolchain, let CMake find packages in $PREFIX
    cmake_args = [
        f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
        f"-DPYTHON_EXECUTABLE={sys.executable}",
        f"-DCMAKE_BUILD_TYPE={cfg}",
        f"-DCMAKE_PREFIX_PATH={os.environ.get('PREFIX', '')}",
    ]
```

**Approach B — Use a separate CMakeLists for conda**:
Create `CMakeLists.conda.txt` that uses `find_package` instead of `FetchContent`/`add_subdirectory`.

**Recommendation**: Approach A — it's a small change and keeps a single CMakeLists.txt.

### 2.6 Update CMakeLists.txt for find_package support

Modify `CMakeLists.txt` to support both vendored (sdist) and system-installed (conda) dependencies:

```cmake
option(USE_SYSTEM_DEPS "Use system-installed dependencies" OFF)

if(USE_SYSTEM_DEPS)
    find_package(SEAL REQUIRED)
    find_package(Kuku REQUIRED)
    find_package(log4cplus REQUIRED)
    find_package(cppzmq REQUIRED)
    find_package(Flatbuffers REQUIRED)
    find_package(jsoncpp REQUIRED)
    add_subdirectory(external/apsi/)
else()
    # FetchContent / vendored approach (for sdist)
    add_subdirectory(external/apsi/)
endif()

pybind11_add_module(_pyapsi src/main.cpp)
target_link_libraries(_pyapsi PRIVATE pybind11::module apsi)
```

**Acceptance criteria**:
- Recipe builds locally with `conda-build`
- All dependencies resolve from conda-forge channel
- Tests pass in the conda build environment

---

## Phase 3: Test Locally with conda-build

### 3.1 Install conda-build

```bash
conda install conda-build conda-verify
```

### 3.2 Build locally

```bash
conda build recipe/ -c conda-forge
```

This will:
1. Download the source tarball (or clone the repo)
2. Create a clean conda environment
3. Install all build/host dependencies
4. Run the build script
5. Run tests
6. Produce a `.conda` or `.tar.bz2` package

### 3.3 Test installation

```bash
conda install --use-local pyapsi -c conda-forge
python -c "from apsi import LabeledServer, LabeledClient; print('OK')"
```

### 3.4 Build for all platforms

Conda-forge's CI (Azure Pipelines via feedstock) handles cross-platform builds automatically. But locally you can only build for your current platform.

**Acceptance criteria**:
- `conda build` succeeds on at least one platform
- Installed package imports and passes tests

---

## Phase 4: Submit to conda-forge

### 4.1 Submit to staged-recipes

1. Fork `https://github.com/conda-forge/staged-recipes`
2. Create PR with your recipe in `recipes/pyapsi/`
3. CI will build on Linux, macOS, and Windows
4. Address reviewer feedback
5. Once merged, `pyapsi-feedstock` is auto-created

### 4.2 Feedstock maintenance

After merge, you become a maintainer of `conda-forge/pyapsi-feedstock`. Key files:

```
pyapsi-feedstock/
├── recipe/
│   ├── meta.yaml
│   ├── build.sh
│   └── bld.bat
└── .github/workflows/  # auto-generated
```

### 4.3 Set up version updates

Configure `conda-forge` bot for automatic version bumping:

Add to `meta.yaml`:
```yaml
extra:
  recipe-maintainers:
    - LGro
  feedstock-name: pyapsi
```

The conda-forge bot will:
- Detect new PyPI releases
- Auto-open PRs with updated version and SHA256
- Trigger CI builds

### 4.4 Configure PyPI source

If sourcing from PyPI (recommended), the bot works automatically. If sourcing from GitHub, configure:

```yaml
source:
  url: https://github.com/LGro/PyAPSI/archive/refs/tags/v{{ version }}.tar.gz
```

**Acceptance criteria**:
- PR merged to staged-recipes
- Package installable via `conda install -c conda-forge pyapsi`
- Auto-version-update bot is active

---

## Phase 5: CI/CD Integration

### 5.1 Update PyAPSI GitHub Actions

Add a conda-forge publishing step to the existing CI, or rely on the conda-forge bot. The bot approach is preferred — it decouples PyPI releases from conda releases.

### 5.2 Release workflow

```
1. Bump version in setup.py/pyproject.toml
2. Push tag → GitHub Actions builds and publishes to PyPI
3. conda-forge bot detects new PyPI version → opens PR on feedstock
4. Feedstock CI builds conda packages for all platforms
5. PR auto-merges (or manual review) → packages available on conda-forge
```

### 5.3 Badge

Add to README:
```markdown
[![Conda Version](https://img.shields.io/conda/vn/conda-forge/pyapsi.svg)](https://anaconda.org/conda-forge/pyapsi)
[![Conda Platforms](https://img.shields.io/conda/pn/conda-forge/pyapsi.svg)](https://anaconda.org/conda-forge/pyapsi)
```

---

## Effort Estimate

| Phase | Effort | Risk |
|-------|--------|------|
| 1. Dependency availability | 1-3 days | Medium — SEAL/Kuku may need new feedstocks |
| 2. Create recipe | 1-2 days | Low-Medium — CMake integration needs care |
| 3. Local testing | 1 day | Low |
| 4. Submit to conda-forge | 1-2 days (review time varies) | Medium — review process can take weeks |
| 5. CI/CD integration | 0.5 days | Low |
| **Total** | **~1-2 weeks** (plus review wait time) | |

---

## Known Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| SEAL not on conda-forge | Submit `libseal` feedstock to staged-recipes first |
| Kuku not on conda-forge | Submit `kuku` feedstock; it's a smaller package, easier review |
| APSI CMake expects vcpkg | Add `USE_SYSTEM_DEPS` option (Phase 2, Step 6) |
| Conda-forge review takes weeks | Start dependency feedstocks early; they have no PyAPSI dependency |
| Windows build fails | Conda-forge has strong Windows support; debug with `conda-build` locally if possible |
| Dependency version mismatch | Pin compatible versions in `meta.yaml`; coordinate with upstream |

---

## Dependency Feedstock Prerequisites

If SEAL and/or Kuku are not on conda-forge, create these feedstocks first (in order):

### libseal feedstock

```yaml
# recipe/meta.yaml
{% set name = "libseal" %}
{% set version = "4.0.0" %}

package:
  name: {{ name|lower }}
  version: {{ version }}

source:
  url: https://github.com/microsoft/SEAL/archive/refs/tags/v{{ version }}.tar.gz
  sha256: <compute>

build:
  number: 0

requirements:
  build:
    - {{ compiler('c') }}
    - {{ compiler('cxx') }}
    - cmake
  host:
    - ms-gsl
  run:
    - ms-gsl

test:
  commands:
    - test -f $PREFIX/lib/libseal*  # [unix]

about:
  home: https://github.com/microsoft/SEAL
  summary: Microsoft SEAL Homomorphic Encryption Library
  license: MIT
```

### kuku feedstock

```yaml
# recipe/meta.yaml
{% set name = "kuku" %}
{% set version = "2.1.0" %}

package:
  name: {{ name|lower }}
  version: {{ version }}

source:
  url: https://github.com/microsoft/Kuku/archive/refs/tags/v{{ version }}.tar.gz
  sha256: <compute>

build:
  number: 0

requirements:
  build:
    - {{ compiler('c') }}
    - {{ compiler('cxx') }}
    - cmake

test:
  commands:
    - test -f $PREFIX/lib/libkuku*  # [unix]

about:
  home: https://github.com/microsoft/Kuku
  summary: Keyword Lookup Table library
  license: MIT
```

---

## Comparison with Strategy 4

| Aspect | Strategy 4 (Hybrid) | Strategy 5 (Conda-forge) |
|--------|---------------------|--------------------------|
| User base | All pip users | Conda users (data science, HPC) |
| Install speed | Fast (wheels) / Slow (sdist) | Fast (pre-built) |
| CPU optimization | sdist only | Always optimized (per-platform build) |
| C++ deps management | FetchContent/vendored | Conda packages |
| Maintenance burden | Medium (CI config) | Medium (feedstock + dep feedstocks) |
| Windows support | Yes (cibuildwheel) | Yes (native conda-forge CI) |
| macOS ARM support | Yes (M1 runners) | Yes (native conda-forge CI) |
| Platform-specific builds | Limited (baseline wheels) | Full (each platform compiles) |
| Time to first release | ~1 week | ~2 weeks + review time |

---

## Recommendation

Do **both** strategies. They are complementary:

1. **Strategy 4 first** — it unblocks pip users and the FetchContent migration benefits both strategies
2. **Strategy 5 second** — once the CMakeLists.txt supports `find_package` (needed for Strategy 4's sdist anyway), the conda recipe is straightforward

The `USE_SYSTEM_DEPS` CMake option created for Strategy 5 also helps Strategy 4 users who have system-installed SEAL/Kuku and want to skip the vendored build.
