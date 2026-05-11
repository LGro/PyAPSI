# Distribution Strategy Plans

This directory contains implementation plans for distributing PyAPSI across platforms.

## Current State

### Source Distribution (sdist) — Implemented

The sdist workflow is active and:
- Builds a source tarball on every push/PR
- Tests installation from the tarball on Python 3.11–3.14 (Ubuntu, macOS, and Windows)
- Publishes to PyPI on tags only after successful install tests

Users install with:
```bash
pip install apsi
```

This triggers a full local build with native CPU optimizations. Build time is ~5–15 minutes.

See the [top-level README](../README.md) for installation instructions, requirements, and usage examples.

## Future Extensions

These strategies document possible future distribution channels that could complement the current sdist-only approach.

- [Strategy 4: Pre-built Wheels](./strategy-4-hybrid-wheels-sdist.md) — Deferred until flatc/pre-generation issues are resolved
- [Strategy 5: Conda-forge Distribution](./strategy-5-conda-forge.md) — Not started
