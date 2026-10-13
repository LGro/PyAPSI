# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/Build%20and%20publish%20sdist/badge.svg)](https://github.com/LGro/PyAPSI/actions)
[![PyPI](https://img.shields.io/pypi/v/apsi)](https://pypi.org/project/apsi/)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/apsi)](https://pypi.org/project/apsi/)
[![License: MIT](https://img.shields.io/github/license/LGro/PyAPSI)](https://github.com/LGro/PyAPSI/blob/main/LICENSE)
[![Code style: black](https://img.shields.io/badge/code%20style-black-000000.svg)](https://github.com/psf/black)

Python wrapper for labeled and unlabeled asymmetric private set intersection
([APSI](https://github.com/microsoft/apsi)).

Built against APSI 1.1.0.

## Upgrading to PyAPSI 0.3

PyAPSI 0.3 moves from APSI 0.12 to APSI 1.1, which brings a few user visible changes:

- The log levels are named `"TRACE"`, `"DEBUG"`, `"INFO"`, `"WARNING"`, `"ERROR"` and
  `"SUPPRESS"`. The previous `"ALL"` and `"OFF"` are still accepted as `"TRACE"` and
  `"SUPPRESS"` respectively.
- APSI retuned its shipped parameter sets, because most of them were above the documented
  2^-40 false-positive probability per query. A server and a client have to agree on their
  parameters, so both sides of a deployment should move to the new values together; the
  example below is APSI's `parameters/1M-256.json`.
- APSI's serialization version is unchanged and the schemas stayed wire compatible, so a
  database saved by an earlier PyAPSI release should still load, but that is untested
  here. A cuckoo filter written before APSI 1.0 carries no assertion that nothing was
  dropped from it, so it is loaded as one that may have dropped items; that costs some
  query time on the affected bin bundles, not correctness.
- Each query a client sends now carries key material of its own, so that a server can no
  longer tell from the public keys that two queries came from the same client.

## Installation

### From PyPI (builds from source)

```bash
pip install apsi
```

This downloads the source distribution and builds APSI + all dependencies locally.
The build automatically selects the best CPU optimizations for your machine.

**Requirements:**
- C++ compiler with full C++17 support (GCC, Clang, or Visual Studio 2022/2026, as
  required by APSI 1.1)
- CMake >= 3.25
- Python 3.11–3.14
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
        "table_size": 585,
        "max_items_per_bin": 180
    },
    "item_params": {"felts_per_item": 7},
    "query_params": {
        "ps_low_degree": 0,
        "query_powers": [
            1, 3, 4, 6, 10, 13, 15, 21, 29, 37, 45,
            53, 61, 69, 77, 81, 83, 86, 87, 90, 92, 96
        ]
    },
    "seal_params": {
        "plain_modulus": 65537,
        "poly_modulus_degree": 4096,
        "coeff_modulus_bits": [40, 40, 24]
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
