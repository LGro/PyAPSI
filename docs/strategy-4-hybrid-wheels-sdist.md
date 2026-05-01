# Strategy 4: Hybrid — Source Distribution + Pre-built Wheels

## Overview

Publish both pre-built wheels (with conservative CPU flags for maximum compatibility) and a source distribution (sdist) that builds with native optimizations. Users get fast installs by default, while power users can opt into optimized builds.

**Implementation approach**: Use vcpkg as a self-bootstrapping build dependency. During `pip install`, setup.py automatically downloads and bootstraps vcpkg if not already present, then uses it to build all C++ dependencies (SEAL, Kuku, log4cplus, cppzmq, flatbuffers, jsoncpp) and APSI itself.

---

## Phase 1: Self-Bootstrapping vcpkg in setup.py

### 1.1 How it works

When a user runs `pip install apsi`:

1. `setup.py` checks for vcpkg in this order:
   - `VCPKG_ROOT_DIR` environment variable (user-provided)
   - `_vcpkg/linux/`, `_vcpkg/macos/`, or `_vcpkg/windows/` (platform-specific cache)
2. If not found, setup.py:
   - Downloads vcpkg source from GitHub
   - Runs `bootstrap-vcpkg.sh` (or `.bat` on Windows)
   - Installs all required dependencies: `seal[no-throw-tran]`, `kuku`, `log4cplus`, `cppzmq`, `flatbuffers`, `jsoncpp`
3. CMake builds with the vcpkg toolchain file
4. The `_vcpkg/` directory is cached for subsequent builds

### 1.2 Key files changed

- **`setup.py`**: Added `_bootstrap_vcpkg()`, `_get_vcpkg_dir()`, `_vcpkg_exists()` functions. The `CMakeBuild` class now auto-bootstraps vcpkg if needed.
- **`CMakeLists.txt`**: Simplified back to using vcpkg toolchain + `add_subdirectory(external/apsi/)`. pybind11 uses FetchContent.
- **`.gitignore`**: Added `_vcpkg/` to ignore the cached vcpkg directory.

### 1.3 AVX2 patching

The Docker-based build previously patched APSI to disable AVX2. For sdist builds, this patch is NOT applied — the user gets native optimizations. For wheel builds (Phase 3), AVX2 is disabled via CMake flags.

**Acceptance criteria**:
- `pip install .` works in a clean environment with only a C++ compiler, CMake, and git
- No manual vcpkg installation required
- Subsequent builds reuse the cached `_vcpkg/` directory

---

## Phase 2: Set up sdist

### 2.1 MANIFEST.in

Created `MANIFEST.in` to ensure the sdist includes:
- Python package (`apsi/`)
- C++ sources (`src/`)
- Build files (`CMakeLists.txt`, `setup.py`, `pyproject.toml`)
- Excludes `_vcpkg/`, `docker/`, `.github/`, build artifacts

### 2.2 External APSI source

The sdist must include the APSI source code. Two options:

**Option A — git submodule** (recommended):
```bash
git submodule add https://github.com/microsoft/APSI external/apsi
```

The `external/` directory is in `.gitignore` but git submodules are tracked separately. During sdist build, use `git archive --recursive` or configure setuptools to include submodule contents.

**Option B — download during build**:
Modify setup.py to download APSI source during build if `external/apsi/` doesn't exist.

**Recommendation**: Option A. Add to `.gitattributes`:
```
external/apsi export-subst
```

And update `MANIFEST.in`:
```
recursive-include external/apsi *.cpp *.h *.hpp *.cmake CMakeLists.txt *.fbs *.json
```

### 2.3 Update pyproject.toml

Build requirements simplified (no cmake/ninja needed since vcpkg handles everything):

```toml
[build-system]
requires = [
    "setuptools>=42",
    "wheel",
    "pybind11>=2.8.0",
]
build-backend = "setuptools.build_meta"
```

### 2.4 Python version support

Updated to support Python 3.8 through 3.12.

### 2.5 Test sdist build

```bash
python -m build --sdist
pip install dist/apsi-*.tar.gz
pytest tests/
```

**Acceptance criteria**:
- `python -m build --sdist` produces a complete tarball
- Installing from the tarball in a fresh venv succeeds and runs tests
- Build time is 5-15 minutes depending on machine

---

## Phase 3: Set up cibuildwheel for pre-built wheels

### 3.1 cibuildwheel configuration

In `pyproject.toml`:

```toml
[tool.cibuildwheel]
build = "cp38-* cp39-* cp310-* cp311-* cp312-*"
skip = "*-musllinux_*"
archs = "auto64"
test-requires = "pytest"
test-command = "pytest {project}/tests"

[tool.cibuildwheel.linux]
manylinux-x86_64-image = "manylinux_2_28"
manylinux-aarch64-image = "manylinux_2_28"
before-all = """
    yum install -y git zip && \
    git clone https://github.com/microsoft/vcpkg /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh && \
    /opt/vcpkg/vcpkg install seal[no-throw-tran]:x64-linux kuku:x64-linux log4cplus:x64-linux cppzmq:x64-linux flatbuffers:x64-linux jsoncpp:x64-linux
    """
environment = "VCPKG_ROOT_DIR=/opt/vcpkg"

[tool.cibuildwheel.macos]
archs = "x86_64 arm64"
before-all = """
    brew install git && \
    git clone https://github.com/microsoft/vcpkg /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh && \
    /opt/vcpkg/vcpkg install seal[no-throw-tran] kuku log4cplus cppzmq flatbuffers jsoncpp
    """
environment = "VCPKG_ROOT_DIR=/opt/vcpkg"

[tool.cibuildwheel.windows]
archs = "AMD64"
before-all = """
    git clone https://github.com/microsoft/vcpkg C:\\vcpkg && \
    C:\\vcpkg\\bootstrap-vcpkg.bat && \
    C:\\vcpkg\\vcpkg install seal[no-throw-tran]:x64-windows-static kuku:x64-windows-static log4cplus:x64-windows-static cppzmq:x64-windows-static flatbuffers:x64-windows-static jsoncpp:x64-windows-static
    """
environment = "VCPKG_ROOT_DIR=C:\\vcpkg"
```

### 3.2 Conservative compiler flags for wheels

For wheel builds, we want maximum CPU compatibility. The vcpkg ports for SEAL and APSI handle most of this, but we need to ensure no AVX2 instructions leak into the binary.

For Linux/macOS, add to the `before-all` script:
```bash
# Patch APSI to disable AVX2
sed -i "s/-D_AVX2_/-D_AVX_/g" /tmp/apsi/CMakeLists.txt
sed -i "s/_AVX2.S/.S/g" /tmp/apsi/common/apsi/fourq/amd64/CMakeLists.txt
```

Alternatively, set compiler flags:
```toml
environment = "VCPKG_ROOT_DIR=/opt/vcpkg CMAKE_CXX_FLAGS='-march=x86-64 -mtune=generic'"
```

**Note**: The vcpkg triplet matters. For maximum compatibility:
- Linux: use `x64-linux` (not `x64-linux-release` with aggressive optimizations)
- Windows: use `x64-windows-static` (static linking avoids DLL issues)
- macOS: use default triplet

### 3.3 GitHub Actions workflow

Created `.github/workflows/build-wheels.yml`:

```yaml
name: Build Wheels and sdist

on:
  push:
    branches: [main]
    tags: ['v*']
  pull_request:
    branches: [main]

jobs:
  build_sdist:
    name: Build source distribution
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - uses: actions/setup-python@v5
        with:
          python-version: '3.10'
      - name: Install build dependencies
        run: pip install build
      - name: Build sdist
        run: python -m build --sdist
      - uses: actions/upload-artifact@v4
        with:
          name: sdist
          path: dist/*.tar.gz

  build_wheels:
    name: Build wheels on ${{ matrix.os }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-13, macos-14, windows-latest]
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - uses: actions/setup-python@v5
        with:
          python-version: '3.10'
      - name: Install cibuildwheel
        run: pip install cibuildwheel
      - name: Build wheels
        run: cibuildwheel --output-dir wheelhouse
      - uses: actions/upload-artifact@v4
        with:
          name: wheels-${{ matrix.os }}
          path: wheelhouse/*.whl

  publish:
    name: Publish to PyPI
    needs: [build_sdist, build_wheels]
    runs-on: ubuntu-latest
    if: github.event_name == 'push' && startsWith(github.ref, 'refs/tags')
    permissions:
      id-token: write
    steps:
      - uses: actions/download-artifact@v4
        with:
          pattern: wheels-*
          merge-multiple: true
          path: dist
      - uses: actions/download-artifact@v4
        with:
          name: sdist
          path: dist
      - uses: pypa/gh-action-pypi-publish@release/v1
```

### 3.4 Wheel testing

Each wheel is tested automatically by cibuildwheel:
```toml
test-requires = "pytest"
test-command = "pytest {project}/tests"
```

**Acceptance criteria**:
- Wheels build for Linux (x86_64), macOS (x86_64 + arm64), and Windows (x86_64)
- All wheels install cleanly and pass tests
- sdist is also produced and uploaded

---

## Phase 4: Documentation and Cleanup

### 4.1 Update README.md

Updated with clear installation instructions:

```markdown
## Installation

### From PyPI (pre-built wheel)

pip install apsi

Pre-built wheels are available for Linux (x86_64), macOS (x86_64, arm64), and Windows (x86_64).
These wheels use conservative CPU flags for maximum compatibility.

### From source (optimized for your CPU)

pip install apsi --no-binary apsi

Requires: C++ compiler, CMake >= 3.13.4, git.
Build time: 5-15 minutes.
```

### 4.2 Update classifiers

Added macOS and Windows classifiers to `setup.py`:
```python
"Operating System :: POSIX :: Linux",
"Operating System :: MacOS :: MacOS X",
"Operating System :: Microsoft :: Windows",
```

Added Python 3.11 and 3.12 classifiers.

### 4.3 Deprecate Docker build

The `docker/` directory and `Taskfile.yml` are no longer needed for the primary build workflow. They can be kept for reference or removed.

### 4.4 Update CI workflow

Updated `.github/workflows/cicd.yaml` to test on multiple platforms (Linux, macOS) and Python versions (3.8-3.12) using the self-bootstrapping vcpkg approach.

---

## File Changes Summary

| File | Change |
|------|--------|
| `setup.py` | Added self-bootstrapping vcpkg logic (`_bootstrap_vcpkg`, `_get_vcpkg_dir`, etc.) |
| `CMakeLists.txt` | Simplified: pybind11 via FetchContent, APSI via `add_subdirectory(external/apsi/)` |
| `pyproject.toml` | Added cibuildwheel config, updated Python version support |
| `MANIFEST.in` | Created for sdist completeness |
| `.gitignore` | Added `_vcpkg/`, `wheelhouse/`, `dist/` |
| `.github/workflows/build-wheels.yml` | Created: wheel + sdist build + publish |
| `.github/workflows/cicd.yaml` | Updated: multi-platform, multi-version testing |
| `README.md` | Updated installation instructions |

---

## Effort Estimate

| Phase | Effort | Risk |
|-------|--------|------|
| 1. Self-bootstrapping vcpkg | 1 day | Low — vcpkg is well-tested |
| 2. sdist setup | 1 day | Medium — APSI submodule handling |
| 3. cibuildwheel | 1-2 days | Medium — CI build time (vcpkg takes ~30min) |
| 4. Documentation | 0.5 days | Low |
| **Total** | **~3-4 days** | |

---

## Known Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| vcpkg bootstrap fails on some platforms | Fallback to `VCPKG_ROOT_DIR` env var for user-provided vcpkg |
| sdist doesn't include APSI source | Use git submodules with `--recursive` |
| Wheel build takes too long in CI (~30-60min per platform) | Use vcpkg binary caching, or pre-build vcpkg triplet in Docker image |
| Wheel users experience poor performance | Document clearly; sdist provides optimized alternative |
| macOS ARM build requires M1 runner | Use `macos-14` (M1) runners |
| Windows static linking increases wheel size | Acceptable trade-off for compatibility |

---

## Future Enhancements

1. **vcpkg binary caching**: Cache vcpkg installed packages in CI to speed up wheel builds
2. **Custom manylinux image**: Pre-build vcpkg + dependencies into a custom Docker image for faster CI
3. **Runtime CPU dispatch**: Modify APSI/SEAL to detect CPU features at runtime
4. **Conda-forge package** (Strategy 5): Complementary distribution channel
