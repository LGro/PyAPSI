# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/ci-cd-pipeline/badge.svg)](https://github.com/LGro/PyAPSI/actions)

A `pybind11` based wrapper for [APSI](https://github.com/microsoft/apsi).

**NOTE:** This is a very early implementation with a high probability of changing
dramatically.

At the current stage, this wraps the ZMQ based communication CLI example from the APSI
repository and does not expose the individual steps of the APSI "advanced API" for a
custom communication channel via Python.

## Build

At the moment only building under Linux is supported.

Make sure to install `vcpkg` and APSI as specified in the APSI README.
Then set the environment variable `VCPKG_INSTALLED_DIR` to
`/your/path/to/vcpkg/installed` install `poetry` and run

```
poetry install
rm -rf build/ && pip install --verbose .
```

## Run

In two separate terminals, run `poetry run python server.py` and then
`poetry run python client.py`.
