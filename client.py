from pyapsi import APSIClient

if __name__ == "__main__":
    server_address = "tcp://localhost:1234"
    client = APSIClient()
    oprf = client.oprf_request("item")
