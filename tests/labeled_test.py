import json
from typing import Dict

from apsi.labeled import Client, Server


def _query(client: Client, server: Server, item: str) -> Dict[str, str]:
    oprf_request = client.oprf_request(item)
    oprf_response = server.handle_oprf_request(oprf_request)
    query = client.build_query(oprf_response)
    response = server.handle_query(query)
    result = client.extract_result_from_query_response(response)
    return result


def test_client_server_integration():
    params = json.dumps(
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

    server = Server()
    server.init_db(params, label_byte_count=10)
    server.add_item("item", "1234567890")
    server.add_item("meti", "0987654321")
    client = Client(params)
    assert _query(client, server, "item") == {"item": "1234567890"}
    assert _query(client, server, "meti") == {"meti": "0987654321"}
    assert _query(client, server, "unknown") == {}
