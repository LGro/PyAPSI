import json

from pyapsi import APSIClient, APSIServer, enable_logging

if __name__ == "__main__":
    n_threads = 2
    params = {
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
    label_byte_count = 10
    nonce_byte_count = 16
    compressed = False

    enable_logging()

    server = APSIServer(n_threads)
    server.init_db(
        json.dumps(params),
        label_byte_count,
        nonce_byte_count,
        compressed,
    )
    server.add_item("item", "1234567890")
    server.add_item("meti", "0987654321")

    client = APSIClient(json.dumps(params))
    oprf_request = client.oprf_request("item")
    oprf_response = server.handle_oprf_request(oprf_request)
    query = client.build_query(oprf_response)
    response = server.handle_query(query)
    result = client.extract_result_from_query_response(response)
    print(result)
