import json

from pyapsi import APSIClient

if __name__ == "__main__":
    server_address = "tcp://localhost:1234"
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
    # The parameters aren't really used in that example, since they are only required
    # for the advanced API
    client = APSIClient(json.dumps(params))
    oprf = client.query("tcp://localhost:1234", "item", 1)
