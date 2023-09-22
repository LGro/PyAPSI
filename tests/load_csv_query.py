from apsi import LabeledServer, LabeledClient

apsi_params = """
{
    "table_params": {
        "hash_func_count": 3,
        "table_size": 1638,
        "max_items_per_bin": 1304
    },
    "item_params": {
        "felts_per_item": 5
    },
    "query_params": {
        "ps_low_degree": 44,
        "query_powers": [ 1, 3, 11, 18, 45, 225 ]
    },
    "seal_params": {
        "plain_modulus_bits": 22,
        "poly_modulus_degree": 8192,
        "coeff_modulus_bits": [ 56, 56, 56, 50 ]
    }
}
"""
def main() -> None:
    server = LabeledServer()
    server.load_csv_db('./tests/test_10.csv',apsi_params)

    client = LabeledClient(apsi_params)
    oprf_request = client.oprf_request(["828123436896012688", "952535141803615208"])
    oprf_response = server.handle_oprf_request(oprf_request)
    query = client.build_query(oprf_response)
    response = server.handle_query(query)
    result = client.extract_result(response)
    print(result)

if __name__ == "__main__":
    main()