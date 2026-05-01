# Strategy 4: Pre-built Wheels

## Status: Deferred

Pre-built wheels would give users fast installs without requiring a local C++ compiler. However, several blockers prevent this from working reliably. This document captures the approach and known issues for future reference.

## Why wheels are deferred

### flatc segfault in manylinux containers

The flatbuffers compiler (`flatc`) from vcpkg segfaults when running in manylinux Docker containers, particularly on ARM64 hosts running x86_64 emulation. This happens during APSI's CMake configuration when it tries to generate C++ headers from `.fbs` schema files.

### FourQlib PIC incompatibility on Linux

The FourQlib assembly files (`fp2_1271_AVX2.S`) use non-PIC relocations (`relocation R_X86_64_PC32 against symbol 'ONEx8'`), which are incompatible with Python shared extensions (`.so` files). This was resolved for sdist builds by disabling `APSI_USE_ASM`, but for wheels we'd need to either:
- Pre-generate the assembly-free C variants
- Build with `APSI_USE_ASM=OFF` (same performance trade-off as sdist on Linux)

### vcpkg bootstrap overhead in CI

Each wheel build requires bootstrapping vcpkg and installing all dependencies (~30 min per platform), making CI builds very long.

## Potential solutions

1. **Pre-generate flatbuffers headers**: Commit the `*_generated.h` files to eliminate the need for `flatc` during wheel builds.

2. **Custom manylinux image**: Pre-build vcpkg + all dependencies + flatbuffers headers into a custom Docker image.

3. **Build wheels on native runners**: Avoid cross-architecture emulation by using native x86_64 runners for Linux wheels.

4. **Consider scikit-build-core**: A modern alternative to setuptools + CMake that handles many of these edge cases better.

## What would need to change

### setup.py

The current `setup.py` already handles self-bootstrapping vcpkg. For wheel builds, the `VCPKG_ROOT_DIR` environment variable would be set by the CI to point to a pre-built vcpkg installation.

### pyproject.toml

Would need `[tool.cibuildwheel]` configuration:

```toml
[tool.cibuildwheel]
build = "cp311-* cp312-* cp313-* cp314-*"
skip = "*-musllinux_*"
archs = "auto64"
test-requires = "pytest"
test-command = "pytest {project}/tests"

[tool.cibuildwheel.linux]
manylinux-x86_64-image = "manylinux_2_28"
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
    git clone https://github.com/microsoft/vcpkg /tmp/vcpkg && \
    /tmp/vcpkg/bootstrap-vcpkg.sh && \
    /tmp/vcpkg/vcpkg install seal[no-throw-tran] kuku log4cplus cppzmq flatbuffers jsoncpp
"""
environment = "VCPKG_ROOT_DIR=/tmp/vcpkg"

[tool.cibuildwheel.windows]
archs = "AMD64"
before-all = """
    git clone https://github.com/microsoft/vcpkg C:\\vcpkg && \
    C:\\vcpkg\\bootstrap-vcpkg.bat && \
    C:\\vcpkg\\vcpkg install seal[no-throw-tran]:x64-windows-static-md kuku:x64-windows-static-md log4cplus:x64-windows-static-md cppzmq:x64-windows-static-md flatbuffers:x64-windows-static-md jsoncpp:x64-windows-static-md
"""
environment = "VCPKG_ROOT_DIR=C:\\vcpkg"
```

### GitHub Actions workflow

Would extend the current `build-sdist.yml` to also build wheels, or create a separate `build-wheels.yml`.

## Summary

| Aspect | Current (sdist) | Future (wheels) |
|--------|----------------|-----------------|
| Install speed | 5–15 min build | Seconds |
| Requirements | C++ compiler, CMake | None |
| CPU optimization | Native (best) | Conservative (compatible) |
| Linux performance | ~2–3x slower OPRF (no FourQlib ASM) | Same |
| Maintenance | Low | Medium (CI config) |
