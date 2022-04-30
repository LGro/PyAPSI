import json
from typing import Dict, Union

from apsi import UnlabeledClient, UnlabeledServer, LabeledServer, LabeledClient


def _query(
    client: Union[UnlabeledClient, LabeledClient],
    server: Union[UnlabeledServer, LabeledServer],
    item: str,
) -> Dict[str, str]:
    oprf_request = client.oprf_request(item)
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
    server.init_db(_get_params_json(), label_byte_count=10)
    server.add_item("item", "1234567890")
    server.add_items([("meti", "0987654321"), ("time", "1010101010")])
    client = LabeledClient(_get_params_json())
    assert _query(client, server, "item") == {"item": "1234567890"}
    assert _query(client, server, "meti") == {"meti": "0987654321"}
    assert _query(client, server, "unknown") == {}


def test_unlabeled_client_server_integration():
    server = UnlabeledServer()
    server.init_db(_get_params_json())
    server.add_item("item")
    server.add_items(["meti", "time"])
    client = UnlabeledClient(_get_params_json())
    assert _query(client, server, "item") == ["item"]
    assert _query(client, server, "meti") == ["meti"]
    assert _query(client, server, "unknown") == []
