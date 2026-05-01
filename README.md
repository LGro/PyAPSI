# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/Build%20and%20publish%20sdist/badge.svg)](https://github.com/LGro/PyAPSI/actions)
[![PyPI](https://img.shields.io/pypi/v/apsi)](https://pypi.org/project/apsi/)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/apsi)](https://pypi.org/project/apsi/)
[![License: MIT](https://img.shields.io/github/license/LGro/PyAPSI)](https://github.com/LGro/PyAPSI/blob/main/LICENSE)
[![Code style: black](https://img.shields.io/badge/code%20style-black-000000.svg)](https://github.com/psf/black)

Python wrapper for labeled and unlabeled asynchronous private set intersection
([APSI](https://github.com/microsoft/apsi)).

## Installation

### From PyPI (builds from source)

```bash
pip install apsi
```

This downloads the source distribution and builds APSI + all dependencies locally.
The build automatically selects the best CPU optimizations for your machine.

**Requirements:**
- C++ compiler (GCC >= 9, Clang >= 10, or MSVC >= 2019)
- CMake >= 3.13.4
- Python >= 3.11
- Internet access (vcpkg downloads dependencies during build)

**Build time:** Approximately 5-15 minutes depending on your machine.

## Performance Note

On Linux x86_64, the FourQlib elliptic curve assembly optimizations are disabled
because the hand-optimized AVX2 assembly files use non-PIC relocations that are
incompatible with Python shared extensions (`.so` files). The generic C fallback is
used instead, which results in approximately 2-3x slower OPRF (Oblivious Pseudorandom
Function) operations.

This does not affect the SEAL homomorphic encryption operations, which dominate runtime
for typical set intersection sizes. On macOS and Windows, FourQlib uses different
optimization paths that are not affected.

For maximum performance on Linux, consider building APSI as a standalone static library
outside of Python and using the native CLI tools.

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

### From Source

```bash
pip install -e .
pytest tests/
```

### Building a Source Distribution

```bash
pip install build
python -m build --sdist
```

## Supported Python Versions

Python 3.11, 3.12, 3.13, 3.14
