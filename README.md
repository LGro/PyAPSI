# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/Build%20Wheels%20and%20sdist/badge.svg)](https://github.com/LGro/PyAPSI/actions)
[![PyPI - Wheel](https://img.shields.io/pypi/wheel/apsi)](https://pypi.org/project/apsi/)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/apsi)](https://pypi.org/project/apsi/)
[![License: MIT](https://img.shields.io/github/license/LGro/PyAPSI)](https://github.com/LGro/PyAPSI/blob/main/LICENSE)
[![Code style: black](https://img.shields.io/badge/code%20style-black-000000.svg)](https://github.com/psf/black)

Python wrapper for labeled and unlabeled asynchronous private set intersection
([APSI](https://github.com/microsoft/apsi)).

## Installation

### From PyPI (pre-built wheel)

```bash
pip install apsi
```

Pre-built wheels are available for:
- **Linux**: x86_64 (manylinux_2_28)
- **macOS**: x86_64 (Intel) and arm64 (Apple Silicon)
- **Windows**: x86_64

These wheels are built with conservative CPU flags (`-march=x86-64 -mtune=generic`) for maximum compatibility across different CPU generations. This means they may not use AVX2/AVX-512 optimizations even if your CPU supports them.

### From source (optimized for your CPU)

To build with native CPU optimizations (AVX2, AVX-512, etc.):

```bash
pip install apsi --no-binary apsi
```

This compiles APSI and all dependencies from source, automatically selecting the best optimizations for your CPU. Build time is approximately 5-15 minutes.

**Requirements:**
- C++ compiler (GCC >= 9, Clang >= 10, or MSVC >= 2019)
- CMake >= 3.13.4
- Python >= 3.8
- Internet access (dependencies are fetched during build)

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

### Building a Wheel Locally

```bash
pip install build cibuildwheel
cibuildwheel --platform linux --output-dir wheelhouse
```

### Building a Source Distribution

```bash
pip install build
python -m build --sdist
```

## Supported Python Versions

Python 3.8, 3.9, 3.10, 3.11, 3.12
