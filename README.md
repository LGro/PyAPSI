# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/ci-cd-pipeline/badge.svg)](https://github.com/LGro/PyAPSI/actions)

A `pybind11` based wrapper for [APSI](https://github.com/microsoft/apsi).

**NOTE:** This is a very early implementation with a high probability of changing
dramatically.

## Building & Running

There are two ways to see PyAPSI in action, one is leveraging the provided Dockerfile,
the other is setting up a Python environment and running example scripts oneself.

### Dockerized

Given that Docker and docker-compose are available, the following will build PyAPSI and
run an example script, showing its output in the terminal.

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

#### APSI Advanced API

With `poetry run python examples/advanced.py` the wrapped APSI "advanced" API is
demonstrated.

#### APSI ZMQ Example

A wrapper example for APSI's "simple" API requires two separate terminals, where in one
`poetry run python examples/server.py` is started and in the other
`poetry run python examples/client.py`
