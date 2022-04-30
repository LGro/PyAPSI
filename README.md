# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/ci-cd-pipeline/badge.svg)](https://github.com/LGro/PyAPSI/actions)

A `pybind11` based wrapper for [APSI](https://github.com/microsoft/apsi).

**NOTE:** This is a very early implementation with a high probability of changing
dramatically.

## Building & Testing

### Dockerized

Given that Docker and docker-compose are available, the following will build PyAPSI and
run tests.

```
docker-compose build
docker-compose run pyapsi
```

### Python

Currently, only building under Linux is supported.

Make sure to install `vcpkg` and APSI as specified in the
[APSI README](https://github.com/microsoft/APSI/blob/main/README.md).
Then set the environment variable `VCPKG_INSTALLED_DIR` to
`/your/path/to/vcpkg/installed` install `poetry` and run

```
poetry install
rm -rf build/
poetry run pip install --verbose .
```

You can then run the tests with

```
poetry run pytest tests/
```
