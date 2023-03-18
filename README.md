# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/ci-cd-pipeline/badge.svg)](https://github.com/LGro/PyAPSI/actions)
[![PyPI - Wheel](https://img.shields.io/pypi/wheel/apsi)](https://pypi.org/project/apsi/)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/apsi)](https://pypi.org/project/apsi/)
[![License: MIT](https://img.shields.io/github/license/LGro/PyAPSI)](https://github.com/LGro/PyAPSI/blob/main/LICENSE)
[![Code style: black](https://img.shields.io/badge/code%20style-black-000000.svg)](https://github.com/psf/black)

Python wrapper for labeled and unlabeled asymmetric private set intersection
([APSI](https://github.com/microsoft/apsi)).

## Setup

For `manylinux_2_31_x86_64` compatible platforms you can install `PyAPSI` from
[PyPi](https://pypi.org/project/apsi/) with

```
pip install apsi
```

You can check the system library versions that are required to be
`manylinux_2_31_x86_64` compatible in the
[auditwheel policy](https://github.com/pypa/auditwheel/blob/main/src/auditwheel/policy/manylinux-policy.json#L335-L340).

NOTE: While AVX2 supported is currently patched out
([#11](https://github.com/LGro/PyAPSI/issues/11)), APSI and its dependencies still seem
to choose optimizations during build time depending on the available CPU flags, which
can cause incompatibility of the pre-built wheels on older CPUs beyond what `auditwheel`
can identify ([#13](https://github.com/LGro/PyAPSI/issues/13)).

In case you feel like contributing a build setup for Windows and OSX compatible wheels
or extend the "From Source" section below, I would be happy to review your pull request.

## Example

Example usage of the labeled APSI server and client.
The unlabeled variant can be used analogous to this.

```python
from apsi import LabeledServer, LabeledClient

apsi_params = """
{
    "table_params": {
        "hash_func_count": 3,
        "table_size": 512,
        "max_items_per_bin": 92
    },
    "item_params": {"felts_per_item": 8},
    "query_params": {
        "ps_low_degree": 0,
        "query_powers": [1, 3, 4, 5, 8, 14, 20, 26, 32, 38, 41, 42, 43, 45, 46]
    },
    "seal_params": {
        "plain_modulus": 40961,
        "poly_modulus_degree": 4096,
        "coeff_modulus_bits": [40, 32, 32]
    }
}
"""

server = LabeledServer()
server.init_db(apsi_params, max_label_length=10)
server.add_items([("item", "1234567890"), ("abc", "123"), ("other", "my label")])

client = LabeledClient(apsi_params)

oprf_request = client.oprf_request(["item", "abc"])
oprf_response = server.handle_oprf_request(oprf_request)
query = client.build_query(oprf_response)
response = server.handle_query(query)
result = client.extract_result(response)

assert result == {"item": "1234567890", "abc": "123"}
```

To control multi threading and logging in `APSI` see
[`apsi.utils`](https://github.com/LGro/PyAPSI/blob/main/apsi/utils.py).

## Building & Testing

### Docker

Before you start, make sure that [Taskfile](https://taskfile.dev/#/installation),
[Docker](https://docs.docker.com/engine/install/) and
[Poetry](https://python-poetry.org/docs/#installation) are installed.

You can then run a full build with tests that will generate a wheel file in `dist/` as
follows:

```
task wheel PYTHON_VERSION=3.10.4
```

Note: Only Python 3.8, 3.9, 3.10, and their patch versions for which
[official Python Docker images](https://hub.docker.com/_/python) exist are supported.

### From Source

Please have a look at the files inside
[`docker/`](https://github.com/LGro/PyAPSI/tree/main/docker) for the required `vcpkg`
setup and `apsi` AVX2 patch, in case you'd like to build from source in a custom
environment.
