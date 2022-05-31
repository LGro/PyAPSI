# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/ci-cd-pipeline/badge.svg)](https://github.com/LGro/PyAPSI/actions)
[![License: MIT](https://img.shields.io/github/license/LGro/PyAPSI)](https://github.com/LGro/PyAPSI/blob/main/LICENSE)
[![Code style: black](https://img.shields.io/badge/code%20style-black-000000.svg)](https://github.com/psf/black)

Python wrapper for labeled and unlabeled asynchronous private set intersection
([APSI](https://github.com/microsoft/apsi)).

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

## Building & Testing

Before you start, make sure that [Taskfile](https://taskfile.dev/#/installation),
[Docker](https://docs.docker.com/engine/install/) and
[Poetry](https://python-poetry.org/docs/#installation) are installed.

You can then run a full build with tests that will generate a wheel file in the
repository root as follows:

```
task wheel PYTHON_VERSION=3.8.13
```

NOTE: Only Python 3.8 and 3.9, and patch versions for which official Python Docker
images exist are supported.

## Wheel

Trying to `auditwheel repair --plat manylinux_2_31_x86_64` but fails due to too recent
symbols.

All libraries should be fine in principle according to the auditwheel policies
https://github.com/pypa/auditwheel/blob/main/src/auditwheel/policy/manylinux-policy.json

```
The wheel references external versioned symbols in these
system-provided shared libraries:

libm.so.6 with versions {'GLIBC_2.2.5', 'GLIBC_2.29'},

# all good
libgcc_s.so.1 with versions {'GCC_3.0', 'GCC_3.3.1'},

# GLIBC_2.33 seems to be a problem -> 2.31 is last supported
libc.so.6 with versions {'GLIBC_2.4', 'GLIBC_2.9', 'GLIBC_2.25', 'GLIBC_2.7', 'GLIBC_2.16', 'GLIBC_2.3', 'GLIBC_2.3.4', 'GLIBC_2.17', 'GLIBC_2.33', 'GLIBC_2.3.2', 'GLIBC_2.2.5', 'GLIBC_2.32', 'GLIBC_2.14', 'GLIBC_2.10'},

libstdc++.so.6 with versions {'GLIBCXX_3.4', 'GLIBCXX_3.4.22', 'CXXABI_1.3', 'GLIBCXX_3.4.21', 'GLIBCXX_3.4.20', 'GLIBCXX_3.4.14', 'GLIBCXX_3.4.11', 'CXXABI_1.3.5', 'CXXABI_1.3.11', 'CXXABI_1.3.8', 'CXXABI_1.3.3', 'GLIBCXX_3.4.19', 'GLIBCXX_3.4.15', 'CXXABI_1.3.7', 'GLIBCXX_3.4.26', 'GLIBCXX_3.4.17', 'GLIBCXX_3.4.9', 'CXXABI_1.3.9', 'CXXABI_1.3.2', 'GLIBCXX_3.4.18'},

# all good
libpthread.so.0 with versions {'GLIBC_2.3.2', 'GLIBC_2.2.5', 'GLIBC_2.30', 'GLIBC_2.12', 'GLIBC_2.3.4'}
```
