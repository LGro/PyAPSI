import json
from typing import Dict, List, Union

import pytest
from apsi import LabeledClient, LabeledServer, UnlabeledClient, UnlabeledServer


def _query(
    client: Union[UnlabeledClient, LabeledClient],
    server: Union[UnlabeledServer, LabeledServer],
    items: List[str],
) -> Dict[str, str]:
    oprf_request = client.oprf_request(items)
    oprf_response = server.handle_oprf_request(oprf_request)
    query = client.build_query(oprf_response)
    response = server.handle_query(query)
    result = client.extract_result(response)
    return result


# TODO: Transform into pytest fixture
def _get_params_json() -> str:
    return json.dumps(
        {
            "table_params": {
                "hash_func_count": 3,
                "table_size": 512,
                "max_items_per_bin": 92,
            },
            "item_params": {"felts_per_item": 8},
            "query_params": {
                "ps_low_degree": 0,
                "query_powers": [1, 3, 4, 5, 8, 14, 20, 26, 32, 38, 41, 42, 43, 45, 46],
            },
            "seal_params": {
                "plain_modulus": 40961,
                "poly_modulus_degree": 4096,
                "coeff_modulus_bits": [40, 32, 32],
            },
        }
    )


def test_labeled_client_server_integration():
    server = LabeledServer()
    server.init_db(_get_params_json(), max_label_length=10)
    server.add_item("item", "1234567890")
    server.add_items([("meti", "0987654321"), ("time", "1010101010")])

    client = LabeledClient(_get_params_json())

    assert _query(client, server, ["item"]) == {"item": "1234567890"}
    assert _query(client, server, ["item", "meti", "unknown"]) == {
        "item": "1234567890",
        "meti": "0987654321",
    }
    assert _query(client, server, ["unknown"]) == {}


def test_labeled_client_server_integration_with_shorter_labels():
    server = LabeledServer()
    server.init_db(_get_params_json(), max_label_length=10)
    server.add_item("long_item", "1234567890")
    server.add_item("short_item", "321")

    client = LabeledClient(_get_params_json())

    result = _query(client, server, ["long_item", "short_item"])
    assert result == {"long_item": "1234567890", "short_item": "321"}


def test_adding_too_long_label_to_labeled_server_raises_error():
    server = LabeledServer()
    server.init_db(_get_params_json(), max_label_length=4)

    server.add_item("item1", "1234")

    with pytest.raises(ValueError, match="length"):
        server.add_item("item2", "12345")

    with pytest.raises(ValueError, match="length"):
        server.add_items([("item2", "12345")])


def test_unlabeled_client_server_integration():
    server = UnlabeledServer()
    server.init_db(_get_params_json())
    server.add_item("item")
    server.add_items(["meti", "time"])

    client = UnlabeledClient(_get_params_json())

    assert _query(client, server, ["item"]) == ["item"]
    assert _query(client, server, ["item", "meti", "unknown"]) == ["item", "meti"]
    assert _query(client, server, ["unknown"]) == []


def test_readme_example():
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
