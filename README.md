# PyAPSI

[![Actions Status](https://github.com/LGro/PyAPSI/workflows/ci-cd-pipeline/badge.svg)](https://github.com/LGro/PyAPSI/actions)

[APSI](https://github.com/microsoft/apsi) wrapped into a Python library with
[pybind11](https://pybind11.readthedocs.io/).

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
server.init_db(apsi_params, label_byte_count=10)
server.add_items(
    [("item", "1234567890"), ("meti", "0987654321"), ("time", "1010101010")]
)

client = LabeledClient(apsi_params)

oprf_request = client.oprf_request(["item", "meti"])
oprf_response = server.handle_oprf_request(oprf_request)
query = client.build_query(oprf_response)
response = server.handle_query(query)
result = client.extract_result(response)

assert result == {"item": "1234567890", "meti": "0987654321"}
```

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
